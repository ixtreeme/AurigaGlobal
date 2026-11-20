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
#include <fstream>
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

#include <nlohmann/json.hpp>
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
			CSpecialAttrGroup * pkGroup = new CSpecialAttrGroup(iVnum);

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
						delete pkGroup;
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
			CSpecialItemGroup * pkGroup = new CSpecialItemGroup(iVnum, type);

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
								delete pkGroup;

								return false;
							}
						}
					}

					int iCount = entry.value("count", 0);
					int iProb = entry.value("prob", 0);
					int iRarePct = entry.value("rare", 0);

					LOG_TRACE("        name {} count {} prob {} rare {}", name.c_str(), iCount, iProb, iRarePct);
					pkGroup->AddItem(dwVnum, iCount, iProb, iRarePct);

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

	nlohmann::json doc;

	if (!JsonFile::Load(szSpecialItemGroupFileName, doc) || !doc.contains("groups") || !doc.at("groups").is_array())
		return false;

	std::ofstream file("special_item_group_vnum.txt");

	if (!file)
	{
		LOG_ERROR("could not open file ({})", "special_item_group_vnum.txt");
		return false;
	}

	for (const auto& group : doc.at("groups"))
	{
		std::string stName = group.value("name", std::string());

		if (!group.contains("vnum"))
		{
			LOG_ERROR("ConvSpecialDropItemFile : Syntax error {} : no vnum, node {}", szSpecialItemGroupFileName, stName.c_str());
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

		file << "Group" << '\t' << stName << '\n';
		file << "{" << '\n';
		file << '\t' << "Vnum" << '\t' << iVnum << '\n';

		if (type==1)
			file << '\t' << "Type" << '\t' << "Pct" << '\n';
		else if (type==2)
			file << '\t' << "Type" << '\t' << "Quest" << '\n';
		else if (type==3)
			file << '\t' << "Type" << '\t' << "special" << '\n';
		else if (type==4)
			file << '\t' << "Type" << '\t' << "ATTR" << '\n';

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

					file << '\t' << k++ << '\t' << dwApply << '\t' << iValue << '\n';
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
						file << '\t' << k++ << '\t' << name << '\t' << iCount << '\t' << iProb << '\n';
					else
						file << '\t' << k++ << '\t' << dwVnum << '\t' << iCount << '\t' << iProb << '\n';
				}
			}
		}

		if (group.contains("effect") && group.at("effect").is_string())
			file << '\t' << "effect" << '\t' << '\"' << group.at("effect").get<std::string>() << '\"' << '\n';

		file << "}" << '\n';
		file << '\n';
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
