#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/EventDispatcher.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ViewSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/services/SpatialService.hpp"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/item.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/MountInventory.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/utils.h"
#include <Core/Logging.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
entt::dispatcher g_dispatcher;

namespace {
int checks = 0;
bool expectDirty = true;
void Check(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected engine service"); }
struct Packet { uint8_t kind, pos, to; TQuickslot slot; };
std::vector<Packet> packets;
std::function<void(entt::entity)> onPacket;
struct Item { entt::entity owner; uint8_t type, subType; };
std::map<std::pair<uint8_t, uint16_t>, entt::entity> inventory;
void Send(entt::entity owner, Packet packet) {
    Check(g_registry.valid(owner), "packet sent to stale entity");
    if (expectDirty) Check(g_registry.all_of<ecs::DirtyTag>(owner), "packet before dirty publication");
    packets.push_back(packet);
    const auto callback = onPacket;
    if (callback) callback(owner);
}
entt::entity Reset() {
    g_registry.clear(); packets.clear(); inventory.clear(); onPacket = {}; expectDirty = true;
    return g_registry.create();
}
TQuickslot Read(entt::entity owner, uint8_t pos) {
    TQuickslot slot {}; Check(InventorySystem::GetQuickslot(owner, pos, slot), "read failed"); return slot;
}
bool Same(TQuickslot a, TQuickslot b) { return a.type == b.type && a.pos == b.pos; }
}

std::shared_ptr<spdlog::logger> logging::GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("quickslot-test"); return logger;
}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger() { return logging::GetLogger(); }
namespace NetworkSyncSystem {
void SendQuickslotAdd(entt::entity e, uint8_t pos, TQuickslot slot) { Send(e, {0, pos, 0, slot}); }
void SendQuickslotDelete(entt::entity e, uint8_t pos) { Send(e, {1, pos, 0, {}}); }
void SendQuickslotSwap(entt::entity e, uint8_t from, uint8_t to) { Send(e, {2, from, to, {}}); }
}
namespace ItemSystem {
bool IsValidItem(entt::entity item) { return g_registry.valid(item) && g_registry.all_of<Item>(item); }
entt::entity GetItem(entt::entity owner, TItemPos pos) {
    Check(g_registry.valid(owner), "lookup with stale owner");
    auto it = inventory.find({pos.window_type, pos.cell}); return it == inventory.end() ? entt::null : it->second;
}
entt::entity GetItemOwner(entt::entity item) { return g_registry.get<Item>(item).owner; }
uint8_t GetItemType(entt::entity item) { return g_registry.get<Item>(item).type; }
uint8_t GetItemSubType(entt::entity item) { return g_registry.get<Item>(item).subType; }
}

namespace {
void Basic() {
    const auto owner = Reset();
    Check(Read(owner, 0).type == QUICKSLOT_TYPE_NONE && !g_registry.all_of<ecs::QuickSlots>(owner), "read created state");
    Check(InventorySystem::SetQuickslot(owner, 0, {QUICKSLOT_TYPE_COMMAND, 3}), "set rejected");
    Check(Same(Read(owner, 0), {QUICKSLOT_TYPE_COMMAND, 3}) && packets.size() == 1, "set state/packet mismatch");
    Check(InventorySystem::SwapQuickslot(owner, 0, 1), "swap rejected");
    Check(Read(owner, 0).type == 0 && Read(owner, 1).pos == 3 && packets.back().kind == 2, "swap mismatch");
    Check(InventorySystem::DelQuickslot(owner, 1), "delete rejected");
    Check(Read(owner, 1).type == 0 && packets.back().kind == 1, "delete mismatch");
}
void DuplicatesAndValidation() {
    auto owner = Reset();
    auto& slots = g_registry.emplace<ecs::QuickSlots>(owner);
    slots.slots[1] = slots.slots[2] = {QUICKSLOT_TYPE_COMMAND, 9};
    onPacket = [](entt::entity e) {
        Check(Read(e, 1).type == 0 && Read(e, 2).type == 0 && Read(e, 3).pos == 9, "partial state published");
    };
    Check(InventorySystem::SetQuickslot(owner, 3, {QUICKSLOT_TYPE_COMMAND, 9}), "duplicate move rejected");
    Check(packets.size() == 3 && packets[0].kind == 1 && packets[1].kind == 1 && packets[2].kind == 0, "duplicate packet order");
    onPacket = {}; packets.clear();
    const auto before = slots.slots;
    for (unsigned pos = 0; pos <= 255; ++pos) {
        if (pos < QUICKSLOT_MAX_NUM) continue;
        Check(!InventorySystem::SetQuickslot(owner, pos, {QUICKSLOT_TYPE_COMMAND, 9}), "invalid destination accepted");
        Check(!InventorySystem::DelQuickslot(owner, pos), "invalid delete accepted");
        Check(!InventorySystem::SwapQuickslot(owner, 0, pos), "invalid swap accepted");
    }
    for (unsigned type = QUICKSLOT_TYPE_MAX_NUM; type <= 255; ++type)
        Check(!InventorySystem::SetQuickslot(owner, 0, {static_cast<uint8_t>(type), 9}), "invalid type accepted");
    Check(!InventorySystem::SetQuickslot(owner, 0, {QUICKSLOT_TYPE_NONE, 9}), "NONE assignment accepted");
    for (size_t i = 0; i < before.size(); ++i) Check(Same(before[i], Read(owner, i)), "rejection mutated slots");
    Check(packets.empty(), "rejection sent packet");
}
void SyncAndLifetime() {
    auto owner = Reset();
    auto& slots = g_registry.emplace<ecs::QuickSlots>(owner);
    slots.slots[1] = slots.slots[2] = {QUICKSLOT_TYPE_ITEM, 8};
    InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, 264, 255);
    Check(packets.empty() && Read(owner, 1).pos == 8, "large cell wrapped");
    InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, 8, 8);
    Check(packets.empty(), "no-op sent packet");
    onPacket = [](entt::entity e) { Check(Read(e, 1).type == 0 && Read(e, 2).type == 0, "sync published partial state"); };
    InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, 8, 255);
    Check(packets.size() == 2, "sync missed duplicate shortcuts");
    owner = Reset();
    auto& state = g_registry.emplace<ecs::QuickSlots>(owner);
    state.slots[1] = state.slots[2] = {QUICKSLOT_TYPE_COMMAND, 7};
    onPacket = [](entt::entity e) { onPacket = {}; InventorySystem::SetQuickslot(e, 3, {QUICKSLOT_TYPE_COMMAND, 19}); };
    InventorySystem::SetQuickslot(owner, 3, {QUICKSLOT_TYPE_COMMAND, 7});
    Check(Read(owner, 3).pos == 19 && packets.size() == 2 && packets.back().slot.pos == 19, "old publication overwrote nested change");
    owner = Reset();
    g_registry.emplace<ecs::QuickSlots>(owner).slots[1] = {QUICKSLOT_TYPE_COMMAND, 7};
    entt::entity replacement {entt::null};
    onPacket = [&](entt::entity e) { g_registry.destroy(e); replacement = g_registry.create(); };
    InventorySystem::SetQuickslot(owner, 3, {QUICKSLOT_TYPE_COMMAND, 7});
    Check(packets.size() == 1 && replacement != owner && !g_registry.all_of<ecs::QuickSlots>(replacement), "recycled owner was mutated");
    Check(!InventorySystem::DelQuickslot(owner, 0) && !InventorySystem::SetQuickslot(entt::null, 0, {QUICKSLOT_TYPE_COMMAND, 2}), "stale owner accepted");
}

void ValueRanges() {
    const auto owner = Reset();
    for (unsigned type = 0; type <= 255; ++type) {
        for (unsigned pos = 0; pos <= 255; ++pos) {
            const TQuickslot candidate {static_cast<uint8_t>(type), static_cast<uint8_t>(pos)};
            const TItemPos location(INVENTORY, pos);
            const bool expected = type == QUICKSLOT_TYPE_COMMAND ||
                (type == QUICKSLOT_TYPE_SKILL && pos < SKILL_MAX_NUM) ||
                (type == QUICKSLOT_TYPE_ITEM && (location.IsDefaultInventoryPosition() || location.IsBeltInventoryPosition()))
#ifdef ENABLE_EXTRA_INVENTORY
                || (type == QUICKSLOT_TYPE_ITEM_EXTRA && pos < EXTRA_INVENTORY_MAX_NUM)
#endif
                ;
            const auto previous = Read(owner, 0);
            const auto packetCount = packets.size();
            Check(InventorySystem::SetQuickslot(owner, 0, candidate) == expected, "value range acceptance mismatch");
            Check(Same(Read(owner, 0), expected ? candidate : previous), "value range changed wrong state");
            Check(packets.size() == packetCount + (expected ? 1 : 0), "value range packet mismatch");
        }
    }
}

void ClientValidation() {
    const auto owner = Reset();
    Check(!InventorySystem::SetQuickslotFromClient(owner, 0, {QUICKSLOT_TYPE_ITEM, 3}), "missing client item accepted");
    const auto item = g_registry.create();
    auto& data = g_registry.emplace<Item>(item, Item {owner, ITEM_USE, USE_POTION});
    inventory[{INVENTORY, 3}] = item;
    Check(InventorySystem::SetQuickslotFromClient(owner, 0, {QUICKSLOT_TYPE_ITEM, 3}), "inventory potion rejected");
    data.type = ITEM_QUEST;
    Check(InventorySystem::SetQuickslotFromClient(owner, 0, {QUICKSLOT_TYPE_ITEM, 3}), "quest shortcut rejected");
    const auto before = packets.size();
    data.type = ITEM_WEAPON;
    Check(!InventorySystem::SetQuickslotFromClient(owner, 0, {QUICKSLOT_TYPE_ITEM, 3}), "weapon shortcut accepted");
    data.type = ITEM_USE; data.owner = g_registry.create();
    Check(!InventorySystem::SetQuickslotFromClient(owner, 0, {QUICKSLOT_TYPE_ITEM, 3}), "foreign item accepted");
    Check(packets.size() == before && Read(owner, 0).pos == 3, "rejected client item mutated quickslots");
    data.owner = owner;
#ifdef ENABLE_EXTRA_INVENTORY
    inventory[{EXTRA_INVENTORY, 3}] = item;
    for (const uint8_t type : {static_cast<uint8_t>(QUICKSLOT_TYPE_ITEM_EXTRA), uint8_t{12}}) {
        const TQuickslot incoming {type, 3};
        Check(!InventorySystem::SetQuickslotFromClient(owner, 0, incoming), "extra potion restriction bypassed");
        data.type = ITEM_QUEST;
        Check(InventorySystem::SetQuickslotFromClient(owner, 0, incoming), "extra quest/alias rejected");
        Check(incoming.type == type && Read(owner, 0).type == QUICKSLOT_TYPE_ITEM_EXTRA, "alias mutated caller or not normalized");
        data.type = ITEM_USE;
    }
#endif
    g_registry.destroy(item);
    const auto replacement = g_registry.create();
    g_registry.emplace<Item>(replacement, Item {owner, ITEM_USE, USE_POTION});
    Check(!InventorySystem::SetQuickslotFromClient(owner, 0, {QUICKSLOT_TYPE_ITEM, 3}), "recycled item accepted through stale slot");
    Check(!InventorySystem::SetQuickslotFromClient(entt::null, 0, {QUICKSLOT_TYPE_ITEM, 3}), "null client owner accepted");
}

void HydrationAndRelocation() {
    const auto owner = Reset();
    TQuickslot saved[QUICKSLOT_MAX_NUM] {};
    saved[0] = saved[4] = {QUICKSLOT_TYPE_COMMAND, 8};
    saved[1] = {255, 9}; saved[2] = {QUICKSLOT_TYPE_ITEM, 5};
    const auto loaded = InventorySystem::MakeQuickSlots(saved);
    Check(loaded.slots[0].type == 0 && loaded.slots[1].type == 0 && loaded.slots[2].pos == 5 && loaded.slots[4].pos == 8,
        "hydration did not normalize invalid/duplicate entries");
    g_registry.emplace<ecs::QuickSlots>(owner, loaded);
    expectDirty = false;
    onPacket = [&](entt::entity e) {
        for (size_t i = 0; i < loaded.slots.size(); ++i) Check(Same(Read(e, i), loaded.slots[i]), "login published partial state");
    };
    InventorySystem::SendQuickslots(owner);
    Check(packets.size() == QUICKSLOT_MAX_NUM && !g_registry.all_of<ecs::DirtyTag>(owner), "login publication mutated state or omitted slots");
    for (size_t i = 0; i < loaded.slots.size(); ++i)
        Check(packets[i].pos == i && packets[i].kind == (loaded.slots[i].type ? 0 : 1), "login packet mismatch");
    onPacket = {}; expectDirty = true; packets.clear();
    InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, 5, 10);
    Check(Read(owner, 2).pos == 10 && packets.size() == 1 && packets[0].kind == 0, "relocation packet missing");
    auto& state = g_registry.get<ecs::QuickSlots>(owner);
    state.slots[3] = {QUICKSLOT_TYPE_ITEM, 10};
    state.slots[6] = {QUICKSLOT_TYPE_ITEM, 11};
    InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, 10, 11);
    Check(Read(owner, 2).type == 0 && Read(owner, 3).pos == 11 && Read(owner, 6).type == 0, "relocation left duplicates");
    // Use the same value-copy reads as the character-save call site.
    for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i) InventorySystem::GetQuickslot(owner, i, saved[i]);
    const auto roundtrip = InventorySystem::MakeQuickSlots(saved);
    for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM; ++i) Check(Same(roundtrip.slots[i], Read(owner, i)), "saved ECS state changed on reload");
}
}
// Link the entire production inventory source. Services outside quickslots
// fail immediately so tests cannot accidentally use a partial engine mock.
int MAX(int, int) { Unexpected(); }
int MIN(int, int) { Unexpected(); }
int number_ex(int, int, char const *, int) { Unexpected(); }
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, char const *, ...) { Unexpected(); }
DESC * ecs::PlayerRuntime::GetDesc(entt::entity) { Unexpected(); }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(entt::entity, entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(entt::entity, entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetItem(entt::entity, SItemPos, entt::entity, bool) { Unexpected(); }
void ecs::PlayerRuntime::SetWear(entt::entity, uint8_t, entt::entity) { Unexpected(); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { Unexpected(); }
SECTREE * ecs::PlayerRuntime::GetSectree(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetPart(entt::entity, uint8_t, uint16_t) { Unexpected(); }
uint16_t ecs::PlayerRuntime::GetOriginalPart(entt::entity, uint8_t) { Unexpected(); }
uint16_t ecs::PlayerRuntime::GetRuneEffect(entt::entity) { Unexpected(); }
void ecs::PointSystem::ComputeBattlePoints(entt::entity) { Unexpected(); }
void ecs::PointSystem::ApplyPoint(entt::entity, uint8_t, int) { Unexpected(); }
SECTREE * CEntity::GetSectree()const { Unexpected(); }
SECTREE * SECTREE_MANAGER::Get(int, int, int) { Unexpected(); }
void MountSystem::UpdateMountSkin(entt::entity) { Unexpected(); }
void MountSystem::MountUnsummon(entt::entity, entt::entity) { Unexpected(); }
void MountSystem::UpdatePetSkin(entt::entity) { Unexpected(); }
void MountSystem::MountSummon(entt::entity, entt::entity) { Unexpected(); }
CMountInventory * MountSystem::GetMountInventory(entt::entity) { Unexpected(); }
entt::entity ItemSystem::GetWearItem(entt::entity, uint8_t) { Unexpected(); }
bool ItemSystem::IsDragonSoulItem(entt::entity) { Unexpected(); }
bool ItemSystem::IsExtraItem(entt::entity) { Unexpected(); }
bool ItemSystem::IsRideItem(entt::entity) { Unexpected(); }
bool ItemSystem::IsMountItem(entt::entity) { Unexpected(); }
bool ItemSystem::IsRuneItem(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemID(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemVID(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemVnum(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemOriginalVnum(entt::entity) { Unexpected(); }
TItemExtraProto * ItemSystem::GetItemExtraProto(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemSIGVnum(entt::entity) { Unexpected(); }
int ItemSystem::GetItemValue(entt::entity, uint32_t) { Unexpected(); }
char const * ItemSystem::GetItemName(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemWearFlag(entt::entity) { Unexpected(); }
int ItemSystem::FindEquipCell(entt::entity, entt::entity, int) { Unexpected(); }
SItemTable const * ItemSystem::GetItemProto(entt::entity) { Unexpected(); }
bool ItemSystem::DestroyItemEntityEcs(entt::entity, char const *) { Unexpected(); }
short ItemSystem::GetItemLockedAttr(entt::entity) { Unexpected(); }
int ItemSystem::GetItemAccessorySocketGrade(entt::entity) { Unexpected(); }
bool ItemSystem::IsAccessoryForSocket(entt::entity) { Unexpected(); }
void ItemSystem::StartUniqueExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StopUniqueExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StartTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StopTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StartAccessorySocketExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StopAccessorySocketExpireEvent(entt::entity) { Unexpected(); }
ecs::ItemEvents & ItemSystem::GetItemEvents(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemSocket(entt::entity, int) { Unexpected(); }
TPlayerItemAttribute ItemSystem::GetItemAttribute(entt::entity, int) { Unexpected(); }
int ItemSystem::GetItemAttributeType(entt::entity, int) { Unexpected(); }
bool ItemSystem::SetItemSocket(entt::entity, int, uint32_t, bool) { Unexpected(); }
void ItemSystem::ClearMountAttributeAndAffect(entt::entity) { Unexpected(); }
void ItemSystem::SaveItem(entt::entity) { Unexpected(); }
void ItemSystem::SetItemOwnerEntity(entt::entity, entt::entity) { Unexpected(); }
void ItemSystem::SetItemLastOwnerPID(entt::entity, uint32_t) { Unexpected(); }
void ItemSystem::SetItemOwnershipPID(entt::entity, uint32_t) { Unexpected(); }
bool ItemSystem::SetItemWindow(entt::entity, uint8_t) { Unexpected(); }
bool ItemSystem::SetItemCell(entt::entity, entt::entity, uint16_t) { Unexpected(); }
uint8_t ItemSystem::GetItemWindow(entt::entity) { Unexpected(); }
uint16_t ItemSystem::GetItemCell(entt::entity) { Unexpected(); }
bool ItemSystem::IsItemEquipped(entt::entity) { Unexpected(); }
void NetworkSyncSystem::UpdatePacket(entt::entity) { Unexpected(); }
void NetworkSyncSystem::UpdateItemOnTitleName(entt::registry &, entt::entity, bool) { Unexpected(); }
void ecs::ViewSystem::ViewCleanup(entt::entity) { Unexpected(); }
void ecs::ViewSystem::PacketView(entt::entity, void const *, int, entt::entity) { Unexpected(); }
void CItem::Save() { Unexpected(); }
int CItem::GetValue(uint32_t) { Unexpected(); }
SItemTable * ITEM_MANAGER::GetTable(uint32_t) { Unexpected(); }
CSpecialItemGroup const * ITEM_MANAGER::GetSpecialItemGroup(uint32_t) { Unexpected(); }
CSpecialAttrGroup const * ITEM_MANAGER::GetSpecialAttrGroup(uint32_t) { Unexpected(); }
bool CMountInventory::RemoveByItem(entt::entity, bool) { Unexpected(); }
bool DSManager::ActivateDragonSoul(entt::entity) { Unexpected(); }
bool DSManager::DeactivateDragonSoul(entt::entity, bool) { Unexpected(); }
bool ecs::SpatialService::InsertEntity(entt::registry &, entt::entity, uint32_t, int, int, int) { Unexpected(); }
void ecs::SpatialService::RemoveEntity(entt::registry &, entt::entity) { Unexpected(); }
void ecs::SpatialService::UpdateSectree(entt::registry &, entt::entity) { Unexpected(); }
void intrusive_ptr_release(event*) { Unexpected(); }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { Unexpected(); }
void event_cancel(LPEVENT*) { Unexpected(); }
EVENTFUNC(ownership_event) { Unexpected(); }
const int aiAccessorySocketEffectivePct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1] = {};
int passes_per_sec = 25;

int main() {
    try { Basic(); DuplicatesAndValidation(); SyncAndLifetime(); ValueRanges(); ClientValidation(); HydrationAndRelocation();
        std::cout << "Quickslot checks passed: " << checks << '\n'; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
