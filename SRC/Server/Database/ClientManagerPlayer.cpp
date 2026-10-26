
#include "stdafx.h"
#include "player_create.h"
#include <Core/packet_string.h>

#include "ClientManager.h"

#include "Main.h"
#include "QID.h"
#include "ItemAwardManager.h"
#include "Cache.h"
#include "player_load.h"

#ifdef __SKILL_COLOR_SYSTEM__
	#include <common/length.h>
#endif
#include <common/CommonDefines.h>

#include <utility>

extern std::string g_stLocale;
extern int g_test_server;
extern int g_log;

//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!! IMPORTANT !!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// Check all SELECT syntax on item table before change this function!!!
//
bool CreateItemTableFromRes(MYSQL_RES * res, std::vector<TPlayerItem> * pVec, uint32_t dwPID)
{
	if (!res)
	{
		pVec->clear();
		return true;
	}

	uint64_t rows;

	if ((rows = mysql_num_rows(res)) <= 0)
	{
		pVec->clear();
		return true;
	}

	pVec->resize(rows);

	for (uint64_t i = 0; i < rows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(res);
		TPlayerItem & item = pVec->at(i);

		int cur = 0;

		// Check all SELECT syntax on item table before change this function!!!
		// Check all SELECT syntax on item table before change this function!!!
		// Check all SELECT syntax on item table before change this function!!!
		str_to_number(item.id, row[cur++]);
		str_to_number(item.window, row[cur++]);
		str_to_number(item.pos, row[cur++]);
		str_to_number(item.count, row[cur++]);
		str_to_number(item.vnum, row[cur++]);
#ifdef ATTR_LOCK
		str_to_number(item.lockedattr, row[cur++]);
#endif
		str_to_number(item.alSockets[0], row[cur++]);
		str_to_number(item.alSockets[1], row[cur++]);
		str_to_number(item.alSockets[2], row[cur++]);

		for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; j++)
		{
			str_to_number(item.aAttr[j].bType, row[cur++]);
			str_to_number(item.aAttr[j].sValue, row[cur++]);
		}

		item.owner		= dwPID;
	}

	return true;
}

#ifdef __SKILL_COLOR_SYSTEM__
bool CreateSkillColorTableFromRes(MYSQL_RES * res, uint32_t * dwSkillColor)
{
	if (mysql_num_rows(res) == 0)
		return false;

	MYSQL_ROW row = mysql_fetch_row(res);

	for (int x = 0; x < ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT; ++x)
	{
		for (int i = 0; i < ESkillColorLength::MAX_EFFECT_COUNT; ++i)
		{
			*(dwSkillColor++) = atoi(row[i + (x*ESkillColorLength::MAX_EFFECT_COUNT)]);
		}
	}

	return true;
}
#endif


CPlayerTableCache * CClientManager::GetPlayerCache(uint32_t id)
{
	const auto it = m_map_playerCache.find(id);

	if (it == m_map_playerCache.end())
		return nullptr;

	TPlayerTable* pTable = it->second->Get(false);
	pTable->logoff_interval = it->second->GetLastUpdateAgeSeconds();
	return it->second;
}

void CClientManager::PutPlayerCache(TPlayerTable * pNew)
{
	CPlayerTableCache* c = GetPlayerCache(pNew->id);

	if (!c)
	{
		c = new CPlayerTableCache;
		m_map_playerCache.insert(TPlayerTableCacheMap::value_type(pNew->id, c));
	}

	c->Put(pNew);
}

/*
 * PLAYER LOAD
 */


#ifdef __SKILL_COLOR_SYSTEM__
void CClientManager::QUERY_SKILL_COLOR_LOAD(CPeer * peer, uint32_t dwHandle, TPlayerLoadPacket * packet)
{
    auto lease = m_playerSessions.Find(packet->account_id);
    ClientHandleInfo info(dwHandle, packet->player_id, packet->account_id);
    info.account_index = packet->account_index;
    info.playerSession = lease;
    if (!PlayerSessionCurrent(peer, info)) return;
	CSKillColorCache * c = GetSkillColorCache(packet->player_id);
	TSkillColor * p;
    if (c && m_playerSessions.TakePart(*lease, HEADER_DG_SKILL_COLOR_LOAD))
    {
        p = c->Get();
        EncodePlayerLoadHeader(peer, info, HEADER_DG_SKILL_COLOR_LOAD, sizeof(p->dwSkillColor));
		peer->Encode(p->dwSkillColor, sizeof(p->dwSkillColor));
	}
	else
	{
		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery), "SELECT "
			"s1_col1, s1_col2, s1_col3, s1_col4, s1_col5, "
			"s2_col1, s2_col2, s2_col3, s2_col4, s2_col5, "
			"s3_col1, s3_col2, s3_col3, s3_col4, s3_col5, "
			"s4_col1, s4_col2, s4_col3, s4_col4, s4_col5, "
			"s5_col1, s5_col2, s5_col3, s5_col4, s5_col5, "
			"s6_col1, s6_col2, s6_col3, s6_col4, s6_col5, " // end of skills
			"s7_col1, s7_col2, s7_col3, s7_col4, s7_col5, " // begin of buffs
			"s8_col1, s8_col2, s8_col3, s8_col4, s8_col5, "
			"s9_col1, s9_col2, s9_col3, s9_col4, s9_col5, "
			"s10_col1, s10_col2, s10_col3, s10_col4, s10_col5, "
			"s11_col1, s11_col2, s11_col3, s11_col4, s11_col5 "
			"FROM skill_color%s WHERE player_id=%u",
			GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(szQuery, QID_SKILL_COLOR, peer->GetHandle(), new ClientHandleInfo(info));
	}
}
#endif

bool CClientManager::QUERY_PLAYER_LOAD(CPeer *peer, uint32_t dwHandle, TPlayerLoadPacket *packet,
    auriga::player_session::Token link, auriga::player_session::Token request)
{
    namespace session = auriga::player_session;
    auto reject = [&]()
    {
        ClientHandleInfo info(dwHandle, packet->player_id, packet->account_id);
        if (session::Nonzero(link) && session::Nonzero(request))
            info.playerSession = session::Lease{{link, request, session::NewToken(), 1,
                packet->account_id, packet->player_id}, peer->GetHandle(), dwHandle};
        EncodePlayerLoadHeader(peer, info, HEADER_DG_PLAYER_LOAD_FAILED, 0);
        return false;
    };
    auto *login = GetLoginDataByAID(packet->account_id);
    bool loggedOn = false;
    if (login)
    {
        char normalized[LOGIN_MAX_LEN + 1];
        trim_and_lower(login->GetAccountRef().login, normalized, sizeof(normalized));
        const auto it = m_map_kLogonAccount.find(normalized);
        loggedOn = it != m_map_kLogonAccount.end() && it->second == login;
    }
    if (!login || !dwHandle || !AuthorizePlayerLoad(login->GetAccountRef(), *packet,
        {peer->GetHandle(), login->GetConnectedPeerHandle(), peer->IsAuthenticated(), loggedOn,
         login->IsPlay(), login->IsDeleted()})) return reject();
    auto lease = m_playerSessions.Open(peer->GetHandle(), dwHandle, packet->account_id, packet->player_id, link, request);
    if (!lease) return reject();
    auto makeInfo = [&]()
    {
        auto *info = new ClientHandleInfo(dwHandle, packet->player_id, packet->account_id);
        info->account_index = packet->account_index;
        info->playerSession = lease;
        return info;
    };
    const std::unique_ptr<ClientHandleInfo> loadInfo(makeInfo());
	if (CLoginData * pLoginData = GetLoginDataByAID(packet->account_id))
	{
		for (const auto& player : pLoginData->GetAccountRef().players)
			if (player.dwID != 0)
				DeleteLogoutPlayer(player.dwID);
	}

	if (CPlayerTableCache * c; (c = GetPlayerCache(packet->player_id)))
	{
		CLoginData * pkLD = GetLoginDataByAID(packet->account_id);

		if (!pkLD || pkLD->IsPlay())
		{
			LOG_INFO("PLAYER_LOAD_ERROR: LoginData {} IsPlay {}", static_cast<const void*>(pkLD), pkLD ? pkLD->IsPlay() : 0);
			FailPlayerLoad(peer, *loadInfo);
            return false;
		}

		TPlayerTable* pTab = c->Get();
        if (pTab->id != packet->player_id) { FailPlayerLoad(peer, *loadInfo); return false; }

        if (!m_playerSessions.TakePart(*lease, HEADER_DG_PLAYER_LOAD_SUCCESS)) return false;
		pkLD->SetPlay(true);
		memcpy(pTab->aiPremiumTimes, pkLD->GetPremiumPtr(), sizeof(pTab->aiPremiumTimes));

		EncodePlayerLoadHeader(peer, *loadInfo, HEADER_DG_PLAYER_LOAD_SUCCESS, sizeof(TPlayerTable));
		peer->Encode(pTab, sizeof(TPlayerTable));

		if (packet->player_id != pkLD->GetLastPlayerID())
		{
			TPacketNeedLoginLogInfo logInfo;
			logInfo.dwPlayerID = packet->player_id;

			pkLD->SetLastPlayerID( packet->player_id );

			EncodePlayerLoadHeader(peer, *loadInfo, HEADER_DG_NEED_LOGIN_LOG, sizeof(TPacketNeedLoginLogInfo) );
			peer->Encode( &logInfo, sizeof(TPacketNeedLoginLogInfo) );
		}

		char szQuery[QUERY_MAX_LEN] = { 0, };

		TItemCacheSet * pSet = GetItemCacheSet(pTab->id);

		LOG_INFO("[PLAYER_LOAD] ID {} pid {} gold {} ", pTab->name, pTab->id, pTab->gold);

#ifdef ENABLE_GAYA_SYSTEM
		LOG_INFO("[PLAYER_LOAD] ID {} pid {} gaya {} ", pTab->name, pTab->id, pTab->gaya);
#endif

		if (pSet)
		{
			std::vector<TPlayerItem> s_items;
			s_items.resize(pSet->size());

			uint32_t dwCount = 0;
			auto it = pSet->begin();

			while (it != pSet->end())
			{
				CItemCache * c = *it++;

				if (const TPlayerItem * p = c->Get(); p->vnum)
					memcpy(&s_items[dwCount++], p, sizeof(TPlayerItem));
			}

			if (g_test_server)
				LOG_INFO("ITEM_CACHE: HIT! {} count: {}", pTab->name, dwCount);

			m_playerSessions.TakePart(*lease, HEADER_DG_ITEM_LOAD);
            EncodePlayerLoadHeader(peer, *loadInfo, HEADER_DG_ITEM_LOAD, sizeof(uint32_t) + sizeof(TPlayerItem) * dwCount);
			peer->EncodeDWORD(dwCount);

			if (dwCount)
				peer->Encode(s_items.data(), sizeof(TPlayerItem) * dwCount);

			// Quest
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%u AND lValue<>0",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery, QID_QUEST, peer->GetHandle(), makeInfo());

			// Affect
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%u",
					GetTablePostfix(), pTab->id);
			// @fixme402 ClientHandleInfo+pTab->id
			CDBManager::instance().ReturnQuery(szQuery, QID_AFFECT, peer->GetHandle(), makeInfo());
		}

		else
		{
			snprintf(szQuery, sizeof(szQuery),
					"SELECT id,window+0,pos,count,vnum,"
#ifdef ATTR_LOCK
					"lockedattr,"
#endif
					"socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6,attrtype7,attrvalue7 "
					"FROM item%s WHERE owner_id=%u AND (window in ('INVENTORY','EQUIPMENT','DRAGON_SOUL_INVENTORY','BELT_INVENTORY'"
#ifdef ENABLE_EXTRA_INVENTORY
					",'EXTRA_INVENTORY'"
#endif
#ifdef ENABLE_SWITCHBOT
					",'SWITCHBOT'"
#endif		
					"))",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_ITEM,
					peer->GetHandle(),
					makeInfo());
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID, szName, szState, lValue FROM quest%s WHERE dwPID=%u",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_QUEST,
					peer->GetHandle(),
					makeInfo());
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost FROM affect%s WHERE dwPID=%u",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_AFFECT,
					peer->GetHandle(),
					makeInfo());
		}
		//ljw
		//return;
	}
	else
	{
		LOG_INFO("[PLAYER_LOAD] Load from PlayerDB pid[{}]", packet->player_id);

		char queryStr[QUERY_MAX_LEN];

		snprintf(queryStr, sizeof(queryStr),
				"SELECT "
				"id,name,job,voice,dir,x,y,z,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,playtime,"
				"gold,"
#ifdef ENABLE_GAYA_SYSTEM
				"gaya,"
#endif			
				"level,level_step,st,ht,dx,iq,"
				"exp,"
				"stat_point,skill_point,sub_skill_point,stat_reset_count,part_base,part_hair,"
#ifdef ENABLE_ACCE_SYSTEM 
				"part_acce, "
#endif
				"skill_level,quickslot,skill_group,alignment,mobile,horse_level,horse_riding,horse_hp,horse_hp_droptime,horse_stamina,"
				"UNIX_TIMESTAMP(NOW())-UNIX_TIMESTAMP(last_play),horse_skill_point "
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
				",envanter "
#endif

#ifdef ENABLE_RANKING
				", r0, "
				"r1, "
				"r2, "
				"r3, "
				"r4, "
				"r5, "
				"r6, "
				"r7, "
				"r8, "
				"r9, "
				"r10, "
				"r11, "
				"r12, "
				"r13, "
				"r14, "
				"r15, "
				"r16, "
				"r17, "
				"r18, "
				"r19 "
#endif
				"FROM player%s WHERE id=%u AND account_id=%u",
                GetTablePostfix(), packet->player_id, packet->account_id);

		auto pkInfo = makeInfo();
		CDBManager::instance().ReturnQuery(queryStr, QID_PLAYER, peer->GetHandle(), pkInfo);

		snprintf(queryStr, sizeof(queryStr),
				"SELECT id,window+0,pos,count,vnum,"
#ifdef ATTR_LOCK
				"lockedattr,"
#endif
				"socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6,attrtype7,attrvalue7 "
				"FROM item%s WHERE owner_id=%u AND (window in ('INVENTORY','EQUIPMENT','DRAGON_SOUL_INVENTORY','BELT_INVENTORY'"
#ifdef ENABLE_EXTRA_INVENTORY
				",'EXTRA_INVENTORY'"
#endif
#ifdef ENABLE_SWITCHBOT
				",'SWITCHBOT'"
#endif
				"))",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_ITEM, peer->GetHandle(), makeInfo());


		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%u",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_QUEST, peer->GetHandle(), makeInfo());

		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%u",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_AFFECT, peer->GetHandle(), makeInfo());
	}



#ifdef __ENABLE_NEW_OFFLINESHOP__
	OfflineshopLoadShopSafebox(peer, packet->player_id);
#endif
    return true;
}

void CClientManager::ItemAward(CPeer * peer,char* login)
{
	char login_t[LOGIN_MAX_LEN + 1] = "";
	strlcpy(login_t,login,LOGIN_MAX_LEN + 1);
	auto* pSet = ItemAwardManager::instance().GetByLogin(login_t);
	if(pSet == nullptr)
		return;
	auto it = pSet->begin();
	while(it != pSet->end() )
	{
		TItemAward * pItemAward = *it++;
        if (pItemAward->bTaken || pItemAward->bGiftDismissed)
            continue;
		char* whyStr = pItemAward->szWhy;
		char cmdStr[100] = "";
		strcpy(cmdStr,whyStr);
		char command[20] = "";
		
		GetCommand(cmdStr, command);
		if( !(strcmp(command,"GIFT") ))
		{
			TPacketItemAwardInfromer giftData;
			strcpy(giftData.login, pItemAward->szLogin);
			strcpy(giftData.command, command);
			giftData.vnum = pItemAward->dwVnum;
			ForwardPacket(HEADER_DG_ITEMAWARD_INFORMER,&giftData,sizeof(TPacketItemAwardInfromer));
		}
	}
}
char* CClientManager::GetCommand(char* str, char* command) // @fixme203
{
	char* tok;

	if( str[0] == '[' )
	{
		tok = strtok(str,"]");
		strcat(command,&tok[1]);
	}

	return command;
}

bool CreatePlayerTableFromRes(MYSQL_RES * res, TPlayerTable * pkTab)
{
	if (mysql_num_rows(res) == 0)
		return false;

	memset(pkTab, 0, sizeof(TPlayerTable));

	MYSQL_ROW row = mysql_fetch_row(res);

	int	col = 0;

	// "id,name,job,voice,dir,x,y,z,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,playtime,"
	// "gold,level,level_step,st,ht,dx,iq,exp,"
	// "stat_point,skill_point,sub_skill_point,stat_reset_count,part_base,part_hair,"
	// "skill_level,quickslot,skill_group,alignment,mobile,horse_level,horse_riding,horse_hp,horse_stamina FROM player%s WHERE id=%d",
	str_to_number(pkTab->id, row[col++]);
	strlcpy(pkTab->name, row[col++], sizeof(pkTab->name));
	str_to_number(pkTab->job, row[col++]);
	str_to_number(pkTab->voice, row[col++]);
	str_to_number(pkTab->dir, row[col++]);
	str_to_number(pkTab->x, row[col++]);
	str_to_number(pkTab->y, row[col++]);
	str_to_number(pkTab->z, row[col++]);
	str_to_number(pkTab->lMapIndex, row[col++]);
	str_to_number(pkTab->lExitX, row[col++]);
	str_to_number(pkTab->lExitY, row[col++]);
	str_to_number(pkTab->lExitMapIndex,  row[col++]);
	str_to_number(pkTab->hp, row[col++]);
	str_to_number(pkTab->sp, row[col++]);
	str_to_number(pkTab->stamina, row[col++]);
	str_to_number(pkTab->sRandomHP, row[col++]);
	str_to_number(pkTab->sRandomSP, row[col++]);
	str_to_number(pkTab->playtime, row[col++]);
	str_to_number(pkTab->gold, row[col++]);
#ifdef ENABLE_GAYA_SYSTEM
	str_to_number(pkTab->gaya, row[col++]);
#endif
	str_to_number(pkTab->level, row[col++]);
	str_to_number(pkTab->level_step, row[col++]);
	str_to_number(pkTab->st, row[col++]);
	str_to_number(pkTab->ht, row[col++]);
	str_to_number(pkTab->dx, row[col++]);
	str_to_number(pkTab->iq, row[col++]);
	str_to_number(pkTab->exp, row[col++]);
	str_to_number(pkTab->stat_point, row[col++]);
	str_to_number(pkTab->skill_point, row[col++]);
	str_to_number(pkTab->sub_skill_point, row[col++]);
	str_to_number(pkTab->stat_reset_count, row[col++]);
	str_to_number(pkTab->part_base, row[col++]);
	str_to_number(pkTab->parts[PART_HAIR], row[col++]);
#ifdef ENABLE_ACCE_SYSTEM
	str_to_number(pkTab->parts[PART_ACCE], row[col++]);
#endif


	if (row[col])
		memcpy(pkTab->skills, row[col], sizeof(pkTab->skills));
	else
		memset(&pkTab->skills, 0, sizeof(pkTab->skills));

	col++;

	if (row[col])
		memcpy(pkTab->quickslot, row[col], sizeof(pkTab->quickslot));
	else
		memset(pkTab->quickslot, 0, sizeof(pkTab->quickslot));

	col++;

	str_to_number(pkTab->skill_group, row[col++]);
	str_to_number(pkTab->lAlignment, row[col++]);

	if (row[col])
	{
		strlcpy(pkTab->szMobile, row[col], sizeof(pkTab->szMobile));
	}

	col++;

	str_to_number(pkTab->horse.bLevel, row[col++]);
	str_to_number(pkTab->horse.bRiding, row[col++]);
	str_to_number(pkTab->horse.sHealth, row[col++]);
	str_to_number(pkTab->horse.dwHorseHealthDropTime, row[col++]);
	str_to_number(pkTab->horse.sStamina, row[col++]);
	str_to_number(pkTab->logoff_interval, row[col++]);
	str_to_number(pkTab->horse_skill_point, row[col++]);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	str_to_number(pkTab->envanter, row[col++]);
#endif

#ifdef ENABLE_RANKING
	for (long long& lRankPoint : pkTab->lRankPoints)
		str_to_number(lRankPoint, row[col++]);
#endif

	NormalizeSecondarySkills(*pkTab);

	return true;
}

bool CClientManager::PlayerSessionCurrent(CPeer *peer, const ClientHandleInfo &info)
{
    if (!peer || !peer->IsAuthenticated() || !info.playerSession || !m_playerSessions.Matches(*info.playerSession) ||
        info.playerSession->peer != peer->GetHandle() || info.playerSession->descriptor != info.dwHandle) return false;
    auto *login = GetLoginDataByAID(info.account_id);
    if (!login || login->IsDeleted() || login->GetConnectedPeerHandle() != peer->GetHandle() ||
        info.account_index >= PLAYER_PER_ACCOUNT || login->GetAccountRef().players[info.account_index].dwID != info.player_id) return false;
    char normalized[LOGIN_MAX_LEN + 1];
    trim_and_lower(login->GetAccountRef().login, normalized, sizeof(normalized));
    const auto it = m_map_kLogonAccount.find(normalized);
    return it != m_map_kLogonAccount.end() && it->second == login &&
        info.playerSession->key.account == info.account_id && info.playerSession->key.character == info.player_id;
}
void CClientManager::EncodePlayerLoadHeader(CPeer *peer, const ClientHandleInfo &info, uint8_t header, uint32_t size)
{
    if (info.playerSession && info.playerSession->key.Versioned())
    {
        const auto prefix = auriga::player_session::ReplyPrefix(info.playerSession->key, header, size);
        peer->EncodeHeader(HEADER_DG_PLAYER_SESSION_REPLY, info.dwHandle, uint32_t(prefix.size()) + size);
        peer->Encode(prefix.data(), prefix.size());
    }
    else peer->EncodeHeader(header, info.dwHandle, size);
}
void CClientManager::FailPlayerLoad(CPeer *peer, const ClientHandleInfo &info)
{
    if (!PlayerSessionCurrent(peer, info)) return;
    EncodePlayerLoadHeader(peer, info, HEADER_DG_PLAYER_LOAD_FAILED, 0);
    m_playerSessions.Invalidate(*info.playerSession);
}

void CClientManager::RESULT_COMPOSITE_PLAYER(CPeer * peer, SQLMsg * pMsg, uint32_t dwQID)
{
	auto qi = (CQueryInfo *) pMsg->pvUserData;
	const std::unique_ptr<ClientHandleInfo> info((ClientHandleInfo *) qi->pvData);

	if (!info || !PlayerSessionCurrent(peer, *info)) return;
    uint8_t part = 0;
    switch (dwQID)
    {
        case QID_PLAYER: part = HEADER_DG_PLAYER_LOAD_SUCCESS; break;
        case QID_ITEM: part = HEADER_DG_ITEM_LOAD; break;
        case QID_QUEST: part = HEADER_DG_QUEST_LOAD; break;
        case QID_AFFECT: part = HEADER_DG_AFFECT_LOAD; break;
#ifdef __SKILL_COLOR_SYSTEM__
        case QID_SKILL_COLOR: part = HEADER_DG_SKILL_COLOR_LOAD; break;
#endif
        default: return;
    }
    // Reject repeated callbacks before SQL-error handling or any cache/logon side effect.
    if (!m_playerSessions.TakePart(*info->playerSession, part)) return;
    MYSQL_RES *pSQLResult = pMsg->Get() ? pMsg->Get()->pSQLResult : nullptr;
	if (pMsg->uiSQLErrno || !pSQLResult)
	{
		LOG_ERROR("Player load SQL failed QID {} errno {}", dwQID, pMsg->uiSQLErrno);
        FailPlayerLoad(peer, *info);
		return;
	}

	switch (dwQID)
	{
		case QID_PLAYER:
			LOG_INFO("QID_PLAYER {} {}", info->dwHandle, info->player_id);
			RESULT_PLAYER_LOAD(peer, pSQLResult, info.get());

			break;

		case QID_ITEM:
			LOG_INFO("QID_ITEM {}", info->dwHandle);
			RESULT_ITEM_LOAD(peer, pSQLResult, *info);
			break;


#ifdef __SKILL_COLOR_SYSTEM__
		case QID_SKILL_COLOR:
			LOG_INFO("QID_SKILL_COLOR {} {}", info->dwHandle, info->player_id);
			RESULT_SKILL_COLOR_LOAD(peer, pSQLResult, *info);
			break;
#endif

		case QID_QUEST:
			{
				LOG_INFO("QID_QUEST {}", info->dwHandle);
				RESULT_QUEST_LOAD(peer, pSQLResult, *info);
				ClientHandleInfo*  temp1 = info.get();
				if (temp1 == nullptr)
					break;

				CLoginData* pLoginData1 = GetLoginDataByAID(temp1->account_id);	//
				if( pLoginData1 == nullptr)
					break;

				if( pLoginData1->GetAccountRef().login == nullptr)
					break;

				LOG_INFO("info of pLoginData1 before call ItemAwardfunction {}", static_cast<const void*>(pLoginData1));
				ItemAward(peer,pLoginData1->GetAccountRef().login);
			}
			break;

		case QID_AFFECT:
			LOG_INFO("QID_AFFECT {}", info->dwHandle);
			// @fixme402 RESULT_AFFECT_LOAD+info->player_id
			RESULT_AFFECT_LOAD(peer, pSQLResult, *info);
			break;
			/*
			   case QID_PLAYER_ITEM_QUEST_AFFECT:
			   LOG_INFO("QID_PLAYER_ITEM_QUEST_AFFECT {}", info->dwHandle);
			   RESULT_PLAYER_LOAD(peer, pSQLResult, info->dwHandle);

			   if (!pMsg->Next())
			   {
			   LOG_ERROR("RESULT_COMPOSITE_PLAYER: QID_PLAYER_ITEM_QUEST_AFFECT: ITEM FAILED");
			   return;
			   }

			   case QID_ITEM_QUEST_AFFECT:
			   LOG_INFO("QID_ITEM_QUEST_AFFECT {}", info->dwHandle);
			   RESULT_ITEM_LOAD(peer, pSQLResult, *info);

			   if (!pMsg->Next())
			   {
			   LOG_ERROR("RESULT_COMPOSITE_PLAYER: QID_PLAYER_ITEM_QUEST_AFFECT: QUEST FAILED");
			   return;
			   }

			   case QID_QUEST_AFFECT:
			   LOG_INFO("QID_QUEST_AFFECT {}", info->dwHandle);
			   RESULT_QUEST_LOAD(peer, pSQLResult, info->dwHandle);

			   if (!pMsg->Next())
			   LOG_ERROR("RESULT_COMPOSITE_PLAYER: QID_PLAYER_ITEM_QUEST_AFFECT: AFFECT FAILED");
			   else
			   RESULT_AFFECT_LOAD(peer, pSQLResult, info->dwHandle);

			   break;
			   */

	}

}


void CClientManager::RESULT_PLAYER_LOAD(CPeer * peer, MYSQL_RES * pRes, ClientHandleInfo * pkInfo)
{
	TPlayerTable tab;

	if (!CreatePlayerTableFromRes(pRes, &tab))
	{
		FailPlayerLoad(peer, *pkInfo);
		return;
	}

	CLoginData * pkLD = GetLoginDataByAID(pkInfo->account_id);

	if (!pkLD || !PlayerSessionCurrent(peer, *pkInfo) || pkLD->IsPlay() || tab.id != pkInfo->player_id)
	{
		LOG_INFO("PLAYER_LOAD_ERROR: LoginData {} IsPlay {}", static_cast<const void*>(pkLD), pkLD ? pkLD->IsPlay() : 0);
		FailPlayerLoad(peer, *pkInfo);
		return;
	}

	pkLD->SetPlay(true);
	memcpy(tab.aiPremiumTimes, pkLD->GetPremiumPtr(), sizeof(tab.aiPremiumTimes));

	EncodePlayerLoadHeader(peer, *pkInfo, HEADER_DG_PLAYER_LOAD_SUCCESS, sizeof(TPlayerTable));
	peer->Encode(&tab, sizeof(TPlayerTable));

	if (tab.id != pkLD->GetLastPlayerID())
	{
		TPacketNeedLoginLogInfo logInfo;
		logInfo.dwPlayerID = tab.id;

		pkLD->SetLastPlayerID( tab.id );

		EncodePlayerLoadHeader(peer, *pkInfo, HEADER_DG_NEED_LOGIN_LOG, sizeof(TPacketNeedLoginLogInfo) );
		peer->Encode( &logInfo, sizeof(TPacketNeedLoginLogInfo) );
	}
}

void CClientManager::RESULT_ITEM_LOAD(CPeer *peer, MYSQL_RES *pRes, const ClientHandleInfo &info)
{
    const uint32_t dwPID = info.player_id;
	std::vector<TPlayerItem> s_items;
	CreateItemTableFromRes(pRes, &s_items, dwPID);
	uint32_t dwCount = (uint32_t)s_items.size();

	EncodePlayerLoadHeader(peer, info, HEADER_DG_ITEM_LOAD, sizeof(uint32_t) + sizeof(TPlayerItem) * dwCount);
	peer->EncodeDWORD(dwCount);

	CreateItemCacheSet(dwPID);

	// ITEM_LOAD_LOG_ATTACH_PID
	LOG_INFO("ITEM_LOAD: count {} pid {}", dwCount, dwPID);
	// END_OF_ITEM_LOAD_LOG_ATTACH_PID

	if (dwCount)
	{
		peer->Encode(s_items.data(), sizeof(TPlayerItem) * dwCount);

		for (uint32_t i = 0; i < dwCount; ++i)
			PutItemCache(&s_items[i], true);
	}
}

#ifdef __SKILL_COLOR_SYSTEM__
void CClientManager::RESULT_SKILL_COLOR_LOAD(CPeer *peer, MYSQL_RES *pRes, const ClientHandleInfo &info)
{

	uint32_t dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
	memset(dwSkillColor, 0, sizeof(dwSkillColor));

	CreateSkillColorTableFromRes(pRes, *dwSkillColor);
	LOG_INFO("SKILL_COLOR_LOAD {}, {}, {}, {},", dwSkillColor[0][0], dwSkillColor[1][1], dwSkillColor[2][2], dwSkillColor[3][3]);
	EncodePlayerLoadHeader(peer, info, HEADER_DG_SKILL_COLOR_LOAD, sizeof(dwSkillColor));
	peer->Encode(&dwSkillColor, sizeof(dwSkillColor));
}
#endif

// @fixme402 (RESULT_AFFECT_LOAD +dwRealPID)
void CClientManager::RESULT_AFFECT_LOAD(CPeer *peer, MYSQL_RES *pRes, const ClientHandleInfo &info)
{
    const uint32_t dwRealPID = info.player_id;
	uint64_t iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0)
	{
		// @fixme402 begin
		//static uint32_t dwPID;
		uint32_t dwCount = 0; //1;
		//static TPacketAffectElement paeTable = {};

		//dwPID = dwRealPID;
		LOG_INFO("AFFECT_LOAD: count {}  RealPID {}", dwCount, dwRealPID);

		EncodePlayerLoadHeader(peer, info, HEADER_DG_AFFECT_LOAD, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPacketAffectElement) * dwCount);
		peer->Encode(&dwRealPID, sizeof(uint32_t));
		peer->Encode(&dwCount, sizeof(uint32_t));
		//peer->Encode(&paeTable, sizeof(TPacketAffectElement) * dwCount);
		// @fixme402 end
		return;
	}

	std::vector<TPacketAffectElement> s_elements;
	s_elements.resize(iNumRows);

	uint32_t dwPID = 0;

	for (uint64_t i = 0; std::cmp_less(i, iNumRows); ++i)
	{
		TPacketAffectElement & r = s_elements[i];
		MYSQL_ROW row = mysql_fetch_row(pRes);

		if (dwPID == 0)
			str_to_number(dwPID, row[0]);

		str_to_number(r.dwType, row[1]);
		str_to_number(r.bApplyOn, row[2]);
		str_to_number(r.lApplyValue, row[3]);
		str_to_number(r.dwFlag, row[4]);
		str_to_number(r.lDuration, row[5]);
		str_to_number(r.lSPCost, row[6]);
	}

	LOG_INFO("AFFECT_LOAD: count {} PID {}", s_elements.size(), dwPID);

	uint32_t dwCount = (uint32_t)s_elements.size();

	EncodePlayerLoadHeader(peer, info, HEADER_DG_AFFECT_LOAD, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPacketAffectElement) * dwCount);
	peer->Encode(&dwPID, sizeof(uint32_t));
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(s_elements.data(), sizeof(TPacketAffectElement) * dwCount);
}

void CClientManager::RESULT_QUEST_LOAD(CPeer *peer, MYSQL_RES *pRes, const ClientHandleInfo &info)
{
    const uint32_t pid = info.player_id;
	uint64_t i_num_rows;

	if ((i_num_rows = mysql_num_rows(pRes)) == 0)
	{
		uint32_t dwCount = 0;
		EncodePlayerLoadHeader(peer, info, HEADER_DG_QUEST_LOAD, sizeof(uint32_t));
		peer->Encode(&dwCount, sizeof(uint32_t));
		return;
	}

	std::vector<TQuestTable> s_table;
	s_table.resize(i_num_rows);

	for (int i = 0; std::cmp_less(i, i_num_rows); ++i)
	{
		TQuestTable & r = s_table[i];

		MYSQL_ROW row = mysql_fetch_row(pRes);

		str_to_number(r.dwPID, row[0]);
		strlcpy(r.szName, row[1], sizeof(r.szName));
		strlcpy(r.szState, row[2], sizeof(r.szState));
		str_to_number(r.lValue, row[3]);
	}

	LOG_INFO("QUEST_LOAD: count {} PID {}", s_table.size(), s_table[0].dwPID);

	uint32_t dwCount = (uint32_t)s_table.size();

	EncodePlayerLoadHeader(peer, info, HEADER_DG_QUEST_LOAD, sizeof(uint32_t) + sizeof(TQuestTable) * dwCount);
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(s_table.data(), sizeof(TQuestTable) * dwCount);
}


/*
 * PLAYER SAVE
 */
void CClientManager::QUERY_PLAYER_SAVE(CPeer * peer, uint32_t dwHandle, TPlayerTable * pkTab)
{
	if (g_test_server)
		LOG_INFO("PLAYER_SAVE: {}", pkTab->name);

	PutPlayerCache(pkTab);
}

typedef std::map<uint32_t, uint32_t> time_by_id_map_t;
time_by_id_map_t s_createTimeByAccountID;

/*
 * PLAYER CREATE
 */
void CClientManager::__QUERY_PLAYER_CREATE(CPeer *peer, uint32_t dwHandle, TPlayerCreatePacket* packet)
{
	if (packet->account_index >= PLAYER_PER_ACCOUNT ||
		!packet_string::IsTerminated(packet->player_table.name))
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}
	char	queryStr[QUERY_MAX_LEN];
	uint32_t		player_id;


	if (auto it = s_createTimeByAccountID.find(packet->account_id); it != s_createTimeByAccountID.end())
	{
		uint32_t curtime = time(nullptr);

		if (curtime - it->second < 30)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}
	}

	snprintf(queryStr, sizeof(queryStr),
	                        "SELECT pid%u FROM player_index%s WHERE id=?", packet->account_index + 1,
	                        GetTablePostfix());

	std::unique_ptr<SQLMsg> pMsg0(CDBManager::instance().DirectQueryPrepared(queryStr,
		{ SqlParam::UInt(packet->account_id) }));

	if (pMsg0->Get()->uiNumRows != 0)
	{
		if (!pMsg0->Get()->pSQLResult && !pMsg0->Get()->bPrepared)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = pMsg0->Get()->FetchRow();

		uint32_t dwPID = 0; str_to_number(dwPID, row[0]);
		if (row[0] && dwPID > 0)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			LOG_INFO("ALREADY EXIST AccountChrIdx {} ID {}", packet->account_index, dwPID);
			return;
		}
	}
	else
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	snprintf(queryStr, sizeof(queryStr),
			"SELECT COUNT(*) as count FROM player%s WHERE name=?", GetTablePostfix());

	std::unique_ptr<SQLMsg> pMsg1(CDBManager::instance().DirectQueryPrepared(queryStr,
			{ SqlParam::Text(packet->player_table.name) }));

	if (pMsg1->Get()->uiNumRows)
	{
		if (!pMsg1->Get()->pSQLResult && !pMsg1->Get()->bPrepared)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = pMsg1->Get()->FetchRow();

		if (*row[0] != '0')
		{
			LOG_INFO("ALREADY EXIST name {}, row[0] {} query {}", packet->player_table.name, row[0], queryStr);
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			return;
		}
	}
	else
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	const auto create = BuildPlayerCreateQuery(GetTablePostfix(), packet->account_id, packet->player_table);
	LOG_INFO("PlayerCreate accountid {} name {} level {} gold {}, st {} ht {} job {}", packet->account_id, packet->player_table.name, packet->player_table.level, packet->player_table.gold, packet->player_table.st, packet->player_table.ht, packet->player_table.job);
	std::unique_ptr<SQLMsg> pMsg2(CDBManager::instance().DirectQueryPrepared(create.sql.c_str(), create.params));
	if (pMsg2->Get()->uiAffectedRows <= 0)
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
		LOG_INFO("ALREADY EXIST3 query: {} AffectedRows {}", create.sql, pMsg2->Get()->uiAffectedRows);
		return;
	}

	player_id = static_cast<uint32_t>(pMsg2->Get()->uiInsertID);

	snprintf(queryStr, sizeof(queryStr), "UPDATE player_index%s SET pid%u=%u WHERE id=%u",
			GetTablePostfix(), packet->account_index + 1, player_id, packet->account_id);
	std::unique_ptr<SQLMsg> pMsg3(CDBManager::instance().DirectQuery(queryStr));

	if (pMsg3->Get()->uiAffectedRows <= 0)
	{
		LOG_ERROR("QUERY_ERROR: {}", queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM player%s WHERE id=%u", GetTablePostfix(), player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	TPacketDGCreateSuccess pack = {};

	pack.bAccountCharacterIndex = packet->account_index;

	pack.player.dwID = player_id;
	strlcpy(pack.player.szName, packet->player_table.name, sizeof(pack.player.szName));
	pack.player.byJob = packet->player_table.job;
	pack.player.byLevel = packet->player_table.level;
	pack.player.dwPlayMinutes = 0;
	pack.player.byST = packet->player_table.st;
	pack.player.byHT = packet->player_table.ht;
	pack.player.byDX = packet->player_table.dx;
	pack.player.byIQ = packet->player_table.iq;
	pack.player.wMainPart = packet->player_table.parts[PART_MAIN] ? packet->player_table.parts[PART_MAIN] : packet->player_table.part_base;
	pack.player.wHairPart = packet->player_table.parts[PART_HAIR];
	pack.player.skill_group = packet->player_table.skill_group;
	pack.player.x = packet->player_table.x;
	pack.player.y = packet->player_table.y;

	peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_SUCCESS, dwHandle, sizeof(TPacketDGCreateSuccess));
	peer->Encode(&pack, sizeof(TPacketDGCreateSuccess));

	LOG_INFO("7 name {} job {}", pack.player.szName, pack.player.byJob);

	s_createTimeByAccountID[packet->account_id] = time(nullptr);
}

/*
 * PLAYER DELETE
 */
void CClientManager::__QUERY_PLAYER_DELETE(CPeer* peer, uint32_t dwHandle, TPlayerDeletePacket* packet)
{
	if (!packet->login[0] || !packet->player_id || packet->account_index >= PLAYER_PER_ACCOUNT)
		return;

	CLoginData * ld = GetLoginDataByLogin(packet->login);

	if (!ld)
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
		peer->EncodeBYTE(packet->account_index);
		return;
	}

	TAccountTable & r = ld->GetAccountRef();

	// block for japan
	if (g_stLocale != "sjis")
	{
		if (strlen(r.social_id) < 7 || strncmp(packet->private_code, r.social_id + strlen(r.social_id) - 7, 7))
		{
			LOG_INFO("PLAYER_DELETE FAILED len({})", strlen(r.social_id));
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
			peer->EncodeBYTE(packet->account_index);
			return;
		}

		// CPlayerTableCache * pkPlayerCache = GetPlayerCache(packet->player_id);
		// if (pkPlayerCache)
		// {
			// TPlayerTable * pTab = pkPlayerCache->Get();

			// if (pTab->level >= m_iPlayerDeleteLevelLimit)
			// {
				// LOG_INFO("PLAYER_DELETE FAILED LEVEL {} >= DELETE LIMIT {}", pTab->level, m_iPlayerDeleteLevelLimit);
				// peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
				// peer->EncodeBYTE(packet->account_index);
				// return;
			// }

			// if (pTab->level < m_iPlayerDeleteLevelLimitLower)
			// {
				// LOG_INFO("PLAYER_DELETE FAILED LEVEL {} < DELETE LIMIT {}", pTab->level, m_iPlayerDeleteLevelLimitLower);
				// peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
				// peer->EncodeBYTE(packet->account_index);
				// return;
			// }
		// }
	}

#ifdef __ENABLE_NEW_OFFLINESHOP__
	if (IsUsingOfflineshopSystem(packet->player_id)) {
		LOG_INFO("PLAYER_DELETE FAILED {} IS USING OFFLINESHOP SYSTEM", packet->player_id);
		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
		peer->EncodeBYTE(packet->account_index);
		return;
	}
#endif

	char szQuery[128];
	snprintf(szQuery, sizeof(szQuery), "SELECT p.id, p.level, p.name FROM player_index%s AS i, player%s AS p WHERE pid%u=%u AND pid%u=p.id",
			GetTablePostfix(), GetTablePostfix(), packet->account_index + 1, packet->player_id, packet->account_index + 1);

	ClientHandleInfo * pi = new ClientHandleInfo(dwHandle, packet->player_id);
	pi->account_index = packet->account_index;

	LOG_INFO("PLAYER_DELETE TRY: {} {} pid{}", packet->login, packet->player_id, packet->account_index + 1);
	CDBManager::instance().ReturnQuery(szQuery, QID_PLAYER_DELETE, peer->GetHandle(), pi);
}

//
//
void CClientManager::__RESULT_PLAYER_DELETE(CPeer *peer, SQLMsg* msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * pi = (ClientHandleInfo *) qi->pvData;

	if (msg->Get() && msg->Get()->uiNumRows)
	{
		MYSQL_ROW row = msg->Get()->FetchRow();

		uint32_t dwPID = 0;
		str_to_number(dwPID, row[0]);

		//int deletedLevelLimit = 0;
		//str_to_number(deletedLevelLimit, row[1]);

		char szName[64];
		strlcpy(szName, row[2], sizeof(szName));

		// if (deletedLevelLimit >= m_iPlayerDeleteLevelLimit)
		// {
			// LOG_INFO("PLAYER_DELETE FAILED LEVEL {} >= DELETE LIMIT {}", deletedLevelLimit, m_iPlayerDeleteLevelLimit);
			// peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			// peer->EncodeBYTE(pi->account_index);
			// return;
		// }

		// if (deletedLevelLimit < m_iPlayerDeleteLevelLimitLower)
		// {
			// LOG_INFO("PLAYER_DELETE FAILED LEVEL {} < DELETE LIMIT {}", deletedLevelLimit, m_iPlayerDeleteLevelLimitLower);
			// peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			// peer->EncodeBYTE(pi->account_index);
			// return;
		// }

		char queryStr[QUERY_MAX_LEN];

		snprintf(queryStr, sizeof(queryStr), "INSERT INTO player_deleted%s SELECT * FROM player%s WHERE id=%u",
				GetTablePostfix(), GetTablePostfix(), pi->player_id);
		std::unique_ptr<SQLMsg> pIns(CDBManager::instance().DirectQuery(queryStr));

		if (pIns->Get()->uiAffectedRows == 0 || pIns->Get()->uiAffectedRows == (uint32_t)-1)
		{
			LOG_INFO("PLAYER_DELETE FAILED {} CANNOT INSERT TO player_deleted{}", dwPID, GetTablePostfix());

			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			peer->EncodeBYTE(pi->account_index);
			return;
		}

		LOG_INFO("PLAYER_DELETE SUCCESS {}", dwPID);

		char account_index_string[16];

		snprintf(account_index_string, sizeof(account_index_string), "player_id%d", m_iPlayerIDStart + pi->account_index);

		CPlayerTableCache * pkPlayerCache = GetPlayerCache(pi->player_id);

		if (pkPlayerCache)
		{
			m_map_playerCache.erase(pi->player_id);
			delete pkPlayerCache;
		}

		TItemCacheSet * pSet = GetItemCacheSet(pi->player_id);

		if (pSet)
		{
			TItemCacheSet::iterator it = pSet->begin();

			while (it != pSet->end())
			{
				CItemCache * pkItemCache = *it++;
				DeleteItemCache(pkItemCache->Get()->id);
			}

			pSet->clear();
			delete pSet;

			m_map_pkItemCacheSetPtr.erase(pi->player_id);
		}

		snprintf(queryStr, sizeof(queryStr), "UPDATE player_index%s SET pid%u=0 WHERE pid%u=%u",
				GetTablePostfix(),
				pi->account_index + 1,
				pi->account_index + 1,
				pi->player_id);

		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(queryStr));

		if (pMsg->Get()->uiAffectedRows == 0 || std::cmp_equal(pMsg->Get()->uiAffectedRows, -1))
		{
			LOG_INFO("PLAYER_DELETE FAIL WHEN UPDATE account table");
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			peer->EncodeBYTE(pi->account_index);
			return;
		}

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM player%s WHERE id=%d", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM item%s WHERE owner_id=%u AND (window in ('INVENTORY','EQUIPMENT','DRAGON_SOUL_INVENTORY','BELT_INVENTORY'"
#ifdef ENABLE_EXTRA_INVENTORY
		",'EXTRA_INVENTORY'"
#endif
#ifdef ENABLE_SWITCHBOT
		",'SWITCHBOT'"
#endif
		"))",
		GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM quest%s WHERE dwPID=%u", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM affect%s WHERE dwPID=%u", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM guild_member%s WHERE pid=%u", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		// MYSHOP_PRICE_LIST
		snprintf(queryStr, sizeof(queryStr), "DELETE FROM myshop_pricelist%s WHERE owner_id=%u", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);
		// END_OF_MYSHOP_PRICE_LIST

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM messenger_list%s WHERE account=? OR companion=?", GetTablePostfix());
		CDBManager::instance().QueryPrepared(queryStr, { SqlParam::Text(szName), SqlParam::Text(szName) });

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM horse_name%s WHERE id=%u", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM guild_comment%s WHERE name=?", GetTablePostfix());
		CDBManager::instance().QueryPrepared(queryStr, { SqlParam::Text(szName) });

		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_SUCCESS, pi->dwHandle, 1);
		peer->EncodeBYTE(pi->account_index);
	}
	else
	{
		LOG_INFO("PLAYER_DELETE FAIL NO ROW");
		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
		peer->EncodeBYTE(pi->account_index);
	}
}

void CClientManager::QUERY_ADD_AFFECT(CPeer * peer, TPacketGDAddAffect * p)
{
	char queryStr[QUERY_MAX_LEN];
	/*
	   snprintf(queryStr, sizeof(queryStr),
	   "INSERT INTO affect%s (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) "
	   "VALUES(%u, %u, %u, %d, %u, %d, %d) "
	   "ON DUPLICATE KEY UPDATE lApplyValue=%d, dwFlag=%u, lDuration=%d, lSPCost=%d",
	   GetTablePostfix(),
	   p->dwPID,
	   p->elem.dwType,
	   p->elem.bApplyOn,
	   p->elem.lApplyValue,
	   p->elem.dwFlag,
	   p->elem.lDuration,
	   p->elem.lSPCost,
	   p->elem.lApplyValue,
	   p->elem.dwFlag,
	   p->elem.lDuration,
	   p->elem.lSPCost);
	   */
	snprintf(queryStr, sizeof(queryStr),
			"REPLACE INTO affect%s (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) "
			"VALUES(%u, %u, %u, %d, %u, %d, %d)",
			GetTablePostfix(),
			p->dwPID,
			p->elem.dwType,
			p->elem.bApplyOn,
			p->elem.lApplyValue,
			p->elem.dwFlag,
			p->elem.lDuration,
			p->elem.lSPCost);

	CDBManager::instance().AsyncQuery(queryStr);
}

void CClientManager::QUERY_REMOVE_AFFECT(CPeer * peer, TPacketGDRemoveAffect * p)
{
	char queryStr[QUERY_MAX_LEN];

	snprintf(queryStr, sizeof(queryStr),
			"DELETE FROM affect%s WHERE dwPID=%u AND bType=%u AND bApplyOn=%u",
			GetTablePostfix(), p->dwPID, p->dwType, p->bApplyOn);

	CDBManager::instance().AsyncQuery(queryStr);
}


void CClientManager::QUERY_HIGHSCORE_REGISTER(CPeer* peer, TPacketGDHighscore * data)
{
	char szQuery[128];
	snprintf(szQuery, sizeof(szQuery), "SELECT value FROM highscore%s WHERE board=? AND pid = ?", GetTablePostfix());

	LOG_INFO("HEADER_GD_HIGHSCORE_REGISTER: PID {}", data->dwPID);

	ClientHandleInfo * pi = new ClientHandleInfo(0);
	strlcpy(pi->login, data->szBoard, sizeof(pi->login));
	pi->account_id = (uint32_t)data->lValue;
	pi->player_id = data->dwPID;
	pi->account_index = (data->cDir > 0);

	CDBManager::instance().ReturnQueryPrepared(szQuery,
			{ SqlParam::Text(data->szBoard), SqlParam::UInt(data->dwPID) },
			QID_HIGHSCORE_REGISTER, peer->GetHandle(), pi);
}

void CClientManager::RESULT_HIGHSCORE_REGISTER(CPeer * pkPeer, SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * pi = (ClientHandleInfo *) qi->pvData;
	//uint32_t dwHandle = pi->dwHandle;

	char szBoard[21];
	strlcpy(szBoard, pi->login, sizeof(szBoard));
	int value = (int)pi->account_id;

	SQLResult * res = msg->Get();

	if (res->uiNumRows == 0)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "INSERT INTO highscore%s VALUES(?, ?, ?)", GetTablePostfix());
		CDBManager::instance().QueryPrepared(buf, { SqlParam::Text(szBoard), SqlParam::UInt(pi->player_id), SqlParam::Int(value) });
	}
	else
	{
		if (!res->pSQLResult && !res->bPrepared)
		{
			delete pi;
			return;
		}

		MYSQL_ROW row = res->FetchRow();
		if (row && row[0])
		{
			int current_value = 0; str_to_number(current_value, row[0]);
			if (((pi->account_index)&&(current_value >= value)) || ((!pi->account_index)&&(current_value <= value)))
			{
				value = current_value;
			}
			else
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "REPLACE INTO highscore%s VALUES(?, ?, ?)", GetTablePostfix());
				CDBManager::instance().QueryPrepared(buf, { SqlParam::Text(szBoard), SqlParam::UInt(pi->player_id), SqlParam::Int(value) });
			}
		}
		else
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "INSERT INTO highscore%s VALUES(?, ?, ?)", GetTablePostfix());
			CDBManager::instance().QueryPrepared(buf, { SqlParam::Text(szBoard), SqlParam::UInt(pi->player_id), SqlParam::Int(value) });
		}
	}
	delete pi;
}

void CClientManager::InsertLogoutPlayer(uint32_t pid)
{
	if (const auto it = m_map_logout.find(pid); it != m_map_logout.end())
	{
		if (g_log)
			LOG_INFO("LOGOUT: Update player time pid({})", pid);

		it->second->time = time(nullptr);
		return;
	}

	auto pLogout = new TLogoutPlayer;
	pLogout->pid = pid;
	pLogout->time = time(nullptr);
	m_map_logout.insert(std::make_pair(pid, pLogout));

	if (g_log)
		LOG_INFO("LOGOUT: Insert player pid({})", pid);
}

void CClientManager::DeleteLogoutPlayer(uint32_t pid)
{
	if (const auto it = m_map_logout.find(pid); it != m_map_logout.end())
	{
		delete it->second;
		m_map_logout.erase(it);
	}
}

extern int g_iLogoutSeconds;

void CClientManager::UpdateLogoutPlayer()
{
	time_t now = time(nullptr);

	auto it = m_map_logout.begin();

	while (it != m_map_logout.end())
	{
		TLogoutPlayer* pLogout = it->second;

		if (now - g_iLogoutSeconds > pLogout->time)
		{
			FlushItemCacheSet(pLogout->pid);
			FlushPlayerCacheSet(pLogout->pid);

			delete pLogout;
			m_map_logout.erase(it++);
		}
		else
			++it;
	}
}

void CClientManager::FlushPlayerCacheSet(uint32_t pid)
{
	if (const auto it = m_map_playerCache.find(pid); it != m_map_playerCache.end())
	{
		CPlayerTableCache * c = it->second;
		m_map_playerCache.erase(it);

		c->Flush();
		delete c;
	}
}

