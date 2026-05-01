#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "constants.h"
#include "utils.h"
#include "config.h"
#include "log.h"
#include "char_interface.hpp"
#include "db.h"
#include "desc_client.h"
#include "buffer_manager.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "packet.h"
#include "war_map.h"
#include "questmanager.h"
#include "locale_service.h"
#include "guild_manager.h"
#include "MarkManager.h"

namespace
{

	struct FGuildNameSender
	{
		FGuildNameSender(uint32_t id, const char* guild_name, uint8_t lv) : id(id), name(guild_name), level(lv)
		{
			p.header = HEADER_GC_GUILD;
			p.subheader = GUILD_SUBHEADER_GC_GUILD_NAME;
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
			p.size = sizeof(p) + sizeof(uint32_t) + sizeof(uint8_t) + GUILD_NAME_MAX_LEN;
#else
			p.size = sizeof(p) + GUILD_NAME_MAX_LEN + sizeof(uint32_t);
#endif
		}

		void operator()(LPCHARACTER ch)
		{
			LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));

			if (d)
			{
				d->BufferedPacket(&p, sizeof(p));
				d->BufferedPacket(&id, sizeof(id));
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
				d->BufferedPacket(&level, sizeof(level));
#endif
				d->Packet(name, GUILD_NAME_MAX_LEN);
			}
		}

		uint32_t		id;
		const char*	name;
		uint8_t		level;
		TPacketGCGuild	p;
	};
}

CGuildManager::CGuildManager()
{
}

CGuildManager::~CGuildManager()
{
	for( TGuildMap::const_iterator iter = m_mapGuild.begin() ; iter != m_mapGuild.end() ; ++iter )
	{
		M2_DELETE(iter->second);
	}

	m_mapGuild.clear();
}

int CGuildManager::GetDisbandDelay()
{
	return quest::CQuestManager::instance().GetEventFlag("guild_disband_delay") * (test_server ? 60 : 86400);
}

int CGuildManager::GetWithdrawDelay()
{
	return quest::CQuestManager::instance().GetEventFlag("guild_withdraw_delay") * (test_server ? 60 : 86400);
}

uint32_t CGuildManager::CreateGuild(TGuildCreateParameter& gcp)
{
	if (!gcp.master)
		return 0;

	if (!check_name(gcp.name))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(gcp.master), CHAT_TYPE_INFO, 135, "");
#endif
		return 0;
	}

	// @fixme143 BEGIN
	static char	__guild_name[GUILD_NAME_MAX_LEN*2+1];
	DBManager::instance().EscapeString(__guild_name, sizeof(__guild_name), gcp.name, strnlen(gcp.name, sizeof(gcp.name)));
	if (strncmp(__guild_name, gcp.name, strnlen(gcp.name, sizeof(gcp.name))))
		return 0;
	// @fixme143 END

	std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery("SELECT COUNT(*) FROM guild%s WHERE name = '%s'",
				get_table_postfix(), __guild_name));

	if (pmsg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);

		if (!(row[0] && row[0][0] == '0'))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(gcp.master), CHAT_TYPE_INFO, 166, "");
#endif
			return 0;
		}
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(gcp.master), CHAT_TYPE_INFO, 136, "");
#endif
		return 0;
	}

	// new CGuild(gcp) queries guild tables and tell dbcache to notice other game servers.
	// other game server calls CGuildManager::LoadGuild to load guild.
	CGuild * pg = M2_NEW CGuild(gcp);
	m_mapGuild.insert(std::make_pair(pg->GetID(), pg));
	return pg->GetID();
}

#ifdef ENABLE_GUILD_ATTRIBUTE
void CGuildManager::RemoveGuildBuff(LPCHARACTER ch)
{
	CAffect* affect = AffectSystem::FindAffect(AIHelpers::EcsOf(ch), AFFECT_GUILD_ATTRIBUTE);
	while (affect != nullptr)
	{
		if (affect)
			AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), affect);
		affect = AffectSystem::FindAffect(AIHelpers::EcsOf(ch), AFFECT_GUILD_ATTRIBUTE);
	}
}
#endif


void CGuildManager::Unlink(uint32_t pid)
{
	m_map_pkGuildByPID.erase(pid);
}

CGuild * CGuildManager::GetLinkedGuild(uint32_t pid)
{
	TGuildMap::iterator it = m_map_pkGuildByPID.find(pid);

	if (it == m_map_pkGuildByPID.end())
		return nullptr;

	return it->second;
}

void CGuildManager::Link(uint32_t pid, CGuild* guild)
{
	if (m_map_pkGuildByPID.find(pid) == m_map_pkGuildByPID.end())
		m_map_pkGuildByPID.insert(std::make_pair(pid,guild));
}

void CGuildManager::P2PLogoutMember(uint32_t pid)
{
	TGuildMap::iterator it = m_map_pkGuildByPID.find(pid);

	if (it != m_map_pkGuildByPID.end())
	{
		it->second->P2PLogoutMember(pid);
	}
}

void CGuildManager::P2PLoginMember(uint32_t pid)
{
	TGuildMap::iterator it = m_map_pkGuildByPID.find(pid);

	if (it != m_map_pkGuildByPID.end())
	{
		it->second->P2PLoginMember(pid);
	}
}

void CGuildManager::LoginMember(LPCHARACTER ch)
{
	TGuildMap::iterator it = m_map_pkGuildByPID.find((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));

	if (it != m_map_pkGuildByPID.end())
	{
		it->second->LoginMember(ch);
	}
}


CGuild* CGuildManager::TouchGuild(uint32_t guild_id)
{
	TGuildMap::iterator it = m_mapGuild.find(guild_id);

	if (it == m_mapGuild.end())
	{
		m_mapGuild.insert(std::make_pair(guild_id, M2_NEW CGuild(guild_id)));
		it = m_mapGuild.find(guild_id);

		#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
		CHARACTER_MANAGER::instance().for_each_pc(FGuildNameSender(guild_id, it->second->GetName(), it->second->GetLevel()));
#else
		CHARACTER_MANAGER::instance().for_each_pc(FGuildNameSender(guild_id, it->second->GetName(), 0));
#endif
	}

	return it->second;
}

CGuild* CGuildManager::FindGuild(uint32_t guild_id)
{
	TGuildMap::iterator it = m_mapGuild.find(guild_id);
	if (it == m_mapGuild.end())
	{
		return nullptr;
	}
	return it->second;
}

#ifdef ADVANCED_GUILD_INFO
void CGuildManager::ResetStatsToAll(){

	for (auto it = m_mapGuild.begin(); it!=m_mapGuild.end(); ++it)
	{
		it->second->ResetAllStats();
	}
}
#endif

CGuild*	CGuildManager::FindGuildByName(const std::string guild_name)
{

	for (auto it = m_mapGuild.begin(); it!=m_mapGuild.end(); ++it)
	{
		if (it->second->GetName()==guild_name)
			return it->second;
	}
	return nullptr;
}

void CGuildManager::Initialize()
{
	LOG_INFO("Initializing Guild");

	if (g_bAuthServer)
	{
		LOG_INFO("	No need for auth server");
		return;
	}

	std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery("SELECT id FROM guild%s", get_table_postfix()));

	std::vector<uint32_t> vecGuildID;
	vecGuildID.reserve(pmsg->Get()->uiNumRows);

	for (uint64_t i = 0; i < pmsg->Get()->uiNumRows; i++)
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
		uint32_t guild_id = strtoul(row[0], (char**)nullptr, 10);
		LoadGuild(guild_id);

		vecGuildID.push_back(guild_id);
	}

	CGuildMarkManager & rkMarkMgr = CGuildMarkManager::instance();

	rkMarkMgr.SetMarkPathPrefix("mark");

	extern bool GuildMarkConvert(const std::vector<uint32_t> & vecGuildID);
	GuildMarkConvert(vecGuildID);

	//rkMarkMgr.LoadMarkIndex();
	//rkMarkMgr.LoadMarkImages();
	//rkMarkMgr.LoadSymbol(GUILD_SYMBOL_FILENAME);
	if (!rkMarkMgr.LoadMarkIndex())
	{
		rkMarkMgr.SaveMarkIndex();
		rkMarkMgr.SaveMarkImage(0);
	}
	else
		rkMarkMgr.LoadMarkImages();
}

void CGuildManager::LoadGuild(uint32_t guild_id)
{
	TGuildMap::iterator it = m_mapGuild.find(guild_id);

	if (it == m_mapGuild.end())
	{
		m_mapGuild.insert(std::make_pair(guild_id, M2_NEW CGuild(guild_id)));
	}
	else
	{
		//it->second->Load(guild_id);
	}
}

void CGuildManager::DisbandGuild(uint32_t guild_id)
{
	TGuildMap::iterator it = m_mapGuild.find(guild_id);

	if (it == m_mapGuild.end())
		return;

	it->second->Disband();

	M2_DELETE(it->second);
	m_mapGuild.erase(it);

	CGuildMarkManager::instance().DeleteMark(guild_id);
}

void CGuildManager::SkillRecharge()
{
	for (TGuildMap::iterator it = m_mapGuild.begin(); it!=m_mapGuild.end();++it)
	{
		it->second->SkillRecharge();
	}
}

int CGuildManager::GetRank(CGuild* g)
{
	int point = g->GetLadderPoint();
	int rank = 1;

	for (auto it = m_mapGuild.begin(); it != m_mapGuild.end();++it)
	{
		CGuild * pg = it->second;

		if (pg->GetLadderPoint() > point)
			rank++;
	}

	return rank;
}

struct FGuildCompare : public std::function<bool (CGuild*, CGuild*)>
{
	bool operator () (CGuild* g1, CGuild* g2) const
	{
		if (g1->GetLadderPoint() < g2->GetLadderPoint())
			return true;
		if (g1->GetLadderPoint() > g2->GetLadderPoint())
			return false;
		if (g1->GetGuildWarWinCount() < g2->GetGuildWarWinCount())
			return true;
		if (g1->GetGuildWarWinCount() > g2->GetGuildWarWinCount())
			return false;
		if (g1->GetGuildWarLossCount() < g2->GetGuildWarLossCount())
			return true;
		if (g1->GetGuildWarLossCount() > g2->GetGuildWarLossCount())
			return false;
		int c = strcmp(g1->GetName(), g2->GetName());
		if (c>0)
			return true;
		return false;
	}
};

void CGuildManager::GetHighRankString(uint32_t dwMyGuild, char * buffer, size_t buflen)
{
	using namespace std;
	vector<CGuild*> v;

	for (auto it = m_mapGuild.begin(); it != m_mapGuild.end(); ++it)
	{
		if (it->second)
			v.push_back(it->second);
	}

	std::sort(v.begin(), v.end(), FGuildCompare());
	int n = v.size();
	int len = 0, len2;
	*buffer = '\0';

	for (int i = 0; i < 8; ++i)
	{
		if (n - i - 1 < 0)
			break;

		CGuild * g = v[n - i - 1];

		if (!g)
			continue;

		if (g->GetLadderPoint() <= 0)
			break;

		if (dwMyGuild == g->GetID())
		{
			len2 = snprintf(buffer + len, buflen - len, "[COLOR r;255|g;255|b;0]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		if (i)
		{
			len2 = snprintf(buffer + len, buflen - len, "[ENTER]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		len2 = snprintf(buffer + len, buflen - len, "%3d | %-12s | %-8d | %4d | %4d | %4d",
				GetRank(g),
				g->GetName(),
				g->GetLadderPoint(),
				g->GetGuildWarWinCount(),
				g->GetGuildWarDrawCount(),
				g->GetGuildWarLossCount());

		if (len2 < 0 || len2 >= (int) buflen - len)
			len += (buflen - len) - 1;
		else
			len += len2;

		if (g->GetID() == dwMyGuild)
		{
			len2 = snprintf(buffer + len, buflen - len, "[/COLOR]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}
	}
}

void CGuildManager::GetAroundRankString(uint32_t dwMyGuild, char * buffer, size_t buflen)
{
	using namespace std;
	vector<CGuild*> v;

	for (auto it = m_mapGuild.begin(); it != m_mapGuild.end(); ++it)
	{
		if (it->second)
			v.push_back(it->second);
	}

	std::sort(v.begin(), v.end(), FGuildCompare());
	int n = v.size();
	int idx;

	for (idx = 0; idx < (int) v.size(); ++idx)
		if (v[idx]->GetID() == dwMyGuild)
			break;

	int len = 0, len2;
	int count = 0;
	*buffer = '\0';

	for (int i = -3; i <= 3; ++i)
	{
		if (idx - i < 0)
			continue;

		if (idx - i >= n)
			continue;

		CGuild * g = v[idx - i];

		if (!g)
			continue;

		if (dwMyGuild == g->GetID())
		{
			len2 = snprintf(buffer + len, buflen - len, "[COLOR r;255|g;255|b;0]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		if (count)
		{
			len2 = snprintf(buffer + len, buflen - len, "[ENTER]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		len2 = snprintf(buffer + len, buflen - len, "%3d | %-12s | %-8d | %4d | %4d | %4d",
				GetRank(g),
				g->GetName(),
				g->GetLadderPoint(),
				g->GetGuildWarWinCount(),
				g->GetGuildWarDrawCount(),
				g->GetGuildWarLossCount());

		if (len2 < 0 || len2 >= (int) buflen - len)
			len += (buflen - len) - 1;
		else
			len += len2;

		++count;

		if (g->GetID() == dwMyGuild)
		{
			len2 = snprintf(buffer + len, buflen - len, "[/COLOR]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}
	}
}

/////////////////////////////////////////////////////////////////////
// Guild War
/////////////////////////////////////////////////////////////////////
void CGuildManager::RequestCancelWar(uint32_t guild_id1, uint32_t guild_id2)
{
	LOG_INFO("RequestCancelWar {} {}", guild_id1, guild_id2);

	TPacketGuildWar p;
	p.bWar = GUILD_WAR_CANCEL;
	p.dwGuildFrom = guild_id1;
	p.dwGuildTo = guild_id2;
	db_clientdesc->DBPacket(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
}

void CGuildManager::RequestEndWar(uint32_t guild_id1, uint32_t guild_id2)
{
	LOG_INFO("RequestEndWar {} {}", guild_id1, guild_id2);

	TPacketGuildWar p;
	p.bWar = GUILD_WAR_END;
	p.dwGuildFrom = guild_id1;
	p.dwGuildTo = guild_id2;
	db_clientdesc->DBPacket(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
}

void CGuildManager::RequestWarOver(uint32_t dwGuild1, uint32_t dwGuild2, uint32_t dwGuildWinner, int32_t lReward)
{
	CGuild * g1 = TouchGuild(dwGuild1);
	CGuild * g2 = TouchGuild(dwGuild2);

	if (g1->GetGuildWarState(g2->GetID()) != GUILD_WAR_ON_WAR)
	{
		LOG_INFO("RequestWarOver : both guild was not in war {} {}", dwGuild1, dwGuild2);
		RequestEndWar(dwGuild1, dwGuild2);
		return;
	}

	TPacketGuildWar p;

	p.bWar = GUILD_WAR_OVER;
	//    .
	//p.lWarPrice = lReward;
	p.lWarPrice = 0;
	p.bType = dwGuildWinner == 0 ? 1 : 0; // bType == 1 means draw for this packet.

	if (dwGuildWinner == 0)
	{
		p.dwGuildFrom = dwGuild1;
		p.dwGuildTo = dwGuild2;
	}
	else
	{
		p.dwGuildFrom = dwGuildWinner;
		p.dwGuildTo = dwGuildWinner == dwGuild1 ? dwGuild2 : dwGuild1;
	}

	db_clientdesc->DBPacket(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
	LOG_INFO("RequestWarOver : winner {} loser {} draw {} betprice {}", p.dwGuildFrom, p.dwGuildTo, static_cast<int>(p.bType), p.lWarPrice);
}

void CGuildManager::DeclareWar(uint32_t guild_id1, uint32_t guild_id2, uint8_t bType)
{
	if (guild_id1 == guild_id2)
		return;

	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);

	if (!g1 || !g2)
		return;

#ifdef TEXTS_IMPROVEMENT
	if (g1->DeclareWar(guild_id2, bType, GUILD_WAR_SEND_DECLARE) && g2->DeclareWar(guild_id1, bType, GUILD_WAR_RECV_DECLARE)) {
		SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 563, "%s#%s", TouchGuild(guild_id1)->GetName(), TouchGuild(guild_id2)->GetName());
	}
#endif
}

void CGuildManager::RefuseWar(uint32_t guild_id1, uint32_t guild_id2)
{
	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);
#ifdef TEXTS_IMPROVEMENT
	if (g1 && g2)
	{
		if (g2->GetMasterCharacter())
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(g2->GetMasterCharacter()), CHAT_TYPE_INFO, 124, "%s", g1->GetName());
	}
#endif
	if ( g1 != nullptr)
		g1->RefuseWar(guild_id2);

	if ( g2 != nullptr && g1 != nullptr)
		g2->RefuseWar(g1->GetID());
}

void CGuildManager::WaitStartWar(uint32_t guild_id1, uint32_t guild_id2)
{
	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);

	if (!g1 || !g2)
	{
		LOG_INFO("GuildWar: CGuildManager::WaitStartWar({},{}) - something wrong in arg. there is no guild like that.", guild_id1, guild_id2);
		return;
	}

#ifdef TEXTS_IMPROVEMENT
	if (g1->WaitStartWar(guild_id2) || g2->WaitStartWar(guild_id1)) {
		SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 564, "%s#%s", g1->GetName(), g2->GetName());
	}
#endif
}

struct FSendWarList
{
	FSendWarList(uint8_t subheader, uint32_t guild_id1, uint32_t guild_id2)
	{
		gid1 = guild_id1;
		gid2 = guild_id2;

		p.header	= HEADER_GC_GUILD;
		p.size		= sizeof(p) + sizeof(uint32_t) * 2;
		p.subheader	= subheader;
	}

	void operator() (LPCHARACTER ch)
	{
		LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));

		if (d)
		{
			d->BufferedPacket(&p, sizeof(p));
			d->BufferedPacket(&gid1, sizeof(uint32_t));
			d->Packet(&gid2, sizeof(uint32_t));
		}
	}

	uint32_t gid1, gid2;
	TPacketGCGuild p;
};

void CGuildManager::StartWar(uint32_t guild_id1, uint32_t guild_id2)
{
	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);

	if (!g1 || !g2)
		return;

	if (!g1->CheckStartWar(guild_id2) || !g2->CheckStartWar(guild_id1))
		return;

	g1->StartWar(guild_id2);
	g2->StartWar(guild_id1);

#ifdef TEXTS_IMPROVEMENT
	SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 565, "%s#%s", g1->GetName(), g2->GetName());
#endif

	if (guild_id1 > guild_id2)
		std::swap(guild_id1, guild_id2);

	CHARACTER_MANAGER::instance().for_each_pc(FSendWarList(GUILD_SUBHEADER_GC_GUILD_WAR_LIST, guild_id1, guild_id2));
	m_GuildWar.insert(std::make_pair(guild_id1, guild_id2));
}

void SendGuildWarOverNotice(CGuild* g1, CGuild* g2, bool bDraw)
{
#ifdef TEXTS_IMPROVEMENT
	if (g1 && g2)
	{
		if ( g1->GetWarScoreAgainstTo( g2->GetID() ) > g2->GetWarScoreAgainstTo( g1->GetID() ) ) {
			SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 566, "%s#%s", g1->GetName(), g2->GetName());
		} else if (g1->GetWarScoreAgainstTo( g2->GetID() ) < g2->GetWarScoreAgainstTo( g1->GetID() )) {
			SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 566, "%s#%s", g1->GetName(), g2->GetName());
		} else {
			SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 567, "%s#%s", g1->GetName(), g2->GetName());
		}
	}
#endif
}

bool CGuildManager::EndWar(uint32_t guild_id1, uint32_t guild_id2)
{
	if (guild_id1 > guild_id2)
		std::swap(guild_id1, guild_id2);

	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);

	std::pair<uint32_t, uint32_t> k = std::make_pair(guild_id1, guild_id2);

	TGuildWarContainer::iterator it = m_GuildWar.find(k);

	if (m_GuildWar.end() == it)
	{
		LOG_INFO("EndWar({},{}) - EndWar request but guild is not in list", guild_id1, guild_id2);
		return false;
	}

	if ( g1 && g2 )
	{
	    if (g1->GetGuildWarType(guild_id2) == GUILD_WAR_TYPE_FIELD)
		{
			SendGuildWarOverNotice(g1, g2, g1->GetWarScoreAgainstTo(guild_id2) == g2->GetWarScoreAgainstTo(guild_id1));
		}
	}
	else
	{
	    return false;
	}

#ifdef ADVANCED_GUILD_INFO
	if(g1->GetWarScoreAgainstTo( g2->GetID() ) > g2->GetWarScoreAgainstTo( g1->GetID()))
	{
		g1->ChangeTrophies(true, false); 
		g2->ChangeTrophies(false, false);
	}
	else if(g1->GetWarScoreAgainstTo( g2->GetID() ) < g2->GetWarScoreAgainstTo( g1->GetID()))
	{
		g2->ChangeTrophies(true, false); 
		g1->ChangeTrophies(false, false);
	}
	else if(g1->GetWarScoreAgainstTo( g2->GetID() ) == g2->GetWarScoreAgainstTo( g1->GetID() ))
	{
		g2->ChangeTrophies(false, true); 
		g1->ChangeTrophies(false, true);
	}
#endif

	if (g1)
		g1->EndWar(guild_id2);

	if (g2)
		g2->EndWar(guild_id1);

	m_GuildWarEndTime[k] = get_global_time();
	CHARACTER_MANAGER::instance().for_each_pc(FSendWarList(GUILD_SUBHEADER_GC_GUILD_WAR_END_LIST, guild_id1, guild_id2));
	m_GuildWar.erase(it);

	return true;
}

void CGuildManager::WarOver(uint32_t guild_id1, uint32_t guild_id2, bool bDraw)
{
	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);

	if (guild_id1 > guild_id2)
		std::swap(guild_id1, guild_id2);

	std::pair<uint32_t, uint32_t> k = std::make_pair(guild_id1, guild_id2);

	TGuildWarContainer::iterator it = m_GuildWar.find(k);

	if (m_GuildWar.end() == it)
		return;

	SendGuildWarOverNotice(g1, g2, bDraw);

	EndWar(guild_id1, guild_id2);
}

void CGuildManager::CancelWar(uint32_t guild_id1, uint32_t guild_id2)
{
	if (!EndWar(guild_id1, guild_id2))
		return;

	CGuild * g1 = FindGuild(guild_id1);
	CGuild * g2 = FindGuild(guild_id2);
#ifdef TEXTS_IMPROVEMENT
	if (g1)
	{
		LPCHARACTER master1 = g1->GetMasterCharacter();
		if (master1) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(master1), CHAT_TYPE_INFO, 146, "");
		}
	}
#endif
#ifdef TEXTS_IMPROVEMENT
	if (g2)
	{
		LPCHARACTER master2 = g2->GetMasterCharacter();
		if (master2) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(master2), CHAT_TYPE_INFO, 146, "");
		}
	}

	if (g1 && g2) {
		SendNoticeNew(CHAT_TYPE_BIG_NOTICE, 0, 0, 568, "%s#%s", g1->GetName(), g2->GetName());
	}
#endif
}

void CGuildManager::ReserveWar(uint32_t dwGuild1, uint32_t dwGuild2, uint8_t bType) // from DB
{
	LOG_INFO("GuildManager::ReserveWar {} {}", dwGuild1, dwGuild2);

	CGuild * g1 = FindGuild(dwGuild1);
	CGuild * g2 = FindGuild(dwGuild2);

	if (!g1 || !g2)
		return;

	g1->ReserveWar(dwGuild2, bType);
	g2->ReserveWar(dwGuild1, bType);
}

void CGuildManager::ShowGuildWarList(LPCHARACTER ch)
{
	for (auto it = m_GuildWar.begin(); it != m_GuildWar.end(); ++it)
	{
		CGuild * A = TouchGuild(it->first);
		CGuild * B = TouchGuild(it->second);

		if (A && B)
		{
			ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_NOTICE, "%s[%d] vs %s[%d] time %u sec.",
					A->GetName(), A->GetID(),
					B->GetName(), B->GetID(),
					get_global_time() - A->GetWarStartTime(B->GetID()));
		}
	}
}

void CGuildManager::SendGuildWar(LPCHARACTER ch)
{
	if (!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
		return;

	TEMP_BUFFER buf;
	TPacketGCGuild p;
	p.header= HEADER_GC_GUILD;
	p.subheader = GUILD_SUBHEADER_GC_GUILD_WAR_LIST;
	p.size = sizeof(p) + (sizeof(uint32_t) * 2) * m_GuildWar.size();
	buf.write(&p, sizeof(p));

	for (auto it = m_GuildWar.begin(); it != m_GuildWar.end(); ++it)
	{
		buf.write(&it->first, sizeof(uint32_t));
		buf.write(&it->second, sizeof(uint32_t));
	}

	ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(buf.read_peek(), buf.size());
}

void SendGuildWarScore(uint32_t dwGuild, uint32_t dwGuildOpp, int iDelta, int iBetScoreDelta)
{
	TPacketGuildWarScore p;

	p.dwGuildGainPoint = dwGuild;
	p.dwGuildOpponent = dwGuildOpp;
	p.lScore = iDelta;
	p.lBetScore = iBetScoreDelta;

	db_clientdesc->DBPacket(HEADER_GD_GUILD_WAR_SCORE, 0, &p, sizeof(TPacketGuildWarScore));
	LOG_INFO("SendGuildWarScore {} {} {}", dwGuild, dwGuildOpp, iDelta);
}

void CGuildManager::Kill(LPCHARACTER killer, LPCHARACTER victim)
{
	if (!ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(killer)))
		return;

	if (!(ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(victim))))
		return;

	if (killer->GetWarMap())
	{
		killer->GetWarMap()->OnKill(killer, victim);
		return;
	}

	CGuild * gAttack = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(killer));
	CGuild * gDefend = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(victim));

	if (!gAttack || !gDefend)
		return;

	if (gAttack->GetGuildWarType(gDefend->GetID()) != GUILD_WAR_TYPE_FIELD)
		return;

	if (!gAttack->UnderWar(gDefend->GetID()))
		return;

	SendGuildWarScore(gAttack->GetID(), gDefend->GetID(), ((victim)->GetLevel()));
}

void CGuildManager::StopAllGuildWar()
{
	for (auto it = m_GuildWar.begin(); it != m_GuildWar.end(); ++it)
	{
		CGuild * g = CGuildManager::instance().TouchGuild(it->first);
		CGuild * pg = CGuildManager::instance().TouchGuild(it->second);
		g->EndWar(it->second);
		pg->EndWar(it->first);
	}

	m_GuildWar.clear();
}

void CGuildManager::ReserveWarAdd(TGuildWarReserve * p)
{
	auto it = m_map_kReserveWar.find(p->dwID);

	CGuildWarReserveForGame * pkReserve;

	if (it != m_map_kReserveWar.end())
		pkReserve = it->second;
	else
	{
		pkReserve = M2_NEW CGuildWarReserveForGame;

		m_map_kReserveWar.insert(std::make_pair(p->dwID, pkReserve));
		m_vec_kReserveWar.push_back(pkReserve);
	}

	memcpy(&pkReserve->data, p, sizeof(TGuildWarReserve));

	LOG_INFO("ReserveWarAdd {} gid1 {} power {} gid2 {} power {} handicap {}", pkReserve->data.dwID, p->dwGuildFrom, p->lPowerFrom, p->dwGuildTo, p->lPowerTo, p->lHandicap);
}

void CGuildManager::ReserveWarBet(TPacketGDGuildWarBet * p)
{
	auto it = m_map_kReserveWar.find(p->dwWarID);

	if (it == m_map_kReserveWar.end())
		return;

	it->second->mapBet.insert(std::make_pair(p->szLogin, std::make_pair(p->dwGuild, p->dwGold)));
}

bool CGuildManager::IsBet(uint32_t dwID, const char * c_pszLogin)
{
	auto it = m_map_kReserveWar.find(dwID);

	if (it == m_map_kReserveWar.end())
		return true;

	return it->second->mapBet.end() != it->second->mapBet.find(c_pszLogin);
}

void CGuildManager::ReserveWarDelete(uint32_t dwID)
{
	LOG_INFO("ReserveWarDelete {}", dwID);
	auto it = m_map_kReserveWar.find(dwID);

	if (it == m_map_kReserveWar.end())
		return;

	auto it_vec = m_vec_kReserveWar.begin();

	while (it_vec != m_vec_kReserveWar.end())
	{
		if (*it_vec == it->second)
		{
			it_vec = m_vec_kReserveWar.erase(it_vec);
			break;
		}
		else
			++it_vec;
	}

	M2_DELETE(it->second);
	m_map_kReserveWar.erase(it);
}

std::vector<CGuildWarReserveForGame *> & CGuildManager::GetReserveWarRef()
{
	return m_vec_kReserveWar;
}

//
// End of Guild War
//

void CGuildManager::ChangeMaster(uint32_t dwGID)
{
	TGuildMap::iterator iter = m_mapGuild.find(dwGID);

	if ( iter != m_mapGuild.end() )
	{
		iter->second->Load(dwGID);
	}

	// Ʈ  ֱ
	DBManager::instance().FuncQuery(std::bind(&CGuild::SendGuildDataUpdateToAllMember, iter->second, std::placeholders::_1),"SELECT 1");

}








