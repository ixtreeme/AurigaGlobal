#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <sstream>
#include <vector>
#include <common/billing.h>
#include <common/length.h>

#include "db.h"

#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "item.h"
#include "item_manager.h"
#include "p2p.h"
#include "log.h"
#include "login_data.h"
#include "locale_service.h"
#include "pcbang.h"
#include "spam.h"
#include "shutdown_manager.h"
#ifdef ENABLE_HWID
#include "hwidmanager.h"
#include "Core/Logging.hpp"
#endif

DBManager::DBManager() : m_bIsConnect(false)
{
}

DBManager::~DBManager()
{
}

bool DBManager::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	if (m_sql.Setup(host, user, pwd, db, g_stLocale.c_str(), false, port))
		m_bIsConnect = true;

	if (!m_sql_direct.Setup(host, user, pwd, db, g_stLocale.c_str(), true, port))
		LOG_ERROR("cannot open direct sql connection to host {}", host);

	if (m_bIsConnect && !g_bAuthServer)
	{
		LoadDBString();
	}

	return m_bIsConnect;
}

void DBManager::Query(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	m_sql.AsyncQuery(szQuery);
}

SQLMsg * DBManager::DirectQuery(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

#ifdef ENABLE_BUG_FIXES
	std::string sQuery(szQuery);
	return m_sql_direct.DirectQuery(sQuery.substr(0, sQuery.find_first_of(";") == std::string::npos ? sQuery.length(): sQuery.find_first_of(";")).c_str());
#else
	return m_sql_direct.DirectQuery(szQuery);
#endif
}

bool DBManager::IsConnected()
{
	return m_bIsConnect;
}

void DBManager::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...)
{
	// LOG_INFO("ReturnQuery {}", c_pszQuery);
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	CReturnQueryInfo * p = M2_NEW CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(szQuery, p);
}

SQLMsg * DBManager::PopResult()
{
	SQLMsg * p;

	if (m_sql.PopResult(&p))
		return p;

	return nullptr;
}

void DBManager::Process()
{
	SQLMsg* pMsg = nullptr;

	while ((pMsg = PopResult()))
	{
		if(nullptr != pMsg->pvUserData )
		{
			switch( reinterpret_cast<CQueryInfo*>(pMsg->pvUserData)->iQueryType )
			{
				case QUERY_TYPE_RETURN:
					AnalyzeReturnQuery(pMsg);
					break;

				case QUERY_TYPE_FUNCTION:
					{
						CFuncQueryInfo* qi = reinterpret_cast<CFuncQueryInfo*>( pMsg->pvUserData );
						qi->f(pMsg);
						M2_DELETE(qi);
					}
					break;

				case QUERY_TYPE_AFTER_FUNCTION:
					{
						CFuncAfterQueryInfo* qi = reinterpret_cast<CFuncAfterQueryInfo*>( pMsg->pvUserData );
						qi->f();
						M2_DELETE(qi);
					}
					break;
			}
		}

		delete pMsg;
	}
}

CLoginData * DBManager::GetLoginData(uint32_t dwKey)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return nullptr;

	return it->second;
}

void DBManager::InsertLoginData(CLoginData * pkLD)
{
	m_map_pkLoginData.insert(std::make_pair(pkLD->GetKey(), pkLD));
}

void DBManager::DeleteLoginData(CLoginData * pkLD)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(pkLD->GetKey());

	if (it == m_map_pkLoginData.end())
		return;

	LOG_INFO("DeleteLoginData {} {}", pkLD->GetLogin(), static_cast<const void*>(pkLD));

	mapLDBilling.erase(pkLD->GetLogin());

	M2_DELETE(it->second);
	m_map_pkLoginData.erase(it);
}

void DBManager::SetBilling(uint32_t dwKey, bool bOn, bool bSkipPush)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
	{
		LOG_ERROR("cannot find login key {}", dwKey);
		return;
	}

	CLoginData * ld = it->second;

	auto it2 = mapLDBilling.find(ld->GetLogin());

	if (it2 != mapLDBilling.end())
		if (it2->second != ld)
			DeleteLoginData(it2->second);

	mapLDBilling.insert(std::make_pair(ld->GetLogin(), ld));

	if (ld->IsBilling() && !bOn && !bSkipPush)
		PushBilling(ld);

	SendLoginPing(ld->GetLogin());
	ld->SetBilling(bOn);
}

void DBManager::PushBilling(CLoginData * pkLD)
{
	TUseTime t;

	t.dwUseSec = (get_dword_time() - pkLD->GetLogonTime()) / 1000;

	if (t.dwUseSec <= 0)
		return;

	pkLD->SetLogonTime();
	int32_t lRemainSecs = pkLD->GetRemainSecs() - t.dwUseSec;
	pkLD->SetRemainSecs(MAX(0, lRemainSecs));

	t.dwLoginKey = pkLD->GetKey();
	t.bBillType = pkLD->GetBillType();

	LOG_INFO("BILLING: PUSH {} {} type {}", pkLD->GetLogin(), t.dwUseSec, t.bBillType);

	if (t.bBillType == BILLING_IP_FREE || t.bBillType == BILLING_IP_TIME || t.bBillType == BILLING_IP_DAY)
		snprintf(t.szLogin, sizeof(t.szLogin), "%u", pkLD->GetBillID());
	else
		strlcpy(t.szLogin, pkLD->GetLogin(), sizeof(t.szLogin));

	strlcpy(t.szIP, pkLD->GetIP(), sizeof(t.szIP));

	m_vec_kUseTime.push_back(t);
}

void DBManager::FlushBilling(bool bForce)
{
	if (bForce)
	{
		std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.begin();

		while (it != m_map_pkLoginData.end())
		{
			CLoginData * pkLD = (it++)->second;

			if (pkLD->IsBilling())
				PushBilling(pkLD);
		}
	}

	if (!m_vec_kUseTime.empty())
	{
		uint32_t dwCount = 0;

		std::vector<TUseTime>::iterator it = m_vec_kUseTime.begin();

		while (it != m_vec_kUseTime.end())
		{
			TUseTime * p = &(*(it++));

			// DISABLE_OLD_BILLING_CODE
			if (!g_bBilling)
			{
				++dwCount;
				continue;
			}

			Query("INSERT GameTimeLog (login, type, logon_time, logout_time, use_time, ip, server) "
					"VALUES('%s', %u, DATE_SUB(NOW(), INTERVAL %u SECOND), NOW(), %u, '%s', '%s')",
					p->szLogin, p->bBillType, p->dwUseSec, p->dwUseSec, p->szIP, g_stHostname.c_str());
			// DISABLE_OLD_BILLING_CODE_END

			switch (p->bBillType)
			{
				case BILLING_FREE:
				case BILLING_IP_FREE:
					break;

				case BILLING_DAY:
					{
						if (!bForce)
						{
							TUseTime * pInfo = M2_NEW TUseTime;
							memcpy(pInfo, p, sizeof(TUseTime));
							ReturnQuery(QID_BILLING_CHECK, 0, pInfo,
									"SELECT UNIX_TIMESTAMP(LimitDt)-UNIX_TIMESTAMP(NOW()),LimitTime FROM GameTime WHERE UserID='%s'", p->szLogin);
						}
					}
					break;

				case BILLING_TIME:
					{
						Query("UPDATE GameTime SET LimitTime=LimitTime-%u WHERE UserID='%s'", p->dwUseSec, p->szLogin);

						if (!bForce)
						{
							TUseTime * pInfo = M2_NEW TUseTime;
							memcpy(pInfo, p, sizeof(TUseTime));
							ReturnQuery(QID_BILLING_CHECK, 0, pInfo,
									"SELECT UNIX_TIMESTAMP(LimitDt)-UNIX_TIMESTAMP(NOW()),LimitTime FROM GameTime WHERE UserID='%s'", p->szLogin);
						}
					}
					break;

				case BILLING_IP_DAY:
					{
						if (!bForce)
						{
							TUseTime * pInfo = M2_NEW TUseTime;
							memcpy(pInfo, p, sizeof(TUseTime));
							ReturnQuery(QID_BILLING_CHECK, 0, pInfo,
									"SELECT UNIX_TIMESTAMP(LimitDt)-UNIX_TIMESTAMP(NOW()),LimitTime FROM GameTimeIP WHERE ipid=%s", p->szLogin);
						}
					}
					break;

				case BILLING_IP_TIME:
					{
						Query("UPDATE GameTimeIP SET LimitTime=LimitTime-%u WHERE ipid=%s", p->dwUseSec, p->szLogin);

						if (!bForce)
						{
							TUseTime * pInfo = M2_NEW TUseTime;
							memcpy(pInfo, p, sizeof(TUseTime));
							ReturnQuery(QID_BILLING_CHECK, 0, pInfo,
									"SELECT UNIX_TIMESTAMP(LimitDt)-UNIX_TIMESTAMP(NOW()),LimitTime FROM GameTimeIP WHERE ipid=%s", p->szLogin);
						}
					}
					break;
			}

			if (!bForce && ++dwCount >= 1000)
				break;
		}

		if (dwCount < m_vec_kUseTime.size())
		{
			int nNewSize = m_vec_kUseTime.size() - dwCount;
			memcpy(&m_vec_kUseTime[0], &m_vec_kUseTime[dwCount], sizeof(TUseTime) * nNewSize);
			m_vec_kUseTime.resize(nNewSize);
		}
		else
			m_vec_kUseTime.clear();

		LOG_INFO("FLUSH_USE_TIME: count {}", dwCount);
	}

	if (m_vec_kUseTime.size() < 10240)
	{
		uint32_t dwCurTime = get_dword_time();

		std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.begin();

		while (it != m_map_pkLoginData.end())
		{
			CLoginData * pkLD = (it++)->second;

			if (!pkLD->IsBilling())
				continue;

			switch (pkLD->GetBillType())
			{
				case BILLING_IP_FREE:
				case BILLING_FREE:
					break;

				case BILLING_IP_DAY:
				case BILLING_DAY:
				case BILLING_IP_TIME:
				case BILLING_TIME:
					if (pkLD->GetRemainSecs() < 0)
					{
						uint32_t dwSecsConnected = (dwCurTime - pkLD->GetLogonTime()) / 1000;

						if (dwSecsConnected % 10 == 0)
							SendBillingExpire(pkLD->GetLogin(), BILLING_DAY, 0, pkLD);
					}
					else if (pkLD->GetRemainSecs() <= 600) // if remain seconds lower than 10 minutes
					{
						uint32_t dwSecsConnected = (dwCurTime - pkLD->GetLogonTime()) / 1000;

						if (dwSecsConnected >= 60) // 60 second cycle
						{
							LOG_INFO("BILLING 1 {} remain {} connected secs {}", pkLD->GetLogin(), pkLD->GetRemainSecs(), dwSecsConnected);
							PushBilling(pkLD);
						}
					}
					else
					{
						uint32_t dwSecsConnected = (dwCurTime - pkLD->GetLogonTime()) / 1000;

						if (dwSecsConnected > (uint32_t) (pkLD->GetRemainSecs() - 600) || dwSecsConnected >= 600)
						{
							LOG_INFO("BILLING 2 {} remain {} connected secs {}", pkLD->GetLogin(), pkLD->GetRemainSecs(), dwSecsConnected);
							PushBilling(pkLD);
						}
					}
					break;
			}
		}
	}

}

void DBManager::CheckBilling()
{
	std::vector<uint32_t> vec;
	vec.push_back(0); // ī��Ʈ�� ���� �̸� ����д�.

	// LOG_INFO("CheckBilling: map size {}", m_map_pkLoginData.size());

	auto it = m_map_pkLoginData.begin();

	while (it != m_map_pkLoginData.end())
	{
		CLoginData * pkLD = (it++)->second;

		if (pkLD->IsBilling())
		{
			LOG_INFO("BILLING: CHECK {}", pkLD->GetKey());
			vec.push_back(pkLD->GetKey());
		}
	}

	vec[0] = vec.size() - 1; // ����� ���� ����� �ִ´�, ������ �ڽ��� �����ؾ� �ϹǷ� -1
	db_clientdesc->DBPacket(HEADER_GD_BILLING_CHECK, 0, &vec[0], sizeof(uint32_t) * vec.size());
}

void DBManager::SendLoginPing(const char * c_pszLogin)
{
/*
	TPacketGGLoginPing ptog;

	ptog.bHeader = HEADER_GG_LOGIN_PING;
	strlcpy(ptog.szLogin, c_pszLogin, sizeof(ptog.szLogin));

	if (!g_pkAuthMasterDesc)  // If I am master, broadcast to others
	{
		P2P_MANAGER::instance().Send(&ptog, sizeof(TPacketGGLoginPing));
	}
	else // If I am slave send login ping to master
	{
		g_pkAuthMasterDesc->Packet(&ptog, sizeof(TPacketGGLoginPing));
	}
*/
}

void DBManager::SendAuthLogin(LPDESC d)
{
	const TAccountTable & r = d->GetAccountTable();

	CLoginData * pkLD = GetLoginData(d->GetLoginKey());

	if (!pkLD)
		return;

	TPacketGDAuthLogin ptod;
	ptod.dwID = r.id;
#ifdef ENABLE_HWID
	strlcpy(ptod.hwid, r.hwid, sizeof(ptod.hwid));
#endif
	trim_and_lower(r.login, ptod.szLogin, sizeof(ptod.szLogin));
	strlcpy(ptod.szSocialID, r.social_id, sizeof(ptod.szSocialID));
	ptod.dwLoginKey = d->GetLoginKey();
#ifdef ENABLE_MULTI_LANGUAGE
	ptod.bLanguage = r.bLanguage;
#endif
	ptod.bBillType = pkLD->GetBillType();
	ptod.dwBillID = pkLD->GetBillID();

	memcpy(ptod.iPremiumTimes, pkLD->GetPremiumPtr(), sizeof(ptod.iPremiumTimes));
	memcpy(&ptod.adwClientKey, pkLD->GetClientKey(), sizeof(uint32_t) * 4);

	db_clientdesc->DBPacket(HEADER_GD_AUTH_LOGIN, d->GetHandle(), &ptod, sizeof(TPacketGDAuthLogin));
	LOG_INFO("SendAuthLogin {} key {}", ptod.szLogin, ptod.dwID);

	SendLoginPing(r.login);
}

void DBManager::LoginPrepare(uint8_t bBillType, uint32_t dwBillID, int32_t lRemainSecs, LPDESC d, uint32_t * pdwClientKey, int * paiPremiumTimes)
{
	const TAccountTable & r = d->GetAccountTable();

	CLoginData * pkLD = M2_NEW CLoginData;

	pkLD->SetKey(d->GetLoginKey());
	pkLD->SetLogin(r.login);
	pkLD->SetBillType(bBillType);
	pkLD->SetBillID(dwBillID);
	pkLD->SetRemainSecs(lRemainSecs);
	pkLD->SetIP(d->GetHostName());
	pkLD->SetClientKey(pdwClientKey);

	if (paiPremiumTimes)
		pkLD->SetPremium(paiPremiumTimes);

	InsertLoginData(pkLD);
	SendAuthLogin(d);
}

bool GetGameTimeIP(MYSQL_RES * pRes, uint8_t & bBillType, uint32_t & dwBillID, int & seconds, const char * c_pszIP)
{
	if (!pRes)
		return true;

	MYSQL_ROW row = mysql_fetch_row(pRes);
	int col = 0;

	str_to_number(dwBillID, row[col++]);

	int ip_start = 0;
	str_to_number(ip_start, row[col++]);

	int ip_end = 0;
	str_to_number(ip_end, row[col++]);

	int type = 0;
	str_to_number(type, row[col++]);

	str_to_number(seconds, row[col++]);

	int day_seconds = 0;
	str_to_number(day_seconds, row[col++]);

	char szIP[MAX_HOST_LENGTH + 1];
	strlcpy(szIP, c_pszIP, sizeof(szIP));

	char * p = strrchr(szIP, '.');
	++p;

	int ip_postfix = 0;
	str_to_number(ip_postfix, p);
	int valid_ip = false;

	if (ip_start <= ip_postfix && ip_end >= ip_postfix)
		valid_ip = true;

	bBillType = BILLING_NONE;

	if (valid_ip)
	{
		if (type == -1)
			return false;

		if (type == 0)
			bBillType = BILLING_IP_FREE;
		else if (day_seconds > 0)
		{
			bBillType = BILLING_IP_DAY;
			seconds = day_seconds;
		}
		else if (seconds > 0)
			bBillType = BILLING_IP_TIME;
	}

	return true;
}

bool GetGameTime(MYSQL_RES * pRes, uint8_t & bBillType, int & seconds)
{
	if (!pRes)
		return true;

	MYSQL_ROW row = mysql_fetch_row(pRes);
	LOG_INFO("GetGameTime {} {} {}", static_cast<const void*>(row[0]), static_cast<const void*>(row[1]), static_cast<const void*>(row[2]));

	int type = 0;
	str_to_number(type, row[0]);
	str_to_number(seconds, row[1]);
	int day_seconds = 0;
	str_to_number(day_seconds, row[2]);
	bBillType = BILLING_NONE;

	if (type == -1)
		return false;
	else if (type == 0)
		bBillType = BILLING_FREE;
	else if (day_seconds > 0)
	{
		bBillType = BILLING_DAY;
		seconds = day_seconds;
	}
	else if (seconds > 0)
		bBillType = BILLING_TIME;

	if (!g_bBilling)
		bBillType = BILLING_FREE;

	return true;
}

void SendBillingExpire(const char * c_pszLogin, uint8_t bBillType, int iSecs, CLoginData * pkLD)
{
	TPacketBillingExpire ptod;

	strlcpy(ptod.szLogin, c_pszLogin, sizeof(ptod.szLogin));
	ptod.bBillType = bBillType;
	ptod.dwRemainSeconds = MAX(0, iSecs);
	db_clientdesc->DBPacket(HEADER_GD_BILLING_EXPIRE, 0, &ptod, sizeof(TPacketBillingExpire));
	LOG_INFO("BILLING: EXPIRE {} type {} sec {} ptr {}", c_pszLogin, bBillType, iSecs, static_cast<const void*>(pkLD));
}

void DBManager::AnalyzeReturnQuery(SQLMsg * pMsg)
{
	CReturnQueryInfo * qi = (CReturnQueryInfo *) pMsg->pvUserData;

	switch (qi->iType)
	{
		case QID_AUTH_LOGIN:
			{
				TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) qi->pvData;
				LPDESC d = DESC_MANAGER::instance().FindByLoginKey(qi->dwIdent);

				if (!d)
				{
					M2_DELETE(pinfo);
					break;
				}
				//��ġ ���� - By SeMinZ
				d->SetLogin(pinfo->login);

				LOG_INFO("QID_AUTH_LOGIN: START {} {}", qi->dwIdent, static_cast<const void*>(get_pointer(d)));

				if (pMsg->Get()->uiNumRows == 0)
				{
					{
						LOG_INFO("   NOID");
						LoginFailure(d, "NOID");
						M2_DELETE(pinfo);
					}
				}
				else
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
					int col = 0;

					// PASSWORD('%s'), password, social_id, id, status
					char szEncrytPassword[45 + 1] = {};
					char szPassword[45 + 1] = {};
					char szSocialID[SOCIAL_ID_MAX_LEN + 1] = {};
					char szStatus[ACCOUNT_STATUS_MAX_LEN + 1] = {};
					uint32_t dwID = 0;
#ifdef ENABLE_MULTI_LANGUAGE
					uint8_t bLanguage = DEFAULT_LANGUAGE;
#endif

					if (!row[col])
					{
						LOG_ERROR("error column {}", col);
						M2_DELETE(pinfo);
					   	break;
					}

					strlcpy(szEncrytPassword, row[col++], sizeof(szEncrytPassword));

					if (!row[col])
					{
					   	LOG_ERROR("error column {}", col);
						M2_DELETE(pinfo);
					   	break;
				   	}

					strlcpy(szPassword, row[col++], sizeof(szPassword));

					if (!row[col])
				   	{
						LOG_ERROR("error column {}", col);
						M2_DELETE(pinfo);
						break;
				   	}

					strlcpy(szSocialID, row[col++], sizeof(szSocialID));

					if (!row[col])
				   	{
					   	LOG_ERROR("error column {}", col);
						M2_DELETE(pinfo);
					   	break;
				   	}

					str_to_number(dwID, row[col++]);

					if (!row[col])
					{
					   	LOG_ERROR("error column {}", col);
						M2_DELETE(pinfo);
						break;
				   	}

#ifdef ENABLE_MULTI_LANGUAGE
					if (!row[col])
					{
					   	LOG_ERROR("error column {}", col);
						M2_DELETE(pinfo);
						break;
				   	}

					str_to_number(bLanguage, row[col++]);
#endif

					strlcpy(szStatus, row[col++], sizeof(szStatus));

					uint8_t bNotAvail = 0;
					str_to_number(bNotAvail, row[col++]);

					int aiPremiumTimes[PREMIUM_MAX_NUM];
					memset(&aiPremiumTimes, 0, sizeof(aiPremiumTimes));

					char szCreateDate[256] = "00000000";

					{
						str_to_number(aiPremiumTimes[PREMIUM_EXP], row[col++]);
						str_to_number(aiPremiumTimes[PREMIUM_ITEM], row[col++]);
						str_to_number(aiPremiumTimes[PREMIUM_SAFEBOX], row[col++]);
						str_to_number(aiPremiumTimes[PREMIUM_AUTOLOOT], row[col++]);
						str_to_number(aiPremiumTimes[PREMIUM_FISH_MIND], row[col++]);
						str_to_number(aiPremiumTimes[PREMIUM_MARRIAGE_FAST], row[col++]);
						str_to_number(aiPremiumTimes[PREMIUM_GOLD], row[col++]);

						{
							int32_t retValue = 0;
							str_to_number(retValue, row[col]);

							time_t create_time = retValue;
							struct tm * tm1;
							tm1 = localtime(&create_time);
							strftime(szCreateDate, 255, "%Y%m%d", tm1);

							LOG_INFO("Create_Time {} {}", retValue, szCreateDate);
							LOG_INFO("Block Time {} ", strncmp(szCreateDate, g_stBlockDate.c_str(), 8));
						}
					}

					int nPasswordDiff = strcmp(szEncrytPassword, szPassword);

					//OpenID : OpenID �� ���, ��й�ȣ üũ�� ���� �ʴ´�.
					if (openid_server)
					{
						nPasswordDiff = 0;
					}

					if (nPasswordDiff)
					{
						LoginFailure(d, "WRONGPWD");
						LOG_INFO("   WRONGPWD");
						M2_DELETE(pinfo);
					}
					else if (bNotAvail)
					{
						LoginFailure(d, "NOTAVAIL");
						LOG_INFO("   NOTAVAIL");
						M2_DELETE(pinfo);
					}
					else if (DESC_MANAGER::instance().FindByLoginName(pinfo->login))
					{
						LoginFailure(d, "ALREADY");
						LOG_INFO("   ALREADY");
						M2_DELETE(pinfo);
					}
					else if(!CShutdownManager::Instance().CheckCorrectSocialID(szSocialID) && !test_server)
					{
						LoginFailure(d, "BADSCLID");
						LOG_INFO("   BADSCLID");
						M2_DELETE(pinfo);
					}
					else if(CShutdownManager::Instance().CheckShutdownAge(szSocialID) && CShutdownManager::Instance().CheckShutdownTime())
					{
						LoginFailure(d, "AGELIMIT");
						LOG_INFO("   AGELIMIT");
						M2_DELETE(pinfo);
					}
					else if (strcmp(szStatus, "OK"))
					{
						LoginFailure(d, szStatus);
						LOG_INFO("   STATUS: {}", szStatus);
						M2_DELETE(pinfo);
					}
					else
					{
						if (strncmp(szCreateDate, g_stBlockDate.c_str(), 8) >= 0)
						{
							LoginFailure(d, "BLKLOGIN");
							LOG_INFO("   BLKLOGIN");
							M2_DELETE(pinfo);
							break;
						}

#ifdef ENABLE_HWID
						char hwid[HWID_LENGTH * 2 + 1];
						DBManager::instance().EscapeString(hwid, sizeof(hwid), pinfo->hwid, strlen(pinfo->hwid));

						if (strlen(hwid) == 0) {
							LoginFailure(d, "UPDATE");
							M2_DELETE(pinfo);
							break;
						}

						if (CHwidManager::Instance().IsBlocked(hwid)) {
							CHwidManager::Instance().AddHwidToAccount(pinfo->login, hwid);

							LoginFailure(d, "BAN2");
							M2_DELETE(pinfo);
							break;
						} else {
							std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("UPDATE account.account SET hwid='%s' WHERE login='%s'", hwid, pinfo->login));
							std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("UPDATE player.player_index SET hwid='%s' WHERE id='%d'", hwid, dwID));
						}
#endif

						std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery("UPDATE account SET last_play=NOW() WHERE id=%u", dwID));

						TAccountTable & r = d->GetAccountTable();
						r.id = dwID;
#ifdef ENABLE_HWID
						strlcpy(r.hwid, hwid, sizeof(r.hwid));
#endif
						trim_and_lower(pinfo->login, r.login, sizeof(r.login));
						strlcpy(r.passwd, pinfo->passwd, sizeof(r.passwd));
						strlcpy(r.social_id, szSocialID, sizeof(r.social_id));
#ifdef ENABLE_MULTI_LANGUAGE
						r.bLanguage = bLanguage;
#endif
						DESC_MANAGER::instance().ConnectAccount(r.login, d);

						if (!g_bBilling)
						{
							LoginPrepare(BILLING_FREE, 0, 0, d, pinfo->adwClientKey, aiPremiumTimes);
							//By SeMinZ
							M2_DELETE(pinfo);
							break;
						}

						LOG_INFO("QID_AUTH_LOGIN: SUCCESS {}", pinfo->login);
					}
				}
			}
			break;

		case QID_BILLING_GET_TIME:
			{
				TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) qi->pvData;
				LPDESC d = DESC_MANAGER::instance().FindByLoginKey(qi->dwIdent);

				LOG_INFO("QID_BILLING_GET_TIME: START ident {} d {}", qi->dwIdent, static_cast<const void*>(get_pointer(d)));

				if (d)
				{
					if (pMsg->Get()->uiNumRows == 0)
					{
						if (g_bBilling)
							LoginFailure(d, "NOBILL");
						else
							LoginPrepare(BILLING_FREE, 0, 0, d, pinfo->adwClientKey);
					}
					else
					{
						int seconds = 0;
						uint8_t bBillType = BILLING_NONE;

						if (!GetGameTime(pMsg->Get()->pSQLResult, bBillType, seconds))
						{
							LOG_INFO("QID_BILLING_GET_TIME: BLOCK");
							LoginFailure(d, "BLOCK");
						}
						else if (bBillType == BILLING_NONE)
						{
							LoginFailure(d, "NOBILL");
							LOG_INFO("QID_BILLING_GET_TIME: NO TIME");
						}
						else
						{
							LoginPrepare(bBillType, 0, seconds, d, pinfo->adwClientKey);
							LOG_INFO("QID_BILLING_GET_TIME: SUCCESS");
						}
					}
				}
				M2_DELETE(pinfo);
			}
			break;

		case QID_BILLING_CHECK:
			{
				TUseTime * pinfo = (TUseTime *) qi->pvData;
				int iRemainSecs = 0;

				CLoginData * pkLD = nullptr;

				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

					int iLimitDt = 0;
					str_to_number(iLimitDt, row[0]);

					int iLimitTime = 0;
					str_to_number(iLimitTime, row[1]);

					pkLD = GetLoginData(pinfo->dwLoginKey);

					if (pkLD)
					{
						switch (pkLD->GetBillType())
						{
							case BILLING_TIME:
								if (iLimitTime <= 600 && iLimitDt > 0)
								{
									iRemainSecs = iLimitDt;
									pkLD->SetBillType(BILLING_DAY);
									pinfo->bBillType = BILLING_DAY;
								}
								else
									iRemainSecs = iLimitTime;
								break;

							case BILLING_IP_TIME:
								if (iLimitTime <= 600 && iLimitDt > 0)
								{
									iRemainSecs = iLimitDt;
									pkLD->SetBillType(BILLING_IP_DAY);
									pinfo->bBillType = BILLING_IP_DAY;
								}
								else
									iRemainSecs = iLimitTime;
								break;

							case BILLING_DAY:
							case BILLING_IP_DAY:
								iRemainSecs = iLimitDt;
								break;
						}

						pkLD->SetRemainSecs(iRemainSecs);
					}
				}

				SendBillingExpire(pinfo->szLogin, pinfo->bBillType, MAX(0, iRemainSecs), pkLD);
				M2_DELETE(pinfo);
			}
			break;



		case QID_MOUNT_INVENTORY_LOAD:
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);
			if (ch)
			{
				SQLResult* res = pMsg->Get();
				std::vector<TMountInventoryItemTable> items;
				items.reserve(res->uiNumRows);

				for (uint64_t i = 0; i < res->uiNumRows; ++i)
				{
					MYSQL_ROW row = mysql_fetch_row(res->pSQLResult);
					if (!row)
						continue;

					TMountInventoryItemTable entry{};
					str_to_number(entry.id, row[0]);
					str_to_number(entry.slot, row[1]);
					str_to_number(entry.vnum, row[2]);
					str_to_number(entry.count, row[3]);
					str_to_number(entry.alSockets[0], row[4]);
					str_to_number(entry.alSockets[1], row[5]);
					str_to_number(entry.alSockets[2], row[6]);

					for (int j = 0; j < 6; ++j)
					{
						int typeIndex = 7 + j * 2;
						int valueIndex = typeIndex + 1;
						str_to_number(entry.aAttr[j].bType, row[typeIndex]);
						str_to_number(entry.aAttr[j].sValue, row[valueIndex]);
					}

					items.push_back(entry);
				}

				ch->LoadMountInventory(items);
			}
		}
		break;


		case QID_SAFEBOX_SIZE:
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);

				if (ch)
				{
					if (pMsg->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
						int	size = 0;
						str_to_number(size, row[0]);
						ch->SetSafeboxSize(SAFEBOX_PAGE_SIZE * size);
					}
				}
			}
			break;

		case QID_DB_STRING:
			{
				m_map_dbstring.clear();
				m_vec_GreetMessage.clear();

				for (uint64_t i = 0; i < pMsg->Get()->uiNumRows; ++i)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
					//ch->SetSafeboxSize(SAFEBOX_PAGE_SIZE * atoi(row[0]));
					if (row[0] && row[1])
					{
						m_map_dbstring.insert(make_pair(std::string(row[0]), std::string(row[1])));
						LOG_INFO("DBSTR '{}' '{}'", row[0], row[1]);
					}
				}
				if (m_map_dbstring.find("GREET") != m_map_dbstring.end())
				{
					std::istringstream is(m_map_dbstring["GREET"]);
					while (!is.eof())
					{
						std::string str;
						getline(is, str);
						m_vec_GreetMessage.push_back(str);
					}
				}
			}
			break;

		case QID_LOTTO:
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);
				uint32_t * pdw = (uint32_t *) qi->pvData;

				if (ch)
				{
					if (pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1)
					{
						LOG_INFO("GIVE LOTTO FAIL TO pid {}", (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
					}
					else
					{
						LPITEM pkItem = ch->AutoGiveItem(pdw[0], pdw[1]);

						if (pkItem)
						{
							LOG_INFO("GIVE LOTTO SUCCESS TO {} (pid {})", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), qi->dwIdent);
							ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, pkItem), 0, pMsg->Get()->uiInsertID);
							ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, pkItem), 1, pdw[2]);
						}
						else
							LOG_INFO("GIVE LOTTO FAIL2 TO pid {}", (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
					}
				}

				M2_DELETE_ARRAY(pdw);
			}
			break;

		case QID_HIGHSCORE_REGISTER:
			{
				THighscoreRegisterQueryInfo * info = (THighscoreRegisterQueryInfo *) qi->pvData;
				bool bQuery = true;

				if (pMsg->Get()->uiNumRows)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

					if (row && row[0])
					{
						int iCur = 0;
						str_to_number(iCur, row[0]);

						if ((info->bOrder && iCur >= info->iValue) ||
								(!info->bOrder && iCur <= info->iValue))
							bQuery = false;
					}
				}

				if (bQuery)
					Query("REPLACE INTO highscore%s VALUES('%s', %u, %d)",
							get_table_postfix(), info->szBoard, info->dwPID, info->iValue);

				M2_DELETE(info);
			}
			break;

		case QID_HIGHSCORE_SHOW:
			{
			}
			break;

			// BLOCK_CHAT
		case QID_BLOCK_CHAT_LIST:
			{
				LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);

				if (ch == nullptr)
					break;
				if (pMsg->Get()->uiNumRows)
				{
					MYSQL_ROW row;
					while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
					{
						ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "%s %s sec", row[0], row[1]);
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 820, "");
				}
#endif
			}
			break;
			// END_OF_BLOCK_CHAT

			// PCBANG_IP_LIST
		case QID_PCBANG_IP_LIST_CHECK:
			{
				const std::string PCBANG_IP_TABLE_NAME("pcbang_ip");

				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row;
					bool isFinded = false;

					while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
					{
						const char* c_szName = row[0];
						const char* c_szUpdateTime = row[12];

						if (test_server)
							LOG_INFO("{}:{}", c_szName, c_szUpdateTime);

						if (PCBANG_IP_TABLE_NAME == c_szName)
						{
							isFinded = true;

							static std::string s_stLastTime;
							if (s_stLastTime != c_szUpdateTime)
							{
								s_stLastTime = c_szUpdateTime;
								LOG_INFO("'{}' mysql table is UPDATED({})", PCBANG_IP_TABLE_NAME.c_str(), c_szUpdateTime);
								ReturnQuery(QID_PCBANG_IP_LIST_SELECT, 0, nullptr, "SELECT pcbang_id, ip FROM %s;", PCBANG_IP_TABLE_NAME.c_str());
							}
							else
							{
								LOG_INFO("'{}' mysql table is NOT updated({})", PCBANG_IP_TABLE_NAME.c_str(), c_szUpdateTime);
							}
							break;
						}
					}

					if (!isFinded)
					{
						LOG_ERROR("'{}' mysql table CANNOT FIND", PCBANG_IP_TABLE_NAME.c_str());
					}
				}
				else if (test_server)
				{
					LOG_ERROR("'{}' mysql table is NOT EXIST", PCBANG_IP_TABLE_NAME.c_str());
				}
			}
			break;

		case QID_PCBANG_IP_LIST_SELECT:
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row;

					while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
					{
						CPCBangManager::instance().InsertIP(row[0], row[1]);
					}
				}
				else if (test_server)
				{
					LOG_INFO("PCBANG_IP_LIST is EMPTY");
				}
			}
			break;


			// END_OF_PCBANG_IP_LIST
		default:
			LOG_ERROR("FATAL ERROR!!! Unhandled return query id {}", qi->iType);
			break;
	}

	M2_DELETE(qi);
}

void DBManager::LoadDBString()
{
	ReturnQuery(QID_DB_STRING, 0, nullptr, "SELECT name, text FROM string%s", get_table_postfix());
}

const std::string& DBManager::GetDBString(const std::string& key)
{
	static std::string null_str = "";
	auto it = m_map_dbstring.find(key);
	if (it == m_map_dbstring.end())
		return null_str;
	return it->second;
}

const std::vector<std::string>& DBManager::GetGreetMessage()
{
	return m_vec_GreetMessage;
}

void DBManager::SendMoneyLog(uint8_t type, uint32_t vnum, int64_t gold)
{
	if (!gold)
		return;
	TPacketMoneyLog p;
	p.type = type;
	p.vnum = vnum;
	p.gold = gold;
	db_clientdesc->DBPacket(HEADER_GD_MONEY_LOG, 0, &p, sizeof(p));
}

void VCardUse(LPCHARACTER CardOwner, LPCHARACTER CardTaker, LPITEM item)
{
	TPacketGDVCard p;

	p.dwID = ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item), 0);
	strlcpy(p.szSellCharacter, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(CardOwner)).data(), sizeof(p.szSellCharacter));
	strlcpy(p.szSellAccount, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(CardOwner))->GetAccountTable().login, sizeof(p.szSellAccount));
	strlcpy(p.szBuyCharacter, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(CardTaker)).data(), sizeof(p.szBuyCharacter));
	strlcpy(p.szBuyAccount, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(CardTaker))->GetAccountTable().login, sizeof(p.szBuyAccount));

	db_clientdesc->DBPacket(HEADER_GD_VCARD, 0, &p, sizeof(TPacketGDVCard));
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(AIHelpers::EcsOf(CardTaker), CHAT_TYPE_INFO, 101, "%d", ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item), 1) / 60, ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, item), 0));
#endif
	LogManager::instance().VCardLog(p.dwID, ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(CardTaker)), ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(CardTaker)), g_stHostname.c_str(),
			ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(CardOwner)).data(), ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(CardOwner))->GetHostName(),
			ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(CardTaker)).data(), ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(CardTaker))->GetHostName());

	ITEM_MANAGER::instance().RemoveItem(item);

	LOG_INFO("VCARD_TAKE: {} {} -> {}", p.dwID, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(CardOwner)).data(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(CardTaker)).data());
}

void DBManager::StopAllBilling()
{
	for (auto it = m_map_pkLoginData.begin(); it != m_map_pkLoginData.end(); ++it)
	{
		SetBilling(it->first, false);
	}
}

uint32_t DBManager::EscapeString(char* dst, uint64_t dstSize, const char *src, uint32_t srcSize)
{
	return m_sql_direct.EscapeString(dst, dstSize, src, srcSize);
}

//
// Common SQL
//
AccountDB::AccountDB() :
	m_IsConnect(false)
{
}

bool AccountDB::IsConnected()
{
	return m_IsConnect;
}

bool AccountDB::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	m_IsConnect = m_sql_direct.Setup(host, user, pwd, db, "", true, port);

	if (false == m_IsConnect)
	{
		LOG_ERROR("cannot open direct sql connection to host: {} user: {} db: {}", host, user, db);
		return false;
	}

	return m_IsConnect;
}

bool AccountDB::ConnectAsync(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale)
{
	m_sql.Setup(host, user, pwd, db, locale, false, port);
	return true;
}

void AccountDB::SetLocale(const std::string & stLocale)
{
	m_sql_direct.SetLocale(stLocale);
	m_sql_direct.QueryLocaleSet();
}

SQLMsg* AccountDB::DirectQuery(const char * query)
{
	return m_sql_direct.DirectQuery(query);
}

void AccountDB::AsyncQuery(const char* query)
{
	m_sql.AsyncQuery(query);
}

void AccountDB::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	CReturnQueryInfo * p = M2_NEW CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(szQuery, p);
}

SQLMsg * AccountDB::PopResult()
{
	SQLMsg * p;

	if (m_sql.PopResult(&p))
		return p;

	return nullptr;
}

void AccountDB::Process()
{
	SQLMsg* pMsg = nullptr;

	while ((pMsg = PopResult()))
	{
		CQueryInfo* qi = (CQueryInfo *) pMsg->pvUserData;

		switch (qi->iQueryType)
		{
			case QUERY_TYPE_RETURN:
				AnalyzeReturnQuery(pMsg);
				break;
		}
	}

	delete pMsg;
}

extern unsigned int g_uiSpamReloadCycle;

enum EAccountQID
{
	QID_SPAM_DB,
};

// 10�и��� ���ε�
static LPEVENT s_pkReloadSpamEvent = nullptr;

EVENTINFO(reload_spam_event_info)
{
	// used to send command
	uint32_t empty;
};

EVENTFUNC(reload_spam_event)
{
	AccountDB::instance().ReturnQuery(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");
	return PASSES_PER_SEC(g_uiSpamReloadCycle);
}

//#define ENABLE_SPAMDB_REFRESH
void LoadSpamDB()
{
	AccountDB::instance().ReturnQuery(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");
#ifdef ENABLE_SPAMDB_REFRESH
	if (NULL == s_pkReloadSpamEvent)
	{
		reload_spam_event_info* info = AllocEventInfo<reload_spam_event_info>();
		s_pkReloadSpamEvent = event_create(reload_spam_event, info, PASSES_PER_SEC(g_uiSpamReloadCycle));
	}
#endif
}

void CancelReloadSpamEvent() {
	s_pkReloadSpamEvent = nullptr;
}

void AccountDB::AnalyzeReturnQuery(SQLMsg * pMsg)
{
	CReturnQueryInfo * qi = (CReturnQueryInfo *) pMsg->pvUserData;

	switch (qi->iType)
	{
		case QID_SPAM_DB:
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row;

					SpamManager::instance().Clear();

					while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
						SpamManager::instance().Insert(row[0], atoi(row[1]));
				}
			}
			break;
	}

	M2_DELETE(qi);
}


