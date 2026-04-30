#include "stdafx.h"
#include "config.h"
#include "utils.h"
#include "crc32.h"
#include "desc.h"
#include "desc_p2p.h"
#include "desc_client.h"
#include "desc_manager.h"

#include <algorithm>
#include "char_interface.hpp"
#include "protocol.h"
#include "messenger_manager.h"
#include "p2p.h"
#include "ip_ban.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#ifdef ENABLE_GENERAL_CH
#include "map_location.h"
#endif

struct valid_ip
{
	const char* ip;
	uint8_t	network;
	uint8_t	mask;
};

static valid_ip admin_ip[] =
{
	{ "210.123.10",     128,    128     },
	{ "\n",             0,      0       }
};

int IsValidIP(valid_ip* ip_table, const char* host)
{
	int         i, j;
	char        ip_addr[256];

	for (i = 0; *(ip_table + i)->ip != '\n'; ++i)
	{
		j = 255 - (ip_table + i)->mask;

		do
		{
			snprintf(ip_addr, sizeof(ip_addr), "%s.%d", (ip_table + i)->ip, (ip_table + i)->network + j);

			if (!strcmp(ip_addr, host))
				return true;

			if (!j)
				break;
		} while (j--);
	}

	return false;
}

#if defined(__IMPROVED_HANDSHAKE_PROCESS__)
/*
* Title: Improved Handshake Process
* Description: An Anti-spam solution that limits the amount
*              of handshakes per second and per attempts.
* Date: YMD.2021.09.07
* Author: Owsap, OSP (Owsap Server Protection)
*
* Copyright 2021 Owsap Productions
*/
void DESC_MANAGER::AcceptHandshake(const char* c_szHost, uint32_t dwHandshakeTime)
{
	// Find if the host is in the AcceptHostHandshakeVector
	AcceptHostHandshakeVector::iterator it = std::find_if(m_vecAcceptHostHandshake.begin(), m_vecAcceptHostHandshake.end(),
		[&c_szHost](const PairedStringDWORD& element)
		{ return (element.first == c_szHost); }
	);
	// Check if host already exists in the AcceptHostHandshakeVector
	if (it != m_vecAcceptHostHandshake.end())
	{
		// Check if the cores pulse is greater than the last handshake time set @ DESC::Setup
		if (thecore_pulse() > it->second)
			m_vecAcceptHostHandshake.erase(it);
	}
	else // Set host handshake time
		m_vecAcceptHostHandshake.emplace_back(std::make_pair(c_szHost, dwHandshakeTime));
}

bool DESC_MANAGER::IsIntrusiveHandshake(const char* c_szHost)
{
	if (!m_vecAcceptHostHandshake.empty())
	{
		// Find if the host is in the AcceptHostHandshakeVector and if the cores
		// pulse is greater than the last handshake time set @ DESC::Setup
		AcceptHostHandshakeVector::iterator it = std::find_if(m_vecAcceptHostHandshake.begin(), m_vecAcceptHostHandshake.end(),
			[&c_szHost](const PairedStringDWORD& element)
			{ return (element.first == c_szHost && element.second > thecore_pulse()); }
		);
		if (it != m_vecAcceptHostHandshake.end())
			return true;
	}
	return false;
}

void DESC_MANAGER::SetIntrusiveCount(const char* c_szHost, bool bReset)
{
	// Find if the host is in the IntrusiveHostCountVector
	IntrusiveHostCountVector::iterator it = std::find_if(m_vecIntrusiveHostCount.begin(), m_vecIntrusiveHostCount.end(),
		[&c_szHost](const PairedStringDWORD& element)
		{ return (element.first == c_szHost); }
	);
	// Set intrusive host count if exists in the IntrusiveHostCountVector
	if (it != m_vecIntrusiveHostCount.end())
	{
		if (bReset)
			it->second = 0;
		else
			it->second = it->second + 1;
	}
	else
		// Emplace the host into the IntrusiveHostCountVector with count set to 1
		m_vecIntrusiveHostCount.emplace_back(std::make_pair(c_szHost, 1));
}

int DESC_MANAGER::GetIntrusiveCount(const char* c_szHost)
{
	if (!m_vecIntrusiveHostCount.empty())
	{
		// Find if the host is in the IntrusiveHostCountVector
		IntrusiveHostCountVector::iterator it = std::find_if(m_vecIntrusiveHostCount.begin(), m_vecIntrusiveHostCount.end(),
			[&c_szHost](const PairedStringDWORD& element)
			{ return (element.first == c_szHost); }
		);
		// Return the intrusive host count
		if (it != m_vecIntrusiveHostCount.end())
			return it->second;
	}
	return 0;
}

void DESC_MANAGER::SetIntruder(const char* c_szHost, uint32_t dwDelayHandshakeTime)
{
	// Reset intrusive host count
	SetIntrusiveCount(c_szHost, true /* reset */);

	// Find if the host is in the IntruderHostVector
	IntruderHostVector::iterator it = std::find_if(m_vecIntruderHost.begin(), m_vecIntruderHost.end(),
		[&c_szHost](const PairedStringDWORD& element)
		{ return (element.first == c_szHost); }
	);
	// Set intruder host with the next (delayed) handshake time
	if (it != m_vecIntruderHost.end())
		it->second = dwDelayHandshakeTime;
	else
		// Emplace the host into the IntruderHostVector with the next (delayed) handshake time
		m_vecIntruderHost.emplace_back(std::make_pair(c_szHost, dwDelayHandshakeTime));
}

bool DESC_MANAGER::IsIntruder(const char* c_szHost)
{
	if (!m_vecIntruderHost.empty())
	{
		// Find if the host is in the IntruderHostVector and if the cores pulse
		// is less than the next (delayed) handshake time
		IntruderHostVector::iterator it = std::find_if(m_vecIntruderHost.begin(), m_vecIntruderHost.end(),
			[&c_szHost](const PairedStringDWORD& element)
			{ return (element.first == c_szHost && element.second > thecore_pulse()); }
		);
		if (it != m_vecIntruderHost.end())
			return true;
	}
	return false;
}

void DESC_MANAGER::AllowHandshake(const char* c_szHost)
{
	// Clear the intrusive host count
	if (!m_vecIntrusiveHostCount.empty())
	{
		IntrusiveHostCountVector::iterator it = std::find_if(m_vecIntrusiveHostCount.begin(), m_vecIntrusiveHostCount.end(),
			[&c_szHost](const PairedStringDWORD& element)
			{ return (element.first == c_szHost); }
		);
		if (it != m_vecIntrusiveHostCount.end())
			m_vecIntrusiveHostCount.erase(it);
	}

	// Clear the intruder host
	if (!m_vecIntruderHost.empty())
	{
		IntruderHostVector::iterator it = std::find_if(m_vecIntruderHost.begin(), m_vecIntruderHost.end(),
			[&c_szHost](const PairedStringDWORD& element)
			{ return (element.first == c_szHost); }
		);
		if (it != m_vecIntruderHost.end())
			m_vecIntruderHost.erase(it);
	}
}
#endif

DESC_MANAGER::DESC_MANAGER() : m_bDestroyed(false)
{
	Initialize();
}

DESC_MANAGER::~DESC_MANAGER()
{
	Destroy();
}

void DESC_MANAGER::Initialize()
{
	m_iSocketsConnected = 0;
	m_iHandleCount = 0;
	m_iLocalUserCount = 0;
	memset(m_aiEmpireUserCount, 0, sizeof(m_aiEmpireUserCount));
	m_bDisconnectInvalidCRC = false;
#if defined(__IMPROVED_HANDSHAKE_PROCESS__)
	m_vecAcceptHostHandshake.clear();
	m_vecIntrusiveHostCount.clear();
	m_vecIntruderHost.clear();
#endif
}

void DESC_MANAGER::Destroy()
{
	if (m_bDestroyed) {
		return;
	}
	m_bDestroyed = true;

	auto i = m_set_pkDesc.begin();

	while (i != m_set_pkDesc.end())
	{
		LPDESC d = *i;
		const auto ci = i;
		++i;

		if (d->GetType() == DESC_TYPE_CONNECTOR)
			continue;

		if (d->IsPhase(PHASE_P2P))
			continue;

		DestroyDesc(d, false);
		m_set_pkDesc.erase(ci);
	}

	i = m_set_pkDesc.begin();

	while (i != m_set_pkDesc.end())
	{
		LPDESC d = *i;
		const auto ci = i;
		++i;

		DestroyDesc(d, false);
		m_set_pkDesc.erase(ci);
	}

	m_set_pkClientDesc.clear();

	//m_AccountIDMap.clear();
	m_map_loginName.clear();
	m_map_handle.clear();

	Initialize();
}

uint32_t DESC_MANAGER::CreateHandshake()
{
	char crc_buf[8];
	crc_t crc;
	DESC_HANDSHAKE_MAP::iterator it;

RETRY:
	do
	{
		uint32_t val = thecore_random() % (1024 * 1024);

		*(uint32_t*)(crc_buf) = val;
		*((uint32_t*)crc_buf + 1) = get_global_time();

		crc = GetCRC32(crc_buf, 8);
		it = m_map_handshake.find(crc);
	} while (it != m_map_handshake.end());

	if (crc == 0)
		goto RETRY;

	return (crc);
}

LPDESC DESC_MANAGER::AcceptDesc(LPFDWATCH fdw, socket_t s)
{
	socket_t                    desc;
	LPDESC						newd;
	static sockaddr_in   peer;
	static char					host[MAX_HOST_LENGTH + 1];

	if ((desc = socket_accept(s, &peer)) == -1)
		return nullptr;

	strlcpy(host, inet_ntoa(peer.sin_addr), sizeof(host));

	if (g_bAuthServer)
	{
		if (IsBanIP(peer.sin_addr))
		{
			LOG_INFO("connection from {} was banned.", host);
			socket_close(desc);
			return nullptr;
		}
	}

	if (!IsValidIP(admin_ip, host)) // admin_ip 에 등록된 IP 는 최대 사용자 수에 구애받지 않는다.
	{
		if (m_iSocketsConnected >= MAX_ALLOW_USER) {
			LOG_ERROR("max connection reached. MAX_ALLOW_USER = {}", MAX_ALLOW_USER);
			socket_close(desc);
			return nullptr;
		}
	}

#if defined(__IMPROVED_HANDSHAKE_PROCESS__)
	// Check if the host is an intruder
	if (IsIntruder(host))
	{
		if (INTRUSIVE_HANDSHAKE_LOG)
			LOG_INFO("intrusive connection from {} was blocked temporarily.", host);
		socket_close(desc);
		return NULL;
	}

	// Check if the handshake is intrusive
	if (IsIntrusiveHandshake(host))
	{
		// Get the intrusive count
		int iIntrusiveCount = GetIntrusiveCount(host);
		if (iIntrusiveCount >= INTRUSIVE_HANDSHAKE_LIMIT)
		{
			// Set intruder with the next (delayed) handshake time
			SetIntruder(host, thecore_pulse() + PASSES_PER_SEC(INTRUSIVE_HANDSHAKE_NEXT_PULSE));
			socket_close(desc);
			return NULL;
		}

		// Set intrusive count
		if (INTRUSIVE_HANDSHAKE_LOG)
			LOG_INFO("intrusive connection from {}. {}/{}", host, iIntrusiveCount, INTRUSIVE_HANDSHAKE_LIMIT);
		SetIntrusiveCount(host, false /* reset */);
	}
	else
		// Allow handshake and clear previous intrusive tracking
		AllowHandshake(host);
#endif

	newd = M2_NEW DESC;
	crc_t handshake = CreateHandshake();

	if (!newd->Setup(fdw, desc, peer, ++m_iHandleCount, handshake))
	{
		socket_close(desc);
		M2_DELETE(newd);
		return nullptr;
	}

	m_map_handshake.insert(DESC_HANDSHAKE_MAP::value_type(handshake, newd));
	m_map_handle.insert(DESC_HANDLE_MAP::value_type(newd->GetHandle(), newd));

	m_set_pkDesc.insert(newd);
	++m_iSocketsConnected;
	return (newd);
}

LPDESC DESC_MANAGER::AcceptP2PDesc(LPFDWATCH fdw, socket_t bind_fd)
{
	socket_t           fd;
	sockaddr_in peer;
	char               host[MAX_HOST_LENGTH + 1];

	if ((fd = socket_accept(bind_fd, &peer)) == -1)
		return nullptr;

	strlcpy(host, inet_ntoa(peer.sin_addr), sizeof(host));

	auto pkDesc = M2_NEW DESC_P2P;

	if (!pkDesc->Setup(fdw, fd, host, peer.sin_port))
	{
		LOG_ERROR("DESC_MANAGER::AcceptP2PDesc : Setup failed");
		socket_close(fd);
		M2_DELETE(pkDesc);
		return nullptr;
	}

	m_set_pkDesc.insert(pkDesc);
	++m_iSocketsConnected;

	LOG_INFO("DESC_MANAGER::AcceptP2PDesc  {}:{}", host, peer.sin_port);
	P2P_MANAGER::instance().RegisterAcceptor(pkDesc);
	return (pkDesc);
}

void DESC_MANAGER::ConnectAccount(const std::string& login, LPDESC d)
{
	dev_log(LOG_DEB0, "BBBB ConnectAccount(%s)", login.c_str());
	m_map_loginName.insert(DESC_LOGINNAME_MAP::value_type(login, d));
#ifdef ENABLE_BLOCK_MULTIFARM
	int iStatus = 1, c = 0;
	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT status FROM account.antifarm WHERE hwid='%s'", d->GetHwid()));
	if (pMsg->Get()->uiNumRows != 0) {
		MYSQL_ROW row;
		while (nullptr != (row = mysql_fetch_row(pMsg->Get()->pSQLResult))) {
			if (atoi(row[0]) > 0)
				c += 1;
		}

		if (c >= 2) {
			iStatus = 0;
		}
	}

	std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("INSERT INTO account.antifarm SET hwid='%s', login='%s', status=%d", d->GetHwid(), login.c_str(), iStatus));
#endif
}

void DESC_MANAGER::DisconnectAccount(const std::string& login)
{
	dev_log(LOG_DEB0, "BBBB DisConnectAccount(%s)", login.c_str());
	m_map_loginName.erase(login);
#ifdef ENABLE_BLOCK_MULTIFARM
	std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("DELETE FROM account.antifarm WHERE login='%s'", login.c_str()));
#endif
}

void DESC_MANAGER::DestroyDesc(LPDESC d, bool bEraseFromSet)
{
	if (bEraseFromSet)
		m_set_pkDesc.erase(d);

	if (d->GetHandshake())
		m_map_handshake.erase(d->GetHandshake());

	if (d->GetHandle() != 0)
		m_map_handle.erase(d->GetHandle());
	else
		m_set_pkClientDesc.erase((LPCLIENT_DESC)d);

	// Explicit call to the virtual function Destroy()
	d->Destroy();

	M2_DELETE(d);
	--m_iSocketsConnected;
}

void DESC_MANAGER::DestroyClosed()
{
	auto i = m_set_pkDesc.begin();

	while (i != m_set_pkDesc.end())
	{
		LPDESC d = *i;
		const auto ci = i;
		++i;

		if (d->IsPhase(PHASE_CLOSE))
		{
			if (d->GetType() == DESC_TYPE_CONNECTOR)
			{
				LPCLIENT_DESC client_desc = (LPCLIENT_DESC)d;

				if (client_desc->IsRetryWhenClosed())
				{
					client_desc->Reset();
					continue;
				}
			}

			DestroyDesc(d, false);
			m_set_pkDesc.erase(ci);
		}
	}
}

LPDESC DESC_MANAGER::FindByLoginName(const std::string& login)
{
	const auto it = m_map_loginName.find(login);

	if (m_map_loginName.end() == it)
		return nullptr;

	return (it->second);
}

LPDESC DESC_MANAGER::FindByHandle(uint32_t handle)
{
	const auto it = m_map_handle.find(handle);

	if (m_map_handle.end() == it)
		return nullptr;

	return (it->second);
}

const DESC_MANAGER::DESC_SET& DESC_MANAGER::GetClientSet()
{
	return m_set_pkDesc;
}

struct name_with_desc_func
{
	const char* m_name;

	name_with_desc_func(const char* name) : m_name(name)
	{
	}

	bool operator () (LPDESC d)
	{
		if (d->GetCharacter() && !strcmp(d->GetCharacter()->GetName(), m_name))
			return true;

		return false;
	}
};

LPDESC DESC_MANAGER::FindByCharacterName(const char* name)
{
	const auto it = std::ranges::find_if(m_set_pkDesc, name_with_desc_func(name));
	return (it == m_set_pkDesc.end()) ? nullptr : (*it);
}

LPCLIENT_DESC DESC_MANAGER::CreateConnectionDesc(LPFDWATCH fdw, const char* host, uint16_t port, int iPhaseWhenSucceed, bool bRetryWhenClosed)
{
	const auto newd = M2_NEW CLIENT_DESC;

	newd->Setup(fdw, host, port);
	newd->Connect(iPhaseWhenSucceed);
	newd->SetRetryWhenClosed(bRetryWhenClosed);

	m_set_pkDesc.insert(newd);
	m_set_pkClientDesc.insert(newd);

	++m_iSocketsConnected;
	return (newd);
}

struct FuncTryConnect
{
	void operator () (LPDESC d)
	{
		((LPCLIENT_DESC)d)->Connect();
	}
};

void DESC_MANAGER::TryConnect()
{
	FuncTryConnect f;
	std::for_each(m_set_pkClientDesc.begin(), m_set_pkClientDesc.end(), f);
}

bool DESC_MANAGER::IsP2PDescExist(const char* szHost, uint16_t wPort)
{
	auto it = m_set_pkClientDesc.begin();

	while (it != m_set_pkClientDesc.end())
	{
		LPCLIENT_DESC d = *(it++);

		if (!strcmp(d->GetP2PHost(), szHost) && d->GetP2PPort() == wPort)
			return true;
	}

	return false;
}

LPDESC DESC_MANAGER::FindByHandshake(uint32_t dwHandshake)
{
	const auto it = m_map_handshake.find(dwHandshake);

	if (it == m_map_handshake.end())
		return nullptr;

	return (it->second);
}

class FuncWho
{
public:
	int iTotalCount;
	int aiEmpireUserCount[EMPIRE_MAX_NUM];

	FuncWho()
	{
		iTotalCount = 0;
		memset(aiEmpireUserCount, 0, sizeof(aiEmpireUserCount));
	}

	void operator() (LPDESC d)
	{
		if (d->GetCharacter())
		{
			++iTotalCount;
			++aiEmpireUserCount[d->GetEmpire()];
		}
	}
};

void DESC_MANAGER::UpdateLocalUserCount()
{
	const DESC_SET& c_ref_set = GetClientSet();
	FuncWho f;
	f = std::for_each(c_ref_set.begin(), c_ref_set.end(), f);

	m_iLocalUserCount = f.iTotalCount;
	memcpy(m_aiEmpireUserCount, f.aiEmpireUserCount, sizeof(m_aiEmpireUserCount));

	m_aiEmpireUserCount[1] += P2P_MANAGER::instance().GetEmpireUserCount(1);
	m_aiEmpireUserCount[2] += P2P_MANAGER::instance().GetEmpireUserCount(2);
	m_aiEmpireUserCount[3] += P2P_MANAGER::instance().GetEmpireUserCount(3);
}

void DESC_MANAGER::GetUserCount(int& iTotal, int** paiEmpireUserCount, int& iLocalCount)
{
	*paiEmpireUserCount = &m_aiEmpireUserCount[0];

	int iCount = P2P_MANAGER::instance().GetCount();
	if (iCount < 0)
	{
		LOG_ERROR("P2P_MANAGER::instance().GetCount() == -1");
	}
	iTotal = m_iLocalUserCount + iCount;
	iLocalCount = m_iLocalUserCount;
}


uint32_t DESC_MANAGER::MakeRandomKey(uint32_t dwHandle)
{
	uint32_t random_key = thecore_random();
	m_map_handle_random_key.insert(std::make_pair(dwHandle, random_key));
	return random_key;
}

bool DESC_MANAGER::GetRandomKey(uint32_t dwHandle, uint32_t* prandom_key)
{
	const auto it = m_map_handle_random_key.find(dwHandle);

	if (it == m_map_handle_random_key.end())
		return false;

	*prandom_key = it->second;
	return true;
}

LPDESC DESC_MANAGER::FindByLoginKey(uint32_t dwKey)
{
	const auto it = m_map_pkLoginKey.find(dwKey);

	if (it == m_map_pkLoginKey.end())
		return nullptr;

	return it->second->m_pkDesc;
}


uint32_t DESC_MANAGER::CreateLoginKey(LPDESC d)
{
	uint32_t dwKey = 0;

	do
	{
		dwKey = number(1, INT_MAX);

		if (m_map_pkLoginKey.contains(dwKey))
			continue;

		auto pkKey = M2_NEW CLoginKey(dwKey, d);
		d->SetLoginKey(pkKey);
		m_map_pkLoginKey.insert(std::make_pair(dwKey, pkKey));
		break;
	} while (1);

	return dwKey;
}

void DESC_MANAGER::ProcessExpiredLoginKey()
{
	uint32_t dwCurrentTime = get_dword_time();

	auto it = m_map_pkLoginKey.begin();

	while (it != m_map_pkLoginKey.end())
	{
		const auto it2 = it++;

		if (it2->second->m_dwExpireTime == 0)
			continue;

		if (dwCurrentTime - it2->second->m_dwExpireTime > 60000)
		{
			M2_DELETE(it2->second);
			m_map_pkLoginKey.erase(it2);
		}
	}
}
