

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
#include "config.h"
#include "vector.h"
#include "char_manager.h"
#include "New_PetSystem.h"
#include <common/VnumHelper.h>
#include "packet.h"
#include "db.h"
#include "ecs/Registry.hpp"
#include "ecs/components/movement_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/components/pet_mount_components.hpp"
#include "ecs/components/inventory_components.hpp"
#include "ecs/components/status_components.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/systems/ViewSystem.hpp"
#include <charconv>
#include <limits>
#include <utility>

//#define DISABLE_TRADE_UNSUMMON // this disable the unsummon of pet when a excange/trade/shop/myshop/safebox windows is open, MAKE SURE to have set the items with vnum 55401/55402/55403/55404 with antiflag ANTI_SAFEBOX | ANTI_PKDROP | ANTI_DROP | ANTI_SELL | ANTI_GIVE | ANTI_STACK | ANTI_MYSHOP
//USE AT OWN YOUR RISK

namespace
{
bool OwnedItem(entt::entity owner, entt::entity item)
{
    return ecs::PlayerRuntime::IsValid(owner) && ItemSystem::IsValidItem(item)
        && ItemSystem::GetItemOwner(item) == owner;
}

bool CanUseMaterial(entt::entity owner, entt::entity item)
{
    return OwnedItem(owner, item) && !ItemSystem::IsItemLocked(item)
        && !ItemSystem::IsItemEquipped(item) && ItemSystem::GetItemCount(item) != 0;
}

bool IsTradeWindowOpen(entt::entity owner)
{
    if (!ecs::PlayerRuntime::IsValid(owner))
        return true;
    const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(owner);
    const auto* cube = g_registry.try_get<ecs::CubeWindowComponent>(owner);
    return ecs::SocialSystem::GetExchange(owner) || ecs::SocialSystem::GetMyShop(owner)
        || ecs::SocialSystem::GetShopOwner(owner) != entt::null
        || (safebox && safebox->isOpening) || (cube && cube->pNpc);
}

uint32_t PetSkin(entt::entity owner)
{
#ifdef ENABLE_COSTUME_PET
    const auto skin = ItemSystem::GetWearItem(owner, WEAR_COSTUME_PET_SKIN);
    if (ItemSystem::IsValidItem(skin))
        return ItemSystem::GetItemValue(skin, 0);
#endif
    return 0;
}

void SendPetLevelUpEffect(entt::entity pet, int type, int value, int amount)
{
    if (!ecs::PlayerRuntime::IsValid(pet))
        return;
    packet_point_change packet {};
    packet.header = HEADER_GC_CHARACTER_POINT_CHANGE;
    packet.dwVID = ecs::PlayerRuntime::GetPacketVID(pet);
    packet.type = type;
    packet.value = value;
    packet.amount = amount;
    ecs::ViewSystem::PacketView(pet, &packet, sizeof(packet));
}

bool SnapFollowerToOwner(entt::entity pet, entt::entity owner, int32_t x, int32_t y, int32_t z)
{
    if (!ecs::PlayerRuntime::IsValid(pet) || !ecs::PlayerRuntime::IsValid(owner))
        return false;
    if (!ecs::MovementSystem::Show(pet, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z))
        return false;
    ecs::MovementSystem::Stop(pet);
    ecs::MovementSystem::SendMovePacket(pet, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool ReadPetNumber(const char* text, int64_t minimum, int64_t maximum, int64_t& value)
{
    if (!text)
        return false;
    const auto* end = text + strlen(text);
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end && value >= minimum && value <= maximum;
}
}

EVENTINFO(newpetsystem_event_info)
{
    entt::entity owner { entt::null };
};

EVENTFUNC(newpetsystem_update_event)
{
    const auto* info = dynamic_cast<newpetsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    auto* system = ecs::PlayerRuntime::GetNewPetSystem(info->owner);
    if (!system || !system->IsUpdateEvent(event))
        return 0;
    system->Update(0);
    return PASSES_PER_SEC(1) / 4;
}

EVENTFUNC(newpetsystem_expire_event)
{
    const auto* info = dynamic_cast<newpetsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    auto* system = ecs::PlayerRuntime::GetNewPetSystem(info->owner);
    if (!system || !system->IsExpireEvent(event))
        return 0;
    system->UpdateTime();
    return PASSES_PER_SEC(1);
}

CNewPetActor::CNewPetActor(entt::entity owner, uint32_t vnum, uint32_t options)
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

	m_character = entt::null;
	m_owner = owner;

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

	m_feedItems.fill({});


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

	m_owner = entt::null;
}

void CNewPetActor::SetName(const char* name)
{
    m_name = name ? name : "";
    if (IsSummoned())
        g_registry.emplace_or_replace<ecs::PlayerName>(m_character, m_name);
}

bool CNewPetActor::IsSummoned() const
{
    return ecs::PlayerRuntime::IsValid(m_character);
}

bool CNewPetActor::HasValidSummon() const
{
    return IsSummoned() && OwnedItem(m_owner, m_summonItem);
}

uint32_t CNewPetActor::GetNextExp() const
{
    return exppet_table && m_dwlevel <= 120 ? exppet_table[m_dwlevel] : 2500000000u;
}

void CNewPetActor::SetItemCube(int pos, int invpos)
{
    // Client slot bounds must be checked BEFORE touching the nine-slot array.
    if (pos < 0 || pos >= static_cast<int>(m_feedItems.size()) || invpos < 0
        || invpos >= INVENTORY_MAX_NUM || !HasValidSummon() || IsTradeWindowOpen(m_owner))
        return;
    const auto item = ItemSystem::GetInventoryItem(m_owner, invpos);
    if (item == m_summonItem || !CanUseMaterial(m_owner, item))
        return;
    for (const auto& selection : m_feedItems)
        if (selection.item == item)
            return;
    if (m_feedItems[pos].item == entt::null)
        m_feedItems[pos] = { item, invpos };
}

void CNewPetActor::ItemCubeFeed(int type)
{
    // Detach selections first, including rejected feeds, so stale selections
    // cannot later consume a replacement item in the same inventory cell.
    const auto selections = std::exchange(m_feedItems, {});
    if (!HasValidSummon() || IsTradeWindowOpen(m_owner) || (type != 1 && type != 3))
        return;
    for (const auto& selection : selections)
    {
        const auto item = selection.item;
        if (!HasValidSummon())
            return;
        if (!CanUseMaterial(m_owner, item) || item == m_summonItem
            || selection.cell < 0 || ItemSystem::GetInventoryItem(m_owner, selection.cell) != item)
            continue;
        if (type == 1)
        {
            const auto vnum = ItemSystem::GetItemVnum(item);
            if (!((vnum >= 55401 && vnum <= 55411) || (vnum >= 55701 && vnum <= 55711) || vnum == 55001))
                continue;
            const uint64_t added = vnum == 55001 ? m_dwtduration / 2 : static_cast<uint64_t>(m_dwtduration) * 3 / 100;
            if (!ItemSystem::DestroyItemEntityEcs(item, "PET_CUBE_FEED"))
                continue;
            m_dwduration = static_cast<uint32_t>(std::min<uint64_t>(m_dwtduration, m_dwduration + added));
            ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetDuration %u %u", m_dwduration, m_dwtduration);
        }
        else if (GetLevel() < 120 && (ItemSystem::GetItemType(item) == ITEM_WEAPON || ItemSystem::GetItemType(item) == ITEM_ARMOR))
        {
            const auto exp = ItemSystem::GetItemShopBuyPrice(item) / 2;
            if (exp <= 0 || exp > UINT32_MAX)
                continue;
            if (ItemSystem::DestroyItemEntityEcs(item, "PET_CUBE_FEED"))
                SetExp(exp, 1);
        }
    }
}

#ifdef ENABLE_NEW_PET_EDITS
bool CNewPetActor::IncreasePetSkill(int iSlot, int iType)
#else
bool CNewPetActor::IncreasePetSkill(int skill)
#endif
{
    if (!HasValidSummon() || IsTradeWindowOpen(m_owner))
        return false;
#ifdef ENABLE_NEW_PET_EDITS
    if (iSlot < 0 || iSlot >= 4 || iType < 0 || iType > UINT16_MAX)
        return false;
	int idx = m_dwskillslot[iSlot];
	if (idx == -1)
		return false;

	if (m_dwskill[iSlot] >= 10) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 58, "");
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
	const entt::entity bookItem = ItemSystem::GetItem(m_owner, Cell);
	if (!CanUseMaterial(m_owner, bookItem))
		return false;

	iType = ItemSystem::GetItemValue(bookItem, 0);
	if ((iType > 12) || (iType < 1) || ((idx != 0) && (idx != iType)) || (ItemSystem::GetItemType(bookItem) != ITEM_TYPE_PET))
		return false;

	for (int i = 0; i < 4; i++) {
		if ((iType == m_dwskillslot[i]) && (iSlot != i)) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 55, "");
#endif
			return false;
		}
	}

	if (!ItemSystem::ConsumeItemEcs(bookItem, 1))
        return false;

	if (idx == 0)
		m_dwskillslot[iSlot] = iType;

	m_dwskill[iSlot] += 1;

#ifdef TEXTS_IMPROVEMENT
	if (m_dwskill[iSlot] == 1) {
		ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 57, "");
	}
	else {
		ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 56, "%d", m_dwskill[iSlot]);
	}
#endif
	ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", iSlot, m_dwskillslot[iSlot], m_dwskill[iSlot]);

	ClearBuff();
	GiveBuff();

	return true;
#else
	if (skill < 1 || skill > std::size(Pet_Skill_Table) || (GetLevel() < 80 && m_dwevolution < 3))
		return false;
	for (int i = 0; i < 4; ++i)
	{ //Itero gli slot per cercare la skill
		if (m_dwskillslot[i] == skill)
		{  //Se trova la skill o la aumenta oppure e' gi?max
			const entt::entity owner = m_owner;
			if (m_dwskill[i] < 20)
			{
				m_dwskill[i] += 1;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 743, "%d", m_dwskill[i]);
#endif
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, m_dwskillslot[i], m_dwskill[i]);
				return true;
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 744, "");
#endif
				return false;
			}
		}
	}

	for (int i = 0; i < 4; ++i)
	{
		if (m_dwskillslot[i] == 0)
		{ //Controllo se trovo uno slot vuoto abilitato
			const entt::entity owner = m_owner;
			m_dwskillslot[i] = skill;
			m_dwskill[i] = 1;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 745, "");
#endif
			ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, m_dwskillslot[i], m_dwskill[i]);
			return true;
		}
	}

	/* Qualora il pet non soddisfi le condizioni precedenti
	   Allora tutti gli slot sono pieni e quind non pu?
	   imparare nuove skill
	*/
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(m_owner, CHAT_TYPE_INFO, 745, "");
#endif
	return false;
#endif
}

#ifdef ENABLE_NEW_PET_EDITS
bool CNewPetActor::IncreasePetSkillByBook(entt::entity bookItemEntity)
{
	const entt::entity owner = m_owner;
	const entt::entity bookItem = bookItemEntity;
    if (!HasValidSummon() || IsTradeWindowOpen(m_owner) || !CanUseMaterial(m_owner, bookItem))
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
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 54, "");
#endif
		return false;
	}

	if (m_dwskill[ret] >= 10) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 58, "");
#endif
		return false;
	}

	char szName[128];
	snprintf(szName, sizeof(szName), "pet_skills.%d", iType);
	int iLast = ecs::QuestSystem::GetFlag(owner, szName);
	int iTime = iLast - get_global_time();
	if (iTime > 0) {
		if (!AffectSystem::FindAffect(owner, AFFECT_SKILL_NO_BOOK_DELAY)) {
			int iHours = iTime / 3600;
			int iMinutes = (iTime - (iHours * 3600)) / 60;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 51, "%d#%d", iHours, iMinutes);
#endif
			return false;
		}
	}

	if (!ItemSystem::ConsumeItemEcs(bookItem, 1))
        return false;

    if (iTime > 0)
        AffectSystem::RemoveAffect(owner, AFFECT_SKILL_NO_BOOK_DELAY);

	if (number(1, 100) < 30) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 52, "");
#endif
		return false;
	}

	m_dwskill[ret] += 1;

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 56, "%d", m_dwskill[ret]);
#endif
	ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", ret, m_dwskillslot[ret], m_dwskill[ret]);

	ClearBuff();
	GiveBuff();

	ecs::QuestSystem::SetFlag(owner, szName, get_global_time() + (3600 * 3));

	return true;
}

int CNewPetActor::ResetSkills()
{
    if (!HasValidSummon()) return 3;
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
		ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, m_dwskillslot[i], m_dwskill[i]);
	}

	ClearBuff();
	GiveBuff();

	return 1;
}

int CNewPetActor::ResetSkill(int iType)
{
    if (!HasValidSummon() || iType < 1 || iType > std::size(Pet_Skill_Table)) return 3;
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
	ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", ret, m_dwskillslot[ret], m_dwskill[ret]);

	ClearBuff();
	GiveBuff();

	return 1;
}
#endif

bool CNewPetActor::IncreasePetEvolution()
{
    if (!HasValidSummon() || m_dwevolution < 0 || m_dwevolution >= 3
        || GetLevel() < static_cast<uint32_t>(40 + m_dwevolution * 20))
        return false;
    ++m_dwevolution;
#ifdef ENABLE_NEW_PET_EDITS
    SetLevel(GetLevel() + 1);
    IncreasePetBonus();
    m_dwlevelstep = m_dwexp = m_dwexpitem = 0;
    ecs::PlayerRuntime::SetExp(m_character, 0);
#endif
    const int slot = m_dwevolution - 1;
    m_dwskillslot[slot] = m_dwskill[slot] = 0;
    SendPetLevelUpEffect(m_character, 1, GetLevel(), 1);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetEvolution %d", m_dwevolution);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", slot, 0, 0);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetExp %u %u %u", m_dwexp, GetExpI(), GetNextExp());
    if (m_dwevolution == 3)
    {
        const auto item = m_summonItem;
        Unsummon();
        return Summon("", item, false) != 0;
    }
    return true;
}

void CNewPetActor:: IncreasePetBonus()
{
    if (!HasValidSummon()) return;
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
    for (auto& bonus : m_dwbonuspet)
        bonus[1] = std::clamp(bonus[1], 0, static_cast<int>(std::numeric_limits<int16_t>::max()));
	ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetBonus %d %d %d", m_dwbonuspet[0][1], m_dwbonuspet[1][1], m_dwbonuspet[2][1]);
	const entt::entity pSummonItem = m_summonItem;
	if (OwnedItem(m_owner, pSummonItem)){
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
    if (!IsSummoned() || !ecs::PlayerRuntime::IsValid(m_owner) || level < 1 || level > 120)
        return;
    ecs::PlayerRuntime::SetLevel(m_character, static_cast<uint8_t>(level));
    m_dwlevel = level;
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetLevel %u", level);
    SetNextExp(GetNextExp());
    if (auto* pet = g_registry.try_get<ecs::GrowthPetComponent>(m_character))
        pet->level = level;
    if (OwnedItem(m_owner, m_summonItem))
    {
#ifdef ENABLE_NEW_PET_EDITS
        ItemSystem::SetItemForceAttributeEcs(m_summonItem, 3, 1, level);
#else
        ItemSystem::SetItemSocket(m_summonItem, 1, level);
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
    if (!HasValidSummon() || GetLevel() >= 120 || (mode != 0 && mode != 1))
        return;
	const entt::entity owner = m_owner;
	const entt::entity charEntity = m_character;

	if(mode == 0) {
#ifdef ENABLE_NEW_PET_EDITS
		if (static_cast<uint64_t>(GetExp()) + exp >= (uint32_t) GetNextExpFromMob())
#else
		if(static_cast<uint64_t>(GetExp()) + exp >= (uint32_t) GetNextExpFromMob() && GetExpI() >= (uint32_t) GetNextExpFromItem())
#endif
		{
			if(GetEvolution() == 0 && GetLevel() == 40) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				ecs::PlayerRuntime::SetExp(m_character, m_dwexp - 1);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp - 1, m_dwexpitem, GetNextExp());
				return;
			}
			else if(GetEvolution() <= 1 && GetLevel() == 60) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				ecs::PlayerRuntime::SetExp(m_character, m_dwexp - 1);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp - 1, m_dwexpitem, GetNextExp());
				return;
			}
			else if(GetEvolution() <= 2 && GetLevel() == 80) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				ecs::PlayerRuntime::SetExp(m_character, m_dwexp - 1);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp - 1, m_dwexpitem, GetNextExp());
				return;
			}
		}
	}
	else if(mode == 1) {
#ifdef ENABLE_NEW_PET_EDITS
		if(GetExp() >= (uint32_t) GetNextExpFromMob())
#else
		if(static_cast<uint64_t>(GetExpI()) + exp >= (uint32_t) GetNextExpFromItem() && GetExp() >= (uint32_t) GetNextExpFromMob())
#endif
		{
			if(GetEvolution() == 0 && GetLevel() == 40) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				ecs::PlayerRuntime::SetExp(m_character, m_dwexp);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
				return;
			}
			else if(GetEvolution() == 1 && GetLevel() == 60) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				ecs::PlayerRuntime::SetExp(m_character, m_dwexp);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
				return;
			}
			else if(GetEvolution() == 2 && GetLevel() == 80) {
				m_dwexp = (uint32_t) GetNextExpFromMob();
				ecs::PlayerRuntime::SetExp(m_character, m_dwexp);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
				return;
			}
		}
	}

	if (mode == 0)  {
		if (static_cast<uint64_t>(GetExp()) + exp >= (uint32_t) GetNextExpFromMob())  {
#ifndef ENABLE_NEW_PET_EDITS
			if (GetExpI() >= (uint32_t) GetNextExpFromItem())
#endif
			{
				SetLevel(GetLevel() + 1);
				SendPetLevelUpEffect(m_character, 1, GetLevel(), 1);
#ifndef ENABLE_NEW_PET_EDITS
				IncreasePetBonus();
#endif
				m_dwlevelstep = 0;
				m_dwexp = 0;
				m_dwexpitem = 0;
				ecs::PlayerRuntime::SetExp(m_character, 0);
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
				//SetEvolution(GetLevel());
				return;
			}
#ifndef ENABLE_NEW_PET_EDITS
			else  {
				SendPetLevelUpEffect(m_character, 25, GetLevel(), 1);
				m_dwlevelstep = 4;
				exp = GetNextExpFromMob() - GetExp();
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
			}
#endif
		}

		m_dwexp += exp;
		ecs::PlayerRuntime::SetExp(m_character, m_dwexp);
		ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
		if (GetLevelStep() < 4) {
			uint32_t dwNextExpQuart = GetNextExpFromMob() / 4;
			if (m_dwexp >= dwNextExpQuart * 3 && m_dwlevelstep == 2) {
				m_dwlevelstep = 3;
				SendPetLevelUpEffect(m_character, 25, GetLevel(), 1);
			} else if (m_dwexp >= dwNextExpQuart * 2 && m_dwlevelstep == 1) {
				m_dwlevelstep = 2;
				SendPetLevelUpEffect(m_character, 25, GetLevel(), 1);
			} else if (m_dwexp >= dwNextExpQuart && m_dwlevelstep == 0)  {
				m_dwlevelstep = 1;
				SendPetLevelUpEffect(m_character, 25, GetLevel(), 1);
			}
		}
	} else if (mode == 1)  {
		if (static_cast<uint64_t>(GetExpI()) + exp >= (uint32_t) GetNextExpFromItem()) {
			if (GetExp() >= (uint32_t) GetNextExpFromMob())
			{
				m_dwexpitem = static_cast<uint64_t>(GetExpI()) + exp - GetNextExpFromItem();
				m_dwexp = 0;
				ecs::PlayerRuntime::SetExp(m_character, 0);
				m_dwlevelstep = 0;
				SetLevel(GetLevel() + 1);
				SendPetLevelUpEffect(m_character, 1, GetLevel(), 1);
#ifndef ENABLE_NEW_PET_EDITS
				IncreasePetBonus();
#endif
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
				return;
			} else  {
				exp = GetNextExpFromItem() - GetExpI();
				ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
			}
		}

		m_dwexpitem += exp;
		ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", m_dwexp, m_dwexpitem, GetNextExp());
	}

}

bool CNewPetActor::Mount()
{
    if (!ecs::PlayerRuntime::IsValid(m_owner) || !HasOption(EPetOption_Mountable))
        return false;
    const auto skin = PetSkin(m_owner);
    m_ridingVnum = skin ? skin : m_dwVnum;
    MountSystem::SetMountVnum(m_owner, m_ridingVnum);
    return MountSystem::GetMountVnum(m_owner) == m_ridingVnum;
}

void CNewPetActor::UpdateTime(bool now)
{
    if (!HasValidSummon())
    {
        Unsummon();
        return;
    }
    if (!now && ++m_dwTimePet < 60)
        return;
    m_dwTimePet = 0;
#ifdef ENABLE_NEW_PET_EDITS
    lMinAge = static_cast<int32_t>(std::clamp<int64_t>(static_cast<int64_t>(get_global_time()) - dwMinAge, 0, INT_MAX));
    if (lMinAge >= 1296000 && m_dwskillslot[3] == -1)
    {
        m_dwskill[3] = m_dwskillslot[3] = 0;
        ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 3, 0, 0);
    }
    const uint8_t ageTier = lMinAge >= 4147200 ? 4 : lMinAge >= 2246400 ? 3 : lMinAge >= 950400 ? 2 : lMinAge >= 86400 ? 1 : 0;
    if (m_idx != ageTier || now)
    {
        m_idx = ageTier;
        ClearBuff();
        GiveBuff();
    }
#endif
    // Initialization/skin refresh must not consume a minute; zero must never
    // underflow into an effectively unlimited lifetime.
    if (!now && m_dwtduration <= 525600 && m_dwduration > 0)
        --m_dwduration;
#ifdef ENABLE_NEW_PET_EDITS
    ItemSystem::SetItemSocket(m_summonItem, 1, m_dwduration);
#else
    ItemSystem::SetItemForceAttributeEcs(m_summonItem, 3, 1, m_dwduration);
    ItemSystem::SetItemForceAttributeEcs(m_summonItem, 4, 1, m_dwtduration);
#endif
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetDuration %u %u", m_dwduration, m_dwtduration);
    if (m_dwduration == 0)
        Unsummon();
}

void CNewPetActor::Unmount()
{
    const auto ridingVnum = std::exchange(m_ridingVnum, 0u);
    if (!ecs::PlayerRuntime::IsValid(m_owner))
        return;
    if (ridingVnum && MountSystem::GetMountVnum(m_owner) == ridingVnum)
        MountSystem::SetMountVnum(m_owner, 0);
    if (MountSystem::IsHorseRiding(m_owner))
        MountSystem::StopRiding(m_owner);
}

void CNewPetActor::Unsummon()
{
    const auto character = std::exchange(m_character, entt::null);
    const auto item = m_summonItem;
    m_dwVID = 0;
    if (ecs::PlayerRuntime::IsValid(m_owner))
        if (auto* skills = g_registry.try_get<ecs::NewPetSkillState>(m_owner);
            skills && skills->immortalSource == character)
            skills->immortalSource = entt::null;
    if (m_ridingVnum)
        Unmount();
    SetSummonItem(entt::null);
    if (OwnedItem(m_owner, item))
    {
        std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
            "UPDATE new_petsystem SET level=%u,evolution=%d,exp=%u,expi=%u,bonus0=%d,bonus1=%d,bonus2=%d,"
            "skill0=%d,skill0lv=%d,skill1=%d,skill1lv=%d,skill2=%d,skill2lv=%d,skill3=%d,skill3lv=%d,"
            "duration=%u,tduration=%u WHERE id=%u",
            m_dwlevel, m_dwevolution, m_dwexp, GetExpI(), m_dwbonuspet[0][1], m_dwbonuspet[1][1], m_dwbonuspet[2][1],
            m_dwskillslot[0], m_dwskill[0], m_dwskillslot[1], m_dwskill[1], m_dwskillslot[2], m_dwskill[2],
            m_dwskillslot[3], m_dwskill[3], m_dwduration, m_dwtduration, ItemSystem::GetItemID(item)));
        if (!msg || msg->uiSQLErrno)
            LOG_ERROR("NewPet: failed to save item {}", ItemSystem::GetItemID(item));
        for (int b = 0; b < 3; ++b)
            ItemSystem::SetItemForceAttributeEcs(item, b, 1, m_dwbonuspet[b][1]);
#ifdef ENABLE_NEW_PET_EDITS
        ItemSystem::SetItemForceAttributeEcs(item, 3, 1, m_dwlevel);
        ItemSystem::SetItemSocket(item, 1, m_dwduration);
        ItemSystem::SetItemSocket(item, 2, m_dwtduration);
#else
        ItemSystem::SetItemForceAttributeEcs(item, 3, 1, m_dwduration);
        ItemSystem::SetItemForceAttributeEcs(item, 4, 1, m_dwtduration);
        ItemSystem::SetItemSocket(item, 1, m_dwlevel);
#endif
        ItemSystem::SetItemSocket(item, 0, 0);
        ItemSystem::UnlockItem(item);
    }
    if (item != entt::null && ecs::PlayerRuntime::IsValid(m_owner))
    {
        ClearBuff();
        ecs::PointSystem::Compute(m_owner);
        if (auto* system = ecs::PlayerRuntime::GetNewPetSystem(m_owner))
            system->RefreshBuff();
        ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetUnsummon");
    }
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
    m_dwlevel = 1;
    m_dwlevelstep = m_dwExpFromMob = m_dwExpFromItem = m_dwexp = m_dwexpitem = 0;
    m_dwTimePet = m_dwImmTime = m_dwslotimm = 0;
    m_feedItems.fill({});
}

uint32_t CNewPetActor::Summon(const char* petName, entt::entity item, bool spawnFar)
{
    if (!OwnedItem(m_owner, item))
        return 0;
    int32_t x = ecs::PlayerRuntime::GetX(m_owner);
    int32_t y = ecs::PlayerRuntime::GetY(m_owner);
    const auto z = ecs::PlayerRuntime::GetZ(m_owner);
    x += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    y += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    if (IsSummoned())
        return item == m_summonItem && SnapFollowerToOwner(m_character, m_owner, x, y, z) ? m_dwVID : 0;
    Unsummon();
    const auto seal = ItemSystem::GetItemVnum(item);
    if (seal < 55701 || seal > 55711)
        return 0;
    std::unique_ptr<SQLMsg> result(DBManager::instance().DirectQuery(
        "SELECT name,level,exp,expi,bonus0,bonus1,bonus2,skill0,skill0lv,skill1,skill1lv,skill2,skill2lv,skill3,skill3lv,"
        "duration,tduration,evolution,evocation"
#ifdef ENABLE_NEW_PET_EDITS
        ",minAge"
#endif
        " FROM new_petsystem WHERE id=%u", ItemSystem::GetItemID(item)));
    auto* table = result ? result->Get() : nullptr;
    constexpr unsigned fields =
#ifdef ENABLE_NEW_PET_EDITS
        20;
#else
        19;
#endif
    if (!result || result->uiSQLErrno || !table || table->uiNumRows != 1 || !table->pSQLResult
        || mysql_num_fields(table->pSQLResult) != fields)
        return 0;
    const auto row = mysql_fetch_row(table->pSQLResult);
    if (!row || !row[0])
        return 0;
    std::array<int64_t, 20> values {};
    for (unsigned i = 1; i < fields; ++i)
    {
        int64_t minimum = 0, maximum = INT_MAX;
        if (i == 1) { minimum = 1; maximum = 120; }
        if (i >= 4 && i <= 6) maximum = std::numeric_limits<int16_t>::max();
        if (i == 7 || i == 9 || i == 11 || i == 13) { minimum = -1; maximum = std::size(Pet_Skill_Table); }
        if (i == 8 || i == 10 || i == 12 || i == 14)
#ifdef ENABLE_NEW_PET_EDITS
            maximum = 10;
#else
            maximum = 20;
#endif
        if (i == 17) maximum = 3;
        if (i == 18) maximum = 1;
        if (i == 19) maximum = UINT32_MAX;
        if (!ReadPetNumber(row[i], minimum, maximum, values[i]))
            return 0;
    }
    if (!exppet_table || exppet_table[values[1]] == 0 || exppet_table[values[1]] > INT_MAX
        || values[15] == 0 || values[15] > values[16])
        return 0;
#ifdef ENABLE_NEW_PET_EDITS
    if (values[18] && ItemSystem::GetItemSocket(item, 1) == 0)
        return 0;
#endif
    static constexpr uint32_t races[11][2] = {
        {34041,34042}, {34045,34046}, {34049,34050}, {34053,34054}, {34036,34037},
        {34064,34065}, {34073,34074}, {34075,34076}, {34080,34081}, {34082,34083}, {34095,34096}
    };
    const auto skin = PetSkin(m_owner);
    const auto race = skin ? skin : races[seal - 55701][values[17] == 3];
    m_character = CHARACTER_MANAGER::instance().SpawnMobEntity(race, ecs::PlayerRuntime::GetMapIndex(m_owner),
        x, y, z, false, static_cast<int>(ecs::PlayerRuntime::GetRotation(m_owner) + 180), false);
    if (!IsSummoned())
        return 0;
    g_registry.get_or_emplace<ecs::StatusFlags>(m_character).isNewPet = true;
    ecs::PlayerRuntime::SetEmpire(m_character, ecs::PlayerRuntime::GetEmpire(m_owner));
    m_dwVID = ecs::PlayerRuntime::GetPacketVID(m_character);
    SetName(*row[0] ? row[0] : petName);
    m_dwevolution = static_cast<int>(values[17]);
    m_dwlevel = static_cast<uint32_t>(values[1]);
    ecs::PlayerRuntime::SetLevel(m_character, static_cast<uint8_t>(m_dwlevel));
    SetNextExp(GetNextExp());
    // Loading is not earning EXP: do not run level-up/evolution transitions here.
    m_dwexp = static_cast<uint32_t>(std::min<int64_t>(values[2], m_dwExpFromMob));
    m_dwexpitem = static_cast<int>(std::min<int64_t>(values[3], m_dwExpFromItem));
    ecs::PlayerRuntime::SetExp(m_character, m_dwexp);
    for (int i = 0; i < 3; ++i)
        m_dwbonuspet[i][1] = static_cast<int>(values[4 + i]);
    for (int i = 0; i < 4; ++i)
    {
        m_dwskillslot[i] = static_cast<int>(values[7 + i * 2]);
        m_dwskill[i] = static_cast<int>(values[8 + i * 2]);
    }
    m_dwduration = static_cast<uint32_t>(values[15]);
    m_dwtduration = static_cast<uint32_t>(values[16]);
#ifdef ENABLE_NEW_PET_EDITS
    dwMinAge = static_cast<uint32_t>(values[19]);
    lMinAge = 0;
    m_idx = 0;
#endif
    // The initial insert packet reads creature-side level before the seal is
    // committed to this actor. Failed Show still has no item to save/unlock.
    g_registry.emplace_or_replace<ecs::GrowthPetComponent>(m_character, m_owner, item, m_dwlevel);
    if (!ecs::MovementSystem::Show(m_character, ecs::PlayerRuntime::GetMapIndex(m_owner), x, y, z))
    {
        Unsummon();
        return 0;
    }
    if (values[18] == 0)
    {
        std::unique_ptr<SQLMsg> mark(DBManager::instance().DirectQuery(
            "UPDATE new_petsystem SET evocation=1 WHERE id=%u", ItemSystem::GetItemID(item)));
        if (!mark || mark->uiSQLErrno)
        {
            Unsummon();
            return 0;
        }
    }
    SetSummonItem(item);
    ItemSystem::LockItem(item);
    ItemSystem::SetItemSocket(item, 0, 1);
    for (int i = 0; i < 3; ++i)
        ItemSystem::SetItemForceAttributeEcs(item, i, 1, m_dwbonuspet[i][1]);
#ifdef ENABLE_NEW_PET_EDITS
    ItemSystem::SetItemForceAttributeEcs(item, 3, 1, m_dwlevel);
    ItemSystem::SetItemSocket(item, 1, m_dwduration);
    ItemSystem::SetItemSocket(item, 2, m_dwtduration);
#else
    ItemSystem::SetItemForceAttributeEcs(item, 3, 1, m_dwduration);
    ItemSystem::SetItemForceAttributeEcs(item, 4, 1, m_dwtduration);
    ItemSystem::SetItemSocket(item, 1, m_dwlevel);
#endif
    ecs::PointSystem::Compute(m_owner);
    UpdateTime(true);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetIcon %u", m_dwSummonItemVnum);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetEvolution %d", m_dwevolution);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetName %s", m_name.c_str());
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetLevel %u", m_dwlevel);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetBonus %d %d %d", m_dwbonuspet[0][1], m_dwbonuspet[1][1], m_dwbonuspet[2][1]);
#ifdef ENABLE_NEW_PET_EDITS
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetAge %u", dwMinAge);
#endif
    for (int i = 0; i < 4; ++i)
    {
        int slot = m_dwskillslot[i];
#ifndef ENABLE_NEW_PET_EDITS
        if (m_dwlevel < 80 || m_dwevolution != 3)
            slot = -1;
#endif
        ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, slot, m_dwskill[i]);
    }
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetExp %u %u %u", m_dwexp, GetExpI(), GetNextExp());
#ifdef ENABLE_RECALL
    AffectSystem::RemoveAffect(m_owner, AFFECT_RECALL2);
    AffectSystem::AddAffect(m_owner, AFFECT_RECALL2, APPLY_NONE, 0, ItemSystem::GetItemID(item),
        INFINITE_AFFECT_DURATION, 0, true, false);
#endif
    return m_dwVID;
}

bool CNewPetActor::_UpdatAloneActionAI(float minDistance, float maxDistance)
{
    if (!HasValidSummon())
        return false;
    const float distance = number(static_cast<int>(minDistance), static_cast<int>(maxDistance));
    const float radians = number(0, 359) * 3.141592f / 180.f;
    ecs::MovementSystem::SetNowWalking(m_character, true);
    if (!g_registry.all_of<ecs::MovementDestination>(m_character)
        && ecs::MovementSystem::Goto(m_character, ecs::PlayerRuntime::GetX(m_owner) + distance * cos(radians),
            ecs::PlayerRuntime::GetY(m_owner) + distance * sin(radians)))
        ecs::MovementSystem::SendMovePacket(m_character, FUNC_WAIT, 0, 0, 0, 0);
    m_dwLastActionTime = get_dword_time();
    return true;
}

// StateHorse함수 그냥 C&P -_-;
bool CNewPetActor::_UpdateFollowAI()
{
    if (!IsSummoned() || !ecs::PlayerRuntime::IsValid(m_owner)
        || !ecs::PlayerRuntime::GetMobTable(m_character))
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(m_owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(m_owner);
    const auto charX = ecs::PlayerRuntime::GetX(m_character);
    const auto charY = ecs::PlayerRuntime::GetY(m_character);
    const float distance = DISTANCE_APPROX(charX - ownerX, charY - ownerY);
    constexpr int approach = 200;
    if (distance >= 4500.f || ecs::PlayerRuntime::GetMapIndex(m_character) != ecs::PlayerRuntime::GetMapIndex(m_owner))
    {
        const float rotation = ecs::PlayerRuntime::GetRotation(m_owner) * 3.141592f / 180.f;
        return SnapFollowerToOwner(m_character, m_owner, ownerX - approach * cos(rotation),
            ownerY - approach * sin(rotation), ecs::PlayerRuntime::GetZ(m_owner));
    }
    if (distance >= 300.f)
    {
        ecs::MovementSystem::SetNowWalking(m_character, distance < 900.f);
        Follow(approach);
        CombatSystem::SetLastAttacked(m_character, get_dword_time());
    }
    else
        ecs::MovementSystem::SendMovePacket(m_character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool CNewPetActor::Update(uint32_t deltaTime)
{
    if (!HasValidSummon() || CombatSystem::IsDead(m_character) || m_dwduration == 0)
    {
        Unsummon();
        return true;
    }
#ifndef ENABLE_NEW_PET_EDITS
    if (m_dwslotimm >= 0 && m_dwslotimm < 4)
    {
        auto& state = g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner);
        const int row = m_dwskillslot[m_dwslotimm] - 1;
        const int column = 2 + m_dwskill[m_dwslotimm];
        if (state.immortalSource == m_character && row >= 0 && row < std::size(Pet_Skill_Table)
            && column >= 2 && column < std::size(Pet_Skill_Table[0])
            && Pet_Skill_Table[row][column] <= (get_global_time() - m_dwImmTime) * 10)
            state.immortalSource = entt::null;
    }
#endif
    return !HasOption(EPetOption_Followable) || _UpdateFollowAI();
}

//NOTE : 주의!!! MinDistance를 크게 잡으면 그 변위만큼의 변화동안은 follow하지 않는다,
bool CNewPetActor::Follow(float minDistance)
{
    if (!ecs::PlayerRuntime::IsValid(m_owner) || !IsSummoned())
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(m_owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(m_owner);
    const auto charX = ecs::PlayerRuntime::GetX(m_character);
    const auto charY = ecs::PlayerRuntime::GetY(m_character);
    const float distance = DISTANCE_SQRT(ownerX - charX, ownerY - charY);
    if (distance <= minDistance)
        return false;
    ecs::MovementSystem::SetRotation(m_character, GetDegreeFromPositionXY(charX, charY, ownerX, ownerY));
    float dx, dy;
    GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(m_character), distance - minDistance, &dx, &dy);
    if (!ecs::MovementSystem::Goto(m_character, static_cast<int>(charX + dx + 0.5f), static_cast<int>(charY + dy + 0.5f)))
        return false;
    ecs::MovementSystem::SendMovePacket(m_character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

void CNewPetActor::SetSummonItem(entt::entity item)
{
    if (!OwnedItem(m_owner, item))
    {
        m_summonItem = entt::null;
        m_dwSummonItemVID = m_dwSummonItemID = m_dwSummonItemVnum = 0;
        return;
    }
    m_summonItem = item;
    m_dwSummonItemVID = ItemSystem::GetItemVID(item);
    m_dwSummonItemID = ItemSystem::GetItemID(item);
    m_dwSummonItemVnum = ItemSystem::GetItemVnum(item);
    if (IsSummoned())
        g_registry.emplace_or_replace<ecs::GrowthPetComponent>(m_character, m_owner, item, m_dwlevel);
}

void CNewPetActor::GiveBuff()
{
    if (!HasValidSummon()) return;
	const entt::entity owner = m_owner;
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
		AffectSystem::AddAffect(owner, AFFECT_NEW_PET, val[i][0], val[i][idx], 0, 60 * 60 * 24 * 365, 0, false);
		if (m_dwbonuspet[i][1] > 0) {
			AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[m_dwbonuspet[i][0]].bPointType, float(m_dwbonuspet[i][1]/10), 0, 60 * 60 * 24 * 365, 0, false);
		}
	}

	for (int i = 0; i < 4; i++) {
		idx = m_dwskillslot[i];
		if (idx > 0 && idx <= std::size(Pet_Skill_Table) && m_dwskill[i] > 0
            && 1 + m_dwskill[i] < std::size(Pet_Skill_Table[0]) && Pet_Skill_Table[idx-1][1] < MAX_APPLY_NUM) {
			AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[Pet_Skill_Table[m_dwskillslot[i]-1][1]].bPointType, Pet_Skill_Table[m_dwskillslot[i]-1][1+m_dwskill[i]], 0, 60 * 60 * 24 * 365, 0, false);
		}
	}
#else
	//Inizializzo i bonus del NewPetSystem //hp sp e def
	// 559 Affect NewPet
	int cbonus[3] = { ecs::PointSystem::GetMaxHP(owner),  ecs::PointSystem::Get(owner, POINT_DEF_GRADE), ecs::PointSystem::GetMaxSP(owner) };
	for (int i = 0; i < 3; ++i) {
		AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[m_dwbonuspet[i][0]].bPointType, float((cbonus[i]*m_dwbonuspet[i][1]/10)/1000), 0,  60 * 60 * 24 * 365, 0, false);
	}

	//Inizializzo le skill del pet inattive  No 10-17-18 No 0 no -1
	//Condizione lv > 81 evo 3 Solo Skill Passive
	if (GetLevel() >= 80 && m_dwevolution == 3)
	{
		for (int s = 0; s < 3; s++)
		{
			if (m_dwskillslot[s] < 1 || m_dwskillslot[s] > std::size(Pet_Skill_Table)
                || m_dwskill[s] < 1 || 2 + m_dwskill[s] >= std::size(Pet_Skill_Table[0])
                || Pet_Skill_Table[m_dwskillslot[s]-1][0] >= MAX_APPLY_NUM) continue;
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
				AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[Pet_Skill_Table[m_dwskillslot[s] - 1][0]].bPointType, float(Pet_Skill_Table[m_dwskillslot[s] - 1][2 + m_dwskill[s]]/10), 0, 60 * 60 * 24 * 365, 0, false);
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
    if (!ecs::PlayerRuntime::IsValid(m_owner)) return;
	AffectSystem::RemoveAffect(m_owner, AFFECT_NEW_PET);
}

void CNewPetActor::DoPetSkill(int skillslot) {
    if (!HasValidSummon() || skillslot < 0 || skillslot >= 4 || m_dwskillslot[skillslot] < 1
        || m_dwskillslot[skillslot] > std::size(Pet_Skill_Table) || m_dwskill[skillslot] < 1
        || 2 + m_dwskill[skillslot] >= std::size(Pet_Skill_Table[0])) return;
#ifdef ENABLE_NEW_PET_EDITS
	return;
#else
	if (GetLevel() < 80 || m_dwevolution < 3)
		return;
	const entt::entity owner = m_owner;
	switch (m_dwskillslot[skillslot])
	{
	case 10:
	{
		if (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[0] <= 480) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 749, "%d", (480 - (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[0])));
#endif
			return;
		}
		if (ecs::PlayerRuntime::GetHPPct(m_owner) > 20) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 750, "");
#endif
			return;
		}
		g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[0] = get_global_time();
		int riphp = MIN(ecs::PlayerRuntime::GetHP(m_owner) + (int)Pet_Skill_Table[9][2 + m_dwskill[skillslot]], ecs::PointSystem::GetMaxHP(owner));
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 751, "");
#endif
		ecs::PointSystem::Change(owner, POINT_HP, riphp);
		NetworkSyncSystem::BroadcastEffect(g_registry, owner, SE_HPUP_RED);
	}
	break;

	case 17:
	{
		if (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[1] <= 600) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 749, "%d", (600 - (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[1])));
#endif
			return;
		}
		g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[1] = get_global_time();
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 752, "");
#endif
		g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).immortalSource = m_character;
		m_dwslotimm = skillslot;
		m_dwImmTime = get_global_time();
	}
	break;
	case 18:
	{
		if (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[2] <= 480) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 749, "%d", (480 - (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[2])));
#endif
			return;
		}
		g_registry.get_or_emplace<ecs::NewPetSkillState>(m_owner).cooldowns[2] = get_global_time();
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 753, "");
#endif
		AffectSystem::RemoveBadAffects(m_owner);
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

CNewPetSystem::CNewPetSystem(entt::entity owner)
    : m_owner(owner), m_dwUpdatePeriod(400), m_dwLastUpdateTime(0)
{
    if (ecs::PlayerRuntime::IsValid(owner))
        g_registry.get_or_emplace<ecs::PetRuntimeRefs>(owner).newPetSystem = this;
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
    if (m_destroying)
        return;
    m_destroying = true;
    event_cancel(&m_pkNewPetSystemUpdateEvent);
    event_cancel(&m_pkNewPetSystemExpireEvent);
    while (!m_petActorMap.empty())
    {
        auto detached = m_petActorMap.extract(m_petActorMap.begin());
    }
    if (ecs::PlayerRuntime::IsValid(m_owner))
        if (auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(m_owner); refs && refs->newPetSystem == this)
            refs->newPetSystem = nullptr;
    m_destroying = false;
}


void CNewPetSystem::UpdateTime()
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second.get();

		if (nullptr != petActor && petActor->IsSummoned())
		{
			petActor->UpdateTime();
		}
	}
}
/// 펫 시스템 업데이트. 등록된 펫들의 AI 처리 등을 함.
bool CNewPetSystem::Update(uint32_t deltaTime)
{
    const auto now = get_dword_time();
    if (m_dwUpdatePeriod > now - m_dwLastUpdateTime)
        return true;
    bool result = true;
    for (auto& [vnum, actor] : m_petActorMap)
        if (actor->GetCharacter() != entt::null || actor->GetSummonItem() != entt::null)
            result = actor->Update(deltaTime) && result;
    m_dwLastUpdateTime = now;
    if (CountSummoned() == 0)
    {
        event_cancel(&m_pkNewPetSystemUpdateEvent);
        event_cancel(&m_pkNewPetSystemExpireEvent);
    }
    return result;
}

void CNewPetSystem::SetUpdatePeriod(uint32_t ms)
{
    m_dwUpdatePeriod = ms;
}

/// 관리 목록에서 펫을 지움
void CNewPetSystem::DeletePet(uint32_t vnum)
{
    auto detached = m_petActorMap.extract(vnum);
    if (!detached.empty())
        detached.mapped().reset();
    if (CountSummoned() == 0)
    {
        event_cancel(&m_pkNewPetSystemUpdateEvent);
        event_cancel(&m_pkNewPetSystemExpireEvent);
    }
}

/// 관리 목록에서 펫을 지움
void CNewPetSystem::DeletePet(CNewPetActor* actor)
{
    for (const auto& [vnum, owned] : m_petActorMap)
        if (owned.get() == actor)
        {
            DeletePet(vnum);
            return;
        }
}

void CNewPetSystem::Unsummon(uint32_t vnum, bool deleteFromList)
{
    if (deleteFromList)
        DeletePet(vnum);
    else if (auto* actor = GetByVnum(vnum))
        actor->Unsummon();
    if (CountSummoned() == 0)
    {
        event_cancel(&m_pkNewPetSystemUpdateEvent);
        event_cancel(&m_pkNewPetSystemExpireEvent);
    }
}

void CNewPetSystem::Unsummon(CNewPetActor* actor, bool deleteFromList)
{
    for (const auto& [vnum, owned] : m_petActorMap)
        if (owned.get() == actor)
        {
            Unsummon(vnum, deleteFromList);
            return;
        }
}

void CNewPetSystem::UnsummonAll()
{
    event_cancel(&m_pkNewPetSystemUpdateEvent);
    event_cancel(&m_pkNewPetSystemExpireEvent);
#ifdef ENABLE_RECALL
    if (ecs::PlayerRuntime::IsValid(m_owner))
        AffectSystem::RemoveAffect(m_owner, AFFECT_RECALL2);
#endif
    for (auto& [vnum, actor] : m_petActorMap)
        actor->Unsummon();
}

uint32_t CNewPetSystem::GetNewPetITemID()
{
	uint32_t itemid = 0;
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
    if (iExp <= 0) return;
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				return petActor->DoPetSkill(skillslot);
			}
		}
	}
}



CNewPetActor* CNewPetSystem::Summon(uint32_t vnum, entt::entity item, const char* name, bool spawnFar, uint32_t options)
{
    if (m_destroying || !OwnedItem(m_owner, item))
        return nullptr;
    for (const auto& [key, owned] : m_petActorMap)
        if (key != vnum && owned->GetSummonItem() == item)
            return nullptr;
    g_registry.get_or_emplace<ecs::PetRuntimeRefs>(m_owner).newPetSystem = this;
    auto* actor = GetByVnum(vnum);
    if (!actor)
    {
        auto fresh = std::make_unique<CNewPetActor>(m_owner, vnum, options);
        actor = fresh.get();
        m_petActorMap.emplace(vnum, std::move(fresh));
    }
    if (!actor->Summon(name, item, spawnFar))
        return nullptr;
    if (!m_pkNewPetSystemUpdateEvent)
    {
        auto* info = AllocEventInfo<newpetsystem_event_info>();
        info->owner = m_owner;
        m_pkNewPetSystemUpdateEvent = event_create(newpetsystem_update_event, info, PASSES_PER_SEC(1) / 4);
    }
    if (!m_pkNewPetSystemExpireEvent)
    {
        auto* info = AllocEventInfo<newpetsystem_event_info>();
        info->owner = m_owner;
        m_pkNewPetSystemExpireEvent = event_create(newpetsystem_expire_event, info, PASSES_PER_SEC(1));
    }
    return actor;
}


CNewPetActor* CNewPetSystem::GetByVID(uint32_t vid) const
{
    if (vid)
        for (const auto& [vnum, actor] : m_petActorMap)
            if (actor->IsSummoned() && actor->GetVID() == vid)
                return actor.get();
    return nullptr;
}

/// 등록 된 펫 중에서 주어진 몹 VNUM을 가진 액터를 반환하는 함수.
CNewPetActor* CNewPetSystem::GetByVnum(uint32_t vnum) const
{
	CNewPetActor* petActor = nullptr;

	TNewPetActorMap::const_iterator iter = m_petActorMap.find(vnum);

	if (m_petActorMap.end() != iter)
		petActor = iter->second.get();

	return petActor;
}

size_t CNewPetSystem::CountSummoned() const
{
	size_t count = 0;

	for (TNewPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CNewPetActor* petActor = iter->second.get();

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
	const entt::entity pSummonItem = m_summonItem;
	if (OwnedItem(m_owner, pSummonItem)){
		Unsummon();
		Summon("Noname", pSummonItem, false);
	}
}

void CNewPetSystem::UpdatePetSkin() {
	for (TNewPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter) {
		CNewPetActor* petActor = iter->second.get();
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
		CNewPetActor* petActor = iter->second.get();

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

void CNewPetActor::ChangeName(const char* name)
{
    if (!HasValidSummon() || !name || !*name || strlen(name) > CHARACTER_NAME_MAX_LEN)
        return;
    char escaped[CHARACTER_NAME_MAX_LEN * 2 + 1] {};
    DBManager::instance().EscapeString(escaped, sizeof(escaped), name, static_cast<uint32_t>(strlen(name)));
    std::unique_ptr<SQLMsg> result(DBManager::instance().DirectQuery(
        "UPDATE new_petsystem SET name='%s' WHERE id=%u", escaped, m_dwSummonItemID));
    if (!result || result->uiSQLErrno)
        return;
    SetName(name);
    ecs::ChatSystem::Send(m_owner, CHAT_TYPE_COMMAND, "PetName %s", m_name.c_str());
    ecs::ViewSystem::ViewReencode(m_character);
}

void CNewPetSystem::ChangeName(const char * name)
{
	for (TNewPetActorMap::iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter) {
		CNewPetActor* petActor = iter->second.get();
		if (petActor != nullptr) {
			if (petActor->IsSummoned()) {
				return petActor->ChangeName(name);
			}
		}
	}
}


