#include <type_traits>
#include <cmath>
// vim:ts=4 sw=4
#include <map>
#include "stdafx.h"
#include "ClientManager.h"
#include "Main.h"
#include "CsvReader.h"
#include "ProtoReader.h"
#include <common/retired_content.h>
#include <algorithm>
#include "Core/Logging.hpp"

using namespace std;

extern int g_test_server;
extern std::string g_stLocaleNameColumn;

#ifdef ENABLE_MULTI_NAMES
static std::string languages[] =
{
	"en", "ro", "it", "tr", "de", "pl", "pt", "es", "cz", "hu",
};
#endif

bool CClientManager::InitializeTables()
{
#ifdef ENABLE_PROTO_FROM_DB
	if (!(InitializeMobTableFromDB()))
#else
	if (!InitializeMobTable())
#endif
	{
		LOG_ERROR("InitializeMobTable FAILED");
		return false;
	}
    // Only native item catalogs, shops, refinement and bonus tables are used.
    // These empty ABI slots remain reserved in the shared non-item boot frame.
    m_vec_itemTable.clear();m_map_itemTableByVnum.clear();
#ifdef ENABLE_PROTO_FROM_DB
    extern bool g_bMirror2DB;
    if (g_bMirror2DB && !MirrorMobTableIntoDB())
    {
        LOG_ERROR("MirrorMobTableIntoDB FAILED");
        return false;
    }
#endif
	if (!InitializeSkillTable())
	{
		LOG_ERROR("InitializeSkillTable FAILED");
		return false;
	}

	if (!InitializeBanwordTable())
	{
		LOG_ERROR("InitializeBanwordTable FAILED");
		return false;
	}

	if (!InitializeLandTable())
	{
		LOG_ERROR("InitializeLandTable FAILED");
		return false;
	}

	if (!InitializeObjectProto())
	{
		LOG_ERROR("InitializeObjectProto FAILED");
		return false;
	}

	if (!InitializeObjectTable())
	{
		LOG_ERROR("InitializeObjectTable FAILED");
		return false;
	}

#ifdef ENABLE_EVENT_MANAGER
	if (!InitializeEventManager())
	{
		LOG_ERROR("InitializeEventManager FAILED");
		return false;
	}
#endif

	return true;
}

class FCompareVnum
{
public:
	bool operator () (const TEntityTable& a, const TEntityTable& b) const
	{
		return (a.dwVnum < b.dwVnum);
	}
};

bool CClientManager::InitializeMobTable()
{
	
	map<int, const char*> localMap;

	cCsvTable nameData;
	if (!nameData.Load("mob_names.txt", '\t'))
	{
		LOG_ERROR("Could not load mob_names.txt");
	}
	else {
		nameData.Next();
		while (nameData.Next()) {
			localMap[atoi(nameData.AsStringByIndex(0))] = nameData.AsStringByIndex(1);
		}
	}

	cCsvTable data;

	if (!data.Load("mob_proto.txt", '\t'))
	{
		LOG_ERROR("Could not load mob_proto.txt. Wrong file format?");
		return false;
	}
	data.Next();

	if (!m_vec_mobTable.empty())
	{
		LOG_INFO("RELOAD: mob_proto");
		m_vec_mobTable.clear();
	}
	m_vec_mobTable.resize(data.m_File.GetRowCount() - 1);
	memset(m_vec_mobTable.data(), 0, sizeof(TMobTable) * m_vec_mobTable.size());
	TMobTable* mob_table = m_vec_mobTable.data();

	while (data.Next())
	{

		if (!Set_Proto_Mob_Table(mob_table, data, localMap))
		{
			LOG_ERROR("Could not process entry.");
		}

#ifdef ENABLE_MULTI_NAMES
		LOG_TRACE("MOB #{:<5} {:<24} {:<24} level: {:<3} rank: {} empire: {}", mob_table->dwVnum, mob_table->szName, mob_table->szLocaleName[0], mob_table->bLevel, mob_table->bRank, mob_table->bEmpire);
#else
		LOG_TRACE("MOB #{:<5} {:<24} {:<24} level: {:<3} rank: {} empire: {}", mob_table->dwVnum, mob_table->szName, mob_table->szLocaleName, mob_table->bLevel, mob_table->bRank, mob_table->bEmpire);
#endif
		++mob_table;

	}

	sort(m_vec_mobTable.begin(), m_vec_mobTable.end(), FCompareVnum());
	return true;
}

bool CClientManager::InitializeSkillTable()
{
	char query[4096];
	snprintf(query, sizeof(query),
		"SELECT dwVnum, szName, bType, bMaxLevel, dwSplashRange, "
		"szPointOn, szPointPoly, szSPCostPoly, szDurationPoly, szDurationSPCostPoly, "
		"szCooldownPoly, szMasterBonusPoly, setFlag+0, setAffectFlag+0, "
		"szPointOn2, szPointPoly2, szDurationPoly2, setAffectFlag2+0, "
		"szPointOn3, szPointPoly3, szDurationPoly3, szGrandMasterAddSPCostPoly, "
		"bLevelStep, bLevelLimit, prerequisiteSkillVnum, prerequisiteSkillLevel, iMaxHit, szSplashAroundDamageAdjustPoly, eSkillType+0, dwTargetRange "
		"FROM skill_proto%s ORDER BY dwVnum",
		GetTablePostfix());

	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery(query));
	SQLResult* pRes = pkMsg->Get();

	if (!pRes->uiNumRows)
	{
		LOG_ERROR("no result from skill_proto");
		return false;
	}

	if (!m_vec_skillTable.empty())
	{
		LOG_INFO("RELOAD: skill_proto");
		m_vec_skillTable.clear();
	}

	m_vec_skillTable.reserve(pRes->uiNumRows);

	MYSQL_ROW	data;

	while ((data = pRes->FetchRow()))
	{
		TSkillTable t = {};

		int col = 0;

		str_to_number(t.dwVnum, data[col++]);
		strlcpy(t.szName, data[col++], sizeof(t.szName));
		str_to_number(t.bType, data[col++]);
		str_to_number(t.bMaxLevel, data[col++]);
		str_to_number(t.dwSplashRange, data[col++]);

		strlcpy(t.szPointOn, data[col++], sizeof(t.szPointOn));
		strlcpy(t.szPointPoly, data[col++], sizeof(t.szPointPoly));
		strlcpy(t.szSPCostPoly, data[col++], sizeof(t.szSPCostPoly));
		strlcpy(t.szDurationPoly, data[col++], sizeof(t.szDurationPoly));
		strlcpy(t.szDurationSPCostPoly, data[col++], sizeof(t.szDurationSPCostPoly));
		strlcpy(t.szCooldownPoly, data[col++], sizeof(t.szCooldownPoly));
		strlcpy(t.szMasterBonusPoly, data[col++], sizeof(t.szMasterBonusPoly));

		str_to_number(t.dwFlag, data[col++]);
		str_to_number(t.dwAffectFlag, data[col++]);

		strlcpy(t.szPointOn2, data[col++], sizeof(t.szPointOn2));
		strlcpy(t.szPointPoly2, data[col++], sizeof(t.szPointPoly2));
		strlcpy(t.szDurationPoly2, data[col++], sizeof(t.szDurationPoly2));
		str_to_number(t.dwAffectFlag2, data[col++]);

		// ADD_GRANDMASTER_SKILL
		strlcpy(t.szPointOn3, data[col++], sizeof(t.szPointOn3));
		strlcpy(t.szPointPoly3, data[col++], sizeof(t.szPointPoly3));
		strlcpy(t.szDurationPoly3, data[col++], sizeof(t.szDurationPoly3));

		strlcpy(t.szGrandMasterAddSPCostPoly, data[col++], sizeof(t.szGrandMasterAddSPCostPoly));
		// END_OF_ADD_GRANDMASTER_SKILL

		str_to_number(t.bLevelStep, data[col++]);
		str_to_number(t.bLevelLimit, data[col++]);
		str_to_number(t.preSkillVnum, data[col++]);
		str_to_number(t.preSkillLevel, data[col++]);

		str_to_number(t.lMaxHit, data[col++]);

		strlcpy(t.szSplashAroundDamageAdjustPoly, data[col++], sizeof(t.szSplashAroundDamageAdjustPoly));

		str_to_number(t.bSkillAttrType, data[col++]);
		str_to_number(t.dwTargetRange, data[col++]);

		LOG_INFO("SKILL: #{} {} flag {} point {} affect {} cooldown {}", t.dwVnum, t.szName, t.dwFlag, t.szPointOn, t.dwAffectFlag, t.szCooldownPoly);

		m_vec_skillTable.push_back(t);
	}

	return true;
}

bool CClientManager::InitializeBanwordTable()
{
	m_vec_banwordTable.clear();

	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery("SELECT word FROM banword"));

	SQLResult* pRes = pkMsg->Get();

	if (pRes->uiNumRows == 0)
		return true;

	MYSQL_ROW data;

	while ((data = pRes->FetchRow()))
	{
		TBanwordTable t;

		if (data[0])
		{
			strlcpy(t.szWord, data[0], sizeof(t.szWord));
			m_vec_banwordTable.push_back(t);
		}
	}

	LOG_INFO("BANWORD: total {}", m_vec_banwordTable.size());
	return true;
}

bool CClientManager::InitializeLandTable()
{
	using namespace building;

	char query[4096];

	snprintf(query, sizeof(query),
		"SELECT id, map_index, x, y, width, height, guild_id, guild_level_limit, price "
		"FROM land%s WHERE enable='YES' ORDER BY id",
		GetTablePostfix());

	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery(query));
	SQLResult* pRes = pkMsg->Get();

	if (!m_vec_kLandTable.empty())
	{
		LOG_INFO("RELOAD: land");
		m_vec_kLandTable.clear();
	}

	m_vec_kLandTable.reserve(pRes->uiNumRows);

	MYSQL_ROW	data;

	if (pRes->uiNumRows > 0)
		while ((data = pRes->FetchRow()))
		{
			TLand t = {};

			int col = 0;

			str_to_number(t.dwID, data[col++]);
			str_to_number(t.lMapIndex, data[col++]);
			str_to_number(t.x, data[col++]);
			str_to_number(t.y, data[col++]);
			str_to_number(t.width, data[col++]);
			str_to_number(t.height, data[col++]);
			str_to_number(t.dwGuildID, data[col++]);
			str_to_number(t.bGuildLevelLimit, data[col++]);
			str_to_number(t.dwPrice, data[col++]);

			LOG_INFO("LAND: {} map {:<4} {:7}x{:<7} w {:<4} h {:<4}", t.dwID, t.lMapIndex, t.x, t.y, t.width, t.height);

			m_vec_kLandTable.push_back(t);
		}

	return true;
}

void parse_pair_number_string(const char* c_pszString, std::vector<std::pair<int, int> >& vec)
{
	// format: 10,1/20,3/300,50
	const char* t = c_pszString;
	const char* p = strchr(t, '/');
	std::pair<int, int> k;

	char szNum[32 + 1];
	char* comma;

	while (p)
	{
		if (isnhdigit(*t))
		{
			strlcpy(szNum, t, MIN(sizeof(szNum), (p - t) + 1));

			comma = strchr(szNum, ',');

			if (comma)
			{
				*comma = '\0';
				str_to_number(k.second, comma + 1);
			}
			else
				k.second = 0;

			str_to_number(k.first, szNum);
			vec.push_back(k);
		}

		t = p + 1;
		p = strchr(t, '/');
	}

	if (isnhdigit(*t))
	{
		strlcpy(szNum, t, sizeof(szNum));

		comma = strchr(const_cast<char*>(t), ',');

		if (comma)
		{
			*comma = '\0';
			str_to_number(k.second, comma + 1);
		}
		else
			k.second = 0;

		str_to_number(k.first, szNum);
		vec.push_back(k);
	}
}

bool CClientManager::InitializeObjectProto()
{
	using namespace building;

	char query[4096];
	snprintf(query, sizeof(query),
		"SELECT vnum, price, materials, upgrade_vnum, upgrade_limit_time, life, reg_1, reg_2, reg_3, reg_4, npc, group_vnum, dependent_group "
		"FROM object_proto%s ORDER BY vnum",
		GetTablePostfix());

	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery(query));
	SQLResult* pRes = pkMsg->Get();

	if (!m_vec_kObjectProto.empty())
	{
		LOG_INFO("RELOAD: object_proto");
		m_vec_kObjectProto.clear();
	}

	m_vec_kObjectProto.reserve(MAX(0, pRes->uiNumRows));

	MYSQL_ROW	data;

	if (pRes->uiNumRows > 0)
		while ((data = pRes->FetchRow()))
		{
			TObjectProto t = {};

			int col = 0;

			str_to_number(t.dwVnum, data[col++]);
			str_to_number(t.dwPrice, data[col++]);

			std::vector<std::pair<int, int> > vec;
			parse_pair_number_string(data[col++], vec);

			for (unsigned int i = 0; i < OBJECT_MATERIAL_MAX_NUM && i < vec.size(); ++i)
			{
				std::pair<int, int>& r = vec[i];

				t.kMaterials[i].dwItemVnum = r.first;
				t.kMaterials[i].dwCount = r.second;
			}

			str_to_number(t.dwUpgradeVnum, data[col++]);
			str_to_number(t.dwUpgradeLimitTime, data[col++]);
			str_to_number(t.lLife, data[col++]);
			str_to_number(t.lRegion[0], data[col++]);
			str_to_number(t.lRegion[1], data[col++]);
			str_to_number(t.lRegion[2], data[col++]);
			str_to_number(t.lRegion[3], data[col++]);

			// ADD_BUILDING_NPC
			str_to_number(t.dwNPCVnum, data[col++]);
			str_to_number(t.dwGroupVnum, data[col++]);
			str_to_number(t.dwDependOnGroupVnum, data[col++]);

			t.lNPCX = 0;
			t.lNPCY = MAX(t.lRegion[1], t.lRegion[3]) + 300;
			// END_OF_ADD_BUILDING_NPC

			LOG_INFO("OBJ_PROTO: vnum {} price {} mat {} {}", t.dwVnum, t.dwPrice, t.kMaterials[0].dwItemVnum, t.kMaterials[0].dwCount);

			m_vec_kObjectProto.push_back(t);
		}

	return true;
}

bool CClientManager::InitializeObjectTable()
{
	char query[4096];
	snprintf(query, sizeof(query), "SELECT id, land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot, life FROM object%s ORDER BY id", GetTablePostfix());

	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery(query));
	return ReadObjectTable(pkMsg.get(), false);
}

bool CClientManager::ReadObjectTable(SQLMsg* message, bool publish)
{
	using namespace building;
	if (!message || message->uiSQLErrno || !message->Get())
		return false;
	SQLResult* pRes = message->Get();
	if (!publish && !m_map_pkObjectTable.empty())
	{
		LOG_INFO("RELOAD: object");
		for (const auto& entry : m_map_pkObjectTable)
			delete entry.second;
		m_map_pkObjectTable.clear();
	}

	MYSQL_ROW data;

	if (pRes->uiNumRows > 0)
		while ((data = pRes->FetchRow()))
		{
			uint32_t id = 0;
			str_to_number(id, data[0]);
			if (!id || m_map_pkObjectTable.contains(id))
				continue;
			TObject* k = new TObject{};

			int col = 1;

			k->dwID = id;
			str_to_number(k->dwLandID, data[col++]);
			str_to_number(k->dwVnum, data[col++]);
			str_to_number(k->lMapIndex, data[col++]);
			str_to_number(k->x, data[col++]);
			str_to_number(k->y, data[col++]);
			str_to_number(k->xRot, data[col++]);
			str_to_number(k->yRot, data[col++]);
			str_to_number(k->zRot, data[col++]);
			str_to_number(k->lLife, data[col++]);

			LOG_INFO("OBJ: {} vnum {} map {:<4} {:7}x{:<7} life {}", k->dwID, k->dwVnum, k->lMapIndex, k->x, k->y, k->lLife);

			m_map_pkObjectTable.insert(std::make_pair(k->dwID, k));
			if (publish)
				ForwardPacket(HEADER_DG_CREATE_OBJECT, k, sizeof(TObject));
		}

	return true;
}

#ifdef ENABLE_ITEM_EXTRA_PROTO

#endif


bool CClientManager::MirrorMobTableIntoDB()
{
#ifdef ENABLE_MULTILANGUAGE_SYSTEM
	for (itertype(m_vec_mobTable) it = m_vec_mobTable.begin(); it != m_vec_mobTable.end(); it++)
	{
		const TMobTable& t = *it;
		char query[QUERY_MAX_LEN];
		int len = snprintf(query, sizeof(query),
			"replace into mob_proto "
			"("
			"vnum, name, ");
		for (int i = 0; i < LANGUAGE_MAX_NUM; ++i)
		{
			len += snprintf(query + len, sizeof(query) - len, "locale_name_%s, ", languages[i].c_str());
		}
		len += snprintf(query + len, sizeof(query) - len, "type, rank, battle_type, level, size, ai_flag, setRaceFlag, setImmuneFlag, "
			"on_click, empire, drop_item, resurrection_vnum, folder, "
			"st, dx, ht, iq, damage_min, damage_max, max_hp, regen_cycle, regen_percent, exp, "
			"gold_min, gold_max, def, attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item, "

			"enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate, "
			"resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow, "
			"resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, "
			"dam_multiply, summon, drain_sp, "

			"skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, "
			"skill_vnum3, skill_level3, skill_vnum4, skill_level4, "
			"sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive"
			") "
			"values ("

			"%d, \"%s\", ", t.dwVnum, t.szName);

		for (int i = 0; i < LANGUAGE_MAX_NUM; ++i)
			len += snprintf(query + len, sizeof(query) - len, "\"%s\", ", t.szLocaleName[i]);
		len += snprintf(query + len, sizeof(query) - len, "%d, %d, %d, %d, %d, %u, %u, %u, "
			"%d, %d, %d, %d, '%s', "
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, "
			"%d, %d, %d, %d, %d, %d, %d, %d, %d, "

			"%d, %d, %d, %d, %d, %d, "
			"%d, %d, %d, %d, %d, %d, "
			"%d, %d, %d, %d, %d, "
			"%f, %d, %d, "

			"%d, %d, %d, %d, %d, %d, "
			"%d, %d, %d, %d, "
			"%d, %d, %d, %d, %d"
			")",
			t.bType, t.bRank, t.bBattleType, t.bLevel, t.bSize, t.dwAIFlag, t.dwRaceFlag, t.dwImmuneFlag,
			t.bOnClickType, t.bEmpire, t.dwDropItemVnum, t.dwResurrectionVnum, t.szFolder,
			t.bStr, t.bDex, t.bCon, t.bInt, t.dwDamageRange[0], t.dwDamageRange[1], static_cast<unsigned long long>(t.dwMaxHP), t.bRegenCycle, t.bRegenPercent, t.dwExp,

			t.dwGoldMin, t.dwGoldMax, t.wDef, t.sAttackSpeed, t.sMovingSpeed, t.bAggresiveHPPct, t.wAggressiveSight, t.wAttackRange, t.dwPolymorphItemVnum,
			t.cEnchants[0], t.cEnchants[1], t.cEnchants[2], t.cEnchants[3], t.cEnchants[4], t.cEnchants[5],
			t.cResists[0], t.cResists[1], t.cResists[2], t.cResists[3], t.cResists[4], t.cResists[5],
			t.cResists[6], t.cResists[7], t.cResists[8], t.cResists[9], t.cResists[10],
			t.fDamMultiply, t.dwSummonVnum, t.dwDrainSP,

			t.Skills[0].dwVnum, t.Skills[0].bLevel, t.Skills[1].dwVnum, t.Skills[1].bLevel, t.Skills[2].dwVnum, t.Skills[2].bLevel,
			t.Skills[3].dwVnum, t.Skills[3].bLevel, t.Skills[4].dwVnum, t.Skills[4].bLevel,
			t.bBerserkPoint, t.bStoneSkinPoint, t.bGodSpeedPoint, t.bDeathBlowPoint, t.bRevivePoint
		);

		CDBManager::instance().AsyncQuery(query);
	}
	return true;
#else
	for (auto it = m_vec_mobTable.begin(); it != m_vec_mobTable.end(); ++it)
	{
		const TMobTable& t = *it;
		char query[4096];
		if (g_stLocaleNameColumn == "name")
		{
			snprintf(query, sizeof(query),
				"replace into mob_proto%s "
				"("
				"vnum, name, type, rank, battle_type, level, size, ai_flag, setRaceFlag, setImmuneFlag, "
				"on_click, empire, drop_item, resurrection_vnum, folder, "
				"st, dx, ht, iq, damage_min, damage_max, max_hp, regen_cycle, regen_percent, exp, "
				"gold_min, gold_max, def, attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item, "

				"enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate, "
				"resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow, "
				"resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, "
				"dam_multiply, summon, drain_sp, "

				"skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, "
				"skill_vnum3, skill_level3, skill_vnum4, skill_level4, "
				"sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive"
				") "
				"values ("

				"%u, \"%s\", %u, %u, %u, %u, %u, %u, %u, %u, "
				"%u, %u, %u, %u, '%s', "
				"%u, %u, %u, %u, %u, %u, %llu, %u, %u, %u, "
				"%u, %u, %d, %d, %d, %u, %d, %d, %u, "

				"%d, %d, %d, %d, %d, %d, "
				"%d, %d, %d, %d, %d, %d, "
				"%d, %d, %d, %d, %d, "
				"%f, %u, %u, "

				"%u, %u, %u, %u, %u, %u, "
				"%u, %u, %u, %u, "
				"%u, %u, %u, %u, %u"
				")",
				GetTablePostfix(),
				t.dwVnum, t.szName,
				t.bType, t.bRank, t.bBattleType, t.bLevel, t.bSize, t.dwAIFlag, t.dwRaceFlag, t.dwImmuneFlag,
				t.bOnClickType, t.bEmpire, t.dwDropItemVnum, t.dwResurrectionVnum, t.szFolder,
				t.bStr, t.bDex, t.bCon, t.bInt, t.dwDamageRange[0], t.dwDamageRange[1], static_cast<unsigned long long>(t.dwMaxHP), t.bRegenCycle, t.bRegenPercent, t.dwExp,

				t.dwGoldMin, t.dwGoldMax, t.wDef, t.sAttackSpeed, t.sMovingSpeed, t.bAggresiveHPPct, t.wAggressiveSight, t.wAttackRange, t.dwPolymorphItemVnum,
				t.cEnchants[0], t.cEnchants[1], t.cEnchants[2], t.cEnchants[3], t.cEnchants[4], t.cEnchants[5],
				t.cResists[0], t.cResists[1], t.cResists[2], t.cResists[3], t.cResists[4], t.cResists[5],
				t.cResists[6], t.cResists[7], t.cResists[8], t.cResists[9], t.cResists[10],
				t.fDamMultiply, t.dwSummonVnum, t.dwDrainSP,

				t.Skills[0].dwVnum, t.Skills[0].bLevel, t.Skills[1].dwVnum, t.Skills[1].bLevel, t.Skills[2].dwVnum, t.Skills[2].bLevel,
				t.Skills[3].dwVnum, t.Skills[3].bLevel, t.Skills[4].dwVnum, t.Skills[4].bLevel,
				t.bBerserkPoint, t.bStoneSkinPoint, t.bGodSpeedPoint, t.bDeathBlowPoint, t.bRevivePoint
			);
		}
		else
		{
			snprintf(query, sizeof(query),
				"replace into mob_proto%s "
				"("
				"vnum, name, %s, type, rank, battle_type, level, size, ai_flag, setRaceFlag, setImmuneFlag, "
				"on_click, empire, drop_item, resurrection_vnum, folder, "
				"st, dx, ht, iq, damage_min, damage_max, max_hp, regen_cycle, regen_percent, exp, "
				"gold_min, gold_max, def, attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item, "

				"enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate, "
				"resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow, "
				"resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, "
				"dam_multiply, summon, drain_sp, "

				"skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, "
				"skill_vnum3, skill_level3, skill_vnum4, skill_level4, "
				"sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive"
				") "
				"values ("

				"%u, \"%s\", \"%s\", %u, %u, %u, %u, %u, %u, %u, %u, "
				"%u, %u, %u, %u, '%s', "
				"%u, %u, %u, %u, %u, %u, %llu, %u, %u, %u, "
				"%u, %u, %d, %d, %d, %u, %d, %d, %u, "

				"%d, %d, %d, %d, %d, %d, "
				"%d, %d, %d, %d, %d, %d, "
				"%d, %d, %d, %d, %d, "
				"%f, %u, %u, "

				"%u, %u, %u, %u, %u, %u, "
				"%u, %u, %u, %u, "
				"%u, %u, %u, %u, %u"
				")",
				GetTablePostfix(),
				g_stLocaleNameColumn.c_str(),
				t.dwVnum, t.szName,
#ifdef ENABLE_MULTI_NAMES
				t.szLocaleName[0],
#else
				t.szLocaleName,
#endif
				t.bType, t.bRank, t.bBattleType, t.bLevel, t.bSize, t.dwAIFlag, t.dwRaceFlag, t.dwImmuneFlag,
				t.bOnClickType, t.bEmpire, t.dwDropItemVnum, t.dwResurrectionVnum, t.szFolder,
				t.bStr, t.bDex, t.bCon, t.bInt, t.dwDamageRange[0], t.dwDamageRange[1], static_cast<unsigned long long>(t.dwMaxHP), t.bRegenCycle, t.bRegenPercent, t.dwExp,

				t.dwGoldMin, t.dwGoldMax, t.wDef, t.sAttackSpeed, t.sMovingSpeed, t.bAggresiveHPPct, t.wAggressiveSight, t.wAttackRange, t.dwPolymorphItemVnum,
				t.cEnchants[0], t.cEnchants[1], t.cEnchants[2], t.cEnchants[3], t.cEnchants[4], t.cEnchants[5],
				t.cResists[0], t.cResists[1], t.cResists[2], t.cResists[3], t.cResists[4], t.cResists[5],
				t.cResists[6], t.cResists[7], t.cResists[8], t.cResists[9], t.cResists[10],
				t.fDamMultiply, t.dwSummonVnum, t.dwDrainSP,

				t.Skills[0].dwVnum, t.Skills[0].bLevel, t.Skills[1].dwVnum, t.Skills[1].bLevel, t.Skills[2].dwVnum, t.Skills[2].bLevel,
				t.Skills[3].dwVnum, t.Skills[3].bLevel, t.Skills[4].dwVnum, t.Skills[4].bLevel,
				t.bBerserkPoint, t.bStoneSkinPoint, t.bGodSpeedPoint, t.bDeathBlowPoint, t.bRevivePoint
			);
		}

		CDBManager::instance().AsyncQuery(query);
	}
	return true;
#endif
}

#ifdef ENABLE_PROTO_FROM_DB
#define VERIFY_IFIELD(x,y) do { if (data[x]!=NULL && data[x][0]!='\0') { std::remove_cv_t<std::remove_reference_t<decltype(y)>> parsedField {}; str_to_number(parsedField, data[x]); y = parsedField; } } while (0);
#define VERIFY_SFIELD(x,y) if (data[x]!=NULL && data[x][0]!='\0') strlcpy(y, data[x], sizeof(y));

#define ENABLE_AUTODETECT_VNUMRANGE

namespace MProto
{
	enum MProtoT
	{
		vnum, name,
#ifdef ENABLE_MULTI_NAMES
		locale_name_en, locale_name_ro, locale_name_it, locale_name_tr, locale_name_de, locale_name_pl, locale_name_pt, locale_name_es, locale_name_cz, locale_name_hu,
#else
		locale_name,
#endif
		type, rank, battle_type, level, size,
		ai_flag, setRaceFlag, setImmuneFlag, on_click, empire, drop_item,
		resurrection_vnum, folder, st, dx, ht, iq, damage_min, damage_max, max_hp,
		regen_cycle, regen_percent, exp, gold_min, gold_max, def,
		attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item,
		enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate,
		resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow,
		resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, dam_multiply, summon, drain_sp,
		skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, skill_vnum3, skill_level3,
		skill_vnum4, skill_level4, sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive
	};
}

bool CClientManager::InitializeMobTableFromDB()
{
#ifdef ENABLE_MULTI_NAMES
	char query[3072];
#else
	char query[2048];
#endif

	LOG_INFO("Loading mob_proto from MySQL");
#ifdef ENABLE_MULTI_NAMES
	int iQueryLen = snprintf(query, sizeof(query), "SELECT vnum, name, ");
	for (int i = 0; i < LANGUAGE_MAX_NUM - 1; i++) {
		iQueryLen += snprintf(query + iQueryLen, sizeof(query) - iQueryLen, "locale_name_%s, ", languages[i].c_str());
	}

	iQueryLen += snprintf(query + iQueryLen, sizeof(query) - iQueryLen, "type, rank, battle_type, level, size+0,"
		" ai_flag+0, setRaceFlag+0, setImmuneFlag+0, on_click, empire, drop_item,"
		" resurrection_vnum, folder, st, dx, ht, iq, damage_min, damage_max, max_hp,"
		" regen_cycle, regen_percent, exp, gold_min, gold_max, def,"
		" attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item,"
		" enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate,"
		" resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow,"
		" resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, dam_multiply, summon, drain_sp,"
		" skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, skill_vnum3, skill_level3,"
		" skill_vnum4, skill_level4, sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive"
		" FROM mob_proto%s ORDER BY vnum;", GetTablePostfix());
#else
	snprintf(query, sizeof(query),
		"SELECT vnum, name, %s, type, rank, battle_type, level, size+0,"
		" ai_flag+0, setRaceFlag+0, setImmuneFlag+0, on_click, empire, drop_item,"
		" resurrection_vnum, folder, st, dx, ht, iq, damage_min, damage_max, max_hp,"
		" regen_cycle, regen_percent, exp, gold_min, gold_max, def,"
		" attack_speed, move_speed, aggressive_hp_pct, aggressive_sight, attack_range, polymorph_item,"
		" enchant_curse, enchant_slow, enchant_poison, enchant_stun, enchant_critical, enchant_penetrate,"
		" resist_sword, resist_twohand, resist_dagger, resist_bell, resist_fan, resist_bow,"
		" resist_fire, resist_elect, resist_magic, resist_wind, resist_poison, dam_multiply, summon, drain_sp,"
		" skill_vnum0, skill_level0, skill_vnum1, skill_level1, skill_vnum2, skill_level2, skill_vnum3, skill_level3,"
		" skill_vnum4, skill_level4, sp_berserk, sp_stoneskin, sp_godspeed, sp_deathblow, sp_revive"
		" FROM mob_proto%s ORDER BY vnum;",
		g_stLocaleNameColumn.c_str(),
		GetTablePostfix()
	);
#endif
	std::unique_ptr<SQLMsg> pkMsg(CDBManager::instance().DirectQuery(query));
	SQLResult* pRes = pkMsg->Get();

	uint32_t addNumber = pRes->uiNumRows;
	if (addNumber == 0)
		return false;

	if (!m_vec_mobTable.empty())
	{
		LOG_INFO("RELOAD: mob_proto");
		m_vec_mobTable.clear();
	}

	m_vec_mobTable.resize(addNumber);
	memset(m_vec_mobTable.data(), 0, sizeof(TMobTable) * m_vec_mobTable.size());
	TMobTable* mob_table = m_vec_mobTable.data();

	MYSQL_ROW data = nullptr;
	while ((data = pRes->FetchRow()))
	{
		// check whether or not the field is NULL or that contains an empty string
		// ## GENERAL
		VERIFY_IFIELD(MProto::vnum, mob_table->dwVnum)
		VERIFY_SFIELD(MProto::name, mob_table->szName)
#ifdef ENABLE_MULTI_NAMES
		VERIFY_SFIELD(MProto::locale_name_en, mob_table->szLocaleName[1])
		VERIFY_SFIELD(MProto::locale_name_ro, mob_table->szLocaleName[2])
		VERIFY_SFIELD(MProto::locale_name_it, mob_table->szLocaleName[3])
		VERIFY_SFIELD(MProto::locale_name_tr, mob_table->szLocaleName[4])
		VERIFY_SFIELD(MProto::locale_name_de, mob_table->szLocaleName[5])
		VERIFY_SFIELD(MProto::locale_name_pl, mob_table->szLocaleName[6])
		VERIFY_SFIELD(MProto::locale_name_pt, mob_table->szLocaleName[7])
		VERIFY_SFIELD(MProto::locale_name_es, mob_table->szLocaleName[8])
		VERIFY_SFIELD(MProto::locale_name_cz, mob_table->szLocaleName[9])
		VERIFY_SFIELD(MProto::locale_name_hu, mob_table->szLocaleName[10])
#else
		VERIFY_SFIELD(MProto::locale_name, mob_table->szLocaleName);
#endif
		VERIFY_IFIELD(MProto::rank, mob_table->bRank)
		VERIFY_IFIELD(MProto::type, mob_table->bType)
		VERIFY_IFIELD(MProto::battle_type, mob_table->bBattleType)
		VERIFY_IFIELD(MProto::level, mob_table->bLevel)
		VERIFY_IFIELD(MProto::size, mob_table->bSize)

		// ## FLAG
		VERIFY_IFIELD(MProto::ai_flag, mob_table->dwAIFlag)
		VERIFY_IFIELD(MProto::setRaceFlag, mob_table->dwRaceFlag)
		VERIFY_IFIELD(MProto::setImmuneFlag, mob_table->dwImmuneFlag)

		// ## OTHERS
		VERIFY_IFIELD(MProto::empire, mob_table->bEmpire)
		VERIFY_SFIELD(MProto::folder, mob_table->szFolder)
		VERIFY_IFIELD(MProto::on_click, mob_table->bOnClickType)
		VERIFY_IFIELD(MProto::st, mob_table->bStr)
		VERIFY_IFIELD(MProto::dx, mob_table->bDex)
		VERIFY_IFIELD(MProto::ht, mob_table->bCon)
		VERIFY_IFIELD(MProto::iq, mob_table->bInt)
		VERIFY_IFIELD(MProto::damage_min, mob_table->dwDamageRange[0])
		VERIFY_IFIELD(MProto::damage_max, mob_table->dwDamageRange[1])
		VERIFY_IFIELD(MProto::max_hp, mob_table->dwMaxHP)
		VERIFY_IFIELD(MProto::regen_cycle, mob_table->bRegenCycle)
		VERIFY_IFIELD(MProto::regen_percent, mob_table->bRegenPercent)
		VERIFY_IFIELD(MProto::gold_min, mob_table->dwGoldMin)
		VERIFY_IFIELD(MProto::gold_max, mob_table->dwGoldMax)
		VERIFY_IFIELD(MProto::exp, mob_table->dwExp)
		VERIFY_IFIELD(MProto::def, mob_table->wDef)
		VERIFY_IFIELD(MProto::attack_speed, mob_table->sAttackSpeed)
		VERIFY_IFIELD(MProto::move_speed, mob_table->sMovingSpeed)
		VERIFY_IFIELD(MProto::aggressive_hp_pct, mob_table->bAggresiveHPPct)
		VERIFY_IFIELD(MProto::aggressive_sight, mob_table->wAggressiveSight)
		VERIFY_IFIELD(MProto::attack_range, mob_table->wAttackRange)
		VERIFY_IFIELD(MProto::drop_item, mob_table->dwDropItemVnum)
		VERIFY_IFIELD(MProto::resurrection_vnum, mob_table->dwResurrectionVnum)

		// ## ENCHANT 6
		VERIFY_IFIELD(MProto::enchant_curse, mob_table->cEnchants[MOB_ENCHANT_CURSE])
		VERIFY_IFIELD(MProto::enchant_slow, mob_table->cEnchants[MOB_ENCHANT_SLOW])
		VERIFY_IFIELD(MProto::enchant_poison, mob_table->cEnchants[MOB_ENCHANT_POISON])
		VERIFY_IFIELD(MProto::enchant_stun, mob_table->cEnchants[MOB_ENCHANT_STUN])
		VERIFY_IFIELD(MProto::enchant_critical, mob_table->cEnchants[MOB_ENCHANT_CRITICAL])
		VERIFY_IFIELD(MProto::enchant_penetrate, mob_table->cEnchants[MOB_ENCHANT_PENETRATE])

		// ## RESIST 11
		VERIFY_IFIELD(MProto::resist_sword, mob_table->cResists[MOB_RESIST_SWORD])
		VERIFY_IFIELD(MProto::resist_twohand, mob_table->cResists[MOB_RESIST_TWOHAND])
		VERIFY_IFIELD(MProto::resist_dagger, mob_table->cResists[MOB_RESIST_DAGGER])
		VERIFY_IFIELD(MProto::resist_bell, mob_table->cResists[MOB_RESIST_BELL])
		VERIFY_IFIELD(MProto::resist_fan, mob_table->cResists[MOB_RESIST_FAN])
		VERIFY_IFIELD(MProto::resist_bow, mob_table->cResists[MOB_RESIST_BOW])
		VERIFY_IFIELD(MProto::resist_fire, mob_table->cResists[MOB_RESIST_FIRE])
		VERIFY_IFIELD(MProto::resist_elect, mob_table->cResists[MOB_RESIST_ELECT])
		VERIFY_IFIELD(MProto::resist_magic, mob_table->cResists[MOB_RESIST_MAGIC])
		VERIFY_IFIELD(MProto::resist_wind, mob_table->cResists[MOB_RESIST_WIND])
		VERIFY_IFIELD(MProto::resist_poison, mob_table->cResists[MOB_RESIST_POISON])

		// ## OTHERS #2
		VERIFY_IFIELD(MProto::dam_multiply, mob_table->fDamMultiply)
		VERIFY_IFIELD(MProto::summon, mob_table->dwSummonVnum)
		VERIFY_IFIELD(MProto::drain_sp, mob_table->dwDrainSP)

		VERIFY_IFIELD(MProto::polymorph_item, mob_table->dwPolymorphItemVnum)

		VERIFY_IFIELD(MProto::skill_vnum0, mob_table->Skills[0].dwVnum)
		VERIFY_IFIELD(MProto::skill_level0, mob_table->Skills[0].bLevel)
		VERIFY_IFIELD(MProto::skill_vnum1, mob_table->Skills[1].dwVnum)
		VERIFY_IFIELD(MProto::skill_level1, mob_table->Skills[1].bLevel)
		VERIFY_IFIELD(MProto::skill_vnum2, mob_table->Skills[2].dwVnum)
		VERIFY_IFIELD(MProto::skill_level2, mob_table->Skills[2].bLevel)
		VERIFY_IFIELD(MProto::skill_vnum3, mob_table->Skills[3].dwVnum)
		VERIFY_IFIELD(MProto::skill_level3, mob_table->Skills[3].bLevel)
		VERIFY_IFIELD(MProto::skill_vnum4, mob_table->Skills[4].dwVnum)
		VERIFY_IFIELD(MProto::skill_level4, mob_table->Skills[4].bLevel)

		// ## SPECIAL
		VERIFY_IFIELD(MProto::sp_berserk, mob_table->bBerserkPoint)
		VERIFY_IFIELD(MProto::sp_stoneskin, mob_table->bStoneSkinPoint)
		VERIFY_IFIELD(MProto::sp_godspeed, mob_table->bGodSpeedPoint)
		VERIFY_IFIELD(MProto::sp_deathblow, mob_table->bDeathBlowPoint)
		VERIFY_IFIELD(MProto::sp_revive, mob_table->bRevivePoint)

#ifdef ENABLE_MULTI_NAMES
		const auto* mobLocaleName = mob_table->szLocaleName[0];
#else
		const auto* mobLocaleName = mob_table->szLocaleName;
#endif
		LOG_INFO("MOB #{:<5} {:<24} {:<24} level: {:<3} rank: {} empire: {}",
			mob_table->dwVnum,
			mob_table->szName,
			mobLocaleName,
			mob_table->bLevel,
			mob_table->bRank,
			mob_table->bEmpire);
		++mob_table;
	}
	sort(m_vec_mobTable.begin(), m_vec_mobTable.end(), FCompareVnum());

	LOG_INFO("Complete! {} Mobs loaded.", addNumber);
	return true;
}

namespace IProto
{
	enum IProtoT
	{
		vnum, type, subtype, name,
#ifdef ENABLE_MULTI_NAMES
		locale_name_en, locale_name_ro, locale_name_it, locale_name_tr, locale_name_de, locale_name_pl, locale_name_pt, locale_name_es, locale_name_cz, locale_name_hu,
#else
		locale_name,
#endif
		gold, shop_buy_price, weight, size,
		flag, wearflag, antiflag, immuneflag, refined_vnum, refine_set, magic_pct,
		socket_pct, addon_type, limittype0, limitvalue0, limittype1, limitvalue1,
#ifdef ENABLE_APPLYTYPE5_RAZOR93
		applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, applytype3, applyvalue3, applytype4, applyvalue4,
#else
		applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2,
#endif
		value0, value1, value2, value3, value4, value5
#if !defined(ENABLE_AUTODETECT_VNUMRANGE)
		, vnum_range
#endif
	};
}


#endif
