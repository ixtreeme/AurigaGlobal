#include "stdafx.h"
#include "utils.h"
#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "desc_client.h"
#include "db.h"
#include "log.h"
#include "skill.h"
#include "text_file_loader.h"
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
#include "group_text_parse_tree.h"
#ifdef __INGAME_WIKI__
#include <common/in_game_wiki.h>
#include "mob_manager.h"
#endif
std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];

#ifdef NEW_READ_COMMON_DROP_ITEM
bool ITEM_MANAGER::ReadCommonDropItemFile(const char *c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);
		loader.GetCurrentNodeName(&stName);

		int rank;

		if (!loader.GetTokenInteger("rank", &rank))
		{
			LOG_ERROR("CommonDropitem : Syntax error {} : no rank, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			return false;
		}

		TTokenVector *pTok;

		for (int k = 1; k < 1024; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				uint32_t vnum = 0;
				if (!GetVnumByOriginalName(pTok->at(3).c_str(), vnum))
				{
					str_to_number(vnum, pTok->at(3).c_str());
					if (!ITEM_MANAGER::instance().GetTable(vnum))
					{
						LOG_ERROR("CommonDropitem : there is no item node {} : vnum {}", stName.c_str(), vnum);
						return false;
					}
				}

				int lvlStart = 0;
				str_to_number(lvlStart, pTok->at(0).c_str());
				if (lvlStart < 1)
				{
					LOG_ERROR("CommonDropitem : lvlStart negative node {} : lvlstart {}", stName.c_str(), lvlStart);
					return false;
				}

				int lvlEnd = 0;
				str_to_number(lvlEnd, pTok->at(1).c_str());
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

				float pct = atof(pTok->at(2).c_str());

				// int count = 0;
				// str_to_number(count, pTok->at(4).c_str());

				uint32_t dwPct = (uint32_t)(pct * 10000.0f);

				if (lvlStart == 0)
					continue;

				LOG_TRACE("CommonDropItem ADD: Rank {}  LvlStart {}  LvlEnd {}   dwPct {}   Vnum {}", rank, lvlStart, lvlEnd, dwPct, vnum);

				g_vec_pkCommonDropItem[rank].push_back(CItemDropInfo(lvlStart, lvlEnd, dwPct, vnum));
			}
		}

		loader.SetParentNode();
	}

	return true;
}
#else
bool ITEM_MANAGER::ReadCommonDropItemFile(const char * c_pszFileName)
{
	FILE * fp = fopen(c_pszFileName, "r");

	if (!fp)
	{
		LOG_ERROR("Cannot open {}", c_pszFileName);
		return false;
	}

	char buf[1024];

	int lines = 0;

	while (fgets(buf, 1024, fp))
	{
		++lines;

		if (!*buf || *buf == '\n')
			continue;

		TDropItem d[MOB_RANK_MAX_NUM];
		char szTemp[64];

		memset(&d, 0, sizeof(d));

		char * p = buf;
		char * p2;

		for (int i = 0; i <= MOB_RANK_S_KNIGHT; ++i)
		{
			for (int j = 0; j < 6; ++j)
			{
				p2 = strchr(p, '\t');

				if (!p2)
					break;

				strlcpy(szTemp, p, MIN(sizeof(szTemp), (p2 - p) + 1));
				p = p2 + 1;

				switch (j)
				{
				case 0: break;
				case 1: str_to_number(d[i].iLvStart, szTemp);	break;
				case 2: str_to_number(d[i].iLvEnd, szTemp);	break;
				case 3: d[i].fPercent = atof(szTemp);	break;
				case 4: strlcpy(d[i].szItemName, szTemp, sizeof(d[i].szItemName));	break;
				case 5: str_to_number(d[i].iCount, szTemp);	break;
				}
			}

			uint32_t dwPct = (uint32_t) (d[i].fPercent * 10000.0f);
			uint32_t dwItemVnum = 0;

			if (!ITEM_MANAGER::instance().GetVnumByOriginalName(d[i].szItemName, dwItemVnum))
			{
				// 이름으로 못찾으면 번호로 검색
				str_to_number(dwItemVnum, d[i].szItemName);
				if (!ITEM_MANAGER::instance().GetTable(dwItemVnum))
				{
					LOG_ERROR("No such an item (name: {})", d[i].szItemName);
					fclose(fp);
					return false;
				}
			}

			if (d[i].iLvStart == 0)
				continue;

			g_vec_pkCommonDropItem[i].push_back(CItemDropInfo(d[i].iLvStart, d[i].iLvEnd, dwPct, dwItemVnum));
		}
	}

	fclose(fp);

	for (int i = 0; i < MOB_RANK_MAX_NUM; ++i)
	{
		std::vector<CItemDropInfo> & v = g_vec_pkCommonDropItem[i];
		std::sort(v.begin(), v.end());

		std::vector<CItemDropInfo>::iterator it = v.begin();

		LOG_TRACE("CommonItemDrop rank {}", i);

		while (it != v.end())
		{
			const CItemDropInfo & c = *(it++);
			LOG_TRACE("CommonItemDrop {} {} {} {}", c.m_iLevelStart, c.m_iLevelEnd, c.m_iPercent, c.m_dwVnum);
		}
	}

	return true;
}

#endif

bool ITEM_MANAGER::ReadSpecialDropItemFile(const char * c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);

		loader.GetCurrentNodeName(&stName);

		int iVnum;

		if (!loader.GetTokenInteger("vnum", &iVnum))
		{
			LOG_ERROR("ReadSpecialDropItemFile : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			return false;
		}

		LOG_TRACE("DROP_ITEM_GROUP {} {}", stName.c_str(), iVnum);

		TTokenVector * pTok;

		//
		std::string stType;
		int type = CSpecialItemGroup::NORMAL;
		if (loader.GetTokenString("type", &stType))
		{
			stl_lowers(stType);
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
			for (int k = 1; k < 1024; ++k) // @fixme148 256 -> 1024
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.GetTokenVector(buf, &pTok))
				{
					uint32_t apply_type = 0;
					int	apply_value = 0;
					str_to_number(apply_type, pTok->at(0).c_str());
					if (0 == apply_type)
					{
						apply_type = FN_get_apply_type(pTok->at(0).c_str());
						if (0 == apply_type)
						{
							LOG_ERROR("Invalid APPLY_TYPE {} in Special Item Group Vnum {}", pTok->at(0).c_str(), iVnum);
							return false;
						}
					}
					str_to_number(apply_value, pTok->at(1).c_str());
					if (apply_type > MAX_APPLY_NUM)
					{
						LOG_ERROR("Invalid APPLY_TYPE {} in Special Item Group Vnum {}", apply_type, iVnum);
						M2_DELETE(pkGroup);
						return false;
					}
					pkGroup->m_vecAttrs.push_back(CSpecialAttrGroup::CSpecialAttrInfo(apply_type, apply_value));
				}
				else
				{
					break;
				}
			}
			if (loader.GetTokenVector("effect", &pTok))
			{
				pkGroup->m_stEffectFileName = pTok->at(0);
			}
			loader.SetParentNode();
			m_map_pkSpecialAttrGroup.insert(std::make_pair(iVnum, pkGroup));
		}
		else
		{
			CSpecialItemGroup * pkGroup = M2_NEW CSpecialItemGroup(iVnum, type);
			for (int k = 1; k < 1024; ++k) // @fixme148 256 -> 1024
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.GetTokenVector(buf, &pTok))
				{
					const std::string& name = pTok->at(0);
					uint32_t dwVnum = 0;

					if (!GetVnumByOriginalName(name.c_str(), dwVnum))
					{
						if (name == "경험치" || name == "exp")
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
#ifdef ENABLE_WOLFMAN_CHARACTER
						else if (name == "bleeding")
						{
							dwVnum = CSpecialItemGroup::BLEEDING;
						}
#endif
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

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());
					int iProb = 0;
					str_to_number(iProb, pTok->at(2).c_str());

					int iRarePct = 0;
					if (pTok->size() > 3)
					{
						str_to_number(iRarePct, pTok->at(3).c_str());
					}

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

					continue;
				}

				break;
			}
			loader.SetParentNode();
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
		"%s/special_item_group.txt", LocaleService_GetBasePath().c_str());

	FILE *fp = fopen("special_item_group_vnum.txt", "w");
	if (!fp)
	{
		LOG_ERROR("could not open file ({})", "special_item_group_vnum.txt");
		return false;
	}

	CTextFileLoader loader;

	if (!loader.Load(szSpecialItemGroupFileName))
	{
		fclose(fp);
		return false;
	}

	std::string stName;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);

		loader.GetCurrentNodeName(&stName);

		int iVnum;

		if (!loader.GetTokenInteger("vnum", &iVnum))
		{
			LOG_ERROR("ConvSpecialDropItemFile : Syntax error {} : no vnum, node {}", szSpecialItemGroupFileName, stName.c_str());
			loader.SetParentNode();
			fclose(fp);
			return false;
		}

		std::string str;
		int type = 0;
		if (loader.GetTokenString("type", &str))
		{
			stl_lowers(str);
			if (str == "pct")
				type = 1;
			// @fixme148 BEGIN
			else if (str == "quest")
				type = 2;
			else if (str == "special")
				type = 3;
			else if (str == "attr")
				type = 4;
			// @fixme148 END
		}

		TTokenVector * pTok;

		fprintf(fp, "Group	%s\n", stName.c_str());
		fprintf(fp, "{\n");
		fprintf(fp, "	Vnum	%i\n", iVnum);
		if (type==1)
			fprintf(fp, "	Type	Pct\n");
			// @fixme148 BEGIN
		else if (type==2)
			fprintf(fp, "	Type	Quest\n");
		else if (type==3)
			fprintf(fp, "	Type	special\n");
		else if (type==4)
			fprintf(fp, "	Type	ATTR\n");
			// @fixme148 END

		for (int k = 1; k < 1024; ++k) // @fixme148 256 -> 1024
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				std::string& name = pTok->at(0);
				uint32_t dwVnum = 0;

				if (!GetVnumByOriginalName(name.c_str(), dwVnum))
				{
					if (
						name == "exp" ||
						name == "mob" ||
						name == "slow" ||
						name == "drain_hp" ||
						name == "poison" ||
#ifdef ENABLE_WOLFMAN_CHARACTER
						name == "bleeding" ||
#endif
						name == "group")
					{
						dwVnum = 0;
					}
					// @fixme148 BEGIN
					else if (name == "경험치")
					{
						dwVnum = 0;
						name = "exp";
					}
					// @fixme148 END
					else
					{
						str_to_number(dwVnum, name.c_str());
						if (!ITEM_MANAGER::instance().GetTable(dwVnum) && type!=4)
						{
							LOG_ERROR("ReadSpecialDropItemFile : there is no item {} : node {}", name.c_str(), stName.c_str());
							fclose(fp);

							return false;
						}
					}
				}

				int iCount = 0;
				str_to_number(iCount, pTok->at(1).c_str());
				int iProb = 0;
				// @fixme148 BEGIN
				if (pTok->size() > 2)
					str_to_number(iProb, pTok->at(2).c_str());
				// @fixme148 END

				int iRarePct = 0;
				if (pTok->size() > 3)
					str_to_number(iRarePct, pTok->at(3).c_str());

				// @fixme148 BEGIN
				if (type==4)
					fprintf(fp, "	%d	%u	%d\n", k, dwVnum, iCount);
				// @fixme148 END
				else
				{
					//    1   "기술 수련서"   1   100
					if (0 == dwVnum)
						fprintf(fp, "	%d	%s	%d	%d\n", k, name.c_str(), iCount, iProb);
					else
						fprintf(fp, "	%d	%u	%d	%d\n", k, dwVnum, iCount, iProb);
				}

				continue;
			}

			break;
		}
		std::string effect;
		if (loader.GetTokenString("effect", &effect))
			fprintf(fp, "	effect	\"%s\"\n", effect.c_str());
		fprintf(fp, "}\n");
		fprintf(fp, "\n");

		loader.SetParentNode();
	}

	fclose(fp);
	return true;
}

bool ITEM_MANAGER::ReadEtcDropItemFile(const char * c_pszFileName)
{
	FILE * fp = fopen(c_pszFileName, "r");

	if (!fp)
	{
		LOG_ERROR("Cannot open {}", c_pszFileName);
		return false;
	}

	char buf[512];

	int lines = 0;

	while (fgets(buf, 512, fp))
	{
		++lines;

		if (!*buf || *buf == '\n')
			continue;

		char szItemName[256];
		float fProb = 0.0f;

		strlcpy(szItemName, buf, sizeof(szItemName));
		char * cpTab = strrchr(szItemName, '\t');

		if (!cpTab)
			continue;

		*cpTab = '\0';
		fProb = atof(cpTab + 1);

		if (!*szItemName || fProb == 0.0f)
			continue;

		uint32_t dwItemVnum;

		if (!ITEM_MANAGER::instance().GetVnumByOriginalName(szItemName, dwItemVnum))
		{
#ifdef ENABLE_BUG_FIXES
			str_to_number(dwItemVnum, szItemName);
			if (!ITEM_MANAGER::instance().GetTable(dwItemVnum))
			{
				LOG_ERROR("No such an item (name/vnum: {})", szItemName);
				fclose(fp);
				return false;
			}
#else
			LOG_ERROR("No such an item (name: {})", szItemName);
			fclose(fp);
			return false;
#endif
		}

		m_map_dwEtcItemDropProb[dwItemVnum] = (uint32_t) (fProb * 10000.0f);
		LOG_TRACE("ETC_DROP_ITEM: {} prob {:f}", szItemName, fProb);
	}

	fclose(fp);
	return true;
}

bool ITEM_MANAGER::ReadMonsterDropItemGroup(const char * c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
		return false;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		std::string stName("");

		loader.GetCurrentNodeName(&stName);

		if (strncmp (stName.c_str(), "kr_", 3) == 0)
			continue;

		loader.SetChildNode(i);

		int iMobVnum = 0;
		int iKillDrop = 0;
		int iLevelLimit = 0;

		std::string strType("");

		if (!loader.GetTokenString("type", &strType))
		{
			LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : no type (kill|drop), node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			return false;
		}

		if (!loader.GetTokenInteger("mob", &iMobVnum))
		{
			LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : no mob vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			return false;
		}

		if (strType == "kill")
		{
			if (!loader.GetTokenInteger("kill_drop", &iKillDrop))
			{
				LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : no kill drop count, node {}", c_pszFileName, stName.c_str());
				loader.SetParentNode();
				return false;
			}
		}
		else
		{
			iKillDrop = 1;
		}

		if ( strType == "limit" )
		{
			if ( !loader.GetTokenInteger("level_limit", &iLevelLimit) )
			{
				LOG_ERROR("ReadmonsterDropItemGroup : Syntax error {} : no level_limit, node {}", c_pszFileName, stName.c_str());
				loader.SetParentNode();
				return false;
			}
		}
		else
		{
			iLevelLimit = 0;
		}

		LOG_TRACE("MOB_ITEM_GROUP {} [{}] {} {}", stName.c_str(), strType.c_str(), iMobVnum, iKillDrop);

		if (iKillDrop == 0)
		{
			loader.SetParentNode();
			continue;
		}

		TTokenVector* pTok = nullptr;

		if (strType == "kill")
		{
			CMobItemGroup * pkGroup = M2_NEW CMobItemGroup(iMobVnum, iKillDrop, stName);

			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.GetTokenVector(buf, &pTok))
				{
					//1, "               %s %s", pTok->at(0).c_str(), pTok->at(1).c_str());
					std::string& name = pTok->at(0);
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

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount<1)
					{
						LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {} : vnum {}, count {}", name.c_str(), stName.c_str(), dwVnum, iCount);
						return false;
					}

					int iPartPct = 0;
					str_to_number(iPartPct, pTok->at(2).c_str());

					if (iPartPct == 0)
					{
						LOG_ERROR("ReadMonsterDropItemGroup : there is no drop percent for item {} : node {} : vnum {}, count {}, pct {}", name.c_str(), stName.c_str(), dwVnum, iCount, iPartPct);
						return false;
					}

					int iRarePct = 0;
					str_to_number(iRarePct, pTok->at(3).c_str());
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
					continue;
				}

				break;
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

			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.GetTokenVector(buf, &pTok))
				{
					std::string& name = pTok->at(0);
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

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount < 1)
					{
						LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
						M2_DELETE(pkGroup);

						return false;
					}

					float fPercent = atof(pTok->at(2).c_str());

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
					continue;
				}

				break;
			}
			if (bNew)
				m_map_pkDropItemGroup.insert(std::map<uint32_t, CDropItemGroup*>::value_type(iMobVnum, pkGroup));

		}
		else if ( strType == "limit" )
		{
			CLevelItemGroup* pkLevelItemGroup = M2_NEW CLevelItemGroup(iLevelLimit);

			for ( int k=1; k < 256; k++ )
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if ( loader.GetTokenVector(buf, &pTok) )
				{
					std::string& name = pTok->at(0);
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

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount < 1)
					{
						LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
						M2_DELETE(pkLevelItemGroup);
						return false;
					}

					float fPct = atof(pTok->at(2).c_str());
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
					continue;
				}

				break;
			}

			m_map_pkLevelItemGroup.insert(std::map<uint32_t, CLevelItemGroup*>::value_type(iMobVnum, pkLevelItemGroup));
		}
		else if (strType == "thiefgloves")
		{
			CBuyerThiefGlovesItemGroup* pkGroup = M2_NEW CBuyerThiefGlovesItemGroup(0, iMobVnum, stName);

			for (int k = 1; k < 256; ++k)
			{
				char buf[4];
				snprintf(buf, sizeof(buf), "%d", k);

				if (loader.GetTokenVector(buf, &pTok))
				{
					std::string& name = pTok->at(0);
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

					int iCount = 0;
					str_to_number(iCount, pTok->at(1).c_str());

					if (iCount < 1)
					{
						LOG_ERROR("ReadMonsterDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());
						M2_DELETE(pkGroup);

						return false;
					}

					float fPercent = atof(pTok->at(2).c_str());

					uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

					LOG_TRACE("        name {} pct {} count {}", name.c_str(), dwPct, iCount);
					pkGroup->AddItem(dwVnum, dwPct, iCount);

					continue;
				}

				break;
			}

			m_map_pkGloveItemGroup.insert(std::map<uint32_t, CBuyerThiefGlovesItemGroup*>::value_type(iMobVnum, pkGroup));
		}
		else
		{
			LOG_ERROR("ReadMonsterDropItemGroup : Syntax error {} : invalid type {} (kill|drop), node {}", c_pszFileName, strType.c_str(), stName.c_str());
			loader.SetParentNode();
			return false;
		}

		loader.SetParentNode();
	}

	return true;
}

bool ITEM_MANAGER::ReadDropItemGroup(const char * c_pszFileName)
{
	CTextFileLoader loader;

	if (!loader.Load(c_pszFileName))
		return false;

	std::string stName;

	for (uint32_t i = 0; i < loader.GetChildNodeCount(); ++i)
	{
		loader.SetChildNode(i);

		loader.GetCurrentNodeName(&stName);

		int iVnum;
		int iMobVnum;

		if (!loader.GetTokenInteger("vnum", &iVnum))
		{
			LOG_ERROR("ReadDropItemGroup : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			return false;
		}

		if (!loader.GetTokenInteger("mob", &iMobVnum))
		{
			LOG_ERROR("ReadDropItemGroup : Syntax error {} : no mob vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			return false;
		}

		LOG_TRACE("DROP_ITEM_GROUP {} {}", stName.c_str(), iMobVnum);

		TTokenVector * pTok;

		auto it = m_map_pkDropItemGroup.find(iMobVnum);

		CDropItemGroup* pkGroup;

		if (it == m_map_pkDropItemGroup.end())
			pkGroup = M2_NEW CDropItemGroup(iVnum, iMobVnum, stName);
		else
			pkGroup = it->second;

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				std::string& name = pTok->at(0);
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

				float fPercent = atof(pTok->at(1).c_str());

				uint32_t dwPct = (uint32_t)(10000.0f * fPercent);

				int iCount = 1;
				if (pTok->size() > 2)
					str_to_number(iCount, pTok->at(2).c_str());

				if (iCount < 1)
				{
					LOG_ERROR("ReadDropItemGroup : there is no count for item {} : node {}", name.c_str(), stName.c_str());

					if (it == m_map_pkDropItemGroup.end())
						M2_DELETE(pkGroup);

					return false;
				}

				LOG_TRACE("        {} {} {}", name.c_str(), dwPct, iCount);
				pkGroup->AddItem(dwVnum, dwPct, iCount);
				continue;
			}

			break;
		}

		if (it == m_map_pkDropItemGroup.end())
			m_map_pkDropItemGroup.insert(std::map<uint32_t, CDropItemGroup*>::value_type(iMobVnum, pkGroup));

		loader.SetParentNode();
	}

	return true;
}

bool ITEM_MANAGER::ReadItemVnumMaskTable(const char * c_pszFileName)
{
	FILE *fp = fopen(c_pszFileName, "r");
	if (!fp)
	{
		return false;
	}

	int ori_vnum, new_vnum;
	while (fscanf(fp, "%u %u", &ori_vnum, &new_vnum) != EOF)
	{
		m_map_new_to_ori.insert (TMapDW2DW::value_type (new_vnum, ori_vnum));
	}
	fclose(fp);
	return true;
}
