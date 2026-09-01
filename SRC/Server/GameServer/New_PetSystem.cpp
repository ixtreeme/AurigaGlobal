

#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "utils.h"
#include "vector.h"
#include "char_interface.hpp"
#include "sectree_manager.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "mob_manager.h"
#include "New_PetSystem.h"
#include <common/VnumHelper.h>
#include "packet.h"
#include "item_manager.h"
#include "item.h"
#include "db.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/movement_components.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/components/pet_mount_components.hpp"

//#define DISABLE_TRADE_UNSUMMON // this disable the unsummon of pet when a excange/trade/shop/myshop/safebox windows is open, MAKE SURE to have set the items with vnum 55401/55402/55403/55404 with antiflag ANTI_SAFEBOX | ANTI_PKDROP | ANTI_DROP | ANTI_SELL | ANTI_GIVE | ANTI_STACK | ANTI_MYSHOP
//USE AT OWN YOUR RISK

namespace
{
entt::entity FindSummonItemByVID(uint32_t vid)
{
	return ItemSystem::FindItemByVID(vid);
}

bool IsSummonItemOwnedBy(uint32_t vid, LPCHARACTER owner)
{
	const entt::entity item = FindSummonItemByVID(vid);
	return item != entt::null && ItemSystem::GetItemOwner(item) == AIHelpers::EcsOf(owner);
}

}


extern int passes_per_sec;
EVENTINFO(newpetsystem_event_info)
{
	CNewPetSystem* pPetSystem;
};

EVENTINFO(newpetsystem_event_infoe)
{
	CNewPetSystem* pPetSystem;
};

// PetSystem을 update 해주는 event.
// PetSystem은 CHRACTER_MANAGER에서 기존 FSM으로 update 해주는 기존 chracters와 달리,
// Owner의 STATE를 update 할 때 _UpdateFollowAI 함수로 update 해준다.
// 그런데 owner의 state를 update를 CHRACTER_MANAGER에서 해주기 때문에,
// petsystem을 update하다가 pet을 unsummon하는 부분에서 문제가 생겼다.
// (CHRACTER_MANAGER에서 update 하면 chracter destroy가 pending되어, CPetSystem에서는 dangling 포인터를 가지고 있게 된다.)
// 따라서 PetSystem만 업데이트 해주는 event를 발생시킴.
EVENTFUNC(newpetsystem_update_event)
{
	newpetsystem_event_info* info = dynamic_cast<newpetsystem_event_info*>( event->info );
	if ( info == nullptr)
	{
		LOG_ERROR("check_speedhack_event> <Factor> Null pointer");
		return 0;
	}

	CNewPetSystem*	pPetSystem = info->pPetSystem;

	if (nullptr == pPetSystem)
		return 0;


	pPetSystem->Update(0);
	// 0.25초마다 갱신.
	return PASSES_PER_SEC(1) / 4;
}

EVENTFUNC(newpetsystem_expire_event)
{
	newpetsystem_event_infoe* info = dynamic_cast<newpetsystem_event_infoe*>(event->info);
	if (info == nullptr)
	{
		LOG_ERROR("check_speedhack_event> <Factor> Null pointer");
		return 0;
	}

	CNewPetSystem*	pPetSystem = info->pPetSystem;

	if (nullptr == pPetSystem)
		return 0;


	pPetSystem->UpdateTime();
	// 0.25초마다 갱신.
	return PASSES_PER_SEC(1);
}


/// NOTE: 1캐릭터가 몇개의 펫을 가질 수 있는지 제한... 캐릭터마다 개수를 다르게 할거라면 변수로 넣등가... 음..
/// 가질 수 있는 개수와 동시에 소환할 수 있는 개수가 틀릴 수 있는데 이런건 기획 없으니 일단 무시
const float PET_COUNT_LIMIT = 3;

///////////////////////////////////////////////////////////////////////////////////////
//  CPetActor
///////////////////////////////////////////////////////////////////////////////////////

CNewPetActor::CNewPetActor(LPCHARACTER owner, uint32_t vnum, uint32_t options)
{
	m_dwVnum = vnum;
	m_dwVID = 0;
	m_dwlevel = 1;
	m_dwlevelstep = 0;
	m_dwExpFromMob = 0;
	m_dwExpFromItem = 0;
	m_dwexp = 0;
	m_dwexpitem = 0;
	m_dwOptions = options;
	m_dwLastActionTime = 0;

	m_pkChar = nullptr;
	m_pkOwner = owner;

	m_originalMoveSpeed = 0;

	m_dwSummonItemVID = 0;
	m_dwSummonItemID = 0;
	m_dwSummonItemVnum = 0;

	m_dwevolution = 0;
	m_dwduration = 0;
	m_dwtduration = 0;
#ifdef ENABLE_NEW_PET_EDITS
	lMinAge = 0;
	dwMinAge = 0;
	m_idx = 0;
#endif
	m_dwTimePet = 0;
	m_dwslotimm = 0;
	m_dwImmTime = 0;

	m_dwskill[0] = 0;
	m_dwskill[1] = 0;
	m_dwskill[2] = 0;
	m_dwskill[3] = 0;

	for (int s = 0; s < 9; ++s)
	{
		m_dwpetslotitem[s] = -1;
	}


	//Riferimento allo slot -1 se non disp 0 disp non set > 0 setted
	m_dwskillslot[0] = -1;
	m_dwskillslot[1] = -1;
	m_dwskillslot[2] = -1;
	m_dwskillslot[3] = -1;

	for (int x = 0; x < 3; ++x) //Inizializzazione bonus del pet
	{
		int btype[3] = { 69, 63, 119};
		m_dwbonuspet[x][0] = btype[x];
		m_dwbonuspet[x][1] = 0;
	}
}

CNewPetActor::~CNewPetActor()
{
	this->Unsummon();

	m_pkOwner = nullptr;
}

void CNewPetActor::SetName(const char* name)
{
	//std::string petName = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pkOwner)).data();
	std::string petName = "";

	if (nullptr != m_pkOwner &&
		nullptr == name &&
		nullptr != ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(m_pkOwner)).data())
	{
		petName += "'s Pet";
	}
	else
		petName += name;

	if (true == IsSummoned())
		m_pkChar->SetName(petName);

	m_name = petName;
}

void CNewPetActor::SetItemCube(int pos, int invpos)
{
	if (m_dwpetslotitem[pos] != -1) //Controllo se l'item e' gia settato
		return;
	if (pos > 180 || pos < 0)
		return;

	m_dwpetslotitem[pos] = invpos;
}

void CNewPetActor::ItemCubeFeed(int type)
{
	for (int i = 0; i < 9; ++i)
	{
		if (m_dwpetslotitem[i] != -1)
		{
			const entt::entity itemxp = ItemSystem::GetInventoryItem(AIHelpers::EcsOf(m_pkOwner), m_dwpetslotitem[i]);
			if (!ItemSystem::IsValidItem(itemxp))
				return;
			if (ItemSystem::GetItemID(itemxp) == ItemSystem::GetItemID(FindSummonItemByVID(this->GetSummonItemVID())) || ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(m_pkOwner)) || m_pkOwner->GetMyShop() || m_pkOwner->GetShopOwner() || m_pkOwner->IsOpenSafebox() || m_pkOwner->IsCubeOpen())
				return;
			if(type == 1)
			{
				if ((ItemSystem::GetItemVnum(itemxp) >= 55401 && ItemSystem::GetItemVnum(itemxp) <= 55411 )|| (ItemSystem::GetItemVnum(itemxp) >= 55701 && ItemSystem::GetItemVnum(itemxp) <= 55711 )||( ItemSystem::GetItemVnum(itemxp) == 55001) )
				{
					if(ItemSystem::GetItemVnum(itemxp) == 55001)
					{
						int tmp_dur = m_dwtduration/2;
						if (m_dwduration+tmp_dur > m_dwtduration)
							m_dwduration = m_dwtduration;
						else
							m_dwduration += tmp_dur;
					}
					else
					{
						int tmp_dur = m_dwtduration * 3 / 100;
						if (m_dwduration+tmp_dur > m_dwtduration)
							m_dwduration = m_dwtduration;
						else
							m_dwduration += tmp_dur;
						ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetDuration %d %d", m_dwduration, m_dwtduration);
					}
					ItemSystem::DestroyItemEntityEcs(itemxp, "PET_CUBE_FEED");
				}
			}
			else if(type == 3)
			{
				if (GetLevel() < 120)
				{
					if(ItemSystem::GetItemType(itemxp) == 1 || ItemSystem::GetItemType(itemxp) == 2)
					{
						SetExp(ItemSystem::GetItemShopBuyPrice(itemxp) / 2, 1);
						ItemSystem::DestroyItemEntityEcs(itemxp, "PET_CUBE_FEED");
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 742, "");
				}
#endif
			}
		}
	}
	for (int s = 0; s < 9; ++s)
	{
		m_dwpetslotitem[s] = -1;
	}
}

#ifdef ENABLE_NEW_PET_EDITS
bool CNewPetActor::IncreasePetSkill(int iSlot, int iType)
#else
bool CNewPetActor::IncreasePetSkill(int skill)
#endif
{
#ifdef ENABLE_NEW_PET_EDITS
	int idx = m_dwskillslot[iSlot];
	if (idx == -1)
		return false;

	if (m_dwskill[iSlot] >= 10) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 58, "");
#endif
		return false;
	}

	TItemPos Cell;
	Cell.cell = iType;
#ifdef ENABLE_EXTRA_INVENTORY
	Cell.window_type = EXTRA_INVENTORY;
#else
	Cell.window_type = INVENTORY;
#endif
	const entt::entity bookItem = ItemSystem::GetItem(AIHelpers::EcsOf(m_pkOwner), Cell);
	if (!ItemSystem::IsValidItem(bookItem))
		return false;

	iType = ItemSystem::GetItemValue(bookItem, 0);
	if ((iType > 12) || (iType < 1) || ((idx != 0) && (idx != iType)) || (ItemSystem::GetItemType(bookItem) != ITEM_TYPE_PET))
		return false;

	for (int i = 0; i < 4; i++) {
		if ((iType == m_dwskillslot[i]) && (iSlot != i)) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 55, "");
#endif
			return false;
		}
	}

	ItemSystem::ConsumeItemEcs(
		bookItem, 1);

	if (idx == 0)
		m_dwskillslot[iSlot] = iType;

	m_dwskill[iSlot] += 1;

#ifdef TEXTS_IMPROVEMENT
	if (m_dwskill[iSlot] == 1) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 57, "");
	}
	else {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 56, "%d", m_dwskill[iSlot]);
	}
#endif
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", iSlot, m_dwskillslot[iSlot], m_dwskill[iSlot]);

	ClearBuff();
	GiveBuff();

	return true;
#else
	if (GetLevel() < 80 && m_dwevolution < 3)
		return false;
	for (int i = 0; i < 4; ++i)
	{ //Itero gli slot per cercare la skill
		if (m_dwskillslot[i] == skill)
		{  //Se trova la skill o la aumenta oppure e' gi?max
			if (m_dwskill[i] < 20)
			{
				m_dwskill[i] += 1;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 743, "%d", m_dwskill[i]);
#endif
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, m_dwskillslot[i], m_dwskill[i]);
				return true;
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 744, "");
#endif
				return false;
			}
		}
	}

	for (int i = 0; i < 4; ++i)
	{
		if (m_dwskillslot[i] == 0)
		{ //Controllo se trovo uno slot vuoto abilitato
			m_dwskillslot[i] = skill;
			m_dwskill[i] = 1;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 745, "");
#endif
			ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, m_dwskillslot[i], m_dwskill[i]);
			return true;
		}
	}

	/* Qualora il pet non soddisfi le condizioni precedenti
	   Allora tutti gli slot sono pieni e quind non pu?
	   imparare nuove skill
	*/
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 745, "");
#endif
	return false;
#endif
}

#ifdef ENABLE_NEW_PET_EDITS
bool CNewPetActor::IncreasePetSkillByBook(entt::entity bookItemEntity)
{
	const entt::entity bookItem = bookItemEntity;
	if (!ItemSystem::IsValidItem(bookItem))
		return false;

	if (ItemSystem::GetItemType(bookItem) != ITEM_TYPE_PET)
		return false;

	int iType = ItemSystem::GetItemValue(bookItem, 0);
	if ((iType > 12) || (iType < 1))
		return false;

	int ret = 0;
	bool bContinue = false;
	for (int i = 0; i < 4; i++) {
		if (iType == m_dwskillslot[i]) {
			ret = i;
			bContinue = true;
			break;
		}
	}

	if (!bContinue) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 54, "");
#endif
		return false;
	}

	if (m_dwskill[ret] >= 10) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 58, "");
#endif
		return false;
	}

	char szName[128];
	snprintf(szName, sizeof(szName), "pet_skills.%d", iType);
	int iLast = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(m_pkOwner), szName);
	int iTime = iLast - get_global_time();
	if (iTime > 0) {
		if (AffectSystem::FindAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_SKILL_NO_BOOK_DELAY))
			AffectSystem::RemoveAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_SKILL_NO_BOOK_DELAY);
		else {
			int iHours = iTime / 3600;
			int iMinutes = (iTime - (iHours * 3600)) / 60;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 51, "%d#%d", iHours, iMinutes);
#endif
			return false;
		}
	}

	ItemSystem::ConsumeItemEcs(
		bookItem, 1);

	if (number(1, 100) < 30) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 52, "");
#endif
		return false;
	}

	m_dwskill[ret] += 1;

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 56, "%d", m_dwskill[ret]);
#endif
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", ret, m_dwskillslot[ret], m_dwskill[ret]);

	ClearBuff();
	GiveBuff();

	ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(m_pkOwner), szName, get_global_time() + (3600 * 3));

	return true;
}

int CNewPetActor::ResetSkills()
{
	bool bContinue = false;
	for (int i = 0; i < 4; i++) {
		if (m_dwskillslot[i] > 0) {
			bContinue = true;
			break;
		}
	}

	if (!bContinue)
		return 3;

	for (int i = 0; i < 4; i++) {
		if (m_dwskillslot[i] > 0)
			m_dwskillslot[i] = 0;

		m_dwskill[i] = 0;
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, m_dwskillslot[i], m_dwskill[i]);
	}

	ClearBuff();
	GiveBuff();

	return 1;
}

int CNewPetActor::ResetSkill(int iType)
{
	int ret = 0;
	bool bContinue = false;
	for (int i = 0; i < 4; i++) {
		if (m_dwskillslot[i] == iType) {
			ret = i;
			bContinue = true;
			break;
		}
	}

	if (!bContinue)
		return 3;

	m_dwskillslot[ret] = 0;
	m_dwskill[ret] = 0;
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", ret, m_dwskillslot[ret], m_dwskill[ret]);

	ClearBuff();
	GiveBuff();

	return 1;
}
#endif

bool CNewPetActor::IncreasePetEvolution()
{
	if (m_dwevolution < 3)
	{
		if ((GetLevel() >= 40 && m_dwevolution < 1 )||( GetLevel() >= 60 && m_dwevolution < 2 )||( GetLevel() >= 80 && m_dwevolution < 3) )
		{
			m_dwevolution += 1;
			m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 1, GetLevel(), 1);
			ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetEvolution %d", m_dwevolution);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 747, "%d", m_dwevolution);
#endif
			if (m_dwevolution == 3)
			{
				const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());
				if (ItemSystem::IsValidItem(pSummonItem)){
					Unsummon();
					Summon("Noname", pSummonItem, false);
				}
			}

#ifdef ENABLE_NEW_PET_EDITS
			SetLevel(GetLevel() + 1);
			m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 1, GetLevel(), 1);
#ifdef ENABLE_NEW_PET_EDITS
			IncreasePetBonus();
#endif
			m_dwlevelstep = 0;
			m_dwexp = 0;
			m_dwexpitem = 0;
			m_pkChar->SetExp(0);
			ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
#endif
			int idx = m_dwevolution - 1;
			m_dwskill[idx] = 0;
			m_dwskillslot[idx] = 0;
			ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", idx, m_dwskillslot[idx], m_dwskill[idx]);
		}
		else
			return false;
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 748, "");
#endif
		return false;
	}
	return true;
}

void CNewPetActor:: IncreasePetBonus()
{
	int tmplevel = GetLevel();
	if (tmplevel % 5 == 0) {
		m_dwbonuspet[0][1] += float(number(1, 6));

	}
	if (tmplevel % 7 == 0) {
		m_dwbonuspet[1][1] += float(number(1, 6));
	}
	if (tmplevel % 4 == 0) {
		m_dwbonuspet[2][1] += float(number(1, 6));
	}
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetBonus %d %d %d", m_dwbonuspet[0][1], m_dwbonuspet[1][1], m_dwbonuspet[2][1]);
	const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());
	if (ItemSystem::IsValidItem(pSummonItem)){
		for (int b = 0; b < 3; b++){
			ItemSystem::SetItemForceAttributeEcs(pSummonItem, b, 1, m_dwbonuspet[b][1]);
		}

	}
}

void CNewPetActor::SetNextExp(int nextExp)
{
	m_dwExpFromMob = (nextExp/10)* 9;
	//m_dwExpFromMob = nextExp;
	//m_dwExpFromItem = 0;
	m_dwExpFromItem = nextExp - m_dwExpFromMob;
}

void CNewPetActor::SetLevel(uint32_t level)
{
	m_pkChar->SetLevel(static_cast<char>(level));
	m_dwlevel = level;
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetLevel %d", m_dwlevel);
	SetNextExp(m_pkChar->PetGetNextExp());
	const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());
	if (ItemSystem::IsValidItem(pSummonItem)) {
#ifdef ENABLE_NEW_PET_EDITS
	ItemSystem::SetItemForceAttributeEcs(pSummonItem, 3, 1, m_dwlevel);
#else
	ItemSystem::SetItemSocket(pSummonItem, 1, m_dwlevel);
#endif
	}
}

void CNewPetActor::SetEvolution(int lv)
{
	if (lv == 40)
		m_dwevolution = 1;
	else if (lv == 60)
		m_dwevolution = 2;
	else if (lv == 80)
		m_dwevolution = 3;
}

void CNewPetActor::SetExp(uint32_t exp, int mode) {
	if (exp < 0)
		exp = MAX(m_dwexp - exp, 0);

	if(mode == 0) {
#ifdef ENABLE_NEW_PET_EDITS
		if (GetExp() + exp >= (uint32_t) GetNextExpFromMob())
#else
		if(GetExp() + exp >= (uint32_t) GetNextExpFromMob() && GetExpI() >= (uint32_t) GetNextExpFromItem())
#endif
		{
			if(GetEvolution() == 0 && GetLevel() == 40) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				m_pkChar->SetExp(m_dwexp - 1);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp - 1, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			}
			else if(GetEvolution() <= 1 && GetLevel() == 60) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				m_pkChar->SetExp(m_dwexp - 1);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp - 1, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			}
			else if(GetEvolution() <= 2 && GetLevel() == 80) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				m_pkChar->SetExp(m_dwexp - 1);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp - 1, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			}
		}
	}
	else if(mode == 1) {
#ifdef ENABLE_NEW_PET_EDITS
		if(GetExp() >= (uint32_t) GetNextExpFromMob())
#else
		if(GetExpI() + exp >= (uint32_t) GetNextExpFromItem() && GetExp() >= (uint32_t) GetNextExpFromMob())
#endif
		{
			if(GetEvolution() == 0 && GetLevel() == 40) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				m_pkChar->SetExp(m_dwexp);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			}
			else if(GetEvolution() == 1 && GetLevel() == 60) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				m_pkChar->SetExp(m_dwexp);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			}
			else if(GetEvolution() == 2 && GetLevel() == 80) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				m_pkChar->SetExp(m_dwexp);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			}
		}
	}

	if (mode == 0)  {
		if (GetExp() + exp >= (uint32_t) GetNextExpFromMob())  {
#ifndef ENABLE_NEW_PET_EDITS
			if (GetExpI() >= (uint32_t) GetNextExpFromItem())
#endif
			{
				SetLevel(GetLevel() + 1);
				m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 1, GetLevel(), 1);
#ifndef ENABLE_NEW_PET_EDITS
				IncreasePetBonus();
#endif
				m_dwlevelstep = 0;
				m_dwexp = 0;
				m_dwexpitem = 0;
				m_pkChar->SetExp(0);
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
				//SetEvolution(GetLevel());
				return;
			}
#ifndef ENABLE_NEW_PET_EDITS
			else  {
				m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 25, GetLevel(), 1);
				m_dwlevelstep = 4;
				exp = GetNextExpFromMob() - GetExp();
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
			}
#endif
		}

		m_dwexp += exp;
		m_pkChar->SetExp(m_dwexp);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
		if (GetLevelStep() < 4) {
			uint32_t dwNextExpQuart = GetNextExpFromMob() / 4;
			if (m_dwexp >= dwNextExpQuart * 3 && m_dwlevelstep == 2) {
				m_dwlevelstep = 3;
				m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 25, GetLevel(), 1);
			} else if (m_dwexp >= dwNextExpQuart * 2 && m_dwlevelstep == 1) {
				m_dwlevelstep = 2;
				m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 25, GetLevel(), 1);
			} else if (m_dwexp >= dwNextExpQuart && m_dwlevelstep == 0)  {
				m_dwlevelstep = 1;
				m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 25, GetLevel(), 1);
			}
		}
	} else if (mode == 1)  {
		if (GetExpI() + exp >= (uint32_t) GetNextExpFromItem()) {
			if (GetExp() >= (uint32_t) GetNextExpFromMob())
			{
				m_dwexpitem = GetExpI() + exp - GetNextExpFromItem();
				m_dwexp = 0;
				m_pkChar->SetExp(0);
				m_dwlevelstep = 0;
				SetLevel(GetLevel() + 1);
				m_pkChar->SendPetLevelUpEffect(ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar)), 1, GetLevel(), 1);
#ifndef ENABLE_NEW_PET_EDITS
				IncreasePetBonus();
#endif
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
				return;
			} else  {
				exp = GetNextExpFromItem() - GetExpI();
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
			}
		}

		m_dwexpitem += exp;
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
	}

}

bool CNewPetActor::Mount()
{
	if (nullptr == m_pkOwner)
		return false;

	if (true == HasOption(EPetOption_Mountable))
		MountSystem::SetMountVnum(AIHelpers::EcsOf(m_pkOwner), m_dwVnum);

	return MountSystem::GetMountVnum(AIHelpers::EcsOf(m_pkOwner)) == m_dwVnum;;
}

void CNewPetActor::UpdateTime(bool now)
{
	m_dwTimePet += 1;
	if (m_dwTimePet >= 60 || now) {
		const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());
		if (ItemSystem::IsValidItem(pSummonItem)) {
			lMinAge = get_global_time() - dwMinAge;
			if (lMinAge >= 1296000 && m_dwskillslot[3] == -1) {
				m_dwskill[3] = 0;
				m_dwskillslot[3] = 0;
				ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 3, m_dwskillslot[3], m_dwskill[3]);
			}

			int idx = m_idx;
			if (m_idx != 1 && lMinAge >= 86400) {
				m_idx = 1;
			}

			if (m_idx != 2 && lMinAge >= 950400) {
				m_idx = 2;
			}

			if (m_idx != 3 && lMinAge >= 2246400) {
				m_idx = 3;
			}

			if (m_idx != 4 && lMinAge >= 4147200) {
				m_idx = 4;
			}

			if (idx != m_idx || now) {
				ClearBuff();
				GiveBuff();
			}
		}

		if (m_dwtduration > 525600)
			return;

		m_dwduration -= 1;
		m_dwTimePet = 0;
		if (ItemSystem::IsValidItem(pSummonItem)){
#ifdef ENABLE_NEW_PET_EDITS
			ItemSystem::SetItemSocket(pSummonItem, 1, m_dwduration);
#else
			ItemSystem::SetItemForceAttributeEcs(pSummonItem, 3, 1, m_dwduration);
			ItemSystem::SetItemForceAttributeEcs(pSummonItem, 4, 1, m_dwtduration);
#endif
		}
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetDuration %d %d", m_dwduration, m_dwtduration);
	}

}

void CNewPetActor::Unmount()
{
	if (nullptr == m_pkOwner)
		return;

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();
}

void CNewPetActor::Unsummon()
{
	if (true == this->IsSummoned())
	{
		const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());

		if (ItemSystem::IsValidItem(pSummonItem))
		{
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET level = %d, evolution=%d, exp=%d, expi=%d, bonus0=%d, bonus1=%d, bonus2=%d, skill0=%d, skill0lv= %d, skill1=%d, skill1lv= %d, skill2=%d, skill2lv= %d, skill3=%d, skill3lv= %d, duration=%d, tduration=%d WHERE id = %lu ", this->GetLevel(), this->m_dwevolution, this->GetExp(), this->GetExpI(), this->m_dwbonuspet[0][1], this->m_dwbonuspet[1][1], this->m_dwbonuspet[2][1], this->m_dwskillslot[0], this->m_dwskill[0], this->m_dwskillslot[1], this->m_dwskill[1], this->m_dwskillslot[2], this->m_dwskill[2], this->m_dwskillslot[3], this->m_dwskill[3], this->m_dwduration, this->m_dwtduration, ItemSystem::GetItemID(FindSummonItemByVID(this->GetSummonItemVID()))));
			this->ClearBuff();

			for (int b = 0; b < 3; b++)
			{
				ItemSystem::SetItemForceAttributeEcs(pSummonItem, b, 1, m_dwbonuspet[b][1]);
			}

#ifdef ENABLE_NEW_PET_EDITS
			ItemSystem::SetItemForceAttributeEcs(pSummonItem, 3, 1, m_dwlevel);
			ItemSystem::SetItemSocket(pSummonItem, 1, m_dwduration);
			ItemSystem::SetItemSocket(pSummonItem, 2, m_dwtduration);
#else
			ItemSystem::SetItemForceAttributeEcs(pSummonItem, 3, 1, m_dwduration);
			ItemSystem::SetItemForceAttributeEcs(pSummonItem, 4, 1, m_dwtduration);
			ItemSystem::SetItemSocket(pSummonItem, 1, m_dwlevel);
#endif
			ItemSystem::SetItemSocket(pSummonItem, 0, false);
			ItemSystem::UnlockItem(pSummonItem);
		}

		this->SetSummonItem(entt::null);

		if (nullptr != m_pkOwner)
			m_pkOwner->ComputePoints();

		if (nullptr != m_pkChar)
			M2_DESTROY_CHARACTER(m_pkChar);

		m_pkChar = nullptr;
		m_dwVID = 0;
		m_dwlevel = 1;
		m_dwlevelstep = 0;
		m_dwExpFromMob = 0;
		m_dwExpFromItem = 0;
		m_dwexp = 0;
		m_dwexpitem = 0;
		m_dwTimePet = 0;
		m_dwImmTime = 0;
		m_dwslotimm = 0;

		for (int s = 0; s < 9; ++s)
		{
			m_dwpetslotitem[s] = -1;
		}

		ClearBuff();
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetUnsummon");
	}
}

uint32_t CNewPetActor::Summon(const char* petName, entt::entity pSummonItemEntity, bool bSpawnFar)
{
	const entt::entity pSummonItem = pSummonItemEntity;
	if (!ItemSystem::IsValidItem(pSummonItem))
		return 0;
	int32_t x = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkOwner));
	int32_t y = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkOwner));
	int32_t z = m_pkOwner->GetZ();

	if (true == bSpawnFar)
	{
		x += (number(0, 1) * 2 - 1) * number(2000, 2500);
		y += (number(0, 1) * 2 - 1) * number(2000, 2500);
	}
	else
	{
		x += number(-100, 100);
		y += number(-100, 100);
	}

	if (nullptr != m_pkChar)
	{
		ecs::MovementSystem::Show(AIHelpers::EcsOf(m_pkChar), ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(m_pkOwner)), x, y);
		m_dwVID = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar));

		return m_dwVID;
	}
	int evolution = 0;
	int ivnum = ItemSystem::IsValidItem(pSummonItem) ? ItemSystem::GetItemVnum(pSummonItem) : 0;
	int evocation = 0;
	char szQuery2[1024];
	snprintf(szQuery2, sizeof(szQuery2), "SELECT evolution,evocation FROM new_petsystem WHERE id = %d ", ItemSystem::GetItemID(pSummonItem));
	std::unique_ptr<SQLMsg> pmsg3(DBManager::instance().DirectQuery(szQuery2));
	if (pmsg3->Get()->uiNumRows > 0) {
		MYSQL_ROW row1 = mysql_fetch_row(pmsg3->Get()->pSQLResult);
		evolution = atoi(row1[0]);
		evocation = atoi(row1[1]);
	}

	if(evocation == 0)
	{
		evocation = 1;
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET evocation = %d WHERE id = %lu ",evocation, ItemSystem::GetItemID(pSummonItem)));
	}
#ifdef ENABLE_NEW_PET_EDITS
	else {
		if (ItemSystem::IsValidItem(pSummonItem) && ItemSystem::GetItemSocket(pSummonItem, 1) <= 0) {
			return 0;
		}
	}
#endif

	m_dwVnum = 0;
	switch (ivnum) {
		case 55701:
			m_dwVnum = evolution == 3 ? 34042 : 34041;
			break;
		case 55702:
			m_dwVnum = evolution == 3 ? 34046 : 34045;
			break;
		case 55703:
			m_dwVnum = evolution == 3 ? 34050 : 34049;
			break;
		case 55704:
			m_dwVnum = evolution == 3 ? 34054 : 34053;
			break;
		case 55705:
			m_dwVnum = evolution == 3 ? 34037 : 34036;
			break;
		case 55706:
			m_dwVnum = evolution == 3 ? 34065 : 34064;
			break;
		case 55707:
			m_dwVnum = evolution == 3 ? 34074 : 34073;
			break;
		case 55708:
			m_dwVnum = evolution == 3 ? 34076 : 34075;
			break;
		case 55709:
			m_dwVnum = evolution == 3 ? 34081 : 34080;
			break;
		case 55710:
			m_dwVnum = evolution == 3 ? 34083 : 34082;
			break;
		case 55711:
			m_dwVnum = evolution == 3 ? 34096 : 34095;
			break;
		default:
			break;
	}

	if (m_dwVnum == 0) {
		LOG_ERROR("[CPetSystem::Summon] Invalid seal: {}.", ivnum);
		return 0;
	}

#ifdef ENABLE_COSTUME_PET
	uint32_t dwPetSkinvnum = m_pkOwner->GetPetSkinVnum();
	m_dwVnum = dwPetSkinvnum != 0 ? dwPetSkinvnum : m_dwVnum;
#endif

	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(
				m_dwVnum,
				ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(m_pkOwner)),
				x, y, z,
				false, (int)(m_pkOwner->GetRotation()+180), false);

	if (nullptr == m_pkChar)
	{
		LOG_ERROR("[CPetSystem::Summon] Failed to summon the pet. (vnum: {})", m_dwVnum);
		return 0;
	}

	m_pkChar->SetNewPet();

//	m_pkOwner->DetailLog();
//	m_pkChar->DetailLog();

	//펫의 국가를 주인의 국가로 설정함.
	m_pkChar->SetEmpire(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(m_pkOwner)));

	m_dwVID = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(m_pkChar));

	char szQuery1[1024];
	snprintf(szQuery1, sizeof(szQuery1), "SELECT name,level,exp,expi,bonus0,bonus1,bonus2,skill0,skill0lv,skill1,skill1lv,skill2,skill2lv,skill3,skill3lv,duration,tduration,evolution "
#ifdef ENABLE_NEW_PET_EDITS
	", minAge "
#endif
	"FROM new_petsystem WHERE id = %d ", ItemSystem::GetItemID(pSummonItem));
	std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
	if (pmsg2->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
		this->SetName(row[0]);
		this->SetLevel(atoi(row[1]));
		this->SetExp(atoi(row[2]), 0);
		this->SetExp(atoi(row[3]), 1);
		this->m_dwbonuspet[0][1] = atoi(row[4]);
		this->m_dwbonuspet[1][1] = atoi(row[5]);
		this->m_dwbonuspet[2][1] = atoi(row[6]);
		this->m_dwskillslot[0] = atoi(row[7]);
		this->m_dwskill[0] = atoi(row[8]);
		this->m_dwskillslot[1] = atoi(row[9]);
		this->m_dwskill[1] = atoi(row[10]);
		this->m_dwskillslot[2] = atoi(row[11]);
		this->m_dwskill[2] = atoi(row[12]);
		this->m_dwskillslot[3] = atoi(row[13]);
		this->m_dwskill[3] = atoi(row[14]);
		this->m_dwduration = atoi(row[15]);
		this->m_dwtduration = atoi(row[16]);
		this->m_dwevolution = atoi(row[17]);
#ifdef ENABLE_NEW_PET_EDITS
		this->dwMinAge = atoi(row[18]);
#endif
	}else
		this->SetName(petName);

//#ifdef ENABLE_NEW_PET_EDITS
	//if ((evocation == 0) && (this->m_dwduration < 1))
	//if (m_dwduration < 1) {
	//	return 0;
	//}
//#endif

	// SetSummonItem(pSummonItem)를 부른 후에 ComputePoints를 부르면 버프 적용됨.
	this->SetSummonItem(pSummonItemEntity);

	//this->SetNextExp(m_pkChar->PetGetNextExp());
	m_pkOwner->ComputePoints();
	ecs::MovementSystem::Show(AIHelpers::EcsOf(m_pkChar), ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(m_pkOwner)), x, y, z);

	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetIcon %d", m_dwSummonItemVnum);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetEvolution %d", m_dwevolution);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetName %s", m_name.c_str());
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetLevel %d", m_dwlevel);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetDuration %d %d", m_dwduration, m_dwtduration);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetBonus %d %d %d",m_dwbonuspet[0][1], m_dwbonuspet[1][1], m_dwbonuspet[2][1]);
#ifndef ENABLE_NEW_PET_EDITS
	if (GetLevel() >= 80 && m_dwevolution == 3 )
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 0, m_dwskillslot[0], m_dwskill[0]);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 1, m_dwskillslot[1], m_dwskill[1]);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 2, m_dwskillslot[2], m_dwskill[2]);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 3, m_dwskillslot[3], m_dwskill[3]);
	}
	else
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 0, -1, m_dwskill[0]);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 1, -1, m_dwskill[1]);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 2, -1, m_dwskill[2]);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 3, -1, m_dwskill[3]);
	}
#else
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetAge %d", dwMinAge);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 0, m_dwskillslot[0], m_dwskill[0]);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 1, m_dwskillslot[1], m_dwskill[1]);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 2, m_dwskillslot[2], m_dwskill[2]);
	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 3, m_dwskillslot[3], m_dwskill[3]);
#endif

	ecs::ChatSystem::Send(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, m_pkChar->PetGetNextExp());
	lMinAge = get_global_time() - dwMinAge;
	m_idx = 0;

	UpdateTime(true);

	for (int b = 0; b < 3; b++){
		ItemSystem::SetItemForceAttributeEcs(pSummonItem, b, 1, m_dwbonuspet[b][1]);
	}

#ifdef ENABLE_NEW_PET_EDITS
	ItemSystem::SetItemForceAttributeEcs(pSummonItem, 3, 1, m_dwlevel);
	ItemSystem::SetItemSocket(pSummonItem, 1, m_dwduration);
	ItemSystem::SetItemSocket(pSummonItem, 2, m_dwtduration);
#else
	ItemSystem::SetItemForceAttributeEcs(pSummonItem, 3, 1, m_dwduration);
	ItemSystem::SetItemForceAttributeEcs(pSummonItem, 4, 1, m_dwtduration);
	ItemSystem::SetItemSocket(pSummonItem, 1, m_dwlevel);
#endif
	ItemSystem::SetItemSocket(pSummonItem, 0, true);
	ItemSystem::LockItem(pSummonItem);
#ifdef ENABLE_RECALL
	const CAffect* pAffect = AffectSystem::FindAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_RECALL2);
	if (pAffect) {
		AffectSystem::RemoveAffect(AIHelpers::EcsOf(m_pkOwner), const_cast<CAffect*>(pAffect));
	}

	AffectSystem::AddAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_RECALL2, APPLY_NONE, 0, ItemSystem::GetItemID(pSummonItem), INFINITE_AFFECT_DURATION, 0, true, false);
#endif
	return m_dwVID;
}

bool CNewPetActor::_UpdatAloneActionAI(float fMinDist, float fMaxDist)
{
	float fDist = number(fMinDist, fMaxDist);
	float r = (float)number (0, 359);
	float dest_x = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(GetOwner())) + fDist * cos(r);
	float dest_y = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(GetOwner())) + fDist * sin(r);

	//m_pkChar->SetRotation(number(0, 359));        // 방향은 랜덤으로 설정

	//GetDeltaByDegree(m_pkChar->GetRotation(), fDist, &fx, &fy);

	// 느슨한 못감 속성 체크; 최종 위치와 중간 위치가 갈수없다면 가지 않는다.
	//if (!(SECTREE_MANAGER::instance().IsMovablePosition(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(m_pkChar)), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkChar)) + (int) fx, ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkChar)) + (int) fy)
	//			&& SECTREE_MANAGER::instance().IsMovablePosition(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(m_pkChar)), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkChar)) + (int) fx/2, ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkChar)) + (int) fy/2)))
	//	return true;

	m_pkChar->SetNowWalking(true);

	//if (ecs::MovementSystem::Goto(AIHelpers::EcsOf(m_pkChar), ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkChar)) + (int) fx, ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkChar)) + (int) fy))
	//	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	const entt::entity petEntity = AIHelpers::EcsOf(m_pkChar);
	const bool isMoving = petEntity != entt::null
		&& g_registry.valid(petEntity)
		&& g_registry.all_of<ecs::MovementDestination>(petEntity);
	if (!isMoving && ecs::MovementSystem::Goto(AIHelpers::EcsOf(m_pkChar), dest_x, dest_y))
	{
		if (petEntity != entt::null && g_registry.valid(petEntity))
				g_registry.emplace_or_replace<ecs::MovementDestination>(petEntity, static_cast<int32_t>(dest_x), static_cast<int32_t>(dest_y));
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	}

	m_dwLastActionTime = get_dword_time();

	return true;
}

// StateHorse함수 그냥 C&P -_-;
bool CNewPetActor::_UpdateFollowAI()
{
	if (nullptr == m_pkChar->GetMobData())
	{
		// LOG_ERROR("[CPetActor::_UpdateFollowAI] m_pkChar->m_pkMobData is NULL");
		return false;
	}

	// NOTE: 캐릭터(펫)의 원래 이동 속도를 알아야 하는데, 해당 값(m_pkChar->m_pkMobData->m_table.sMovingSpeed)을 직접적으로 접근해서 알아낼 수도 있지만
	// m_pkChar->m_pkMobData 값이 invalid한 경우가 자주 발생함. 현재 시간관계상 원인은 다음에 파악하고 일단은 m_pkChar->m_pkMobData 값을 아예 사용하지 않도록 함.
	// 여기서 매번 검사하는 이유는 최초 초기화 할 때 정상 값을 제대로 못얻어오는 경우도 있음.. -_-;; ㅠㅠㅠㅠㅠㅠㅠㅠㅠ
	if (0 == m_originalMoveSpeed)
	{
		const CMob* mobData = CMobManager::Instance().Get(m_dwVnum);

		if (nullptr != mobData)
			m_originalMoveSpeed = mobData->m_table.sMovingSpeed;
	}
	float	START_FOLLOW_DISTANCE = 300.0f;		// 이 거리 이상 떨어지면 쫓아가기 시작함
	float	START_RUN_DISTANCE = 900.0f;		// 이 거리 이상 떨어지면 뛰어서 쫓아감.

	float	RESPAWN_DISTANCE = 4500.f;			// 이 거리 이상 멀어지면 주인 옆으로 소환함.
	int		APPROACH = 290;						// 접근 거리
	bool bRun = false;							// 뛰어야 하나?

	uint32_t currentTime = get_dword_time();

	int32_t ownerX = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkOwner));		int32_t ownerY = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkOwner));
	int32_t charX = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkChar));			int32_t charY = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkChar));

	float fDist = DISTANCE_APPROX(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		float fOwnerRot = m_pkOwner->GetRotation() * 3.141592f / 180.f;
		float fx = -APPROACH * cos(fOwnerRot);
		float fy = -APPROACH * sin(fOwnerRot);
		if (ecs::MovementSystem::Show(AIHelpers::EcsOf(m_pkChar), ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(m_pkOwner)), ownerX + fx, ownerY + fy))
		{
			return true;
		}
	}


	if (fDist >= START_FOLLOW_DISTANCE)
	{
		if( fDist >= START_RUN_DISTANCE)
		{
			bRun = true;
		}

		m_pkChar->SetNowWalking(!bRun);		// NOTE: 함수 이름보고 멈추는건줄 알았는데 SetNowWalking(false) 하면 뛰는거임.. -_-;

		Follow(APPROACH);

		m_pkChar->SetLastAttacked(currentTime);
		m_dwLastActionTime = currentTime;
	}
	//else
	//{
	//	if (fabs(m_pkChar->GetRotation() - GetDegreeFromPositionXY(charX, charY, ownerX, ownerX)) > 10.f || fabs(m_pkChar->GetRotation() - GetDegreeFromPositionXY(charX, charY, ownerX, ownerX)) < 350.f)
	//	{
	//		m_pkChar->Follow(m_pkOwner, APPROACH);
	//		m_pkChar->SetLastAttacked(currentTime);
	//		m_dwLastActionTime = currentTime;
	//	}
	//}
	// Follow 중이지만 주인과 일정 거리 이내로 가까워졌다면 멈춤
	else
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	//else if (currentTime - m_dwLastActionTime > number(5000, 12000))
	//{
	//	this->_UpdatAloneActionAI(START_FOLLOW_DISTANCE / 2, START_FOLLOW_DISTANCE);
	//}

	return true;
}

bool CNewPetActor::Update(uint32_t deltaTime)
{
	bool bResult = true;

	// 펫 주인이 죽었거나, 소환된 펫의 상태가 이상하다면 펫을 없앰. (NOTE: 가끔가다 이런 저런 이유로 소환된 펫이 DEAD 상태에 빠지는 경우가 있음-_-;)
	// 펫을 소환한 아이템이 없거나, 내가 가진 상태가 아니라면 펫을 없앰.
#ifndef ENABLE_NEW_PET_EDITS
	if (IsSummoned()) {
		if (m_pkOwner->IsImmortal() && Pet_Skill_Table[16][2 + m_dwskill[m_dwslotimm]] <= (get_global_time() - m_dwImmTime)*10) {
			m_dwImmTime = 0;
			m_pkOwner->SetImmortal(0);
		}
	}
#endif

#ifdef DISABLE_TRADE_UNSUMMON
	//if (CombatSystem::IsDead(AIHelpers::EcsOf(m_pkOwner)) || (IsSummoned() && CombatSystem::IsDead(AIHelpers::EcsOf(m_pkChar))) || (IsSummoned() && m_dwduration <= 0)
	if ((IsSummoned() && CombatSystem::IsDead(AIHelpers::EcsOf(m_pkChar))) || (IsSummoned() && m_dwduration <= 0)
		|| !IsSummonItemOwnedBy(this->GetSummonItemVID(), this->GetOwner())
		)
#else
	//if (CombatSystem::IsDead(AIHelpers::EcsOf(m_pkOwner)) || (IsSummoned() && CombatSystem::IsDead(AIHelpers::EcsOf(m_pkChar))) || (IsSummoned() && (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(m_pkOwner)) || m_pkOwner->GetMyShop() || m_pkOwner->GetShopOwner() || m_pkOwner->IsOpenSafebox() || m_pkOwner->IsCubeOpen() || m_dwduration <= 0))
	//if ((IsSummoned() && CombatSystem::IsDead(AIHelpers::EcsOf(m_pkChar))) || (IsSummoned() && (ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(m_pkOwner)) || m_pkOwner->GetMyShop() || m_pkOwner->GetShopOwner() || m_pkOwner->IsOpenSafebox() || m_pkOwner->IsCubeOpen() || m_dwduration <= 0))
	if ((IsSummoned() && CombatSystem::IsDead(AIHelpers::EcsOf(m_pkChar))) || (IsSummoned() && m_dwduration <= 0)
		|| !IsSummonItemOwnedBy(this->GetSummonItemVID(), this->GetOwner())
		)
#endif
	{
		this->Unsummon();
		return true;
	}

	if (this->IsSummoned() && HasOption(EPetOption_Followable))
		bResult = bResult && this->_UpdateFollowAI();

	return bResult;
}

//NOTE : 주의!!! MinDistance를 크게 잡으면 그 변위만큼의 변화동안은 follow하지 않는다,
bool CNewPetActor::Follow(float fMinDistance)
{
	// 가려는 위치를 바라봐야 한다.
	if( !m_pkOwner || !m_pkChar)
		return false;

	float fOwnerX = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkOwner));
	float fOwnerY = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkOwner));

	float fPetX = ecs::PlayerRuntime::GetX(AIHelpers::EcsOf(m_pkChar));
	float fPetY = ecs::PlayerRuntime::GetY(AIHelpers::EcsOf(m_pkChar));

	float fDist = DISTANCE_SQRT(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= fMinDistance)
		return false;

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);

	if (!ecs::MovementSystem::Goto(AIHelpers::EcsOf(m_pkChar), (int)(fPetX+fx+0.5f), (int)(fPetY+fy+0.5f)) )
		return false;

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void CNewPetActor::SetSummonItem (entt::entity pItemEntity)
{

	if (!ItemSystem::IsValidItem(pItemEntity))
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = ItemSystem::GetItemVID(pItemEntity);
	m_dwSummonItemID = ItemSystem::GetItemID(pItemEntity);
	m_dwSummonItemVnum = ItemSystem::GetItemVnum(pItemEntity);

	const entt::entity owner = AIHelpers::EcsOf(m_pkOwner);
	if (owner != entt::null && g_registry.valid(owner)) {
		auto& pet = g_registry.emplace_or_replace<ecs::PetComponent>(owner);
		pet.owner = owner;
		pet.itemID = ItemSystem::GetItemID(pItemEntity);
		pet.itemVID = ItemSystem::GetItemVID(pItemEntity);
		pet.itemVnum = ItemSystem::GetItemVnum(pItemEntity);
		pet.level = m_dwlevel;
		pet.state = IsSummoned() ? 1u : 0u;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			pet.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket(pItemEntity, i));
	}
}

void CNewPetActor::GiveBuff()
{
#ifdef ENABLE_NEW_PET_EDITS
	int idx = 1;
	if ((lMinAge >= 950400) && (lMinAge < 2246400)) {
		idx = 2;
	} else if ((lMinAge >= 2246400) && (lMinAge < 4147200)) {
		idx = 3;
	} else if (lMinAge >= 4147200) {
		idx = 4;
	}

	int val[3][5] = {{POINT_MAX_HP, 500, 1200, 2100, 3000}, {POINT_RESIST_MONSTER, 2, 4, 7, 10}, {POINT_RESIST_MEZZIUOMINI, 2, 4, 7, 10}};

	for (int i = 0; i < 3; ++i) {
		AffectSystem::AddAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_NEW_PET, val[i][0], val[i][idx], 0, 60 * 60 * 24 * 365, 0, false);
		if (m_dwbonuspet[i][1] > 0) {
			AffectSystem::AddAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_NEW_PET, aApplyInfo[m_dwbonuspet[i][0]].bPointType, float(m_dwbonuspet[i][1]/10), 0, 60 * 60 * 24 * 365, 0, false);
		}
	}

	for (int i = 0; i < 4; i++) {
		idx = m_dwskillslot[i];
		if (idx != -1 && idx != 0) {
			AffectSystem::AddAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_NEW_PET, aApplyInfo[Pet_Skill_Table[m_dwskillslot[i]-1][1]].bPointType, Pet_Skill_Table[m_dwskillslot[i]-1][1+m_dwskill[i]], 0, 60 * 60 * 24 * 365, 0, false);
		}
	}
#else
	//Inizializzo i bonus del NewPetSystem //hp sp e def
	// 559 Affect NewPet
	int cbonus[3] = { ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(m_pkOwner)),  ecs::PointSystem::Get(AIHelpers::EcsOf(m_pkOwner), POINT_DEF_GRADE), ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(m_pkOwner)) };
	for (int i = 0; i < 3; ++i) {
		AffectSystem::AddAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_NEW_PET, aApplyInfo[m_dwbonuspet[i][0]].bPointType, float((cbonus[i]*m_dwbonuspet[i][1]/10)/1000), 0,  60 * 60 * 24 * 365, 0, false);
	}

	//Inizializzo le skill del pet inattive  No 10-17-18 No 0 no -1
	//Condizione lv > 81 evo 3 Solo Skill Passive
	if (GetLevel() >= 80 && m_dwevolution == 3)
	{
		for (int s = 0; s < 3; s++)
		{
			switch (m_dwskillslot[s])
			{

			/*
				Pet_Skill_Table[m_dwskillslot[s] - 1][0]; //Mi ritorna il type della skill
				Pet_Skill_Table[m_dwskillslot[s] - 1][1]; // Mi ritorna attiva/passiva della skill
				Pet_Skill_Table[m_dwskillslot[s] - 1][2]; // Mi ritorna il cd della skill
				Pet_Skill_Table[m_dwskillslot[s] - 1][2 + m_dwskill[s]]; //Mi ritorna l'apply della skill
			*/
			case 1: //Resistenza Guerriero 78 Punti
			case 2: //Resistenza Sura 80
			case 3: //Resistenza Ninja 79
			case 4: //Resistenza Shamani 81
			case 6: //Valore Attacco 53 Punti
			case 9: //critici 25 Punti
			case 10: // exp 43
			case 13: // Blocco corp 27 Punti
			case 14: // Riflessione 28 Punti
			case 15: // Drop Yang 44 Punti
			case 16: //forte contro mostri 63
			case 17: //forte contro metin 116
			case 18: //forte contro boss 117
				AffectSystem::AddAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_NEW_PET, aApplyInfo[Pet_Skill_Table[m_dwskillslot[s] - 1][0]].bPointType, float(Pet_Skill_Table[m_dwskillslot[s] - 1][2 + m_dwskill[s]]/10), 0, 60 * 60 * 24 * 365, 0, false);
				break;
			default:
				return;
			}
		}
	}
#endif
}

void CNewPetActor::ClearBuff()
{
	AffectSystem::RemoveAffect(AIHelpers::EcsOf(m_pkOwner), AFFECT_NEW_PET);
}

void CNewPetActor::DoPetSkill(int skillslot) {
#ifdef ENABLE_NEW_PET_EDITS
	return;
#else
	if (GetLevel() < 80 || m_dwevolution < 3)
		return;
	switch (m_dwskillslot[skillslot])
	{
	case 10:
	{
		if (get_global_time() - m_pkOwner->GetNewPetSkillCD(0) <= 480) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 749, "%d", (480 - (get_global_time() - m_pkOwner->GetNewPetSkillCD(0))));
#endif
			return;
		}
		if (m_pkOwner->GetHPPct() > 20) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 750, "");
#endif
			return;
		}
		m_pkOwner->SetNewPetSkillCD(0, get_global_time());
		int riphp = MIN(m_pkOwner->GetHP() + (int)Pet_Skill_Table[9][2 + m_dwskill[skillslot]], ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(m_pkOwner)));
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 751, "");
#endif
		ecs::PointSystem::Change(AIHelpers::EcsOf(m_pkOwner), POINT_HP, riphp);
		NetworkSyncSystem::BroadcastEffect(g_registry, AIHelpers::EcsOf(m_pkOwner), SE_HPUP_RED);
	}
	break;

	case 17:
	{
		if (get_global_time() - m_pkOwner->GetNewPetSkillCD(1) <= 600) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 749, "%d", (600 - (get_global_time() - m_pkOwner->GetNewPetSkillCD(1))));
#endif
			return;
		}
		m_pkOwner->SetNewPetSkillCD(1, get_global_time());
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 752, "");
#endif
		m_pkOwner->SetImmortal(1);
		m_dwslotimm = skillslot;
		m_dwImmTime = get_global_time();
	}
	break;
	case 18:
	{
		if (get_global_time() - m_pkOwner->GetNewPetSkillCD(2) <= 480) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 749, "%d", (480 - (get_global_time() - m_pkOwner->GetNewPetSkillCD(2))));
#endif
			return;
		}
		m_pkOwner->SetNewPetSkillCD(2, get_global_time());
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(m_pkOwner), CHAT_TYPE_INFO, 753, "");
#endif
		m_pkOwner->RemoveBadAffect();
	}
	break;

	default:
		return;
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////////////
//  CPetSystem
///////////////////////////////////////////////////////////////////////////////////////

CNewPetSystem::CNewPetSystem(LPCHARACTER owner)
{
//	assert(0 != owner && "[CPetSystem::CPetSystem] Invalid owner");

	m_pkOwner = owner;
	m_dwUpdatePeriod = 400;

	m_dwLastUpdateTime = 0;
}

CNewPetSystem::~CNewPetSystem()
{
	Destroy();
}

#ifdef ENABLE_NEW_PET_EDITS
bool CNewPetSystem::IncreasePetSkill(int iSlot, int iType)
#else
bool CNewPetSystem::IncreasePetSkill(int skill)
#endif
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
#ifdef ENABLE_NEW_PET_EDITS
				return petActor->IncreasePetSkill(iSlot, iType);
#else
				return petActor->IncreasePetSkill(skill);
#endif
			}
		}
	}
	return false;
}

#ifdef ENABLE_NEW_PET_EDITS
bool CNewPetSystem::IncreasePetSkillByBook(entt::entity bookItem)
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->IncreasePetSkillByBook(bookItem);
			}
		}
	}
	return false;
}

int CNewPetSystem::ResetSkills()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->ResetSkills();
			}
		}
	}

	return 2;
}

int CNewPetSystem::ResetSkill(int iType)
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->ResetSkill(iType);
			}
		}
	}

	return 2;
}
#endif

bool  CNewPetSystem::IncreasePetEvolution() {
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->IncreasePetEvolution();
			}
		}
	}
	return false;
}


void CNewPetSystem::Destroy()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			delete petActor;
		}
	}
	event_cancel(&m_pkNewPetSystemUpdateEvent);
	event_cancel(&m_pkNewPetSystemExpireEvent);
	m_petActorMap.clear();
}


void CNewPetSystem::UpdateTime()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;

		if (nullptr != petActor && petActor->IsSummoned())
		{
			petActor->UpdateTime();
		}
	}
}
/// 펫 시스템 업데이트. 등록된 펫들의 AI 처리 등을 함.
bool CNewPetSystem::Update(uint32_t deltaTime)
{
	bool bResult = true;

	uint32_t currentTime = get_dword_time();

	// CHARACTER_MANAGER에서 캐릭터류 Update할 때 매개변수로 주는 (Pulse라고 되어있는)값이 이전 프레임과의 시간차이인줄 알았는데
	// 전혀 다른 값이라서-_-; 여기에 입력으로 들어오는 deltaTime은 의미가 없음ㅠㅠ

	if (m_dwUpdatePeriod > currentTime - m_dwLastUpdateTime)
		return true;

	std::vector <CNewPetActor*> v_garbageActor;

	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;

		if (nullptr != petActor && petActor->IsSummoned())
		{
			LPCHARACTER pPet = petActor->GetCharacter();

			const entt::entity petEntity = AIHelpers::EcsOf(pPet);
			if (petEntity == entt::null || !g_registry.valid(petEntity))
			{
				v_garbageActor.push_back(petActor);
			}
			else
			{
				bResult = bResult && petActor->Update(deltaTime);
			}
		}
	}
	for (std::vector<CNewPetActor*>::iterator it = v_garbageActor.begin(); it != v_garbageActor.end(); it++)
		DeletePet(*it);

	m_dwLastUpdateTime = currentTime;

	return bResult;
}

/// 관리 목록에서 펫을 지움
void CNewPetSystem::DeletePet(uint32_t mobVnum)
{
	TNewPetActorMap::iterator iter = m_petActorMap.find(mobVnum);

	if (m_petActorMap.end() == iter)
	{
		LOG_ERROR("[CPetSystem::DeletePet] Can't find pet on my list (VNUM: {})", mobVnum);
		return;
	}

	CNewPetActor* petActor = iter->second;

	if (nullptr == petActor)
		LOG_ERROR("[CPetSystem::DeletePet] Null Pointer (petActor)");
	else
		delete petActor;

	m_petActorMap.erase(iter);
}

/// 관리 목록에서 펫을 지움
void CNewPetSystem::DeletePet(CNewPetActor* petActor)
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		if (iter->second == petActor)
		{
			delete petActor;
			m_petActorMap.erase(iter);

			return;
		}
	}

	LOG_ERROR("[CPetSystem::DeletePet] Can't find petActor({}) on my list(size: {}) ", static_cast<const void*>(petActor), m_petActorMap.size());
}

void CNewPetSystem::Unsummon(uint32_t vnum, bool bDeleteFromList)
{
	CNewPetActor* actor = this->GetByVnum(vnum);

	if (nullptr == actor)
	{
		LOG_ERROR("[CPetSystem::GetByVnum({})] Null Pointer (petActor)", vnum);
		return;
	}
	actor->Unsummon();

	if (true == bDeleteFromList)
		this->DeletePet(actor);

	bool bActive = false;
	for (TNewPetActorMap::iterator it = m_petActorMap.begin(); it != m_petActorMap.end(); it++)
	{
		bActive |= it->second->IsSummoned();
	}
	if (false == bActive)
	{
		event_cancel(&m_pkNewPetSystemUpdateEvent);
		event_cancel(&m_pkNewPetSystemExpireEvent);
		m_pkNewPetSystemUpdateEvent = nullptr;
		m_pkNewPetSystemExpireEvent = nullptr;
	}
}

void CNewPetSystem::UnsummonAll(LPCHARACTER ch)
{
	if (!ch)
		return;

#ifdef ENABLE_RECALL
	const CAffect* pAffect = AffectSystem::FindAffect(AIHelpers::EcsOf(ch), AFFECT_RECALL2);
	if (pAffect) {
		AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), const_cast<CAffect*>(pAffect));
	}
#endif

	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				petActor->Unsummon();
				break;
			}
		}
	}

	if (m_pkNewPetSystemUpdateEvent) {
		event_cancel(&m_pkNewPetSystemUpdateEvent);
		m_pkNewPetSystemUpdateEvent = nullptr;
	}

	if (m_pkNewPetSystemExpireEvent) {
		event_cancel(&m_pkNewPetSystemExpireEvent);
		m_pkNewPetSystemExpireEvent = nullptr;
	}
}

uint32_t CNewPetSystem::GetNewPetITemID()
{
	uint32_t itemid = 0;
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				itemid = petActor->GetSummonItemID();
				break;
			}
		}
	}
	return itemid;

}

bool CNewPetSystem::IsActivePet()
{
	bool state = false;
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				state = true;
				break;
			}
		}
	}
	return state;

}

int CNewPetSystem::GetLevelStep()
{
	int step = 4;
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				step = petActor->GetLevelStep();
				break;
			}
		}
	}
	return step;
}

void CNewPetSystem::SetExp(int iExp, int mode)
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				petActor->SetExp(iExp, mode);
				break;
			}
		}
	}
}

int CNewPetSystem::GetEvolution()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->GetEvolution();
			}
		}
	}
	return -1;
}

int CNewPetSystem::GetLevel()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->GetLevel();
			}
		}
	}
	return -1;
}

int CNewPetSystem::GetExp()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->GetExp();
			}
		}
	}
	return 0;
}

#ifdef ENABLE_NEW_PET_EDITS
int CNewPetSystem::GetNextExpFromMob()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->GetNextExpFromMob();
			}
		}
	}
	return 0;
}
#endif

void CNewPetSystem::SetItemCube(int pos, int invpos) {
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->SetItemCube(pos, invpos);
			}
		}
	}
}

void CNewPetSystem::ItemCubeFeed(int type) {
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->ItemCubeFeed(type);
			}
		}
	}
}

void CNewPetSystem::DoPetSkill(int skillslot) {
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->DoPetSkill(skillslot);
			}
		}
	}
}



CNewPetActor* CNewPetSystem::Summon(uint32_t mobVnum, entt::entity pSummonItem, const char* petName, bool bSpawnFar, uint32_t options)
{
	CNewPetActor* petActor = this->GetByVnum(mobVnum);

	// 등록된 펫이 아니라면 새로 생성 후 관리 목록에 등록함.
	if (nullptr == petActor)
	{
		petActor = M2_NEW CNewPetActor(m_pkOwner, mobVnum, options);
		m_petActorMap.insert(std::make_pair(mobVnum, petActor));
	}

	uint32_t petVID = petActor->Summon(petName, pSummonItem, bSpawnFar);
	LOG_INFO("Summon: {}", petVID);

	if (nullptr == m_pkNewPetSystemUpdateEvent)
	{
		newpetsystem_event_info* info = AllocEventInfo<newpetsystem_event_info>();

		info->pPetSystem = this;

		m_pkNewPetSystemUpdateEvent = event_create(newpetsystem_update_event, info, PASSES_PER_SEC(1) / 4);	// 0.25초
	}

	if (nullptr == m_pkNewPetSystemExpireEvent)
	{
		newpetsystem_event_infoe* infoe = AllocEventInfo<newpetsystem_event_infoe>();

		infoe->pPetSystem = this;

		m_pkNewPetSystemExpireEvent = event_create(newpetsystem_expire_event, infoe, PASSES_PER_SEC(1) );	// 1 volata per sec
	}

	return petActor;
}


CNewPetActor* CNewPetSystem::GetByVID(uint32_t vid) const
{
	CNewPetActor* petActor = nullptr;

	bool bFound = false;

	for (TNewPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		petActor = iter->second;

		if (nullptr == petActor)
		{
			LOG_ERROR("[CPetSystem::GetByVID({})] Null Pointer (petActor)", vid);
			continue;
		}

		bFound = petActor->GetVID() == vid;

		if (true == bFound)
			break;
	}

	return bFound ? petActor : nullptr;
}

/// 등록 된 펫 중에서 주어진 몹 VNUM을 가진 액터를 반환하는 함수.
CNewPetActor* CNewPetSystem::GetByVnum(uint32_t vnum) const
{
	CNewPetActor* petActor = nullptr;

	TNewPetActorMap::const_iterator iter = m_petActorMap.find(vnum);

	if (m_petActorMap.end() != iter)
		petActor = iter->second;

	return petActor;
}

size_t CNewPetSystem::CountSummoned() const
{
	size_t count = 0;

	for (TNewPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->IsSummoned())
				++count;
		}
	}

	return count;
}

#ifdef ENABLE_COSTUME_PET
void CNewPetActor::UpdatePetSkin() {
	const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());
	if (ItemSystem::IsValidItem(pSummonItem)){
		Unsummon();
		Summon("Noname", pSummonItem, false);
	}
}

void CNewPetSystem::UpdatePetSkin() {
	for (TNewPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter) {
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned())
				petActor->UpdatePetSkin();
		}
	}
}
#endif

void CNewPetSystem::RefreshBuff()
{
	for (TNewPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->IsSummoned())
			{
				petActor->ClearBuff();
				petActor->GiveBuff();
			}
		}
	}
}

void CNewPetActor::ChangeName(const char * name) {
	char query2[256] = {0};
	snprintf(query2, sizeof(query2), "UPDATE player.new_petsystem SET name='%s' WHERE name='%s' LIMIT 1;", name, m_name.c_str());
	std::unique_ptr<SQLMsg> pRes(DBManager::instance().DirectQuery(query2));

	const entt::entity pSummonItem = FindSummonItemByVID(this->GetSummonItemVID());
	if (ItemSystem::IsValidItem(pSummonItem)){
		Unsummon();
		Summon("Noname", pSummonItem, false);
	}
}

void CNewPetSystem::ChangeName(const char * name)
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter) {
		CNewPetActor* petActor = iter->second;
		if (petActor != nullptr) {
			if (petActor->IsSummoned()) {
				return petActor->ChangeName(name);
			}
		}
	}
}


