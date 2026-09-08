#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/DragonSoulSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/marriage.h"
#include "../../SRC/Server/GameServer/MountSystem.h"
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
#include "../../SRC/Server/GameServer/log.h"
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
struct PlacementActor {
    DESC* desc = nullptr; uint32_t pid = 37;
    uint8_t job = JOB_WARRIOR, sex = SEX_MALE;
    int level = 100, maxHP = 100, maxSP = 100;
    bool duelBlock = false, poly = false, riding = false, deck = false;
    std::array<int64_t, POINT_MAX_NUM> points {};
};
bool married = false, marriageItem = false;
std::vector<uint32_t> notices;
PlacementActor& Actor(entt::entity e) {
    Check(g_registry.valid(e) && g_registry.all_of<PlacementActor>(e), "stale actor service");
    return g_registry.get<PlacementActor>(e);
}
struct PlacementMeta {
    TItemTable proto {};
    uint8_t category = 0;
    uint16_t dragonBase = 0;
    bool extra = false, dragon = false, rune = false, locked = false, exchanging = false, pending = false;
    int wear = WEAR_BODY, group = 0;
};
std::vector<std::unique_ptr<DESC>> descriptors;
std::function<void(entt::entity)> onSave;
std::function<void()> onCancel, onRegistration;
std::function<void(entt::entity, uint8_t, TItemPos)> onStoragePacket;
std::function<void(const char*)> onService;
std::function<entt::entity(uint32_t, uint32_t)> onCreate;
std::function<ItemSystem::StackMergeResult(entt::entity, entt::entity, entt::entity, uint32_t)> onMerge;
std::function<bool(entt::entity, TItemPos, entt::entity&)> onPullOut;
bool switchActive = false, switchAllowed = true;
int retiredSplits = 0;
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
    proto.cLimitTimerBasedOnWearIndex = -1; proto.cLimitRealTimeFirstUseIndex = -1;
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
    onCreate = {}; onMerge = {}; onPullOut = {}; retiredSplits = 0;
    switchActive = false; switchAllowed = true;
    onSave = {}; onCancel = onRegistration = {}; onStoragePacket = {}; onService = {};
    notices.clear(); married = marriageItem = false;
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
    if (g_registry.all_of<PlacementActor>(owner)) {
        const auto lookup = [&](const auto* storage) {
            return storage && pos.cell < storage->items.size() ? storage->items[pos.cell] : entt::entity(entt::null);
        };
        switch (pos.window_type) {
        case INVENTORY: case EQUIPMENT: return lookup(g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner));
        case DRAGON_SOUL_INVENTORY: return lookup(g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner));
#ifdef ENABLE_EXTRA_INVENTORY
        case EXTRA_INVENTORY: return lookup(g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner));
#endif
#ifdef ENABLE_SWITCHBOT
        case SWITCHBOT: return lookup(g_registry.try_get<ecs::SwitchbotRuntimeComponent>(owner));
#endif
        default: return entt::entity(entt::null);
        }
    }
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
        const auto item = ItemSystem::GetItem(GetEntity(), pos);
        Check(ItemSystem::IsValidItem(item) && p.count == ItemSystem::GetItemCount(item) &&
            p.vnum == ItemSystem::GetItemVnum(item), "item set payload");
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
            Check(uint32_t(p.alSockets[i]) == ItemSystem::GetItemSocket(item, i), "item set socket payload");
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) {
            const auto attr = ItemSystem::GetItemAttribute(item, i);
            Check(p.aAttr[i].bType == attr.bType && p.aAttr[i].sValue == attr.sValue, "item set attribute payload");
        }
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
bool DSManager::PullOutEcs(entt::entity owner, TItemPos pos, entt::entity& stone, entt::entity extractor) {
    Check(extractor == entt::null, "drag unexpectedly supplied an extractor");
    if (!onPullOut) Unexpected();
    auto callback = onPullOut; return callback(owner, pos, stone);
}
ITEM_MANAGER::ITEM_MANAGER() = default;
ITEM_MANAGER::~ITEM_MANAGER() = default;
CAsyncSQL::CAsyncSQL() = default;
CAsyncSQL::~CAsyncSQL() = default;
CSemaphore::CSemaphore() = default;
CSemaphore::~CSemaphore() = default;
LogManager::LogManager() : m_bIsConnect(false) {}
LogManager::~LogManager() = default;
void LogManager::ItemLogEntity(entt::entity owner, entt::entity item, const char* action, const char* hint) {
    Actor(owner); Meta(item);
    unsigned cloneID = 0, split = 0, remaining = 0, total = 0;
    Check(std::string_view(action) == "ITEM_SPLIT" && sscanf(hint, "%u %u %u %u", &cloneID, &split, &remaining, &total) == 4,
        "split audit fields lost");
    Check(split > 0 && remaining > 0 && split + remaining == total && ItemSystem::GetItemCount(item) == remaining,
        "split audit precedes commit or lost quantity");
    Service("split-log");
}
entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t id, bool magic, int rare, bool skip) {
    Check(id == 0 && !magic && rare == -1 && !skip, "split factory changed creation flags");
    if (!onCreate) Unexpected();
    auto callback = onCreate; return callback(vnum, count);
}
bool ItemSystem::IsItemStackable(entt::entity item) { return (GetItemFlags(item) & ITEM_FLAG_STACKABLE) != 0; }
bool ItemSystem::IsItemConsumptionPending(entt::entity item) { return Meta(item).pending; }
ItemSystem::StackMergeResult ItemSystem::MergeItemStacksEcs(entt::entity owner, entt::entity source, entt::entity target,
    uint32_t count, StackSource context) {
    Actor(owner); Meta(source); Meta(target);
    Check(context == StackSource::Inventory, "client drag selected reward merge semantics");
    if (!onMerge) return {};
    auto callback = onMerge; return callback(owner, source, target, count);
}
void ecs::PointSystem::Compute(entt::entity e) { Actor(e); Service("compute-points"); }
void MountSystem::UpdateMountCountOverheadToViewers(entt::entity e) { Actor(e); Service("mount-overhead"); }
void CombatSystem::SendLeaderboardDataSkillMob(entt::entity e, entt::entity viewer) {
    Actor(e); Actor(viewer); Service("skill-leaderboard");
}
#ifdef ENABLE_SWITCHBOT
CSwitchbotManager::CSwitchbotManager() = default;
CSwitchbotManager::~CSwitchbotManager() = default;
bool CSwitchbotManager::IsActive(uint32_t, uint8_t) { return switchActive; }
bool SwitchbotHelper::IsValidItem(entt::entity e) { Meta(e); return switchAllowed; }
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
short ItemSystem::GetItemLockedAttributeIndex(entt::entity e) {
    Meta(e); const auto* lock = g_registry.try_get<ecs::ItemLockedAttribute>(e); return lock ? lock->index : -1;
}
int MAX(int a, int b) { return std::max(a, b); }
int MIN(int a, int b) { return std::min(a, b); }
int number_ex(int, int, char const *, int) { Unexpected(); }
void ecs::ChatSystem::SendNew(entt::entity e, uint8_t, uint32_t id, char const *, ...) { Actor(e); notices.push_back(id); Service("notice"); }
void ecs::ChatSystem::Send(entt::entity e, uint8_t, char const *, ...) { Actor(e); Service("notice"); }
DESC * ecs::PlayerRuntime::GetDesc(entt::entity e) { return g_registry.get<PlacementActor>(e).desc; }
uint32_t ecs::PlayerRuntime::GetPlayerID(entt::entity e) { return g_registry.get<PlacementActor>(e).pid; }
void ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(entt::entity e, entt::entity i) { Actor(e); Meta(i); Service("buff-add"); }
void ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(entt::entity, entt::entity) { Service("buff-remove"); }
void ecs::PlayerRuntime::SetItem(entt::entity, SItemPos, entt::entity, bool) { Unexpected(); }
void ecs::PlayerRuntime::SetWear(entt::entity, uint8_t, entt::entity) { Unexpected(); }
std::string_view ecs::PlayerRuntime::GetName(entt::entity) { Unexpected(); }
SECTREE * ecs::PlayerRuntime::GetSectree(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetPart(entt::entity e, uint8_t, uint16_t) { Actor(e); Service("set-part"); }
uint16_t ecs::PlayerRuntime::GetOriginalPart(entt::entity e, uint8_t) { Actor(e); return 0; }
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
bool ItemSystem::IsDragonSoulItem(entt::entity e) { return Meta(e).dragon; }
bool ItemSystem::IsExtraItem(entt::entity e) { return Meta(e).extra; }
bool ItemSystem::IsRideItem(entt::entity e) { Meta(e); return false; }
bool ItemSystem::IsMountItem(entt::entity e) { Meta(e); return false; }
bool ItemSystem::IsRuneItem(entt::entity e) { return Meta(e).rune; }
uint32_t ItemSystem::GetItemID(entt::entity e) { return g_registry.get<ecs::ItemIdentity>(e).id; }
uint32_t ItemSystem::GetItemVID(entt::entity) { Unexpected(); }
uint32_t ItemSystem::GetItemVnum(entt::entity e) { return g_registry.get<ecs::ItemIdentity>(e).vnum; }
uint32_t ItemSystem::GetItemOriginalVnum(entt::entity e) { return GetItemVnum(e); }
TItemExtraProto * ItemSystem::GetItemExtraProto(entt::entity e) { Meta(e); return nullptr; }
uint32_t ItemSystem::GetItemSIGVnum(entt::entity e) { Meta(e); return 0; }
int ItemSystem::GetItemValue(entt::entity e, uint32_t i) { return Meta(e).proto.alValues[i]; }
char const * ItemSystem::GetItemName(entt::entity e) { Meta(e); return "test-item"; }
uint32_t ItemSystem::GetItemWearFlag(entt::entity e) { return Meta(e).proto.dwWearFlags; }
int ItemSystem::FindEquipCell(entt::entity owner, entt::entity e, int) { Actor(owner); return Meta(e).wear; }
SItemTable const * ItemSystem::GetItemProto(entt::entity e) { return &Meta(e).proto; }
bool ItemSystem::DestroyItemEntityEcs(entt::entity e, char const * reason) {
    Check(std::string_view(reason) == "SPLIT_ABORT" && RawOwner(e) == entt::null, "unexpected item destruction");
    ++retiredSplits; g_registry.destroy(e); return true;
}
short ItemSystem::GetItemLockedAttr(entt::entity) { Unexpected(); }
int ItemSystem::GetItemAccessorySocketGrade(entt::entity) { Unexpected(); }
bool ItemSystem::IsAccessoryForSocket(entt::entity e) { Meta(e); return false; }
void ItemSystem::StartUniqueExpireEvent(entt::entity e) { Meta(e); Service("start-unique"); }
void ItemSystem::StopUniqueExpireEvent(entt::entity e) { Meta(e); Service("stop-unique"); }
void ItemSystem::StartTimerBasedOnWearExpireEvent(entt::entity e) { Meta(e); Service("start-wear-timer"); }
void ItemSystem::StopTimerBasedOnWearExpireEvent(entt::entity) { Unexpected(); }
void ItemSystem::StartAccessorySocketExpireEvent(entt::entity e) { Meta(e); Service("start-accessory"); }
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
uint8_t ItemSystem::GetItemWindow(entt::entity e) { Meta(e); const auto* l = g_registry.try_get<ecs::ItemLocation>(e); return l ? l->window : RESERVED_WINDOW; }
uint16_t ItemSystem::GetItemCell(entt::entity e) { Meta(e); const auto* l = g_registry.try_get<ecs::ItemLocation>(e); return l ? l->cell : 0; }
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
bool DSManager::ActivateDragonSoul(entt::entity e) { Meta(e); Service("ds-activate"); return true; }
bool DSManager::DeactivateDragonSoul(entt::entity e, bool) { Meta(e); Service("ds-deactivate"); return true; }
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


marriage::CManager::CManager() = default;
marriage::CManager::~CManager() = default;
bool marriage::CManager::IsMarriageUniqueItem(uint32_t) { return marriageItem; }
bool marriage::CManager::IsMarried(uint32_t) { return married; }
quest::CQuestManager::CQuestManager() = default;
quest::CQuestManager::~CQuestManager() = default;
quest::NPC::NPC() = default;
quest::NPC::~NPC() = default;
quest::PC::PC() : m_RunningQuestState(nullptr) {}
quest::PC::~PC() = default;
bool quest::CQuestManager::UseItem(unsigned int, entt::entity item, bool) { Meta(item); Service("quest-use"); return true; }
void CMountSystem::Mount(uint32_t, entt::entity) { Unexpected(); }
int ecs::PlayerRuntime::GetDuelOption(entt::entity e, const char*) { return Actor(e).duelBlock; }
uint8_t ecs::PlayerRuntime::GetJob(entt::entity e) { return Actor(e).job; }
uint8_t ecs::PlayerRuntime::GetSex(entt::entity e) { return Actor(e).sex; }
int64_t ecs::PointSystem::Get(entt::entity e, uint8_t type) { return Actor(e).points[type]; }
int32_t ecs::PointSystem::GetLevel(entt::entity e) { return Actor(e).level; }
int32_t ecs::PointSystem::GetMaxHP(entt::entity e) { return Actor(e).maxHP; }
int32_t ecs::PointSystem::GetMaxSP(entt::entity e) { return Actor(e).maxSP; }
void ecs::PointSystem::Change(entt::entity e, uint8_t type, int64_t amount, bool, bool
#ifdef __ENABLE_BLOCK_EXP__
, bool
#endif
) { Actor(e).points[type] += amount; Service("point-change"); }
bool ItemSystem::IsSameSpecialGroup(entt::entity a, entt::entity b) {
    return IsPlacement(a) && IsPlacement(b) && Meta(a).group && Meta(a).group == Meta(b).group;
}
bool ItemSystem::IsItemLocked(entt::entity e) { return Meta(e).locked; }
bool ItemSystem::IsItemExchanging(entt::entity e) { return Meta(e).exchanging; }
bool ItemSystem::StartRealTimeExpireEventEcs(entt::entity e) { Meta(e); Service("real-time"); return true; }
bool DragonSoulSystem::IsDeckActivated(entt::entity e) { return Actor(e).deck; }
bool DragonSoulSystem::CanRefine(entt::entity e) {
    const auto* state = g_registry.try_get<ecs::DragonSoulRuntimeStateComponent>(e);
    return state && g_registry.valid(state->refineWindowOpener);
}
bool AffectSystem::IsPolymorphed(entt::entity e) { return Actor(e).poly; }
bool AffectSystem::IsAffectFlag(entt::entity e, uint32_t) { Actor(e); return false; }
bool AffectSystem::RemoveAffect(entt::entity e, uint32_t) { Actor(e); Service("affect-remove"); return true; }
bool MountSystem::IsRiding(entt::entity e) { return Actor(e).riding; }
uint32_t MountSystem::GetMountVnum(entt::entity e) { Actor(e); return 0; }
void MountSystem::ForceClearRidingState(entt::entity e) { Actor(e).riding = false; }
void NetworkSyncSystem::BroadcastEffect(entt::registry&, entt::entity e, uint8_t) { Actor(e); Service("effect"); }
void NetworkSyncSystem::BroadcastSpecificEffect(entt::registry&, entt::entity e, const char*) { Actor(e); Service("effect"); }


namespace {
entt::entity Gear(entt::entity owner, uint16_t cell = 0, int wear = WEAR_BODY) {
    const auto item = PlacementItem();
    Meta(item).proto.bType = ITEM_RING;
    Meta(item).proto.dwWearFlags = WEARABLE_BODY;
    Meta(item).proto.bSubType = 0; Meta(item).wear = wear;
    Check(ItemSystem::PlaceItemEcs(owner, item, INVENTORY, cell), "gear fixture placement");
    return item;
}
void AssertWorn(entt::entity owner, entt::entity item, int slot) {
    Check(ItemSystem::IsItemEquipped(item) && RawOwner(item) == owner &&
        ItemSystem::GetItemCell(item) == INVENTORY_MAX_NUM + slot &&
        g_registry.get<ecs::MainInventoryRuntimeComponent>(owner).items[INVENTORY_MAX_NUM + slot] == item,
        "equipped state mismatch");
}
void EquipmentPolicies() {
    Reset();
    const auto owner = PlacementOwner(), item = Gear(owner);
    Check(InventorySystem::CanEquipNow(owner, item), "basic equipment policy");
    Check(InventorySystem::IsEquipmentSexAllowed(owner, item), "basic sex policy");
    for (const auto [job, anti] : {std::pair{JOB_WARRIOR, ITEM_ANTIFLAG_WARRIOR},
        std::pair{JOB_ASSASSIN, ITEM_ANTIFLAG_ASSASSIN}, std::pair{JOB_SURA, ITEM_ANTIFLAG_SURA},
        std::pair{JOB_SHAMAN, ITEM_ANTIFLAG_SHAMAN}}) {
        Actor(owner).job = job; Meta(item).proto.dwAntiFlags = anti;
        Check(!InventorySystem::CanEquipNow(owner, item), "job restriction ignored");
    }
    Actor(owner).job = JOB_WARRIOR; Meta(item).proto.dwAntiFlags = 0;
    for (const uint8_t limit : {uint8_t(LIMIT_LEVEL), uint8_t(LIMIT_STR), uint8_t(LIMIT_INT), uint8_t(LIMIT_DEX), uint8_t(LIMIT_CON)}) {
        Meta(item).proto.aLimits[0] = {limit, 101};
        Check(!InventorySystem::CanEquipNow(owner, item), "stat/level restriction ignored");
        Check(!notices.empty(), "missing restriction message");
        Meta(item).proto.aLimits[0] = {};
    }
    for (const auto [sex, anti] : {std::pair{SEX_MALE, ITEM_ANTIFLAG_MALE}, std::pair{SEX_FEMALE, ITEM_ANTIFLAG_FEMALE}}) {
        Actor(owner).sex = sex; Meta(item).proto.dwAntiFlags = anti;
        Check(!InventorySystem::IsEquipmentSexAllowed(owner, item), "sex restriction ignored");
    }
    Actor(owner).sex = SEX_MALE; Meta(item).proto.dwAntiFlags = 0;
#ifdef ENABLE_PVP_ADVANCED
    Actor(owner).duelBlock = true;
    Check(!InventorySystem::CanEquipNow(owner, item), "duel restriction ignored");
    Actor(owner).duelBlock = false;
#endif
    Meta(item).proto.dwWearFlags = WEARABLE_UNIQUE;
    marriageItem = true;
    Check(!InventorySystem::CanEquipNow(owner, item), "marriage restriction ignored");
    married = true;
    Check(InventorySystem::CanEquipNow(owner, item), "married unique rejected");
    marriageItem = false; Meta(item).proto.dwWearFlags = 0;
    Meta(item).locked = true;
    Check(!ItemSystem::EquipItemEcs(owner, item), "locked equipment accepted");
    Meta(item).locked = false; Meta(item).exchanging = true;
    Check(!ItemSystem::EquipItemEcs(owner, item), "exchange equipment accepted");
    Meta(item).exchanging = false;
    Actor(owner).poly = true;
    Check(!ItemSystem::EquipItemEcs(owner, item), "polymorphed equipment accepted");
    Actor(owner).poly = false;
    const auto foreign = PlacementOwner();
    Check(!ItemSystem::EquipItemEcs(foreign, item), "foreign equipment accepted");
    Check(!ItemSystem::EquipItemEcs(owner, item, WEAR_WEAPON), "wrong candidate accepted");
    AssertPlaced(owner, item, TItemPos(INVENTORY, 0));
    Check(!ItemSystem::UnequipItemEcs(owner, item), "non-worn item unequipped");
    // A destroyed owner with the same player ID is not an unowned item.
    g_registry.destroy(owner);
    Check(!ItemSystem::EquipItemEcs(foreign, item), "stale ownership silently adopted");
}
void EquipmentRoundTripAndSwap() {
    Reset();
    const auto owner = PlacementOwner(), item = Gear(owner, BELT_INVENTORY_SLOT_START);
    static_assert(BELT_INVENTORY_SLOT_START > UINT8_MAX);
    constexpr uint8_t truncatedCell = static_cast<uint8_t>(BELT_INVENTORY_SLOT_START);
    g_registry.emplace<ecs::QuickSlots>(owner).slots[0] = {QUICKSLOT_TYPE_ITEM, truncatedCell};
    onStoragePacket = [&](entt::entity e, uint8_t kind, TItemPos pos) {
        if (kind == HEADER_GC_ITEM_SET && pos.window_type == EQUIPMENT) AssertWorn(e, item, WEAR_BODY);
    };
    Check(ItemSystem::EquipItemEcs(owner, item), "native high-level equip failed");
    AssertWorn(owner, item, WEAR_BODY);
    Check(Read(owner, 0).pos == truncatedCell, "large source wrapped into quickslot");
    onStoragePacket = {};
    Actor(owner).points[POINT_HP] = 170; Actor(owner).points[POINT_SP] = 150;
    Check(ItemSystem::UnequipItemEcs(owner, item), "native high-level unequip failed");
    Check(!ItemSystem::IsItemEquipped(item) && RawOwner(item) == owner &&
        Actor(owner).points[POINT_HP] == 100 && Actor(owner).points[POINT_SP] == 100, "unequip/clamp failed");

    Reset();
    const auto actor = PlacementOwner(), old = Gear(actor), replacement = Gear(actor, INVENTORY_MAX_NUM - 1);
    Check(ItemSystem::EquipItemEcs(actor, old), "swap setup equip");
    const auto blocker = PlacementItem();
    auto& main = g_registry.get<ecs::MainInventoryRuntimeComponent>(actor);
    for (int i = 0; i < INVENTORY_MAX_NUM; ++i) {
        if (main.items[i] == entt::null) { main.items[i] = blocker; main.itemGrid[i] = i + 1; }
    }
    Check(!InventorySystem::CanUnequipNow(actor, old), "full inventory unequip accepted");
    g_registry.emplace<ecs::StatusFlags>(actor).isObserverMode = true;
    Check(!ItemSystem::EquipItemEcs(actor, replacement), "observer swapped equipment");
    g_registry.get<ecs::StatusFlags>(actor).isObserverMode = false;
    Check(ItemSystem::EquipItemEcs(actor, replacement), "full inventory native swap failed");
    AssertWorn(actor, replacement, WEAR_BODY);
    AssertPlaced(actor, old, TItemPos(INVENTORY, INVENTORY_MAX_NUM - 1));
    Meta(replacement).proto.dwFlags |= ITEM_FLAG_IRREMOVABLE;
    Check(!ItemSystem::EquipItemEcs(actor, old), "irremovable displaced item swapped");
    AssertWorn(actor, replacement, WEAR_BODY);

    Reset();
    const auto p = PlacementOwner(), a = Gear(p), b = Gear(p, INVENTORY_PAGE_SIZE - 1);
    Meta(a).proto.bSize = 2;
    Check(ItemSystem::EquipItemEcs(p, a), "tall swap setup");
    Check(!ItemSystem::EquipItemEcs(p, b), "swap crossed inventory page");
    AssertWorn(p, a, WEAR_BODY); AssertPlaced(p, b, TItemPos(INVENTORY, INVENTORY_PAGE_SIZE - 1));

    Reset();
    const auto questOwner = PlacementOwner(), created = PlacementItem();
    Meta(created).proto.bType = ITEM_RING;
    Check(ItemSystem::EquipItemEcs(questOwner, created), "fresh unowned quest item rejected");
    AssertWorn(questOwner, created, WEAR_BODY);

#ifdef ENABLE_EXTRA_INVENTORY
    Reset(); extraUnlock = INT32_MAX;
    const auto extraOwner = PlacementOwner(), first = PlacementItem(), second = PlacementItem();
    constexpr int source = 2 * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
    static_assert(source > UINT8_MAX);
    for (const auto e : {first, second}) {
        Meta(e).proto.bType = ITEM_RING; Meta(e).extra = true; Meta(e).category = 2;
    }
    Check(ItemSystem::PlaceItemEcs(extraOwner, first, EXTRA_INVENTORY, source), "extra first setup");
    Check(ItemSystem::EquipItemEcs(extraOwner, first), "extra first equip");
    Check(ItemSystem::PlaceItemEcs(extraOwner, second, EXTRA_INVENTORY, source), "extra replacement setup");
    Check(ItemSystem::EquipItemEcs(extraOwner, second), "extra swap lost window or cell");
    AssertPlaced(extraOwner, first, TItemPos(EXTRA_INVENTORY, source));
    AssertWorn(extraOwner, second, WEAR_BODY);
    Check(ItemSystem::UnequipItemEcs(extraOwner, second), "extra unequip");
    AssertPlaced(extraOwner, second, TItemPos(EXTRA_INVENTORY, source + 1));
#endif
}
void EquipmentCallbacks() {
    for (const std::string stage : {"start-unique", "start-accessory", "buff-add", "battle-points", "update-packet"}) {
        Reset();
        const auto owner = PlacementOwner(), item = Gear(owner);
        entt::entity recycled = entt::null;
        onService = [&](const char* name) {
            if (stage == name) {
                onService = {}; g_registry.destroy(item); recycled = g_registry.create();
            }
        };
        Check(ItemSystem::EquipItemEcs(owner, item), "committed equip misreported callback destruction");
        Check(recycled != entt::null && recycled != item && !g_registry.any_of<ecs::ItemOwner, ecs::ItemEquipped>(recycled),
            "equip wrote through recycled entity");
    }
    Reset();
    auto owner = PlacementOwner(), item = Gear(owner);
    onService = [&](const char* name) {
        if (std::string_view(name) == "buff-add") {
            Check(!ItemSystem::UnequipItemEcs(owner, item), "recursive high-level unequip accepted");
            Check(!ItemSystem::EquipItemEcs(owner, item), "recursive high-level equip accepted");
        }
    };
    Check(ItemSystem::EquipItemEcs(owner, item), "recursive guard setup");
    AssertWorn(owner, item, WEAR_BODY);
    onService = {};
    const auto blocker = PlacementItem();
    onService = [&](const char* name) {
        if (std::string_view(name) == "battle-points" && !ItemSystem::IsItemEquipped(item)) {
            onService = {};
            auto& main = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
            for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell) { main.items[cell] = blocker; main.itemGrid[cell] = cell + 1; }
        }
    };
    Check(!ItemSystem::UnequipItemEcs(owner, item), "lost destination reported as success");
    AssertWorn(owner, item, WEAR_BODY);

    Reset(); owner = PlacementOwner();
    const auto old = Gear(owner); item = Gear(owner, 1);
    Check(ItemSystem::EquipItemEcs(owner, old), "lock callback setup");
    onSave = [&](entt::entity e) {
        if (e == item && RawOwner(item) == entt::null) { onSave = {}; Meta(old).locked = true; }
    };
    Check(!ItemSystem::EquipItemEcs(owner, item), "swap ignored changed removal permission");
    AssertWorn(owner, old, WEAR_BODY);
    AssertPlaced(owner, item, TItemPos(INVENTORY, 1));

    Reset(); owner = PlacementOwner();
    const auto tall = Gear(owner); Meta(tall).proto.bSize = 2;
    Check(ItemSystem::EquipItemEcs(owner, tall), "recovery setup");
    item = Gear(owner, 10);
    const auto filler = PlacementItem();
    auto& main = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
    for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell) {
        if (cell != 10 && cell != 10 + INVENTORY_PAGE_COLUMN) {
            main.items[cell] = filler; main.itemGrid[cell] = cell + 1;
        }
    }
    onService = [&](const char* name) {
        if (std::string_view(name) == "buff-add" && ItemSystem::IsItemEquipped(item)) {
            onService = {};
            auto& current = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
            current.items[10 + INVENTORY_PAGE_COLUMN] = filler;
            current.itemGrid[10 + INVENTORY_PAGE_COLUMN] = 11 + INVENTORY_PAGE_COLUMN;
        }
    };
    Check(!ItemSystem::EquipItemEcs(owner, item), "interrupted swap reported success");
    AssertWorn(owner, tall, WEAR_BODY);
    AssertPlaced(owner, item, TItemPos(INVENTORY, 10));
    Check(g_registry.get<ecs::MainInventoryRuntimeComponent>(owner).items[10 + INVENTORY_PAGE_COLUMN] == filler,
        "recovery overwrote callback item");

    Reset(); owner = PlacementOwner();
    const auto original = Gear(owner); item = Gear(owner, 1);
    const auto callbackItem = PlacementItem(); Meta(callbackItem).proto.bType = ITEM_RING;
    Check(ItemSystem::EquipItemEcs(owner, original), "occupied wear recovery setup");
    onService = [&](const char* name) {
        if (std::string_view(name) == "battle-points" && !ItemSystem::IsItemEquipped(original)) {
            onService = {};
            Check(InventorySystem::EquipTo(callbackItem, owner, WEAR_BODY), "callback wear takeover");
        }
    };
    Check(!ItemSystem::EquipItemEcs(owner, item), "occupied wear swap reported success");
    AssertWorn(owner, callbackItem, WEAR_BODY);
    AssertPlaced(owner, original, TItemPos(INVENTORY, 0));
    AssertPlaced(owner, item, TItemPos(INVENTORY, 1));

    Reset(); owner = PlacementOwner(); item = Gear(owner);
    onService = [&](const char* name) {
        if (std::string_view(name) == "buff-add") { onService = {}; g_registry.destroy(owner); }
    };
    Check(ItemSystem::EquipItemEcs(owner, item), "committed equip rejected destroyed actor");
    Check(!g_registry.valid(owner), "actor callback not executed");
}
void EquipmentDragonSoulAndTimers() {
    Reset();
    const auto owner = PlacementOwner(), item = PlacementItem();
    Meta(item).dragon = true; Meta(item).proto.bType = ITEM_DS; Meta(item).wear = WEAR_MAX_NUM;
    Check(ItemSystem::PlaceItemEcs(owner, item, DRAGON_SOUL_INVENTORY, 0), "DS equipment fixture");
#ifdef ENABLE_DS_SET
    Actor(owner).deck = true;
    Check(!ItemSystem::EquipItemEcs(owner, item), "active deck accepted equip");
    Actor(owner).deck = false;
#endif
    Check(ItemSystem::EquipItemEcs(owner, item), "DS equip failed");
    AssertWorn(owner, item, WEAR_MAX_NUM);
    Check(ItemSystem::GetWearItem(owner, WEAR_MAX_NUM) == item, "DS wear query reported an empty slot");
    Check(ItemSystem::GetWearItem(owner, WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX) == entt::null,
        "out-of-range wear query accepted");
    Check(ItemSystem::GetWearItem(entt::null, WEAR_MAX_NUM) == entt::null, "null wear owner accepted");
#ifdef ENABLE_DS_SET
    Actor(owner).deck = true;
    Check(!ItemSystem::UnequipItemEcs(owner, item), "active deck accepted unequip");
    Actor(owner).deck = false;
#endif
    Check(ItemSystem::UnequipItemEcs(owner, item), "DS unequip failed");
    AssertPlaced(owner, item, TItemPos(DRAGON_SOUL_INVENTORY, 0));
    Reset();
    const auto actor = PlacementOwner(), timer = Gear(actor);
    Meta(timer).proto.cLimitRealTimeFirstUseIndex = 0; Meta(timer).proto.aLimits[0] = {LIMIT_REAL_TIME_START_FIRST_USE, 60};
    Check(ItemSystem::EquipItemEcs(actor, timer), "first use equip failed");
    Check(ItemSystem::GetItemSocket(timer, 0) > uint32_t(time(nullptr)) && ItemSystem::GetItemSocket(timer, 1) == 1,
        "first-use timer not initialized");
    Check(ItemSystem::UnequipItemEcs(actor, timer), "timer unequip");
    const auto deadline = ItemSystem::GetItemSocket(timer, 0);
    g_registry.get<ecs::ItemSockets>(timer).sockets[1] = -1;
    Check(ItemSystem::EquipItemEcs(actor, timer), "repeat timer equip");
    Check(ItemSystem::GetItemSocket(timer, 0) == deadline && ItemSystem::GetItemSocket(timer, 1) == UINT32_MAX,
        "timer restart or use counter overflow");
}
}

namespace {
void EnableSplitting(entt::entity source)
{
    const auto metadata = Meta(source);
    onCreate = [metadata](uint32_t vnum, uint32_t count) {
        const auto clone = PlacementItem(metadata.proto.bSize);
        g_registry.get<PlacementMeta>(clone) = metadata;
        g_registry.get<ecs::ItemIdentity>(clone).vnum = vnum;
        g_registry.get<ecs::ItemCount>(clone).count = static_cast<int>(count);
        return clone;
    };
}
entt::entity StackAt(entt::entity owner, TItemPos pos, int count = 7, uint8_t size = 1)
{
    const auto item = PlacementItem(size);
    Meta(item).proto.dwFlags |= ITEM_FLAG_STACKABLE;
    Meta(item).dragon = pos.window_type == DRAGON_SOUL_INVENTORY;
    Meta(item).dragonBase = (pos.cell / DRAGON_SOUL_BOX_SIZE) * DRAGON_SOUL_BOX_SIZE;
#ifdef ENABLE_EXTRA_INVENTORY
    Meta(item).extra = pos.window_type == EXTRA_INVENTORY;
    Meta(item).category = pos.cell / EXTRA_INVENTORY_CATEGORY_MAX_NUM;
#endif
    g_registry.get<ecs::ItemCount>(item).count = count;
    Check(ItemSystem::PlaceItemEcs(owner, item, pos.window_type, pos.cell), "move fixture placement");
    return item;
}
void NativeMoves()
{
    std::vector<TItemPos> positions {TItemPos(INVENTORY, 0), TItemPos(DRAGON_SOUL_INVENTORY, 576)};
#ifdef ENABLE_EXTRA_INVENTORY
    positions.emplace_back(EXTRA_INVENTORY, EXTRA_INVENTORY_CATEGORY_MAX_NUM * 4);
#endif
    for (const auto from : positions)
    {
        Reset(); extraUnlock = INT32_MAX;
        const auto owner = PlacementOwner(), item = StackAt(owner, from, 7, 2);
        const TItemPos to(from.window_type, from.cell + 1);
        onSave = [&](entt::entity e) {
            Check(e == item, "move saved another item"); AssertPlaced(owner, item, to);
            Check(ItemSystem::GetItem(owner, from) == entt::null && ItemSystem::GetItemCount(item) == 7,
                "move published partial storage");
            Check(!InventorySystem::MoveItem(owner, to, from, 0), "recursive drag accepted");
        };
        Check(InventorySystem::MoveItem(owner, from, to, 0), "native multi-cell move rejected");
        onSave = {};
        AssertPlaced(owner, item, to);
        // A move may overlap its own footprint. A split may not.
        const int columns = from.window_type == DRAGON_SOUL_INVENTORY ? DRAGON_SOUL_BOX_COLUMN_NUM : INVENTORY_PAGE_COLUMN;
        const TItemPos overlap(to.window_type, to.cell + columns);
        Check(InventorySystem::MoveItem(owner, to, overlap, 0), "overlapping footprint move rejected");
        AssertPlaced(owner, item, overlap);
        Storage(owner, from.window_type, [&](const auto& storage) {
            if constexpr (requires { storage.itemGrid; })
                Check(storage.itemGrid[to.cell] == 0 && storage.itemGrid[overlap.cell] == overlap.cell + 1 &&
                    storage.itemGrid[overlap.cell + columns] == overlap.cell + 1, "move left old footprint");
        });
    }
    Reset();
    auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
    auto& slots = g_registry.emplace<ecs::QuickSlots>(owner);
    slots.slots[0] = {QUICKSLOT_TYPE_ITEM, 0};
    slots.slots[1] = {QUICKSLOT_TYPE_ITEM, 4};
    onPacket = [&](entt::entity e) { AssertPlaced(e, item, TItemPos(INVENTORY, 4)); };
    onSave = [&](entt::entity) { Check(Read(owner, 0).pos == 4 && Read(owner, 1).type == 0, "quickslot lagged item commit"); };
    Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 4), 7), "full-stack move");
    Check(Read(owner, 0).pos == 4 && Read(owner, 1).type == 0, "quickslot relocation not atomic");
    Reset(); owner = PlacementOwner(); item = StackAt(owner, TItemPos(INVENTORY, 0));
    Meta(item).proto.dwFlags = 0;
    Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 1), "non-stackable partial drag");
    Check(ItemSystem::GetItemCount(item) == 7, "non-stackable drag split count");
}
void MoveRejections()
{
    Reset(); extraUnlock = INT32_MAX;
    const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0), 7, 2);
    const TItemPos from(INVENTORY, 0), to(INVENTORY, 1);
    const int saved = saves;
    for (const int count : {INT32_MIN, -1, 8, INT32_MAX})
        Check(!InventorySystem::MoveItem(owner, from, to, count), "invalid count accepted");
    Check(!InventorySystem::MoveItem(owner, from, from, 0), "self move accepted");
    Check(!InventorySystem::MoveItem(entt::null, from, to, 0), "null owner moved");
    for (const uint8_t window : {uint8_t(RESERVED_WINDOW), uint8_t(SAFEBOX), uint8_t(MALL),
            uint8_t(MOUNT_INVENTORY), uint8_t(BELT_INVENTORY), uint8_t(255)})
    {
        Check(!InventorySystem::MoveItem(owner, from, TItemPos(window, 1), 0), "unsupported target window");
        Check(!InventorySystem::MoveItem(owner, TItemPos(window, 0), to, 0), "unsupported source window");
    }
    for (unsigned cell = INVENTORY_AND_EQUIP_SLOT_MAX; cell <= UINT16_MAX; ++cell)
        Check(!InventorySystem::MoveItem(owner, from, TItemPos(INVENTORY, uint16_t(cell)), 0), "oversized target cell");
    Check(!InventorySystem::MoveItem(owner, from, TItemPos(DRAGON_SOUL_INVENTORY, 0), 0), "ordinary item in DS");
    Check(!InventorySystem::MoveItem(owner, from, TItemPos(INVENTORY, INVENTORY_PAGE_SIZE - 1), 0), "cross-page drag");
    for (auto flag : {&PlacementMeta::locked, &PlacementMeta::exchanging, &PlacementMeta::pending})
    {
        Meta(item).*flag = true;
        Check(!InventorySystem::MoveItem(owner, from, to, 0), "restricted source moved");
        Meta(item).*flag = false;
    }
    g_registry.emplace<ecs::StatusFlags>(owner).isObserverMode = true;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "observer moved item");
    g_registry.get<ecs::StatusFlags>(owner).isObserverMode = false;
    const auto other = PlacementOwner();
    g_registry.get<ecs::ItemOwner>(item).owner = other;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "foreign same-PID owner moved");
    g_registry.get<ecs::ItemOwner>(item).owner = owner;
    auto& inv = g_registry.get<ecs::MainInventoryRuntimeComponent>(owner);
    inv.items[10] = item;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "duplicate anchor moved");
    inv.items[10] = entt::null;
    inv.itemGrid[INVENTORY_PAGE_COLUMN] = 0;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "broken source footprint moved");
    inv.itemGrid[INVENTORY_PAGE_COLUMN] = 1;
    const auto blocker = PlacementItem();
    inv.items[1 + INVENTORY_PAGE_COLUMN] = blocker;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "ungridded target anchor overwritten");
    inv.items[1 + INVENTORY_PAGE_COLUMN] = entt::null;
    g_registry.get<ecs::ItemCount>(item).count = 0;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "zero stack moved");
    g_registry.get<ecs::ItemCount>(item).count = -1;
    Check(!InventorySystem::MoveItem(owner, from, to, 0), "negative stack moved");
    Check(saves == saved, "rejected move saved state");
}
void MoveSpecialWindows()
{
#ifdef ENABLE_EXTRA_INVENTORY
    Reset(); extraUnlock = INT32_MAX;
    auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(EXTRA_INVENTORY, 360));
    Check(!InventorySystem::MoveItem(owner, TItemPos(EXTRA_INVENTORY, 360), TItemPos(EXTRA_INVENTORY, 0), 0), "extra category escaped");
    Check(!InventorySystem::MoveItem(owner, TItemPos(EXTRA_INVENTORY, 360), TItemPos(INVENTORY, 0), 0), "extra item escaped inventory");
    extraUnlock = INT32_MIN;
    Check(!InventorySystem::MoveItem(owner, TItemPos(EXTRA_INVENTORY, 360), TItemPos(EXTRA_INVENTORY, 539), 0), "locked extra slot accepted");
#endif
#ifdef ENABLE_SWITCHBOT
    Reset();
    auto switchOwner = PlacementOwner(), switchItem = StackAt(switchOwner, TItemPos(INVENTORY, 0));
    const TItemPos normal(INVENTORY, 0), slot(SWITCHBOT, 2);
    switchAllowed = false;
    Check(!InventorySystem::MoveItem(switchOwner, normal, slot, 0), "invalid switchbot item accepted");
    switchAllowed = true;
    onRegistration = [&] { AssertPlaced(switchOwner, switchItem, slot); };
    Check(InventorySystem::MoveItem(switchOwner, normal, slot, 0), "switchbot insert failed");
    switchActive = true;
    Check(!InventorySystem::MoveItem(switchOwner, slot, normal, 0), "active switchbot source moved");
    switchActive = false;
    onRegistration = [&] { AssertPlaced(switchOwner, switchItem, normal); };
    Check(InventorySystem::MoveItem(switchOwner, slot, normal, 0), "switchbot return failed");
    Check(registrations == 0, "switchbot registration leaked");
#endif
    Reset();
    const auto beltOwner = PlacementOwner(), belt = StackAt(beltOwner, TItemPos(INVENTORY, 0), 1);
    const TItemPos beltPos(INVENTORY, BELT_INVENTORY_SLOT_START);
    Check(!InventorySystem::MoveItem(beltOwner, TItemPos(INVENTORY, 0), beltPos, 0), "non-belt allowed");
    g_registry.get<ecs::ItemIdentity>(belt).vnum = 18000;
    Meta(belt).proto.aLimits[0] = {LIMIT_LEVEL, 101};
    Check(!InventorySystem::MoveItem(beltOwner, TItemPos(INVENTORY, 0), beltPos, 0), "underlevel belt allowed");
    Meta(belt).proto.aLimits[0].lValue = 100;
    int computes = 0;
    onService = [&](const char* name) { if (std::string_view(name) == "compute-points") { ++computes; AssertPlaced(beltOwner, belt, beltPos); } };
    Check(InventorySystem::MoveItem(beltOwner, TItemPos(INVENTORY, 0), beltPos, 0), "belt insert failed");
    onService = {};
    const auto duplicate = StackAt(beltOwner, TItemPos(INVENTORY, 1), 1);
    g_registry.get<ecs::ItemIdentity>(duplicate).vnum = 18009;
    Check(!InventorySystem::MoveItem(beltOwner, TItemPos(INVENTORY, 1), TItemPos(INVENTORY, BELT_INVENTORY_SLOT_START + 1), 0),
        "same belt group duplicated");
    Check(!InventorySystem::MoveItem(beltOwner, beltPos, TItemPos(SAFEBOX, 0), 0), "belt moved to safebox protocol");
    int affects = 0;
    onService = [&](const char* name) {
        AssertPlaced(beltOwner, belt, TItemPos(INVENTORY, 0));
        if (std::string_view(name) == "affect-remove") ++affects;
        if (std::string_view(name) == "compute-points") ++computes;
    };
    Check(InventorySystem::MoveItem(beltOwner, beltPos, TItemPos(INVENTORY, 0), 0), "belt removal failed");
    Check(computes == 2 && affects == 1, "belt points/affect refresh omitted");
    Reset();
    const auto dsOwner = PlacementOwner(), ds = StackAt(dsOwner, TItemPos(DRAGON_SOUL_INVENTORY, 576));
    Check(!InventorySystem::MoveItem(dsOwner, TItemPos(DRAGON_SOUL_INVENTORY, 576), TItemPos(INVENTORY, 0), 0), "DS escaped inventory");
    Check(!InventorySystem::MoveItem(dsOwner, TItemPos(DRAGON_SOUL_INVENTORY, 576), TItemPos(DRAGON_SOUL_INVENTORY, 0), 0), "DS wrong box");
    Check(InventorySystem::MoveItem(dsOwner, TItemPos(DRAGON_SOUL_INVENTORY, 576), TItemPos(DRAGON_SOUL_INVENTORY, 577), 0),
        "DS cell mistaken for belt/equipment");
}
void NativeSplits()
{
    std::vector<TItemPos> positions {TItemPos(INVENTORY, 0), TItemPos(DRAGON_SOUL_INVENTORY, 576)};
#ifdef ENABLE_EXTRA_INVENTORY
    positions.emplace_back(EXTRA_INVENTORY, 360);
#endif
    for (const auto from : positions)
    {
        Reset(); extraUnlock = INT32_MAX;
        const auto owner = PlacementOwner(), item = StackAt(owner, from);
        const TItemPos dest(from.window_type, from.cell + 1);
        g_registry.get<ecs::ItemSockets>(item).sockets[1] = -19;
        g_registry.get<ecs::ItemAttributes>(item).attrs[0] = {APPLY_MAX_HP, 37};
        g_registry.emplace<ecs::ItemLockedAttribute>(item).index = 2;
        g_registry.get<ecs::ItemIdentity>(item).transmutationVnum = 800;
        EnableSplitting(item);
        const auto originalID = ItemSystem::GetItemID(item);
        onSave = [&](entt::entity) {
            const auto clone = ItemSystem::GetItem(owner, dest);
            Check(clone != entt::null && clone != item, "split published before clone anchored");
            AssertPlaced(owner, item, from); AssertPlaced(owner, clone, dest);
            Check(ItemSystem::GetItemCount(item) == 4 && ItemSystem::GetItemCount(clone) == 3, "partial split count visible");
            Check(!InventorySystem::MoveItem(owner, from, TItemPos(from.window_type, from.cell + 2), 1), "recursive split accepted");
        };
        Check(InventorySystem::MoveItem(owner, from, dest, 3), "native split failed");
        const auto clone = ItemSystem::GetItem(owner, dest);
        Check(ItemSystem::GetItemID(clone) != originalID, "split copied persistent item identity");
        Check(g_registry.get<ecs::ItemSockets>(clone).sockets[1] == -19 &&
            g_registry.get<ecs::ItemAttributes>(clone).attrs[0].sValue == 37 &&
            g_registry.get<ecs::ItemLockedAttribute>(clone).index == 2 &&
            g_registry.get<ecs::ItemIdentity>(clone).transmutationVnum == 800, "split lost payload");
        Check(retiredSplits == 0, "committed split retired");
    }
    Reset();
    const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0), 7, 2);
    Check(!InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, INVENTORY_PAGE_COLUMN), 2),
        "split overlapped source footprint");
    onCreate = [](uint32_t, uint32_t) { return entt::entity(entt::null); };
    Check(!InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 2), "failed allocation committed");
    Check(ItemSystem::GetItemCount(item) == 7, "allocation failure debited source");
}
void SplitFailuresAndCallbacks()
{
    for (int scenario = 0; scenario < 11; ++scenario)
    {
        Reset();
        const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
        const TItemPos from(INVENTORY, 0), dest(INVENTORY, 1);
        EnableSplitting(item);
        const auto create = onCreate;
        entt::entity clone = entt::null, replacement = entt::null;
        onCreate = [&](uint32_t vnum, uint32_t count) {
            clone = create(vnum, count);
            switch (scenario)
            {
            case 0: Meta(item).locked = true; break;
            case 1: g_registry.get<ecs::ItemCount>(item).count = 2; break;
            case 2: g_registry.get<ecs::ItemSockets>(item).sockets[0] = 99; break;
            case 3: g_registry.get<ecs::ItemAttributes>(item).attrs[0] = {APPLY_MAX_HP, 9}; break;
            case 4: replacement = StackAt(owner, dest); break;
            case 5: g_registry.destroy(item); replacement = PlacementItem(); break;
            case 6: g_registry.destroy(owner); replacement = PlacementOwner(); break;
            case 7: g_registry.get<ecs::ItemCount>(clone).count = 1; break; // clamped factory
            case 8: Meta(clone).locked = true; break;
            case 9: Meta(item).proto.dwAntiFlags |= ITEM_ANTIFLAG_STACK; break;
            case 10: Meta(item).pending = true; break;
            }
            return clone;
        };
        Check(!InventorySystem::MoveItem(owner, from, dest, 3), "mutated preparation committed");
        Check(!g_registry.valid(clone) && retiredSplits == 1, "failed prepared item leaked");
        if (g_registry.valid(item))
            Check(ItemSystem::GetItemCount(item) == (scenario == 1 ? 2 : 7), "failed preparation debited source");
        if (scenario == 4) AssertPlaced(owner, replacement, dest);
        if (scenario == 5 || scenario == 6) Check(replacement != (scenario == 5 ? item : owner), "generation not changed");
    }
    for (bool destroyOwner : {false, true})
    {
        Reset();
        const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
        EnableSplitting(item);
        entt::entity clone = entt::null, replacement = entt::null;
        onSave = [&](entt::entity e) {
            Check(e == item, "split source must publish first");
            clone = ItemSystem::GetItem(owner, TItemPos(INVENTORY, 1));
            Check(ItemSystem::GetItemCount(item) == 4 && ItemSystem::GetItemCount(clone) == 3, "split save precedes commit");
            onSave = {};
            g_registry.destroy(destroyOwner ? owner : item); replacement = g_registry.create();
        };
        Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 3),
            "committed split deletion reported failure");
        Check(retiredSplits == 0 && g_registry.valid(clone) &&
            !g_registry.any_of<ecs::ItemCount, ecs::MainInventoryRuntimeComponent>(replacement), "split replayed onto replacement");
    }
    Reset();
    const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0)), other = PlacementOwner();
    EnableSplitting(item);
    entt::entity movedClone = entt::null;
    onSave = [&](entt::entity) {
        onSave = {};
        movedClone = ItemSystem::GetItem(owner, TItemPos(INVENTORY, 1));
        Check(ItemSystem::RemoveItemEcs(movedClone) && ItemSystem::PlaceItemEcs(other, movedClone, INVENTORY, 9),
            "nested split transfer failed");
    };
    Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 3), "transferred split not committed");
    AssertPlaced(other, movedClone, TItemPos(INVENTORY, 9));
    Check(ItemSystem::GetItemCount(item) == 4 && retiredSplits == 0, "transferred clone refunded or retired");
}
void MovePreparationSignals()
{
    for (bool wasSplit : {false, true})
    {
        Reset();
        const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
        g_registry.emplace<ecs::QuickSlots>(owner).slots[0] = {QUICKSLOT_TYPE_ITEM, 0};
        ConstructionCallback callback {[&](entt::registry& registry, entt::entity) {
            registry.get<ecs::ItemCount>(item).count = wasSplit ? 3 : 10;
        }};
        entt::scoped_connection connection = g_registry.on_construct<ecs::DirtyTag>().connect<&ConstructionCallback::OnConstruct>(callback);
        Check(!InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), wasSplit ? 3 : 7),
            "preparation changed full-move/split semantics");
        AssertPlaced(owner, item, TItemPos(INVENTORY, 0));
        Check(ItemSystem::GetItemCount(item) == (wasSplit ? 3 : 10), "reclassified request debited source");
    }
    for (bool duringSplit : {false, true})
        for (bool destroyOwner : {false, true})
        {
            Reset();
            const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
            g_registry.emplace<ecs::QuickSlots>(owner).slots[0] = {QUICKSLOT_TYPE_ITEM, 0};
            EnableSplitting(item);
            entt::entity replacement = entt::null;
            ConstructionCallback callback {[&](entt::registry& registry, entt::entity e) {
                Check(ItemSystem::GetItemCount(item) == 7, "construction preceded count commit");
                registry.destroy(destroyOwner ? owner : e); replacement = registry.create();
            }};
            entt::scoped_connection connection = duringSplit ?
                g_registry.on_construct<ecs::ItemOwner>().connect<&ConstructionCallback::OnConstruct>(callback) :
                g_registry.on_construct<ecs::DirtyTag>().connect<&ConstructionCallback::OnConstruct>(callback);
            Check(!InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), duringSplit ? 3 : 0),
                "component destruction ignored");
            Check(ItemSystem::GetItemCount(item) == 7 &&
                !g_registry.any_of<ecs::ItemCount, ecs::ItemLocation, ecs::MainInventoryRuntimeComponent>(replacement),
                "failed component preparation changed source or replacement");
        }
    for (bool destroyClone : {false, true})
    {
        Reset();
        const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
        EnableSplitting(item);
        const auto create = onCreate;
        entt::entity clone = entt::null;
        onCreate = [&](uint32_t vnum, uint32_t count) {
            clone = create(vnum, count);
            g_registry.emplace<ecs::ItemEvents>(clone).destroy = LPEVENT(new EVENT);
            return clone;
        };
        onCancel = [&] {
            onCancel = {};
            Check(ItemSystem::GetItemCount(item) == 7, "cancel callback saw pre-debit");
            if (destroyClone) g_registry.destroy(clone);
            else Meta(item).locked = true;
        };
        Check(!InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 3),
            "timer cancellation mutation ignored");
        Check(ItemSystem::GetItemCount(item) == 7 && !g_registry.valid(clone), "timer failure leaked a split");
    }
    for (bool destroyOwner : {false, true})
    {
        Reset();
        const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0));
        entt::entity replacement = entt::null;
        onStoragePacket = [&](entt::entity, uint8_t, TItemPos) {
            onStoragePacket = {};
            g_registry.destroy(destroyOwner ? owner : item); replacement = g_registry.create();
        };
        Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 0),
            "post-commit packet deletion reported failure");
        Check(!g_registry.any_of<ecs::ItemCount, ecs::ItemLocation, ecs::MainInventoryRuntimeComponent>(replacement),
            "move publication wrote recycled entity");
    }
}

void MoveCallbacksAndDispatch()
{
    for (bool packet : {false, true})
    {
        Reset();
        const auto owner = PlacementOwner(), item = StackAt(owner, TItemPos(INVENTORY, 0)), other = PlacementOwner();
        const TItemPos from(INVENTORY, 0), dest(INVENTORY, 1);
        auto moveAgain = [&] {
            Check(ItemSystem::RemoveItemEcs(item), "callback detach failed");
            Check(ItemSystem::PlaceItemEcs(other, item, INVENTORY, 7), "callback transfer failed");
            StackAt(owner, dest); // Must not be overwritten by the outer publication.
        };
        if (packet)
            onStoragePacket = [&](entt::entity, uint8_t, TItemPos) { onStoragePacket = {}; moveAgain(); };
        else onSave = [&](entt::entity) { onSave = {}; moveAgain(); };
        Check(InventorySystem::MoveItem(owner, from, dest, 0), "committed relocation reported failure");
        AssertPlaced(other, item, TItemPos(INVENTORY, 7));
        Check(ItemSystem::GetItem(owner, dest) != item && ItemSystem::GetItem(owner, from) == entt::null,
            "callback replacement overwritten");
    }
    Reset();
    auto owner = PlacementOwner(), source = StackAt(owner, TItemPos(INVENTORY, 0)), target = StackAt(owner, TItemPos(INVENTORY, 1));
    onMerge = [&](entt::entity e, entt::entity a, entt::entity b, uint32_t n) {
        Check(e == owner && a == source && b == target && n == 3, "merge dispatch identity/count changed");
        return ItemSystem::StackMergeResult {3, false};
    };
    Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), TItemPos(INVENTORY, 1), 3), "native merge result ignored");
    Reset();
    owner = PlacementOwner(); source = Gear(owner);
    const TItemPos wear(INVENTORY, INVENTORY_MAX_NUM + WEAR_BODY);
    Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, 0), wear, 0), "equip drag failed");
    AssertWorn(owner, source, WEAR_BODY);
    Check(!InventorySystem::MoveItem(owner, wear, TItemPos(EQUIPMENT, wear.cell), 0), "equipment aliases not normalized");
    Check(InventorySystem::MoveItem(owner, wear, TItemPos(INVENTORY, 9), 0), "unequip drag failed");
    AssertPlaced(owner, source, TItemPos(INVENTORY, 9));
    Meta(source).dragon = true; Meta(source).wear = WEAR_MAX_NUM;
    Meta(source).proto.bType = ITEM_DS;
    Check(ItemSystem::EquipItemEcs(owner, source), "DS dispatch setup");
    onPullOut = [&](entt::entity e, TItemPos pos, entt::entity& stone) {
        Check(e == owner && stone == source && pos == TItemPos(DRAGON_SOUL_INVENTORY, 2), "DS extraction dispatch changed");
        return true;
    };
    Check(InventorySystem::MoveItem(owner, TItemPos(INVENTORY, INVENTORY_MAX_NUM + WEAR_MAX_NUM),
        TItemPos(DRAGON_SOUL_INVENTORY, 2), 0), "DS extraction not dispatched");
}
} // namespace

int main() {
    try {
        DSManager dragonSouls;
        ITEM_MANAGER items;
        LogManager logs;
        marriage::CManager marriages;
        quest::CQuestManager quests;
#ifdef ENABLE_SWITCHBOT
        CSwitchbotManager switchbots;
#endif
        MovePreparationSignals(); NativeMoves(); MoveRejections(); MoveSpecialWindows(); NativeSplits(); SplitFailuresAndCallbacks(); MoveCallbacksAndDispatch();
        EquipmentPolicies(); EquipmentRoundTripAndSwap(); EquipmentCallbacks(); EquipmentDragonSoulAndTimers();
        PlacementWindows(); PlacementValidation(); PlacementQueries(); PlacementCallbacks(); NativeUnequip(); RemovalCallbacksAndAliases(); PurePlacementRules();
        InventoryGuards(); InventoryGrids(); Basic(); DuplicatesAndValidation(); SyncAndLifetime(); ValueRanges(); ClientValidation(); HydrationAndRelocation();
        std::cout << "Quickslot checks passed: " << checks << '\n'; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
