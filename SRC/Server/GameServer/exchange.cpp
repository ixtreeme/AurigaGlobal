#include "stdafx.h"
#include "exchange.h"
#include "ecs/Registry.hpp"
#include "ecs/components/character_runtime_components.hpp"
#include "ecs/components/dirty_components.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/components/inventory_components.hpp"
#include "ecs/components/social_components.hpp"
#include "ecs/components/status_components.hpp"
#include "ecs/systems/InventorySystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/SessionSystem.hpp"
#include "ecs/systems/ChatSystem.hpp"
#include "char_manager.h"
#include "config.h"
#include "db.h"
#include "desc.h"
#include "desc_client.h"
#include "DragonSoul.h"
#include "log.h"
#include "packet.h"
#include "questmanager.h"
#include "utils.h"
#include <Core/Logging.hpp>
#include <algorithm>
#include <limits>
#include <set>
#include <vector>

namespace
{
using Session = ecs::ExchangeSession;
using Offer = ecs::ExchangeOffer;
using OfferedItem = ecs::ExchangeItem;
using Phase = ecs::ExchangePhase;

Session* FindSession(entt::entity session)
{
    return g_registry.valid(session) ? g_registry.try_get<Session>(session) : nullptr;
}

int SideOf(const Session& state, entt::entity participant)
{
    for (int side = 0; side < 2; ++side)
        if (state.offers[side].owner == participant)
            return side;
    return -1;
}

bool Current(entt::entity session, uint64_t revision)
{
    const auto* state = FindSession(session);
    return state && state->phase == Phase::Open && state->revision == revision;
}

LPDESC Descriptor(entt::entity participant)
{
    if (!ecs::PlayerRuntime::IsPC(participant))
        return nullptr;
    auto* desc = ecs::PlayerRuntime::GetDesc(participant);
    return desc && desc->GetEntity() == participant ? desc : nullptr;
}

void Info(entt::entity participant, uint32_t message, const std::string& argument = "")
{
#ifdef TEXTS_IMPROVEMENT
    if (Descriptor(participant))
        ecs::ChatSystem::SendNew(participant, CHAT_TYPE_INFO, message, "%s", argument.c_str());
#endif
}

void Packet(entt::entity participant, uint8_t subHeader, bool self = false,
    int64_t arg1 = 0, TItemPos arg2 = NPOS, uint32_t arg3 = 0, const OfferedItem* item = nullptr)
{
    auto* desc = Descriptor(participant);
    if (!desc)
        return;
    packet_exchange packet {};
    packet.header = HEADER_GC_EXCHANGE;
    packet.sub_header = subHeader;
    packet.is_me = self;
    packet.arg1 = arg1;
    packet.arg2 = arg2;
    packet.arg3 = arg3;
#ifdef ATTR_LOCK
    packet.lockedattr = item ? item->lockedAttribute : -1;
#endif
#ifdef WJ_ENABLE_TRADABLE_ICON
    packet.arg4 = item ? item->source : TItemPos(RESERVED_WINDOW, 0);
#endif
    if (item)
    {
        std::copy(item->sockets.begin(), item->sockets.end(), packet.alSockets);
        std::copy(item->attributes.begin(), item->attributes.end(), packet.aAttr);
    }
    desc->Packet(&packet, sizeof(packet));
}

void PairPacket(entt::entity session, uint64_t revision, int side, uint8_t subHeader,
    int64_t arg1 = 0, TItemPos arg2 = NPOS, uint32_t arg3 = 0, const OfferedItem* item = nullptr)
{
    const auto* state = FindSession(session);
    if (!state || !Current(session, revision))
        return;
    const auto first = state->offers[side].owner, second = state->offers[1 - side].owner;
    Packet(first, subHeader, true, arg1, arg2, arg3, item);
    if (Current(session, revision))
        Packet(second, subHeader, false, arg1, arg2, arg3, item);
}

void Unlock(const Session& snapshot, entt::entity session)
{
    for (const auto& offer : snapshot.offers)
    {
        const auto* current = g_registry.valid(offer.owner)
            ? g_registry.try_get<ecs::ExchangeRef>(offer.owner) : nullptr;
        if (current && current->session != session && FindSession(current->session))
            continue;
        for (const auto& selection : offer.items)
            if (ItemSystem::IsValidItem(selection.item) &&
                ItemSystem::GetItemOwner(selection.item) == offer.owner)
                if (auto* flags = g_registry.try_get<ecs::ItemFlags>(selection.item))
                    flags->exchanging = false;
    }
}

void Close(entt::entity session, bool destroySession = true)
{
    auto* state = FindSession(session);
    if (!state || state->phase == Phase::Closing)
        return;
    const Session snapshot = *state;
    state->phase = Phase::Closing;
    ++state->revision;
    Unlock(snapshot, session);

    // Keep participant reservations until both END packets have been delivered.
    // A packet callback cannot start a replacement trade and have it closed by
    // the old operation's second packet.
    for (const auto& offer : snapshot.offers)
        Packet(offer.owner, EXCHANGE_SUBHEADER_GC_END);
    for (const auto& offer : snapshot.offers)
        if (g_registry.valid(offer.owner))
            if (auto* ref = g_registry.try_get<ecs::ExchangeRef>(offer.owner);
                ref && ref->session == session)
                ref->session = entt::null;
    if (destroySession && g_registry.valid(session))
        g_registry.destroy(session);
}

void ParticipantDestroyed(entt::registry& registry, entt::entity participant)
{
    const auto session = registry.get<ecs::ExchangeRef>(participant).session;
    Close(session);
}

void SessionDestroyed(entt::registry&, entt::entity session)
{
    Close(session, false);
}

void EnsureLifecycle()
{
    struct HooksInstalled {};
    if (g_registry.ctx().contains<HooksInstalled>())
        return;
    g_registry.on_destroy<ecs::ExchangeRef>().connect<&ParticipantDestroyed>();
    g_registry.on_destroy<Session>().connect<&SessionDestroyed>();
    g_registry.ctx().emplace<HooksInstalled>();
}

bool CanTrade(entt::entity participant)
{
    if (!Descriptor(participant) || !InventorySystem::CanHandleItems(participant))
        return false;
    const auto* flags = g_registry.try_get<ecs::StatusFlags>(participant);
    if (flags && (flags->isDead || flags->isStunned || flags->isObserverMode))
        return false;
    if (ecs::SessionSystem::IsSafeboxOpen(participant) ||
        ecs::SessionSystem::IsCubeOpen(participant))
        return false;
    if (const auto* shop = g_registry.try_get<ecs::ShopState>(participant);
        shop && (shop->myShop || shop->currentShop || shop->shopOwner != entt::null
#ifdef __ENABLE_NEW_OFFLINESHOP__
            || shop->offlineShopGuest || shop->auctionGuest
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
            || shop->wheelDestiny
#endif
        ))
        return false;
    return true;
}

bool Near(entt::entity first, entt::entity second)
{
    if (ecs::PlayerRuntime::GetMapIndex(first) != ecs::PlayerRuntime::GetMapIndex(second))
        return false;
    const int64_t dx = std::abs(int64_t(ecs::PlayerRuntime::GetX(first)) - ecs::PlayerRuntime::GetX(second));
    const int64_t dy = std::abs(int64_t(ecs::PlayerRuntime::GetY(first)) - ecs::PlayerRuntime::GetY(second));
    return std::max(dx, dy) + std::min(dx, dy) / 2 < EXCHANGE_MAX_DISTANCE;
}

bool SafeboxDelayPassed(entt::entity participant)
{
    const auto* timing = g_registry.try_get<ecs::WarpBlockState>(participant);
    if (timing && int64_t(thecore_pulse()) - timing->safeboxLoadTime < PASSES_PER_SEC(g_nPortalLimitTime))
    {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(participant, CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
        return false;
    }
    return true;
}

bool SourceWindow(TItemPos position)
{
    if (position.window_type == INVENTORY)
        return position.cell < INVENTORY_MAX_NUM || position.IsBeltInventoryPosition();
    if (position.window_type == DRAGON_SOUL_INVENTORY)
        return position.cell < DRAGON_SOUL_INVENTORY_MAX_NUM;
#ifdef ENABLE_EXTRA_INVENTORY
    if (position.window_type == EXTRA_INVENTORY)
        return position.cell < EXTRA_INVENTORY_MAX_NUM;
#endif
    return false;
}

bool CaptureItem(entt::entity owner, TItemPos source, OfferedItem& output, bool reserved)
{
    if (!SourceWindow(source))
        return false;
    const auto item = ItemSystem::GetItem(owner, source);
    if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemOwner(item) != owner ||
        !g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation, ecs::ItemFlags>(item) ||
        ItemSystem::GetItemWindow(item) != source.window_type ||
        ItemSystem::GetItemCell(item) != source.cell ||
        ItemSystem::IsItemEquipped(item) || ItemSystem::IsItemLocked(item) ||
        ItemSystem::GetItemSkipSave(item) ||
        ItemSystem::IsItemExchanging(item) != reserved ||
        (ItemSystem::GetItemAntiFlag(item) & ITEM_ANTIFLAG_GIVE) ||
        ItemSystem::GetItemCount(item) == 0 || ItemSystem::GetItemSize(item) == 0)
        return false;
    output.item = item;
    output.source = source;
    output.id = ItemSystem::GetItemID(item);
    output.vnum = ItemSystem::GetItemVnum(item);
    output.count = ItemSystem::GetItemCount(item);
    output.size = ItemSystem::GetItemSize(item);
#ifdef ATTR_LOCK
    output.lockedAttribute = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
    for (size_t i = 0; i < output.sockets.size(); ++i)
        output.sockets[i] = ItemSystem::GetItemSocket(item, i);
    for (size_t i = 0; i < output.attributes.size(); ++i)
        output.attributes[i] = ItemSystem::GetItemAttribute(item, i);
    return true;
}

bool Unchanged(entt::entity owner, const OfferedItem& offered)
{
    OfferedItem current;
    if (!CaptureItem(owner, offered.source, current, true) ||
        current.item != offered.item || current.id != offered.id ||
        current.vnum != offered.vnum || current.count != offered.count ||
        current.size != offered.size || current.sockets != offered.sockets ||
        current.lockedAttribute != offered.lockedAttribute)
        return false;
    for (size_t i = 0; i < current.attributes.size(); ++i)
        if (current.attributes[i].bType != offered.attributes[i].bType ||
            current.attributes[i].sValue != offered.attributes[i].sValue)
            return false;
    return true;
}

bool DisplayFits(const Offer& offer, uint32_t cell, uint8_t size)
{
#ifdef __NEW_EXCHANGE_WINDOW__
    constexpr int columns = 6;
#else
    constexpr int columns = 4;
#endif
    if (size == 0 || cell >= EXCHANGE_ITEM_MAX_NUM ||
        uint64_t(cell) + uint64_t(size - 1) * columns >= EXCHANGE_ITEM_MAX_NUM)
        return false;
    for (const auto& other : offer.items)
        if (other.item != entt::null)
            for (int row = 0; row < other.size; ++row)
                for (int ownRow = 0; ownRow < size; ++ownRow)
                    if (other.display + row * columns == cell + ownRow * columns)
                        return false;
    return true;
}

void ClearAccepts(Session& state)
{
    for (auto& offer : state.offers)
        offer.accepted = false;
    ++state.revision;
}

void PublishAccepts(entt::entity session, uint64_t revision)
{
    PairPacket(session, revision, 0, EXCHANGE_SUBHEADER_GC_ACCEPT, false);
    PairPacket(session, revision, 1, EXCHANGE_SUBHEADER_GC_ACCEPT, false);
}

// All inventories below are value snapshots. Planning never edits live slots;
// both participants' outgoing items are freed before incoming slots are chosen.
struct Inventories
{
    ecs::MainInventoryRuntimeComponent main;
    ecs::DragonSoulInventoryComponent dragonSoul;
#ifdef ENABLE_EXTRA_INVENTORY
    ecs::ExtraInventoryRuntimeComponent extra;
#endif
};

bool HasInventories(entt::entity owner)
{
    return g_registry.valid(owner) && g_registry.all_of<ecs::MainInventoryRuntimeComponent,
        ecs::DragonSoulInventoryComponent, ecs::GoldAmount>(owner)
#ifdef ENABLE_EXTRA_INVENTORY
        && g_registry.all_of<ecs::ExtraInventoryRuntimeComponent>(owner)
#endif
        ;
}

Inventories Snapshot(entt::entity owner)
{
    Inventories result;
    if (const auto* value = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(owner)) result.main = *value;
    if (const auto* value = g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner)) result.dragonSoul = *value;
#ifdef ENABLE_EXTRA_INVENTORY
    if (const auto* value = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner)) result.extra = *value;
#endif
    return result;
}

bool Matches(entt::entity owner, const Inventories& saved)
{
    if (!HasInventories(owner)) return false;
    const auto current = Snapshot(owner);
    return current.main.items == saved.main.items && current.main.itemGrid == saved.main.itemGrid &&
        current.dragonSoul.items == saved.dragonSoul.items && current.dragonSoul.itemGrid == saved.dragonSoul.itemGrid
#ifdef ENABLE_EXTRA_INVENTORY
        && current.extra.items == saved.extra.items && current.extra.itemGrid == saved.extra.itemGrid
#endif
        ;
}

size_t References(const Inventories& inventories, entt::entity item)
{
    return std::count(inventories.main.items.begin(), inventories.main.items.end(), item) +
        std::count(inventories.dragonSoul.items.begin(), inventories.dragonSoul.items.end(), item)
#ifdef ENABLE_EXTRA_INVENTORY
        + std::count(inventories.extra.items.begin(), inventories.extra.items.end(), item)
#endif
        ;
}

template <typename Function>
bool WithInventory(Inventories& inventories, TItemPos position, Function&& function)
{
    if (position.window_type == INVENTORY)
        return function(inventories.main, position.IsBeltInventoryPosition() ? 1 : INVENTORY_PAGE_COLUMN,
            position.IsBeltInventoryPosition() ? INVENTORY_AND_EQUIP_SLOT_MAX : INVENTORY_PAGE_SIZE);
    if (position.window_type == DRAGON_SOUL_INVENTORY)
        return function(inventories.dragonSoul, DRAGON_SOUL_BOX_COLUMN_NUM, DRAGON_SOUL_BOX_SIZE);
#ifdef ENABLE_EXTRA_INVENTORY
    if (position.window_type == EXTRA_INVENTORY)
        return function(inventories.extra, EXTRA_INVENTORY_PAGE_COLUMN, EXTRA_INVENTORY_PAGE_SIZE);
#endif
    return false;
}

bool ClearSlot(Inventories& inventories, const OfferedItem& selection)
{
    return WithInventory(inventories, selection.source, [&](auto& inventory, int columns, int pageSize) {
        const size_t cell = selection.source.cell;
        if (cell >= inventory.items.size() || inventory.items[cell] != selection.item)
            return false;
        for (int row = 0; row < selection.size; ++row)
        {
            const auto covered = cell + row * columns;
            if (covered >= inventory.items.size() || covered / pageSize != cell / pageSize ||
                inventory.itemGrid[covered] != cell + 1 ||
                (covered != cell && inventory.items[covered] != entt::null))
                return false;
        }
        for (int row = 0; row < selection.size; ++row)
            inventory.itemGrid[cell + row * columns] = 0;
        inventory.items[cell] = entt::null;
        return true;
    });
}

int ExtraLimit(entt::entity owner, int category)
{
#ifdef ENABLE_EXTRA_INVENTORY
    const int begin = category * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
    static constexpr std::array<std::string_view, 6> flags {
        "lock_extra.cat1", "lock_extra.cat2", "lock_extra.cat3",
        "lock_extra.cat4", "lock_extra.cat5", "lock_extra.cat6"
    };
    const int64_t unlocked = std::clamp<int64_t>(
        int64_t(ecs::QuestSystem::GetFlag(owner, flags[category])) * 5, 0, 25 + EXTRA_INVENTORY_PAGE_SIZE);
    return std::min(begin + EXTRA_INVENTORY_CATEGORY_MAX_NUM,
        begin + EXTRA_INVENTORY_PAGE_SIZE * 2 + 20 + int(unlocked));
#else
    return begin + EXTRA_INVENTORY_CATEGORY_MAX_NUM;
#endif
#else
    return 0;
#endif
}

TItemPos Place(Inventories& inventories, entt::entity owner, const OfferedItem& selection)
{
    int begin = 0, end = InventorySystem::GetInventorySize(owner);
    uint8_t window = INVENTORY;
    if (ItemSystem::IsDragonSoulItem(selection.item))
    {
        window = DRAGON_SOUL_INVENTORY;
        begin = DSManager::instance().GetBasePosition(selection.item);
        if (begin == std::numeric_limits<uint16_t>::max() || begin + DRAGON_SOUL_BOX_SIZE > DRAGON_SOUL_INVENTORY_MAX_NUM)
            return NPOS;
        end = begin + DRAGON_SOUL_BOX_SIZE;
    }
#ifdef ENABLE_EXTRA_INVENTORY
    else if (ItemSystem::IsExtraItem(selection.item))
    {
        const int category = ItemSystem::GetItemExtraCategory(selection.item);
        if (category < 0 || category >= 6)
            return NPOS;
        window = EXTRA_INVENTORY;
        begin = category * EXTRA_INVENTORY_CATEGORY_MAX_NUM;
        end = ExtraLimit(owner, category);
    }
#endif
    for (int cell = begin; cell < end; ++cell)
    {
        const TItemPos position(window, cell);
        if (WithInventory(inventories, position, [&](auto& inventory, int columns, int pageSize) {
            for (int row = 0; row < selection.size; ++row)
            {
                const auto covered = size_t(cell) + row * columns;
                if (covered >= size_t(end) || covered >= inventory.items.size() ||
                    covered / pageSize != size_t(cell) / pageSize ||
                    inventory.itemGrid[covered] || inventory.items[covered] != entt::null)
                    return false;
            }
            inventory.items[cell] = selection.item;
            for (int row = 0; row < selection.size; ++row)
                inventory.itemGrid[cell + row * columns] = cell + 1;
            return true;
        }))
            return position;
    }
    return NPOS;
}

struct Transfer
{
    OfferedItem selection;
    entt::entity from, to;
    TItemPos destination;
    uint32_t fromPID, toPID;
    std::string name;
};

void SendSlot(entt::entity owner, TItemPos position)
{
    auto* desc = Descriptor(owner);
    if (!desc)
        return;
    const auto item = ItemSystem::GetItem(owner, position);
    if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemOwner(item) == owner)
    {
        TPacketGCItemSet packet {};
        packet.header = HEADER_GC_ITEM_SET;
        packet.Cell = position;
        packet.vnum = ItemSystem::GetItemVnum(item);
        packet.count = ItemSystem::GetItemCount(item);
        packet.flags = ItemSystem::GetItemFlags(item);
        packet.anti_flags = ItemSystem::GetItemAntiFlag(item);
        packet.highlight = true;
#ifdef ATTR_LOCK
        packet.lockedattr = ItemSystem::GetItemLockedAttributeIndex(item);
#endif
        for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) packet.alSockets[i] = ItemSystem::GetItemSocket(item, i);
        for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i) packet.aAttr[i] = ItemSystem::GetItemAttribute(item, i);
        desc->Packet(&packet, sizeof(packet));
    }
    else if (item == entt::null)
    {
        TPacketGCItemDelDeprecated packet {};
        packet.header = HEADER_GC_ITEM_DEL;
        packet.Cell = position;
#ifdef ATTR_LOCK
        packet.lockedattr = -1;
#endif
        desc->Packet(&packet, sizeof(packet));
    }
}

void TouchExchangeTime(entt::entity owner)
{
    if (g_registry.valid(owner))
        g_registry.get_or_emplace<ecs::WarpBlockState>(owner).exchangeTime = thecore_pulse();
}

bool Complete(entt::entity session)
{
    const auto* live = FindSession(session);
    if (!live || live->phase != Phase::Open)
        return false;
    const Session snapshot = *live;
    const auto first = snapshot.offers[0].owner, second = snapshot.offers[1].owner;
    if (!CanTrade(first) || !CanTrade(second) || !Near(first, second) ||
        !SafeboxDelayPassed(first) || !SafeboxDelayPassed(second) ||
        ExchangeSystem::GetSession(first) != session || ExchangeSystem::GetSession(second) != session)
        return false;
    if (!db_clientdesc || db_clientdesc->GetSocket() == INVALID_SOCKET)
    {
        Info(first, 759); Info(second, 759);
        return false;
    }
    for (const auto& offer : snapshot.offers)
    {
        auto* quest = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(offer.owner));
        if (!quest || quest->IsRunning())
        {
            Info(first, 631); Info(second, 631);
            return false;
        }
    }

    // Player factories install inventory and gold components before login.
    // Missing state is a failed validation, never a reason to create/reset it.
    if (!Current(session, snapshot.revision) || !HasInventories(first) || !HasInventories(second))
        return false;
    const std::array<Inventories, 2> original { Snapshot(first), Snapshot(second) };
    auto planned = original;
    const std::array<uint32_t, 2> pids { ecs::PlayerRuntime::GetPlayerID(first), ecs::PlayerRuntime::GetPlayerID(second) };
    const std::array<std::string, 2> names { std::string(ecs::PlayerRuntime::GetName(first)), std::string(ecs::PlayerRuntime::GetName(second)) };
    std::array<int64_t, 2> gold { ecs::PointSystem::GetGold(first), ecs::PointSystem::GetGold(second) };
    std::array<int64_t, 2> balances {};
    std::vector<Transfer> transfers;
    transfers.reserve(EXCHANGE_ITEM_MAX_NUM * 2);
    std::set<entt::entity> unique;
    for (int side = 0; side < 2; ++side)
    {
        const auto& offer = snapshot.offers[side];
        if (offer.gold < 0 || gold[side] < offer.gold || gold[side] < 0 || gold[side] >= GOLD_MAX)
            return false;
        const auto remaining = gold[side] - offer.gold;
        if (snapshot.offers[1 - side].gold < 0 || snapshot.offers[1 - side].gold >= GOLD_MAX - remaining)
            return false;
        balances[side] = remaining + snapshot.offers[1 - side].gold;
        for (const auto& item : offer.items)
            if (item.item != entt::null &&
                (!unique.insert(item.item).second || References(original[0], item.item) + References(original[1], item.item) != 1 ||
                    !Unchanged(offer.owner, item) || !ClearSlot(planned[side], item)))
                return false;
    }
    for (int side = 0; side < 2; ++side)
        for (const auto& item : snapshot.offers[side].items)
            if (item.item != entt::null)
            {
                const auto destination = Place(planned[1 - side], snapshot.offers[1 - side].owner, item);
                if (destination == NPOS)
                {
                    const std::string name(ecs::PlayerRuntime::GetName(snapshot.offers[1 - side].owner));
                    Info(snapshot.offers[side].owner, 365, name);
                    Info(snapshot.offers[1 - side].owner, 366);
                    return false;
                }
                transfers.push_back({item, snapshot.offers[side].owner, snapshot.offers[1 - side].owner, destination,
                    pids[side], pids[1 - side], std::string(ItemSystem::GetItemName(item.item))});
            }

    if (!CanTrade(first) || !CanTrade(second) || !Near(first, second) ||
        !Current(session, snapshot.revision) || ExchangeSystem::GetSession(first) != session ||
        ExchangeSystem::GetSession(second) != session ||
        !Matches(first, original[0]) || !Matches(second, original[1]))
        return false;
    for (int side = 0; side < 2; ++side)
        if (ecs::PointSystem::GetGold(snapshot.offers[side].owner) != gold[side])
            return false;
    for (const auto& transfer : transfers)
        if (!Unchanged(transfer.from, transfer.selection))
            return false;

    // No failure path or external callback after the first live store write.
    // A transferred item never becomes temporarily ownerless, and additions do
    // not rerun creation/autoequip/random-accessory initialization rules.
    for (int side = 0; side < 2; ++side)
    {
        const auto owner = snapshot.offers[side].owner;
        g_registry.get<ecs::MainInventoryRuntimeComponent>(owner) = planned[side].main;
        g_registry.get<ecs::DragonSoulInventoryComponent>(owner) = planned[side].dragonSoul;
#ifdef ENABLE_EXTRA_INVENTORY
        g_registry.get<ecs::ExtraInventoryRuntimeComponent>(owner) = planned[side].extra;
#endif
        g_registry.get<ecs::GoldAmount>(owner).amount = balances[side];
    }
    for (const auto& transfer : transfers)
    {
        auto& owner = g_registry.get<ecs::ItemOwner>(transfer.selection.item);
        owner.owner = transfer.to;
        owner.ownerPID = transfer.toPID;
        owner.lastOwnerPID = owner.ownerPID;
        auto& location = g_registry.get<ecs::ItemLocation>(transfer.selection.item);
        location.window = transfer.destination.window_type;
        location.cell = transfer.destination.cell;
        g_registry.get<ecs::ItemFlags>(transfer.selection.item).exchanging = false;
    }
    // Quickslot bindings refer to the outgoing item, even if an incoming item
    // reuses its cell. Mutate them before publication, then guard by revision.
    std::array<std::array<bool, QUICKSLOT_MAX_NUM>, 2> changedSlots {};
    std::array<uint64_t, 2> slotRevisions {};
    for (int side = 0; side < 2; ++side)
        if (auto* slots = g_registry.try_get<ecs::QuickSlots>(snapshot.offers[side].owner))
        {
            for (const auto& transfer : transfers)
            {
                if (transfer.from != snapshot.offers[side].owner ||
                    transfer.selection.source.cell > UINT8_MAX ||
                    transfer.selection.source.window_type == DRAGON_SOUL_INVENTORY)
                    continue;
                uint8_t type = QUICKSLOT_TYPE_ITEM;
#ifdef ENABLE_EXTRA_INVENTORY
                if (transfer.selection.source.window_type == EXTRA_INVENTORY) type = QUICKSLOT_TYPE_ITEM_EXTRA;
#endif
                for (size_t i = 0; i < slots->slots.size(); ++i)
                    if (slots->slots[i].type == type && slots->slots[i].pos == transfer.selection.source.cell)
                    {
                        slots->slots[i] = {};
                        changedSlots[side][i] = true;
                    }
            }
            if (std::any_of(changedSlots[side].begin(), changedSlots[side].end(), [](bool changed) { return changed; }))
                slotRevisions[side] = ++slots->revision;
        }
    g_registry.get<Session>(session).phase = Phase::Committed;
    for (const auto& offer : snapshot.offers)
    {
        TouchExchangeTime(offer.owner);
        if (g_registry.valid(offer.owner)) CHARACTER_MANAGER::instance().DelayedSave(offer.owner);
    }
    for (int side = 0; side < 2; ++side)
    {
        const auto owner = snapshot.offers[side].owner;
        for (uint8_t i = 0; i < QUICKSLOT_MAX_NUM && g_registry.valid(owner); ++i)
        {
            const auto* slots = g_registry.try_get<ecs::QuickSlots>(owner);
            if (!slots || slots->revision != slotRevisions[side]) break;
            if (changedSlots[side][i]) NetworkSyncSystem::SendQuickslotDelete(owner, i);
        }
    }

    // The exchange is committed even if a publication callback disconnects a
    // participant or cancels the window. Work below holds only value snapshots.
    for (const auto& transfer : transfers)
    {
        if (ItemSystem::IsValidItem(transfer.selection.item))
            ItemSystem::SaveItemEcs(transfer.selection.item, true);
        SendSlot(transfer.from, transfer.selection.source);
        SendSlot(transfer.to, transfer.destination);
        if (ItemSystem::IsValidItem(transfer.selection.item) &&
            ItemSystem::GetItemOwner(transfer.selection.item) == transfer.to)
        {
            const std::string takeHint = transfer.name + " " + std::to_string(transfer.fromPID) + " " + std::to_string(transfer.selection.count);
            const std::string giveHint = transfer.name + " " + std::to_string(transfer.toPID) + " " + std::to_string(transfer.selection.count);
            if (Descriptor(transfer.to))
                LogManager::instance().ItemLog(transfer.to, transfer.selection.id, transfer.selection.vnum,
                    "EXCHANGE_TAKE", takeHint.c_str());
            if (Descriptor(transfer.from))
                LogManager::instance().ItemLog(transfer.from, transfer.selection.id, transfer.selection.vnum,
                    "EXCHANGE_GIVE", giveHint.c_str());
            if (transfer.selection.vnum >= 80003 && transfer.selection.vnum <= 80007)
            {
                LogManager::instance().GoldBarLog(transfer.toPID, transfer.selection.id, EXCHANGE_TAKE, "");
                LogManager::instance().GoldBarLog(transfer.fromPID, transfer.selection.id, EXCHANGE_GIVE, "");
            }
            if (transfer.selection.vnum == 90008 || transfer.selection.vnum == 90009)
                VCardUse(transfer.from, transfer.to, transfer.selection.item);
        }
    }
    for (int side = 0; side < 2; ++side)
    {
        const auto owner = snapshot.offers[side].owner;
        if (!Descriptor(owner)) continue;
        if (snapshot.offers[0].gold || snapshot.offers[1].gold)
        {
            if (snapshot.offers[side].gold > 1000)
                LogManager::instance().CharLog(owner, snapshot.offers[side].gold, "EXCHANGE_GOLD_GIVE",
                    (std::to_string(pids[1 - side]) + " " + names[1 - side]).c_str());
            if (snapshot.offers[1 - side].gold > 1000 && Descriptor(owner))
                LogManager::instance().CharLog(owner, snapshot.offers[1 - side].gold, "EXCHANGE_GOLD_TAKE",
                    (std::to_string(pids[1 - side]) + " " + names[1 - side]).c_str());
            TPacketGCPointChange packet {};
            packet.header = HEADER_GC_CHARACTER_POINT_CHANGE;
            packet.dwVID = ecs::PlayerRuntime::GetPacketVID(owner);
            packet.type = POINT_GOLD;
            packet.amount = balances[side] - gold[side];
            packet.value = ecs::PointSystem::GetGold(owner);
            if (auto* desc = Descriptor(owner)) desc->Packet(&packet, sizeof(packet));
        }
        const auto other = snapshot.offers[1 - side].owner;
        if (ecs::PlayerRuntime::IsValid(other))
            Info(owner, 105, std::string(ecs::PlayerRuntime::GetName(other)));
    }
    return true;
}
}

namespace ExchangeSystem
{
entt::entity GetSession(entt::entity participant)
{
    if (!g_registry.valid(participant))
        return entt::null;
    const auto* ref = g_registry.try_get<ecs::ExchangeRef>(participant);
    const auto* session = ref ? FindSession(ref->session) : nullptr;
    return session && SideOf(*session, participant) >= 0 ? ref->session : entt::null;
}

bool IsActive(entt::entity participant)
{
    return GetSession(participant) != entt::null;
}

int GetLastExchangePulse(entt::entity participant)
{
    const auto* timing = g_registry.valid(participant)
        ? g_registry.try_get<ecs::WarpBlockState>(participant) : nullptr;
    return timing ? timing->exchangeTime : 0;
}

bool Start(entt::entity initiator, entt::entity target)
{
    if (initiator == target || !CanTrade(initiator) || !CanTrade(target) ||
        IsActive(initiator) || !Near(initiator, target))
        return false;
    if (IsActive(target))
    {
        Packet(initiator, EXCHANGE_SUBHEADER_GC_ALREADY);
        return false;
    }
    if (!SafeboxDelayPassed(initiator) || !SafeboxDelayPassed(target))
        return false;
#ifdef ENABLE_PVP_ADVANCED
    if (ecs::PlayerRuntime::GetDuelOption(initiator, "BlockExchange"))
    {
        Info(initiator, 516); return false;
    }
    if (ecs::PlayerRuntime::GetDuelOption(target, "BlockExchange"))
    {
        Info(initiator, 517, std::string(ecs::PlayerRuntime::GetName(target))); return false;
    }
#endif
    if (const auto* flags = g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(target);
        flags && (flags->blockMode & BLOCK_EXCHANGE))
    {
        Info(initiator, 368, std::string(ecs::PlayerRuntime::GetName(target))); return false;
    }
#ifdef ENABLE_RESTRICT_GM_PERMISSIONS
    for (const auto owner : {initiator, target})
        if (ecs::PlayerRuntime::GetGMLevel(owner) > GM_PLAYER && ecs::PlayerRuntime::GetGMLevel(owner) < GM_IMPLEMENTOR)
            return false;
#endif
    if (quest::CQuestManager::instance().GiveItemToPC(ecs::PlayerRuntime::GetPlayerID(initiator), target))
        return false;
    if (!CanTrade(initiator) || !CanTrade(target) || IsActive(initiator) || IsActive(target) || !Near(initiator, target))
        return false;
    EnsureLifecycle();
    // Install empty reservations before creating the session. Construction
    // callbacks can invalidate a participant, so reacquire after each one.
    for (const auto owner : {initiator, target})
    {
        if (!g_registry.valid(owner)) return false;
        (void)g_registry.get_or_emplace<ecs::ExchangeRef>(owner);
        TouchExchangeTime(owner);
    }
    if (!CanTrade(initiator) || !CanTrade(target) || IsActive(initiator) || IsActive(target) ||
        !g_registry.all_of<ecs::ExchangeRef>(initiator) || !g_registry.all_of<ecs::ExchangeRef>(target) ||
        !Near(initiator, target))
        return false;
    const auto session = g_registry.create();
    Session state;
    state.offers[0].owner = initiator;
    state.offers[1].owner = target;
    g_registry.emplace<Session>(session, state);
    if (!Current(session, 0) || !CanTrade(initiator) || !CanTrade(target) ||
        IsActive(initiator) || IsActive(target) || !g_registry.all_of<ecs::ExchangeRef>(initiator) ||
        !g_registry.all_of<ecs::ExchangeRef>(target))
    {
        // No START packet has been sent; do not close a callback-created trade.
        if (auto* orphan = FindSession(session)) orphan->phase = Phase::Closing;
        if (g_registry.valid(session)) g_registry.destroy(session);
        return false;
    }
    g_registry.get<ecs::ExchangeRef>(initiator).session = session;
    g_registry.get<ecs::ExchangeRef>(target).session = session;
    Packet(target, EXCHANGE_SUBHEADER_GC_START, false, ecs::PlayerRuntime::GetPacketVID(initiator));
    if (Current(session, 0))
        Packet(initiator, EXCHANGE_SUBHEADER_GC_START, false, ecs::PlayerRuntime::GetPacketVID(target));
    return IsActive(initiator) && GetSession(initiator) == session;
}

bool AddItem(entt::entity participant, TItemPos position, uint32_t display)
{
    if (!CanTrade(participant) || !SafeboxDelayPassed(participant)) return false;
    const auto session = GetSession(participant);
    auto* state = FindSession(session);
    if (!state || state->phase != Phase::Open)
        return false;
    const int side = SideOf(*state, participant);
    if (state->offers[1 - side].accepted)
        return false;
    OfferedItem item;
    if (!CaptureItem(participant, position, item, false) || !DisplayFits(state->offers[side], display, item.size))
        return false;
    auto slot = std::find_if(state->offers[side].items.begin(), state->offers[side].items.end(),
        [](const OfferedItem& value) { return value.item == entt::null; });
    if (slot == state->offers[side].items.end())
        return false;
    item.display = uint8_t(display);
    *slot = item;
    g_registry.get<ecs::ItemFlags>(item.item).exchanging = true;
    ClearAccepts(*state);
    const auto revision = state->revision;
    PublishAccepts(session, revision);
    PairPacket(session, revision, side, EXCHANGE_SUBHEADER_GC_ITEM_ADD,
        item.vnum, TItemPos(RESERVED_WINDOW, display), item.count, &item);
    return true;
}

bool RemoveItem(entt::entity participant, uint32_t slot)
{
    const auto session = GetSession(participant);
    auto* state = FindSession(session);
    if (!state || state->phase != Phase::Open || slot >= EXCHANGE_ITEM_MAX_NUM)
        return false;
    const int side = SideOf(*state, participant);
    if (state->offers[1 - side].accepted)
        return false;
    const auto item = state->offers[side].items[slot];
    if (item.item == entt::null)
        return false;
    state->offers[side].items[slot] = {};
    if (ItemSystem::IsValidItem(item.item) && ItemSystem::GetItemOwner(item.item) == participant)
        if (auto* flags = g_registry.try_get<ecs::ItemFlags>(item.item))
            flags->exchanging = false;
    ClearAccepts(*state);
    const auto revision = state->revision;
    const auto other = state->offers[1 - side].owner;
    PublishAccepts(session, revision);
    if (Current(session, revision)) Packet(participant, EXCHANGE_SUBHEADER_GC_ITEM_DEL, true, slot);
    if (Current(session, revision)) Packet(other, EXCHANGE_SUBHEADER_GC_ITEM_DEL, false, slot, item.source);
    return true;
}

bool AddGold(entt::entity participant, int64_t amount)
{
    if (!CanTrade(participant) || !SafeboxDelayPassed(participant)) return false;
    const auto session = GetSession(participant);
    auto* state = FindSession(session);
    if (!state || state->phase != Phase::Open || amount <= 0 || amount >= GOLD_MAX)
        return false;
    const int side = SideOf(*state, participant);
    if (state->offers[1 - side].accepted || state->offers[side].gold != 0)
        return false;
    if (ecs::PointSystem::GetGold(participant) < amount)
    {
        Packet(participant, EXCHANGE_SUBHEADER_GC_LESS_GOLD);
        return false;
    }
    const auto recipient = state->offers[1 - side].owner;
    const auto originalRevision = state->revision;
    if (!CanTrade(recipient) || ecs::PointSystem::GetGold(recipient) < 0 ||
        ecs::PointSystem::GetGold(recipient) >= GOLD_MAX - amount)
        return false;
    if (!Current(session, originalRevision)) return false;
    state = FindSession(session);
    state->offers[side].gold = amount;
    ClearAccepts(*state);
    const auto revision = state->revision;
    PublishAccepts(session, revision);
    PairPacket(session, revision, side, EXCHANGE_SUBHEADER_GC_GOLD_ADD, amount);
    return true;
}

bool Accept(entt::entity participant, bool accepted)
{
    const auto session = GetSession(participant);
    auto* state = FindSession(session);
    if (!state || state->phase != Phase::Open)
        return false;
    const int side = SideOf(*state, participant);
    if (state->offers[side].accepted == accepted)
        return true;
    state->offers[side].accepted = accepted;
    const auto revision = ++state->revision;
    if (accepted && state->offers[1 - side].accepted)
    {
        const bool completed = Complete(session);
        Close(session);
        return completed;
    }
    PairPacket(session, revision, side, EXCHANGE_SUBHEADER_GC_ACCEPT, accepted);
    return true;
}

void Cancel(entt::entity participant)
{
    Close(GetSession(participant));
}
}

// Cards are a special exchange item. Retire the entity before sending a credit
// request, and retain only copied identities across deletion/network callbacks.
void VCardUse(entt::entity seller, entt::entity buyer, entt::entity item)
{
    auto* sellerDesc = Descriptor(seller);
    auto* buyerDesc = Descriptor(buyer);
    if (!sellerDesc || !buyerDesc || !db_clientdesc || db_clientdesc->GetSocket() == INVALID_SOCKET ||
        !ItemSystem::IsValidItem(item) || ItemSystem::IsItemExchanging(item) ||
        (ItemSystem::GetItemVnum(item) != 90008 && ItemSystem::GetItemVnum(item) != 90009) ||
        (ItemSystem::GetItemOwner(item) != seller && ItemSystem::GetItemOwner(item) != buyer))
        return;

    TPacketGDVCard packet {};
    packet.dwID = ItemSystem::GetItemSocket(item, 0);
    strlcpy(packet.szSellCharacter, ecs::PlayerRuntime::GetName(seller).data(), sizeof(packet.szSellCharacter));
    strlcpy(packet.szSellAccount, sellerDesc->GetAccountTable().login, sizeof(packet.szSellAccount));
    strlcpy(packet.szBuyCharacter, ecs::PlayerRuntime::GetName(buyer).data(), sizeof(packet.szBuyCharacter));
    strlcpy(packet.szBuyAccount, buyerDesc->GetAccountTable().login, sizeof(packet.szBuyAccount));
    const std::string sellerHost(sellerDesc->GetHostName()), buyerHost(buyerDesc->GetHostName());
    const auto x = ecs::PlayerRuntime::GetX(buyer), y = ecs::PlayerRuntime::GetY(buyer);
    const auto minutes = ItemSystem::GetItemSocket(item, 1) / 60;
    const std::string serverHost(g_stHostname);

    if (!ItemSystem::DestroyItemEntityEcs(item, "VCARD_USE"))
        return;
    // No acknowledgement-based DB transaction is introduced here. Persistence
    // retains the existing delete/request protocol; this guard is in-process.
    db_clientdesc->DBPacket(HEADER_GD_VCARD, 0, &packet, sizeof(packet));
    LogManager::instance().VCardLog(packet.dwID, x, y, serverHost.c_str(),
        packet.szSellCharacter, sellerHost.c_str(), packet.szBuyCharacter, buyerHost.c_str());
#ifdef TEXTS_IMPROVEMENT
    if (Descriptor(buyer))
        ecs::ChatSystem::SendNew(buyer, CHAT_TYPE_INFO, 101, "%d", minutes);
#endif
    LOG_INFO("VCARD_TAKE: {} {} -> {}", packet.dwID, packet.szSellCharacter, packet.szBuyCharacter);
}
