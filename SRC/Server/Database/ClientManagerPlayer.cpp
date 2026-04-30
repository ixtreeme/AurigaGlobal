
#include "stdafx.h"

#include "ClientManager.h"

#include "Main.h"
#include "QID.h"
#include "ItemAwardManager.h"
#include "Cache.h"

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

	if ((rows = mysql_num_rows(res)) <= 0)	// 데이터 없음
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

uint64_t CreatePlayerSaveQuery(char * pszQuery, uint64_t querySize, TPlayerTable * pkTab)
{
	uint64_t queryLen;

	queryLen = snprintf(pszQuery, querySize,
			"UPDATE player%s SET "
			"job = %d, "
			"voice = %d, "
			"dir = %d, "
			"x = %d, "
			"y = %d, "
			"z = %d, "
			"map_index = %d, "
			"exit_x = %d, "
			"exit_y = %d, "
			"exit_map_index = %d, "
			"hp = %d, "
			"mp = %d, "
			"stamina = %d, "
			"random_hp = %d, "
			"random_sp = %d, "
			"playtime = %d, "
			"level = %d, "
			"level_step = %d, "
			"st = %d, "
			"ht = %d, "
			"dx = %d, "
			"iq = %d, "

			"gold = %lld, "

#ifdef ENABLE_GAYA_SYSTEM
			"gaya = %d, " 
#endif
			"exp = %u, "
			"stat_point = %d, "
			"skill_point = %d, "
			"sub_skill_point = %d, "
			"stat_reset_count = %d, "
			"ip = '%s', "
			"part_main = %d, "
			"part_hair = %d, "
#ifdef ENABLE_ACCE_SYSTEM
			"part_acce = %d, "
#endif
			"last_play = NOW(), "
			"skill_group = %d, "
			"alignment = %u, "
			"horse_level = %d, "
			"horse_riding = %d, "
			"horse_hp = %d, "
			"horse_hp_droptime = %u, "
			"horse_stamina = %d, "
			"horse_skill_point = %d, "
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
			"envanter = %d, "
#endif

#ifdef ENABLE_BATTLE_PASS
			"battle_pass_end = FROM_UNIXTIME(%d), "
#endif
#ifdef ENABLE_RANKING
			"r0 = %lld, "
			"r1 = %lld, "
			"r2 = %lld, "
			"r3 = %lld, "
			"r4 = %lld, "
			"r5 = %lld, "
			"r6 = %lld, "
			"r7 = %lld, "
			"r8 = %lld, "
			"r9 = %lld, "
			"r10 = %lld, "
			"r11 = %lld, "
			"r12 = %lld, "
			"r13 = %lld, "
			"r14 = %lld, "
			"r15 = %lld, "
			"r16 = %lld, "
			"r17 = %lld, "
			"r18 = %lld, "
			"r19 = %lld, "
#endif
		, GetTablePostfix(),
		pkTab->job,
		pkTab->voice,
		pkTab->dir,
		pkTab->x,
		pkTab->y,
		pkTab->z,
		pkTab->lMapIndex,
		pkTab->lExitX,
		pkTab->lExitY,
		pkTab->lExitMapIndex,
		pkTab->hp,
		pkTab->sp,
		pkTab->stamina,
		pkTab->sRandomHP,
		pkTab->sRandomSP,
		pkTab->playtime,
		pkTab->level,
		pkTab->level_step,
		pkTab->st,
		pkTab->ht,
		pkTab->dx,
		pkTab->iq,
		pkTab->gold,
#ifdef ENABLE_GAYA_SYSTEM
		pkTab->gaya,
#endif
		pkTab->exp,
		pkTab->stat_point,
		pkTab->skill_point,
		pkTab->sub_skill_point,
		pkTab->stat_reset_count,
		pkTab->ip,
		pkTab->parts[PART_MAIN],
		pkTab->parts[PART_HAIR],
#ifdef ENABLE_ACCE_SYSTEM
		pkTab->parts[PART_ACCE],
#endif
		pkTab->skill_group,
		pkTab->lAlignment,
		pkTab->horse.bLevel,
		pkTab->horse.bRiding,
		pkTab->horse.sHealth,
		pkTab->horse.dwHorseHealthDropTime,
		pkTab->horse.sStamina,
		pkTab->horse_skill_point
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		, pkTab->envanter
#endif
#ifdef ENABLE_BATTLE_PASS
		,pkTab->dwBattlePassEndTime
#endif
#ifdef ENABLE_RANKING
		, pkTab->lRankPoints[0],
		pkTab->lRankPoints[1],
		pkTab->lRankPoints[2],
		pkTab->lRankPoints[3],
		pkTab->lRankPoints[4],
		pkTab->lRankPoints[5],
		pkTab->lRankPoints[6],
		pkTab->lRankPoints[7],
		pkTab->lRankPoints[8],
		pkTab->lRankPoints[9],
		pkTab->lRankPoints[10],
		pkTab->lRankPoints[11],
		pkTab->lRankPoints[12],
		pkTab->lRankPoints[13],
		pkTab->lRankPoints[14],
		pkTab->lRankPoints[15],
		pkTab->lRankPoints[16],
		pkTab->lRankPoints[17],
		pkTab->lRankPoints[18],
		pkTab->lRankPoints[19]
#endif
		);

	static char text[8192 + 1];

	CDBManager::instance().EscapeString(text, pkTab->skills, sizeof(pkTab->skills));
	queryLen += snprintf(pszQuery + queryLen, querySize - queryLen, "skill_level = '%s', ", text);

	CDBManager::instance().EscapeString(text, pkTab->quickslot, sizeof(pkTab->quickslot));
	queryLen += snprintf(pszQuery + queryLen, querySize - queryLen, "quickslot = '%s' ", text);

	queryLen += snprintf(pszQuery + queryLen, querySize - queryLen, " WHERE id=%u", pkTab->id);
	return queryLen;
}

CPlayerTableCache * CClientManager::GetPlayerCache(uint32_t id)
{
	const auto it = m_map_playerCache.find(id);

	if (it == m_map_playerCache.end())
		return nullptr;

	TPlayerTable* pTable = it->second->Get(false);
	pTable->logoff_interval = GetCurrentTime() - it->second->GetLastUpdateTime();
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
	CSKillColorCache * c = GetSkillColorCache(packet->player_id);
	TSkillColor * p;
	if (c)
	{
		p = c->Get();
		peer->EncodeHeader(HEADER_DG_SKILL_COLOR_LOAD, dwHandle, sizeof(p->dwSkillColor));
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
			"s10_col1, s10_col2, s10_col3, s10_col4, s10_col5 "
			"s11_col1, s11_col2, s11_col3, s11_col4, s11_col5 "
			"FROM skill_color%s WHERE player_id=%u",
			GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(szQuery, QID_SKILL_COLOR, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id));
	}
}
#endif

void CClientManager::QUERY_PLAYER_LOAD(CPeer * peer, uint32_t dwHandle, TPlayerLoadPacket * packet)
{
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
			LOG_INFO("PLAYER_LOAD_ERROR: LoginData {} IsPlay {}", pkLD, pkLD ? pkLD->IsPlay() : 0);
			peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_FAILED, dwHandle, 0);
			return;
		}

		TPlayerTable* pTab = c->Get();

		pkLD->SetPlay(true);
		SendLoginToBilling(pkLD, true);
		memcpy(pTab->aiPremiumTimes, pkLD->GetPremiumPtr(), sizeof(pTab->aiPremiumTimes));

		peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_SUCCESS, dwHandle, sizeof(TPlayerTable));
		peer->Encode(pTab, sizeof(TPlayerTable));

		if (packet->player_id != pkLD->GetLastPlayerID())
		{
			TPacketNeedLoginLogInfo logInfo;
			logInfo.dwPlayerID = packet->player_id;

			pkLD->SetLastPlayerID( packet->player_id );

			peer->EncodeHeader( HEADER_DG_NEED_LOGIN_LOG, dwHandle, sizeof(TPacketNeedLoginLogInfo) );
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

			peer->EncodeHeader(HEADER_DG_ITEM_LOAD, dwHandle, sizeof(uint32_t) + sizeof(TPlayerItem) * dwCount);
			peer->EncodeDWORD(dwCount);

			if (dwCount)
				peer->Encode(s_items.data(), sizeof(TPlayerItem) * dwCount);

			// Quest
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%u AND lValue<>0",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery, QID_QUEST, peer->GetHandle(), new ClientHandleInfo(dwHandle,0,packet->account_id));

			// Affect
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%u",
					GetTablePostfix(), pTab->id);
			// @fixme402 ClientHandleInfo+pTab->id
			CDBManager::instance().ReturnQuery(szQuery, QID_AFFECT, peer->GetHandle(), new ClientHandleInfo(dwHandle, pTab->id));
		}

		else
		{
			snprintf(szQuery, sizeof(szQuery),
					"SELECT id,window+0,pos,count,vnum,"
#ifdef ATTR_LOCK
					"lockedattr,"
#endif
					"socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
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
					new ClientHandleInfo(dwHandle, pTab->id));
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID, szName, szState, lValue FROM quest%s WHERE dwPID=%u",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_QUEST,
					peer->GetHandle(),
					new ClientHandleInfo(dwHandle, pTab->id));
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost FROM affect%s WHERE dwPID=%u",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_AFFECT,
					peer->GetHandle(),
					new ClientHandleInfo(dwHandle, pTab->id));
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

#ifdef ENABLE_BATTLE_PASS
				",UNIX_TIMESTAMP(battle_pass_end) "
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
				"FROM player%s WHERE id=%u",
				GetTablePostfix(), packet->player_id);

		auto pkInfo = new ClientHandleInfo(dwHandle, packet->player_id);
		pkInfo->account_id = packet->account_id;
		CDBManager::instance().ReturnQuery(queryStr, QID_PLAYER, peer->GetHandle(), pkInfo);

		snprintf(queryStr, sizeof(queryStr),
				"SELECT id,window+0,pos,count,vnum,"
#ifdef ATTR_LOCK
				"lockedattr,"
#endif
				"socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
				"FROM item%s WHERE owner_id=%u AND (window in ('INVENTORY','EQUIPMENT','DRAGON_SOUL_INVENTORY','BELT_INVENTORY'"
#ifdef ENABLE_EXTRA_INVENTORY
				",'EXTRA_INVENTORY'"
#endif
#ifdef ENABLE_SWITCHBOT
				",'SWITCHBOT'"
#endif
				"))",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_ITEM, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id));


		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%u",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_QUEST, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id,packet->account_id));

		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%u",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_AFFECT, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id));
	}

#ifdef ENABLE_BATTLE_PASS
	// Load all missions from table
	char queryStrBP[QUERY_MAX_LEN];
	snprintf(queryStrBP, sizeof(queryStrBP),
		"SELECT player_id, mission_id, battle_pass_id, extra_info, completed FROM battle_pass WHERE player_id = %u", packet->player_id);
		
	CDBManager::instance().ReturnQuery(queryStrBP, QID_BATTLE_PASS, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id));
#endif


#ifdef __ENABLE_NEW_OFFLINESHOP__
	OfflineshopLoadShopSafebox(peer, packet->player_id);
#endif
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

#ifdef ENABLE_BATTLE_PASS
	str_to_number(pkTab->dwBattlePassEndTime, row[col++]);
#endif
#ifdef ENABLE_RANKING
	for (long long& lRankPoint : pkTab->lRankPoints)
		str_to_number(lRankPoint, row[col++]);
#endif

	// reset sub_skill_point
	{
		pkTab->skills[123].bLevel = 0; // SKILL_CREATE

		if (pkTab->level > 9)
		{
			int max_point = pkTab->level - 9;

			int skill_point =
				MIN(20, pkTab->skills[121].bLevel) +	// SKILL_LEADERSHIP
				MIN(20, pkTab->skills[124].bLevel) +	// SKILL_MINING
				MIN(10, pkTab->skills[131].bLevel) +	// SKILL_HORSE_SUMMON
				MIN(20, pkTab->skills[141].bLevel) +	// SKILL_ADD_HP
				MIN(20, pkTab->skills[142].bLevel);	// SKILL_RESIST_PENETRATE

#ifdef ENABLE_NEW_SECONDARY_SKILLS
			skill_point =+ MIN(10, pkTab->skills[143].bLevel) + MIN(10, pkTab->skills[144].bLevel) + MIN(10, pkTab->skills[145].bLevel) + MIN(10, pkTab->skills[146].bLevel);
#endif

			pkTab->sub_skill_point = max_point - skill_point;
		}
		else
			pkTab->sub_skill_point = 0;
	}

	return true;
}

void CClientManager::RESULT_COMPOSITE_PLAYER(CPeer * peer, SQLMsg * pMsg, uint32_t dwQID)
{
	auto qi = (CQueryInfo *) pMsg->pvUserData;
	const std::unique_ptr<ClientHandleInfo> info((ClientHandleInfo *) qi->pvData);

	MYSQL_RES * pSQLResult = pMsg->Get()->pSQLResult;
	if (!pSQLResult)
	{
		LOG_ERROR("null MYSQL_RES QID {}", dwQID);
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
			RESULT_ITEM_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);
			break;

#ifdef ENABLE_BATTLE_PASS
		case QID_BATTLE_PASS:
		LOG_INFO("QID_BATTLE_PASS {}", info->dwHandle);
		RESULT_BATTLE_PASS_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);
		break;
#endif

#ifdef __SKILL_COLOR_SYSTEM__
		case QID_SKILL_COLOR:
			LOG_INFO("QID_SKILL_COLOR {} {}", info->dwHandle, info->player_id);
			RESULT_SKILL_COLOR_LOAD(peer, pSQLResult, info->dwHandle);
			break;
#endif

		case QID_QUEST:
			{
				LOG_INFO("QID_QUEST {}", info->dwHandle);
				RESULT_QUEST_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);
				//aid얻기
				ClientHandleInfo*  temp1 = info.get();
				if (temp1 == nullptr)
					break;

				CLoginData* pLoginData1 = GetLoginDataByAID(temp1->account_id);	//
				//독일 선물 기능
				if( pLoginData1 == nullptr)
					break;

				if( pLoginData1->GetAccountRef().login == nullptr)
					break;

				LOG_INFO("info of pLoginData1 before call ItemAwardfunction {}", pLoginData1);
				ItemAward(peer,pLoginData1->GetAccountRef().login);
			}
			break;

		case QID_AFFECT:
			LOG_INFO("QID_AFFECT {}", info->dwHandle);
			// @fixme402 RESULT_AFFECT_LOAD+info->player_id
			RESULT_AFFECT_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);
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
			   RESULT_ITEM_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);

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
		peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_FAILED, pkInfo->dwHandle, 0);
		return;
	}

	CLoginData * pkLD = GetLoginDataByAID(pkInfo->account_id);

	if (!pkLD || pkLD->IsPlay())
	{
		LOG_INFO("PLAYER_LOAD_ERROR: LoginData {} IsPlay {}", pkLD, pkLD ? pkLD->IsPlay() : 0);
		peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_FAILED, pkInfo->dwHandle, 0);
		return;
	}

	pkLD->SetPlay(true);
	SendLoginToBilling(pkLD, true);
	memcpy(tab.aiPremiumTimes, pkLD->GetPremiumPtr(), sizeof(tab.aiPremiumTimes));

	peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_SUCCESS, pkInfo->dwHandle, sizeof(TPlayerTable));
	peer->Encode(&tab, sizeof(TPlayerTable));

	if (tab.id != pkLD->GetLastPlayerID())
	{
		TPacketNeedLoginLogInfo logInfo;
		logInfo.dwPlayerID = tab.id;

		pkLD->SetLastPlayerID( tab.id );

		peer->EncodeHeader( HEADER_DG_NEED_LOGIN_LOG, pkInfo->dwHandle, sizeof(TPacketNeedLoginLogInfo) );
		peer->Encode( &logInfo, sizeof(TPacketNeedLoginLogInfo) );
	}
}

void CClientManager::RESULT_ITEM_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwPID)
{
	std::vector<TPlayerItem> s_items;
	//DB에서 아이템 정보를 읽어온다.
	CreateItemTableFromRes(pRes, &s_items, dwPID);
	uint32_t dwCount = (uint32_t)s_items.size();

	peer->EncodeHeader(HEADER_DG_ITEM_LOAD, dwHandle, sizeof(uint32_t) + sizeof(TPlayerItem) * dwCount);
	peer->EncodeDWORD(dwCount);

	//CacheSet을 만든다
	CreateItemCacheSet(dwPID);

	// ITEM_LOAD_LOG_ATTACH_PID
	LOG_INFO("ITEM_LOAD: count {} pid {}", dwCount, dwPID);
	// END_OF_ITEM_LOAD_LOG_ATTACH_PID

	if (dwCount)
	{
		peer->Encode(s_items.data(), sizeof(TPlayerItem) * dwCount);

		for (uint32_t i = 0; i < dwCount; ++i)
			PutItemCache(&s_items[i], true); // 로드한 것은 따로 저장할 필요 없으므로, 인자 bSkipQuery에 true를 넣는다.
	}
}

#ifdef __SKILL_COLOR_SYSTEM__
void CClientManager::RESULT_SKILL_COLOR_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle)
{
	uint32_t dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
	memset(dwSkillColor, 0, sizeof(dwSkillColor));

	CreateSkillColorTableFromRes(pRes, *dwSkillColor);
	LOG_INFO("SKILL_COLOR_LOAD {}, {}, {}, {},", dwSkillColor[0][0], dwSkillColor[1][1], dwSkillColor[2][2], dwSkillColor[3][3]);
	peer->EncodeHeader(HEADER_DG_SKILL_COLOR_LOAD, dwHandle, sizeof(dwSkillColor));
	peer->Encode(&dwSkillColor, sizeof(dwSkillColor));
}
#endif

// @fixme402 (RESULT_AFFECT_LOAD +dwRealPID)
void CClientManager::RESULT_AFFECT_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwRealPID)
{
	uint64_t iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0) // 데이터 없음
	{
		// @fixme402 begin
		//static uint32_t dwPID;
		uint32_t dwCount = 0; //1;
		//static TPacketAffectElement paeTable = {};

		//dwPID = dwRealPID;
		LOG_INFO("AFFECT_LOAD: count {}  RealPID {}", dwCount, dwRealPID);

		peer->EncodeHeader(HEADER_DG_AFFECT_LOAD, dwHandle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPacketAffectElement) * dwCount);
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

	peer->EncodeHeader(HEADER_DG_AFFECT_LOAD, dwHandle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPacketAffectElement) * dwCount);
	peer->Encode(&dwPID, sizeof(uint32_t));
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(s_elements.data(), sizeof(TPacketAffectElement) * dwCount);
}

void CClientManager::RESULT_QUEST_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t pid)
{
	uint64_t i_num_rows;

	if ((i_num_rows = mysql_num_rows(pRes)) == 0)
	{
		uint32_t dwCount = 0;
		peer->EncodeHeader(HEADER_DG_QUEST_LOAD, dwHandle, sizeof(uint32_t));
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

	peer->EncodeHeader(HEADER_DG_QUEST_LOAD, dwHandle, sizeof(uint32_t) + sizeof(TQuestTable) * dwCount);
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(s_table.data(), sizeof(TQuestTable) * dwCount);
}

#ifdef ENABLE_BATTLE_PASS
void CClientManager::RESULT_BATTLE_PASS_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwRealPID)
{
	int iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0)
	{
		uint32_t dwCount = 0;
		TPlayerBattlePassMission pbpTable = {0};

		LOG_INFO("BATTLE_PASS_LOAD: count {} PID {}", dwCount, dwRealPID);

		peer->EncodeHeader(HEADER_DG_BATTLE_PASS_LOAD, dwHandle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPlayerBattlePassMission) * dwCount);
		peer->Encode(&dwRealPID, sizeof(uint32_t));
		peer->Encode(&dwCount, sizeof(uint32_t));
		peer->Encode(&pbpTable, sizeof(TPlayerBattlePassMission) * dwCount);
		return;
	}

	static std::vector<TPlayerBattlePassMission> s_mission;
	s_mission.resize(iNumRows);

	for (int i = 0; i < iNumRows; ++i)
	{
		int col = 0;
		TPlayerBattlePassMission & r = s_mission[i];
		MYSQL_ROW row = mysql_fetch_row(pRes);

		str_to_number(r.dwPlayerId, row[col++]);
		str_to_number(r.dwMissionId, row[col++]);
		str_to_number(r.dwBattlePassId, row[col++]);
		str_to_number(r.dwExtraInfo, row[col++]);
		str_to_number(r.bCompleted, row[col++]);

		r.bIsUpdated = 0;
	}

	LOG_INFO("BATTLE_PASS_LOAD: count {} PID {}", s_mission.size(), dwRealPID);

	uint32_t dwCount = s_mission.size();

	peer->EncodeHeader(HEADER_DG_BATTLE_PASS_LOAD, dwHandle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPlayerBattlePassMission) * dwCount);
	peer->Encode(&dwRealPID, sizeof(uint32_t));
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(s_mission.data(), sizeof(TPlayerBattlePassMission) * dwCount);
}

void CClientManager::QUERY_SAVE_BATTLE_PASS(CPeer * peer, uint32_t dwHandle, TPlayerBattlePassMission* battlePass)
{
	if (g_test_server)
		LOG_INFO("QUERY_SAVE_BATTLE_PASS: {}", battlePass->dwPlayerId);

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery),
		"REPLACE INTO battle_pass (player_id, mission_id, battle_pass_id, extra_info, completed) VALUES (%d, %d, %d, %d, %d)",
		battlePass->dwPlayerId,
		battlePass->dwMissionId,
		battlePass->dwBattlePassId,
		battlePass->dwExtraInfo,
		battlePass->bCompleted ? 1 : 0);
	CDBManager::instance().AsyncQuery(szQuery);
}

extern int g_iCurrentBattlePassId;
bool CClientManager::LoadBattlePassRanking()
{
	char query[256];
	snprintf(query, sizeof(query), "SELECT player_name, UNIX_TIMESTAMP(finish_time) "
				"FROM battle_pass_ranking WHERE battle_pass_id = %d ORDER BY finish_time ASC LIMIT 45;", g_iCurrentBattlePassId);

	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery(query));
	SQLResult * pRes = pkMsg->Get();

	if (!pRes->uiNumRows)
		return true; // No ranking for battle pass with id %i%

	if(!m_vec_battlePassRanking.empty())
		m_vec_battlePassRanking.clear();

	uint8_t posContor = 1;

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(pRes->pSQLResult)))
	{
		TBattlePassRanking * ranking = new TBattlePassRanking;
		ranking->bPos = posContor;
		strlcpy(ranking->playerName, row[0], sizeof(ranking->playerName));
		str_to_number(ranking->dwFinishTime, row[1]);

		m_vec_battlePassRanking.push_back(ranking);
		posContor++;
	}

	return true;
}

void CClientManager::RequestLoadBattlePassRanking(CPeer * peer, uint32_t dwHandle, const char* data)
{
	uint32_t dwPlayerID = *(uint32_t *)data;
	data += sizeof(uint32_t);

	uint8_t bIsGlobal = *(uint8_t *)data;
	data += sizeof(uint8_t);

	if(m_vec_battlePassRanking.empty())
	{
		uint32_t dwCount = 0;
		TBattlePassRanking pbpTable = {0};

		LOG_INFO("RequestLoadBattlePassRanking: count {} PID {}", dwCount, dwPlayerID);

		peer->EncodeHeader(HEADER_DG_BATTLE_PASS_LOAD_RANKING, dwHandle, sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(TBattlePassRanking) * dwCount);
		peer->Encode(&dwPlayerID, sizeof(uint32_t));
		peer->Encode(&bIsGlobal, sizeof(uint8_t));
		peer->Encode(&dwCount, sizeof(uint32_t));
		peer->Encode(&pbpTable, sizeof(TBattlePassRanking) * dwCount);
		return;
	}
	else
	{
		static std::vector<TBattlePassRanking> sendVector;
		sendVector.resize(m_vec_battlePassRanking.size());

		//MYSQL_ROW row;

		for (unsigned int i = 0; i < m_vec_battlePassRanking.size(); ++i)
		{
			TBattlePassRanking * pkRanking = m_vec_battlePassRanking[i];
			TBattlePassRanking & r = sendVector[i];

			r.bPos = pkRanking->bPos;
			strlcpy(r.playerName, pkRanking->playerName, sizeof(r.playerName));
			r.dwFinishTime = pkRanking->dwFinishTime;
		}

		LOG_INFO("RequestLoadBattlePassRanking: count {} PID {}", sendVector.size(), dwPlayerID);

		uint32_t dwCount = sendVector.size();

		peer->EncodeHeader(HEADER_DG_BATTLE_PASS_LOAD_RANKING, dwHandle, sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(TBattlePassRanking) * dwCount);
		peer->Encode(&dwPlayerID, sizeof(uint32_t));
		peer->Encode(&bIsGlobal, sizeof(uint8_t));
		peer->Encode(&dwCount, sizeof(uint32_t));
		peer->Encode(sendVector.data(), sizeof(TBattlePassRanking) * dwCount);
	}
}

void CClientManager::QUERY_REGISTER_RANKING(CPeer * peer, uint32_t dwHandle, TBattlePassRegisterRanking* pRanking)
{
	if (g_test_server)
		LOG_INFO("REGISTER_RANKING: {} id {}", pRanking->playerName, pRanking->bBattlePassId);

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery),
		"REPLACE INTO battle_pass_ranking (player_name, battle_pass_id, finish_time) VALUES ('%s', %d, NOW())", pRanking->playerName, pRanking->bBattlePassId);

	CDBManager::instance().AsyncQuery(szQuery);
}
#endif

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
	char	queryStr[QUERY_MAX_LEN];
	int queryLen;
	uint32_t		player_id;

	// 한 계정에 X초 내로 캐릭터 생성을 할 수 없다.

	if (auto it = s_createTimeByAccountID.find(packet->account_id); it != s_createTimeByAccountID.end())
	{
		uint32_t curtime = time(nullptr);

		if (curtime - it->second < 30)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}
	}

	queryLen = snprintf(queryStr, sizeof(queryStr),
	                        "SELECT pid%u FROM player_index%s WHERE id=%d", packet->account_index + 1,
	                        GetTablePostfix(), packet->account_id);

	std::unique_ptr<SQLMsg> pMsg0(CDBManager::instance().DirectQuery(queryStr));

	if (pMsg0->Get()->uiNumRows != 0)
	{
		if (!pMsg0->Get()->pSQLResult)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg0->Get()->pSQLResult);

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
			"SELECT COUNT(*) as count FROM player%s WHERE name='%s'", GetTablePostfix(), packet->player_table.name);

	std::unique_ptr<SQLMsg> pMsg1(CDBManager::instance().DirectQuery(queryStr));

	if (pMsg1->Get()->uiNumRows)
	{
		if (!pMsg1->Get()->pSQLResult)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg1->Get()->pSQLResult);

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

	queryLen = snprintf(queryStr, sizeof(queryStr),
			"INSERT INTO player%s "
			"(id, account_id, name, "
			"level, st, ht, dx, iq, "
			"job, voice, dir, x, y, z, "
			"hp, mp, random_hp, random_sp, stat_point, stamina, part_base, part_main, part_hair,"
#ifdef ENABLE_ACCE_SYSTEM
			"part_acce, "
#endif
			"gold, "
#ifdef ENABLE_GAYA_SYSTEM
			"gaya, "
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
			"envanter, "
#endif
			"playtime, "
			"skill_level, quickslot) "
			"VALUES(0, %u, '%s', "
			"%d, %d, %d, %d, %d, "
			"%d, %d, %d, %d, %d, %d, %d, "
			"%d, %d, %d, %d, %d, %d, %d, 0,"
#ifdef ENABLE_ACCE_SYSTEM
			"0, "
#endif
			"%lld, "

#ifdef ENABLE_GAYA_SYSTEM
			"%d, "
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
			"%d, "
#endif
			"0, ",
			GetTablePostfix(),
			packet->account_id, packet->player_table.name,
			packet->player_table.level, packet->player_table.st, packet->player_table.ht, packet->player_table.dx, packet->player_table.iq,
			packet->player_table.job, packet->player_table.voice, packet->player_table.dir, packet->player_table.x, packet->player_table.y, packet->player_table.z,
			packet->player_table.hp, packet->player_table.sp, packet->player_table.sRandomHP, packet->player_table.sRandomSP, packet->player_table.stat_point, packet->player_table.stamina, packet->player_table.part_base, packet->player_table.part_base, packet->player_table.gold
#ifdef ENABLE_GAYA_SYSTEM
			,packet->player_table.gaya
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
			,packet->player_table.envanter
#endif
			);

	LOG_INFO("PlayerCreate accountid {} name {} level {} gold {}, st {} ht {} job {}", packet->account_id, packet->player_table.name, packet->player_table.level, packet->player_table.gold, packet->player_table.st, packet->player_table.ht, packet->player_table.job);

			
#ifdef ENABLE_GAYA_SYSTEM
	LOG_INFO("PlayerCreate accountid {} name {} gaya {}", packet->account_id, packet->player_table.name, packet->player_table.gaya);
#endif

	static char text[8192 + 1];

	CDBManager::instance().EscapeString(text, packet->player_table.skills, sizeof(packet->player_table.skills));
	queryLen += snprintf(queryStr + queryLen, sizeof(queryStr) - queryLen, "'%s', ", text);
	if (g_test_server)
		LOG_INFO("Create_Player queryLen[{}] TEXT[{}]", queryLen, text);

	CDBManager::instance().EscapeString(text, packet->player_table.quickslot, sizeof(packet->player_table.quickslot));
	queryLen += snprintf(queryStr + queryLen, sizeof(queryStr) - queryLen, "'%s')", text);

	std::unique_ptr<SQLMsg> pMsg2(CDBManager::instance().DirectQuery(queryStr));
	if (g_test_server)
		LOG_INFO("Create_Player queryLen[{}] TEXT[{}]", queryLen, text);

	if (pMsg2->Get()->uiAffectedRows <= 0)
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
		LOG_INFO("ALREADY EXIST3 query: {} AffectedRows {}", queryStr, pMsg2->Get()->uiAffectedRows);
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
		CDBManager::instance().DirectQuery(queryStr);

		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	TPacketDGCreateSuccess pack = {};

	pack.bAccountCharacterIndex = packet->account_index;

	pack.player.dwID = player_id;
	strlcpy(pack.player.szName, packet->player_table.name, sizeof(pack.player.szName));
	pack.player.byJob = packet->player_table.job;
	pack.player.byLevel = 1;
	pack.player.dwPlayMinutes = 0;
	pack.player.byST = packet->player_table.st;
	pack.player.byHT = packet->player_table.ht;
	pack.player.byDX = packet->player_table.dx;
	pack.player.byIQ = packet->player_table.iq;
	pack.player.wMainPart = packet->player_table.part_base;
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
// @version	05/06/10 Bang2ni - 플레이어 삭제시 가격정보 리스트 삭제 추가.
//
void CClientManager::__RESULT_PLAYER_DELETE(CPeer *peer, SQLMsg* msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * pi = (ClientHandleInfo *) qi->pvData;

	if (msg->Get() && msg->Get()->uiNumRows)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

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

		// 삭제 성공
		LOG_INFO("PLAYER_DELETE SUCCESS {}", dwPID);

		char account_index_string[16];

		snprintf(account_index_string, sizeof(account_index_string), "player_id%d", m_iPlayerIDStart + pi->account_index);

		// 플레이어 테이블을 캐쉬에서 삭제한다.
		CPlayerTableCache * pkPlayerCache = GetPlayerCache(pi->player_id);

		if (pkPlayerCache)
		{
			m_map_playerCache.erase(pi->player_id);
			delete pkPlayerCache;
		}

		// 아이템들을 캐쉬에서 삭제한다.
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
		delete CDBManager::instance().DirectQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM item%s WHERE owner_id=%u AND (window in ('INVENTORY','EQUIPMENT','DRAGON_SOUL_INVENTORY','BELT_INVENTORY'"
#ifdef ENABLE_EXTRA_INVENTORY
		",'EXTRA_INVENTORY'"
#endif
#ifdef ENABLE_SWITCHBOT
		",'SWITCHBOT'"
#endif
		"))",
		GetTablePostfix(), pi->player_id);
		delete CDBManager::instance().DirectQuery(queryStr);

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

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM messenger_list%s WHERE account='%s' OR companion='%s'", GetTablePostfix(), szName, szName);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM horse_name%s WHERE id=%u", GetTablePostfix(), pi->player_id);
		CDBManager::instance().AsyncQuery(queryStr);

		snprintf(queryStr, sizeof(queryStr), "DELETE FROM guild_comment%s WHERE name='%s'", GetTablePostfix(), szName);
		CDBManager::instance().AsyncQuery(queryStr);

		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_SUCCESS, pi->dwHandle, 1);
		peer->EncodeBYTE(pi->account_index);
	}
	else
	{
		// 삭제 실패
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
	snprintf(szQuery, sizeof(szQuery), "SELECT value FROM highscore%s WHERE board='%s' AND pid = %u", GetTablePostfix(), data->szBoard, data->dwPID);

	LOG_INFO("HEADER_GD_HIGHSCORE_REGISTER: PID {}", data->dwPID);

	ClientHandleInfo * pi = new ClientHandleInfo(0);
	strlcpy(pi->login, data->szBoard, sizeof(pi->login));
	pi->account_id = (uint32_t)data->lValue;
	pi->player_id = data->dwPID;
	pi->account_index = (data->cDir > 0);

	CDBManager::instance().ReturnQuery(szQuery, QID_HIGHSCORE_REGISTER, peer->GetHandle(), pi);
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
		// 새로운 하이스코어를 삽입
		char buf[256];
		snprintf(buf, sizeof(buf), "INSERT INTO highscore%s VALUES('%s', %u, %d)", GetTablePostfix(), szBoard, pi->player_id, value);
		CDBManager::instance().AsyncQuery(buf);
	}
	else
	{
		if (!res->pSQLResult)
		{
			delete pi;
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(res->pSQLResult);
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
				snprintf(buf, sizeof(buf), "REPLACE INTO highscore%s VALUES('%s', %u, %d)", GetTablePostfix(), szBoard, pi->player_id, value);
				CDBManager::instance().AsyncQuery(buf);
			}
		}
		else
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "INSERT INTO highscore%s VALUES('%s', %u, %d)", GetTablePostfix(), szBoard, pi->player_id, value);
			CDBManager::instance().AsyncQuery(buf);
		}
	}
	// TODO: 이곳에서 하이스코어가 업데이트 되었는지 체크하여 공지를 뿌려야한다.
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

