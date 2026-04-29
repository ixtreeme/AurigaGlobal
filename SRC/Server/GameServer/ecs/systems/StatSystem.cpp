#include "../../stdafx.h"

#include "StatSystem.hpp"

#include <common/VnumHelper.h>

#include "../EntityFactory.hpp"
#include "../AIHelpers.hpp"
#include "../Registry.hpp"
#include "ItemSystem.hpp"
#include "PointSystem.hpp"

#include "../../char.h"


#include "../../config.h"
#include "../../utils.h"
#include "../../crc32.h"
#include "../../char_manager.h"
#include "../../desc_client.h"
#include "../../desc_manager.h"
#include "../../buffer_manager.h"
#include "../../item_manager.h"
#include "../../motion.h"
#include "../../vector.h"
#include "../../packet.h"
#include "../../cmd.h"
#include "../../fishing.h"
#include "../../exchange.h"
#include "../../battle.h"
#include "../../affect.h"
#include "../../shop.h"
#include "../../shop_manager.h"
#include "../../safebox.h"
#include "../../MountInventory.h"
#include "../../regen.h"
#include "../../pvp.h"
#include "../../party.h"
#include "../../start_position.h"
#include "../../questmanager.h"
#include "../../log.h"
#include "../../p2p.h"
#include "../../guild.h"
#include "../../guild_manager.h"
#include "../../dungeon.h"
#include "../../messenger_manager.h"
#include "../../unique_item.h"
#include "../../priv_manager.h"
#include "../../war_map.h"
#include "../../banword.h"
#include "../../target.h"
#include "../../wedding.h"
#include "../../mob_manager.h"
#include "../../mining.h"
#include "../../arena.h"
#include "../../dev_log.h"
#include "../../horsename_manager.h"
#include "../../pcbang.h"
#include "../../gm.h"
#include "../../map_location.h"
#include "../../skill_power.h"
#include "../../buff_on_attributes.h"
#include "../../constants.h"
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "../../new_offlineshop.h"
#include "../../new_offlineshop_manager.h"
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#include "../../MountSystem.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "../../battle_pass.h"
#endif

#ifdef __PET_SYSTEM__
#include "../../PetSystem.h"
#endif
#ifdef __NEWPET_SYSTEM__
#include "../../New_PetSystem.h"
#endif
#include <boost/algorithm/string/find.hpp>

#include "../../DragonSoul.h"
#include <common/CommonDefines.h>

#include "../../../Poly/Constants.h"
#ifdef __SEND_TARGET_INFO__
#include <algorithm>
#include <iterator>
#endif
#ifdef ENABLE_SWITCHBOT
#include "../../new_switchbot.h"
#endif
#ifdef ENABLE_RUNE_SYSTEM
#include <common/rune_length.h>
#endif
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif
#include "../../mount_inventory_helper.h"
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "../../OrcsDungeon.h"
#include "../../TritonTempleDungeon.h"
#include "../../ValentineDungeon.h"
#include "../../RuneDungeon.h"
#include "../../PyramidDungeonRazor93.h"
#include "../../NightmareDungeonRazor93.h"
#include "../../Halloween2022Dungeon.h"
#include "../../VikingDungeon.h"
#include "../../EasterDungeon.h"
#endif

void CHARACTER::ComputeBattlePoints()
{
	if (IsPolymorphed())
	{
		uint32_t dwMobVnum = GetPolymorphVnum();
		const CMob* pMob = CMobManager::instance().Get(dwMobVnum);
		int iAtt = 0;
		int iDef = 0;

		if (pMob)
		{
			iAtt = GetLevel() * 2 + GetPolymorphPoint(POINT_ST) * 2;
			// lev + con
			iDef = GetLevel() + GetPolymorphPoint(POINT_HT) + pMob->m_table.wDef;
		}

		SetPoint(POINT_ATT_GRADE, iAtt);
		SetPoint(POINT_DEF_GRADE, iDef);
		SetPoint(POINT_MAGIC_ATT_GRADE, GetPoint(POINT_ATT_GRADE));
		SetPoint(POINT_MAGIC_DEF_GRADE, GetPoint(POINT_DEF_GRADE));
	}
	else if (IsPC())
	
	{
		SetPoint(POINT_ATT_GRADE, 0);
		SetPoint(POINT_DEF_GRADE, 0);
		SetPoint(POINT_CLIENT_DEF_GRADE, 0);
		SetPoint(POINT_MAGIC_ATT_GRADE, GetPoint(POINT_ATT_GRADE));
		SetPoint(POINT_MAGIC_DEF_GRADE, GetPoint(POINT_DEF_GRADE));

		//
		// ±âo» ATK = 2lev + 2str, Á÷3÷?! ¸¶´U 2strAo 1U2? 1ö AÖA1
		//
		int iAtk = GetLevel() * 2;
		int iStatAtk = 0;

		switch (GetJob())
		{
		case JOB_WARRIOR:
		case JOB_SURA:
			iStatAtk = (2 * GetPoint(POINT_ST));
			break;

		case JOB_ASSASSIN:
			iStatAtk = (4 * GetPoint(POINT_ST) + 2 * GetPoint(POINT_DX)) / 3;
			break;

		case JOB_SHAMAN:
			iStatAtk = (4 * GetPoint(POINT_ST) + 2 * GetPoint(POINT_IQ)) / 3;
			break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
			// TODO: 1öAÎÁ· °o°Ý·Â °o1Ä ±âE1AÚ?!°Ô ?äA»
			iStatAtk = (2 * GetPoint(POINT_ST));
			break;
#endif
		default:
			sys_err("invalid job %d", GetJob());
			iStatAtk = (2 * GetPoint(POINT_ST));
			break;
		}

		// ¸»A» A¸°í AÖ°í, 1oAEA¸·Î AÎÇN °o°Ý·ÂAI ST*2 o¸´U 3·A¸¸é ST*2·Î ÇN´U.
		// 1oAEA» Aß¸o ÂiAo »ç¶÷ °o°Ý·ÂAI ´o 3·Áö 3E°Ô ÇI±â A§ÇO1­´U.
		if (GetMountVnum() && iStatAtk < 2 * GetPoint(POINT_ST))
			iStatAtk = (2 * GetPoint(POINT_ST));

		iAtk += iStatAtk;

		// 1Â¸¶(¸») : °Ë1ö¶ó µY1IÁö °¨1O
		if (GetMountVnum())
		{
			if (GetJob() == JOB_SURA && GetSkillGroup() == 1)
			{
				iAtk += (iAtk * GetHorseLevel()) / 60;
			}
			else
			{
				iAtk += (iAtk * GetHorseLevel()) / 30;
			}
		}

		//
		// ATK Setting
		//
		iAtk += GetPoint(POINT_ATT_GRADE_BONUS);

		PointChange(POINT_ATT_GRADE, iAtk);

		// DEF = LEV + CON + ARMOR
		int iShowDef = GetLevel() + GetPoint(POINT_HT); // For Ymir(Aµ¸¶)
		int iDef = GetLevel() + (int)(GetPoint(POINT_HT) / 1.25); // For Other
		int iArmor = 0;

		LPITEM pkItem;

		for (int i = 0; i < WEAR_MAX_NUM; ++i)
			if ((pkItem = GetWear(i)) && ItemSystem::GetItemType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ITEM_ARMOR)
			{
#ifdef ENABLE_PENDANT
				if (ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_BODY || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_HEAD || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_FOOTS || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_SHIELD || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_PENDANT)
#else
				if (ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_BODY || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_HEAD || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_FOOTS || ItemSystem::GetItemSubType(EntityFactory::CreateItemEntity(g_registry, pkItem)) == ARMOR_SHIELD)
#endif
				{
					iArmor += ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkItem), 1);
					iArmor += (2 * ItemSystem::GetItemValue(EntityFactory::CreateItemEntity(g_registry, pkItem), 5));
				}
			}

		// ¸» A¸°í AÖA» ¶§ 1a3î·ÂAI ¸»AÇ ±âÁO 1a3î·Âo¸´U 3·A¸¸é ±âÁO 1a3î·ÂA¸·Î 13Á¤
		if (true == IsHorseRiding())
		{
			if (iArmor < GetHorseArmor())
				iArmor = GetHorseArmor();

			const char* pHorseName = CHorseNameManager::instance().GetHorseName(GetPlayerID());

			if (pHorseName != nullptr && strlen(pHorseName))
			{
				iArmor += 20;
			}
		}

		iArmor += GetPoint(POINT_DEF_GRADE_BONUS);
		iArmor += GetPoint(POINT_PARTY_DEFENDER_BONUS);

		int iServerDef = iDef + iArmor;
		int iClientDef = iShowDef + iArmor;

		if (GetPoint(POINT_MALL_DEFBONUS) > 0)
		{
			const int iPct = GetPoint(POINT_MALL_DEFBONUS);
			iServerDef += (iServerDef * iPct) / 100;
			iClientDef += (iClientDef * iPct) / 100;
		}

		PointChange(POINT_DEF_GRADE, iServerDef - GetPoint(POINT_DEF_GRADE));
		PointChange(POINT_CLIENT_DEF_GRADE, iClientDef - GetPoint(POINT_CLIENT_DEF_GRADE));

		PointChange(POINT_MAGIC_ATT_GRADE, GetLevel() * 2 + GetPoint(POINT_IQ) * 2 + GetPoint(POINT_MAGIC_ATT_GRADE_BONUS));
		PointChange(POINT_MAGIC_DEF_GRADE, GetLevel() + (GetPoint(POINT_IQ) * 3 + GetPoint(POINT_HT)) / 3 + iArmor / 2 + GetPoint(POINT_MAGIC_DEF_GRADE_BONUS));
	}
	else
	{
		// 2lev + str * 2
		int iAtt = GetLevel() * 2 + GetPoint(POINT_ST) * 2;
		// lev + con
		int iDef = GetLevel() + GetPoint(POINT_HT) + GetMobTable().wDef;

		SetPoint(POINT_ATT_GRADE, iAtt);
		SetPoint(POINT_DEF_GRADE, iDef);
		SetPoint(POINT_MAGIC_ATT_GRADE, GetPoint(POINT_ATT_GRADE));
		SetPoint(POINT_MAGIC_DEF_GRADE, GetPoint(POINT_DEF_GRADE));
	}
}

void CHARACTER::ComputePoints()
{
	int32_t lStat = GetPoint(POINT_STAT);
	int32_t lStatResetCount = GetPoint(POINT_STAT_RESET_COUNT);
	int32_t lSkillActive = GetPoint(POINT_SKILL);
	int32_t lSkillSub = GetPoint(POINT_SUB_SKILL);
	int32_t lSkillHorse = GetPoint(POINT_HORSE_SKILL);
	int32_t lLevelStep = GetPoint(POINT_LEVEL_STEP);

	int32_t lAttackerBonus = GetPoint(POINT_PARTY_ATTACKER_BONUS);
	int32_t lTankerBonus = GetPoint(POINT_PARTY_TANKER_BONUS);
	int32_t lBufferBonus = GetPoint(POINT_PARTY_BUFFER_BONUS);
	int32_t lSkillMasterBonus = GetPoint(POINT_PARTY_SKILL_MASTER_BONUS);
	int32_t lHasteBonus = GetPoint(POINT_PARTY_HASTE_BONUS);
	int32_t lDefenderBonus = GetPoint(POINT_PARTY_DEFENDER_BONUS);

	int32_t lHPRecovery = GetPoint(POINT_HP_RECOVERY);
	int32_t lSPRecovery = GetPoint(POINT_SP_RECOVERY);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int32_t envanterim = Inven_Point();
#endif

	memset(m_pointsInstant.points, 0, sizeof(m_pointsInstant.points));
	m_alignAppliedHP = 0;
	m_alignAppliedMonster = 0;
	m_alignAppliedHuman = 0;
	m_alignAppliedMetin = 0;
	m_alignAppliedBoss = 0;
	m_alignAppliedPvm = 0;
	m_alignAppliedNormal = 0;
	m_alignAppliedSkill = 0;

	BuffOnAttr_ClearAll();
	// Mount bonuszok eltavolitasa
	for (int i = 0; i < POINT_MAX_NUM; ++i) {
		if (i == POINT_ATTBONUS_MONSTER || i == POINT_RESIST_MAGIC || i == POINT_CRITICAL_PCT ||
			i == POINT_PENETRATE_PCT || i == POINT_ATT_GRADE_BONUS || i == POINT_DEF_GRADE_BONUS) {
			SetPoint(i, 0); // Vagy PointChange(i, -GetPoint(i)) ha inkabb valtoztatassal m?kodik
		}
	}
	//ComputeMountInventoryBonuses();

	m_SkillDamageBonus.clear();

	SetPoint(POINT_STAT, lStat);
	SetPoint(POINT_SKILL, lSkillActive);
	SetPoint(POINT_SUB_SKILL, lSkillSub);
	SetPoint(POINT_HORSE_SKILL, lSkillHorse);
	SetPoint(POINT_LEVEL_STEP, lLevelStep);
	SetPoint(POINT_STAT_RESET_COUNT, lStatResetCount);

	SetPoint(POINT_ST, GetRealPoint(POINT_ST));
	SetPoint(POINT_HT, GetRealPoint(POINT_HT));
	SetPoint(POINT_DX, GetRealPoint(POINT_DX));
	SetPoint(POINT_IQ, GetRealPoint(POINT_IQ));
#ifdef ENABLE_FIX_LEVELUP_EFFECT
	SetPart(PART_MAIN, GetPart(PART_MAIN));
#else
	SetPart(PART_MAIN, GetOriginalPart(PART_MAIN));
#endif
	SetPart(PART_WEAPON, GetOriginalPart(PART_WEAPON));
	SetPart(PART_HEAD, GetOriginalPart(PART_HEAD));
	SetPart(PART_HAIR, GetOriginalPart(PART_HAIR));
#ifdef ENABLE_RUNE_SYSTEM
	SetPart(PART_RUNE, GetOriginalPart(PART_RUNE));
#endif
#ifdef ENABLE_ACCE_SYSTEM
	SetPart(PART_ACCE, GetOriginalPart(PART_ACCE));
#endif
#ifdef ENABLE_COSTUME_EFFECT
	SetPart(PART_EFFECT_BODY, GetOriginalPart(PART_EFFECT_BODY));
	SetPart(PART_EFFECT_WEAPON, GetOriginalPart(PART_EFFECT_WEAPON));
#endif
	SetPoint(POINT_PARTY_ATTACKER_BONUS, lAttackerBonus);
	SetPoint(POINT_PARTY_TANKER_BONUS, lTankerBonus);
	SetPoint(POINT_PARTY_BUFFER_BONUS, lBufferBonus);
	SetPoint(POINT_PARTY_SKILL_MASTER_BONUS, lSkillMasterBonus);
	SetPoint(POINT_PARTY_HASTE_BONUS, lHasteBonus);
	SetPoint(POINT_PARTY_DEFENDER_BONUS, lDefenderBonus);

	SetPoint(POINT_HP_RECOVERY, lHPRecovery);
	SetPoint(POINT_SP_RECOVERY, lSPRecovery);

	// PC_BANG_ITEM_ADD
	SetPoint(POINT_PC_BANG_EXP_BONUS, 0);
	SetPoint(POINT_PC_BANG_DROP_BONUS, 0);
	// END_PC_BANG_ITEM_ADD

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	SetPoint(POINT_INVEN, envanterim);
#endif
	ComputeMountInventoryBonuses();
	int64_t iMaxHP;
	int	iMaxSP;
	int iMaxStamina;

	if (IsPC())
	{
		// AÖ´ë »ý¸í·Â/Á¤1A·Â
		iMaxHP = JobInitialPoints[GetJob()].max_hp + m_points.iRandomHP + GetPoint(POINT_HT) * JobInitialPoints[GetJob()].hp_per_ht;
		iMaxSP = JobInitialPoints[GetJob()].max_sp + m_points.iRandomSP + GetPoint(POINT_IQ) * JobInitialPoints[GetJob()].sp_per_iq;
		iMaxStamina = JobInitialPoints[GetJob()].max_stamina + GetPoint(POINT_HT) * JobInitialPoints[GetJob()].stamina_per_con;

		{
			CSkillProto* pkSk = CSkillManager::instance().Get(SKILL_ADD_HP);

			if (nullptr != pkSk)
			{
				pkSk->SetPointVar("k", 1.0f * GetSkillPower(SKILL_ADD_HP) / 100.0f);

				iMaxHP += static_cast<int>(pkSk->kPointPoly.Eval());
			}
		}

#ifdef ENABLE_NEW_SECONDARY_SKILLS
		{
			int32_t lValue[4][11] = {
								{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
								{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20},
								{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
								{0, 200, 400, 800, 1200, 1600, 2000, 2200, 2500, 2700, 3000},
			};

			PointChange(POINT_MALL_ATTBONUS, lValue[0][GetSkillLevel(NEW_SUPPORT_SKILL_ATTACK)]);
			PointChange(POINT_MALL_GOLDBONUS, lValue[1][GetSkillLevel(NEW_SUPPORT_SKILL_YANG)]);
			PointChange(POINT_ATTBONUS_MONSTER, lValue[2][GetSkillLevel(NEW_SUPPORT_SKILL_MONSTERS)]);
			iMaxHP += lValue[3][GetSkillLevel(NEW_SUPPORT_SKILL_HP)];
		}
#endif


		SetPoint(POINT_MOV_SPEED, 200);
		SetPoint(POINT_ATT_SPEED, 100);
		PointChange(POINT_ATT_SPEED, GetPoint(POINT_PARTY_HASTE_BONUS));
		SetPoint(POINT_CASTING_SPEED, 100);
	}
	else
	{
		iMaxHP = m_pkMobData->m_table.dwMaxHP;
		iMaxSP = 0;
		iMaxStamina = 0;

		SetPoint(POINT_ATT_SPEED, m_pkMobData->m_table.sAttackSpeed);
		SetPoint(POINT_MOV_SPEED, m_pkMobData->m_table.sMovingSpeed);
		SetPoint(POINT_CASTING_SPEED, m_pkMobData->m_table.sAttackSpeed);
	}

	if (IsPC())
	{
		uint32_t mountVnum = GetMountVnum();
		if (mountVnum)
		{
			bool horse = mountVnum >= 20101 && mountVnum <= 20107 ? true : false;
			int st = horse == true ? GetHorseST() : 36;
			int dx = horse == true ? GetHorseDX() : 18;
			int ht = horse == true ? GetHorseHT() : 53;
			int iq = horse == true ? GetHorseIQ() : 71;
			if (st > GetPoint(POINT_ST))
				PointChange(POINT_ST, st - GetPoint(POINT_ST));

			if (dx > GetPoint(POINT_DX))
				PointChange(POINT_DX, dx - GetPoint(POINT_DX));

			if (ht > GetPoint(POINT_HT))
				PointChange(POINT_HT, ht - GetPoint(POINT_HT));

			if (iq > GetPoint(POINT_IQ))
				PointChange(POINT_IQ, iq - GetPoint(POINT_IQ));
		}

	}

	// 1. mount_bonus_map letrehozasa
	// Ervenyes mount bonuszt ado itemek
	static const std::set<uint32_t> valid_mount_items = {
		//14590, 14591, 14592, 14593,
		//52040, 60001, 48421, 49009,
		//49049, 60003, 71223, 71253,
		//71224, 71228, 71251, 71125,
		//71126, 71127, 71139, 71166,
		//71171, 71176, 71177, 71221,
		//71222, 71252, 71256, 71225,
		//71226, 71227, 71255, 71254,
		//71233, 71250, 71128, 23014, 23015, 23016, 71137, 71140, 71185,
		// Övek: 18000 - 18119
				18000, 18001, 18002, 18003, 18004, 18005, 18006, 18007, 18008, 18009,
				18010, 18011, 18012, 18013, 18014, 18015, 18016, 18017, 18018, 18019,
				18020, 18021, 18022, 18023, 18024, 18025, 18026, 18027, 18028, 18029,
				18030, 18031, 18032, 18033, 18034, 18035, 18036, 18037, 18038, 18039,
				18040, 18041, 18042, 18043, 18044, 18045, 18046, 18047, 18048, 18049,
				18050, 18051, 18052, 18053, 18054, 18055, 18056, 18057, 18058, 18059,
				18060, 18061, 18062, 18063, 18064, 18065, 18066, 18067, 18068, 18069,
				18070, 18071, 18072, 18073, 18074, 18075, 18076, 18077, 18078, 18079,
				18080, 18081, 18082, 18083, 18084, 18085, 18086, 18087, 18088, 18089,
				18090, 18091, 18092, 18093, 18094, 18095, 18096, 18097, 18098, 18099,
				18100, 18101, 18102, 18103, 18104, 18105, 18106, 18107, 18108, 18109,
				18110, 18111, 18112, 18113, 18114, 18115, 18116, 18117, 18118, 18119,
				18120, 18121, 18122, 18123, 18124, 18125, 18126, 18127, 18128, 18129,
				18130, 18131, 18132, 18133, 18134, 18135, 18136, 18137, 18138, 18139,
				//kártyák: 18140 - 18149
				18140, 18141, 18142, 18143, 18144, 18145, 18146, 18147, 18148, 18149,
				18150, 18151, 18152, 18153, 18154, 18155, 18156, 18157, 18158, 18159

				// uj mountok 
	/*			,611500, 611501, 611502, 611503, 611504, 611505, 611506, 611507, 611508,
				611510, 611511, 611512, 611513, 611514, 611515, 611516, 611517, 611518,
				611520, 611521, 611522, 611523, 611524, 611525, 611526, 611527, 611528,
				611530, 611531, 611532, 611533, 611534, 611535, 611536, 611537, 611538,
				611540, 611541, 611542, 611543, 611544,
				611545,
				611546,
				611547,
				611548,
				611549,
				611550,
				611551,
				611552,
				611553,
				611554,
				611555,
				611556,
				611557,
				611558,
				611559,
				611560,
				611561,
				611562,
				611563,
				611564,
				611565,
				611566,
				611567,
				611568,
				611569,
				611570,
				611571,
				611572,
				611573,
				611574,
				611575,
				611576,
				611577,
				611578,
				611579,
				611580,
				611581,
				611582,
				611583,
				611584,
				611585,
				611586,
				611587,
				611588,
				611589,
				611590,
				611591,
				611592,
				611593,
				611594,
				611595,
				611596,
				611597,
				611598,
				611599,
				611600,
				611601,
				611602,
				611603,
				611604,
				611605,
				611606,
				611607,
				611608,
				611609,
				611610,
				611611,
				611612,
				611613,
				611614,
				611615,
				611616,
				611617,
				611618,
				611619,
				611620,
				611621,
				611622,
				611623,
				611624,
				611625,
				611626,
				611627,
				611628,
				611629,
				611630,
				611631,
				611632,
				611633,
				611634,
				611635,
				611636,
				611637,
				611638,
				611639,
				611640,
				611641,
				611642,
				611643,
				611644,
				611645,
				611646,
				611647,
				611648,
				611649,
				611650,
				611651,
				611652,
				611653,
				611654,
				611655,
				611656,
				611657,
				611658,
				611659,
				611660,
				611661,
				611662,
				611663,
				611664,
				611665,
				611666*/
	};

	std::map<uint8_t, int32_t> mount_bonus_map;

	for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
	{
		LPITEM item = GetInventoryItem(i);
		if (!item)
			continue;

		uint32_t vnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, item));
		if (valid_mount_items.contains(vnum))
		{
			const TItemTable* proto = ItemSystem::GetItemProto(EntityFactory::CreateItemEntity(g_registry, item));
			if (!proto)
				continue;

			for (const auto apply : proto->aApplies)
			{
				if (apply.bType != APPLY_NONE && apply.lValue != 0)
				{
					uint8_t pointType = aApplyInfo[apply.bType].bPointType;
					if (pointType != POINT_NONE)
						mount_bonus_map[pointType] += apply.lValue;
				}
			}
		}
	}


	for (const auto& [pointType, value] : mount_bonus_map)
	{
		PointChange(pointType, value);
		//UpdatePacket();
		//sys_log(0, "DEBUG: VEGLEGES MOUNT BONUS APPLY -> POINT %d = +%d", pointType, value);
	}


	ComputeBattlePoints();

	// ±âo» HP/SP 13Á¤
	if (iMaxHP != GetMaxHP())
	{
		SetRealPoint(POINT_MAX_HP, iMaxHP); // ±âo»HP¸¦ RealPoint?! AúAaÇO 3o´Â´U.
	}

	PointChange(POINT_MAX_HP, 0);

	if (iMaxSP != GetMaxSP())
	{
		SetRealPoint(POINT_MAX_SP, iMaxSP); // ±âo»SP¸¦ RealPoint?! AúAaÇO 3o´Â´U.
	}

	PointChange(POINT_MAX_SP, 0);

	SetMaxStamina(iMaxStamina);
	// @fixme118 part1
	int64_t iCurHP = this->GetHP();
	int64_t iCurSP = this->GetSP();

	uint32_t immuneFlag = 0;

	for (int i = 0; i < WEAR_MAX_NUM; i++) {
		LPITEM pItem = GetWear(i);
		if (pItem) {
#ifdef ENABLE_RUNE_SYSTEM
			if (pItem->IsRune() && ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), 1) != 1) {
				continue;
			}
#endif

			pItem->ModifyPoints(true);
			SET_BIT(immuneFlag, GetWear(i)->GetImmuneFlag());
		}
	}

	// ?ëEY1® 1A1oAU
	// ComputePoints?!1­´Â ÄÉ¸—AÍAÇ ¸?µç 1Ó1o°aA» AE±âE­ÇI°í,
	// 3AAIAU, 1öÇÁ µî?! °ü·AµE ¸?µç 1Ó1o°aA» Aç°e»eÇI±â ¶§1®?!,
	// ?ëEY1® 1A1oAUµµ ActiveDeck?! AÖ´Â ¸?µç ?ëEY1®AÇ 1Ó1o°aA» ´U1A Au?ë1AÄN3ß ÇN´U.
#ifdef ENABLE_EVENT_MANAGER
	CHARACTER_MANAGER::Instance().CheckBonusEvent(this);
#endif

	if (DragonSoul_IsDeckActivated())
	{
		for (int i = WEAR_MAX_NUM + DS_SLOT_MAX * DragonSoul_GetActiveDeck();
			i < WEAR_MAX_NUM + DS_SLOT_MAX * (DragonSoul_GetActiveDeck() + 1); i++)
		{
			LPITEM pItem = GetWear(i);
			if (pItem)
			{
				if (DSManager::instance().IsTimeLeftDragonSoul(EntityFactory::CreateItemEntity(g_registry, pItem)))
					pItem->ModifyPoints(true);
			}
		}
	}

	if (GetHP() > GetMaxHP())
		PointChange(POINT_HP, GetMaxHP() - GetHP());

	if (GetSP() > GetMaxSP())
		PointChange(POINT_SP, GetMaxSP() - GetSP());

	ComputeSkillPoints();

	RefreshAffect();

	CPetSystem* pPetSystem = GetPetSystem();
	if (nullptr != pPetSystem) {
		pPetSystem->RefreshBuff();
	}

	//#ifdef __NEWPET_SYSTEM__
	//	if (m_newpetSystem) {
	//		m_newpetSystem->RefreshBuff();
	//	}
	//#endif


		// @fixme118 part2 (after petsystem stuff)
	if (IsPC())
	{
		if (this->GetHP() != iCurHP)
			ecs::PointSystem::Change(AIHelpers::EcsOf(this), POINT_HP, iCurHP - this->GetHP());
		if (this->GetSP() != iCurSP)
			ecs::PointSystem::Change(AIHelpers::EcsOf(this), POINT_SP, iCurSP - this->GetSP());
	}
	//#ifdef ENABLE_FAKE_SHOP_HEADER
	//	UpdateMountCountOverhead();
	//#endif
	ApplyAlignmentBonus();

	// csak a kulonbseget addjuk hozza -> nem tud stackelni
	const int32_t dHP = m_alignBonusHP - m_alignAppliedHP;
	const int32_t dMon = m_alignBonusMonster - m_alignAppliedMonster;
	const int32_t dHum = m_alignBonusHuman - m_alignAppliedHuman;
	const int32_t dMet = m_alignBonusMetin - m_alignAppliedMetin;
	const int32_t dBoss = m_alignBonusBoss - m_alignAppliedBoss;
	const int32_t dPvm = m_alignBonusPvm - m_alignAppliedPvm;
	const int32_t dNormal = m_alignBonusNormal - m_alignAppliedNormal;
	const int32_t dSkill = m_alignBonusSkill - m_alignAppliedSkill;

	if (dHP)     PointChange(POINT_MAX_HP, dHP);
	if (dMon)    PointChange(POINT_ATTBONUS_MONSTER, dMon);
	if (dHum)    PointChange(POINT_ATTBONUS_HUMAN, dHum);
	if (dMet)    PointChange(POINT_ATTBONUS_METIN, dMet);
	if (dBoss)   PointChange(POINT_ATTBONUS_BOSS, dBoss);
	if (dPvm)    PointChange(POINT_ATTBONUS_MEDI_PVM, dPvm);
	if (dNormal) PointChange(POINT_NORMAL_HIT_DAMAGE_BONUS, dNormal);
	if (dSkill)  PointChange(POINT_SKILL_DAMAGE_BONUS, dSkill);

	// felrakott ertekek eltetele
	m_alignAppliedHP = m_alignBonusHP;
	m_alignAppliedMonster = m_alignBonusMonster;
	m_alignAppliedHuman = m_alignBonusHuman;
	m_alignAppliedMetin = m_alignBonusMetin;
	m_alignAppliedBoss = m_alignBonusBoss;
	m_alignAppliedPvm = m_alignBonusPvm;
	m_alignAppliedNormal = m_alignBonusNormal;
	m_alignAppliedSkill = m_alignBonusSkill;


	UpdatePacket();
	ComputeBattlePoints();


}

// m_dwPlayStartTimeAÇ ´ÜA§´Â milisecond´U. µYAIAÍoLAI1o?!´Â o?´ÜA§·Î ±â·IÇI±â
// ¶§1®?! ÇA·1AI1A°LA» °e»eÇO ¶§ / 60000 A¸·Î 3a´21­ ÇI´ÂµY, ±× 3a¸ÓÁö °aAI 323O
// A» ¶§ ?©±â?! dwTimeRemainA¸·Î 3Ö3î1­ Á¦´ë·Î °e»eµÇµµ·I ÇOÁÖ3î3ß ÇN´U.





