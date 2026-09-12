#include "../../stdafx.h"
#include "ViewSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "CombatSystem.hpp"

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
#include "../components/pet_mount_components.hpp"
#include "../components/movement_components.hpp"

#include <common/VnumHelper.h>
#include <utility>
#include <Core/Logging.hpp>
#include "../CharacterAccessors.hpp"
#include "../components/visibility_components.hpp"
#include "../services/EntityNetworkDispatch.hpp"
#include "MovementSystem.hpp"

namespace
{

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

} // namespace

namespace MountSystem {

} // namespace MountSystem

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
    MountSystem::SendMountInventory(GetEntityHandle());
    ecs::PointSystem::Compute(GetEntityHandle());
}

void MountSystem::SendMountInventory(entt::entity owner)
{
    if (!ecs::PlayerRuntime::GetDesc(owner))
        return;
    auto* inventory = GetMountInventory(owner);
    if (!inventory) return;

    std::vector<TMountInventoryItemTable> items;
    inventory->CollectItems(items);

    TPacketGCMountInventory header{};
    header.bHeader = HEADER_GC_MOUNT_INVENTORY;
    header.size = sizeof(TPacketGCMountInventory) + static_cast<uint16_t>(items.size() * sizeof(TMountInventoryItemData));
    header.bWidth = inventory->GetWidth();
    header.bHeight = inventory->GetSize();
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
		SummonHorse(rider, false);

	mountSystem->Summon(mobVnum, mountItem, false);
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

// The packet-dedup counters and the pulse gate. They were four CHARACTER
// members mirrored into MountState by every SyncMountState call; the component
// is the only copy now, so the mirror argument list goes away with them.
entt::entity GetMountInventoryItem(entt::entity rider, uint32_t cell)
{
    const auto* inventory = GetMountInventory(rider);
    return inventory ? inventory->Get(cell) : entt::null;
}

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

void ForceClearRidingState(entt::entity rider)
{
    if (!g_registry.valid(rider)) return;
    const auto vnum = GetMountVnum(rider);
    if (auto* system = GetMountSystem(rider); system && vnum && system->GetByVnum(vnum))
        system->Unsummon(vnum, false);
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

} // namespace MountSystem

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
void CHARACTER::MountUnsummon(entt::entity mountItem)
{
	CMountSystem* mountSystem = GetMountSystem();

	if (!mountSystem || !ItemSystem::IsValidItem(mountItem))
		return;

	const uint32_t mobVnum = GetMountMobVnum(mountItem);

	if (MountSystem::GetMountVnum(GetEntityHandle()) == mobVnum)
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
		AffectSystem::FindAffect(GetEntityHandle(), AFFECT_MOUNT);
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

#endif

#ifdef ENABLE_COSTUME_MOUNT
void CHARACTER::UpdateMountSkin() {
	if (!m_mountSystem)
		return;

	m_mountSystem->UpdateMountSkin();

	if (MountSystem::IsRiding(GetEntityHandle())) {
		const entt::entity item = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);
		if (!ItemSystem::IsValidItem(item))
			return;

		const uint32_t mobVnum = GetMountMobVnum(item);

		m_mountSystem->Unmount(mobVnum);
		m_mountSystem->Mount(mobVnum, item);
	}
}

#endif

void MountSystem::ComputeMountInventoryBonuses(entt::entity owner)
{
	std::map<uint8_t, int64_t> mount_bonus_map;
	CMountInventory* mi = GetMountInventory(owner);
	if (!mi)
		return;

	const auto& valid_items = CMountInventoryHelper::GetAllowedItems();
	const int total = mi->GetWidth() * mi->GetSize();

	for (int pos = 0; pos < total; ++pos)
	{
		const entt::entity item = mi->Get(pos);
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
