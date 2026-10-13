#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "desc_client.h"
#include "db.h"
#include "log.h"
#include "skill.h"
#include "JsonFile.h"
#include "priv_manager.h"
#include "questmanager.h"
#include "unique_item.h"
#include "safebox.h"
#include "blend_item.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"
#include "item_manager_private_types.h"

#include <nlohmann/json.hpp>
#ifdef __INGAME_WIKI__
#include <common/in_game_wiki.h>
#include "mob_manager.h"
#endif
std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];


bool ITEM_MANAGER::ReadCommonDropItemFile(const char *c_pszFileName)
{
	nlohmann::json doc;

	if (!JsonFile::Load(c_pszFileName, doc) || !doc.contains("groups") || !doc.at("groups").is_array())
		return false;

	for (const auto& group : doc.at("groups"))
	{
		std::string stName = group.value("name", std::string());

		if (!group.contains("rank"))
		{
			LOG_ERROR("CommonDropitem : Syntax error {} : no rank, node {}", c_pszFileName, stName.c_str());
			return false;
		}

		int rank = group.value("rank", 0);

		if (!group.contains("items") || !group.at("items").is_array())
			continue;

		for (const auto& entry : group.at("items"))
		{
			std::string name = entry.value("item", std::string());
			uint32_t vnum = 0;

			if (!GetVnumByOriginalName(name.c_str(), vnum))
			{
				str_to_number(vnum, name.c_str());

				if (!ITEM_MANAGER::instance().GetTable(vnum))
				{
					LOG_ERROR("CommonDropitem : there is no item node {} : vnum {}", stName.c_str(), vnum);
					return false;
				}
			}

			int lvlStart = entry.value("level_start", 0);

			if (lvlStart < 1)
			{
				LOG_ERROR("CommonDropitem : lvlStart negative node {} : lvlstart {}", stName.c_str(), lvlStart);
				return false;
			}

			int lvlEnd = entry.value("level_end", 0);

			if (lvlEnd < 1)
			{
				LOG_ERROR("CommonDropitem : lvlEnd negative node {} : lvlEnd {}", stName.c_str(), lvlEnd);
				return false;
			}

			if (lvlStart > lvlEnd)
			{
				LOG_ERROR("CommonDropitem : lvlStart and lvlEnd >< node {} : lvlStart {} lvlEnd {}", stName.c_str(), lvlStart, lvlEnd);
				return false;
			}

			float pct = entry.value("chance", 0.0f);

			uint32_t dwPct = (uint32_t)(pct * 10000.0f);

			if (lvlStart == 0)
				continue;

			LOG_TRACE("CommonDropItem ADD: Rank {}  LvlStart {}  LvlEnd {}   dwPct {}   Vnum {}", rank, lvlStart, lvlEnd, dwPct, vnum);

			g_vec_pkCommonDropItem[rank].push_back(CItemDropInfo(lvlStart, lvlEnd, dwPct, vnum));
		}
	}

	return true;
}



bool ITEM_MANAGER::ReadSpecialDropItemFile(const char * c_pszFileName)
{
	nlohmann::json doc;

	if (!JsonFile::Load(c_pszFileName, doc) || !doc.contains("groups") || !doc.at("groups").is_array())
		return false;

	for (const auto& group : doc.at("groups"))
	{
		std::string stName = group.value("name", std::string());

		if (!group.contains("vnum"))
		{
			LOG_ERROR("ReadSpecialDropItemFile : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			return false;
		}

		int iVnum = group.value("vnum", 0);

		LOG_TRACE("DROP_ITEM_GROUP {} {}", stName.c_str(), iVnum);

		std::string stType = group.value("type", std::string());
		int type = CSpecialItemGroup::NORMAL;

		if (!stType.empty())
		{
			if (stType == "pct")
			{
				type = CSpecialItemGroup::PCT;
			}
			else if (stType == "quest")
			{
				type = CSpecialItemGroup::QUEST;
				quest::CQuestManager::instance().RegisterNPCVnum(iVnum);
			}
			else if (stType == "special")
			{
				type = CSpecialItemGroup::SPECIAL;
			}
		}

		if ("attr" == stType)
		{
			CSpecialAttrGroup * pkGroup = M2_NEW CSpecialAttrGroup(iVnum);

			if (group.contains("attrs") && group.at("attrs").is_array())
			{
				for (const auto& attr : group.at("attrs"))
				{
					uint32_t apply_type = 0;
					int	apply_value = 0;
					std::string applyName = attr.value("apply", std::string());

					str_to_number(apply_type, applyName.c_str());

					if (0 == apply_type)
					{
						apply_type = FN_get_apply_type(applyName.c_str());

						if (0 == apply_type)
						{
							LOG_ERROR("Invalid APPLY_TYPE {} in Special Item Group Vnum {}", applyName.c_str(), iVnum);
							return false;
						}
					}

					apply_value = attr.value("value", 0);

					if (apply_type > MAX_APPLY_NUM)
					{
						LOG_ERROR("Invalid APPLY_TYPE {} in Special Item Group Vnum {}", apply_type, iVnum);
						M2_DELETE(pkGroup);
						return false;
					}

					pkGroup->m_vecAttrs.push_back(CSpecialAttrGroup::CSpecialAttrInfo(apply_type, apply_value));
				}
			}

			if (group.contains("effect") && group.at("effect").is_string())
				pkGroup->m_stEffectFileName = group.at("effect").get<std::string>();

			m_map_pkSpecialAttrGroup.insert(std::make_pair(iVnum, pkGroup));
		}
		else
		{
			CSpecialItemGroup * pkGroup = M2_NEW CSpecialItemGroup(iVnum, type);

			if (group.contains("items") && group.at("items").is_array())
			{
				for (const auto& entry : group.at("items"))
				{
					std::string name = entry.value("item", std::string());
					uint32_t dwVnum = 0;

					if (!GetVnumByOriginalName(name.c_str(), dwVnum))
					{
						if (name == "exp")
						{
							dwVnum = CSpecialItemGroup::EXP;
						}
						else if (name == "mob")
						{
							dwVnum = CSpecialItemGroup::MOB;
						}
						else if (name == "slow")
						{
							dwVnum = CSpecialItemGroup::SLOW;
						}
						else if (name == "drain_hp")
						{
							dwVnum = CSpecialItemGroup::DRAIN_HP;
						}
						else if (name == "poison")
						{
							dwVnum = CSpecialItemGroup::POISON;
						}
						else if (name == "group")
						{
							dwVnum = CSpecialItemGroup::MOB_GROUP;
						}
						else
						{
							str_to_number(dwVnum, name.c_str());

							if (!ITEM_MANAGER::instance().GetTable(dwVnum))
							{
								LOG_ERROR("ReadSpecialDropItemFile : there is no item {} : node {}", name.c_str(), stName.c_str());
								M2_DELETE(pkGroup);

								return false;
							}
						}
					}

					int iCount = entry.value("count", 0);
					int iProb = entry.value("prob", 0);
					int iRarePct = entry.value("rare", 0);

					LOG_TRACE("        name {} count {} prob {} rare {}", name.c_str(), iCount, iProb, iRarePct);
					pkGroup->AddItem(dwVnum, iCount, iProb, iRarePct);

#ifdef __INGAME_WIKI__
					auto pTableTemp = GetTable(dwVnum);
					uint32_t addVnum = dwVnum;
					uint32_t startRefineVnum = GetWikiItemStartRefineVnum(dwVnum);

					if (pTableTemp && (pTableTemp->bType == ITEM_WEAPON || pTableTemp->bType == ITEM_ARMOR) && startRefineVnum != addVnum)
						addVnum = (startRefineVnum != 0 ? startRefineVnum : addVnum);

					CommonWikiData::TWikiItemOriginInfo origin_info;
					origin_info.set_vnum(iVnum);
					origin_info.set_is_mob(false);

					m_itemOriginMap[addVnum].push_back(origin_info);
#endif

					// CHECK_UNIQUE_GROUP
					if (iVnum < 30000)
					{
						m_ItemToSpecialGroup[dwVnum] = iVnum;
					}
					// END_OF_CHECK_UNIQUE_GROUP
				}
			}

			if (CSpecialItemGroup::QUEST == type)
			{
				m_map_pkQuestItemGroup.insert(std::make_pair(iVnum, pkGroup));
			}
			else
			{
				m_map_pkSpecialItemGroup.insert(std::make_pair(iVnum, pkGroup));
			}
		}
	}

	return true;
}



bool ITEM_MANAGER::ConvSpecialDropItemFile()
{
	char szSpecialItemGroupFileName[256];
	snprintf(szSpecialItemGroupFileName, sizeof(szSpecialItemGroupFileName),
		"%s/special_item_group.json", LocaleService_GetBasePath().c_str());

	FILE *fp = fopen("special_item_group_vnum.txt", "w");
	if (!fp)
	{
		LOG_ERROR("could not open file ({})", "special_item_group_vnum.txt");
		return false;
	}

	nlohmann::json doc;

	if (!JsonFile::Load(szSpecialItemGroupFileName, doc) || !doc.contains("groups") || !doc.at("groups").is_array())
	{
		fclose(fp);
		return false;
	}

	for (const auto& group : doc.at("groups"))
	{
		std::string stName = group.value("name", std::string());

		if (!group.contains("vnum"))
		{
			LOG_ERROR("ConvSpecialDropItemFile : Syntax error {} : no vnum, node {}", szSpecialItemGroupFileName, stName.c_str());
			fclose(fp);
			return false;
		}

		int iVnum = group.value("vnum", 0);

		std::string str = group.value("type", std::string());
		int type = 0;
		if (str == "pct")
			type = 1;
		else if (str == "quest")
			type = 2;
		else if (str == "special")
			type = 3;
		else if (str == "attr")
			type = 4;

		fprintf(fp, "Group	%s\n", stName.c_str());
		fprintf(fp, "{\n");
		fprintf(fp, "	Vnum	%i\n", iVnum);
		if (type==1)
			fprintf(fp, "	Type	Pct\n");
		else if (type==2)
			fprintf(fp, "	Type	Quest\n");
		else if (type==3)
			fprintf(fp, "	Type	special\n");
		else if (type==4)
			fprintf(fp, "	Type	ATTR\n");

		int k = 1;

		if (type==4)
		{
			if (group.contains("attrs") && group.at("attrs").is_array())
			{
				for (const auto& attr : group.at("attrs"))
				{
					std::string applyName = attr.value("apply", std::string());
					uint32_t dwApply = 0;
					str_to_number(dwApply, applyName.c_str());

					if (0 == dwApply)
						dwApply = FN_get_apply_type(applyName.c_str());

					int iValue = attr.value("value", 0);

					fprintf(fp, "	%d	%u	%d\n", k++, dwApply, iValue);
				}
			}
		}
		else
		{
			if (group.contains("items") && group.at("items").is_array())
			{
				for (const auto& entry : group.at("items"))
				{
					std::string name = entry.value("item", std::string());
					uint32_t dwVnum = 0;

					if (!GetVnumByOriginalName(name.c_str(), dwVnum))
						str_to_number(dwVnum, name.c_str());

					int iCount = entry.value("count", 0);
					int iProb = entry.value("prob", 0);

					if (0 == dwVnum)
						fprintf(fp, "	%d	%s	%d	%d\n", k++, name.c_str(), iCount, iProb);
					else
						fprintf(fp, "	%d	%u	%d	%d\n", k++, dwVnum, iCount, iProb);
				}
			}
		}

		std::string effect;
		if (group.contains("effect") && group.at("effect").is_string())
			fprintf(fp, "\teffect\t\"%s\"\n", group.at("effect").get<std::string>().c_str());

		fprintf(fp, "}\n");
		fprintf(fp, "\n");
	}

	fclose(fp);
	return true;
}

bool ITEM_MANAGER::ReadEtcDropItemFile(const char * c_pszFileName)
{
	nlohmann::json doc;

	if (!JsonFile::Load(c_pszFileName, doc))
	{
		LOG_ERROR("Cannot open {}", c_pszFileName);
		return false;
	}

	if (!doc.is_array())
	{
		LOG_ERROR("ReadEtcDropItemFile : {} must be an array of {{item, prob}} entries", c_pszFileName);
		return false;
	}

	for (const auto& entry : doc)
	{
		if (!entry.is_object())
			continue;

		std::string szItemName = entry.value("item", std::string());
		float fProb = entry.value("prob", 0.0f);

		if (szItemName.empty() || fProb == 0.0f)
			continue;

		uint32_t dwItemVnum;

		if (!ITEM_MANAGER::instance().GetVnumByOriginalName(szItemName.c_str(), dwItemVnum))
		{
#ifdef ENABLE_BUG_FIXES
			str_to_number(dwItemVnum, szItemName.c_str());
			if (!ITEM_MANAGER::instance().GetTable(dwItemVnum))
			{
				LOG_ERROR("No such an item (name/vnum: {})", szItemName.c_str());
				return false;
			}
#else
			LOG_ERROR("No such an item (name: {})", szItemName.c_str());
			return false;
#endif
		}

		m_map_dwEtcItemDropProb[dwItemVnum] = (uint32_t) (fProb * 10000.0f);
		LOG_TRACE("ETC_DROP_ITEM: {} prob {:f}", szItemName.c_str(), fProb);
	}

	return true;
}


bool ITEM_MANAGER::ReadMonsterDropItemGroup(const char * c_pszFileName)
{
	nlohmann::json doc;

	if (!JsonFile::Load(c_pszFileName, doc) || !doc.contains("groups") || !doc.at("groups").is_array())
		return false;

	for (const auto& group : doc.at("groups"))
	{
		std::string stName = group.value("name", std::string());

		if (strncmp (stName.c_str(), "kr_", 3) == 0)
			continue;

		int iMobVnum = 0;
		int iKillDrop = 0;
		int iLevelLimit = 0;

		std::string strType = group.value("type", std::string());

		if (strType.empty())
		{
			LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : no type (kill|drop), node {}", c_pszFileName, stName.c_str());
			return false;
		}

		if (!group.contains("mob"))
		{
			LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : no mob vnum, node {}", c_pszFileName, stName.c_str());
			return false;
		}

		iMobVnum = group.value("mob", 0);

		if (strType == "kill")
		{
			if (!group.contains("kill_drop"))
			{
				LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : no kill drop count, node {}", c_pszFileName, stName.c_str());
				return false;
			}

			iKillDrop = group.value("kill_drop", 0);
		}
		else
		{
			iKillDrop = 1;
		}

		if ( strType == "limit" )
		{
			if (!group.contains("level_limit"))
			{
				LOG_ERROR("ReadmonsterDropItemGroup : Syntax error {} : no level_limit, node {}", c_pszFileName, stName.c_str());
				return false;
			}

			iLevelLimit = group.value("level_limit", 0);
		}
		else
		{
			iLevelLimit = 0;
		}

		LOG_TRACE("MOB_ITEM_GROUP {} [{}] {} {}", stName.c_str(), strType.c_str(), iMobVnum, iKillDrop);

		if (iKillDrop == 0)
		{
			continue;
		}

		const nlohmann::json& items = (group.contains("items") && group.at("items").is_array())
			? group.at("items") : nlohmann::json::array();

		if (strType == "kill")
		{
			CMobItemGroup * pkGroup = M2_NEW CMobItemGroup(iMobVnum, iKillDrop, stName);

			for (const auto& entry : items)
			{
				std::string name = entry.value("item", std::string());
				uint32_t dwVnum = 0;

				if (!GetVnumByOriginalName(name.c_str(), dwVnum))
				{
					str_to_number(dwVnum, name.c_str());
					if (!ITEM_MANAGER::instance().GetTable(dwVnum))
					{
						LOG_ERROR("ReadMonsterDropItemGroup : there is no item {} : node {} : vnum {}", name.c_str(), stName.c_str(), dwVnum);
						return false;
					}
				}

				int iCount = entry.value("count", 0);

				if (iCount<1)
				{
					LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {} : vnum {}, count {}", name.c_str(), stName.c_str(), dwVnum, iCount);
					return false;
				}

				int iPartPct = entry.value("chance", 0);

				if (iPartPct == 0)
				{
					LOG_ERROR("ReadMonsterDropItemGroup : there is no drop percent for item {} : node {} : vnum {}, count {}, pct {}", name.c_str(), stName.c_str(), dwVnum, iCount, iPartPct);
					return false;
				}

				int iRarePct = entry.value("rare", 0);
				iRarePct = MINMAX(0, iRarePct, 100);

				LOG_TRACE("        {} count {} rare {}", name.c_str(), iCount, iRarePct);
				pkGroup->AddItem(dwVnum, iCount, iPartPct, iRarePct);
#ifdef __INGAME_WIKI__
				CommonWikiData::TWikiInfoTable* tbl;
				if ((tbl = GetItemWikiInfo(dwVnum)) && !tbl->origin_vnum)
					tbl->origin_vnum = iMobVnum;

				auto pTableTemp = GetTable(dwVnum);
				uint32_t currVnum = dwVnum;
				uint32_t startRefineVnum = GetWikiItemStartRefineVnum(dwVnum);

				if (pTableTemp && (pTableTemp->bType == ITEM_WEAPON || pTableTemp->bType == ITEM_ARMOR) && startRefineVnum != currVnum)
					currVnum = (startRefineVnum != 0 ? startRefineVnum : currVnum);

				CommonWikiData::TWikiItemOriginInfo origin_info;
				origin_info.set_vnum(iMobVnum);
				origin_info.set_is_mob(true);

				m_itemOriginMap[currVnum].push_back(origin_info);
				CMobManager::instance().GetMobWikiInfo(iMobVnum).push_back(CommonWikiData::TWikiMobDropInfo(dwVnum, iCount));
#endif
			}
			m_map_pkMobItemGroup.insert(std::map<uint32_t, CMobItemGroup*>::value_type(iMobVnum, pkGroup));

		}
		else if (strType == "drop")
		{
			CDropItemGroup* pkGroup;
			bool bNew = true;
			auto it = m_map_pkDropItemGroup.find (iMobVnum);
			if (it == m_map_pkDropItemGroup.end())
			{
				pkGroup = M2_NEW CDropItemGroup(0, iMobVnum, stName);
			}
			else
			{
				bNew = false;
				pkGroup = it->second;
			}

			for (const auto& entry : items)
			{
				std::string name = entry.value("item", std::string());
				uint32_t dwVnum = 0;

				if (!GetVnumByOriginalName(name.c_str(), dwVnum))
				{
					str_to_number(dwVnum, name.c_str());
					if (!ITEM_MANAGER::instance().GetTable(dwVnum))
					{
						LOG_ERROR("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());
						M2_DELETE(pkGroup);

						return false;
					}
				}

				int iCount = entry.value("count", 0);

				if (iCount < 1)
				{
					LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
					M2_DELETE(pkGroup);

					return false;
				}

				float fPercent = entry.value("chance", 0.0f);

				uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

				LOG_TRACE("        name {} pct {} count {}", name.c_str(), dwPct, iCount);
				pkGroup->AddItem(dwVnum, dwPct, iCount);
#ifdef __INGAME_WIKI__
				CommonWikiData::TWikiInfoTable* tbl;
				if ((tbl = GetItemWikiInfo(dwVnum)) && !tbl->origin_vnum)
					tbl->origin_vnum = iMobVnum;

				auto pTableTemp = GetTable(dwVnum);
				uint32_t currVnum = dwVnum;
				uint32_t startRefineVnum = GetWikiItemStartRefineVnum(dwVnum);

				if (pTableTemp && (pTableTemp->bType == ITEM_WEAPON || pTableTemp->bType == ITEM_ARMOR) && startRefineVnum != currVnum)
					currVnum = startRefineVnum != 0 ? startRefineVnum : currVnum;

				CommonWikiData::TWikiItemOriginInfo origin_info;
				origin_info.set_vnum(iMobVnum);
				origin_info.set_is_mob(true);

				m_itemOriginMap[currVnum].push_back(origin_info);
				CMobManager::instance().GetMobWikiInfo(iMobVnum).push_back(CommonWikiData::TWikiMobDropInfo(dwVnum, iCount));
#endif
			}
			if (bNew)
				m_map_pkDropItemGroup.insert(std::map<uint32_t, CDropItemGroup*>::value_type(iMobVnum, pkGroup));

		}
		else if ( strType == "limit" )
		{
			CLevelItemGroup* pkLevelItemGroup = M2_NEW CLevelItemGroup(iLevelLimit);

			for (const auto& entry : items)
			{
				std::string name = entry.value("item", std::string());
				uint32_t dwItemVnum = 0;

				if (false == GetVnumByOriginalName(name.c_str(), dwItemVnum))
				{
					str_to_number(dwItemVnum, name.c_str());
					if ( !ITEM_MANAGER::instance().GetTable(dwItemVnum) )
					{
						LOG_ERROR("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());
						M2_DELETE(pkLevelItemGroup);
						return false;
					}
				}

				int iCount = entry.value("count", 0);

				if (iCount < 1)
				{
					LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
					M2_DELETE(pkLevelItemGroup);
					return false;
				}

				float fPct = entry.value("chance", 0.0f);
				uint32_t dwPct = (uint32_t)(10000.0f * fPct);

				LOG_TRACE("        name {} pct {} count {}", name.c_str(), dwPct, iCount);
				pkLevelItemGroup->AddItem(dwItemVnum, dwPct, iCount);
#ifdef __INGAME_WIKI__
				CommonWikiData::TWikiInfoTable* tbl;
				if ((tbl = GetItemWikiInfo(dwItemVnum)) && !tbl->origin_vnum)
					tbl->origin_vnum = iMobVnum;

				auto pTableTemp = GetTable(dwItemVnum);
				uint32_t currVnum = dwItemVnum;
				uint32_t startRefineVnum = GetWikiItemStartRefineVnum(dwItemVnum);

				if (pTableTemp && (pTableTemp->bType == ITEM_WEAPON || pTableTemp->bType == ITEM_ARMOR) && startRefineVnum != currVnum)
					currVnum = startRefineVnum != 0 ? startRefineVnum : currVnum;

				CommonWikiData::TWikiItemOriginInfo origin_info;
				origin_info.set_vnum(iMobVnum);
				origin_info.set_is_mob(true);

				m_itemOriginMap[currVnum].push_back(origin_info);
				CMobManager::instance().GetMobWikiInfo(iMobVnum).push_back(CommonWikiData::TWikiMobDropInfo(dwItemVnum, iCount));
#endif
			}

			m_map_pkLevelItemGroup.insert(std::map<uint32_t, CLevelItemGroup*>::value_type(iMobVnum, pkLevelItemGroup));
		}
		else if (strType == "thiefgloves")
		{
			CBuyerThiefGlovesItemGroup* pkGroup = M2_NEW CBuyerThiefGlovesItemGroup(0, iMobVnum, stName);

			for (const auto& entry : items)
			{
				std::string name = entry.value("item", std::string());
				uint32_t dwVnum = 0;

				if (!GetVnumByOriginalName(name.c_str(), dwVnum))
				{
					str_to_number(dwVnum, name.c_str());
					if (!ITEM_MANAGER::instance().GetTable(dwVnum))
					{
						LOG_ERROR("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());
						M2_DELETE(pkGroup);

						return false;
					}
				}

				int iCount = entry.value("count", 0);

				if (iCount < 1)
				{
					LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
					M2_DELETE(pkGroup);

					return false;
				}

				float fPercent = entry.value("chance", 0.0f);

				uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

				LOG_TRACE("        name {} pct {} count {}", name.c_str(), dwPct, iCount);
				pkGroup->AddItem(dwVnum, dwPct, iCount);
			}

			m_map_pkGloveItemGroup.insert(std::map<uint32_t, CBuyerThiefGlovesItemGroup*>::value_type(iMobVnum, pkGroup));
		}
		else
		{
			LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : invalid type {} (kill|drop), node {}", c_pszFileName, strType.c_str(), stName.c_str());
			return false;
		}
	}

	return true;
}


bool ITEM_MANAGER::ReadDropItemGroup(const char * c_pszFileName)
{
	nlohmann::json doc;

	if (!JsonFile::Load(c_pszFileName, doc) || !doc.contains("groups") || !doc.at("groups").is_array())
		return false;

	for (const auto& group : doc.at("groups"))
	{
		std::string stName = group.value("name", std::string());

		if (!group.contains("vnum"))
		{
			LOG_ERROR("ReadDropItemGroup : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			return false;
		}

		if (!group.contains("mob"))
		{
			LOG_ERROR("ReadDropItemGroup : Syntax error {} : no mob vnum, node {}", c_pszFileName, stName.c_str());
			return false;
		}

		int iVnum = group.value("vnum", 0);
		int iMobVnum = group.value("mob", 0);

		LOG_TRACE("DROP_ITEM_GROUP {} {}", stName.c_str(), iMobVnum);

		auto it = m_map_pkDropItemGroup.find(iMobVnum);

		CDropItemGroup* pkGroup;

		if (it == m_map_pkDropItemGroup.end())
			pkGroup = M2_NEW CDropItemGroup(iVnum, iMobVnum, stName);
		else
			pkGroup = it->second;

		if (group.contains("items") && group.at("items").is_array())
		{
			for (const auto& entry : group.at("items"))
			{
				std::string name = entry.value("item", std::string());
				uint32_t dwVnum = 0;

				if (!GetVnumByOriginalName(name.c_str(), dwVnum))
				{
					str_to_number(dwVnum, name.c_str());
					if (!ITEM_MANAGER::instance().GetTable(dwVnum))
					{
						LOG_ERROR("ReadDropItemGroup : there is no item {} : node {}", name.c_str(), stName.c_str());

						if (it == m_map_pkDropItemGroup.end())
							M2_DELETE(pkGroup);

						return false;
					}
				}

				float fPercent = entry.value("chance", 0.0f);

				uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

				int iCount = entry.value("count", 1);

				if (iCount < 1)
				{
					LOG_ERROR("ReadDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());

					if (it == m_map_pkDropItemGroup.end())
						M2_DELETE(pkGroup);

					return false;
				}

				LOG_TRACE("        {} {} {}", name.c_str(), dwPct, iCount);
				pkGroup->AddItem(dwVnum, dwPct, iCount);
			}
		}

		if (it == m_map_pkDropItemGroup.end())
			m_map_pkDropItemGroup.insert(std::map<uint32_t, CDropItemGroup*>::value_type(iMobVnum, pkGroup));
	}

	return true;
}

bool ITEM_MANAGER::ReadItemVnumMaskTable(const char * c_pszFileName)
{
	nlohmann::json doc;

	if (!JsonFile::Load(c_pszFileName, doc))
		return false;

	if (!doc.contains("pairs") || !doc.at("pairs").is_array())
	{
		LOG_ERROR("ReadItemVnumMaskTable : {} must have a 'pairs' array of {{ori, new}} entries", c_pszFileName);
		return false;
	}

	for (const auto& entry : doc.at("pairs"))
	{
		if (!entry.is_object())
			continue;

		uint32_t ori_vnum = entry.value("ori", 0u);
		uint32_t new_vnum = entry.value("new", 0u);

		m_map_new_to_ori.insert (TMapDW2DW::value_type (new_vnum, ori_vnum));
	}

	return true;
}
