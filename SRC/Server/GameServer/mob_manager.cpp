#include "stdafx.h"
#include <Core/Logging.hpp>
#include "char_interface.hpp"
#include "char_manager.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "regen.h"
#include "sectree.h"
#include "text_file_loader.h"
#include "questmanager.h"
#include "locale_service.h"

CMob::CMob()
{
	memset( &m_table, 0, sizeof(m_table) );

	for( size_t i=0 ; i < MOB_SKILL_MAX_NUM ; ++i )
	{
		m_mobSkillInfo[i].dwSkillVnum = 0;
		m_mobSkillInfo[i].bSkillLevel = 0;
		m_mobSkillInfo[i].vecSplashAttack.clear();
	}
}

CMob::~CMob()
{
}

void CMob::AddSkillSplash(int iIndex, uint32_t dwTiming, uint32_t dwHitDistance)
{
	if (iIndex >= MOB_SKILL_MAX_NUM || iIndex < 0)
		return;
	
	m_mobSkillInfo[iIndex].vecSplashAttack.push_back(TMobSplashAttackInfo(dwTiming, dwHitDistance));
}

CMobInstance::CMobInstance()
	: m_IsBerserk(false), m_IsGodSpeed(false), m_IsRevive(false)
{
	m_dwLastAttackedTime = get_dword_time();
	m_dwLastWarpTime = get_dword_time();

	memset( &m_posLastAttacked, 0, sizeof(m_posLastAttacked) );
}

CMobManager::CMobManager()
{
}

CMobManager::~CMobManager()
{
}

bool CMobManager::Initialize(TMobTable * pTable, int iSize)
{
	m_map_pkMobByVnum.clear();
	m_map_pkMobByName.clear();

	TMobTable * t = pTable;


	for (int i = 0; i < iSize; ++i, ++t)
	{
		CMob * pkMob = M2_NEW CMob;

		memcpy(&pkMob->m_table, t, sizeof(TMobTable));

		m_map_pkMobByVnum.insert(std::map<uint32_t, CMob *>::value_type(t->dwVnum, pkMob));
#ifdef ENABLE_MULTI_NAMES
		m_map_pkMobByName.insert(std::map<std::string, CMob *>::value_type(t->szLocaleName[DEFAULT_LANGUAGE], pkMob));
#else
		m_map_pkMobByName.insert(std::map<std::string, CMob *>::value_type(t->szLocaleName, pkMob));
#endif 

		int SkillCount = 0;

		for (int j = 0; j < MOB_SKILL_MAX_NUM; ++j)
			if (pkMob->m_table.Skills[j].dwVnum)
				++SkillCount;

#ifdef ENABLE_MULTI_NAMES
		const char* mobName = t->szLocaleName[DEFAULT_LANGUAGE];
#else
		const char* mobName = t->szLocaleName;
#endif
		LOG_TRACE("MOB: #{:<5} {:<30} LEVEL {} HP {} DEF {} EXP {} DROP_ITEM_VNUM {} SKILL_COUNT {}",
				t->dwVnum, mobName, t->bLevel, t->dwMaxHP, t->wDef, t->dwExp, t->dwDropItemVnum, SkillCount);


		if (t->bType == CHAR_TYPE_NPC || t->bType == CHAR_TYPE_WARP || t->bType == CHAR_TYPE_GOTO)
			CHARACTER_MANAGER::instance().RegisterRaceNum(t->dwVnum);

		quest::CQuestManager::instance().RegisterNPCVnum(t->dwVnum);
	}


	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szGroupFileName[FILE_NAME_LEN];
	char szGroupGroupFileName[FILE_NAME_LEN];

	snprintf(szGroupFileName, sizeof(szGroupGroupFileName),
			"%s/group.txt", LocaleService_GetBasePath().c_str());
	snprintf(szGroupGroupFileName, sizeof(szGroupGroupFileName),
			"%s/group_group.txt", LocaleService_GetBasePath().c_str());

	if (!LoadGroup(szGroupFileName))
	{
		LOG_ERROR("cannot load {}", szGroupFileName);
		thecore_shutdown();
	}
	if (!LoadGroupGroup(szGroupGroupFileName))
	{
		LOG_ERROR("cannot load {}", szGroupGroupFileName);
		thecore_shutdown();
	}
	// END_OF_LOCALE_SERVICE

	//exit(1);
	CHARACTER_MANAGER::instance().for_each_pc(std::bind(&CMobManager::RebindMobProto,this, std::placeholders::_1));
	return true;
}

void CMobManager::RebindMobProto(LPCHARACTER ch)
{
	if (ch->IsPC())
		return;

	const CMob * pMob = Get(ch->GetRaceNum());

	if (pMob)
		ch->SetProto(pMob);
}

const CMob * CMobManager::Get(uint32_t dwVnum)
{
	std::map<uint32_t, CMob *>::iterator it = m_map_pkMobByVnum.find(dwVnum);

	if (it == m_map_pkMobByVnum.end())
		return nullptr;

	return it->second;
}

const CMob * CMobManager::Get(const char * c_pszName, bool bIsAbbrev)
{
	std::map<std::string, CMob *>::iterator it;

	if (!bIsAbbrev)
	{
		it = m_map_pkMobByName.find(c_pszName);

		if (it == m_map_pkMobByName.end())
			return nullptr;

		return it->second;
	}

	int len = strlen(c_pszName);
	it = m_map_pkMobByName.begin();

	while (it != m_map_pkMobByName.end())
	{
		if (!strncmp(it->first.c_str(), c_pszName, len))
			return it->second;

		++it;
	}

	return nullptr;
}

void CMobManager::IncRegenCount(uint8_t bRegenType, uint32_t dwVnum, int iCount, int iTime)
{
	switch (bRegenType)
	{
		case REGEN_TYPE_MOB:
			m_mapRegenCount[dwVnum] += iCount * 86400. / iTime;
			break;

		case REGEN_TYPE_GROUP:
			{
				CMobGroup * pkGroup = CMobManager::Instance().GetGroup(dwVnum);
				if (!pkGroup)
					return;
				const std::vector<uint32_t> & c_rdwMembers = pkGroup->GetMemberVector();

				for (uint32_t i=0; i<c_rdwMembers.size(); i++)
					m_mapRegenCount[c_rdwMembers[i]] += iCount * 86400. / iTime;
			}
			break;

		case REGEN_TYPE_GROUP_GROUP:
			{
				std::map<uint32_t, CMobGroupGroup *>::iterator it = m_map_pkMobGroupGroup.find(dwVnum);

				if (it == m_map_pkMobGroupGroup.end())
					return;

				std::vector<uint32_t>& v = it->second->m_vec_dwMemberVnum;
				for (uint32_t i=0; i<v.size(); i++)
				{
					//m_mapRegenCount[v[i]] += iCount * 86400. / iTime / v.size();
					CMobGroup * pkGroup = CMobManager::Instance().GetGroup(v[i]);
					if (!pkGroup)
						return;
					const std::vector<uint32_t> & c_rdwMembers = pkGroup->GetMemberVector();

					for (uint32_t i=0; i<c_rdwMembers.size(); i++)
						m_mapRegenCount[c_rdwMembers[i]] += iCount * 86400. / iTime / v.size();
				}
			}
			break;
	}
}

void CMobManager::DumpRegenCount(const char* c_szFilename)
{
	FILE* fp = fopen(c_szFilename, "w");

	if (fp)
	{
		std::map<uint32_t, double>::iterator it;

		fprintf(fp,"MOB_VNUM\tCOUNT\n");

		for (it = m_mapRegenCount.begin(); it != m_mapRegenCount.end(); ++it)
		{
			fprintf(fp,"%u\t%g\n", it->first, it->second);
		}

		fclose(fp);
	}
}

uint32_t CMobManager::GetGroupFromGroupGroup(uint32_t dwVnum)
{
	std::map<uint32_t, CMobGroupGroup *>::iterator it = m_map_pkMobGroupGroup.find(dwVnum);

	if (it == m_map_pkMobGroupGroup.end())
		return 0;

	return it->second->GetMember();
}

CMobGroup * CMobManager::GetGroup(uint32_t dwVnum)
{
	std::map<uint32_t, CMobGroup *>::iterator it = m_map_pkMobGroup.find(dwVnum);

	if (it == m_map_pkMobGroup.end())
		return nullptr;

	return it->second;
}

bool CMobManager::LoadGroupGroup(const char * c_pszFileName)
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
			LOG_ERROR("LoadGroupGroup : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		TTokenVector * pTok;

		CMobGroupGroup * pkGroup = M2_NEW CMobGroupGroup(iVnum);

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				uint32_t dwMobVnum = 0;
				str_to_number(dwMobVnum, pTok->at(0).c_str());

				// ADD_MOB_GROUP_GROUP_PROB
				int prob = 1;
				if (pTok->size() > 1)
					str_to_number(prob, pTok->at(1).c_str());
				// END_OF_ADD_MOB_GROUP_GROUP_PROB

				if (dwMobVnum)
					pkGroup->AddMember(dwMobVnum);

				continue;
			}

			break;
		}

		loader.SetParentNode();

		m_map_pkMobGroupGroup.insert(std::make_pair((uint32_t)iVnum, pkGroup));
	}

	return true;
}

bool CMobManager::LoadGroup(const char * c_pszFileName)
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
			LOG_ERROR("LoadGroup : Syntax error {} : no vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		TTokenVector * pTok;

		if (!loader.GetTokenVector("leader", &pTok))
		{
			LOG_ERROR("LoadGroup : Syntax error {} : no leader, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		if (pTok->size() < 2)
		{
			LOG_ERROR("LoadGroup : Syntax error {} : no leader vnum, node {}", c_pszFileName, stName.c_str());
			loader.SetParentNode();
			continue;
		}

		CMobGroup * pkGroup = M2_NEW CMobGroup;

		pkGroup->Create(iVnum, stName);
		uint32_t vnum = 0;
		str_to_number(vnum, pTok->at(1).c_str());
		pkGroup->AddMember(vnum);

		LOG_TRACE("GROUP: {:<5} {}", iVnum, stName.c_str());
		LOG_TRACE("               {} {}", pTok->at(0).c_str(), pTok->at(1).c_str());

		for (int k = 1; k < 256; ++k)
		{
			char buf[4];
			snprintf(buf, sizeof(buf), "%d", k);

			if (loader.GetTokenVector(buf, &pTok))
			{
				LOG_TRACE("               {} {}", pTok->at(0).c_str(), pTok->at(1).c_str());
				uint32_t vnum = 0;
				str_to_number(vnum, pTok->at(1).c_str());
				pkGroup->AddMember(vnum);
				continue;
			}

			break;
		}

		loader.SetParentNode();
		m_map_pkMobGroup.insert(std::map<uint32_t, CMobGroup *>::value_type(iVnum, pkGroup));
	}

	return true;
}

