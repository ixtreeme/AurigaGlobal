#include "../../stdafx.h"
#include "ViewSystem.hpp"
#include "PlayerRuntimeSystem.hpp"

#include "MountSystem.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "VisibilitySystem.hpp"

#include "../../config.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../db.h"
#include "../../packet.h"
#include "../../guild.h"
#include "../../vector.h"
#include "../../questmanager.h"
#include "../../item.h"
#include "../../item_manager.h"
#include "../../MountSystem.h"
#include "../../MountInventory.h"
#include "../../mount_inventory_helper.h"
#include "../../horsename_manager.h"
#include "../../locale_service.h"
#include "../../arena.h"
#include "../../desc.h"
#include "../../PetSystem.h"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/social_components.hpp"

#include <common/VnumHelper.h>
#include <utility>
#include <Core/Logging.hpp>
#include "../CharacterAccessors.hpp"
#include "../components/visibility_components.hpp"

namespace
{

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

// Transitional boundary for the two operations whose side effects still live
// in CHARACTER/CMountSystem (network rebroadcast and legacy actor teardown).
// Do not use this for entity state reads.
LegacyCharHandle ResolveLegacyMountOwnerBoundary(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy ? legacy->ptr : nullptr;
}

ecs::MountState* GetMountState(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.get_or_emplace<ecs::MountState>(e);
}

// Was SyncMountState, taking all six fields from CHARACTER members and copying
// them in. Those members are gone - MountState is the only copy, and MountVnum
// writes it directly - so every one of the fourteen calls had become the
// component written back onto itself. Marking it dirty is what they were
// actually still doing.
void MarkMountDirty(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

void SyncHorseRiding(entt::entity e, bool riding)
{
    auto* state = GetMountState(e);
    if (!state)
        return;

    state->horseRiding = riding;
    g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

uint32_t GetMountMobVnum(entt::entity item)
{
    if (!ItemSystem::IsValidItem(item))
        return 0;

#ifdef __CHANGELOOK_SYSTEM__
    const uint32_t transmutationVnum = ItemSystem::GetItemTransmutationVnum(item);
    if (transmutationVnum != 0)
    {
        if (const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(transmutationVnum))
            return itemTable->alValues[1];
    }
#endif

    return ItemSystem::GetItemValue(item, 1);
}

} // namespace

namespace MountSystem {

} // namespace MountSystem

CMountInventory* CHARACTER::GetMountInventory() const
{
    return MountSystem::GetMountInventory(GetEntityHandle());
}

void CHARACTER::QueryMountInventory()
{
    if (m_bMountInventoryLoaded || !GetDesc())
        return;

    DBManager::instance().ReturnQuery(QID_MOUNT_INVENTORY_LOAD,
        GetPlayerID(),
        nullptr,
        "SELECT id, slot, vnum, count, socket0, socket1, socket2, "
        "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, "
        "attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5 "
        "FROM account_mount_inventory WHERE account_id=%u ORDER BY slot",
        GetDesc()->GetAccountTable().id);
}

void CHARACTER::LoadMountInventory(const std::vector<TMountInventoryItemTable>& items)
{
    if (m_bMountInventoryLoaded)
        return;

    const int iHeight = 16;
    MountSystem::SetMountInventory(GetEntityHandle(), M2_NEW CMountInventory(GetEntityHandle(), iHeight));

    for (const auto& entry : items)
    {
        const entt::entity item = ITEM_MANAGER::instance().CreateItem(
            entry.vnum, entry.count, entry.id);
        if (!ItemSystem::IsValidItem(item))
            continue;

        ItemSystem::SetItemSkipSave(item, true);
        for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
            ItemSystem::SetItemSocketEcs(item, socket, entry.alSockets[socket]);
        for (int attribute = 0; attribute < ITEM_ATTRIBUTE_MAX_NUM; ++attribute)
            ItemSystem::SetItemForceAttributeEcs(
                item, attribute, entry.aAttr[attribute].bType,
                entry.aAttr[attribute].sValue);

        if (!MountSystem::GetMountInventory(GetEntityHandle())->Add(entry.slot, item, true))
            ItemSystem::DestroyItemEntityEcs(item, "MOUNT_INVENTORY_LOAD_ADD_FAILED");
    }

    m_bMountInventoryLoaded = true;
    SendMountInventory();
    ComputePoints();
}

void CHARACTER::SendMountInventory()
{
    if (!GetDesc() || !MountSystem::GetMountInventory(GetEntityHandle()))
        return;

    std::vector<TMountInventoryItemTable> items;
    MountSystem::GetMountInventory(GetEntityHandle())->CollectItems(items);

    TPacketGCMountInventory header{};
    header.bHeader = HEADER_GC_MOUNT_INVENTORY;
    header.size = sizeof(TPacketGCMountInventory) + static_cast<uint16_t>(items.size() * sizeof(TMountInventoryItemData));
    header.bWidth = MountSystem::GetMountInventory(GetEntityHandle())->GetWidth();
    header.bHeight = MountSystem::GetMountInventory(GetEntityHandle())->GetSize();
    header.wCount = static_cast<uint16_t>(items.size());

    TEMP_BUFFER buf;
    buf.write(&header, sizeof(header));

    for (const auto& entry : items)
    {
        TMountInventoryItemData data{};
        data.wSlot = entry.slot;
        data.dwVnum = entry.vnum;
        data.dwCount = entry.count;
        memcpy(data.alSockets, entry.alSockets, sizeof(data.alSockets));
        memcpy(data.aAttr, entry.aAttr, sizeof(data.aAttr));
        buf.write(&data, sizeof(data));
    }

    GetDesc()->Packet(buf.read_peek(), buf.size());
}

int CHARACTER::GetBeltCount() const
{
    return MountSystem::GetBeltCount(GetEntityHandle());
}

int CHARACTER::GetMountCount() const
{
    return MountSystem::GetMountCount(GetEntityHandle());
}

void CHARACTER::UpdateMountCountOverheadToViewers()
{
#ifdef ENABLE_FAKE_SHOP_HEADER
    MountSystem::UpdateMountInventoryCountOverhead(GetEntityHandle(), GetEntityHandle());

    // The ECS ViewMap, not m_map_view: this is a CHARACTER, and for characters
    // the legacy map stopped being maintained when D.6 disabled the polling in
    // UpdateSectree. It is frozen at whatever it held then, so this loop was
    // walking stale contents.
    const entt::entity selfEntity = GetEntityHandle();
    if (const auto* viewMap = g_registry.try_get<ecs::ViewMap>(selfEntity))
    {
        for (const entt::entity viewerEntity : viewMap->visible)
        {
            if (viewerEntity == selfEntity)
                continue;

            if (ecs::PlayerRuntime::IsPC(viewerEntity) && ecs::PlayerRuntime::GetDesc(viewerEntity))
                MountSystem::UpdateMountInventoryCountOverhead(GetEntityHandle(), viewerEntity);
        }
    }
#endif
}

namespace MountSystem {

bool IsRiding(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return false;

    const auto* state = g_registry.try_get<ecs::MountState>(rider);
    return state && (state->horseRiding || state->mountVnum != 0);
}

bool IsSummoned(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character && character->GetHorse();
}

bool IsRidingCostume(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character && character->IsRidingMount();
}

bool IsOwnedHorse(entt::entity rider, entt::entity horse)
{
    auto* legacyHorse = ResolveLegacyMountOwnerBoundary(horse);
    auto* legacyRider = ResolveLegacyMountOwnerBoundary(rider);
    return legacyHorse && legacyHorse->GetRider() == legacyRider;
}

bool StartRiding(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character && character->StartRiding();
}

bool StopRiding(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character && character->StopRiding();
}

void SummonHorse(entt::entity rider, bool summon, bool fromFar,
    uint32_t vnum, const char* name)
{
    if (auto* character = ResolveLegacyMountOwnerBoundary(rider))
        character->HorseSummon(summon, fromFar, vnum, name);
}

uint32_t GetMountVnum(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return 0;

    const auto* state = g_registry.try_get<ecs::MountState>(rider);
    return state ? state->mountVnum : 0;
}

void SetMountVnum(entt::entity rider, uint32_t vnum)
{
    auto* ch = ResolveLegacyMountOwnerBoundary(rider);
    if (ch)
        ch->MountVnum(vnum);
}

} // namespace MountSystem

LPCHARACTER CHARACTER::GetHorse() const
{
	return ecs::LegacyCharOf(MountSystem::GetSummonedHorse(GetEntityHandle()));
}

EVENTFUNC(horse_dead_event);

namespace MountSystem {

void HorseSummon(entt::entity rider, bool bSummon, bool bFromFar = false,
    uint32_t dwVnum = 0, const char* pPetName = nullptr);
static ::CMountSystem* GetMountSystem(entt::entity e);

void MountSummon(entt::entity rider, entt::entity mountItem)
{
#define MOUNT_SYSTEM_FIX_POLY
#ifdef MOUNT_SYSTEM_FIX_POLY
	if (AffectSystem::IsPolymorphed(rider) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 732, "");
#endif
		return;
	}
#endif
	if (ecs::PlayerRuntime::GetMapIndex(rider) == 113)
		return;

	if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(rider)) == true)
		return;

	CMountSystem* mountSystem = GetMountSystem(rider);

	if (!mountSystem || !ItemSystem::IsValidItem(mountItem))
		return;

	const uint32_t mobVnum = GetMountMobVnum(mountItem);

	if (IsHorseRiding(rider))
		StopRiding(rider);

	if (GetSummonedHorse(rider) != entt::null)
		HorseSummon(rider, false);

	mountSystem->Summon(mobVnum, mountItem, false);
}

void HorseSummon(entt::entity rider, bool bSummon, bool bFromFar, uint32_t dwVnum, const char* pPetName)
{
	if ( bSummon )
	{
		if( ecs::LegacyCharOf(GetSummonedHorse(rider)) != nullptr)
			return;

		if (GetHorseLevel(rider) <= 0)
			return;

		if (IsRiding(rider))
			return;

		LOG_INFO("HorseSummon : {} lv:{} bSummon:{} fromFar:{}", ecs::PlayerRuntime::GetName(rider), ecs::PointSystem::GetLevel(rider), bSummon, bFromFar);

		int32_t x = ecs::PlayerRuntime::GetX(rider);
		int32_t y = ecs::PlayerRuntime::GetY(rider);

		if (GetHorseHealth(rider) <= 0)
			bFromFar = false;

		if (bFromFar)
		{
			x += (number(0, 1) * 2 - 1) * number(2000, 2500);
			y += (number(0, 1) * 2 - 1) * number(2000, 2500);
		}
		else
		{
			x += number(-100, 100);
			y += number(-100, 100);
		}

		SetSummonedHorse(rider, CHARACTER_MANAGER::instance().SpawnMob(
				(0 == dwVnum) ? GetMyHorseVnum(rider) : dwVnum,
				ecs::PlayerRuntime::GetMapIndex(rider),
				x, y,
				ecs::PlayerRuntime::GetZ(rider), false, (int)(ecs::PlayerRuntime::GetRotation(rider)+180), false) ? CHARACTER_MANAGER::instance().SpawnMob(
				(0 == dwVnum) ? GetMyHorseVnum(rider) : dwVnum,
				ecs::PlayerRuntime::GetMapIndex(rider),
				x, y,
				ecs::PlayerRuntime::GetZ(rider), false, (int)(ecs::PlayerRuntime::GetRotation(rider)+180), false)->GetEntityHandle() : entt::null);

		if (!ecs::LegacyCharOf(GetSummonedHorse(rider)))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 328, "");
#endif
			return;
		}

		if (GetHorseHealth(rider) <= 0)
		{
			ecs::LegacyCharOf(GetSummonedHorse(rider))->SetPosition(POS_DEAD);

			char_event_info* info = AllocEventInfo<char_event_info>();
			info->ch = rider;
			ecs::PlayerRuntime::SetCharEvent(ecs::LegacyCharOf(GetSummonedHorse(rider))->GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Dead,
				event_create(horse_dead_event, info, PASSES_PER_SEC(60)));
		}

		ecs::LegacyCharOf(GetSummonedHorse(rider))->SetLevel(GetHorseLevel(rider));

		const char* pHorseName = CHorseNameManager::instance().GetHorseName(ecs::PlayerRuntime::GetPlayerID(rider));

		if ( pHorseName != nullptr && strlen(pHorseName) != 0 )
		{
			ecs::LegacyCharOf(GetSummonedHorse(rider))->SetName(pHorseName);
		}
		else
		{
			uint8_t bLang = 0;
			if (ecs::PlayerRuntime::GetDesc(rider)) {
				bLang = ecs::PlayerRuntime::GetDesc(rider)->GetLanguage(); 
			}
			
			ecs::LegacyCharOf(GetSummonedHorse(rider))->SetName(std::string(ecs::PlayerRuntime::GetName(rider)));
			ecs::LegacyCharOf(GetSummonedHorse(rider))->SetName(
				std::string(ecs::LegacyCharOf(GetSummonedHorse(rider))->GetName()) + " ");
			ecs::LegacyCharOf(GetSummonedHorse(rider))->SetName(
				std::string(ecs::LegacyCharOf(GetSummonedHorse(rider))->GetName()) + m_horseText[bLang]);
		}

		if (!ecs::LegacyCharOf(GetSummonedHorse(rider))->Show(ecs::PlayerRuntime::GetMapIndex(rider), x, y, ecs::PlayerRuntime::GetZ(rider)))
		{
			M2_DESTROY_CHARACTER(ecs::LegacyCharOf(GetSummonedHorse(rider)));
			LOG_ERROR("cannot show monster");
			SetSummonedHorse(rider, entt::null);
			return;
		}

		if ((GetHorseHealth(rider) <= 0))
		{
			TPacketGCDead pack;
			pack.header	= HEADER_GC_DEAD;
			pack.vid    = ecs::PlayerRuntime::GetPacketVID(ecs::LegacyCharOf(GetSummonedHorse(rider))->GetEntityHandle());
			ecs::ViewSystem::PacketView(rider, &pack, sizeof(pack));
		}

		ecs::LegacyCharOf(GetSummonedHorse(rider))->SetRider(rider);
	}
	else
	{
		if (!ecs::LegacyCharOf(GetSummonedHorse(rider)))
			return;

		auto* chHorse = ecs::LegacyCharOf(GetSummonedHorse(rider));

		chHorse->SetRider(entt::null);

		if ((GetHorseHealth(rider) <= 0))
			bFromFar = false;

		if (!bFromFar)
		{
			M2_DESTROY_CHARACTER(chHorse);
		}
		else
		{
			chHorse->SetNowWalking(false);
			const entt::entity horseEntity = chHorse->GetEntityHandle();
			float fx, fy;
			chHorse->SetRotation(GetDegreeFromPositionXY(
				ecs::PlayerRuntime::GetX(horseEntity),
				ecs::PlayerRuntime::GetY(horseEntity), ecs::PlayerRuntime::GetX(rider), ecs::PlayerRuntime::GetY(rider)) + 180);
			GetDeltaByDegree(chHorse->GetRotation(), 3500, &fx, &fy);
			chHorse->Goto(
				static_cast<int32_t>(ecs::PlayerRuntime::GetX(horseEntity) + fx),
				static_cast<int32_t>(ecs::PlayerRuntime::GetY(horseEntity) + fy));
			chHorse->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
		}

		SetSummonedHorse(rider, entt::null);
	}

	MarkMountDirty(rider);
}

// The packet-dedup counters and the pulse gate. They were four CHARACTER
// members mirrored into MountState by every SyncMountState call; the component
// is the only copy now, so the mirror argument list goes away with them.
CMountInventory* GetMountInventory(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return nullptr;

    const auto* ref = g_registry.try_get<ecs::MountInventoryRef>(rider);
    return ref ? ref->inventory : nullptr;
}

void SetMountInventory(entt::entity rider, CMountInventory* inventory)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return;

    g_registry.get_or_emplace<ecs::MountInventoryRef>(rider).inventory = inventory;
}

ecs::MountState& GetMountStateRef(entt::entity rider)
{
    static ecs::MountState detached;
    if (rider == entt::null || !g_registry.valid(rider)) {
        detached = ecs::MountState {};
        return detached;
    }

    return g_registry.get_or_emplace<ecs::MountState>(rider);
}

uint32_t GetLastMountTime(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return 0;

    const auto* state = g_registry.try_get<ecs::MountState>(rider);
    return state ? state->mountTime : 0;
}

uint32_t GetMyHorseVnum(entt::entity rider)
{
    int delta = 0;

    if (CGuild* guild = ecs::SocialSystem::GetGuild(rider))
    {
        ++delta;

        if (guild->GetMasterPID() == ecs::PlayerRuntime::GetPlayerID(rider))
            ++delta;
    }

    return c_aHorseStat[GetHorseLevel(rider)].iNPCRace + delta;
}

int GetBeltCount(entt::entity e)
{
    int beltItemCount = 0;
    for (int i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
    {
        if (ItemSystem::GetInventoryItem(e, i) != entt::null)
            ++beltItemCount;
    }

    return beltItemCount;
}

int GetMountCount(entt::entity e)
{
    int mountItemCount = 0;
    if (CMountInventory* mi = GetMountInventory(e))
    {
        const int total = mi->GetWidth() * mi->GetSize();
        for (int pos = 0; pos < total; ++pos)
        {
            if (mi->Get(pos) != entt::null)
                ++mountItemCount;
        }
    }

    return mountItemCount;
}

void UpdateMountInventoryCountOverhead(entt::entity source, entt::entity viewerEntity)
{
    // Both sides must be a PC with a descriptor, as in the legacy method.
    if (!ecs::PlayerRuntime::GetDesc(source))
        return;

    if (!ecs::PlayerRuntime::IsPC(viewerEntity))
        return;

    LPDESC viewerDesc = ecs::PlayerRuntime::GetDesc(viewerEntity);
    if (!viewerDesc)
        return;

    TPacketGCFakeShopSign p;
    p.bHeader = HEADER_GC_FAKE_SHOP_SIGN;
    p.dwVID = ecs::PlayerRuntime::GetPacketVID(source);
    p.iMountCount = GetMountCount(source);
    p.iBeltCount = GetBeltCount(source);

    viewerDesc->Packet(&p, sizeof(p));
}

entt::entity GetSummonedHorse(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return entt::null;

    const auto* summoned = g_registry.try_get<ecs::SummonedHorse>(rider);
    if (!summoned || summoned->horse == entt::null || !g_registry.valid(summoned->horse))
        return entt::null;

    return summoned->horse;
}

void SetSummonedHorse(entt::entity rider, entt::entity horse)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return;

    g_registry.get_or_emplace<ecs::SummonedHorse>(rider).horse = horse;
}

bool IsHorseRiding(entt::entity rider)
{
    // Strictly the riding flag, not IsRiding - that one also answers true for a
    // summoned mount. MountState::horseRiding is written by SyncHorseRiding at
    // both ends of CHARACTER::StartRiding / StopRiding, the only paths that
    // reach CHorseRider's field.
    if (rider == entt::null || !g_registry.valid(rider))
        return false;

    const auto* state = g_registry.try_get<ecs::MountState>(rider);
    return state && state->horseRiding;
}

int GetHorseArmor(entt::entity rider)
{
    // Pure table lookup off the level, exactly as CHorseRider spells it.
    return c_aHorseStat[GetHorseLevel(rider)].iArmor;
}

int GetHorseLevel(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character ? character->GetHorseLevel() : 0;
}

void SetHorseLevel(entt::entity rider, int level)
{
    if (auto* character = ResolveLegacyMountOwnerBoundary(rider))
    {
        character->SetHorseLevel(level);
        character->ComputePoints();
        character->SkillLevelPacket();
    }
}

int GetHorseHealth(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character ? character->GetHorseHealth() : 0;
}

int GetHorseMaxHealth(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character ? character->GetHorseMaxHealth() : 0;
}

int GetHorseStamina(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character ? character->GetHorseStamina() : 0;
}

int GetHorseMaxStamina(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character ? character->GetHorseMaxStamina() : 0;
}

int GetHorseGrade(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character ? character->GetHorseGrade() : 0;
}

bool ReviveHorse(entt::entity rider)
{
    auto* character = ResolveLegacyMountOwnerBoundary(rider);
    return character && character->ReviveHorse();
}

void FeedHorse(entt::entity rider)
{
    if (auto* character = ResolveLegacyMountOwnerBoundary(rider))
        character->FeedHorse();
}

void ForceClearRidingState(entt::entity rider)
{
    if (rider == entt::null || !g_registry.valid(rider))
        return;

    auto* ch = ResolveLegacyMountOwnerBoundary(rider);
    if (ch)
    {
        const uint32_t mountVnum = ch->GetMountVnum();
        if (mountVnum != 0)
        {
            if (auto* mountSystem = ch->GetMountSystem())
            {
                if (mountSystem->GetByVnum(mountVnum))
                    mountSystem->Unsummon(mountVnum, false);
            }
        }

        if (ch->IsHorseRiding())
            ch->StopRiding();
        else
            ch->MountVnum(0);
    }

    auto& state = g_registry.get_or_emplace<ecs::MountState>(rider);
    state.mountVnum = 0;
    state.horseRiding = false;
    g_registry.emplace_or_replace<ecs::DirtyTag>(rider);
}

} // namespace MountSystem

bool CHARACTER::StartRiding()
{
	const entt::entity rider = GetEntityHandle();
	ecs::ChatSystem::Send(rider, CHAT_TYPE_INFO, "DEBUG: char_horse.cpp: bool CHARACTER::StartRiding()");
#ifdef ENABLE_BUG_FIXES
	if (IsRiding()) {
		return false;
	}
#endif

#ifdef BLOCK_RIDING_INSIDE_WAR
	if (GetWarMap()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 852, "");
#endif
		RemoveAffect(AFFECT_MOUNT);
		RemoveAffect(AFFECT_MOUNT_BONUS);
		if (IsRiding())
			StopRiding();
		return false;
	}
#endif

#ifdef ENABLE_NEWSTUFF
	if (g_NoMountAtGuildWar && GetWarMap())
	{
		RemoveAffect(AFFECT_MOUNT);
		RemoveAffect(AFFECT_MOUNT_BONUS);
		if (IsRiding())
			StopRiding();
		return false;
	}
#endif
	if (IsDead() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 356, "");
#endif
		return false;
	}

	if (IsPolymorphed())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 355, "");
#endif
		return false;
	}

	const entt::entity armor = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_BODY);
	const uint32_t armorVnum = ItemSystem::GetItemVnum(armor);

	if (ItemSystem::IsValidItem(armor) && armorVnum >= 11901 && armorVnum <= 11904)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 410, "");
#endif
		return false;
	}

	if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
		return false;

	uint32_t dwMountVnum = GetHorse()
		? ecs::PlayerRuntime::GetRaceNum(GetHorse()->GetEntityHandle())
		: GetMyHorseVnum();

	if (false == CHorseRider::StartRiding())
	{
#ifdef TEXTS_IMPROVEMENT
		if (GetHorseLevel() <= 0) {
			ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 333, "");
		} else if (GetHorseHealth() <= 0) {
			ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 335, "");
		} else if (GetHorseStamina() <= 0) {
			ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 334, "");
		}
#endif
		return false;
	}

	HorseSummon(false);

	MountVnum(dwMountVnum);

	if(test_server)
		LOG_INFO("Ride Horse : {} ", GetName());

		MarkMountDirty(GetEntityHandle());
	SyncHorseRiding(rider, true);
	return true;
}

bool CHARACTER::StopRiding()
{
	const entt::entity rider = GetEntityHandle();
	ecs::ChatSystem::Send(rider, CHAT_TYPE_INFO, "DEBUG: char_horse.cpp: bool CHARACTER::StopRiding()");

	if (CHorseRider::StopRiding())
	{
		quest::CQuestManager::instance().Unmount(GetPlayerID());

		if (!IsDead() && !IsStun())
		{
			uint32_t dwOldVnum = GetMountVnum();
			MountVnum(0);
			HorseSummon(true, false, dwOldVnum);
		}
		else
		{
			m_dwMountVnum = 0;
			ComputePoints();
			NetworkSyncSystem::UpdatePacket(rider);
		}

		PointChange(POINT_ST, 0);
		PointChange(POINT_DX, 0);
		PointChange(POINT_HT, 0);
		PointChange(POINT_IQ, 0);
		MarkMountDirty(GetEntityHandle());
		SyncHorseRiding(rider, false);
		return true;
	}

	return false;
}

EVENTFUNC(horse_dead_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("horse_dead_event> <Factor> Null pointer");
		return 0;
	}

	auto* ch = ecs::LegacyCharOf(info->ch);
	if (ch == nullptr) {
		return 0;
	}
	ch->HorseSummon(false);
	return 0;
}

void CHARACTER::SetRider(entt::entity chEntity)
{
	LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
	if (m_chRider)
		m_chRider->ClearHorseInfo();

	m_chRider = ch;

	if (m_chRider)
		m_chRider->SendHorseInfo();
}

LPCHARACTER CHARACTER::GetRider() const
{
	return m_chRider;
}

void CHARACTER::HorseSummon(bool bSummon, bool bFromFar, uint32_t dwVnum, const char* pPetName)
{
	MountSystem::HorseSummon(GetEntityHandle(), bSummon, bFromFar, dwVnum, pPetName);
}

uint32_t CHARACTER::GetMyHorseVnum() const
{
	return MountSystem::GetMyHorseVnum(GetEntityHandle());
}

void CHARACTER::HorseDie()
{
	CHorseRider::HorseDie();
	HorseSummon(false);
}

bool CHARACTER::ReviveHorse()
{
	if (CHorseRider::ReviveHorse())
	{
		HorseSummon(false);
		HorseSummon(true);
		MarkMountDirty(GetEntityHandle());
		return true;
	}
	return false;
}

void CHARACTER::ClearHorseInfo()
{
	if (!IsHorseRiding())
	{
		ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "hide_horse_state");

		MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseLevel = 0;
		MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseHealthGrade = 0;
		MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseStaminaGrade = 0;
	}

	MountSystem::SetSummonedHorse(GetEntityHandle(), entt::null);
	MarkMountDirty(GetEntityHandle());
}

void CHARACTER::SendHorseInfo()
{
	if (GetHorse() || IsHorseRiding())
	{
		int iHealthGrade;
		int iStaminaGrade;
		if (GetHorseHealth() == 0)
			iHealthGrade = 0;
		else if (GetHorseHealth() * 10 <= GetHorseMaxHealth() * 3)
			iHealthGrade = 1;
		else if (GetHorseHealth() * 10 <= GetHorseMaxHealth() * 7)
			iHealthGrade = 2;
		else
			iHealthGrade = 3;

		if (GetHorseStamina() * 10 <= GetHorseMaxStamina())
			iStaminaGrade = 0;
		else if (GetHorseStamina() * 10 <= GetHorseMaxStamina() * 3)
			iStaminaGrade = 1;
		else if (GetHorseStamina() * 10 <= GetHorseMaxStamina() * 7)
			iStaminaGrade = 2;
		else
			iStaminaGrade = 3;

		if (MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseLevel != GetHorseLevel() ||
				MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseHealthGrade != iHealthGrade ||
				MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseStaminaGrade != iStaminaGrade)
		{
			ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "horse_state %d %d %d", GetHorseLevel(), iHealthGrade, iStaminaGrade);

			MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseLevel = GetHorseLevel();
			MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseHealthGrade = iHealthGrade;
			MountSystem::GetMountStateRef(GetEntityHandle()).sendHorseStaminaGrade = iStaminaGrade;
			MarkMountDirty(GetEntityHandle());
		}
	}
}

bool CHARACTER::CanUseHorseSkill()
{
	if(IsRiding())
	{
		if (GetHorseGrade() == 3)
			return true;
		else
			return false;

		if(GetMountVnum())
		{
			if (GetMountVnum() >= 20209 && GetMountVnum() <= 20212)
				return true;

			if (CMobVnumHelper::IsRamadanBlackHorse(GetMountVnum()))
				return true;
		}
		else
			return false;

	}

	return false;
}

void CHARACTER::SetHorseLevel(int iLevel)
{
	CHorseRider::SetHorseLevel(iLevel);
	SetSkillLevel(SKILL_HORSE, GetHorseLevel());
	MarkMountDirty(GetEntityHandle());
}

#ifdef ENABLE_FAKE_SHOP_HEADER
#ifdef DISABLE_CORE_PULSE_RAZOR93
bool CHARACTER::IsNextMountPulse() const
{
	return (MountSystem::GetMountStateRef(GetEntityHandle()).mountPulse == 0 || (MountSystem::GetMountStateRef(GetEntityHandle()).mountPulse < thecore_pulse()));
}

#endif
#endif

uint8_t CHARACTER::GetMountCounter() const
{
	return m_bMountCounter;
}

void CHARACTER::ResetMountCounter()
{
	m_bMountCounter = 0;
}

uint8_t CHARACTER::IncreaseMountCounter()
{
	return ++m_bMountCounter;
}

bool CHARACTER::IsRiding() const
{
	return IsHorseRiding() || GetMountVnum();
}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
void CHARACTER::MountUnsummon(entt::entity mountItem)
{
	CMountSystem* mountSystem = GetMountSystem();

	if (!mountSystem || !ItemSystem::IsValidItem(mountItem))
		return;

	const uint32_t mobVnum = GetMountMobVnum(mountItem);

	if (GetMountVnum() == mobVnum)
		mountSystem->Unmount(mobVnum);

	mountSystem->Unsummon(mobVnum);
}

void CHARACTER::CheckMount()
{
	CMountSystem* mountSystem = GetMountSystem();
	const entt::entity mountItem = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);

	if (!mountSystem || !ItemSystem::IsValidItem(mountItem))
		return;

	const uint32_t mobVnum = GetMountMobVnum(mountItem);

	if (mountSystem->CountSummoned() == 0)
	{
		mountSystem->Summon(mobVnum, mountItem, false);
	}
}

bool CHARACTER::IsRidingMount()
{
	return ItemSystem::IsValidItem(
		ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT)) ||
		FindAffect(AFFECT_MOUNT);
}
#endif

#ifdef ENABLE_COSTUME_PET
namespace MountSystem {

// The skin and unsummon paths, entity-native. The subsystem pointers come from
// MountRuntimeRefs / PetRuntimeRefs rather than CHARACTER members, so CItem
// can drive them without holding an owner pointer.

static ::CMountSystem* GetMountSystem(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    const auto* refs = g_registry.try_get<ecs::MountRuntimeRefs>(e);
    return refs ? refs->mountSystem : nullptr;
}

void UpdateMountSkin(entt::entity e)
{
    ::CMountSystem* mountSystem = GetMountSystem(e);
    if (!mountSystem)
        return;

    mountSystem->UpdateMountSkin();

    if (!IsRiding(e))
        return;

    const entt::entity item = ItemSystem::GetWearItem(e, WEAR_COSTUME_MOUNT);
    if (!ItemSystem::IsValidItem(item))
        return;

    const uint32_t mobVnum = GetMountMobVnum(item);

    mountSystem->Unmount(mobVnum);
    mountSystem->Mount(mobVnum, item);
}

void MountUnsummon(entt::entity e, entt::entity mountItem)
{
    ::CMountSystem* mountSystem = GetMountSystem(e);
    if (!mountSystem || !ItemSystem::IsValidItem(mountItem))
        return;

    const uint32_t mobVnum = GetMountMobVnum(mountItem);

    if (GetMountVnum(e) == mobVnum)
        mountSystem->Unmount(mobVnum);

    mountSystem->Unsummon(mobVnum);
}

void UpdatePetSkin(entt::entity e)
{
#ifdef __PET_SYSTEM__
    if (e == entt::null || !g_registry.valid(e))
        return;

    const auto* refs = g_registry.try_get<ecs::PetRuntimeRefs>(e);
    if (refs && refs->petSystem)
        refs->petSystem->UpdatePetSkin();
#endif
}

} // namespace MountSystem

void CHARACTER::UpdatePetSkin() {
	if (!m_petSystem)
		return;

	m_petSystem->UpdatePetSkin();
}

uint32_t CHARACTER::GetPetSkinVnum() {
	const entt::entity item = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_PET_SKIN);
	return ItemSystem::IsValidItem(item) ? ItemSystem::GetItemValue(item, 0) : 0;
}
#endif

#ifdef ENABLE_COSTUME_MOUNT
void CHARACTER::UpdateMountSkin() {
	if (!m_mountSystem)
		return;

	m_mountSystem->UpdateMountSkin();

	if (IsRiding()) {
		const entt::entity item = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);
		if (!ItemSystem::IsValidItem(item))
			return;

		const uint32_t mobVnum = GetMountMobVnum(item);

		m_mountSystem->Unmount(mobVnum);
		m_mountSystem->Mount(mobVnum, item);
	}
}

uint32_t CHARACTER::GetMountSkinVnum() {
	const entt::entity item = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT_SKIN);
	return ItemSystem::IsValidItem(item) ? ItemSystem::GetItemValue(item, 0) : 0;
}
#endif

void CHARACTER::ComputeMountInventoryBonuses()
{
	std::map<uint8_t, int32_t> mount_bonus_map;

	CMountInventory* mi = GetMountInventory();
	if (!mi)
		return;

	const auto& valid_items = CMountInventoryHelper::GetAllowedItems();
	const int total = mi->GetWidth() * mi->GetSize();

	for (int pos = 0; pos < total; ++pos)
	{
		const entt::entity item = mi->Get(pos);
		if (!ItemSystem::IsValidItem(item))
			continue;

		const uint32_t vnum = ItemSystem::GetItemVnum(item);
		if (!valid_items.contains(vnum))
			continue;

		const TItemTable* proto = ItemSystem::GetItemProto(item);
		if (!proto)
			continue;

		for (const auto& apply : proto->aApplies)
		{
			if (apply.bType == APPLY_NONE || apply.lValue == 0)
				continue;

			if (apply.bType >= MAX_APPLY_NUM)
				continue;

			const uint8_t pointType = aApplyInfo[apply.bType].bPointType;
			if (pointType != POINT_NONE)
				mount_bonus_map[pointType] += apply.lValue;
		}

		for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		{
			const auto attribute = ItemSystem::GetItemAttribute(item, i);
			const uint8_t bType = attribute.bType;
			const int16_t sVal = attribute.sValue;

			if (bType == APPLY_NONE || sVal == 0)
				continue;

			if (bType >= MAX_APPLY_NUM)
				continue;

			const uint8_t pointType = aApplyInfo[bType].bPointType;
			if (pointType != POINT_NONE)
				mount_bonus_map[pointType] += sVal;
		}
	}

	for (const auto& it : mount_bonus_map)
		PointChange(it.first, it.second);
}
