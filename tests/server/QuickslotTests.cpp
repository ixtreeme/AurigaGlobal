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
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/new_switchbot.h"
#include "../../SRC/Server/GameServer/dragon_soul_table.h"
#include "../../SRC/Server/GameServer/ecs/components/transform_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/spatial_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/events.hpp"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/MountInventory.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/utils.h"
#include <Core/Logging.hpp>
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/components/status_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/character_runtime_components.hpp"
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
struct PlacementActor { DESC* desc = nullptr; uint32_t pid = 37; };
struct PlacementMeta {
    TItemTable proto {};
    uint8_t category = 0;
    uint16_t dragonBase = 0;
    bool extra = false, dragon = false, rune = false;
};
std::vector<std::unique_ptr<DESC>> descriptors;
std::function<void(entt::entity)> onSave;
std::function<void()> onCancel, onRegistration;
std::function<void(entt::entity, uint8_t, TItemPos)> onStoragePacket;
std::function<void(const char*)> onService;
int saves = 0, storagePackets = 0, registrations = 0;
void Service(const char* name) { if (onService) { auto f = onService; f(name); } }
PlacementMeta& Meta(entt::entity e) {
    Check(g_registry.valid(e) && g_registry.all_of<PlacementMeta>(e), "stale item metadata read");
    return g_registry.get<PlacementMeta>(e);
}
bool IsPlacement(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<PlacementMeta>(e); }
entt::entity RawOwner(entt::entity e) {
    Check(g_registry.valid(e), "stale owner read");
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(e);
    return owner ? owner->owner : entt::null;
}
entt::entity PlacementOwner() {
    auto e = g_registry.create();
    auto desc = std::make_unique<DESC>(); desc->SetEntity(e);
    g_registry.emplace<PlacementActor>(e, desc.get());
    g_registry.emplace<ecs::PlayerID>(e).pid = 37;
    descriptors.push_back(std::move(desc));
    g_registry.emplace<ecs::CharacterPoints>(e).base.envanter = INT32_MAX;
    return e;
}
entt::entity PlacementItem(uint8_t size = 1) {
    auto e = g_registry.create();
    auto& proto = g_registry.emplace<PlacementMeta>(e).proto;
    proto.dwVnum = 100; proto.bSize = size; proto.bType = ITEM_USE; proto.bSubType = USE_POTION;
    proto.cLimitTimerBasedOnWearIndex = -1;
    auto& id = g_registry.emplace<ecs::ItemIdentity>(e); id.id = entt::to_integral(e) + 10; id.vnum = 100;
    g_registry.emplace<ecs::ItemCount>(e, 7);
    g_registry.emplace<ecs::ItemSockets>(e);
    g_registry.emplace<ecs::ItemAttributes>(e);
    return e;
}
void Send(entt::entity owner, Packet packet) {
    Check(g_registry.valid(owner), "packet sent to stale entity");
    if (expectDirty) Check(g_registry.all_of<ecs::DirtyTag>(owner), "packet before dirty publication");
    packets.push_back(packet);
    const auto callback = onPacket;
    if (callback) callback(owner);
}
entt::entity Reset() {
    onSave = {}; onCancel = onRegistration = {}; onStoragePacket = {}; onService = {};
    g_registry.clear(); descriptors.clear(); saves = storagePackets = registrations = 0;
    packets.clear(); inventory.clear(); onPacket = {}; expectDirty = true;
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
bool IsValidItem(entt::entity item) { return g_registry.valid(item) && g_registry.any_of<Item, ecs::ItemIdentity>(item); }
entt::entity GetItem(entt::entity owner, TItemPos pos) {
    Check(g_registry.valid(owner), "lookup with stale owner");
    auto it = inventory.find({pos.window_type, pos.cell}); return it == inventory.end() ? entt::null : it->second;
}
entt::entity GetItemOwner(entt::entity item) { return IsPlacement(item) ? RawOwner(item) : g_registry.get<Item>(item).owner; }
uint8_t GetItemType(entt::entity item) { return IsPlacement(item) ? Meta(item).proto.bType : g_registry.get<Item>(item).type; }
uint8_t GetItemSubType(entt::entity item) { return IsPlacement(item) ? Meta(item).proto.bSubType : g_registry.get<Item>(item).subType; }
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

namespace {
int extraUnlock = 0;
void InventoryGuards()
{
    auto owner = Reset();
    Check(!InventorySystem::CanHandleItems(entt::null), "null item handler accepted");
    Check(InventorySystem::CanHandleItems(owner), "entity-only handler rejected");
    auto& status = g_registry.emplace<ecs::StatusFlags>(owner);
    status.isObserverMode = true;
    Check(!InventorySystem::CanHandleItems(owner), "observer accepted");
    Check(InventorySystem::CanHandleItems(owner, false, true), "observer override ignored");
    status.isObserverMode = false;
    InventorySystem::SetRefineMode(owner, 37);
    Check(InventorySystem::IsRefining(owner) && !InventorySystem::CanHandleItems(owner), "refine gate ignored");
    Check(InventorySystem::CanHandleItems(owner, true), "refine override ignored");
    auto npc = g_registry.create();
    InventorySystem::SetRefineNPC(owner, npc);
    Check(InventorySystem::GetRefineNPC(owner) == npc, "refine NPC not entity-native");
    g_registry.destroy(npc);
    const auto replacement = g_registry.create();
    Check(replacement != npc && InventorySystem::GetRefineNPC(owner) == entt::null, "stale refine NPC accepted");
    InventorySystem::ClearRefineMode(owner);
    Check(!InventorySystem::IsRefining(owner) && InventorySystem::GetRefineScrollCell(owner) == 37,
          "closing mode lost the scroll consumed by DoRefineWithScroll");
    Check(InventorySystem::CanHandleItems(owner), "closed refine still blocks inventory");
    auto& cube = g_registry.emplace<ecs::CubeWindowComponent>(owner);
    cube.npc = replacement;
    Check(!InventorySystem::CanHandleItems(owner, true, true), "cube bypassed");
    cube.npc = entt::null;
    auto& dragonSoul = g_registry.emplace<ecs::DragonSoulRuntimeStateComponent>(owner);
    dragonSoul.refineWindowOpener = replacement;
    Check(!InventorySystem::CanHandleItems(owner), "dragon soul window ignored");
    g_registry.destroy(replacement);
    Check(InventorySystem::CanHandleItems(owner), "stale window still blocks inventory");
#ifdef __ATTR_TRANSFER_SYSTEM__
    auto& transfer = g_registry.emplace<ecs::AttrTransferWindowComponent>(owner);
    transfer.busy = true;
    Check(!InventorySystem::CanHandleItems(owner), "busy attribute transfer ignored");
    transfer.busy = false;
#endif
#ifdef ENABLE_ACCE_SYSTEM
    auto& acce = g_registry.emplace<ecs::AcceWindowComponent>(owner);
    acce.combinationOpen = true;
    Check(!InventorySystem::CanHandleItems(owner), "accessory combination ignored");
    acce.combinationOpen = false;
    acce.absorptionOpen = true;
    Check(!InventorySystem::CanHandleItems(owner), "accessory absorption ignored");
    acce.absorptionOpen = false;
#endif
    auto& events = g_registry.emplace<ecs::LegacyCharEvents>(owner);
    events.warp = LPEVENT(new EVENT);
    Check(!InventorySystem::CanHandleItems(owner, true, true), "warp bypassed");
    events.warp.reset();
    Check(InventorySystem::CanHandleItems(owner), "closed windows still block inventory");
    g_registry.destroy(owner);
    InventorySystem::SetRefineMode(owner, 1);
    InventorySystem::SetRefineNPC(owner, g_registry.create());
    InventorySystem::ClearRefineMode(owner);
    Check(!InventorySystem::IsRefining(owner) && InventorySystem::GetRefineScrollCell(owner) == -1,
          "stale owner acquired state");
    Check(!InventorySystem::CanHandleItems(owner), "stale item handler accepted");
}
void InventoryGrids()
{
    const auto owner = Reset();
    extraUnlock = 0;
    g_registry.emplace<ecs::MainInventoryRuntimeComponent>(owner);
    auto& points = g_registry.emplace<ecs::CharacterPoints>(owner);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    points.base.envanter = 0;
    Check(InventorySystem::GetInventorySize(owner) == 90, "base inventory size wrong");
    points.base.envanter = INT32_MAX;
    Check(InventorySystem::GetInventorySize(owner) == INVENTORY_MAX_NUM, "extension overflow");
    points.base.envanter = -100;
    Check(InventorySystem::GetInventorySize(owner) == 0, "negative extension accepted");
    points.base.envanter = INT32_MAX;
#endif
    auto& main = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
    for (unsigned cell = 0; cell <= UINT16_MAX; ++cell)
    {
        const TItemPos pos(INVENTORY, static_cast<uint16_t>(cell));
        const bool expected = cell < INVENTORY_MAX_NUM || pos.IsBeltInventoryPosition();
        Check(InventorySystem::IsEmptyItemGrid(owner, pos, 1) == expected, "main slot bounds");
        Check(!InventorySystem::IsEmptyItemGrid(owner, pos, 0), "zero-height item accepted");
    }
    for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
        for (uint8_t size = 1; size <= 10; ++size)
        {
            const int last = cell + (size - 1) * INVENTORY_PAGE_COLUMN;
            const bool expected = last < INVENTORY_MAX_NUM && last / INVENTORY_PAGE_SIZE == cell / INVENTORY_PAGE_SIZE;
            Check(InventorySystem::IsEmptyItemGrid(owner, TItemPos(INVENTORY, cell), size) == expected,
                  "main item crosses page");
        }
    main.itemGrid[5] = 1;
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(INVENTORY, 0), 2), "secondary main cell ignored");
    Check(InventorySystem::IsEmptyItemGrid(owner, TItemPos(INVENTORY, 0), 2, 0), "move exception ignored");
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(INVENTORY, 0), 2, INT32_MAX), "invalid exception accepted");
    main.itemGrid[5] = 0;
    auto& ds = g_registry.emplace<ecs::DragonSoulInventoryComponent>(owner);
    ds.itemGrid[DRAGON_SOUL_BOX_COLUMN_NUM] = 7;
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(DRAGON_SOUL_INVENTORY, 0), 2),
          "dragon soul checked empty main grid instead of its own");
    Check(InventorySystem::IsEmptyItemGrid(owner, TItemPos(DRAGON_SOUL_INVENTORY, 0), 2, 6),
          "dragon soul exception ignored");
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(DRAGON_SOUL_INVENTORY, DRAGON_SOUL_INVENTORY_MAX_NUM), 1),
          "dragon soul out of bounds");
#ifdef ENABLE_EXTRA_INVENTORY
    auto& extra = g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(owner);
    const int cell = EXTRA_INVENTORY_CATEGORY_MAX_NUM * 2;
    extra.itemGrid[cell + EXTRA_INVENTORY_PAGE_COLUMN] = cell + 1;
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(EXTRA_INVENTORY, cell), 2), "large extra cell truncated");
    Check(InventorySystem::IsEmptyItemGrid(owner, TItemPos(EXTRA_INVENTORY, cell), 2, cell), "large exception truncated");
    extra.itemGrid.fill(0);
    for (int unlock : {-1, 0, 1, 14, INT32_MAX})
    {
        extraUnlock = unlock;
        for (int pos = 0; pos < EXTRA_INVENTORY_MAX_NUM; ++pos)
        {
            const int relative = pos % EXTRA_INVENTORY_CATEGORY_MAX_NUM;
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
            const int unlocked = static_cast<int>(std::clamp<int64_t>(int64_t(unlock) * 5, 0, 70));
            const bool expected = relative < 110 + unlocked;
#else
            const bool expected = true;
#endif
            Check(InventorySystem::IsEmptyItemGrid(owner, TItemPos(EXTRA_INVENTORY, pos), 1) == expected,
                  "extra category unlock bounds");
        }
    }
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(EXTRA_INVENTORY, EXTRA_INVENTORY_MAX_NUM), 1),
          "extra inventory end accepted");
#endif
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(EQUIPMENT, 0), 1), "equipment accepted as storage destination");
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(SAFEBOX, 0), 1), "foreign window accepted");
    Check(!InventorySystem::HasBeltItems(owner), "empty belt reported occupied");
    const auto item = g_registry.create();
    g_registry.emplace<Item>(item, Item {owner, ITEM_USE, USE_POTION});
    main.items[BELT_INVENTORY_SLOT_END - 1] = item;
    Check(InventorySystem::HasBeltItems(owner), "large belt slot truncated");
    g_registry.destroy(item);
    Check(!InventorySystem::HasBeltItems(owner), "stale belt item accepted");
    Check(!InventorySystem::IsEmptyItemGrid(owner, TItemPos(INVENTORY, BELT_INVENTORY_SLOT_START), 2),
          "multi-cell item accepted in belt");
    g_registry.destroy(owner);
    Check(InventorySystem::GetInventorySize(owner) == 0 &&
          !InventorySystem::IsEmptyItemGrid(owner, TItemPos(INVENTORY, 0), 1), "stale inventory accepted");
}
}
int32_t ecs::QuestSystem::GetFlag(entt::entity owner, std::string_view flag)
{
    Check(g_registry.valid(owner) && flag.starts_with("lock_extra.cat"), "unexpected quest query");
    return extraUnlock;
}

// Link the entire production inventory source. Only external services are
// doubled; placement, removal, unequip, grid queries and quickslots are real.
DESC::DESC() { m_sock = 1; m_entity = entt::null; m_accountTable = {}; }
DESC::~DESC() {}
void DESC::Destroy() { Unexpected(); }
void DESC::SetPhase(int) { Unexpected(); }
CLIENT_DESC::CLIENT_DESC() {}
CLIENT_DESC::~CLIENT_DESC() {}
void CLIENT_DESC::Destroy() { Unexpected(); }
void CLIENT_DESC::SetPhase(int) { Unexpected(); }
CInputProcessor::CInputProcessor() {}
bool CInputProcessor::Process(DESC*, const void*, int, int&) { Unexpected(); }
void CInputProcessor::Handshake(DESC*, const char*) { Unexpected(); }
CInputHandshake::CInputHandshake() {}
CInputHandshake::~CInputHandshake() {}
int CInputHandshake::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputLogin::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputMain::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputDead::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputDB::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
bool CInputDB::Process(DESC*, const void*, int, int&) { Unexpected(); }
CInputP2P::CInputP2P() {}
CInputAuth::CInputAuth() {}
int CInputP2P::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
int CInputAuth::Analyze(DESC*, uint8_t, const char*) { Unexpected(); }
CPacketInfo::CPacketInfo() : m_pCurrentPacket(nullptr), m_dwStartTime(0) {}
CPacketInfo::~CPacketInfo() {}
CPacketInfoCG::CPacketInfoCG() {}
CPacketInfoGG::CPacketInfoGG() {}
CPacketInfoCG::~CPacketInfoCG() {}
CPacketInfoGG::~CPacketInfoGG() {}
Cipher::Cipher() : activated_(false), encoder_(nullptr), decoder_(nullptr), key_agreement_(nullptr) {}
Cipher::~Cipher() {}

void DESC::Packet(const void* data, int size) {
    const auto header = *static_cast<const uint8_t*>(data);
    TItemPos pos;
    if (header == HEADER_GC_ITEM_SET) {
        Check(size == sizeof(TPacketGCItemSet), "item set wire size");
        const auto& p = *static_cast<const TPacketGCItemSet*>(data); pos = p.Cell;
        Check(p.count == 7 && p.vnum == 100, "item set payload");
    } else {
        Check(header == HEADER_GC_ITEM_DEL && size == sizeof(TPacketGCItemDelDeprecated), "item del wire size");
        pos = static_cast<const TPacketGCItemDelDeprecated*>(data)->Cell;
    }
    ++storagePackets;
    if (onStoragePacket) { auto f = onStoragePacket; f(GetEntity(), header, pos); }
}
DSManager::DSManager() = default;
DSManager::~DSManager() = default;
DragonSoulTable::~DragonSoulTable() = default;
uint16_t DSManager::GetBasePosition(entt::entity e) const { return Meta(e).dragonBase; }
#ifdef ENABLE_SWITCHBOT
CSwitchbotManager::CSwitchbotManager() = default;
CSwitchbotManager::~CSwitchbotManager() = default;
void CSwitchbotManager::RegisterItem(uint32_t, uint32_t, uint16_t) {
    ++registrations; if (onRegistration) { auto f = onRegistration; f(); }
}
void CSwitchbotManager::UnregisterItem(uint32_t, uint16_t) {
    --registrations; if (onRegistration) { auto f = onRegistration; f(); }
}
#endif
bool ecs::PlayerRuntime::IsValid(entt::entity e) { return g_registry.valid(e); }
std::shared_ptr<CSafebox> SafeboxSystem::Get(entt::entity, uint8_t) { Unexpected(); }
entt::entity CSafebox::Get(unsigned int) const { Unexpected(); }
entt::entity CSafebox::Remove(unsigned int) { Unexpected(); }
uint8_t ItemSystem::GetItemSize(entt::entity e) { return Meta(e).proto.bSize; }
uint8_t ItemSystem::GetItemExtraCategory(entt::entity e) { return Meta(e).category; }
uint32_t ItemSystem::GetItemCount(entt::entity e) { return g_registry.get<ecs::ItemCount>(e).count; }
int ItemSystem::GetItemFlags(entt::entity e) { return Meta(e).proto.dwFlags; }
uint32_t ItemSystem::GetItemAntiFlag(entt::entity e) { return Meta(e).proto.dwAntiFlags; }
short ItemSystem::GetItemLockedAttributeIndex(entt::entity e) { Meta(e); return -1; }
int MAX(int a, int b) { return std::max(a, b); }
int MIN(int a, int b) { return std::min(a, b); }
int number_ex(int, int, char const *, int) { Unexpected(); }
void ecs::ChatSystem::SendNew(entt::entity, uint8_t, uint32_t, char const *, ...) { Unexpected(); }
DESC * ecs::PlayerRuntime::GetDesc(entt::entity e) { return g_registry.get<PlacementActor>(e).desc; }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e) { return g_registry.get<PlacementActor>(e).pid; }
void ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(entt::entity, entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(entt::entity, entt::entity) { Service("buff-remove"); }
void ecs::PlayerRuntime::SetItem(entt::entity, SItemPos, entt::entity, bool) { Unexpected(); }
void ecs::PlayerRuntime::SetWear(entt::entity, uint8_t, entt::entity) { Unexpected(); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { Unexpected(); }
SECTREE * ecs::PlayerRuntime::GetSectree(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetPart(entt::entity, uint8_t, uint16_t) { Unexpected(); }
uint16_t ecs::PlayerRuntime::GetOriginalPart(entt::entity, uint8_t) { Unexpected(); }
uint16_t ecs::PlayerRuntime::GetRuneEffect(entt::entity) { Unexpected(); }
void ecs::PointSystem::ComputeBattlePoints(entt::entity) { Service("battle-points"); }
void ecs::PointSystem::ApplyPoint(entt::entity, uint8_t, int) { Unexpected(); }
SECTREE * CEntity::GetSectree()const { Unexpected(); }
SECTREE * SECTREE_MANAGER::Get(int, int, int) { Unexpected(); }
void MountSystem::UpdateMountSkin(entt::entity) { Unexpected(); }
void MountSystem::MountUnsummon(entt::entity, entt::entity) { Unexpected(); }
void MountSystem::UpdatePetSkin(entt::entity) { Unexpected(); }
void MountSystem::MountSummon(entt::entity, entt::entity) { Unexpected(); }
CMountInventory * MountSystem::GetMountInventory(entt::entity) { Unexpected(); }
entt::entity ItemSystem::GetWearItem(entt::entity e, uint8_t slot) { const auto* inv = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(e); return inv ? inv->items[INVENTORY_MAX_NUM + slot] : entt::null; }
bool ItemSystem::IsDragonSoulItem(entt::entity e) { return Meta(e).dragon; }
bool ItemSystem::IsExtraItem(entt::entity e) { return Meta(e).extra; }
bool ItemSystem::IsRideItem(entt::entity e) { Meta(e); return false; }
bool ItemSystem::IsMountItem(entt::entity e) { Meta(e); return false; }
bool ItemSystem::IsRuneItem(entt::entity e) { return Meta(e).rune; }
uint32_t ItemSystem::GetItemID(entt::entity e) { return g_registry.get<ecs::ItemIdentity>(e).id; }
uint32_t ItemSystem::GetItemVID(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemVnum(entt::entity e) { return g_registry.get<ecs::ItemIdentity>(e).vnum; }
uint32_t ItemSystem::GetItemOriginalVnum(entt::entity) { Unexpected(); }
TItemExtraProto * ItemSystem::GetItemExtraProto(entt::entity e) { Meta(e); return nullptr; }
uint32_t ItemSystem::GetItemSIGVnum(entt::entity) { Unexpected(); }
int ItemSystem::GetItemValue(entt::entity e, uint32_t i) { return Meta(e).proto.alValues[i]; }
char const * ItemSystem::GetItemName(entt::entity e) { Meta(e); return "test-item"; }
uint32_t ItemSystem::GetItemWearFlag(entt::entity) { Unexpected(); }
int ItemSystem::FindEquipCell(entt::entity, entt::entity, int) { Unexpected(); }
SItemTable const * ItemSystem::GetItemProto(entt::entity e) { return &Meta(e).proto; }
bool ItemSystem::DestroyItemEntityEcs(entt::entity, char const *) { Unexpected(); }
short ItemSystem::GetItemLockedAttr(entt::entity) { Unexpected(); }
int ItemSystem::GetItemAccessorySocketGrade(entt::entity) { Unexpected(); }
bool ItemSystem::IsAccessoryForSocket(entt::entity e) { Meta(e); return false; }
void ItemSystem::StartUniqueExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StopUniqueExpireEvent(entt::entity e) { Meta(e); Service("stop-unique"); }
void ItemSystem::StartTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StopTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StartAccessorySocketExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StopAccessorySocketExpireEvent(entt::entity e) { Meta(e); Service("stop-accessory"); }
ecs::ItemEvents & ItemSystem::GetItemEvents(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemSocket(entt::entity e, int i) { return g_registry.get<ecs::ItemSockets>(e).sockets[i]; }
TPlayerItemAttribute ItemSystem::GetItemAttribute(entt::entity e, int i) { return g_registry.get<ecs::ItemAttributes>(e).attrs[i]; }
int ItemSystem::GetItemAttributeType(entt::entity e, int i) { return GetItemAttribute(e, i).bType; }
bool ItemSystem::SetItemSocket(entt::entity e, int i, uint32_t value, bool) { g_registry.get<ecs::ItemSockets>(e).sockets[i] = value; Service("set-socket"); return true; }
void ItemSystem::ClearMountAttributeAndAffect(entt::entity) { Unexpected(); }
void ItemSystem::SaveItem(entt::entity e) { Meta(e); ++saves; if (onSave) { auto f = onSave; f(e); } }
void ItemSystem::SetItemOwnerEntity(entt::entity, entt::entity) { Unexpected(); }
void ItemSystem::SetItemLastOwnerPID(entt::entity, uint32_t) { Unexpected(); }
void ItemSystem::SetItemOwnershipPID(entt::entity, uint32_t) { Unexpected(); }
bool ItemSystem::SetItemWindow(entt::entity, uint8_t) { Unexpected(); }
bool ItemSystem::SetItemCell(entt::entity, entt::entity, uint16_t) { Unexpected(); }
uint8_t ItemSystem::GetItemWindow(entt::entity e) { return g_registry.get<ecs::ItemLocation>(e).window; }
uint16_t ItemSystem::GetItemCell(entt::entity e) { return g_registry.get<ecs::ItemLocation>(e).cell; }
bool ItemSystem::IsItemEquipped(entt::entity e) { Meta(e); const auto* c = g_registry.try_get<ecs::ItemEquipped>(e); return c && c->equipped; }
void NetworkSyncSystem::UpdatePacket(entt::entity) { Service("update-packet"); }
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
void intrusive_ptr_add_ref(event* e) { ++e->ref_count; }
void intrusive_ptr_release(event* e) { if (--e->ref_count == 0) delete e; }
LPEVENT event_create_ex(TEVENTFUNC, event_info_data*, int32_t) { Unexpected(); }
void event_cancel(LPEVENT* e) { e->reset(); if (onCancel) { auto f = onCancel; f(); } }
EVENTFUNC(ownership_event) { Unexpected(); }
const int aiAccessorySocketEffectivePct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1] = {};
int passes_per_sec = 25;


namespace {
template<class F> decltype(auto) Storage(entt::entity owner, uint8_t window, F&& f) {
#ifdef ENABLE_EXTRA_INVENTORY
    if (window == EXTRA_INVENTORY) return f(g_registry.get<ecs::ExtraInventoryRuntimeComponent>(owner));
#endif
#ifdef ENABLE_SWITCHBOT
    if (window == SWITCHBOT) return f(g_registry.get<ecs::SwitchbotRuntimeComponent>(owner));
#endif
    if (window == DRAGON_SOUL_INVENTORY) return f(g_registry.get<ecs::DragonSoulInventoryComponent>(owner));
    return f(g_registry.get<ecs::MainInventoryRuntimeComponent>(owner));
}
void AssertPlaced(entt::entity owner, entt::entity item, TItemPos pos) {
    Check(RawOwner(item) == owner && ItemSystem::GetItemWindow(item) == pos.window_type &&
          ItemSystem::GetItemCell(item) == pos.cell, "slot/ownership transaction split");
    Check(g_registry.get<ecs::ItemOwner>(item).ownerPID == ecs::PlayerRuntime::GetPlayerID(owner), "owner PID mismatch");
    Storage(owner, pos.window_type, [&](const auto& inv) { Check(inv.items[pos.cell] == item, "missing anchor"); });
}
void AssertDetached(entt::entity owner, entt::entity item, uint8_t window) {
    Check(RawOwner(item) == entt::null && ItemSystem::GetItemWindow(item) == RESERVED_WINDOW &&
        !ItemSystem::IsItemEquipped(item), "item was reattached during removal");
    Storage(owner, window, [&](const auto& inv) {
        Check(std::find(inv.items.begin(), inv.items.end(), item) == inv.items.end(), "dangling inventory anchor");
        if constexpr (requires { inv.itemGrid; })
            Check(std::all_of(inv.itemGrid.begin(), inv.itemGrid.end(), [](auto cell) { return cell == 0; }), "dangling footprint");
    });
}
void PlacementWindows() {
    std::vector<uint8_t> windows {INVENTORY, DRAGON_SOUL_INVENTORY};
#ifdef ENABLE_EXTRA_INVENTORY
    windows.push_back(EXTRA_INVENTORY);
#endif
#ifdef ENABLE_SWITCHBOT
    windows.push_back(SWITCHBOT);
#endif
    for (auto window : windows) {
        Reset(); extraUnlock = INT32_MAX;
        const auto owner = PlacementOwner(), item = PlacementItem(2);
        Meta(item).dragon = window == DRAGON_SOUL_INVENTORY;
#ifdef ENABLE_EXTRA_INVENTORY
        Meta(item).extra = window == EXTRA_INVENTORY;
#endif
        const TItemPos pos(window, 2);
        onSave = [&](entt::entity e) { AssertPlaced(owner, e, pos); };
        onStoragePacket = [&](entt::entity e, uint8_t header, TItemPos p) {
            Check(e == owner && header == HEADER_GC_ITEM_SET, "unexpected insertion packet"); AssertPlaced(owner, item, p);
        };
        onRegistration = [&] { AssertPlaced(owner, item, pos); };
        Check(ItemSystem::PlaceItemEcs(owner, item, window, 2), "native insertion rejected");
        Check(saves == 1 && storagePackets == 1, "insert publication count");
        Check(g_registry.get<ecs::ItemOwner>(item).lastOwnerPID == 37, "last owner PID not recorded");
        onSave = [&](entt::entity e) { AssertDetached(owner, e, window); };
        onRegistration = [&] { AssertDetached(owner, item, window); };
        onStoragePacket = [&](entt::entity e, uint8_t header, TItemPos p) {
            Check(e == owner && header == HEADER_GC_ITEM_DEL && p.cell == 2, "unexpected removal packet");
            AssertDetached(owner, item, window);
        };
        Check(ItemSystem::RemoveItemEcs(item), "native detach rejected");
        AssertDetached(owner, item, window);
        Check(saves == 2 && storagePackets == 2 && registrations == 0, "detach publication count");
        Check(ItemSystem::RemoveItemEcs(item) && saves == 2 && storagePackets == 2, "idempotent remove published twice");
    }
    Reset();
    const auto owner = PlacementOwner(), belt = PlacementItem();
    Check(ItemSystem::PlaceItemEcs(owner, belt, INVENTORY, BELT_INVENTORY_SLOT_START), "belt rejected");
    Check(ItemSystem::RemoveItemEcs(belt), "belt detach failed");
    AssertDetached(owner, belt, INVENTORY);
}
void PlacementValidation() {
    Reset(); extraUnlock = INT32_MAX;
    const auto owner = PlacementOwner(), other = PlacementOwner(), item = PlacementItem(2);
    Check(!ItemSystem::PlaceItemEcs(entt::null, item, INVENTORY, 0), "null owner accepted");
    Check(!ItemSystem::PlaceItemEcs(owner, entt::null, INVENTORY, 0), "null item accepted");
    Check(!ItemSystem::PlaceItemEcs(item, item, INVENTORY, 0), "non-player entity accepted as inventory owner");
    const auto stale = PlacementOwner(); g_registry.destroy(stale);
    g_registry.emplace<ecs::ItemOwner>(item).owner = stale;
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "stale raw owner accepted");
    g_registry.get<ecs::ItemOwner>(item).owner = other;
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "same-PID foreign owner accepted");
    g_registry.get<ecs::ItemOwner>(item).owner = entt::null;
    for (uint8_t window : {uint8_t(RESERVED_WINDOW), uint8_t(EQUIPMENT), uint8_t(SAFEBOX), uint8_t(MALL), uint8_t(MOUNT_INVENTORY), uint8_t(255)})
        Check(!ItemSystem::PlaceItemEcs(owner, item, window, 0), "unsupported window accepted");
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, INVENTORY_PAGE_SIZE - 1), "item crossed main page");
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, BELT_INVENTORY_SLOT_START), "tall item in belt");
    Check(!ItemSystem::PlaceItemEcs(owner, item, DRAGON_SOUL_INVENTORY, 0), "non-DS item in DS inventory");
    Meta(item).dragon = true; Meta(item).dragonBase = DRAGON_SOUL_BOX_SIZE;
    Check(!ItemSystem::PlaceItemEcs(owner, item, DRAGON_SOUL_INVENTORY, 0), "wrong DS box accepted");
    Check(!ItemSystem::PlaceItemEcs(owner, item, DRAGON_SOUL_INVENTORY, 2 * DRAGON_SOUL_BOX_SIZE - 1), "DS item crosses box");
    Meta(item).dragon = false;
#ifdef ENABLE_EXTRA_INVENTORY
    Meta(item).extra = true; Meta(item).category = 2;
    Check(!ItemSystem::PlaceItemEcs(owner, item, EXTRA_INVENTORY, 0), "wrong extra category accepted");
    extraUnlock = INT32_MIN;
    Check(!ItemSystem::PlaceItemEcs(owner, item, EXTRA_INVENTORY, 3 * EXTRA_INVENTORY_CATEGORY_MAX_NUM - 1), "locked extra cell accepted");
    Meta(item).extra = false;
#endif
    auto& inv = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
    const auto blocker = PlacementItem();
    inv.items[INVENTORY_PAGE_COLUMN] = blocker;
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "secondary anchor without grid ignored");
    inv.items[INVENTORY_PAGE_COLUMN] = entt::null; inv.itemGrid[INVENTORY_PAGE_COLUMN] = 17;
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "secondary grid ignored");
    inv.itemGrid[INVENTORY_PAGE_COLUMN] = 0;
    g_registry.get<ecs::DragonSoulInventoryComponent>(owner).items[0] = item;
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "cross-window alias accepted");
    g_registry.get<ecs::DragonSoulInventoryComponent>(owner).items[0] = entt::null;
    g_registry.emplace<ecs::SectorPlacement>(item);
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "ground item inserted without removal");
    g_registry.remove<ecs::SectorPlacement>(item);
    Meta(item).proto.bSize = 0;
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "zero size inserted");
    Check(saves == 0 && storagePackets == 0 && RawOwner(item) == entt::null, "rejected insertion published state");
    Meta(item).proto.bSize = 1;
    for (unsigned cell = INVENTORY_AND_EQUIP_SLOT_MAX; cell <= UINT16_MAX; ++cell)
        Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, static_cast<uint16_t>(cell)), "oversized cell accepted");
}
void PlacementQueries() {
    Reset(); extraUnlock = INT32_MAX;
    const auto owner = PlacementOwner(), item = PlacementItem(2), blocker = PlacementItem();
    auto& main = g_registry.emplace<ecs::MainInventoryRuntimeComponent>(owner);
    main.items[0] = blocker; main.items[1 + INVENTORY_PAGE_COLUMN] = blocker;
    Check(ItemSystem::GetEmptyInventoryPositionEcs(owner, item) == 2, "main query ignored anchors");
    Check(ItemSystem::HasMainInventorySpaceEcs(owner, 2), "native main space query rejected");
    main.items.fill(blocker);
    Check(ItemSystem::GetEmptyInventoryPositionEcs(owner, item) == -1, "full main query ignored anchors");
#ifdef ENABLE_EXTRA_INVENTORY
    auto& extra = g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(owner);
    Meta(item).extra = true; Meta(item).category = 3;
    const int begin = 3 * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
    extra.items[begin] = blocker; extra.items[begin + 1 + EXTRA_INVENTORY_PAGE_COLUMN] = blocker;
    Check(ItemSystem::GetEmptyExtraInventory(owner, item) == begin + 2, "native extra query wrong");
    Check(ItemSystem::GetEmptyInventoryPositionEcs(owner, item) == begin + 2, "extra dispatch wrong");
    extra.items.fill(blocker);
    Check(ItemSystem::GetEmptyExtraInventory(owner, item) == -1, "full extra query ignored anchors");
    extra.items[begin + EXTRA_INVENTORY_CATEGORY_MAX_NUM - 1] = entt::null;
    Meta(item).proto.bSize = 1;
    Check(ItemSystem::GetEmptyExtraInventory(owner, item) == begin + EXTRA_INVENTORY_CATEGORY_MAX_NUM - 1, "unlock multiplication overflow");
    extraUnlock = INT32_MIN;
    Check(ItemSystem::GetEmptyExtraInventory(owner, item) == -1, "negative unlock opened slot");
    Meta(item).extra = false;
#endif
    auto& ds = g_registry.emplace<ecs::DragonSoulInventoryComponent>(owner);
    Meta(item).dragon = true; Meta(item).proto.bSize = 2;
    Meta(item).dragonBase = DRAGON_SOUL_BOX_SIZE;
    ds.items[DRAGON_SOUL_BOX_SIZE] = blocker;
    ds.items[DRAGON_SOUL_BOX_SIZE + 1 + DRAGON_SOUL_BOX_COLUMN_NUM] = blocker;
    Check(ItemSystem::GetEmptyDragonSoulInventory(owner, item) == DRAGON_SOUL_BOX_SIZE + 2, "DS query ignored anchors");
    ds.items.fill(blocker);
    Check(ItemSystem::GetEmptyInventoryPositionEcs(owner, item) == -1, "full DS query ignored anchors");
    g_registry.destroy(owner);
    Check(ItemSystem::GetEmptyDragonSoulInventory(owner, item) == -1 &&
        ItemSystem::GetEmptyInventoryPositionEcs(owner, item) == -1, "query accepted stale owner");
}
struct ConstructionCallback {
    std::function<void(entt::registry&, entt::entity)> callback;
    void OnConstruct(entt::registry& registry, entt::entity e) { callback(registry, e); }
};
void PlacementCallbacks() {
    Reset();
    auto owner = PlacementOwner(), other = PlacementOwner(), item = PlacementItem();
    onSave = [&](entt::entity e) {
        onSave = {};
        Check(ItemSystem::RemoveItemEcs(e), "nested removal failed");
        Check(ItemSystem::PlaceItemEcs(other, e, INVENTORY, 3), "nested move failed");
    };
    Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "committed insertion misreported failure");
    AssertPlaced(other, item, TItemPos(INVENTORY, 3));
    Check(g_registry.get<ecs::MainInventoryRuntimeComponent>(owner).items[0] == entt::null, "old callback restored source slot");

    Reset(); owner = PlacementOwner(); item = PlacementItem();
    Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "setup insert");
    const auto replacement = PlacementItem();
    onSave = [&](entt::entity e) {
        Check(RawOwner(e) == entt::null, "removal save before commit");
        onSave = {};
        Check(ItemSystem::PlaceItemEcs(owner, replacement, INVENTORY, 0), "replacement insert failed");
    };
    Check(ItemSystem::RemoveItemEcs(item), "removal with replacement failed");
    AssertPlaced(owner, replacement, TItemPos(INVENTORY, 0));
    Check(RawOwner(item) == entt::null, "removed item reattached over replacement");

    for (bool destroyOwner : {false, true}) {
        Reset(); owner = PlacementOwner(); item = PlacementItem();
        entt::entity recycled = entt::null;
        onSave = [&](entt::entity e) {
            onSave = {};
            g_registry.destroy(destroyOwner ? owner : e); recycled = g_registry.create();
        };
        Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "committed destroy callback rolled back");
        Check(recycled != (destroyOwner ? owner : item) &&
            !g_registry.any_of<ecs::ItemOwner, ecs::ItemLocation, ecs::MainInventoryRuntimeComponent>(recycled), "recycled entity mutated");
    }

    Reset(); owner = PlacementOwner(); item = PlacementItem();
    g_registry.emplace<ecs::ItemEvents>(item).destroy = LPEVENT(new EVENT);
    onCancel = [&] { g_registry.destroy(item); };
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "destroyed item inserted after timer callback");
    Check(storagePackets == 0 && saves == 0, "failed timer cancellation published");

    Reset(); owner = PlacementOwner(); item = PlacementItem();
    const auto blocker = PlacementItem();
    g_registry.emplace<ecs::ItemEvents>(item).destroy = LPEVENT(new EVENT);
    onCancel = [&] { onCancel = {}; Check(ItemSystem::PlaceItemEcs(owner, blocker, INVENTORY, 0), "timer callback replacement"); };
    Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "occupied destination not rechecked after cancellation");
    AssertPlaced(owner, blocker, TItemPos(INVENTORY, 0));
    Check(RawOwner(item) == entt::null, "failed insert prewrote ownership");

    for (bool destroyOwner : {false, true}) {
        Reset(); owner = PlacementOwner(); item = PlacementItem();
        ConstructionCallback callback {[](entt::registry& registry, entt::entity e) { registry.destroy(e); }};
        entt::scoped_connection connection = destroyOwner ?
            g_registry.on_construct<ecs::MainInventoryRuntimeComponent>().connect<&ConstructionCallback::OnConstruct>(callback) :
            g_registry.on_construct<ecs::ItemOwner>().connect<&ConstructionCallback::OnConstruct>(callback);
        Check(!ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "component callback destruction ignored");
        Check(saves == 0 && storagePackets == 0, "constructor failure published");
    }
}
entt::entity WornItem(entt::entity owner) {
    const auto item = PlacementItem();
    auto& inv = g_registry.get_or_emplace<ecs::MainInventoryRuntimeComponent>(owner);
    inv.items[INVENTORY_MAX_NUM + WEAR_BODY] = item;
    inv.itemGrid[INVENTORY_MAX_NUM + WEAR_BODY] = INVENTORY_MAX_NUM + WEAR_BODY + 1;
    auto& ownership = g_registry.emplace<ecs::ItemOwner>(item);
    ownership.owner = owner; ownership.ownerPID = 37;
    g_registry.emplace<ecs::ItemLocation>(item, ecs::ItemLocation {EQUIPMENT, INVENTORY_MAX_NUM + WEAR_BODY});
    g_registry.emplace<ecs::ItemEquipped>(item, ecs::ItemEquipped {true, WEAR_BODY});
    return item;
}
struct UnequipListener {
    entt::entity owner, item;
    int events = 0;
    void OnUnequip(const ecs::EvItemUnequipped& event) {
        Check(event.itemEntity == item && event.charEntity == owner, "wrong unequip event identity");
        AssertDetached(owner, item, INVENTORY); ++events;
    }
};
void NativeUnequip() {
    Reset();
    auto owner = PlacementOwner(), item = WornItem(owner);
    UnequipListener listener {owner, item};
    entt::scoped_connection connection = g_dispatcher.sink<ecs::EvItemUnequipped>().connect<&UnequipListener::OnUnequip>(listener);
    onService = [&](const char* name) {
        if (std::string_view(name) == "stop-unique") {
            Check(!InventorySystem::Unequip(item), "recursive unequip was accepted");
            Check(!ItemSystem::RemoveItemEcs(item), "recursive remove was accepted");
        }
        if (std::string_view(name) == "battle-points" || std::string_view(name) == "update-packet")
            AssertDetached(owner, item, INVENTORY);
    };
    onStoragePacket = [&](entt::entity e, uint8_t header, TItemPos pos) {
        Check(e == owner && header == HEADER_GC_ITEM_DEL && pos.window_type == EQUIPMENT &&
            pos.cell == INVENTORY_MAX_NUM + WEAR_BODY, "unequip wire position changed");
        AssertDetached(owner, item, INVENTORY);
    };
    Check(ItemSystem::RemoveItemEcs(item), "equipped removal failed");
    AssertDetached(owner, item, INVENTORY);
    Check(listener.events == 1 && storagePackets == 1, "unequip duplicate/missing event or slot packet");
    Check(!InventorySystem::Unequip(item), "detached item unequipped again");
    connection.release();
    for (const std::string stage : {"stop-unique", "stop-accessory", "buff-remove", "battle-points", "update-packet"}) {
        Reset(); owner = PlacementOwner(); item = WornItem(owner);
        entt::entity replacement = entt::null;
        onService = [&](const char* name) {
            if (name == stage) { onService = {}; g_registry.destroy(item); replacement = g_registry.create(); }
        };
        ItemSystem::RemoveItemEcs(item);
        Check(replacement != entt::null && replacement != item &&
            !g_registry.any_of<ecs::ItemOwner, ecs::ItemLocation>(replacement), "unequip wrote into recycled item");
    }
    Check(!InventorySystem::Unequip(entt::null), "null item unequipped");
}

void RemovalCallbacksAndAliases() {
    Reset();
    const auto owner = PlacementOwner(), item = PlacementItem();
    Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "alias fixture insertion");
    auto& main = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
    main.items[2] = item; main.itemGrid[2] = main.itemGrid[7] = 3;
    auto& ds = g_registry.emplace<ecs::DragonSoulInventoryComponent>(owner);
    ds.items[3] = item; ds.itemGrid[3] = 4;
    onSave = [&](entt::entity e) {
        AssertDetached(owner, e, INVENTORY); AssertDetached(owner, e, DRAGON_SOUL_INVENTORY);
    };
    Check(ItemSystem::RemoveItemEcs(item), "alias removal failed");
    AssertDetached(owner, item, INVENTORY); AssertDetached(owner, item, DRAGON_SOUL_INVENTORY);

    Reset();
    const auto actor = PlacementOwner(), moved = PlacementItem();
    Check(ItemSystem::PlaceItemEcs(actor, moved, INVENTORY, 0), "callback fixture insertion");
    g_registry.remove<ecs::ItemEquipped>(moved);
    ConstructionCallback callback {[&](entt::registry&, entt::entity e) {
        Check(ItemSystem::RemoveItemEcs(e), "nested constructor removal");
        Check(ItemSystem::PlaceItemEcs(actor, e, INVENTORY, 7), "nested constructor relocation");
    }};
    entt::scoped_connection connection = g_registry.on_construct<ecs::ItemEquipped>().connect<&ConstructionCallback::OnConstruct>(callback);
    Check(!ItemSystem::RemoveItemEcs(moved), "old removal accepted changed location");
    AssertPlaced(actor, moved, TItemPos(INVENTORY, 7));
    connection.release();

    Reset();
    const auto first = PlacementOwner(), second = PlacementOwner(), worn = WornItem(first);
    onStoragePacket = [&](entt::entity, uint8_t header, TItemPos pos) {
        if (header == HEADER_GC_ITEM_DEL && pos.window_type == EQUIPMENT) {
            onStoragePacket = {};
            Check(ItemSystem::PlaceItemEcs(second, worn, INVENTORY, 0), "post-unequip relocation");
        }
    };
    Check(!ItemSystem::RemoveItemEcs(worn), "old removal continued after reownership");
    AssertPlaced(second, worn, TItemPos(INVENTORY, 0));
}

void PurePlacementRules() {
    Reset();
    const auto owner = PlacementOwner(), item = PlacementItem();
#ifdef ENABLE_RUNE_SYSTEM
    Meta(item).rune = true;
    Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "pure placement invoked rune acquisition");
    Check(ItemSystem::RemoveItemEcs(item), "rune detach failed");
    Meta(item).rune = false;
#endif
#ifdef ENABLE_ACCE_SYSTEM
    Meta(item).proto.bType = ITEM_COSTUME; Meta(item).proto.bSubType = COSTUME_ACCE;
    Meta(item).proto.alValues[ACCE_GRADE_VALUE_FIELD] = 2;
    Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, 0), "pure accessory placement failed");
    Check(ItemSystem::GetItemSocket(item, ACCE_ABSORPTION_SOCKET) == 0, "transfer rerolled accessory");
    Check(ItemSystem::RemoveItemEcs(item), "accessory detach failed");
    Check(InventorySystem::AddToCharacter(item, owner, TItemPos(INVENTORY, 0)), "acquisition accessory insertion failed");
    Check(ItemSystem::GetItemSocket(item, ACCE_ABSORPTION_SOCKET) == ACCE_GRADE_2_ABS, "acquisition lost accessory initialization");
#endif
}
}

int main() {
    try {
        DSManager dragonSouls;
#ifdef ENABLE_SWITCHBOT
        CSwitchbotManager switchbots;
#endif
        PlacementWindows(); PlacementValidation(); PlacementQueries(); PlacementCallbacks(); NativeUnequip(); RemovalCallbacksAndAliases(); PurePlacementRules();
        InventoryGuards(); InventoryGrids(); Basic(); DuplicatesAndValidation(); SyncAndLifetime(); ValueRanges(); ClientValidation(); HydrationAndRelocation();
        std::cout << "Quickslot checks passed: " << checks << '\n'; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
