
#include "stdafx.h"
#include <Core/server_config.h>
#include "DatabaseConfig.h"
#include <common/CommonDefines.h>

#include <Core/peer_auth.h>
#include <Core/Time.h>

#include <common/building.h>
#include <common/DragonCoinWallet.h>
#include <common/VnumHelper.h>
#include <Base/grid.h>

#include "ClientManager.h"

#include <algorithm>

#include "Main.h"
#include "DBManager.h"
#include "QID.h"
#include "GuildManager.h"
#include "PrivManager.h"
#include "MoneyLog.h"
#include <bit>
#include "Marriage.h"
#include "Cache.h"
#include <sstream>
#include <memory>
#include <cmath>
#include <print>
#include <fstream>
#include <iomanip>
#include <string>
#include <mysql/mysql.h>

#ifdef max
#undef max
#endif


#if defined(ENABLE_EXTEND_ITEM_AWARD)
#include <float.h>
#include "Core/Logging.hpp"
inline const double uniform_random(double a, double b)
{
	return thecore_random() / (RAND_MAX + 1.f) * (b - a) + a;
}

inline const float GetRandomGaussian(float avg, float sigma)
{
	static bool bHasNextGaussian = false;
	static float fNextGaussian = 0.0f;

	if (bHasNextGaussian)
	{
		bHasNextGaussian = false;
		return fNextGaussian * sigma + avg;
	}
	else
	{
		double v1, v2, s;
		do
		{
			v1 = uniform_random(-1.f, 1.f);
			v2 = uniform_random(-1.f, 1.f);
			s = v1 * v1 + v2 * v2;
		} while (s >= 1.f || fabs(s) < FLT_EPSILON);

		double multiplier = sqrtf(-2 * logf(s) / s);
		fNextGaussian = v2 * multiplier;
		bHasNextGaussian = true;
		return v1 * multiplier * sigma + avg;
	}
}
#endif

extern int g_iPlayerCacheFlushSeconds;
extern int g_test_server;
extern int g_log;
extern std::string g_stLocale;
extern std::string g_stLocaleNameColumn;

uint32_t g_dwUsageMax = 0;
uint32_t g_dwUsageAvg = 0;

CPacketInfo g_query_info;

int g_query_count[2];
#ifdef ENABLE_PROTO_FROM_DB
bool g_bMirror2DB = false;
#endif

CClientManager::CClientManager() : m_looping(0), m_acceptor(nullptr),
                                   m_pkAuthPeer(nullptr),
                                   m_iPlayerIDStart(0),
                                   //m_iPlayerDeleteLevelLimit(0),
                                   //m_iPlayerDeleteLevelLimitLower(0),
                                   m_bShutdowned(false), m_bLastHeader(0),
                                   m_iCacheFlushCount(0),
                                   m_iCacheFlushCountLimit(200)
{

	memset(g_query_count, 0, sizeof(g_query_count));
#ifdef ENABLE_PROTO_FROM_DB
	bIsProtoReadFromDB = false;
#endif
}

CClientManager::~CClientManager()
{
	Destroy();
}

void CClientManager::SetPlayerIDStart(int iIDStart)
{
	m_iPlayerIDStart = iIDStart;
}

void CClientManager::GetPeerP2PHostNames(std::string& peerHostNames)
{
	std::ostringstream oss(std::ostringstream::out);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * peer = *it;
		oss << peer->GetHost() << " " << peer->GetP2PPort() << " channel : " << (int)peer->GetChannel() << "\n";
	}

	peerHostNames += oss.str();
}

void CClientManager::Destroy()
{
	m_mChannelStatus.clear();

	for (CPeer * peer : m_peerList)
	{
		peer->Destroy();

		if (peer->HasPendingIoWork())
		{
			// The engine is stopped by this point; abandoned io work can
			// never release the peer, so keep it alive instead of freeing
			// memory a queued completion could still reference.
			peer->MarkDeletePending();
			m_pendingDeletePeers.push_back(peer);
		}
		else
		{
			delete peer;
		}
	}

	m_peerList.clear();

#if defined(BL_OFFLINE_MESSAGE)
	m_OfflineMessage.clear();
#endif
	if (m_acceptor)
	{
		// The accept handler may still reference the acceptor on the io
		// thread; the close is serialized through the io thread and the
		// shared_ptr copy keeps the object alive until it ran.
		auto acceptor = std::move(m_acceptor);
		m_acceptor.reset();

		auriga::net::PostIo(
			[acceptor]()
			{
				boost::system::error_code ec;
				acceptor->close(ec);
			});
	}
}

#define ENABLE_DEFAULT_PRIV
#ifdef ENABLE_DEFAULT_PRIV
static bool bCleanOldPriv = true;
static bool __InitializeDefaultPriv()
{
	if (bCleanOldPriv)
	{
		std::unique_ptr<SQLMsg> pCleanStuff(CDBManager::instance().DirectQuery("DELETE FROM priv_settings WHERE value <= 0 OR duration <= NOW();", SQL_COMMON));
		std::print("DEFAULT_PRIV_EMPIRE: removed {} expired priv settings.\n", pCleanStuff->Get()->uiAffectedRows);
	}
	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery("SELECT priv_type, id, type, value, UNIX_TIMESTAMP(duration) FROM priv_settings", SQL_COMMON));
	if (pMsg->Get()->uiNumRows == 0)
		return false;
	MYSQL_ROW row = nullptr;
	while ((row = pMsg->Get()->FetchRow()))
	{
		if (!strcmp(row[0], "EMPIRE"))
		{
			// init
			uint8_t empire = 0;
			uint8_t type = 1;
			int value = 0;
			int64_t duration_sec = 0;
			// set
			str_to_number(empire, row[1]);
			str_to_number(type, row[2]);
			str_to_number(value, row[3]);
			str_to_number(duration_sec, row[4]);
			// recalibrate time
			int64_t now_time_sec = CClientManager::instance().GetCurrentTime();
			if (now_time_sec>duration_sec)
				duration_sec = 0;
			else
				duration_sec -= now_time_sec;
			// send priv
			std::print("DEFAULT_PRIV_EMPIRE: set empire({}), type({}), value({}), duration({})\n", empire, type, value, duration_sec);
			CPrivManager::instance().AddEmpirePriv(empire, type, value, duration_sec);
		}
		else if (!strcmp(row[0], "GUILD"))
		{
			// init
			uint32_t guild_id = 0;
			uint8_t type = 1;
			int value = 0;
			int64_t duration_sec = 0;
			// set
			str_to_number(guild_id, row[1]);
			str_to_number(type, row[2]);
			str_to_number(value, row[3]);
			str_to_number(duration_sec, row[4]);
			// recalibrate time
			int64_t now_time_sec = CClientManager::instance().GetCurrentTime();
			if (now_time_sec>duration_sec)
				duration_sec = 0;
			else
				duration_sec -= now_time_sec;
			// send priv
			if (guild_id)
			{
				std::print("DEFAULT_PRIV_GUILD: set guild_id({}), type({}), value({}), duration({})\n", guild_id, type, value, duration_sec);
				CPrivManager::instance().AddGuildPriv(guild_id, type, value, duration_sec);
			}
		}
		else if (!strcmp(row[0], "PLAYER"))
		{
			// init
			uint32_t pid = 0;
			uint8_t type = 1;
			int value = 0;
			// set
			str_to_number(pid, row[1]);
			str_to_number(type, row[2]);
			str_to_number(value, row[3]);
			// send priv
			if (pid)
			{
				LOG_INFO("DEFAULT_PRIV_PLAYER: set pid({}), type({}), value({})", pid, type, value);
				CPrivManager::instance().AddCharPriv(pid, type, value);
			}
		}
	}
	return true;
}

static bool __UpdateDefaultPriv(const char* priv_type, uint32_t id, uint8_t type, int value, int64_t duration_sec)
{
	char szQuery[1024];
	snprintf(szQuery, 1024,
		"REPLACE INTO priv_settings SET priv_type=?, id=?, type=?, value=?, duration=DATE_ADD(NOW(), INTERVAL %lld SECOND)",
		static_cast<long long>(duration_sec)
	);
	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQueryPrepared(szQuery,
			{ SqlParam::Text(priv_type), SqlParam::UInt(id), SqlParam::UInt(type), SqlParam::Int(value) },
			SQL_COMMON));
	return pMsg->Get()->uiAffectedRows;
}
#endif

bool CClientManager::Initialize()
{
	int tmpValue;

	//BOOT_LOCALIZATION
	if (!InitializeLocalization())
	{
		LOG_ERROR("Failed Localization Infomation so exit");
		return false;
	}
#ifdef ENABLE_DEFAULT_PRIV
	if (!__InitializeDefaultPriv())
	{
		LOG_ERROR("Failed Default Priv Setting so exit");
		// return false;
	}
#endif
	//END_BOOT_LOCALIZATION


#ifdef ENABLE_PROTO_FROM_DB
	bIsProtoReadFromDB = DbConfig().proto.fromDb;
	LOG_INFO("PROTO_FROM_DB: {}", bIsProtoReadFromDB?"Enabled":"Disabled");

	if (!bIsProtoReadFromDB)
	{
		g_bMirror2DB = DbConfig().proto.mirror2db;
		LOG_INFO("MIRROR2DB: {}", g_bMirror2DB?"Enabled":"Disabled");
	}
#endif
	if (!InitializeTables())
	{
		LOG_ERROR("Table Initialize FAILED");
		return false;
	}

	CGuildManager::instance().BootReserveWar();

	tmpValue = DbConfig().network.bindPort;

	char szBindIP[128];
	strlcpy(szBindIP, DbConfig().network.bindIp.c_str(), sizeof(szBindIP));

	boost::system::error_code ec;
	auto address = boost::asio::ip::make_address(szBindIP, ec);

	if (ec)
	{
		ec.clear();
		address = boost::asio::ip::address_v4::any();
	}

	const auriga::net::tcp::endpoint endpoint(address, tmpValue);

	auto acceptor = std::make_shared<auriga::net::tcp::acceptor>(auriga::net::Io());

	acceptor->open(endpoint.protocol(), ec);

	if (!ec)
		acceptor->set_option(boost::asio::socket_base::reuse_address(true), ec);

	if (!ec)
		acceptor->bind(endpoint, ec);

	if (!ec)
		acceptor->listen(boost::asio::socket_base::max_listen_connections, ec);

	if (ec)
	{
		LOG_ERROR("ACCEPT_BIND: {} ({})", szBindIP, ec.message().c_str());
		return false;
	}

	m_acceptor = std::move(acceptor);

	LOG_INFO("ACCEPT_HANDLE: {}", tmpValue);
	DoAccept();

	m_looping = true;

	// {
		// LOG_ERROR("conf.txt: Cannot find PLAYER_DELETE_LEVEL_LIMIT, use default level {}", PLAYER_MAX_LEVEL_CONST + 1);
		// m_iPlayerDeleteLevelLimit = PLAYER_MAX_LEVEL_CONST + 1;
	// }

	// {
		// m_iPlayerDeleteLevelLimitLower = 0;
	// }

	// LOG_INFO("PLAYER_DELETE_LEVEL_LIMIT set to {}", m_iPlayerDeleteLevelLimit);
	// LOG_INFO("PLAYER_DELETE_LEVEL_LIMIT_LOWER set to {}", m_iPlayerDeleteLevelLimitLower);

	LoadEventFlag();

	return true;
}

void CClientManager::MainLoop()
{
	SQLMsg * tmp;

	LOG_INFO("ClientManager pointer is {}", static_cast<const void*>(this));

	while (!m_bShutdowned && !auriga::config::ShutdownRequested())
	{
		while ((tmp = CDBManager::instance().PopResult()))
		{
			AnalyzeQueryResult(tmp);
			delete tmp;
		}

		if (!Process())
			break;

		log_rotate();
	}

	LOG_INFO("MainLoop exited, Starting cache flushing");

	signal_timer_disable();

#ifdef __SKILL_COLOR_SYSTEM__
	auto itColor = m_map_SkillColorCache.begin();

	while (itColor != m_map_SkillColorCache.end())
	{
		CSKillColorCache* pCache = itColor->second;
		pCache->Flush();
		m_map_SkillColorCache.erase(itColor++);
	}

	m_map_SkillColorCache.clear();
#endif

	auto it = m_map_playerCache.begin();
	while (it != m_map_playerCache.end())
	{
		CPlayerTableCache * c = it++->second;

		c->Flush();
		delete c;
	}
	m_map_playerCache.clear();



}

void CClientManager::Quit()
{
	m_bShutdowned = true;
}

void CClientManager::QUERY_BOOT(CPeer* peer, TPacketGDBoot * p)
{
	const uint8_t bPacketVersion = DATABASE_BOOT_VERSION;

	std::vector<tAdminInfo> vAdmin;
	std::vector<std::string> vHost;

	__GetHostInfo(vHost);
	__GetAdminInfo(p->szIP, vAdmin);

	LOG_INFO("QUERY_BOOT : AdminInfo (Request ServerIp {}) ", p->szIP);

	uint32_t dwPacketSize =
		sizeof(uint32_t) +
		sizeof(uint8_t) +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TMobTable) * m_vec_mobTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TSkillTable) * m_vec_skillTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TBanwordTable) * m_vec_banwordTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::TLand) * m_vec_kLandTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::TObjectProto) * m_vec_kObjectProto.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::TObject) * m_map_pkObjectTable.size() +
		sizeof(int64_t) +
		//ADMIN_MANAGER
		sizeof(uint16_t) + sizeof(uint16_t) + 16 * vHost.size() +
		sizeof(uint16_t) + sizeof(uint16_t) +  sizeof(tAdminInfo) *  vAdmin.size() +
		//END_ADMIN_MANAGER
		sizeof(uint16_t);

	peer->EncodeHeader(HEADER_DG_BOOT, 0, dwPacketSize);
	peer->Encode(&dwPacketSize, sizeof(uint32_t));
	peer->Encode(&bPacketVersion, sizeof(uint8_t));

	LOG_INFO("BOOT: PACKET: {}", dwPacketSize);
	LOG_INFO("BOOT: VERSION: {}", bPacketVersion);

	LOG_INFO("sizeof(TMobTable) = {}", sizeof(TMobTable));
	LOG_INFO("sizeof(TSkillTable) = {}", sizeof(TSkillTable));
	LOG_INFO("sizeof(TBanwordTable) = {}", sizeof(TBanwordTable));
	LOG_INFO("sizeof(TLand) = {}", sizeof(building::TLand));
	LOG_INFO("sizeof(TObjectProto) = {}", sizeof(building::TObjectProto));
	LOG_INFO("sizeof(TObject) = {}", sizeof(building::TObject));
	//ADMIN_MANAGER
	LOG_INFO("sizeof(tAdminInfo) = {} * {} ", sizeof(tAdminInfo), vAdmin.size());
	//END_ADMIN_MANAGER

	peer->EncodeWORD(sizeof(TMobTable));
    peer->EncodeWORD(m_vec_mobTable.size());
    peer->Encode(m_vec_mobTable.data(), sizeof(TMobTable) * m_vec_mobTable.size());

	peer->EncodeWORD(sizeof(TSkillTable));
	peer->EncodeWORD(m_vec_skillTable.size());
    peer->Encode(m_vec_skillTable.data(), sizeof(TSkillTable) * (uint32_t)m_vec_skillTable.size());

	peer->EncodeWORD(sizeof(TBanwordTable));
	peer->EncodeWORD(m_vec_banwordTable.size());
	peer->Encode(m_vec_banwordTable.data(), sizeof(TBanwordTable) * (uint32_t)m_vec_banwordTable.size());

	peer->EncodeWORD(sizeof(building::TLand));
	peer->EncodeWORD(m_vec_kLandTable.size());
	peer->Encode(m_vec_kLandTable.data(), sizeof(building::TLand) * (uint32_t)m_vec_kLandTable.size());

	peer->EncodeWORD(sizeof(building::TObjectProto));
	peer->EncodeWORD(m_vec_kObjectProto.size());
	peer->Encode(m_vec_kObjectProto.data(), sizeof(building::TObjectProto) * (uint32_t)m_vec_kObjectProto.size());


	peer->EncodeWORD(sizeof(building::TObject));
	peer->EncodeWORD(m_map_pkObjectTable.size());

	auto it = m_map_pkObjectTable.begin();

	while (it != m_map_pkObjectTable.end())
		peer->Encode(it++->second, sizeof(building::TObject));

	// Wire format: explicit int64 (a time_t merete platformfuggo).
	const int64_t now = static_cast<int64_t>(std::time(nullptr));
	peer->Encode(&now, sizeof(now));


	//ADMIN_MANAGER
	peer->EncodeWORD(16);
	peer->EncodeWORD(vHost.size());

	for (size_t n = 0; n < vHost.size(); ++n)
	{
		peer->Encode(vHost[n].c_str(), 16);
		LOG_INFO("GMHosts {}", vHost[n].c_str());
	}

	peer->EncodeWORD(sizeof(tAdminInfo));
	peer->EncodeWORD(vAdmin.size());

	for (size_t n = 0; n < vAdmin.size(); ++n)
	{
		peer->Encode(&vAdmin[n], sizeof(tAdminInfo));
		LOG_INFO("Admin name {} ConntactIP {}", vAdmin[n].m_szName, vAdmin[n].m_szContactIP);
	}
	//END_ADMIN_MANAGER

	peer->EncodeWORD(0xffff);

#ifdef ENABLE_EVENT_MANAGER
	SendEventData(peer);
#endif
}

void CClientManager::SendPartyOnSetup(CPeer* pkPeer)
{
	TPartyMap & pm = m_map_pkChannelParty[pkPeer->GetChannel()];

	for (auto it_party = pm.begin(); it_party != pm.end(); ++it_party)
	{
		LOG_INFO("PARTY SendPartyOnSetup Party [{}]", it_party->first);
		pkPeer->EncodeHeader(HEADER_DG_PARTY_CREATE, 0, sizeof(TPacketPartyCreate));
		pkPeer->Encode(&it_party->first, sizeof(uint32_t));

		for (auto it_member = it_party->second.begin(); it_member != it_party->second.end(); ++it_member)
		{
			LOG_INFO("PARTY SendPartyOnSetup Party [{}] Member [{}]", it_party->first, it_member->first);
			pkPeer->EncodeHeader(HEADER_DG_PARTY_ADD, 0, sizeof(TPacketPartyAdd));
			pkPeer->Encode(&it_party->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->second.bRole, sizeof(uint8_t));

			pkPeer->EncodeHeader(HEADER_DG_PARTY_SET_MEMBER_LEVEL, 0, sizeof(TPacketPartySetMemberLevel));
			pkPeer->Encode(&it_party->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->second.bLevel, sizeof(uint8_t));
		}
	}
}

void CClientManager::QUERY_PLAYER_COUNT(CPeer * pkPeer, TPlayerCountPacket * pPacket)
{
	pkPeer->SetUserCount(pPacket->dwCount);
}

void CClientManager::QUERY_QUEST_SAVE(CPeer * pkPeer, TQuestTable * pTable, uint32_t dwLen)
{
	if (0 != dwLen % sizeof(TQuestTable))
	{
		LOG_ERROR("invalid packet size {}, sizeof(TQuestTable) == {}", dwLen, sizeof(TQuestTable));
		return;
	}

	int iSize = dwLen / sizeof(TQuestTable);

	char szQuery[1024];

	for (int i = 0; i < iSize; ++i, ++pTable)
	{
		if (pTable->lValue == 0)
		{
			snprintf(szQuery, sizeof(szQuery),
					"DELETE FROM quest%s WHERE dwPID=? AND szName=? AND szState=?",
					GetTablePostfix());

			CDBManager::instance().ReturnQueryPrepared(szQuery,
					{
						SqlParam::UInt(pTable->dwPID),
						SqlParam::Text(pTable->szName),
						SqlParam::Text(pTable->szState)
					},
					QID_QUEST_SAVE, pkPeer->GetHandle(), nullptr);
		}
		else
		{
			snprintf(szQuery, sizeof(szQuery),
					"REPLACE INTO quest%s (dwPID, szName, szState, lValue) VALUES(?, ?, ?, ?)",
					GetTablePostfix());

			CDBManager::instance().ReturnQueryPrepared(szQuery,
					{
						SqlParam::UInt(pTable->dwPID),
						SqlParam::Text(pTable->szName),
						SqlParam::Text(pTable->szState),
						SqlParam::Int(pTable->lValue)
					},
					QID_QUEST_SAVE, pkPeer->GetHandle(), nullptr);
		}
	}
}

















// MYSHOP_PRICE_LIST



// END_OF_MYSHOP_PRICE_LIST



void CClientManager::QUERY_EMPIRE_SELECT(CPeer * pkPeer, uint32_t dwHandle, TEmpireSelectPacket * p)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET empire=%u WHERE id=%u", GetTablePostfix(), p->bEmpire, p->dwAccountID);
	CDBManager::instance().AsyncQuery(szQuery);

	LOG_INFO("EmpireSelect: {}", szQuery);
	{
		snprintf(szQuery, sizeof(szQuery),"SELECT pid1, pid2, pid3, pid4 FROM player_index%s WHERE id=%u", GetTablePostfix(), p->dwAccountID);

		std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

		SQLResult * pRes = pmsg->Get();

		if (pRes->uiNumRows)
		{
			LOG_INFO("EMPIRE {}", pRes->uiNumRows);

			MYSQL_ROW row = pRes->FetchRow();
			int pids[PLAYER_PER_ACCOUNT];

			UINT g_start_map[4] =
			{
				0,
				1,
				21,
				41
			};

			uint32_t g_start_position[4][2] =
			{
				{      0,      0 },
	{ 469300, 964200 },
	{  55700, 157900 },
	{ 969600, 278400 }
			};

			for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
			{
				str_to_number(pids[i], row[i]);
				LOG_INFO("EMPIRE PIDS[{}]", pids[i]);

				if (pids[i])
				{
					LOG_INFO("EMPIRE move to pid[{}] to villiage of {}, map_index {}", pids[i], p->bEmpire, g_start_map[p->bEmpire]);

					snprintf(szQuery, sizeof(szQuery), "UPDATE player%s SET map_index=%u,x=%u,y=%u WHERE id=%u",
							GetTablePostfix(),
							g_start_map[p->bEmpire],
							g_start_position[p->bEmpire][0],
							g_start_position[p->bEmpire][1],
							pids[i]);

					std::unique_ptr<SQLMsg> pmsg2(CDBManager::instance().DirectQuery(szQuery));
				}
			}
		}
	}

	pkPeer->EncodeHeader(HEADER_DG_EMPIRE_SELECT, dwHandle, sizeof(uint8_t));
	pkPeer->EncodeBYTE(p->bEmpire);
}

void CClientManager::QUERY_SETUP(CPeer * peer, uint32_t dwHandle, const char * c_pData)
{
	TPacketGDSetup * p = (TPacketGDSetup *) c_pData;
	c_pData += sizeof(TPacketGDSetup);

	if (p->bAuthServer)
	{
#ifdef ENABLE_AUTH_PERFORMANCE
		peer->SetChannel(1);
#endif
		LOG_INFO("AUTH_PEER ptr {}", static_cast<const void*>(peer));

		m_pkAuthPeer = peer;
		return;
	}

	peer->SetPublicIP(p->szPublicIP);
	peer->SetChannel(p->bChannel);
	peer->SetListenPort(p->wListenPort);
	peer->SetP2PPort(p->wP2PPort);
	peer->SetMaps(p->alMaps);

	//
	//
	TMapLocation kMapLocations;

	strlcpy(kMapLocations.szHost, peer->GetPublicIP(), sizeof(kMapLocations.szHost));
	kMapLocations.wPort = peer->GetListenPort();
#ifdef ENABLE_GENERAL_CH
	kMapLocations.bChannel = peer->GetChannel();
#endif
	memcpy(kMapLocations.alMaps, peer->GetMaps(), sizeof(kMapLocations.alMaps));

	uint8_t bMapCount;

	std::vector<TMapLocation> vec_kMapLocations;

#ifdef ENABLE_GENERAL_CH
	for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i) {
		CPeer * tmp = *i;
		if (tmp == peer || !tmp->GetChannel()) {
			continue;
		}

		TMapLocation kMapLocation2;
		strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
		kMapLocation2.wPort = tmp->GetListenPort();
		kMapLocation2.bChannel = tmp->GetChannel();
		memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));

		vec_kMapLocations.push_back(kMapLocation2);

		tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
		bMapCount = 1;
		tmp->EncodeBYTE(bMapCount);
		tmp->Encode(&kMapLocations, sizeof(TMapLocation));

	}
#else
	if (peer->GetChannel() == 1)
	{
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer * tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == GUILD_WARP_WAR_CHANNEL || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;
				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);

				tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
				bMapCount = 1;
				tmp->EncodeBYTE(bMapCount);
				tmp->Encode(&kMapLocations, sizeof(TMapLocation));
			}
		}
	}
	else if (peer->GetChannel() == GUILD_WARP_WAR_CHANNEL)
	{
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer * tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == 1 || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;
				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);
			}

			tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
			bMapCount = 1;
			tmp->EncodeBYTE(bMapCount);
			tmp->Encode(&kMapLocations, sizeof(TMapLocation));
		}
	}
	else
	{
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer * tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == GUILD_WARP_WAR_CHANNEL || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;

				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);
			}

			if (tmp->GetChannel() == peer->GetChannel())
			{
				tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
				bMapCount = 1;
				tmp->EncodeBYTE(bMapCount);
				tmp->Encode(&kMapLocations, sizeof(TMapLocation));
			}
		}
	}
#endif

	vec_kMapLocations.push_back(kMapLocations);

	peer->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation) * (uint32_t)vec_kMapLocations.size());
	bMapCount = vec_kMapLocations.size();
	peer->EncodeBYTE(bMapCount);
	peer->Encode(vec_kMapLocations.data(), sizeof(TMapLocation) * (uint32_t)vec_kMapLocations.size());

	//
	//
	LOG_INFO("SETUP: channel {} listen {} p2p {} count {}", peer->GetChannel(), p->wListenPort, p->wP2PPort, bMapCount);

	TPacketDGP2P p2pSetupPacket;
	p2pSetupPacket.wPort = peer->GetP2PPort();
	p2pSetupPacket.bChannel = peer->GetChannel();
	strlcpy(p2pSetupPacket.szHost, peer->GetPublicIP(), sizeof(p2pSetupPacket.szHost));

	for (auto i = m_peerList.begin(); i != m_peerList.end();++i)
	{
		CPeer * tmp = *i;

		if (tmp == peer)
			continue;

		if (0 == tmp->GetChannel())
			continue;

		tmp->EncodeHeader(HEADER_DG_P2P, 0, sizeof(TPacketDGP2P));
		tmp->Encode(&p2pSetupPacket, sizeof(TPacketDGP2P));
	}

	//
	//
	TPacketLoginOnSetup * pck = (TPacketLoginOnSetup *) c_pData;

	for (uint32_t c = 0; c < p->dwLoginCount; ++c, ++pck)
	{
		CLoginData * pkLD = new CLoginData;

		pkLD->SetKey(pck->dwLoginKey);
		pkLD->SetClientKey(pck->adwClientKey);
		pkLD->SetIP(pck->szHost);

		TAccountTable & r = pkLD->GetAccountRef();

		r.id = pck->dwID;
#ifdef ENABLE_HWID
		strlcpy(r.hwid, pck->hwid, sizeof(r.hwid));
#endif
		trim_and_lower(pck->szLogin, r.login, sizeof(r.login));
		strlcpy(r.social_id, pck->szSocialID, sizeof(r.social_id));
		strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

#ifdef ENABLE_MULTI_LANGUAGE
		r.bLanguage = pck->bLanguage;
#endif

		InsertLoginData(pkLD);

		if (InsertLogonAccount(pck->szLogin, peer->GetHandle(), pck->szHost))
		{
			LOG_INFO("SETUP: login {} {} login_key {} host {}", pck->dwID, pck->szLogin, pck->dwLoginKey, pck->szHost);
			pkLD->SetPlay(true);

		}
		else
			LOG_INFO("SETUP: login_fail {} {} login_key {}", pck->dwID, pck->szLogin, pck->dwLoginKey);
	}


	SendPartyOnSetup(peer);
	CGuildManager::instance().OnSetup(peer);
	CPrivManager::instance().SendPrivOnSetup(peer);
	SendEventFlagsOnSetup(peer);
	marriage::CManager::instance().OnSetup(peer);
}

#ifdef __SKILL_COLOR_SYSTEM__
void CClientManager::QUERY_SKILL_COLOR_SAVE(const char * c_pData)
{
	PutSkillColorCache((TSkillColor*)c_pData);
}
#endif

















// MYSHOP_PRICE_LIST




void CClientManager::UpdatePlayerCache()
{
	auto it = m_map_playerCache.begin();

	while (it != m_map_playerCache.end())
	{
		CPlayerTableCache * c = it++->second;

		if (c->CheckTimeout())
		{
			if (g_log)
				LOG_INFO("UPDATE : UpdatePlayerCache() ==> FlushPlayerCache {} {} ", c->Get(false)->id, c->Get(false)->name);

			c->Flush();

		}
		else if (c->CheckFlushTimeout())
			c->Flush();
	}
}
// END_OF_MYSHOP_PRICE_LIST

#ifdef __SKILL_COLOR_SYSTEM__
CSKillColorCache * CClientManager::GetSkillColorCache(uint32_t id)
{
	const auto it = m_map_SkillColorCache.find(id);

	if (it == m_map_SkillColorCache.end())
		return nullptr;

	return it->second;
}

void CClientManager::PutSkillColorCache(const TSkillColor * pNew)
{
	CSKillColorCache* pCache = GetSkillColorCache(pNew->player_id);

	if (!pCache)
	{
		pCache = new CSKillColorCache;
		m_map_SkillColorCache.insert(TSkillColorCacheMap::value_type(pNew->player_id, pCache));
	}

	pCache->Put(const_cast<TSkillColor*>(pNew), false);
}

void CClientManager::UpdateSkillColorCache()
{
	auto it = m_map_SkillColorCache.begin();

	while (it != m_map_SkillColorCache.end())
	{
		CSKillColorCache* pCache = it->second;

		if (pCache->CheckFlushTimeout())
		{
			pCache->Flush();
			m_map_SkillColorCache.erase(it++);
		}
		else
			++it;
	}
}
#endif

void CClientManager::SetCacheFlushCountLimit(int iLimit)
{
	m_iCacheFlushCountLimit = MAX(10, iLimit);
	LOG_INFO("CACHE_FLUSH_LIMIT_PER_SECOND: {}", m_iCacheFlushCountLimit);
}







void CClientManager::QUERY_FLUSH_CACHE(CPeer * pkPeer, const char * c_pData)
{
	uint32_t dwPID = *(uint32_t *) c_pData;

	CPlayerTableCache * pkCache = GetPlayerCache(dwPID);

	if (!pkCache)
		return;

	LOG_INFO("FLUSH_CACHE: {}", dwPID);

	pkCache->Flush();

	m_map_playerCache.erase(dwPID);
	delete pkCache;
}

void CClientManager::QUERY_SMS(CPeer * pkPeer, TPacketGDSMS * pack)
{
	CDBManager::instance().QueryPrepared(
			"INSERT INTO sms_pool (server, sender, receiver, mobile, msg) VALUES(?, ?, ?, ?, ?)",
			{
				SqlParam::Int((m_iPlayerIDStart + 2) / 3),
				SqlParam::Text(pack->szFrom),
				SqlParam::Text(pack->szTo),
				SqlParam::Text(pack->szMobile),
				SqlParam::Text(pack->szMsg)
			});
}

void CClientManager::QUERY_RELOAD_PROTO()
{
	if (!InitializeTables())
	{
		LOG_ERROR("QUERY_RELOAD_PROTO: cannot load tables");
		return;
	}

	for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
	{
		CPeer * tmp = *i;

		if (!tmp->GetChannel())
			continue;

		tmp->EncodeHeader(HEADER_DG_RELOAD_PROTO, 0,
				sizeof(uint16_t) + sizeof(TSkillTable) * m_vec_skillTable.size() +
				sizeof(uint16_t) + sizeof(TBanwordTable) * m_vec_banwordTable.size() +
				sizeof(uint16_t) + sizeof(TMobTable) * m_vec_mobTable.size());

		tmp->EncodeWORD(m_vec_skillTable.size());
		tmp->Encode(m_vec_skillTable.data(), sizeof(TSkillTable) * m_vec_skillTable.size());

		tmp->EncodeWORD(m_vec_banwordTable.size());
		tmp->Encode(m_vec_banwordTable.data(), sizeof(TBanwordTable) * m_vec_banwordTable.size());


		tmp->EncodeWORD(m_vec_mobTable.size());
		tmp->Encode(m_vec_mobTable.data(), sizeof(TMobTable) * m_vec_mobTable.size());
	}
}

// ADD_GUILD_PRIV_TIME
void CClientManager::AddGuildPriv(TPacketGiveGuildPriv* p)
{
	CPrivManager::instance().AddGuildPriv(p->guild_id, p->type, p->value, p->duration_sec);
#ifdef ENABLE_DEFAULT_PRIV
	__UpdateDefaultPriv("GUILD", p->guild_id, p->type, p->value, p->duration_sec);
#endif
}

void CClientManager::AddEmpirePriv(TPacketGiveEmpirePriv* p)
{
	CPrivManager::instance().AddEmpirePriv(p->empire, p->type, p->value, p->duration_sec);
#ifdef ENABLE_DEFAULT_PRIV
	__UpdateDefaultPriv("EMPIRE", p->empire, p->type, p->value, p->duration_sec);
#endif
}
// END_OF_ADD_GUILD_PRIV_TIME

void CClientManager::AddCharacterPriv(TPacketGiveCharacterPriv* p)
{
	CPrivManager::instance().AddCharPriv(p->pid, p->type, p->value);
#ifdef ENABLE_DEFAULT_PRIV
	__UpdateDefaultPriv("PLAYER", p->pid, p->type, p->value, 0);
#endif
}

void CClientManager::MoneyLog(TPacketMoneyLog* p)
{
	CMoneyLog::instance().AddLog(p->type, p->vnum, p->gold);
}

CLoginData * CClientManager::GetLoginData(uint32_t dwKey)
{
	const auto it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return nullptr;

	return it->second;
}

CLoginData * CClientManager::GetLoginDataByLogin(const char * c_pszLogin)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(c_pszLogin, szLogin, sizeof(szLogin));

	const auto it = m_map_pkLoginDataByLogin.find(szLogin);

	if (it == m_map_pkLoginDataByLogin.end())
		return nullptr;

	return it->second;
}

CLoginData * CClientManager::GetLoginDataByAID(uint32_t dwAID)
{
	const auto it = m_map_pkLoginDataByAID.find(dwAID);

	if (it == m_map_pkLoginDataByAID.end())
		return nullptr;

	return it->second;
}

void CClientManager::InsertLoginData(CLoginData * pkLD)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(pkLD->GetAccountRef().login, szLogin, sizeof(szLogin));

	m_map_pkLoginData.insert(std::make_pair(pkLD->GetKey(), pkLD));
	m_map_pkLoginDataByLogin.insert(std::make_pair(szLogin, pkLD));
	m_map_pkLoginDataByAID.insert(std::make_pair(pkLD->GetAccountRef().id, pkLD));
}

void CClientManager::DeleteLoginData(CLoginData * pkLD)
{
    m_playerSessions.InvalidateAccount(pkLD->GetAccountRef().id);
	m_map_pkLoginData.erase(pkLD->GetKey());
	m_map_pkLoginDataByLogin.erase(pkLD->GetAccountRef().login);
	m_map_pkLoginDataByAID.erase(pkLD->GetAccountRef().id);

	if (!m_map_kLogonAccount.contains(pkLD->GetAccountRef().login))
		delete pkLD;
	else
		pkLD->SetDeleted(true);
}

void CClientManager::QUERY_AUTH_LOGIN(CPeer * pkPeer, uint32_t dwHandle, TPacketGDAuthLogin * p)
{
	if (g_test_server)
		LOG_INFO("QUERY_AUTH_LOGIN {} {} {}", p->dwID, p->dwLoginKey, p->szLogin);
	CLoginData * pkLD = GetLoginDataByLogin(p->szLogin);

	if (pkLD)
	{
		DeleteLoginData(pkLD);
	}

	uint8_t bResult;

	if (GetLoginData(p->dwLoginKey))
	{
		LOG_ERROR("LoginData already exist key {} login {}", p->dwLoginKey, p->szLogin);
		bResult = 0;

		pkPeer->EncodeHeader(HEADER_DG_AUTH_LOGIN, dwHandle, sizeof(uint8_t));
		pkPeer->EncodeBYTE(bResult);
	}
	else
	{
		CLoginData * pkLD = new CLoginData;

		pkLD->SetKey(p->dwLoginKey);
		pkLD->SetClientKey(p->adwClientKey);
		pkLD->SetPremium(p->iPremiumTimes);

		TAccountTable & r = pkLD->GetAccountRef();

		r.id = p->dwID;
#ifdef ENABLE_HWID
		strlcpy(r.hwid, p->hwid, sizeof(r.hwid));
#endif
		trim_and_lower(p->szLogin, r.login, sizeof(r.login));
		strlcpy(r.social_id, p->szSocialID, sizeof(r.social_id));
		strlcpy(r.passwd, "TEMP", sizeof(r.passwd));
#ifdef ENABLE_MULTI_LANGUAGE
		r.bLanguage = p->bLanguage;
#endif

		LOG_INFO("AUTH_LOGIN id({}) login({}) social_id({}) login_key({}), client_key({} {} {} {})", p->dwID, p->szLogin, p->szSocialID, p->dwLoginKey, p->adwClientKey[0], p->adwClientKey[1], p->adwClientKey[2], p->adwClientKey[3]);
		bResult = 1;

		InsertLoginData(pkLD);

		pkPeer->EncodeHeader(HEADER_DG_AUTH_LOGIN, dwHandle, sizeof(uint8_t));
		pkPeer->EncodeBYTE(bResult);
	}
}

void CClientManager::GuildDepositMoney(TPacketGDGuildMoney* p)
{
	CGuildManager::instance().DepositMoney(p->dwGuild, p->iGold);
}

void CClientManager::GuildWithdrawMoney(CPeer* peer, TPacketGDGuildMoney* p)
{
	CGuildManager::instance().WithdrawMoney(peer, p->dwGuild, p->iGold);
}

void CClientManager::GuildWithdrawMoneyGiveReply(TPacketGDGuildMoneyWithdrawGiveReply* p)
{
	CGuildManager::instance().WithdrawMoneyReply(p->dwGuild, p->bGiveSuccess, p->iChangeGold);
}

void CClientManager::GuildWarBet(TPacketGDGuildWarBet * p)
{
	CGuildManager::instance().Bet(p->dwWarID, p->szLogin, p->dwGold, p->dwGuild);
}

void CClientManager::CreateObject(TPacketGDCreateObject * p)
{
	using namespace building;

	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery),
			"INSERT INTO object%s (land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot) VALUES(%u, %u, %d, %d, %d, %f, %f, %f)",
			GetTablePostfix(), p->dwLandID, p->dwVnum, p->lMapIndex, p->x, p->y, p->xRot, p->yRot, p->zRot);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiInsertID == 0)
	{
		LOG_ERROR("cannot insert object");
		return;
	}

	auto pkObj = new TObject;

	memset(pkObj, 0, sizeof(TObject));

	pkObj->dwID = pmsg->Get()->uiInsertID;
	pkObj->dwVnum = p->dwVnum;
	pkObj->dwLandID = p->dwLandID;
	pkObj->lMapIndex = p->lMapIndex;
	pkObj->x = p->x;
	pkObj->y = p->y;
	pkObj->xRot = p->xRot;
	pkObj->yRot = p->yRot;
	pkObj->zRot = p->zRot;
	pkObj->lLife = 0;

	ForwardPacket(HEADER_DG_CREATE_OBJECT, pkObj, sizeof(TObject));

	m_map_pkObjectTable.insert(std::make_pair(pkObj->dwID, pkObj));
}

void CClientManager::DeleteObject(uint32_t dwID)
{
	char szQuery[128];

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM object%s WHERE id=%u", GetTablePostfix(), dwID);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiAffectedRows == 0 || std::cmp_equal(pmsg->Get()->uiAffectedRows, -1))
	{
		LOG_ERROR("no object by id {}", dwID);
		return;
	}

	if (const auto it = m_map_pkObjectTable.find(dwID); it != m_map_pkObjectTable.end())
	{
		delete it->second;
		m_map_pkObjectTable.erase(it);
	}
	++m_buildingDeleteEpoch;

	ForwardPacket(HEADER_DG_DELETE_OBJECT, &dwID, sizeof(uint32_t));
}

void CClientManager::UpdateLand(uint32_t * pdw)
{
	uint32_t dwID = pdw[0];
	uint32_t dwGuild = pdw[1];

	building::TLand * p = m_vec_kLandTable.data();

	uint32_t i;

	for (i = 0; i < m_vec_kLandTable.size(); ++i, ++p)
	{
		if (p->dwID == dwID)
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "UPDATE land%s SET guild_id=%u WHERE id=%u", GetTablePostfix(), dwGuild, dwID);
			CDBManager::instance().AsyncQuery(buf);

			p->dwGuildID = dwGuild;
			break;
		}
	}

	if (i < m_vec_kLandTable.size())
		ForwardPacket(HEADER_DG_UPDATE_LAND, p, sizeof(building::TLand));
}

// BLOCK_CHAT
void CClientManager::BlockChat(TPacketBlockChat* p)
{
	char szQuery[256];

	if (g_stLocale == "sjis")
		snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player%s WHERE name = ? collate sjis_japanese_ci", GetTablePostfix());
	else
		snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player%s WHERE name = ?", GetTablePostfix());
	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQueryPrepared(szQuery, { SqlParam::Text(p->szName) }));
	SQLResult * pRes = pmsg->Get();

	if (pRes->uiNumRows)
	{
		MYSQL_ROW row = pRes->FetchRow();
		uint32_t pid = strtoul(row[0], nullptr, 10);

		TPacketGDAddAffect pa;
		pa.dwPID = pid;
		pa.elem.dwType = 223;
		pa.elem.bApplyOn = 0;
		pa.elem.lApplyValue = 0;
		pa.elem.dwFlag = 0;
		pa.elem.lDuration = p->lDuration;
		pa.elem.lSPCost = 0;
		QUERY_ADD_AFFECT(nullptr, &pa);
	}
	else
	{
		// cannot find user with that name
	}
}
// END_OF_BLOCK_CHAT

void CClientManager::MarriageAdd(TPacketMarriageAdd * p)
{
	LOG_INFO("MarriageAdd {} {} {} {}", p->dwPID1, p->dwPID2, p->szName1, p->szName2);
	marriage::CManager::instance().Add(p->dwPID1, p->dwPID2, p->szName1, p->szName2);
}

void CClientManager::MarriageUpdate(TPacketMarriageUpdate * p)
{
	LOG_INFO("MarriageUpdate PID:{} {} LP:{} ST:{}", p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
	marriage::CManager::instance().Update(p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
}

void CClientManager::MarriageRemove(TPacketMarriageRemove * p)
{
	LOG_INFO("MarriageRemove {} {}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().Remove(p->dwPID1, p->dwPID2);
}

void CClientManager::WeddingRequest(TPacketWeddingRequest * p)
{
	LOG_INFO("WeddingRequest {} {}", p->dwPID1, p->dwPID2);
	ForwardPacket(HEADER_DG_WEDDING_REQUEST, p, sizeof(TPacketWeddingRequest));
	//marriage::CManager::instance().RegisterWedding(p->dwPID1, p->szName1, p->dwPID2, p->szName2);
}

void CClientManager::WeddingReady(TPacketWeddingReady * p)
{
	LOG_INFO("WeddingReady {} {}", p->dwPID1, p->dwPID2);
	ForwardPacket(HEADER_DG_WEDDING_READY, p, sizeof(TPacketWeddingReady));
	marriage::CManager::instance().ReadyWedding(p->dwMapIndex, p->dwPID1, p->dwPID2);
}

void CClientManager::WeddingEnd(TPacketWeddingEnd * p)
{
	LOG_INFO("WeddingEnd {} {}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().EndWedding(p->dwPID1, p->dwPID2);
}

//
//


// MYSHOP_PRICE_LIST
//

// END_OF_MYSHOP_PRICE_LIST

void CPacketInfo::Add(int header)
{
	auto it = m_map_info.find(header);

	if (it == m_map_info.end())
		m_map_info.insert(std::map<int, int>::value_type(header, 1));
	else
		++it->second;
}

void CPacketInfo::Reset()
{
	m_map_info.clear();
}

namespace
{
	// Minimum payload (dwLength) for the fixed layout GameServer -> Database
	// messages. Headers whose handlers parse variable length data (item
	// cache, offline shop, event manager, ...) are either covered with the
	// smallest structure the parser reads or intentionally left out.
	const std::unordered_map<uint8_t, uint32_t>& GetMinPeerPayloadSizes()
	{
		static const std::unordered_map<uint8_t, uint32_t> s_sizes =
		{
			{ HEADER_GD_BOOT, sizeof(TPacketGDBoot) },
			{ HEADER_GD_LOGIN_BY_KEY, sizeof(TPacketGDLoginByKey) },
			{ HEADER_GD_LOGOUT, sizeof(TLogoutPacket) },
			{ HEADER_GD_PLAYER_LOAD, sizeof(TPlayerLoadPacket) },
			{ HEADER_GD_PLAYER_SAVE, sizeof(TPlayerTable) },
			{ HEADER_GD_PLAYER_CREATE, sizeof(TPlayerCreatePacket) },
			{ HEADER_GD_PLAYER_DELETE, sizeof(TPlayerDeletePacket) },
			{ HEADER_GD_PLAYER_COUNT, sizeof(TPlayerCountPacket) },
			{ HEADER_GD_QUEST_SAVE, sizeof(TQuestTable) },
			{ HEADER_GD_SAFEBOX_LOAD, sizeof(TSafeboxLoadPacket) },
			{ HEADER_GD_SAFEBOX_SAVE, sizeof(TSafeboxTable) },
			{ HEADER_GD_SAFEBOX_CHANGE_SIZE, sizeof(TSafeboxChangeSizePacket) },
			{ HEADER_GD_SAFEBOX_CHANGE_PASSWORD, sizeof(TSafeboxChangePasswordPacket) },
			{ HEADER_GD_MALL_LOAD, sizeof(TSafeboxLoadPacket) },
			{ HEADER_GD_EMPIRE_SELECT, sizeof(TEmpireSelectPacket) },
			{ HEADER_GD_SETUP, sizeof(TPacketGDSetup) },
			{ HEADER_GD_GUILD_CREATE, sizeof(uint32_t) },
			{ HEADER_GD_GUILD_SKILL_UPDATE, sizeof(TPacketGuildSkillUpdate) },
			{ HEADER_GD_GUILD_EXP_UPDATE, sizeof(TPacketGuildExpUpdate) },
			{ HEADER_GD_GUILD_ADD_MEMBER, sizeof(TPacketGDGuildAddMember) },
			{ HEADER_GD_GUILD_REMOVE_MEMBER, sizeof(TPacketGuild) },
			{ HEADER_GD_GUILD_CHANGE_GRADE, sizeof(TPacketGuild) },
			{ HEADER_GD_GUILD_CHANGE_MEMBER_DATA, sizeof(TPacketGuildChangeMemberData) },
			{ HEADER_GD_GUILD_DISBAND, sizeof(TPacketGuild) },
			{ HEADER_GD_GUILD_WAR, sizeof(TPacketGuildWar) },
			{ HEADER_GD_GUILD_WAR_SCORE, sizeof(TPacketGuildWarScore) },
			{ HEADER_GD_GUILD_CHANGE_LADDER_POINT, sizeof(TPacketGuildLadderPoint) },
			{ HEADER_GD_GUILD_USE_SKILL, sizeof(TPacketGuildUseSkill) },
			{ HEADER_GD_FLUSH_CACHE, sizeof(uint32_t) },
			{ HEADER_GD_ITEM_DESTROY, sizeof(uint32_t) * 2 },
			{ HEADER_GD_ITEM_FLUSH, sizeof(uint32_t) },
			{ HEADER_GD_ADD_AFFECT, sizeof(TPacketGDAddAffect) },
			{ HEADER_GD_REMOVE_AFFECT, sizeof(TPacketGDRemoveAffect) },
			{ HEADER_GD_HIGHSCORE_REGISTER, sizeof(TPacketGDHighscore) },
			{ HEADER_GD_PARTY_CREATE, sizeof(TPacketPartyCreate) },
			{ HEADER_GD_PARTY_DELETE, sizeof(TPacketPartyDelete) },
			{ HEADER_GD_PARTY_ADD, sizeof(TPacketPartyAdd) },
			{ HEADER_GD_PARTY_REMOVE, sizeof(TPacketPartyRemove) },
			{ HEADER_GD_PARTY_STATE_CHANGE, sizeof(TPacketPartyStateChange) },
			{ HEADER_GD_PARTY_SET_MEMBER_LEVEL, sizeof(TPacketPartySetMemberLevel) },
			{ HEADER_GD_CHANGE_NAME, sizeof(TPacketGDChangeName) },
			{ HEADER_GD_SMS, sizeof(TPacketGDSMS) },
			{ HEADER_GD_AUTH_LOGIN, sizeof(TPacketGDAuthLogin) },
			{ HEADER_GD_REQUEST_GUILD_PRIV, sizeof(TPacketGiveGuildPriv) },
			{ HEADER_GD_REQUEST_EMPIRE_PRIV, sizeof(TPacketGiveEmpirePriv) },
			{ HEADER_GD_REQUEST_CHARACTER_PRIV, sizeof(TPacketGiveCharacterPriv) },
			{ HEADER_GD_MONEY_LOG, sizeof(TPacketMoneyLog) },
			{ HEADER_GD_GUILD_DEPOSIT_MONEY, sizeof(TPacketGDGuildMoney) },
			{ HEADER_GD_GUILD_WITHDRAW_MONEY, sizeof(TPacketGDGuildMoney) },
			{ HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY, sizeof(TPacketGDGuildMoneyWithdrawGiveReply) },
			{ HEADER_GD_GUILD_WAR_BET, sizeof(TPacketGDGuildWarBet) },
			{ HEADER_GD_SET_EVENT_FLAG, sizeof(TPacketSetEventFlag) },
			{ HEADER_GD_CREATE_OBJECT, sizeof(TPacketGDCreateObject) },
			{ HEADER_GD_DELETE_OBJECT, sizeof(uint32_t) },
			{ HEADER_GD_MARRIAGE_ADD, sizeof(TPacketMarriageAdd) },
			{ HEADER_GD_MARRIAGE_UPDATE, sizeof(TPacketMarriageUpdate) },
			{ HEADER_GD_MARRIAGE_REMOVE, sizeof(TPacketMarriageRemove) },
			{ HEADER_GD_WEDDING_REQUEST, sizeof(TPacketWeddingRequest) },
			{ HEADER_GD_WEDDING_READY, sizeof(TPacketWeddingReady) },
			{ HEADER_GD_WEDDING_END, sizeof(TPacketWeddingEnd) },
			{ HEADER_GD_BLOCK_CHAT, sizeof(TPacketBlockChat) },
			{ HEADER_GD_MYSHOP_PRICELIST_UPDATE, sizeof(TPacketMyshopPricelistHeader) },
			{ HEADER_GD_MYSHOP_PRICELIST_REQ, sizeof(uint32_t) },
			{ HEADER_GD_RELOAD_ADMIN, sizeof(TPacketReloadAdmin) },
			{ HEADER_GD_REQ_CHANGE_GUILD_MASTER, sizeof(TPacketChangeGuildMaster) },
			{ HEADER_GD_UPDATE_HORSE_NAME, sizeof(TPacketUpdateHorseName) },
			{ HEADER_GD_REQ_HORSE_NAME, sizeof(uint32_t) },
			{ HEADER_GD_DC, sizeof(TPacketDC) },
			{ HEADER_GD_VALID_LOGOUT, sizeof(TPacketNeedLoginLogInfo) },
			{ HEADER_GD_REQUEST_CHARGE_CASH, sizeof(TRequestChargeCash) },
			{ HEADER_GD_DELETE_AWARDID, sizeof(TPacketDeleteAwardID) },
			{ HEADER_GD_UPDATE_CHANNELSTATUS, sizeof(SChannelStatus) },
			{ HEADER_GD_BREAK_MARRIAGE, sizeof(uint32_t) * 2 },
#ifdef BL_OFFLINE_MESSAGE
			{ HEADER_GD_REQUEST_OFFLINE_MESSAGES, sizeof(TPacketGDReadOfflineMessage) },
			{ HEADER_GD_SEND_OFFLINE_MESSAGE, sizeof(TPacketGDSendOfflineMessage) },
#endif
#ifdef ENABLE_HWID
			{ HEADER_GD_BLOCKHWID, sizeof(THwidRequest) },
			{ HEADER_GD_UNBLOCKHWID, sizeof(THwidRequest) },
#endif
#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
			{ HEADER_GD_FIND_CHANNEL, sizeof(TPacketChangeChannel) },
#endif
#ifdef __SKILL_COLOR_SYSTEM__
			{ HEADER_GD_SKILL_COLOR_SAVE, sizeof(TSkillColor) },
#endif
#ifdef ENABLE_EVENT_MANAGER
			{ HEADER_GD_EVENT_MANAGER, sizeof(uint8_t) + sizeof(uint16_t) },
#endif
#ifdef ENABLE_ITEMSHOP
			{ HEADER_GD_ITEMSHOP, sizeof(std::uint8_t) },
#endif
		};

		return s_sizes;
	}
}

void CClientManager::ProcessPackets(CPeer * peer)
{
	uint8_t		header;
	uint32_t		dwHandle;
	uint32_t		dwLength;
	const char * data = nullptr;
	int			i = 0;
	int			iCount = 0;

	while (peer->PeekPacket(i, header, dwHandle, dwLength, &data))
	{
		// DISABLE_DB_HEADER_LOG
		// LOG_INFO("header {} {} size {}", header, this, dwLength);
		// END_OF_DISABLE_DB_HEADER_LOG

		if (!peer->IsAuthenticated())
		{
			if (header != HEADER_GD_AUTH || dwLength != static_cast<uint32_t>(PeerAuth::MAC_SIZE))
			{
				LOG_ERROR("Unauthenticated peer packet (header {} length {}) from {} - closing", header, dwLength, peer->GetHost());
				RemovePeer(peer);
				return;
			}

			const auto * payload = reinterpret_cast<const uint8_t *>(data);
			const bool ok = peer->VerifyAuthResponse(payload);

			peer->EncodeHeader(HEADER_DG_AUTH_RESULT, 0, 1);
			peer->EncodeBYTE(ok ? 1 : 0);
			peer->Send();

			peer->SetAuthenticated(ok);

			if (!ok)
			{
				LOG_ERROR("Peer auth failed from {}", peer->GetHost());
				RemovePeer(peer);
				return;
			}

			continue;
		}

        // Fail closed even for authenticated, outdated peers. No old item writer
        // may recreate retired SQL state after the native-only schema cutover.
        if (header == HEADER_GD_ITEM_SAVE ||
            header == HEADER_GD_ITEM_DESTROY ||
            header == HEADER_GD_ITEM_FLUSH ||
            header == HEADER_GD_SAFEBOX_LOAD ||
            header == HEADER_GD_SAFEBOX_SAVE ||
            header == HEADER_GD_SAFEBOX_CHANGE_SIZE ||
            header == HEADER_GD_SAFEBOX_CHANGE_PASSWORD ||
            header == HEADER_GD_MALL_LOAD ||
            header == HEADER_GD_MYSHOP_PRICELIST_UPDATE ||
            header == HEADER_GD_MYSHOP_PRICELIST_REQ ||
            header == HEADER_GD_DELETE_AWARDID)
        {
            LOG_ERROR("Retired item persistence packet {} from {}", header, peer->GetHost());
            RemovePeer(peer);
            return;
        }
		m_bLastHeader = header;
		++iCount;

		// Strict per-message size check: the handlers below cast the payload
		// to fixed structures, a short frame must never reach them.
		const auto minSizeIt = GetMinPeerPayloadSizes().find(header);

		if (minSizeIt != GetMinPeerPayloadSizes().end() && dwLength < minSizeIt->second)
		{
			LOG_ERROR("Peer packet too short (header {} length {} minimum {}) from {} - dropping",
				header, dwLength, minSizeIt->second, peer->GetHost());
			continue;
		}

#ifdef _TEST
		if (header != 10)
			LOG_INFO(" ProcessPacket Header [{}] Handle[{}] Length[{}] iCount[{}]", header, dwHandle, dwLength, iCount);
#endif
		if (g_test_server)
		{
			if (header != 10)
				LOG_INFO(" ProcessPacket Header [{}] Handle[{}] Length[{}] iCount[{}]", header, dwHandle, dwLength, iCount);
		}

		switch (header)
		{
			case HEADER_GD_BOOT:
				QUERY_BOOT(peer, (TPacketGDBoot *) data);
				break;

			case HEADER_GD_HAMMER_OF_TOR:
				break;

			case HEADER_GD_PING:
				peer->EncodeReturn(HEADER_DG_PONG, 0);
				peer->Send();
				break;

			case HEADER_GD_PONG:
				break;

			case HEADER_GD_LOGIN_BY_KEY:
				QUERY_LOGIN_BY_KEY(peer, dwHandle, (TPacketGDLoginByKey *) data);
				break;

			case HEADER_GD_LOGOUT:
				//LOG_INFO("HEADER_GD_LOGOUT (handle: {} length: {})", dwHandle, dwLength);
                if (dwLength != sizeof(TLogoutPacket)) break;
				QUERY_LOGOUT(peer, dwHandle, data);
				break;

            case HEADER_GD_PLAYER_SESSION_HELLO:
            {
                auto link = auriga::player_session::DecodeHello(
                    {reinterpret_cast<const uint8_t*>(data), dwLength}, false);
                if (!PeerAuth::HasKey() || !peer->GetChannel() || !link || !peer->EnablePlayerSessions(*link)) break;
                const auto reply = auriga::player_session::Hello(*link, true);
                peer->EncodeHeader(HEADER_DG_PLAYER_SESSION_HELLO, 0, reply.size());
                peer->Encode(reply.data(), reply.size());
                break;
            }
            case HEADER_GD_PLAYER_CACHE_HELLO:
            {
                auto link = auriga::player_cache_wire::DecodeHello(
                    {reinterpret_cast<const uint8_t*>(data), dwLength}, false);
                if (!PeerAuth::HasKey() || !peer->IsAuthenticated() || !link ||
                    *link != peer->PlayerSessionLink()) break;
                const auto frame = auriga::player_cache_wire::Hello(*link, true);
                peer->EncodeHeader(HEADER_DG_PLAYER_CACHE_HELLO, 0, frame.size());
                peer->Encode(frame.data(), frame.size());
                break;
            }
            case HEADER_GD_PLAYER_CACHE_REQUEST:
                HandlePlayerCacheRequest(peer, dwHandle, {reinterpret_cast<const uint8_t*>(data), dwLength});
                break;
            case HEADER_GD_PLAYER_LOAD_SESSION:
            {
                auto request = auriga::player_session::DecodeRequest(
                    {reinterpret_cast<const uint8_t*>(data), dwLength});
                if (!PeerAuth::HasKey() || !request || request->link != peer->PlayerSessionLink()) break;
                TPlayerLoadPacket packet{};
                packet.account_id = request->account;
                packet.player_id = request->character;
                packet.account_index = request->slot;
                if (QUERY_PLAYER_LOAD(peer, dwHandle, &packet, request->link, request->request))
                {
#ifdef __SKILL_COLOR_SYSTEM__
                    QUERY_SKILL_COLOR_LOAD(peer, dwHandle, &packet);
#endif
                }
                break;
            }
            case HEADER_GD_PLAYER_LOAD:
                if (dwLength != sizeof(TPlayerLoadPacket)) break;
                if (QUERY_PLAYER_LOAD(peer, dwHandle, (TPlayerLoadPacket*)data))
                {
#ifdef __SKILL_COLOR_SYSTEM__
                    QUERY_SKILL_COLOR_LOAD(peer, dwHandle, (TPlayerLoadPacket*)data);
#endif
                }
                break;

			case HEADER_GD_PLAYER_SAVE:
#ifdef HEADER_GD_PLAYER_SAVE_SIZE_CHECK
				if (dwLength != sizeof(TPlayerTable)) {
					LOG_ERROR("PLAYER_SAVE size mismatch got={} expected={} - DROP", dwLength, sizeof(TPlayerTable));
					break;
				}
#endif
				LOG_TRACE("HEADER_GD_PLAYER_SAVE (handle: {} length: {})", dwHandle, dwLength);
				QUERY_PLAYER_SAVE(peer, dwHandle, (TPlayerTable *) data);
				break;

			case HEADER_GD_PLAYER_CREATE:
				LOG_INFO("HEADER_GD_PLAYER_CREATE (handle: {} length: {})", dwHandle, dwLength);
				__QUERY_PLAYER_CREATE(peer, dwHandle, (TPlayerCreatePacket *) data);
				LOG_INFO("END");
				break;

			case HEADER_GD_PLAYER_DELETE:
				LOG_TRACE("HEADER_GD_PLAYER_DELETE (handle: {} length: {})", dwHandle, dwLength);
				__QUERY_PLAYER_DELETE(peer, dwHandle, (TPlayerDeletePacket *) data);
				break;

			case HEADER_GD_PLAYER_COUNT:
				QUERY_PLAYER_COUNT(peer, (TPlayerCountPacket *) data);
				break;

			case HEADER_GD_QUEST_SAVE:
				LOG_TRACE("HEADER_GD_QUEST_SAVE (handle: {} length: {})", dwHandle, dwLength);
				QUERY_QUEST_SAVE(peer, (TQuestTable *) data, dwLength);
				break;






			case HEADER_GD_EMPIRE_SELECT:
				QUERY_EMPIRE_SELECT(peer, dwHandle, (TEmpireSelectPacket *) data);
				break;

			case HEADER_GD_SETUP:
				QUERY_SETUP(peer, dwHandle, data);
				break;

			case HEADER_GD_GUILD_CREATE:
				GuildCreate(peer, *(uint32_t *) data);
				break;

			case HEADER_GD_GUILD_SKILL_UPDATE:
				GuildSkillUpdate(peer, (TPacketGuildSkillUpdate *) data);
				break;

			case HEADER_GD_GUILD_EXP_UPDATE:
				GuildExpUpdate(peer, (TPacketGuildExpUpdate *) data);
				break;

			case HEADER_GD_GUILD_ADD_MEMBER:
				GuildAddMember(peer, (TPacketGDGuildAddMember*) data);
				break;

			case HEADER_GD_GUILD_REMOVE_MEMBER:
				GuildRemoveMember(peer, (TPacketGuild*) data);
				break;

			case HEADER_GD_GUILD_CHANGE_GRADE:
				GuildChangeGrade(peer, (TPacketGuild*) data);
				break;

			case HEADER_GD_GUILD_CHANGE_MEMBER_DATA:
				GuildChangeMemberData(peer, (TPacketGuildChangeMemberData*) data);
				break;

			case HEADER_GD_GUILD_DISBAND:
				GuildDisband(peer, (TPacketGuild*) data);
				break;

			case HEADER_GD_GUILD_WAR:
				GuildWar(peer, (TPacketGuildWar*) data);
				break;

			case HEADER_GD_GUILD_WAR_SCORE:
				GuildWarScore(peer, (TPacketGuildWarScore*) data);
				break;
				
#ifdef ADVANCED_GUILD_INFO
			case HEADER_GD_GUILD_RESET:
				GuildResetInfo();
				break;
#endif				

			case HEADER_GD_GUILD_CHANGE_LADDER_POINT:
				GuildChangeLadderPoint((TPacketGuildLadderPoint*) data);
				break;

			case HEADER_GD_GUILD_USE_SKILL:
				GuildUseSkill((TPacketGuildUseSkill*) data);
				break;

			case HEADER_GD_FLUSH_CACHE:
				QUERY_FLUSH_CACHE(peer, data);
				break;




			case HEADER_GD_ADD_AFFECT:
				LOG_TRACE("HEADER_GD_ADD_AFFECT");
				QUERY_ADD_AFFECT(peer, (TPacketGDAddAffect *) data);
				break;

			case HEADER_GD_REMOVE_AFFECT:
				LOG_TRACE("HEADER_GD_REMOVE_AFFECT");
				QUERY_REMOVE_AFFECT(peer, (TPacketGDRemoveAffect *) data);
				break;

			case HEADER_GD_HIGHSCORE_REGISTER:
				QUERY_HIGHSCORE_REGISTER(peer, (TPacketGDHighscore *) data);
				break;

			case HEADER_GD_PARTY_CREATE:
				QUERY_PARTY_CREATE(peer, (TPacketPartyCreate*) data);
				break;

			case HEADER_GD_PARTY_DELETE:
				QUERY_PARTY_DELETE(peer, (TPacketPartyDelete*) data);
				break;

			case HEADER_GD_PARTY_ADD:
				QUERY_PARTY_ADD(peer, (TPacketPartyAdd*) data);
				break;

			case HEADER_GD_PARTY_REMOVE:
				QUERY_PARTY_REMOVE(peer, (TPacketPartyRemove*) data);
				break;

			case HEADER_GD_PARTY_STATE_CHANGE:
				QUERY_PARTY_STATE_CHANGE(peer, (TPacketPartyStateChange*) data);
				break;

			case HEADER_GD_PARTY_SET_MEMBER_LEVEL:
				QUERY_PARTY_SET_MEMBER_LEVEL(peer, (TPacketPartySetMemberLevel*) data);
				break;

			case HEADER_GD_RELOAD_PROTO:
				QUERY_RELOAD_PROTO();
				break;

			case HEADER_GD_CHANGE_NAME:
				QUERY_CHANGE_NAME(peer, dwHandle, (TPacketGDChangeName *) data);
				break;

			case HEADER_GD_SMS:
				QUERY_SMS(peer, (TPacketGDSMS *) data);
				break;

			case HEADER_GD_AUTH_LOGIN:
				QUERY_AUTH_LOGIN(peer, dwHandle, (TPacketGDAuthLogin *) data);
				break;

			case HEADER_GD_REQUEST_GUILD_PRIV:
				AddGuildPriv((TPacketGiveGuildPriv*)data);
				break;

			case HEADER_GD_REQUEST_EMPIRE_PRIV:
				AddEmpirePriv((TPacketGiveEmpirePriv*)data);
				break;

			case HEADER_GD_REQUEST_CHARACTER_PRIV:
				AddCharacterPriv((TPacketGiveCharacterPriv*) data);
				break;

			case HEADER_GD_MONEY_LOG:
				MoneyLog((TPacketMoneyLog*)data);
				break;

			case HEADER_GD_GUILD_DEPOSIT_MONEY:
				GuildDepositMoney((TPacketGDGuildMoney*)data);
				break;

			case HEADER_GD_GUILD_WITHDRAW_MONEY:
				GuildWithdrawMoney(peer, (TPacketGDGuildMoney*)data);
				break;

			case HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY:
				GuildWithdrawMoneyGiveReply((TPacketGDGuildMoneyWithdrawGiveReply*)data);
				break;

			case HEADER_GD_GUILD_WAR_BET:
				GuildWarBet((TPacketGDGuildWarBet *) data);
				break;

			case HEADER_GD_SET_EVENT_FLAG:
				SetEventFlag((TPacketSetEventFlag*) data);
				break;

			case HEADER_GD_CREATE_OBJECT:
				CreateObject((TPacketGDCreateObject *) data);
				break;

			case HEADER_GD_DELETE_OBJECT:
				DeleteObject(*(uint32_t *) data);
				break;

			case HEADER_GD_UPDATE_LAND:
				UpdateLand((uint32_t *) data);
				break;

			case HEADER_GD_MARRIAGE_ADD:
				MarriageAdd((TPacketMarriageAdd *) data);
				break;

			case HEADER_GD_MARRIAGE_UPDATE:
				MarriageUpdate((TPacketMarriageUpdate *) data);
				break;

			case HEADER_GD_MARRIAGE_REMOVE:
				MarriageRemove((TPacketMarriageRemove *) data);
				break;

			case HEADER_GD_WEDDING_REQUEST:
				WeddingRequest((TPacketWeddingRequest *) data);
				break;

			case HEADER_GD_WEDDING_READY:
				WeddingReady((TPacketWeddingReady *) data);
				break;

			case HEADER_GD_WEDDING_END:
				WeddingEnd((TPacketWeddingEnd *) data);
				break;

				// BLOCK_CHAT
			case HEADER_GD_BLOCK_CHAT:
				BlockChat((TPacketBlockChat *) data);
				break;
				// END_OF_BLOCK_CHAT

				// MYSHOP_PRICE_LIST

				// END_OF_MYSHOP_PRICE_LIST

				//RELOAD_ADMIN
			case HEADER_GD_RELOAD_ADMIN:
				ReloadAdmin(peer, (TPacketReloadAdmin*)data);
				break;
				//END_RELOAD_ADMIN

			case HEADER_GD_BREAK_MARRIAGE:
				BreakMarriage(peer, data);
				break;
			case HEADER_GD_REQ_SPARE_ITEM_ID_RANGE:
                LOG_ERROR("Retired item ID allocator request");
                RemovePeer(peer);
                return;

			case HEADER_GD_REQ_CHANGE_GUILD_MASTER :
				GuildChangeMaster((TPacketChangeGuildMaster*) data);
				break;

			case HEADER_GD_UPDATE_HORSE_NAME :
				UpdateHorseName((TPacketUpdateHorseName*) data, peer);
				break;

			case HEADER_GD_REQ_HORSE_NAME :
				AckHorseName(*(uint32_t*)data, peer);
				break;

			case HEADER_GD_DC:
				DeleteLoginKey((TPacketDC*) data);
				break;

			case HEADER_GD_VALID_LOGOUT:
				ResetLastPlayerID((TPacketNeedLoginLogInfo*)data);
				break;

			case HEADER_GD_REQUEST_CHARGE_CASH:
				ChargeCash((TRequestChargeCash*)data);
				break;

			//delete gift notify icon


			case HEADER_GD_UPDATE_CHANNELSTATUS:
				UpdateChannelStatus((SChannelStatus*) data);
				break;
			case HEADER_GD_REQUEST_CHANNELSTATUS:
				RequestChannelStatus(peer, dwHandle);
				break;
#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
			case HEADER_GD_FIND_CHANNEL:
				FindChannel(peer, dwHandle, (TPacketChangeChannel*)data);
				break;
#endif

#ifdef __SKILL_COLOR_SYSTEM__
			case HEADER_GD_SKILL_COLOR_SAVE:
				QUERY_SKILL_COLOR_SAVE(data);
				break;
#endif

#if defined(BL_OFFLINE_MESSAGE)
			case HEADER_GD_REQUEST_OFFLINE_MESSAGES:
				RequestReadOfflineMessages(peer, dwHandle, (TPacketGDReadOfflineMessage*) data);
				break;
			case HEADER_GD_SEND_OFFLINE_MESSAGE:
				SendOfflineMessage((TPacketGDSendOfflineMessage*) data);
				break;
#endif
#ifdef ENABLE_HWID
			case HEADER_GD_BLOCKHWID:
				{
					BlockHwid((THwidRequest*) data);
				}
				break;
			case HEADER_GD_UNBLOCKHWID:
				{
					UnblockHwid((THwidRequest*) data);
				}
				break;
#endif
#ifdef ENABLE_EVENT_MANAGER
		case HEADER_GD_EVENT_MANAGER:
			RecvEventManagerPacket(data);
			break;
#endif
#ifdef ENABLE_ITEMSHOP
		case HEADER_GD_ITEMSHOP:
			if (dwLength == 1) RecvItemShop(data);
			break;
#endif
			default:
				LOG_ERROR("Unknown header (header: {} handle: {} length: {})", header, dwHandle, dwLength);
				break;
		}
	}

	peer->RecvEnd(i);
}

void CClientManager::AddPeer(auriga::net::tcp::socket&& sock)
{
	auto pPeer = new CPeer;

	if (pPeer->Accept(std::move(sock)))
	{
		if (PeerAuth::HasKey())
		{
			pPeer->SetAuthenticated(false);
			pPeer->SendAuthChallenge();
		}
		else
		{
			pPeer->SetAuthenticated(true);
		}

		m_peerList.push_front(pPeer);
	}
	else
		delete pPeer;
}

void CClientManager::CheckHeartbeats()
{
	auto it = m_peerList.begin();

	while (it != m_peerList.end())
	{
		CPeer * pPeer = *(it++);
		pPeer->CheckHeartbeat();
	}
}

void CClientManager::DoAccept()
{
	DoAccept(m_acceptor);
}

void CClientManager::DoAccept(std::shared_ptr<auriga::net::tcp::acceptor> acceptor)
{
	if (!acceptor || !acceptor->is_open())
		return;

	auto socket = std::make_shared<auriga::net::tcp::socket>(auriga::net::Io());

	acceptor->async_accept(*socket,
		[this, socket, acceptor](const boost::system::error_code & ec)
		{
			if (!ec)
			{
				// Adding the peer touches the manager state, so it is
				// created on the game thread.
				auriga::net::PostCompletion([this, socket]() { AddPeer(std::move(*socket)); });
			}
			else if (ec != boost::asio::error::operation_aborted)
				LOG_ERROR("Accept failed: {}", ec.message().c_str());

			DoAccept(acceptor);
		});
}

void CClientManager::CollectPendingDeletes()
{
	auto it = m_pendingDeletePeers.begin();

	while (it != m_pendingDeletePeers.end())
	{
		CPeer * pPeer = *it;

		if (!pPeer->HasPendingIoWork())
		{
			it = m_pendingDeletePeers.erase(it);
			delete pPeer;
		}
		else
			++it;
	}
}

void CClientManager::RemovePeer(CPeer * pPeer)
{
    m_playerSessions.InvalidatePeer(pPeer->GetHandle());
	if (m_pkAuthPeer == pPeer)
	{
		m_pkAuthPeer = nullptr;
	}
	else
	{
		auto it = m_map_kLogonAccount.begin();

		while (it != m_map_kLogonAccount.end())
		{
			CLoginData * pkLD = it->second;

			if (pkLD->GetConnectedPeerHandle() == pPeer->GetHandle())
			{
				if (pkLD->IsPlay())
				{
					pkLD->SetPlay(false);
				}

				if (pkLD->IsDeleted())
				{
					LOG_INFO("DELETING LoginData");
					delete pkLD;
				}

				m_map_kLogonAccount.erase(it++);
			}
			else
				++it;
		}
	}

	m_peerList.remove(pPeer);

	// Cancel the socket operations and release the buffers
	pPeer->Destroy();

	if (pPeer->HasPendingIoWork())
	{
		// Asio completion handlers and queued io->game completions may
		// still reference this peer.
		pPeer->MarkDeletePending();
		m_pendingDeletePeers.push_back(pPeer);
	}
	else
	{
		delete pPeer;
	}
}

CPeer * CClientManager::GetPeer(IDENT ident)
{
	for (auto i = m_peerList.begin(); i != m_peerList.end();++i)
	{
		CPeer * tmp = *i;

		if (tmp->GetHandle() == ident)
			return tmp;
	}

	return nullptr;
}

CPeer * CClientManager::GetAnyPeer()
{
	if (m_peerList.empty())
		return nullptr;

	return m_peerList.front();
}

//
int CClientManager::AnalyzeQueryResult(SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	if (qi->iType == QID_COMMITTED_BUILDINGS)
	{
		m_buildingReadPending = false;
		if (qi->dwIdent == m_buildingDeleteEpoch)
			ReadObjectTable(msg, true);
		delete qi;
		return true;
	}
	CPeer * peer = GetPeer(qi->dwIdent);

#ifdef _TEST
	if (qi->iType != QID_ITEM_AWARD_LOAD)
	LOG_INFO("AnalyzeQueryResult {}", qi->iType);
#endif
	switch (qi->iType)
	{

		case QID_GUILD_RANKING:
			CGuildManager::instance().ResultRanking(msg->Get()->pSQLResult);
			break;

			// MYSHOP_PRICE_LIST
			// END_OF_MYSHOP_PRICE_LIST
	}

    if (!peer && (qi->iType == QID_PLAYER || qi->iType == QID_ITEM || qi->iType == QID_QUEST || qi->iType == QID_AFFECT
#ifdef __SKILL_COLOR_SYSTEM__
        || qi->iType == QID_SKILL_COLOR
#endif
        ))
    {
        RESULT_COMPOSITE_PLAYER(nullptr, msg, qi->iType); // Frees the owned load context even after peer loss.
        delete qi;
        return true;
    }
	if (!peer)
	{
		//LOG_ERROR("CClientManager::AnalyzeQueryResult: peer not exist anymore. (ident: {})", qi->dwIdent);
		delete qi;
		return true;
	}

	switch (qi->iType)
	{
		case QID_PLAYER:
		case QID_ITEM:
		case QID_QUEST:
		case QID_AFFECT:


#ifdef __SKILL_COLOR_SYSTEM__
		case QID_SKILL_COLOR:
#endif


			RESULT_COMPOSITE_PLAYER(peer, msg, qi->iType);
			break;

		case QID_LOGIN:
			RESULT_LOGIN(peer, msg);
			break;





		case QID_HIGHSCORE_REGISTER:
			LOG_INFO("QUERY_RESULT: HEADER_GD_HIGHSCORE_REGISTER {}", static_cast<const void*>(msg));
			RESULT_HIGHSCORE_REGISTER(peer, msg);
			break;

		case QID_SAFEBOX_SAVE:
		case QID_ITEM_SAVE:
		case QID_ITEM_DESTROY:
		case QID_QUEST_SAVE:
		case QID_PLAYER_SAVE:
		case QID_ITEM_AWARD_TAKEN:

#ifdef __SKILL_COLOR_SYSTEM__
		case QID_SKILL_COLOR_SAVE:
#endif

			break;

			// PLAYER_INDEX_CREATE_BUG_FIX
		case QID_PLAYER_INDEX_CREATE:
			RESULT_PLAYER_INDEX_CREATE(peer, msg);
			break;
			// END_PLAYER_INDEX_CREATE_BUG_FIX

		case QID_PLAYER_DELETE:
			__RESULT_PLAYER_DELETE(peer, msg);
			break;

		case QID_LOGIN_BY_KEY:
			RESULT_LOGIN_BY_KEY(peer, msg);
			break;

			// MYSHOP_PRICE_LIST
			// END_OF_MYSHOP_PRICE_LIST

		default:
			LOG_INFO("CClientManager::AnalyzeQueryResult unknown query result type: {}, str: {}", qi->iType, msg->stQuery.c_str());
			break;
	}

	delete qi;
	return true;
}

void UsageLog()
{
	std::ofstream file("usage.txt", std::ios::app);

	if (!file)
		return;

	const time_t ct = time(nullptr);
	const std::tm lt = auriga::time::LocalTime(ct);
	char timeBuf[32] = {};
	std::strftime(timeBuf, sizeof(timeBuf), "%a %b %e %H:%M:%S %Y", &lt);
	const char* time_s = timeBuf;

	std::string stamp(time_s + 4);

	if (stamp.size() > 15)
		stamp.resize(15);

	const int avg = g_dwUsageAvg / 3600;

	file << "| " << std::setw(4) << lt.tm_year + 1900 << ' '
		<< std::left << std::setw(15) << stamp << std::right
		<< " | " << std::setw(5) << avg
		<< " | " << std::setw(5) << g_dwUsageMax << " |\n";

	g_dwUsageMax = g_dwUsageAvg = 0;
}

//#define ENABLE_ITEMAWARD_REFRESH
int CClientManager::Process()
{
	int pulses;

	if (!(pulses = thecore_idle()))
		return 0;

	while (pulses--)
	{
		++thecore_heart->pulse;


		if (!(thecore_heart->pulse % thecore_heart->passes_per_sec))
		{
			// GameServer commits paid construction together with the wallet and materials.
			// Discover durable objects even if the initiating core died after COMMIT.
			if (!m_buildingReadPending)
			{
				const auto query = fmt::format("SELECT id,land_id,vnum,map_index,x,y,x_rot,y_rot,z_rot,life FROM object{} ORDER BY id",
					GetTablePostfix());
				m_buildingReadPending = true;
				CDBManager::instance().ReturnQuery(query.c_str(), QID_COMMITTED_BUILDINGS, m_buildingDeleteEpoch, nullptr);
			}
			if (g_test_server)
			{

				if (!(thecore_heart->pulse % thecore_heart->passes_per_sec * 10))

				{
					pt_log("[%9d] return %d/%d/%d/%d async %d/%d/%d/%d",
							thecore_heart->pulse,
							CDBManager::instance().CountReturnQuery(SQL_PLAYER),
							CDBManager::instance().CountReturnResult(SQL_PLAYER),
							CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
							CDBManager::instance().CountReturnCopiedQuery(SQL_PLAYER),
							CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
							CDBManager::instance().CountAsyncResult(SQL_PLAYER),
							CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER),
							CDBManager::instance().CountAsyncCopiedQuery(SQL_PLAYER));

					if (thecore_heart->pulse % 50 == 0)
						LOG_INFO("[{:9}] return {}/{}/{} async {}/{}/{}", thecore_heart->pulse, CDBManager::instance().CountReturnQuery(SQL_PLAYER), CDBManager::instance().CountReturnResult(SQL_PLAYER), CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER), CDBManager::instance().CountAsyncQuery(SQL_PLAYER), CDBManager::instance().CountAsyncResult(SQL_PLAYER), CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER));
				}
			}
			else
			{
				pt_log("[%9d] return %d/%d/%d/%d async %d/%d/%d%/%d",
						thecore_heart->pulse,
						CDBManager::instance().CountReturnQuery(SQL_PLAYER),
						CDBManager::instance().CountReturnResult(SQL_PLAYER),
						CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountReturnCopiedQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncResult(SQL_PLAYER),
						CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountAsyncCopiedQuery(SQL_PLAYER));

						if (thecore_heart->pulse % 50 == 0)
						LOG_INFO("[{:9}] return {}/{}/{} async {}/{}/{}", thecore_heart->pulse, CDBManager::instance().CountReturnQuery(SQL_PLAYER), CDBManager::instance().CountReturnResult(SQL_PLAYER), CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER), CDBManager::instance().CountAsyncQuery(SQL_PLAYER), CDBManager::instance().CountAsyncResult(SQL_PLAYER), CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER));
						}

			CDBManager::instance().ResetCounter();

			uint32_t dwCount = CClientManager::instance().GetUserCount();

			g_dwUsageAvg += dwCount;
			g_dwUsageMax = MAX(g_dwUsageMax, dwCount);

			memset(&thecore_profiler[0], 0, sizeof(thecore_profiler));

			if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600)))
				UsageLog();

			m_iCacheFlushCount = 0;

#ifdef __SKILL_COLOR_SYSTEM__
			// Skill color fush
			UpdateSkillColorCache();
#endif


			UpdatePlayerCache();
			UpdateLogoutPlayer();

			// MYSHOP_PRICE_LIST
			// END_OF_MYSHOP_PRICE_LIST

			CGuildManager::instance().Update();
			CPrivManager::instance().Update();
			marriage::CManager::instance().Update();
#ifdef ENABLE_EVENT_MANAGER
			UpdateEventManager();
#endif
		}
#ifdef ENABLE_ITEMAWARD_REFRESH
		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 5)))
		{

		}
#endif
		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 10)))
		{
			/*
			char buf[4096 + 1];
			int len
			itertype(g_query_info.m_map_info) it;

			/////////////////////////////////////////////////////////////////
			buf[0] = '\0';
			len = 0;

			it = g_query_info.m_map_info.begin();

			int count = 0;

			while (it != g_query_info.m_map_info.end())
			{
				len += snprintf(buf + len, sizeof(buf) - len, "%2d %3d\n", it->first, it->second);
				count += it->second;
				it++;
			}

			pt_log("QUERY:\n%s-------------------- MAX : %d\n", buf, count);
			g_query_info.Reset();
			*/
			pt_log("QUERY: MAIN[%d] ASYNC[%d]", g_query_count[0], g_query_count[1]);
			g_query_count[0] = 0;
			g_query_count[1] = 0;
			/////////////////////////////////////////////////////////////////

			/////////////////////////////////////////////////////////////////
			/*
			buf[0] = '\0';
			len = 0;

			it = g_item_info.m_map_info.begin();

			count = 0;
			while (it != g_item_info.m_map_info.end())
			{
				len += snprintf(buf + len, sizeof(buf) - len, "%5d %3d\n", it->first, it->second);
				count += it->second;
				it++;
			}

			pt_log("ITEM:\n%s-------------------- MAX : %d\n", buf, count);
			g_item_info.Reset();
			*/
			/////////////////////////////////////////////////////////////////
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 60)))


		{
			CClientManager::instance().SendTime();
#ifndef ENABLE_25082021
			std::string st;
			CClientManager::instance().GetPeerP2PHostNames(st);
			LOG_INFO("Current Peer host names...\n{}", st.c_str());
#endif
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600)))
		{
			CMoneyLog::instance().Save();
		}
		
#if defined(BL_OFFLINE_MESSAGE)
		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600)))
		{
			CClientManager::instance().OfflineMessageGarbage();
		}
#endif
	}

	// Runs the accept/read/write completions the io thread handed over
	// since the previous pulse. Everything runs on this thread.
	auriga::net::DrainCompletions();

    std::erase_if(m_playerCacheDrains, [this](const auto &entry)
        { return !m_playerSessions.Matches(entry.second.Session()); });

	// Deferred deletion of peers whose aborted operations have finished.
	CollectPendingDeletes();

	// Server-to-server link keepalive / idle timeout.
	CheckHeartbeats();

#ifdef _WIN32
	if (_kbhit()) {
		int c = _getch();
		switch (c) {
			case 0x1b: // Esc
				return 0; // shutdown
				break;
			default:
				break;
		}
	}
#endif

	return 1;
}

uint32_t CClientManager::GetUserCount()
{
	return m_map_kLogonAccount.size();
}

void CClientManager::SendAllGuildSkillRechargePacket()
{
	ForwardPacket(HEADER_DG_GUILD_SKILL_RECHARGE, nullptr, 0);
}

void CClientManager::SendTime()
{
	uint64_t now = GetCurrentTime();
	ForwardPacket(HEADER_DG_TIME, &now, sizeof(uint64_t));
}

void CClientManager::ForwardPacket(uint8_t header, const void* data, int size, uint8_t bChannel, CPeer* except)
{
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * peer = *it;

		if (peer == except)
			continue;

		if (!peer->GetChannel())
			continue;

		if (bChannel && peer->GetChannel() != bChannel)
			continue;

		peer->EncodeHeader(header, 0, size);

		if (size > 0 && data)
			peer->Encode(data, size);
	}
}

void CClientManager::SendNotice(const char * c_pszFormat, ...)
{
	char szBuf[255+1];
	va_list args;

	va_start(args, c_pszFormat);
	int len = vsnprintf(szBuf, sizeof(szBuf), c_pszFormat, args);
	va_end(args);
	szBuf[len] = '\0';

	ForwardPacket(HEADER_DG_NOTICE, szBuf, len + 1);
}

uint64_t CClientManager::GetCurrentTime()
{
	return time(nullptr);
}

// ITEM_UNIQUE_ID





// ITEM_UNIQUE_ID_END
//BOOT_LOCALIZATION

bool CClientManager::InitializeLocalization()
{
	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT mValue, mKey FROM locale");
	SQLMsg * pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_COMMON);

	if (pMsg->Get()->uiNumRows == 0)
	{
		LOG_ERROR("InitializeLocalization() ==> DirectQuery failed({})", szQuery);
		delete pMsg;
		return false;
	}

	LOG_INFO("InitializeLocalization() - LoadLocaleTable(count:{})", pMsg->Get()->uiNumRows);

	m_vec_Locale.clear();

	MYSQL_ROW row = nullptr;

	for (int n = 0; (row = pMsg->Get()->FetchRow()) != nullptr; ++n)
	{
		int col = 0;
		tLocale locale;

		strlcpy(locale.szValue, row[col++], sizeof(locale.szValue));
		strlcpy(locale.szKey, row[col++], sizeof(locale.szKey));

		//DB_NAME_COLUMN Setting
		if (strcmp(locale.szKey, "DB_NAME_COLUMN") == 0)
		{
			LOG_INFO("locale[DB_NAME_COLUMN] = {}", locale.szValue);
			g_stLocaleNameColumn = locale.szValue;
		}
		else
		{
			LOG_INFO("locale[UNKNOWN_KEY({})] = {}", locale.szKey, locale.szValue);
		}
		m_vec_Locale.push_back(locale);
	}

	delete pMsg;

	return true;
}
//END_BOOT_LOCALIZATION
//ADMIN_MANAGER

bool CClientManager::__GetAdminInfo(const char *szIP, std::vector<tAdminInfo> & rAdminVec)
{
	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery),
			"SELECT mID,mAccount,mName,mContactIP,mServerIP,mAuthority FROM gmlist WHERE mServerIP='ALL' or mServerIP='%s'",
		   	szIP ? szIP : "ALL");

	SQLMsg * pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_COMMON);

	if (pMsg->Get()->uiNumRows == 0)
	{
		// LOG_ERROR("__GetAdminInfo() ==> DirectQuery failed({})", szQuery); // @warme013
		delete pMsg;
		return false;
	}

	MYSQL_ROW row;
	rAdminVec.reserve(pMsg->Get()->uiNumRows);

	while ((row = pMsg->Get()->FetchRow()))
	{
		int idx = 0;
		tAdminInfo Info;

		str_to_number(Info.m_ID, row[idx++]);
		trim_and_lower(row[idx++], Info.m_szAccount, sizeof(Info.m_szAccount));
		strlcpy(Info.m_szName, row[idx++], sizeof(Info.m_szName));
		strlcpy(Info.m_szContactIP, row[idx++], sizeof(Info.m_szContactIP));
		strlcpy(Info.m_szServerIP, row[idx++], sizeof(Info.m_szServerIP));
		std::string stAuth = row[idx++];

		if (!stAuth.compare("IMPLEMENTOR"))
			Info.m_Authority = GM_IMPLEMENTOR;
		else if (!stAuth.compare("GOD"))
			Info.m_Authority = GM_GOD;
		else if (!stAuth.compare("HIGH_WIZARD"))
			Info.m_Authority = GM_HIGH_WIZARD;
		else if (!stAuth.compare("LOW_WIZARD"))
			Info.m_Authority = GM_LOW_WIZARD;
		else if (!stAuth.compare("WIZARD"))
			Info.m_Authority = GM_WIZARD;
		else
			continue;

		rAdminVec.push_back(Info);

		LOG_INFO("GM: PID {} Login {} Character {} ContactIP {} ServerIP {} Authority {}[{}]", Info.m_ID, Info.m_szAccount, Info.m_szName, Info.m_szContactIP, Info.m_szServerIP, Info.m_Authority, stAuth.c_str());
	}

	delete pMsg;

	return true;
}

bool CClientManager::__GetHostInfo(std::vector<std::string> & rIPVec)
{
	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT mIP FROM gmhost");
	SQLMsg * pMsg = CDBManager::instance().DirectQuery(szQuery, SQL_COMMON);

	if (pMsg->Get()->uiNumRows == 0)
	{
		// LOG_ERROR("__GetHostInfo() ==> DirectQuery failed({})", szQuery); // @warme013
		delete pMsg;
		return false;
	}

	rIPVec.reserve(pMsg->Get()->uiNumRows);

	MYSQL_ROW row;

	while ((row = pMsg->Get()->FetchRow()))
	{
		if (row[0] && *row[0])
		{
			rIPVec.emplace_back(row[0]);
			LOG_INFO("GMHOST: {}", row[0]);
		}
	}

	delete pMsg;
	return true;
}
//END_ADMIN_MANAGER

void CClientManager::ReloadAdmin(CPeer*, TPacketReloadAdmin* p)
{
	std::vector<tAdminInfo> vAdmin;
	std::vector<std::string> vHost;

	__GetHostInfo(vHost);
	__GetAdminInfo(p->szIP, vAdmin);

	uint32_t dwPacketSize = sizeof(uint16_t) + sizeof (uint16_t) + sizeof(tAdminInfo) * vAdmin.size() +
		  sizeof(uint16_t) + sizeof(uint16_t) + 16 * vHost.size();

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * peer = *it;

		if (!peer->GetChannel())
			continue;

		peer->EncodeHeader(HEADER_DG_RELOAD_ADMIN, 0, dwPacketSize);

		peer->EncodeWORD(16);
		peer->EncodeWORD(vHost.size());

		for (size_t n = 0; n < vHost.size(); ++n)
			peer->Encode(vHost[n].c_str(), 16);

		peer->EncodeWORD(sizeof(tAdminInfo));
		peer->EncodeWORD(vAdmin.size());

		for (size_t n = 0; n < vAdmin.size(); ++n)
			peer->Encode(&vAdmin[n], sizeof(tAdminInfo));
	}

	LOG_INFO("ReloadAdmin End {}", p->szIP);
}

//BREAK_MARRIAGE
void CClientManager::BreakMarriage(CPeer * peer, const char * data)
{
	uint32_t pid1, pid2;

	pid1 = *(int *) data;
	data += sizeof(int);

	pid2 = *(int *) data;
	data += sizeof(int);

	LOG_INFO("Breaking off a marriage engagement! pid {} and pid {}", pid1, pid2);
	marriage::CManager::instance().Remove(pid1, pid2);
}
//END_BREAK_MARIIAGE





//
//
void CClientManager::DeleteLoginKey(TPacketDC *data)
{
	char login[LOGIN_MAX_LEN+1] = {0};
	trim_and_lower(data->login, login, sizeof(login));

	CLoginData *pkLD = GetLoginDataByLogin(login);

	if (pkLD)
	{
		if (const auto it = m_map_pkLoginData.find(pkLD->GetKey()); it != m_map_pkLoginData.end())
			m_map_pkLoginData.erase(it);
	}
}

// delete gift notify icon


void CClientManager::UpdateChannelStatus(TChannelStatus* pData)
{
	if (const auto it = m_mChannelStatus.find(pData->nPort); it != m_mChannelStatus.end()) {
		it->second = pData->bStatus;
	}
	else {
		m_mChannelStatus.insert(TChannelStatusMap::value_type(pData->nPort, pData->bStatus));
	}
}

void CClientManager::RequestChannelStatus(CPeer* peer, uint32_t dwHandle)
{
	const int nSize = m_mChannelStatus.size();
	peer->EncodeHeader(HEADER_DG_RESPOND_CHANNELSTATUS, dwHandle, sizeof(TChannelStatus)*nSize+sizeof(int));
	peer->Encode(&nSize, sizeof(int));
	for (auto it = m_mChannelStatus.begin(); it != m_mChannelStatus.end(); ++it) {
		peer->Encode(&it->first, sizeof(short));
		peer->Encode(&it->second, sizeof(uint8_t));
	}
}

void CClientManager::ResetLastPlayerID(const TPacketNeedLoginLogInfo* data)
{
	CLoginData* pkLD = GetLoginDataByAID( data->dwPlayerID );

	if (nullptr != pkLD)
	{
		pkLD->SetLastPlayerID( 0 );
	}
}

void CClientManager::ChargeCash(const TRequestChargeCash* packet)
{
	// Sárkányérme top-up goes straight into the native account wallet.
	if (packet->aid <= 0 || packet->amount <= 0)
		return;
	const bool credited = auriga::dragon_coin::Committed(CDBManager::instance(),
		auriga::dragon_coin::Credit(static_cast<uint32_t>(packet->aid), static_cast<uint64_t>(packet->amount)));
	LOG_INFO("Request Charge (amount : {}, aid : {}) {}", packet->amount, packet->aid, credited ? "credited" : "refused");
}

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
void CClientManager::FindChannel(CPeer * requestPeer, uint32_t dwHandle, TPacketChangeChannel * p)
{
	if (!p->lMapIndex || !p->channel)
		return;

	unsigned long lAddr = 0;
	uint16_t port = 0;

	for (const auto peer : m_peerList)
	{
		if (peer->GetChannel() != p->channel) //not the channel we are looking for!
			continue;

		TMapLocation kMapLocation;
		memcpy(kMapLocation.alMaps, peer->GetMaps(), sizeof(kMapLocation.alMaps));

		for (const auto midx : kMapLocation.alMaps)
		{
			if (midx == p->lMapIndex)
			{
				//Get host, and convert to int
				char host[16];
				strlcpy(host, peer->GetPublicIP(), sizeof(kMapLocation.szHost));
				lAddr = inet_addr(host);

				//Target port
				port = peer->GetListenPort();

				break;
			}
		}

		if (lAddr && port) //We already obtained them
			break;
	}

	TPacketReturnChannel r;
	r.lAddr = lAddr;
	r.port = port;

	requestPeer->EncodeHeader(HEADER_DG_CHANNEL_RESULT, dwHandle, sizeof(r));
	requestPeer->Encode(&r, sizeof(r));
}
#endif


#if defined(BL_OFFLINE_MESSAGE)
void CClientManager::RequestReadOfflineMessages(CPeer* pkPeer, uint32_t dwHandle, TPacketGDReadOfflineMessage* p)
{
	const auto it = m_OfflineMessage.find(p->szName);
	if (it == m_OfflineMessage.end())
		return;

	const auto& vec = it->second;
	TPacketDGReadOfflineMessage r;
	for (const auto& msg : vec) {
		strlcpy(r.szFrom, msg->From.c_str(), sizeof(r.szFrom));
		strlcpy(r.szMessage, msg->Message.c_str(), sizeof(r.szMessage));
		pkPeer->EncodeHeader(HEADER_DG_RESPOND_OFFLINE_MESSAGES, dwHandle, sizeof(r));
		pkPeer->Encode(&r, sizeof(r));
	}

	m_OfflineMessage.erase(it);
}

void CClientManager::SendOfflineMessage(TPacketGDSendOfflineMessage* p)
{
	auto msg = std::make_shared<SOfflineMessage>(p->szFrom, p->szMessage);
	m_OfflineMessage[p->szTo].emplace_back(std::move(msg));
}

void CClientManager::OfflineMessageGarbage()
{
	if (m_OfflineMessage.empty())
		return;

	static constexpr int iHour = 5;
	const auto tCurrent = std::chrono::system_clock::now();

	for (auto& mOff : m_OfflineMessage) {
		auto& vMessage = mOff.second;
		for (auto iter = vMessage.begin(); iter != vMessage.end();) {
			if (std::chrono::duration_cast<std::chrono::hours>(tCurrent - iter->get()->t).count() >= iHour)
				iter = vMessage.erase(iter);
			else
				++iter;
		}
	}
}
#endif


#ifdef ENABLE_HWID
void CClientManager::BlockHwid(THwidRequest * pack) {
	if (m_pkAuthPeer) {
		THwidRequest pack2;
		strlcpy(pack2.whoname, pack->whoname, sizeof(pack2.whoname));
		strlcpy(pack2.targetname, pack->targetname, sizeof(pack2.targetname));

		m_pkAuthPeer->EncodeHeader(HEADER_DG_BLOCKHWID, 0, sizeof(THwidRequest));
		m_pkAuthPeer->Encode(&pack2, sizeof(THwidRequest));
	}
}

void CClientManager::UnblockHwid(THwidRequest * pack) {
	if (m_pkAuthPeer) {
		THwidRequest pack2;
		strlcpy(pack2.whoname, pack->whoname, sizeof(pack2.whoname));
		strlcpy(pack2.targetname, pack->targetname, sizeof(pack2.targetname));

		m_pkAuthPeer->EncodeHeader(HEADER_DG_UNBLOCKHWID, 0, sizeof(THwidRequest));
		m_pkAuthPeer->Encode(&pack2, sizeof(THwidRequest));
	}
}
#endif
#ifdef ENABLE_EVENT_MANAGER
#include "buffer_manager.h"
void SetTimeToString(TEventManagerData* eventData)
{
	if (eventData->startTime <= 0)
		snprintf(eventData->startTimeText, sizeof(eventData->startTimeText), "1970-01-01 00:00:00");
	else
	{
		const time_t startTime = eventData->startTime;
		const std::tm vEventStartKey = auriga::time::LocalTime(startTime);
		snprintf(eventData->startTimeText, sizeof(eventData->startTimeText), "%d-%02d-%02d %02d:%02d:%02d", vEventStartKey.tm_year + 1900, vEventStartKey.tm_mon + 1, vEventStartKey.tm_mday, vEventStartKey.tm_hour, vEventStartKey.tm_min, vEventStartKey.tm_sec);
	}

	if (eventData->endTime <= 0)
		snprintf(eventData->endTimeText, sizeof(eventData->endTimeText), "1970-01-01 00:00:00");
	else
	{
		const time_t endTime = eventData->endTime;
		const std::tm vEventEndKey = auriga::time::LocalTime(endTime);
		snprintf(eventData->endTimeText, sizeof(eventData->endTimeText), "%d-%02d-%02d %02d:%02d:%02d", vEventEndKey.tm_year + 1900, vEventEndKey.tm_mon + 1, vEventEndKey.tm_mday, vEventEndKey.tm_hour, vEventEndKey.tm_min, vEventEndKey.tm_sec);
	}
}
bool IsDontHaveEndTimeEvent(uint8_t eventIndex)
{
	switch (eventIndex)
	{
		case EMPIRE_WAR_EVENT:
		case TOURNAMENT_EVENT:
			return true;
			break;
	}
	return false;
}
void CClientManager::RecvEventManagerPacket(const char* data)
{
	const uint8_t subIndex = *(uint8_t*)data;
	data += sizeof(uint8_t);
	if (subIndex == EVENT_MANAGER_UPDATE)
		InitializeEventManager(true);
	else if (subIndex == EVENT_MANAGER_REMOVE_EVENT)
	{
		const uint16_t index = *(uint16_t*)data;
		data += sizeof(uint16_t);

		if (!m_EventManager.empty())
		{
			for (auto it = m_EventManager.begin(); it != m_EventManager.end(); ++it)
			{
				for (uint32_t j = 0; j < it->second.size(); ++j)
				{
					TEventManagerData& eventPtr = it->second[j];
					if (eventPtr.eventID == index)
					{
						eventPtr.endTime = time(nullptr);
						SetTimeToString(&eventPtr);
						UpdateEventManager();
						char szQuery[QUERY_MAX_LEN];
						snprintf(szQuery, sizeof(szQuery), "UPDATE player.event_table SET endTime = NOW() WHERE id = %u", index);
						std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER));
						return;
					}
				}
			}
		}
	}
}
void CClientManager::UpdateEventManager()
{
	const time_t now = time(nullptr);
	const std::tm vKey = auriga::time::LocalTime(now);
	if (vKey.tm_mday == 1 && vKey.tm_hour == 0 && vKey.tm_min == 0 && vKey.tm_sec == 0) // Month Update!
	{
		InitializeEventManager(true);
		return;
	}
	//const auto it = m_EventManager.find(vKey.tm_mday);
	//if (it != m_EventManager.end())
	for(auto it = m_EventManager.begin();it!=m_EventManager.end();++it)
	{
		if (!it->second.empty())
		{
			for(uint32_t j=0;j<it->second.size();++j)
			{
				TEventManagerData& pData = it->second[j];
				int leftTime = pData.startTime - now;
				bool sendStatusPacket = false;
				
				if (leftTime == 0 && pData.eventStatus == false)
				{
					pData.eventStatus = true;
					sendStatusPacket = true;
				}

				leftTime = pData.endTime - now;
				if (leftTime == 0 && pData.endTime != 0 && pData.eventStatus == true)
				{
					pData.eventStatus = false;
					sendStatusPacket = true;
				}

				if (sendStatusPacket)
				{
					const uint8_t subIndex = EVENT_MANAGER_EVENT_STATUS;
					TEMP_BUFFER buf;
					buf.write(&subIndex, sizeof(uint8_t));
					buf.write(&pData.eventID, sizeof(uint16_t));
					buf.write(&pData.eventStatus, sizeof(bool));
					buf.write(&pData.endTime, sizeof(int));
					buf.write(&pData.endTimeText, sizeof(pData.endTimeText));
					ForwardPacket(HEADER_DG_EVENT_MANAGER, buf.read_peek(), buf.size());
				}
			}
		}
	}
}

bool SortWithTime(const TEventManagerData& a, const TEventManagerData& b)
{
	return a.startTime < b.startTime;
}
bool CClientManager::InitializeEventManager(bool updateFromGameMaster)
{
	m_EventManager.clear();

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT id, eventIndex+0, UNIX_TIMESTAMP(startTime), UNIX_TIMESTAMP(endTime), empireFlag, channelFlag, value0, value1, value2, value3 FROM player.event_table");
	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER));
	if (pMsg->Get()->uiNumRows != 0)
	{
		const time_t cur_Time = time(nullptr);
		const std::tm vKey = auriga::time::LocalTime(cur_Time);

		MYSQL_ROW row = nullptr;
		for (int n = 0; (row = pMsg->Get()->FetchRow()) != nullptr; ++n)
		{
			int col = 0;
			TEventManagerData p;
			str_to_number(p.eventID, row[col++]);
			str_to_number(p.eventIndex, row[col++]);
			str_to_number(p.startTime, row[col++]);
			str_to_number(p.endTime, row[col++]);
			str_to_number(p.empireFlag, row[col++]);
			str_to_number(p.channelFlag, row[col++]);
			for (uint32_t j = 0; j < 4; ++j)
				str_to_number(p.value[j], row[col++]);

			const time_t eventEndTime = p.endTime;
			const std::tm vEventEndKey = auriga::time::LocalTime(eventEndTime);
			if (vEventEndKey.tm_mon < vKey.tm_mon && p.endTime != 0)
				continue;



			p.eventTypeOnlyStart = IsDontHaveEndTimeEvent(p.eventIndex);
			p.eventStatus = p.eventTypeOnlyStart ? false : cur_Time >= p.startTime && cur_Time <= p.endTime;

			const time_t eventStartTime = p.startTime;
			const std::tm vEventStartKey = auriga::time::LocalTime(eventStartTime);

			bool insertWithStart = true;
			if (vEventStartKey.tm_mon < vKey.tm_mon)
				insertWithStart = false;

			if(vEventStartKey.tm_mon != vKey.tm_mon && vEventEndKey.tm_mon != vKey.tm_mon)
				continue;
			SetTimeToString(&p);

			if (insertWithStart)
			{
				const auto it = m_EventManager.find(vEventStartKey.tm_mday);
				if (it != m_EventManager.end())
					it->second.emplace_back(p);
				else
				{
					std::vector<TEventManagerData> m_vec;
					m_vec.emplace_back(p);
					m_EventManager.emplace(vEventStartKey.tm_mday, m_vec);
				}
			}
			else
			{
				const auto it = m_EventManager.find(vEventEndKey.tm_mday);
				if (it != m_EventManager.end())
					it->second.emplace_back(p);
				else
				{
					std::vector<TEventManagerData> m_vec;
					m_vec.emplace_back(p);
					m_EventManager.emplace(vEventEndKey.tm_mday, m_vec);
				}
			}
		}
		if (!m_EventManager.empty())
		{
			for (auto it = m_EventManager.begin(); it != m_EventManager.end(); ++it)
				std::ranges::stable_sort(it->second, SortWithTime);
		}
	}
	SendEventData(nullptr, updateFromGameMaster);
	return true;
}

void CClientManager::SendEventData(CPeer* pkPeer, bool updateFromGameMaster)
{
	const uint8_t subIndex = EVENT_MANAGER_LOAD;
	const uint8_t dayCount = m_EventManager.size();
	TEMP_BUFFER buf;
	buf.write(&subIndex, sizeof(uint8_t));
	buf.write(&dayCount, sizeof(uint8_t));
	buf.write(&updateFromGameMaster, sizeof(bool));

	for (const auto& [dayIndex, dayData] : m_EventManager)
	{
		uint8_t dayEventCount = dayData.size();
		buf.write(&dayIndex, sizeof(uint8_t));
		buf.write(&dayEventCount, sizeof(uint8_t));
		if (dayEventCount > 0)
			buf.write(dayData.data(), dayEventCount * sizeof(TEventManagerData));
	}

	if (pkPeer != nullptr)
	{
		pkPeer->EncodeHeader(HEADER_DG_EVENT_MANAGER, 0, buf.size());
		pkPeer->Encode(buf.read_peek(), buf.size());
	}
	else
		ForwardPacket(HEADER_DG_EVENT_MANAGER, buf.read_peek(), buf.size());
}
#endif

#ifdef ENABLE_ITEMSHOP
void CClientManager::RecvItemShop(const char* data)
{
    const auto subheader = static_cast<std::uint8_t>(*data);
    if (subheader == ITEMSHOP_RELOAD)
        ForwardPacket(HEADER_DG_ITEMSHOP, &subheader, sizeof(subheader));
}
#endif
