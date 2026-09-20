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
#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>
#include <vector>

//#define DISABLE_TRADE_UNSUMMON // this disable the unsummon of pet when a excange/trade/shop/myshop/safebox windows is open, MAKE SURE to have set the items with vnum 55401/55402/55403/55404 with antiflag ANTI_SAFEBOX | ANTI_PKDROP | ANTI_DROP | ANTI_SELL | ANTI_GIVE | ANTI_STACK | ANTI_MYSHOP
//USE AT OWN YOUR RISK

EVENTINFO(newpetsystem_event_info)
{
    entt::entity owner { entt::null };
};

EVENTFUNC(newpetsystem_update_event);
EVENTFUNC(newpetsystem_expire_event);

namespace
{
using NewPetRecord = ecs::NewPetActorState;
using NewPetRuntime = ecs::NewPetRuntime;

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
    return ecs::SocialSystem::HasExchange(owner) || ecs::SocialSystem::GetMyShop(owner)
        || ecs::SocialSystem::GetShopOwner(owner) != entt::null
        || (safebox && safebox->isOpening) || (cube && g_registry.valid(cube->npc));
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

NewPetRuntime* Runtime(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return nullptr;
    return &g_registry.get_or_emplace<NewPetRuntime>(owner);
}

NewPetRecord* FindLiveRecord(NewPetRuntime& runtime, uint32_t vnum)
{
    if (vnum == 0)
        return nullptr;
    for (auto& record : runtime.actors)
        if (record.vnum == vnum)
            return &record;
    return nullptr;
}

NewPetRecord* FreeRecordSlot(NewPetRuntime& runtime)
{
    for (auto& record : runtime.actors)
        if (record.vnum == 0)
            return &record;
    return nullptr;
}

NewPetRecord* FindSummonedRecord(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return nullptr;
    auto* runtime = g_registry.try_get<NewPetRuntime>(owner);
    if (!runtime)
        return nullptr;
    for (auto& record : runtime->actors)
        if (NewPetSystem::IsSummoned(record))
            return &record;
    return nullptr;
}

uint32_t NextExp(const NewPetRecord& actor)
{
    return exppet_table && actor.level <= 120 ? exppet_table[actor.level] : 2500000000u;
}

void SetSummonItem(entt::entity owner, NewPetRecord& actor, entt::entity item)
{
    if (!OwnedItem(owner, item))
    {
        actor.summonItem = entt::null;
        actor.summonItemVID = actor.summonItemID = actor.summonItemVnum = 0;
        return;
    }
    actor.summonItem = item;
    actor.summonItemVID = ItemSystem::GetItemVID(item);
    actor.summonItemID = ItemSystem::GetItemID(item);
    actor.summonItemVnum = ItemSystem::GetItemVnum(item);
    if (NewPetSystem::IsSummoned(actor))
        g_registry.emplace_or_replace<ecs::GrowthPetComponent>(actor.character, owner, item, actor.level);
}

void SetActorName(entt::entity owner, NewPetRecord& actor, const char* name)
{
    actor.name = name ? name : "";
    if (NewPetSystem::IsSummoned(actor))
        g_registry.emplace_or_replace<ecs::PlayerName>(actor.character, actor.name);
}

void ClearBuff(entt::entity owner)
{
    if (!ecs::PlayerRuntime::IsValid(owner))
        return;
    AffectSystem::RemoveAffect(owner, AFFECT_NEW_PET);
}

void GiveBuff(entt::entity owner, NewPetRecord& actor)
{
    if (!NewPetSystem::HasValidSummon(owner, actor))
        return;
#ifdef ENABLE_NEW_PET_EDITS
    int idx = 1;
    if ((actor.minAge >= 950400) && (actor.minAge < 2246400)) {
        idx = 2;
    } else if ((actor.minAge >= 2246400) && (actor.minAge < 4147200)) {
        idx = 3;
    } else if (actor.minAge >= 4147200) {
        idx = 4;
    }

    int val[3][5] = {{POINT_MAX_HP, 500, 1200, 2100, 3000}, {POINT_RESIST_MONSTER, 2, 4, 7, 10}, {POINT_RESIST_MEZZIUOMINI, 2, 4, 7, 10}};

    for (int i = 0; i < 3; ++i) {
        AffectSystem::AddAffect(owner, AFFECT_NEW_PET, val[i][0], val[i][idx], 0, 60 * 60 * 24 * 365, 0, false);
        if (actor.bonusPet[i][1] > 0) {
            AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[actor.bonusPet[i][0]].bPointType, float(actor.bonusPet[i][1]/10), 0, 60 * 60 * 24 * 365, 0, false);
        }
    }

    for (int i = 0; i < 4; i++) {
        idx = actor.skillSlot[i];
        if (idx > 0 && idx <= std::size(Pet_Skill_Table) && actor.skill[i] > 0
            && 1 + actor.skill[i] < std::size(Pet_Skill_Table[0]) && Pet_Skill_Table[idx-1][1] < MAX_APPLY_NUM) {
            AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[Pet_Skill_Table[idx-1][1]].bPointType, Pet_Skill_Table[idx-1][1+actor.skill[i]], 0, 60 * 60 * 24 * 365, 0, false);
        }
    }
#else
    //Inizializzo i bonus del NewPetSystem //hp sp e def
    // 559 Affect NewPet
    int cbonus[3] = { ecs::PointSystem::GetMaxHP(owner),  ecs::PointSystem::Get(owner, POINT_DEF_GRADE), ecs::PointSystem::GetMaxSP(owner) };
    for (int i = 0; i < 3; ++i) {
        AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[actor.bonusPet[i][0]].bPointType, float((cbonus[i]*actor.bonusPet[i][1]/10)/1000), 0,  60 * 60 * 24 * 365, 0, false);
    }

    //Inizializzo le skill del pet inattive  No 10-17-18 No 0 no -1
    //Condizione lv > 81 evo 3 Solo Skill Passive
    if (actor.level >= 80 && actor.evolution == 3)
    {
        for (int s = 0; s < 3; s++)
        {
            if (actor.skillSlot[s] < 1 || actor.skillSlot[s] > std::size(Pet_Skill_Table)
                || actor.skill[s] < 1 || 2 + actor.skill[s] >= std::size(Pet_Skill_Table[0])
                || Pet_Skill_Table[actor.skillSlot[s]-1][0] >= MAX_APPLY_NUM) continue;
            switch (actor.skillSlot[s])
            {

            /*
                Pet_Skill_Table[actor.skillSlot[s] - 1][0]; //Mi ritorna il type della skill
                Pet_Skill_Table[actor.skillSlot[s] - 1][1]; // Mi ritorna attiva/passiva della skill
                Pet_Skill_Table[actor.skillSlot[s] - 1][2]; // Mi ritorna il cd della skill
                Pet_Skill_Table[actor.skillSlot[s] - 1][2 + actor.skill[s]]; //Mi ritorna l'apply della skill
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
                AffectSystem::AddAffect(owner, AFFECT_NEW_PET, aApplyInfo[Pet_Skill_Table[actor.skillSlot[s] - 1][0]].bPointType, float(Pet_Skill_Table[actor.skillSlot[s] - 1][2 + actor.skill[s]]/10), 0, 60 * 60 * 24 * 365, 0, false);
                break;
            default:
                return;
            }
        }
    }
#endif
}

void UnmountActor(entt::entity owner, NewPetRecord& actor)
{
    const auto ridingVnum = std::exchange(actor.ridingVnum, 0u);
    if (!ecs::PlayerRuntime::IsValid(owner))
        return;
    if (ridingVnum && MountSystem::GetMountVnum(owner) == ridingVnum)
        MountSystem::SetMountVnum(owner, 0);
    if (MountSystem::IsHorseRiding(owner))
        MountSystem::StopRiding(owner);
}

void TeardownActor(entt::entity owner, NewPetRecord& actor, bool keepRecord)
{
    const auto character = std::exchange(actor.character, entt::null);
    const auto item = actor.summonItem;
    actor.vid = 0;
    if (ecs::PlayerRuntime::IsValid(owner))
        if (auto* skills = g_registry.try_get<ecs::NewPetSkillState>(owner);
            skills && skills->immortalSource == character)
            skills->immortalSource = entt::null;
    if (actor.ridingVnum)
        UnmountActor(owner, actor);
    SetSummonItem(owner, actor, entt::null);
    if (OwnedItem(owner, item))
    {
        std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
            "UPDATE new_petsystem SET level=%u,evolution=%d,exp=%u,expi=%u,bonus0=%d,bonus1=%d,bonus2=%d,"
            "skill0=%d,skill0lv=%d,skill1=%d,skill1lv=%d,skill2=%d,skill2lv=%d,skill3=%d,skill3lv=%d,"
            "duration=%u,tduration=%u WHERE id=%u",
            actor.level, actor.evolution, actor.exp, actor.expItem, actor.bonusPet[0][1], actor.bonusPet[1][1], actor.bonusPet[2][1],
            actor.skillSlot[0], actor.skill[0], actor.skillSlot[1], actor.skill[1], actor.skillSlot[2], actor.skill[2],
            actor.skillSlot[3], actor.skill[3], actor.duration, actor.totalDuration, ItemSystem::GetItemID(item)));
        if (!msg || msg->uiSQLErrno)
            LOG_ERROR("NewPet: failed to save item {}", ItemSystem::GetItemID(item));
        for (int b = 0; b < 3; ++b)
            ItemSystem::SetItemForceAttributeEcs(item, b, 1, actor.bonusPet[b][1]);
#ifdef ENABLE_NEW_PET_EDITS
        ItemSystem::SetItemForceAttributeEcs(item, 3, 1, actor.level);
        ItemSystem::SetItemSocket(item, 1, actor.duration);
        ItemSystem::SetItemSocket(item, 2, actor.totalDuration);
#else
        ItemSystem::SetItemForceAttributeEcs(item, 3, 1, actor.duration);
        ItemSystem::SetItemForceAttributeEcs(item, 4, 1, actor.totalDuration);
        ItemSystem::SetItemSocket(item, 1, actor.level);
#endif
        ItemSystem::SetItemSocket(item, 0, 0);
        ItemSystem::UnlockItem(item);
    }
    if (item != entt::null && ecs::PlayerRuntime::IsValid(owner))
    {
        ClearBuff(owner);
        ecs::PointSystem::Compute(owner);
        NewPetSystem::RefreshBuff(owner);
        ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetUnsummon");
    }
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
    actor.level = 1;
    actor.levelStep = actor.expFromMob = actor.expFromItem = actor.exp = actor.expItem = 0;
    actor.timePet = actor.immTime = actor.slotImm = 0;
    actor.feedItems.fill({});
    if (!keepRecord)
        actor = {};
}

void SetNextExp(NewPetRecord& actor, int nextExp)
{
    actor.expFromMob = (nextExp/10)* 9;
    //actor.expFromMob = nextExp;
    //actor.expFromItem = 0;
    actor.expFromItem = nextExp - actor.expFromMob;
}

void SetActorLevel(entt::entity owner, NewPetRecord& actor, uint32_t level)
{
    if (!NewPetSystem::IsSummoned(actor) || !ecs::PlayerRuntime::IsValid(owner) || level < 1 || level > 120)
        return;
    ecs::PlayerRuntime::SetLevel(actor.character, static_cast<uint8_t>(level));
    actor.level = level;
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetLevel %u", level);
    SetNextExp(actor, NextExp(actor));
    if (auto* pet = g_registry.try_get<ecs::GrowthPetComponent>(actor.character))
        pet->level = level;
    if (OwnedItem(owner, actor.summonItem))
    {
#ifdef ENABLE_NEW_PET_EDITS
        ItemSystem::SetItemForceAttributeEcs(actor.summonItem, 3, 1, level);
#else
        ItemSystem::SetItemSocket(actor.summonItem, 1, level);
#endif
    }
}

void IncreasePetBonus(entt::entity owner, NewPetRecord& actor)
{
    if (!NewPetSystem::HasValidSummon(owner, actor)) return;
    int tmplevel = actor.level;
    if (tmplevel % 5 == 0) {
        actor.bonusPet[0][1] += float(number(1, 6));

    }
    if (tmplevel % 7 == 0) {
        actor.bonusPet[1][1] += float(number(1, 6));
    }
    if (tmplevel % 4 == 0) {
        actor.bonusPet[2][1] += float(number(1, 6));
    }
    for (auto& bonus : actor.bonusPet)
        bonus[1] = std::clamp(bonus[1], 0, static_cast<int>(std::numeric_limits<int16_t>::max()));
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetBonus %d %d %d", actor.bonusPet[0][1], actor.bonusPet[1][1], actor.bonusPet[2][1]);
    const entt::entity pSummonItem = actor.summonItem;
    if (OwnedItem(owner, pSummonItem)){
        for (int b = 0; b < 3; b++){
            ItemSystem::SetItemForceAttributeEcs(pSummonItem, b, 1, actor.bonusPet[b][1]);
        }

    }
}

void SetActorExp(entt::entity owner, NewPetRecord& actor, uint32_t exp, int mode)
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || actor.level >= 120 || (mode != 0 && mode != 1))
        return;
    const entt::entity charEntity = actor.character;

    if(mode == 0) {
#ifdef ENABLE_NEW_PET_EDITS
        if (static_cast<uint64_t>(actor.exp) + exp >= (uint32_t) actor.expFromMob)
#else
        if(static_cast<uint64_t>(actor.exp) + exp >= (uint32_t) actor.expFromMob && actor.expItem >= (uint32_t) actor.expFromItem)
#endif
        {
            if(actor.evolution == 0 && actor.level == 40) {
                actor.exp = (uint32_t) actor.expFromMob;
                ecs::PlayerRuntime::SetExp(charEntity, actor.exp - 1);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp - 1, actor.expItem, NextExp(actor));
                return;
            }
            else if(actor.evolution <= 1 && actor.level == 60) {
                actor.exp = (uint32_t) actor.expFromMob;
                ecs::PlayerRuntime::SetExp(charEntity, actor.exp - 1);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp - 1, actor.expItem, NextExp(actor));
                return;
            }
            else if(actor.evolution <= 2 && actor.level == 80) {
                actor.exp = (uint32_t) actor.expFromMob;
                ecs::PlayerRuntime::SetExp(charEntity, actor.exp - 1);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp - 1, actor.expItem, NextExp(actor));
                return;
            }
        }
    }
    else if(mode == 1) {
#ifdef ENABLE_NEW_PET_EDITS
        if(actor.exp >= (uint32_t) actor.expFromMob)
#else
        if(static_cast<uint64_t>(actor.expItem) + exp >= (uint32_t) actor.expFromItem && actor.exp >= (uint32_t) actor.expFromMob)
#endif
        {
            if(actor.evolution == 0 && actor.level == 40) {
                actor.exp = (uint32_t) actor.expFromMob;
                ecs::PlayerRuntime::SetExp(charEntity, actor.exp);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
                return;
            }
            else if(actor.evolution == 1 && actor.level == 60) {
                actor.exp = (uint32_t) actor.expFromMob;
                ecs::PlayerRuntime::SetExp(charEntity, actor.exp);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
                return;
            }
            else if(actor.evolution == 2 && actor.level == 80) {
                actor.exp = (uint32_t) actor.expFromMob;
                ecs::PlayerRuntime::SetExp(charEntity, actor.exp);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
                return;
            }
        }
    }

    if (mode == 0)  {
        if (static_cast<uint64_t>(actor.exp) + exp >= (uint32_t) actor.expFromMob)  {
#ifndef ENABLE_NEW_PET_EDITS
            if (actor.expItem >= (uint32_t) actor.expFromItem)
#endif
            {
                SetActorLevel(owner, actor, actor.level + 1);
                SendPetLevelUpEffect(charEntity, 1, actor.level, 1);
#ifndef ENABLE_NEW_PET_EDITS
                IncreasePetBonus(owner, actor);
#endif
                actor.levelStep = 0;
                actor.exp = 0;
                actor.expItem = 0;
                ecs::PlayerRuntime::SetExp(charEntity, 0);
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
                //SetEvolution(GetLevel());
                return;
            }
#ifndef ENABLE_NEW_PET_EDITS
            else  {
                SendPetLevelUpEffect(charEntity, 25, actor.level, 1);
                actor.levelStep = 4;
                exp = actor.expFromMob - actor.exp;
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
            }
#endif
        }

        actor.exp += exp;
        ecs::PlayerRuntime::SetExp(charEntity, actor.exp);
        ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
        if (actor.levelStep < 4) {
            uint32_t dwNextExpQuart = actor.expFromMob / 4;
            if (actor.exp >= dwNextExpQuart * 3 && actor.levelStep == 2) {
                actor.levelStep = 3;
                SendPetLevelUpEffect(charEntity, 25, actor.level, 1);
            } else if (actor.exp >= dwNextExpQuart * 2 && actor.levelStep == 1) {
                actor.levelStep = 2;
                SendPetLevelUpEffect(charEntity, 25, actor.level, 1);
            } else if (actor.exp >= dwNextExpQuart && actor.levelStep == 0)  {
                actor.levelStep = 1;
                SendPetLevelUpEffect(charEntity, 25, actor.level, 1);
            }
        }
    } else if (mode == 1)  {
        if (static_cast<uint64_t>(actor.expItem) + exp >= (uint32_t) actor.expFromItem) {
            if (actor.exp >= (uint32_t) actor.expFromMob)
            {
                actor.expItem = static_cast<uint64_t>(actor.expItem) + exp - actor.expFromItem;
                actor.exp = 0;
                ecs::PlayerRuntime::SetExp(charEntity, 0);
                actor.levelStep = 0;
                SetActorLevel(owner, actor, actor.level + 1);
                SendPetLevelUpEffect(charEntity, 1, actor.level, 1);
#ifndef ENABLE_NEW_PET_EDITS
                IncreasePetBonus(owner, actor);
#endif
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
                return;
            } else  {
                exp = actor.expFromItem - actor.expItem;
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
            }
        }

        actor.expItem += exp;
        ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %d %d %d", actor.exp, actor.expItem, NextExp(actor));
    }

}

void SetActorItemCube(entt::entity owner, NewPetRecord& actor, int pos, int invpos)
{
    // Client slot bounds must be checked BEFORE touching the nine-slot array.
    if (pos < 0 || pos >= static_cast<int>(actor.feedItems.size()) || invpos < 0
        || invpos >= INVENTORY_MAX_NUM || !NewPetSystem::HasValidSummon(owner, actor) || IsTradeWindowOpen(owner))
        return;
    const auto item = ItemSystem::GetInventoryItem(owner, invpos);
    if (item == actor.summonItem || !CanUseMaterial(owner, item))
        return;
    for (const auto& selection : actor.feedItems)
        if (selection.item == item)
            return;
    if (actor.feedItems[pos].item == entt::null)
        actor.feedItems[pos] = { item, invpos };
}

void ActorItemCubeFeed(entt::entity owner, NewPetRecord& actor, int type)
{
    // Detach selections first, including rejected feeds, so stale selections
    // cannot later consume a replacement item in the same inventory cell.
    const auto selections = std::exchange(actor.feedItems, {});
    if (!NewPetSystem::HasValidSummon(owner, actor) || IsTradeWindowOpen(owner) || (type != 1 && type != 3))
        return;
    for (const auto& selection : selections)
    {
        const auto item = selection.item;
        if (!NewPetSystem::HasValidSummon(owner, actor))
            return;
        if (!CanUseMaterial(owner, item) || item == actor.summonItem
            || selection.cell < 0 || ItemSystem::GetInventoryItem(owner, selection.cell) != item)
            continue;
        if (type == 1)
        {
            const auto vnum = ItemSystem::GetItemVnum(item);
            if (!((vnum >= 55401 && vnum <= 55411) || (vnum >= 55701 && vnum <= 55711) || vnum == 55001))
                continue;
            const uint64_t added = vnum == 55001 ? actor.totalDuration / 2 : static_cast<uint64_t>(actor.totalDuration) * 3 / 100;
            if (!ItemSystem::DestroyItemEntityEcs(item, "PET_CUBE_FEED"))
                continue;
            actor.duration = static_cast<uint32_t>(std::min<uint64_t>(actor.totalDuration, actor.duration + added));
            ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetDuration %u %u", actor.duration, actor.totalDuration);
        }
        else if (actor.level < 120 && (ItemSystem::GetItemType(item) == ITEM_WEAPON || ItemSystem::GetItemType(item) == ITEM_ARMOR))
        {
            const auto exp = ItemSystem::GetItemShopBuyPrice(item) / 2;
            if (exp <= 0 || exp > UINT32_MAX)
                continue;
            if (ItemSystem::DestroyItemEntityEcs(item, "PET_CUBE_FEED"))
                SetActorExp(owner, actor, exp, 1);
        }
    }
}

#ifdef ENABLE_NEW_PET_EDITS
bool ActorIncreasePetSkill(entt::entity owner, NewPetRecord& actor, int iSlot, int iType)
#else
bool ActorIncreasePetSkill(entt::entity owner, NewPetRecord& actor, int skill)
#endif
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || IsTradeWindowOpen(owner))
        return false;
#ifdef ENABLE_NEW_PET_EDITS
    if (iSlot < 0 || iSlot >= 4 || iType < 0 || iType > UINT16_MAX)
        return false;
    int idx = actor.skillSlot[iSlot];
    if (idx == -1)
        return false;

    if (actor.skill[iSlot] >= 10) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 58, "");
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
    const entt::entity bookItem = ItemSystem::GetItem(owner, Cell);
    if (!CanUseMaterial(owner, bookItem))
        return false;

    iType = ItemSystem::GetItemValue(bookItem, 0);
    if ((iType > 12) || (iType < 1) || ((idx != 0) && (idx != iType)) || (ItemSystem::GetItemType(bookItem) != ITEM_TYPE_PET))
        return false;

    for (int i = 0; i < 4; i++) {
        if ((iType == actor.skillSlot[i]) && (iSlot != i)) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 55, "");
#endif
            return false;
        }
    }

    if (!ItemSystem::ConsumeItemEcs(bookItem, 1))
        return false;

    if (idx == 0)
        actor.skillSlot[iSlot] = iType;

    actor.skill[iSlot] += 1;

#ifdef TEXTS_IMPROVEMENT
    if (actor.skill[iSlot] == 1) {
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 57, "");
    }
    else {
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 56, "%d", actor.skill[iSlot]);
    }
#endif
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", iSlot, actor.skillSlot[iSlot], actor.skill[iSlot]);

    ClearBuff(owner);
    GiveBuff(owner, actor);

    return true;
#else
    if (skill < 1 || skill > std::size(Pet_Skill_Table) || (actor.level < 80 && actor.evolution < 3))
        return false;
    for (int i = 0; i < 4; ++i)
    { //Itero gli slot per cercare la skill
        if (actor.skillSlot[i] == skill)
        {  //Se trova la skill o la aumenta oppure e' gi?max
            if (actor.skill[i] < 20)
            {
                actor.skill[i] += 1;
#ifdef TEXTS_IMPROVEMENT
                ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 743, "%d", actor.skill[i]);
#endif
                ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, actor.skillSlot[i], actor.skill[i]);
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
        if (actor.skillSlot[i] == 0)
        { //Controllo se trovo uno slot vuoto abilitato
            actor.skillSlot[i] = skill;
            actor.skill[i] = 1;
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 745, "");
#endif
            ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, actor.skillSlot[i], actor.skill[i]);
            return true;
        }
    }

    /* Qualora il pet non soddisfi le condizioni precedenti
       Allora tutti gli slot sono pieni e quind non pu?
       imparare nuove skill
    */
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 745, "");
#endif
    return false;
#endif
}

#ifdef ENABLE_NEW_PET_EDITS
bool ActorIncreasePetSkillByBook(entt::entity owner, NewPetRecord& actor, entt::entity bookItem)
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || IsTradeWindowOpen(owner) || !CanUseMaterial(owner, bookItem))
        return false;

    if (ItemSystem::GetItemType(bookItem) != ITEM_TYPE_PET)
        return false;

    int iType = ItemSystem::GetItemValue(bookItem, 0);
    if ((iType > 12) || (iType < 1))
        return false;

    int ret = 0;
    bool bContinue = false;
    for (int i = 0; i < 4; i++) {
        if (iType == actor.skillSlot[i]) {
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

    if (actor.skill[ret] >= 10) {
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

    actor.skill[ret] += 1;

#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 56, "%d", actor.skill[ret]);
#endif
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", ret, actor.skillSlot[ret], actor.skill[ret]);

    ClearBuff(owner);
    GiveBuff(owner, actor);

    ecs::QuestSystem::SetFlag(owner, szName, get_global_time() + (3600 * 3));

    return true;
}

int ActorResetSkills(entt::entity owner, NewPetRecord& actor)
{
    if (!NewPetSystem::HasValidSummon(owner, actor)) return 3;
    bool bContinue = false;
    for (int i = 0; i < 4; i++) {
        if (actor.skillSlot[i] > 0) {
            bContinue = true;
            break;
        }
    }

    if (!bContinue)
        return 3;

    for (int i = 0; i < 4; i++) {
        if (actor.skillSlot[i] > 0)
            actor.skillSlot[i] = 0;

        actor.skill[i] = 0;
        ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, actor.skillSlot[i], actor.skill[i]);
    }

    ClearBuff(owner);
    GiveBuff(owner, actor);

    return 1;
}

int ActorResetSkill(entt::entity owner, NewPetRecord& actor, int iType)
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || iType < 1 || iType > std::size(Pet_Skill_Table)) return 3;
    int ret = 0;
    bool bContinue = false;
    for (int i = 0; i < 4; i++) {
        if (actor.skillSlot[i] == iType) {
            ret = i;
            bContinue = true;
            break;
        }
    }

    if (!bContinue)
        return 3;

    actor.skillSlot[ret] = 0;
    actor.skill[ret] = 0;
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", ret, actor.skillSlot[ret], actor.skill[ret]);

    ClearBuff(owner);
    GiveBuff(owner, actor);

    return 1;
}
#endif

bool ActorIncreasePetEvolution(entt::entity owner, NewPetRecord& actor)
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || actor.evolution < 0 || actor.evolution >= 3
        || actor.level < static_cast<uint32_t>(40 + actor.evolution * 20))
        return false;
    const uint32_t vnum = actor.vnum;
    ++actor.evolution;
#ifdef ENABLE_NEW_PET_EDITS
    SetActorLevel(owner, actor, actor.level + 1);
    IncreasePetBonus(owner, actor);
    actor.levelStep = actor.exp = actor.expItem = 0;
    ecs::PlayerRuntime::SetExp(actor.character, 0);
#endif
    const int slot = actor.evolution - 1;
    actor.skillSlot[slot] = actor.skill[slot] = 0;
    SendPetLevelUpEffect(actor.character, 1, actor.level, 1);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetEvolution %d", actor.evolution);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", slot, 0, 0);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %u %u %u", actor.exp, actor.expItem, NextExp(actor));
    if (actor.evolution == 3)
    {
        const auto item = actor.summonItem;
        NewPetSystem::Unsummon(owner, vnum);
        return NewPetSystem::Summon(owner, vnum, item, "", false) != nullptr;
    }
    return true;
}

bool Follow(entt::entity owner, NewPetRecord& actor, float minDistance)
{
    if (!ecs::PlayerRuntime::IsValid(owner) || !NewPetSystem::IsSummoned(actor))
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(owner);
    const auto charX = ecs::PlayerRuntime::GetX(actor.character);
    const auto charY = ecs::PlayerRuntime::GetY(actor.character);
    const float distance = DISTANCE_SQRT(ownerX - charX, ownerY - charY);
    if (distance <= minDistance)
        return false;
    ecs::MovementSystem::SetRotation(actor.character, GetDegreeFromPositionXY(charX, charY, ownerX, ownerY));
    float dx, dy;
    GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(actor.character), distance - minDistance, &dx, &dy);
    if (!ecs::MovementSystem::Goto(actor.character, static_cast<int>(charX + dx + 0.5f), static_cast<int>(charY + dy + 0.5f)))
        return false;
    ecs::MovementSystem::SendMovePacket(actor.character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool UpdateFollowAI(entt::entity owner, NewPetRecord& actor)
{
    if (!NewPetSystem::IsSummoned(actor) || !ecs::PlayerRuntime::IsValid(owner)
        || !ecs::PlayerRuntime::GetMobTable(actor.character))
        return false;
    const auto ownerX = ecs::PlayerRuntime::GetX(owner);
    const auto ownerY = ecs::PlayerRuntime::GetY(owner);
    const auto charX = ecs::PlayerRuntime::GetX(actor.character);
    const auto charY = ecs::PlayerRuntime::GetY(actor.character);
    const float distance = DISTANCE_APPROX(charX - ownerX, charY - ownerY);
    constexpr int approach = 200;
    if (distance >= 4500.f || ecs::PlayerRuntime::GetMapIndex(actor.character) != ecs::PlayerRuntime::GetMapIndex(owner))
    {
        const float rotation = ecs::PlayerRuntime::GetRotation(owner) * 3.141592f / 180.f;
        return SnapFollowerToOwner(actor.character, owner, ownerX - approach * cos(rotation),
            ownerY - approach * sin(rotation), ecs::PlayerRuntime::GetZ(owner));
    }
    if (distance >= 300.f)
    {
        ecs::MovementSystem::SetNowWalking(actor.character, distance < 900.f);
        Follow(owner, actor, approach);
        CombatSystem::SetLastAttacked(actor.character, get_dword_time());
    }
    else
        ecs::MovementSystem::SendMovePacket(actor.character, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

bool UpdateActor(entt::entity owner, NewPetRecord& actor, uint32_t deltaTime)
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || CombatSystem::IsDead(actor.character)
        || actor.duration == 0)
    {
        NewPetSystem::Unsummon(owner, actor.vnum);
        return true;
    }
#ifndef ENABLE_NEW_PET_EDITS
    if (actor.slotImm >= 0 && actor.slotImm < 4)
    {
        auto& state = g_registry.get_or_emplace<ecs::NewPetSkillState>(owner);
        const int row = actor.skillSlot[actor.slotImm] - 1;
        const int column = 2 + actor.skill[actor.slotImm];
        if (state.immortalSource == actor.character && row >= 0 && row < std::size(Pet_Skill_Table)
            && column >= 2 && column < std::size(Pet_Skill_Table[0])
            && Pet_Skill_Table[row][column] <= (get_global_time() - actor.immTime) * 10)
            state.immortalSource = entt::null;
    }
#endif
    return !NewPetSystem::HasOption(actor, NewPetSystem::EPetOption_Followable) || UpdateFollowAI(owner, actor);
}

void UpdateActorTime(entt::entity owner, NewPetRecord& actor, bool now)
{
    if (!NewPetSystem::HasValidSummon(owner, actor))
    {
        NewPetSystem::Unsummon(owner, actor.vnum);
        return;
    }
    if (!now && ++actor.timePet < 60)
        return;
    actor.timePet = 0;
#ifdef ENABLE_NEW_PET_EDITS
    actor.minAge = static_cast<int32_t>(std::clamp<int64_t>(static_cast<int64_t>(get_global_time()) - actor.minAgeStart, 0, INT_MAX));
    if (actor.minAge >= 1296000 && actor.skillSlot[3] == -1)
    {
        actor.skill[3] = actor.skillSlot[3] = 0;
        ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", 3, 0, 0);
    }
    const uint8_t ageTier = actor.minAge >= 4147200 ? 4 : actor.minAge >= 2246400 ? 3 : actor.minAge >= 950400 ? 2 : actor.minAge >= 86400 ? 1 : 0;
    if (actor.ageTier != ageTier || now)
    {
        actor.ageTier = ageTier;
        ClearBuff(owner);
        GiveBuff(owner, actor);
    }
#endif
    // Initialization/skin refresh must not consume a minute; zero must never
    // underflow into an effectively unlimited lifetime.
    if (!now && actor.totalDuration <= 525600 && actor.duration > 0)
        --actor.duration;
#ifdef ENABLE_NEW_PET_EDITS
    ItemSystem::SetItemSocket(actor.summonItem, 1, actor.duration);
#else
    ItemSystem::SetItemForceAttributeEcs(actor.summonItem, 3, 1, actor.duration);
    ItemSystem::SetItemForceAttributeEcs(actor.summonItem, 4, 1, actor.totalDuration);
#endif
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetDuration %u %u", actor.duration, actor.totalDuration);
    if (actor.duration == 0)
        NewPetSystem::Unsummon(owner, actor.vnum);
}

void ActorDoPetSkill(entt::entity owner, NewPetRecord& actor, int skillslot)
{
    if (!NewPetSystem::HasValidSummon(owner, actor) || skillslot < 0 || skillslot >= 4 || actor.skillSlot[skillslot] < 1
        || actor.skillSlot[skillslot] > std::size(Pet_Skill_Table) || actor.skill[skillslot] < 1
        || 2 + actor.skill[skillslot] >= std::size(Pet_Skill_Table[0])) return;
#ifdef ENABLE_NEW_PET_EDITS
    return;
#else
    if (actor.level < 80 || actor.evolution < 3)
        return;
    switch (actor.skillSlot[skillslot])
    {
    case 10:
    {
        if (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[0] <= 480) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 749, "%d", (480 - (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[0])));
#endif
            return;
        }
        if (ecs::PlayerRuntime::GetHPPct(owner) > 20) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 750, "");
#endif
            return;
        }
        g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[0] = get_global_time();
        int riphp = MIN(ecs::PlayerRuntime::GetHP(owner) + (int)Pet_Skill_Table[9][2 + actor.skill[skillslot]], ecs::PointSystem::GetMaxHP(owner));
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 751, "");
#endif
        ecs::PointSystem::Change(owner, POINT_HP, riphp);
        NetworkSyncSystem::BroadcastEffect(g_registry, owner, SE_HPUP_RED);
    }
    break;

    case 17:
    {
        if (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[1] <= 600) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 749, "%d", (600 - (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[1])));
#endif
            return;
        }
        g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[1] = get_global_time();
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 752, "");
#endif
        g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).immortalSource = actor.character;
        actor.slotImm = skillslot;
        actor.immTime = get_global_time();
    }
    break;
    case 18:
    {
        if (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[2] <= 480) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 749, "%d", (480 - (get_global_time() - g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[2])));
#endif
            return;
        }
        g_registry.get_or_emplace<ecs::NewPetSkillState>(owner).cooldowns[2] = get_global_time();
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 753, "");
#endif
        AffectSystem::RemoveBadAffects(owner);
    }
    break;

    default:
        return;
    }
#endif
}
} // namespace

EVENTFUNC(newpetsystem_update_event)
{
    const auto* info = dynamic_cast<newpetsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    const auto owner = info->owner;
    const auto* runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    // A cancelled callback cannot enter a replacement runtime on the same owner.
    if (!runtime || runtime->updateEvent != event)
        return 0;
    NewPetSystem::Update(owner, 0);
    return PASSES_PER_SEC(1) / 4;
}

EVENTFUNC(newpetsystem_expire_event)
{
    const auto* info = dynamic_cast<newpetsystem_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    const auto owner = info->owner;
    const auto* runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime || runtime->expireEvent != event)
        return 0;
    NewPetSystem::UpdateTime(owner);
    return PASSES_PER_SEC(1);
}

namespace NewPetSystem {

bool HasOption(const ecs::NewPetActorState& actor, uint32_t option)
{
    return (actor.options & option) != 0;
}

bool IsSummoned(const ecs::NewPetActorState& actor)
{
    return ecs::PlayerRuntime::IsValid(actor.character);
}

bool HasValidSummon(entt::entity owner, const ecs::NewPetActorState& actor)
{
    return IsSummoned(actor) && OwnedItem(owner, actor.summonItem);
}

ecs::NewPetActorState* FindActor(entt::entity owner, uint32_t vnum)
{
    if (owner == entt::null || !g_registry.valid(owner) || vnum == 0)
        return nullptr;
    auto* runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return nullptr;
    for (auto& record : runtime->actors)
        if (record.vnum == vnum)
            return &record;
    return nullptr;
}

ecs::NewPetActorState* FindActorByVID(entt::entity owner, uint32_t vid)
{
    if (owner == entt::null || !g_registry.valid(owner) || vid == 0)
        return nullptr;
    auto* runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return nullptr;
    for (auto& record : runtime->actors)
        if (IsSummoned(record) && record.vid == vid)
            return &record;
    return nullptr;
}

bool IsActivePet(entt::entity owner)
{
    return FindSummonedRecord(owner) != nullptr;
}

size_t CountSummoned(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return 0;
    const auto* runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return 0;
    return std::count_if(runtime->actors.begin(), runtime->actors.end(),
        [](const auto& record) { return g_registry.valid(record.character); });
}

ecs::NewPetActorState* Summon(entt::entity owner, uint32_t vnum, entt::entity item,
    const char* petName, bool spawnFar, uint32_t options)
{
    if (!OwnedItem(owner, item))
        return nullptr;
    auto* runtime = Runtime(owner);
    if (!runtime || runtime->destroying)
        return nullptr;
    for (const auto& record : runtime->actors)
        if (record.vnum != 0 && record.vnum != vnum && record.summonItem == item)
            return nullptr;

    auto* actor = FindLiveRecord(*runtime, vnum);
    if (!actor)
    {
        actor = FreeRecordSlot(*runtime);
        if (!actor)
        {
            runtime->actors.push_back({});
            actor = &runtime->actors.back();
        }
        *actor = {};
        actor->vnum = vnum;
        actor->options = options;
    }

    int32_t x = ecs::PlayerRuntime::GetX(owner);
    int32_t y = ecs::PlayerRuntime::GetY(owner);
    const auto z = ecs::PlayerRuntime::GetZ(owner);
    x += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    y += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);

    if (IsSummoned(*actor))
    {
        if (item != actor->summonItem || !SnapFollowerToOwner(actor->character, owner, x, y, z))
            return nullptr;
        return actor;
    }

    // A stale record may still carry progression from a previous summon.
    TeardownActor(owner, *actor, true);
    runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    actor = runtime ? FindLiveRecord(*runtime, vnum) : nullptr;
    if (!actor)
        return nullptr;

    const auto seal = ItemSystem::GetItemVnum(item);
    if (seal < 55701 || seal > 55711)
        return nullptr;
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
        return nullptr;
    const auto row = mysql_fetch_row(table->pSQLResult);
    if (!row || !row[0])
        return nullptr;
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
            return nullptr;
    }
    if (!exppet_table || exppet_table[values[1]] == 0 || exppet_table[values[1]] > INT_MAX
        || values[15] == 0 || values[15] > values[16])
        return nullptr;
#ifdef ENABLE_NEW_PET_EDITS
    if (values[18] && ItemSystem::GetItemSocket(item, 1) == 0)
        return nullptr;
#endif
    static constexpr uint32_t races[11][2] = {
        {34041,34042}, {34045,34046}, {34049,34050}, {34053,34054}, {34036,34037},
        {34064,34065}, {34073,34074}, {34075,34076}, {34080,34081}, {34082,34083}, {34095,34096}
    };
    const auto skin = PetSkin(owner);
    const auto race = skin ? skin : races[seal - 55701][values[17] == 3];
    actor->character = CHARACTER_MANAGER::instance().SpawnMobEntity(race, ecs::PlayerRuntime::GetMapIndex(owner),
        x, y, z, false, static_cast<int>(ecs::PlayerRuntime::GetRotation(owner) + 180), false);
    if (!IsSummoned(*actor))
        return nullptr;
    g_registry.get_or_emplace<ecs::StatusFlags>(actor->character).isNewPet = true;
    ecs::PlayerRuntime::SetEmpire(actor->character, ecs::PlayerRuntime::GetEmpire(owner));
    actor->vid = ecs::PlayerRuntime::GetPacketVID(actor->character);
    SetActorName(owner, *actor, *row[0] ? row[0] : petName);
    actor->evolution = static_cast<int>(values[17]);
    actor->level = static_cast<uint32_t>(values[1]);
    ecs::PlayerRuntime::SetLevel(actor->character, static_cast<uint8_t>(actor->level));
    SetNextExp(*actor, NextExp(*actor));
    // Loading is not earning EXP: do not run level-up/evolution transitions here.
    actor->exp = static_cast<uint32_t>(std::min<int64_t>(values[2], actor->expFromMob));
    actor->expItem = static_cast<int>(std::min<int64_t>(values[3], actor->expFromItem));
    ecs::PlayerRuntime::SetExp(actor->character, actor->exp);
    for (int i = 0; i < 3; ++i)
        actor->bonusPet[i][1] = static_cast<int>(values[4 + i]);
    for (int i = 0; i < 4; ++i)
    {
        actor->skillSlot[i] = static_cast<int>(values[7 + i * 2]);
        actor->skill[i] = static_cast<int>(values[8 + i * 2]);
    }
    actor->duration = static_cast<uint32_t>(values[15]);
    actor->totalDuration = static_cast<uint32_t>(values[16]);
#ifdef ENABLE_NEW_PET_EDITS
    actor->minAgeStart = static_cast<uint32_t>(values[19]);
    actor->minAge = 0;
    actor->ageTier = 0;
#endif
    // The initial insert packet reads creature-side level before the seal is
    // committed to this actor. Failed Show still has no item to save/unlock.
    g_registry.emplace_or_replace<ecs::GrowthPetComponent>(actor->character, owner, item, actor->level);
    if (!ecs::MovementSystem::Show(actor->character, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z))
    {
        Unsummon(owner, vnum);
        return nullptr;
    }
    if (values[18] == 0)
    {
        std::unique_ptr<SQLMsg> mark(DBManager::instance().DirectQuery(
            "UPDATE new_petsystem SET evocation=1 WHERE id=%u", ItemSystem::GetItemID(item)));
        if (!mark || mark->uiSQLErrno)
        {
            Unsummon(owner, vnum);
            return nullptr;
        }
    }
    SetSummonItem(owner, *actor, item);
    ItemSystem::LockItem(item);
    ItemSystem::SetItemSocket(item, 0, 1);
    for (int i = 0; i < 3; ++i)
        ItemSystem::SetItemForceAttributeEcs(item, i, 1, actor->bonusPet[i][1]);
#ifdef ENABLE_NEW_PET_EDITS
    ItemSystem::SetItemForceAttributeEcs(item, 3, 1, actor->level);
    ItemSystem::SetItemSocket(item, 1, actor->duration);
    ItemSystem::SetItemSocket(item, 2, actor->totalDuration);
#else
    ItemSystem::SetItemForceAttributeEcs(item, 3, 1, actor->duration);
    ItemSystem::SetItemForceAttributeEcs(item, 4, 1, actor->totalDuration);
    ItemSystem::SetItemSocket(item, 1, actor->level);
#endif
    ecs::PointSystem::Compute(owner);
    runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    actor = runtime ? FindLiveRecord(*runtime, vnum) : nullptr;
    if (!actor || !IsSummoned(*actor))
        return nullptr;
    UpdateActorTime(owner, *actor, true);
    runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    actor = runtime ? FindLiveRecord(*runtime, vnum) : nullptr;
    if (!actor || !IsSummoned(*actor))
        return nullptr;
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetIcon %u", actor->summonItemVnum);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetEvolution %d", actor->evolution);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetName %s", actor->name.c_str());
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetLevel %u", actor->level);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetBonus %d %d %d", actor->bonusPet[0][1], actor->bonusPet[1][1], actor->bonusPet[2][1]);
#ifdef ENABLE_NEW_PET_EDITS
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetAge %u", actor->minAgeStart);
#endif
    for (int i = 0; i < 4; ++i)
    {
        int slot = actor->skillSlot[i];
#ifndef ENABLE_NEW_PET_EDITS
        if (actor->level < 80 || actor->evolution != 3)
            slot = -1;
#endif
        ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetSkill %d %d %d", i, slot, actor->skill[i]);
    }
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetExp %u %u %u", actor->exp, actor->expItem, NextExp(*actor));
#ifdef ENABLE_RECALL
    AffectSystem::RemoveAffect(owner, AFFECT_RECALL2);
    AffectSystem::AddAffect(owner, AFFECT_RECALL2, APPLY_NONE, 0, ItemSystem::GetItemID(item),
        INFINITE_AFFECT_DURATION, 0, true, false);
#endif
    runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    actor = runtime ? FindLiveRecord(*runtime, vnum) : nullptr;
    if (!runtime || !actor)
        return nullptr;
    if (!runtime->updateEvent)
    {
        auto* info = AllocEventInfo<newpetsystem_event_info>();
        info->owner = owner;
        runtime->updateEvent = event_create(newpetsystem_update_event, info, PASSES_PER_SEC(1) / 4);
    }
    if (!runtime->expireEvent)
    {
        auto* info = AllocEventInfo<newpetsystem_event_info>();
        info->owner = owner;
        runtime->expireEvent = event_create(newpetsystem_expire_event, info, PASSES_PER_SEC(1));
    }
    return actor;
}

void Unsummon(entt::entity owner, uint32_t vnum, bool deleteFromList)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return;
    auto* actor = FindLiveRecord(*runtime, vnum);
    if (!actor)
        return;
    TeardownActor(owner, *actor, !deleteFromList);
    runtime = g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (runtime && CountSummoned(owner) == 0)
    {
        event_cancel(&runtime->updateEvent);
        event_cancel(&runtime->expireEvent);
    }
}

void UnsummonAll(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return;
    event_cancel(&runtime->updateEvent);
    event_cancel(&runtime->expireEvent);
#ifdef ENABLE_RECALL
    if (ecs::PlayerRuntime::IsValid(owner))
        AffectSystem::RemoveAffect(owner, AFFECT_RECALL2);
#endif
    for (auto& actor : runtime->actors)
        if (actor.vnum != 0)
            TeardownActor(owner, actor, true);
}

void DeletePet(entt::entity owner, uint32_t vnum)
{
    Unsummon(owner, vnum, true);
}

void DestroyRuntime(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime || runtime->destroying)
        return;
    runtime->destroying = true;
    event_cancel(&runtime->updateEvent);
    event_cancel(&runtime->expireEvent);
    // Teardown runs with destroying set: ComputePoints -> RefreshBuff must see
    // only living actors, never one whose teardown is in progress.
    for (auto& actor : runtime->actors)
        if (actor.vnum != 0)
            TeardownActor(owner, actor, true);
    runtime->actors.clear();
    if (g_registry.valid(owner))
        g_registry.remove<ecs::NewPetRuntime>(owner);
}

bool Update(entt::entity owner, uint32_t deltaTime)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return true;
    const uint32_t now = get_dword_time();
    if (runtime->updatePeriod > now - runtime->lastUpdateTime)
        return true;
    bool result = true;
    std::vector<uint32_t> vnums;
    vnums.reserve(runtime->actors.size());
    for (const auto& actor : runtime->actors)
        if (actor.character != entt::null || actor.summonItem != entt::null)
            vnums.push_back(actor.vnum);
    for (const uint32_t vnum : vnums)
    {
        auto* current = g_registry.try_get<ecs::NewPetRuntime>(owner);
        auto* actor = current ? FindLiveRecord(*current, vnum) : nullptr;
        if (!actor)
            continue;
        result = UpdateActor(owner, *actor, deltaTime) && result;
    }
    auto* current = g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (current)
    {
        current->lastUpdateTime = now;
        if (CountSummoned(owner) == 0)
        {
            event_cancel(&current->updateEvent);
            event_cancel(&current->expireEvent);
        }
    }
    return result;
}

void UpdateTime(entt::entity owner, bool now)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return;
    std::vector<uint32_t> vnums;
    vnums.reserve(runtime->actors.size());
    for (const auto& actor : runtime->actors)
        if (IsSummoned(actor))
            vnums.push_back(actor.vnum);
    for (const uint32_t vnum : vnums)
    {
        auto* current = g_registry.try_get<ecs::NewPetRuntime>(owner);
        auto* actor = current ? FindLiveRecord(*current, vnum) : nullptr;
        if (!actor || !IsSummoned(*actor))
            continue;
        UpdateActorTime(owner, *actor, now);
    }
}

void SetUpdatePeriod(entt::entity owner, uint32_t ms)
{
    if (auto* runtime = Runtime(owner))
        runtime->updatePeriod = ms;
}

void RefreshBuff(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime || runtime->destroying)
        return;
    std::vector<std::pair<uint32_t, entt::entity>> actors;
    for (const auto& actor : runtime->actors)
        actors.push_back({actor.vnum, actor.summonItem});
    for (const auto& [vnum, item] : actors)
    {
        auto* current = g_registry.try_get<ecs::NewPetRuntime>(owner);
        if (!current || current->destroying)
            return;
        auto* actor = FindLiveRecord(*current, vnum);
        if (!actor || !IsSummoned(*actor) || actor->summonItem != item)
            continue;
        ClearBuff(owner);
        GiveBuff(owner, *actor);
    }
}

void UpdatePetSkin(entt::entity owner)
{
    auto* runtime = owner == entt::null || !g_registry.valid(owner)
        ? nullptr : g_registry.try_get<ecs::NewPetRuntime>(owner);
    if (!runtime)
        return;
    std::vector<std::pair<uint32_t, entt::entity>> pending;
    for (const auto& actor : runtime->actors)
        if (actor.vnum != 0 && IsSummoned(actor) && OwnedItem(owner, actor.summonItem))
            pending.push_back({actor.vnum, actor.summonItem});
    for (const auto& [vnum, item] : pending)
    {
        Unsummon(owner, vnum);
        Summon(owner, vnum, item, "Noname", false);
    }
    if (auto* current = g_registry.try_get<ecs::NewPetRuntime>(owner); current && CountSummoned(owner) == 0)
    {
        event_cancel(&current->updateEvent);
        event_cancel(&current->expireEvent);
    }
}

void ChangeName(entt::entity owner, const char* name)
{
    auto* actor = FindSummonedRecord(owner);
    if (!actor || !name || !*name || strlen(name) > CHARACTER_NAME_MAX_LEN)
        return;
    char escaped[CHARACTER_NAME_MAX_LEN * 2 + 1] {};
    DBManager::instance().EscapeString(escaped, sizeof(escaped), name, static_cast<uint32_t>(strlen(name)));
    std::unique_ptr<SQLMsg> result(DBManager::instance().DirectQuery(
        "UPDATE new_petsystem SET name='%s' WHERE id=%u", escaped, actor->summonItemID));
    if (!result || result->uiSQLErrno)
        return;
    SetActorName(owner, *actor, name);
    ecs::ChatSystem::Send(owner, CHAT_TYPE_COMMAND, "PetName %s", actor->name.c_str());
    ecs::ViewSystem::ViewReencode(actor->character);
}

bool Mount(entt::entity owner, uint32_t vnum)
{
    auto* actor = FindActor(owner, vnum);
    if (!actor || !ecs::PlayerRuntime::IsValid(owner) || !HasOption(*actor, EPetOption_Mountable))
        return false;
    const auto skin = PetSkin(owner);
    actor->ridingVnum = skin ? skin : actor->vnum;
    MountSystem::SetMountVnum(owner, actor->ridingVnum);
    return MountSystem::GetMountVnum(owner) == actor->ridingVnum;
}

void Unmount(entt::entity owner, uint32_t vnum)
{
    auto* actor = FindActor(owner, vnum);
    if (!actor)
        return;
    UnmountActor(owner, *actor);
}

bool IncreasePetEvolution(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor && ActorIncreasePetEvolution(owner, *actor);
}

void SetExp(entt::entity owner, int exp, int mode)
{
    if (exp <= 0)
        return;
    auto* actor = FindSummonedRecord(owner);
    if (!actor)
        return;
    SetActorExp(owner, *actor, exp, mode);
}

int GetEvolution(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? actor->evolution : -1;
}

int GetLevel(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? static_cast<int>(actor->level) : -1;
}

int GetExp(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? static_cast<int>(actor->exp) : 0;
}

int GetLevelStep(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? actor->levelStep : 4;
}

#ifdef ENABLE_NEW_PET_EDITS
int GetNextExpFromMob(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? actor->expFromMob : 0;
}

int ResetSkills(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? ActorResetSkills(owner, *actor) : 2;
}

int ResetSkill(entt::entity owner, int type)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? ActorResetSkill(owner, *actor, type) : 2;
}

bool IncreasePetSkill(entt::entity owner, int slot, int type)
{
    auto* actor = FindSummonedRecord(owner);
    return actor && ActorIncreasePetSkill(owner, *actor, slot, type);
}

bool IncreasePetSkillByBook(entt::entity owner, entt::entity bookItem)
{
    auto* actor = FindSummonedRecord(owner);
    return actor && ActorIncreasePetSkillByBook(owner, *actor, bookItem);
}
#else
bool IncreasePetSkill(entt::entity owner, int skill)
{
    auto* actor = FindSummonedRecord(owner);
    return actor && ActorIncreasePetSkill(owner, *actor, skill);
}
#endif

void SetItemCube(entt::entity owner, int pos, int invpos)
{
    auto* actor = FindSummonedRecord(owner);
    if (actor)
        SetActorItemCube(owner, *actor, pos, invpos);
}

void ItemCubeFeed(entt::entity owner, int type)
{
    auto* actor = FindSummonedRecord(owner);
    if (actor)
        ActorItemCubeFeed(owner, *actor, type);
}

void DoPetSkill(entt::entity owner, int skillslot)
{
    auto* actor = FindSummonedRecord(owner);
    if (actor)
        ActorDoPetSkill(owner, *actor, skillslot);
}

uint32_t GetNewPetItemID(entt::entity owner)
{
    auto* actor = FindSummonedRecord(owner);
    return actor ? actor->summonItemID : 0;
}

} // namespace NewPetSystem
