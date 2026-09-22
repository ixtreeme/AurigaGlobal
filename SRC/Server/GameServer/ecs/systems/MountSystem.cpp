#include "../../core/stdafx.h"
#include "ViewSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "CombatSystem.hpp"

#include "MountSystem.hpp"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "VisibilitySystem.hpp"

#include "../../core/config.h"
#include "../../entity/char.h"
#include "../../entity/char_manager.h"
#include "../../network/db.h"
#include "../../core/packet.h"
#include "../../guild/guild.h"
#include "../../core/vector.h"
#include "../../quest/questmanager.h"
#include "../../item/item.h"
#include "../../item/item_manager.h"
#include "../../pet/MountInventory.h"
#include "../../pet/mount_inventory_helper.h"
#include "../../pet/horsename_manager.h"
#include "../../core/locale_service.h"
#include "../../world/arena.h"
#include "../../network/desc.h"
#include "../../pet/PetSystem.h"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "../VIDRegistry.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../components/social_components.hpp"
#include "../components/pet_mount_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/status_components.hpp"

#include <common/VnumHelper.h>
#include <utility>
#include <Core/Logging.hpp>
#include "../CharacterAccessors.hpp"
#include "../components/visibility_components.hpp"
#include "../EventDispatcher.hpp"
#include "../events.hpp"
#include "../services/EntityNetworkDispatch.hpp"
#include "MovementSystem.hpp"

EVENTINFO(costume_mount_event_info)
{
    entt::entity owner { entt::null };
};

EVENTFUNC(costume_mount_update_event);

namespace
{
uint64_t NextMountInventoryRequestId()
{
    static uint64_t nextRequestId = 0;
    return ++nextRequestId;
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

    if (!g_registry.all_of<ecs::DirtyTag>(e))
        g_registry.insert<ecs::DirtyTag>(&e, &e + 1);
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

uint32_t GetMountSkinVnum(entt::entity owner)
{
#ifdef ENABLE_COSTUME_MOUNT
    const auto item = ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT_SKIN);
    if (ItemSystem::IsValidItem(item))
        return ItemSystem::GetItemValue(item, 0);
#endif
    return 0;
}

bool IsOwnedMountItem(entt::entity owner, entt::entity item)
{
    return ecs::PlayerRuntime::IsValid(owner) && ItemSystem::IsValidItem(item)
        && ItemSystem::GetItemOwner(item) == owner;
}

bool MountItemDuration(entt::entity owner, entt::entity item, int32_t& duration)
{
    if (!IsOwnedMountItem(owner, item))
        return false;
    const auto* proto = ItemSystem::GetItemProto(item);
    if (!proto)
        return false;
    for (const auto& apply : proto->aApplies)
        if (apply.bType >= MAX_APPLY_NUM)
            return false;
    const int64_t remaining = ItemSystem::IsUnlimitedTimeUnique(item) ? 86400
        : static_cast<int64_t>(ItemSystem::GetItemSocket(item, 0)) - time(nullptr);
    if (remaining <= 0)
        return false;
    duration = static_cast<int32_t>(std::min<int64_t>(remaining, std::numeric_limits<int32_t>::max()));
    return true;
}

bool SnapFollowerToOwner(entt::entity follower, entt::entity owner, int32_t x, int32_t y, int32_t z)
{
    if (!ecs::PlayerRuntime::IsValid(follower) || !ecs::PlayerRuntime::IsValid(owner))
        return false;
    if (!ecs::MovementSystem::Show(follower, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z))
        return false;
    ecs::MovementSystem::Stop(follower);
    ecs::MovementSystem::SendMovePacket(follower, FUNC_WAIT, 0, 0, 0, 0);
    return true;
}

ecs::CostumeMountRuntime* CostumeRuntime(entt::entity owner)
{
    if (owner == entt::null || !g_registry.valid(owner))
        return nullptr;
    return &g_registry.get_or_emplace<ecs::CostumeMountRuntime>(owner);
}

ecs::CostumeMountActorState* CostumeRecord(ecs::CostumeMountRuntime& runtime,
    uint32_t vnum)
{
    for (auto& record : runtime.actors)
        if (record.vnum == vnum)
            return &record;
    return nullptr;
}

ecs::CostumeMountActorState* CostumeRecordForItem(ecs::CostumeMountRuntime& runtime,
    entt::entity item)
{
    for (auto& record : runtime.actors)
        if (record.summonItem == item)
            return &record;
    return nullptr;
}

void SetMountComponent(entt::entity owner, entt::entity item, bool summoned)
{
    if (!g_registry.valid(owner))
        return;

    auto& state = g_registry.get_or_emplace<ecs::MountComponent>(owner);
    if (!ItemSystem::IsValidItem(item))
    {
        state = {};
        return;
    }

    state.owner = owner;
    state.item = item;
    state.itemID = ItemSystem::GetItemID(item);
    state.itemVID = ItemSystem::GetItemVID(item);
    state.itemVnum = ItemSystem::GetItemVnum(item);
    state.state = summoned ? 1u : 0u;
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        state.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket(item, i));
}

void ClearCostumeMountEffects(entt::entity owner)
{
    if (!ecs::PlayerRuntime::IsValid(owner))
        return;

    AffectSystem::RemoveAffect(owner, AFFECT_MOUNT);
    AffectSystem::RemoveAffect(owner, AFFECT_MOUNT_BONUS);
    MountSystem::SetMountVnum(owner, 0);
    for (const auto point : { POINT_ST, POINT_DX, POINT_HT, POINT_IQ })
        ecs::PointSystem::Change(owner, point, 0);
}

void DestroyCostumeRecord(entt::entity owner, uint32_t vnum)
{
    auto* runtime = CostumeRuntime(owner);
    if (!runtime)
        return;

    entt::entity character = entt::null;
    entt::entity item = entt::null;
    if (auto* record = CostumeRecord(*runtime, vnum))
    {
        character = record->character;
        item = record->summonItem;
        *record = { vnum, entt::null, entt::null, 0 };
    }
    if (item != entt::null && g_registry.valid(owner))
    {
        if (auto* state = g_registry.try_get<ecs::MountComponent>(owner);
            state && state->item == item)
            *state = {};
    }
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
}

// The ridden costume mount is the rider's mount model, so the follow creature
// must not stay visible next to it. Hiding keeps the record and its item
// association, so the dismount re-summons the same mount - the legacy actor
// kept its vnum/summon item through Unsummon the same way.
void HideCostumeRecord(entt::entity owner, uint32_t vnum)
{
    auto* runtime = CostumeRuntime(owner);
    if (!runtime)
        return;

    auto* record = CostumeRecord(*runtime, vnum);
    if (!record)
        return;

    const entt::entity character = std::exchange(record->character, entt::null);
    if (ecs::PlayerRuntime::IsValid(character))
        ecs::PlayerRuntime::DestroyCharacter(character);
}

void HideCostumeFollowers(entt::entity owner)
{
    auto* runtime = CostumeRuntime(owner);
    if (!runtime)
        return;

    std::vector<uint32_t> vnums;
    for (const auto& record : runtime->actors)
        vnums.push_back(record.vnum);
    for (const uint32_t vnum : vnums)
        HideCostumeRecord(owner, vnum);
}

void EnsureCostumeMountEvent(entt::entity owner)
{
    auto* runtime = CostumeRuntime(owner);
    if (!runtime || runtime->updateEvent)
        return;
    auto* info = AllocEventInfo<costume_mount_event_info>();
    info->owner = owner;
    runtime->updateEvent = event_create(costume_mount_update_event, info,
        PASSES_PER_SEC(1) / 4);
}

void UpdateCostumeMounts(entt::entity owner)
{
    auto* runtime = CostumeRuntime(owner);
    if (!runtime)
        return;
    const uint32_t now = get_dword_time();
    if (runtime->updatePeriod > now - runtime->lastUpdateTime)
        return;

    // Only the ridden mount may be visible: any follower left from an earlier
    // state (or a stale session) is hidden until the dismount re-summons it.
    if (MountSystem::GetMountVnum(owner) != 0)
    {
        HideCostumeFollowers(owner);
        if (auto* current = CostumeRuntime(owner))
            current->lastUpdateTime = now;
        return;
    }

    std::vector<uint32_t> vnums;
    for (const auto& record : runtime->actors)
        vnums.push_back(record.vnum);

    for (const uint32_t vnum : vnums)
    {
        runtime = CostumeRuntime(owner);
        auto* record = runtime ? CostumeRecord(*runtime, vnum) : nullptr;
        if (!record)
            continue;
        if (!ecs::PlayerRuntime::IsValid(record->character) ||
            !ItemSystem::IsValidItem(record->summonItem) ||
            ItemSystem::GetItemOwner(record->summonItem) != owner ||
            CombatSystem::IsDead(owner) || CombatSystem::IsDead(record->character) ||
            !ecs::PlayerRuntime::GetMobTable(record->character))
        {
            DestroyCostumeRecord(owner, vnum);
            continue;
        }

        const auto ownerX = ecs::PlayerRuntime::GetX(owner);
        const auto ownerY = ecs::PlayerRuntime::GetY(owner);
        const auto charX = ecs::PlayerRuntime::GetX(record->character);
        const auto charY = ecs::PlayerRuntime::GetY(record->character);
        const float distance = DISTANCE_APPROX(charX - ownerX, charY - ownerY);
        bool moved = false;
        if (distance >= 4500.f ||
            ecs::PlayerRuntime::GetMapIndex(record->character) != ecs::PlayerRuntime::GetMapIndex(owner))
        {
            const float rotation = ecs::PlayerRuntime::GetRotation(owner) * 3.141592f / 180.f;
            moved = SnapFollowerToOwner(record->character, owner,
                ownerX - static_cast<int32_t>(200 * cos(rotation)),
                ownerY - static_cast<int32_t>(200 * sin(rotation)),
                ecs::PlayerRuntime::GetZ(owner));
        }
        if (!moved && distance >= 300.f)
        {
            ecs::MovementSystem::SetNowWalking(record->character, false);
            const auto rotation = GetDegreeFromPositionXY(charX, charY, ownerX, ownerY);
            ecs::MovementSystem::SetRotation(record->character, rotation);
            float dx, dy;
            GetDeltaByDegree(rotation, distance - 200.f, &dx, &dy);
            ecs::MovementSystem::Goto(record->character,
                static_cast<int32_t>(charX + dx + 0.5f),
                static_cast<int32_t>(charY + dy + 0.5f));
            ecs::MovementSystem::SendMovePacket(record->character, FUNC_WAIT, 0, 0, 0, 0);
            record->lastActionTime = now;
            CombatSystem::SetLastAttacked(record->character, now);
        }
        else if (!moved)
            ecs::MovementSystem::SendMovePacket(record->character, FUNC_WAIT, 0, 0, 0, 0);
    }
    if (auto* current = CostumeRuntime(owner))
        current->lastUpdateTime = now;
}

} // namespace

namespace MountSystem {

} // namespace MountSystem

EVENTFUNC(costume_mount_update_event)
{
    const auto* info = dynamic_cast<costume_mount_event_info*>(event->info);
    if (!info || !ecs::PlayerRuntime::IsValid(info->owner))
        return 0;
    const auto owner = info->owner;
    const auto* runtime = g_registry.try_get<ecs::CostumeMountRuntime>(owner);
    if (!runtime || runtime->updateEvent != event)
        return 0;
    UpdateCostumeMounts(owner);
    if (g_registry.valid(owner))
        g_dispatcher.trigger(ecs::EvMountSystemUpdate { owner });
    return PASSES_PER_SEC(1) / 4;
}

void MountSystem::QueryMountInventory(entt::entity e)
{
    if (MountSystem::GetMountInventory(e) != entt::null || !ecs::PlayerRuntime::GetDesc(e) ||
        !g_registry.valid(e))
        return;

    const uint32_t accountId = ecs::PlayerRuntime::GetAccountID(e);
    if (accountId == 0)
        return;

    if (const auto* pending = g_registry.try_get<ecs::MountInventoryLoadState>(e);
        pending && pending->accountId == accountId)
        return;

    const uint64_t requestId = NextMountInventoryRequestId();
    g_registry.emplace_or_replace<ecs::MountInventoryLoadState>(e, accountId, requestId);

    auto* request = M2_NEW MountInventoryLoadRequest;
    request->character = e;
    request->accountId = accountId;
    request->requestId = requestId;

    DBManager::instance().ReturnQuery(QID_MOUNT_INVENTORY_LOAD,
        ecs::PlayerRuntime::GetPlayerID(e),
        request,
        "SELECT id, slot, vnum, count, socket0, socket1, socket2, "
        "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, "
        "attrtype3, attrvalue3, attrtype4, attrvalue4, attrtype5, attrvalue5 "
        "FROM account_mount_inventory WHERE account_id=%u ORDER BY slot",
        accountId);
}

void MountSystem::LoadMountInventory(entt::entity e, uint32_t accountId,
    uint64_t requestId, const std::vector<TMountInventoryItemTable>& items)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    const auto* pending = g_registry.try_get<ecs::MountInventoryLoadState>(e);
    if (accountId == 0 || !pending || pending->accountId != accountId ||
        pending->requestId != requestId || ecs::PlayerRuntime::GetAccountID(e) != accountId)
        return;

    if (MountSystem::GetMountInventory(e) != entt::null)
    {
        g_registry.remove<ecs::MountInventoryLoadState>(e);
        return;
    }

    const entt::entity inventory = MountSystem::CreateMountInventory(e, accountId, 16);
    if (inventory == entt::null)
    {
        g_registry.remove<ecs::MountInventoryLoadState>(e);
        return;
    }

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

        if (!MountSystem::AddMountInventoryItem(e, entry.slot, item, true))
            ItemSystem::DestroyItemEntityEcs(item, "MOUNT_INVENTORY_LOAD_ADD_FAILED");
    }

    g_registry.remove<ecs::MountInventoryLoadState>(e);
    if (!g_registry.valid(e) || MountSystem::GetMountInventory(e) != inventory)
        return;

    MountSystem::SendMountInventory(e);
    if (g_registry.valid(e))
        ecs::PointSystem::Compute(e);
}

void MountSystem::SendMountInventory(entt::entity owner)
{
    if (!ecs::PlayerRuntime::GetDesc(owner))
        return;
    if (GetMountInventory(owner) == entt::null)
        return;

    std::vector<TMountInventoryItemTable> items;
    CollectMountInventoryItems(owner, items);

    TPacketGCMountInventory header{};
    header.bHeader = HEADER_GC_MOUNT_INVENTORY;
    header.size = sizeof(TPacketGCMountInventory) + static_cast<uint16_t>(items.size() * sizeof(TMountInventoryItemData));
    header.bWidth = static_cast<uint8_t>(GetMountInventoryWidth(owner));
    header.bHeight = static_cast<uint8_t>(GetMountInventorySize(owner));
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

    if (auto* desc = ecs::PlayerRuntime::GetDesc(owner))
        desc->Packet(buf.read_peek(), buf.size());
}

void MountSystem::UpdateMountCountOverheadToViewers(entt::entity owner)
{
#ifdef ENABLE_FAKE_SHOP_HEADER
    if (!g_registry.valid(owner)) return;
    MountSystem::UpdateMountInventoryCountOverhead(owner, owner);
    if (!g_registry.valid(owner)) return;

    // The ECS ViewMap, not m_map_view: this is a CHARACTER, and for characters
    // the legacy map stopped being maintained when D.6 disabled the polling in
    // UpdateSectree. It is frozen at whatever it held then, so this loop was
    // walking stale contents.
    const entt::entity selfEntity = owner;
    if (const auto* viewMap = g_registry.try_get<ecs::ViewMap>(selfEntity))
    {
        const auto viewers = viewMap->visible;
        for (const entt::entity viewerEntity : viewers)
        {
            if (!g_registry.valid(owner)) return;
            if (viewerEntity == selfEntity)
                continue;

            if (ecs::PlayerRuntime::IsPC(viewerEntity) && ecs::PlayerRuntime::GetDesc(viewerEntity))
                MountSystem::UpdateMountInventoryCountOverhead(owner, viewerEntity);
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
    return GetSummonedHorse(rider) != entt::null;
}

bool IsRidingCostume(entt::entity rider)
{
    return ecs::PlayerRuntime::IsPC(rider) && (ItemSystem::IsValidItem(
        ItemSystem::GetWearItem(rider, WEAR_COSTUME_MOUNT)) ||
        AffectSystem::FindAffect(rider, AFFECT_MOUNT));
}

bool IsOwnedHorse(entt::entity rider, entt::entity horse)
{
    return horse != entt::null && g_registry.valid(horse) && GetRider(horse) == rider;
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
    if (rider == entt::null || !g_registry.valid(rider))
        return;

    if (GetMountVnum(rider) == vnum)
        return;
    if (GetMountVnum(rider) != 0 && vnum != 0)
        SetMountVnum(rider, 0);

    auto& mount = g_registry.get_or_emplace<ecs::MountState>(rider);
    mount.mountVnum = vnum;
    mount.mountTime = get_dword_time();
    if (vnum == 0)
        mount.horseRiding = false;
    g_registry.emplace_or_replace<ecs::DirtyTag>(rider);

    if (ecs::PlayerRuntime::IsObserverMode(rider))
        return;

    // Phase C.3: the legacy destination field write is gone.
    // SyncDestinationClear drops MovementDestination so the next INSERT
    // packet carries the current position.
    ecs::MovementSystem::SyncDestinationClear(rider);

    ecs::EntityNetworkDispatch::SendInsert(g_registry, rider, rider);

    // The viewers are re-sent the rider because the mount changes how it
    // is drawn. This walk reads ViewerMap; the legacy m_map_view it used
    // to walk had been frozen at spawn, so the rebroadcast reached nobody
    // and every viewer kept rendering the old mount state.
    if (auto* viewerMap = g_registry.try_get<ecs::ViewerMap>(rider))
    {
        const auto viewers = viewerMap->viewers;
        for (const entt::entity viewer : viewers)
        {
            if (viewer == entt::null || !g_registry.valid(viewer))
                continue;
            ecs::EntityNetworkDispatch::SendInsert(g_registry, rider, viewer);
        }
    }

    CombatSystem::SetValidComboInterval(rider, 0);
    CombatSystem::SetComboSequence(rider, 0);

    ecs::PointSystem::Compute(rider);
}

} // namespace MountSystem

EVENTFUNC(horse_dead_event);

namespace MountSystem {

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

	if (!ItemSystem::IsValidItem(mountItem))
		return;

	if (IsHorseRiding(rider))
		StopRiding(rider);

	if (GetSummonedHorse(rider) != entt::null)
		SummonHorse(rider, false);

	SummonCostumeMount(rider, mountItem, false);
}

void SummonHorse(entt::entity rider, bool bSummon, bool bFromFar, uint32_t dwVnum, const char* pPetName)
{
	if (rider == entt::null || !g_registry.valid(rider))
		return;
	if ( bSummon )
	{
		if (GetSummonedHorse(rider) != entt::null)
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

		// The condition used to spawn a horse and the branch a second one; only the
		// second was kept as the summoned horse and the first stood on the map.
		SetSummonedHorse(rider, CHARACTER_MANAGER::instance().SpawnMobEntity(
				(0 == dwVnum) ? GetMyHorseVnum(rider) : dwVnum,
				ecs::PlayerRuntime::GetMapIndex(rider),
				x, y,
				ecs::PlayerRuntime::GetZ(rider), false, (int)(ecs::PlayerRuntime::GetRotation(rider)+180), false));

		// A spawn that answered null left nothing to name, show or ride.
		const entt::entity horse = GetSummonedHorse(rider);
		if (horse == entt::null)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(rider, CHAT_TYPE_INFO, 328, "");
#endif
			return;
		}

		if (GetHorseHealth(rider) <= 0)
		{
			ecs::PlayerRuntime::SetPosition(horse, POS_DEAD);

			char_event_info* info = AllocEventInfo<char_event_info>();
			info->ch = rider;
			ecs::PlayerRuntime::SetCharEvent(horse, ecs::PlayerRuntime::CharEvent::Dead,
				event_create(horse_dead_event, info, PASSES_PER_SEC(60)));
		}

		ecs::PlayerRuntime::SetLevel(horse, GetHorseLevel(rider));

		const char* pHorseName = CHorseNameManager::instance().GetHorseName(ecs::PlayerRuntime::GetPlayerID(rider));

		if ( pHorseName != nullptr && strlen(pHorseName) != 0 )
		{
			ecs::PlayerRuntime::SetName(horse, pHorseName);
		}
		else
		{
			uint8_t bLang = 0;
			if (ecs::PlayerRuntime::GetDesc(rider)) {
				bLang = ecs::PlayerRuntime::GetDesc(rider)->GetLanguage();
			}

			// Three SetName calls built this one name a piece at a time.
			ecs::PlayerRuntime::SetName(horse,
				std::string(ecs::PlayerRuntime::GetName(rider)) + " " + m_horseText[bLang]);
		}

		if (!ecs::MovementSystem::Show(horse, ecs::PlayerRuntime::GetMapIndex(rider), x, y, ecs::PlayerRuntime::GetZ(rider)))
		{
			M2_DESTROY_CHARACTER(horse);
			LOG_ERROR("cannot show monster");
			SetSummonedHorse(rider, entt::null);
			return;
		}

		if ((GetHorseHealth(rider) <= 0))
		{
			TPacketGCDead pack;
			pack.header	= HEADER_GC_DEAD;
			pack.vid    = ecs::PlayerRuntime::GetPacketVID(horse);
			ecs::ViewSystem::PacketView(rider, &pack, sizeof(pack));
		}

		SetRider(horse, rider);
	}
	else
	{
		const entt::entity horse = GetSummonedHorse(rider);
		if (horse == entt::null)
			return;

		SetRider(horse, entt::null);
        if (!g_registry.valid(horse) || !g_registry.valid(rider)) return;

		if ((GetHorseHealth(rider) <= 0))
			bFromFar = false;

		if (!bFromFar)
		{
			M2_DESTROY_CHARACTER(horse);
		}
		else
		{
            const auto* movement = g_registry.try_get<ecs::MovementState>(horse);
            if (movement && movement->isNowWalking) {
                ecs::MovementSystem::SetNowWalking(horse, false);
                if (g_registry.valid(horse) && ecs::PlayerRuntime::IsNPC(horse))
                    ecs::PlayerRuntime::MonsterLog(horse, "horse run");
            }
            if (!g_registry.valid(horse) || !g_registry.valid(rider)) return;

			float fx, fy;
			ecs::MovementSystem::SetRotation(horse, GetDegreeFromPositionXY(
				ecs::PlayerRuntime::GetX(horse),
				ecs::PlayerRuntime::GetY(horse), ecs::PlayerRuntime::GetX(rider), ecs::PlayerRuntime::GetY(rider)) + 180);
			GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(horse), 3500, &fx, &fy);
			ecs::MovementSystem::Goto(horse,
				static_cast<int32_t>(ecs::PlayerRuntime::GetX(horse) + fx),
				static_cast<int32_t>(ecs::PlayerRuntime::GetY(horse) + fy));
			ecs::MovementSystem::SendMovePacket(horse, FUNC_WAIT, 0, 0, 0, 0);
		}

		SetSummonedHorse(rider, entt::null);
	}

	MarkMountDirty(rider);
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
    if (GetMountInventory(e) != entt::null)
    {
        const int total = GetMountInventoryWidth(e) * GetMountInventorySize(e);
        for (int pos = 0; pos < total; ++pos)
        {
            if (GetMountInventoryItem(e, pos) != entt::null)
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

    if (!g_registry.all_of<ecs::SummonedHorse>(rider))
        g_registry.insert<ecs::SummonedHorse>(&rider, &rider + 1);
    if (g_registry.valid(rider))
        if (auto* state = g_registry.try_get<ecs::SummonedHorse>(rider))
            state->horse = horse;
}

bool IsHorseRiding(entt::entity rider)
{
    // Strictly the riding flag, not IsRiding - that one also answers true for a
    // summoned mount. This is the flag itself: CHorseRider used to keep it in
    // m_Horse.bRiding and have the two riding calls copy it here.
    if (rider == entt::null || !g_registry.valid(rider))
        return false;

    const auto* state = g_registry.try_get<ecs::MountState>(rider);
    return state && state->horseRiding;
}

bool IsCostumeMountSummoned(entt::entity rider)
{
    const auto* runtime = rider == entt::null || !g_registry.valid(rider)
        ? nullptr : g_registry.try_get<ecs::CostumeMountRuntime>(rider);
    if (!runtime)
        return false;
    for (const auto& record : runtime->actors)
        if (g_registry.valid(record.character))
            return true;
    return false;
}

size_t CountCostumeMounts(entt::entity rider)
{
    const auto* runtime = rider == entt::null || !g_registry.valid(rider)
        ? nullptr : g_registry.try_get<ecs::CostumeMountRuntime>(rider);
    if (!runtime)
        return 0;
    return std::count_if(runtime->actors.begin(), runtime->actors.end(),
        [](const auto& record) { return g_registry.valid(record.character); });
}

void SummonCostumeMount(entt::entity rider, entt::entity mountItem, bool spawnFar)
{
    if (rider == entt::null || !g_registry.valid(rider) ||
        !ItemSystem::IsValidItem(mountItem) ||
        ItemSystem::GetItemOwner(mountItem) != rider)
        return;

    const uint32_t vnum = GetMountMobVnum(mountItem);
    if (vnum == 0)
        return;
    auto* runtime = CostumeRuntime(rider);
    if (!runtime || runtime->destroying)
        return;

    auto* record = CostumeRecord(*runtime, vnum);
    if (!record)
    {
        if (auto* byItem = CostumeRecordForItem(*runtime, mountItem))
            record = byItem;
        else
        {
            runtime->actors.push_back({});
            record = &runtime->actors.back();
            record->vnum = vnum;
        }
    }
    const uint32_t recordVnum = record->vnum;

    std::vector<uint32_t> duplicateVnums;
    for (const auto& other : runtime->actors)
        if (other.vnum != recordVnum && other.summonItem == mountItem)
            duplicateVnums.push_back(other.vnum);
    for (const uint32_t duplicate : duplicateVnums)
        DestroyCostumeRecord(rider, duplicate);

    runtime = CostumeRuntime(rider);
    record = runtime ? CostumeRecord(*runtime, recordVnum) : nullptr;
    if (!record)
        return;

    int32_t x = ecs::PlayerRuntime::GetX(rider);
    int32_t y = ecs::PlayerRuntime::GetY(rider);
    const int32_t z = ecs::PlayerRuntime::GetZ(rider);
    x += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);
    y += spawnFar ? (number(0, 1) * 2 - 1) * number(2000, 2500) : number(-100, 100);

    if (g_registry.valid(record->character))
    {
        if (record->summonItem != mountItem ||
            !SnapFollowerToOwner(record->character, rider, x, y, z))
            return;
        record->summonItem = mountItem;
    }
    else
    {
        record->character = CHARACTER_MANAGER::instance().SpawnMobEntity(
            GetMountSkinVnum(rider) ? GetMountSkinVnum(rider) : recordVnum,
            ecs::PlayerRuntime::GetMapIndex(rider), x, y, z, false,
            static_cast<int>(ecs::PlayerRuntime::GetRotation(rider) + 180), false);
        if (!g_registry.valid(record->character))
            return;
        g_registry.get_or_emplace<ecs::MountOwner>(record->character).owner = rider;
        g_registry.get_or_emplace<ecs::StatusFlags>(record->character).isMount = true;
        ecs::PlayerRuntime::SetEmpire(record->character, ecs::PlayerRuntime::GetEmpire(rider));
        g_registry.emplace_or_replace<ecs::PlayerName>(record->character,
            std::string(ecs::PlayerRuntime::GetName(rider)) + "'s Mount");
        record->summonItem = mountItem;
        if (!ecs::MovementSystem::Show(record->character,
                ecs::PlayerRuntime::GetMapIndex(rider), x, y, z))
        {
            DestroyCostumeRecord(rider, recordVnum);
            return;
        }
    }

    SetMountComponent(rider, mountItem, true);
    EnsureCostumeMountEvent(rider);
    if (ItemSystem::GetItemSocket(mountItem, 2) == 1)
        MountCostume(rider, mountItem);
}

void MountCostume(entt::entity rider, entt::entity mountItem)
{
    if (rider == entt::null || !g_registry.valid(rider) ||
        !ItemSystem::IsValidItem(mountItem) ||
        ItemSystem::GetItemOwner(mountItem) != rider)
        return;

    int32_t duration = 0;
    if (!MountItemDuration(rider, mountItem, duration))
        return;

    // Taking a costume mount dismisses the regular horse first, as the legacy
    // actor did: a summoned horse would stay visible next to the mount.
    if (IsHorseRiding(rider))
        StopRiding(rider);
    if (!g_registry.valid(rider))
        return;
    if (GetSummonedHorse(rider) != entt::null)
        SummonHorse(rider, false);
    if (!g_registry.valid(rider))
        return;

    auto* runtime = CostumeRuntime(rider);
    if (!runtime)
        return;
    auto* record = CostumeRecordForItem(*runtime, mountItem);
    if (!record)
    {
        SummonCostumeMount(rider, mountItem, false);
        runtime = CostumeRuntime(rider);
        record = runtime ? CostumeRecordForItem(*runtime, mountItem) : nullptr;
    }
    if (!record)
        return;

    const uint32_t ridingVnum = GetMountSkinVnum(rider) ? GetMountSkinVnum(rider) : record->vnum;
    if (GetMountVnum(rider) != ridingVnum)
        ClearCostumeMountEffects(rider);

    if (const auto* proto = ItemSystem::GetItemProto(mountItem))
    {
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93
        if (!AffectSystem::FindAffect(rider, AFFECT_MOUNT_BONUS))
#endif
        {
            for (const auto& apply : proto->aApplies)
                if (apply.bType != APPLY_NONE)
                    AffectSystem::AddAffect(rider, AFFECT_MOUNT_BONUS,
                        aApplyInfo[apply.bType].bPointType, apply.lValue,
                        AFF_NONE, duration, 0, false);
            AffectSystem::AddAffect(rider, AFFECT_MOUNT_BONUS, POINT_MOV_SPEED,
                50, AFF_NONE, duration, 0, false);
        }
    }
    AffectSystem::AddAffect(rider, AFFECT_MOUNT, POINT_MOUNT, ridingVnum,
        AFF_NONE, duration, 0, true);

    // Mounting hides every follower creature; the records stay so the unmount
    // re-summons the same mount. Without this the ridden model and the
    // follower render as two mounts.
    HideCostumeFollowers(rider);
    if (!g_registry.valid(rider))
        return;

    if (GetMountVnum(rider) == ridingVnum)
    {
        ItemSystem::SetItemSocket(mountItem, 2, 1);
        SetMountComponent(rider, mountItem, false);
    }
}

void UnmountCostume(entt::entity rider)
{
    if (!ecs::PlayerRuntime::IsValid(rider) || GetMountVnum(rider) == 0)
        return;

    const entt::entity item = ItemSystem::GetWearItem(rider, WEAR_COSTUME_MOUNT);
    auto* runtime = CostumeRuntime(rider);
    uint32_t vnum = 0;
    if (runtime)
    {
        if (auto* record = ItemSystem::IsValidItem(item)
                ? CostumeRecordForItem(*runtime, item) : nullptr)
            vnum = record->vnum;
        else
            for (const auto& record : runtime->actors)
                if (record.summonItem != entt::null)
                {
                    vnum = record.vnum;
                    break;
                }
    }

    ClearCostumeMountEffects(rider);
    if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == rider)
        ItemSystem::SetItemSocket(item, 2, 0);
    if (vnum != 0 && ItemSystem::IsValidItem(item))
        SummonCostumeMount(rider, item, false);
}

void DestroyCostumeMountRuntime(entt::entity rider)
{
    auto* runtime = rider == entt::null || !g_registry.valid(rider)
        ? nullptr : g_registry.try_get<ecs::CostumeMountRuntime>(rider);
    if (!runtime)
        return;
    runtime->destroying = true;
    event_cancel(&runtime->updateEvent);
    ClearCostumeMountEffects(rider);
    std::vector<uint32_t> vnums;
    for (const auto& record : runtime->actors)
        vnums.push_back(record.vnum);
    for (const uint32_t vnum : vnums)
        DestroyCostumeRecord(rider, vnum);
    if (auto* current = g_registry.try_get<ecs::CostumeMountRuntime>(rider))
        current->actors.clear();
    if (g_registry.valid(rider))
        g_registry.remove<ecs::CostumeMountRuntime>(rider);
}

void ForceClearRidingState(entt::entity rider)
{
    if (!g_registry.valid(rider)) return;
    const auto vnum = GetMountVnum(rider);
    if (vnum && g_registry.try_get<ecs::CostumeMountRuntime>(rider))
        DestroyCostumeMountRuntime(rider);
    if (!g_registry.valid(rider)) return;
    if (IsHorseRiding(rider)) StopRiding(rider);
    else SetMountVnum(rider, 0);
    if (!g_registry.valid(rider)) return;
    if (auto* state = g_registry.try_get<ecs::MountState>(rider)) {
        state->mountVnum = 0;
        state->horseRiding = false;
    }
    MarkMountDirty(rider);
}

} // namespace MountSystem

EVENTFUNC(horse_dead_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("horse_dead_event> <Factor> Null pointer");
		return 0;
	}

	MountSystem::SummonHorse(info->ch, false);
	return 0;
}

namespace MountSystem {

entt::entity GetRider(entt::entity horse)
{
	if (horse == entt::null || !g_registry.valid(horse))
		return entt::null;

	const auto* link = g_registry.try_get<ecs::HorseRider>(horse);
	return link ? link->rider : entt::null;
}

void SetRider(entt::entity horse, entt::entity rider)
{
    if (!g_registry.valid(horse)) return;
    if (!g_registry.all_of<ecs::HorseRider>(horse))
        g_registry.insert<ecs::HorseRider>(&horse, &horse + 1);
    if (!g_registry.valid(horse) || !g_registry.all_of<ecs::HorseRider>(horse)) return;
    const auto previous = g_registry.get<ecs::HorseRider>(horse).rider;
    g_registry.get<ecs::HorseRider>(horse).rider = rider;
    if (GetSummonedHorse(previous) == horse) ClearHorseInfo(previous);
    if (g_registry.valid(horse) && GetRider(horse) == rider) SendHorseInfo(rider);
}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
// Summons the worn costume mount when nothing is summoned yet.
void CheckMount(entt::entity e)
{
	const entt::entity mountItem = ItemSystem::GetWearItem(e, WEAR_COSTUME_MOUNT);

	if (!ItemSystem::IsValidItem(mountItem))
		return;

	// While the rider is mounted the follower stays hidden; the unmount path
	// re-summons it. Without this an affect change mid-ride would spawn a
	// second visible mount.
	if (GetMountVnum(e) != 0)
		return;

	if (!IsCostumeMountSummoned(e))
		SummonCostumeMount(e, mountItem, false);
}
#endif

} // namespace MountSystem

#ifdef ENABLE_COSTUME_PET
namespace MountSystem {

// The skin and unsummon paths, entity-native. The costume mount runtime state
// is the CostumeMountRuntime component; no subsystem pointer is retained.

void UpdateMountSkin(entt::entity e)
{
    auto* runtime = CostumeRuntime(e);
    if (!runtime)
        return;

    const bool riding = GetMountVnum(e) != 0;
    std::vector<entt::entity> items;
    for (const auto& record : runtime->actors)
        if (ItemSystem::IsValidItem(record.summonItem))
            items.push_back(record.summonItem);
    if (riding)
        ClearCostumeMountEffects(e);
    for (const entt::entity item : items)
    {
        runtime = CostumeRuntime(e);
        if (!runtime)
            return;
        if (auto* record = CostumeRecordForItem(*runtime, item))
            DestroyCostumeRecord(e, record->vnum);
        SummonCostumeMount(e, item, false);
        if (riding)
            MountCostume(e, item);
    }
}

void MountUnsummon(entt::entity e, entt::entity)
{
    DestroyCostumeMountRuntime(e);
}

void UpdatePetSkin(entt::entity e)
{
#ifdef __PET_SYSTEM__
    PetSystem::UpdatePetSkin(e);
#endif
}

} // namespace MountSystem

#endif

void MountSystem::ComputeMountInventoryBonuses(entt::entity owner)
{
	std::map<uint8_t, int64_t> mount_bonus_map;
	if (GetMountInventory(owner) == entt::null)
		return;

	const auto& valid_items = CMountInventoryHelper::GetAllowedItems();
	const int total = GetMountInventoryWidth(owner) * GetMountInventorySize(owner);

	for (int pos = 0; pos < total; ++pos)
	{
		const entt::entity item = GetMountInventoryItem(owner, pos);
		if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemOwner(item) != owner)
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
	{
		if (!g_registry.valid(owner)) return;
		ecs::PointSystem::Change(owner, it.first, it.second);
	}
}
