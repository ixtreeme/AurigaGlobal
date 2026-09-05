#include "stdafx.h"
#include "attr_transfer.h"
#include "ecs/Registry.hpp"
#include "ecs/components/inventory_components.hpp"
#include "ecs/components/social_components.hpp"
#include "ecs/components/status_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "constants.h"
#include "log.h"
#include <Core/Logging.hpp>
#include <algorithm>
#include <charconv>

namespace {
using Window = ecs::AttrTransferWindowComponent;

Window* WindowOf(entt::entity character)
{
    return ecs::PlayerRuntime::IsValid(character) ? g_registry.try_get<Window>(character) : nullptr;
}

void Info(entt::entity character, uint32_t message)
{
#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, message, "");
#endif
}

bool CanUse(entt::entity character)
{
    if (!ecs::PlayerRuntime::IsPC(character))
        return false;
    if (const auto* status = g_registry.try_get<ecs::StatusFlags>(character);
        status && (status->isObserverMode || status->isDead || status->isStunned))
        return false;
    if (ecs::SocialSystem::GetExchange(character) || ecs::SocialSystem::GetShop(character) ||
        ecs::SocialSystem::GetMyShop(character) || ecs::SocialSystem::GetShopOwner(character) != entt::null)
        return false;
    if (const auto* shop = g_registry.try_get<ecs::ShopState>(character); shop && shop->underRefine)
        return false;
    if (const auto* box = g_registry.try_get<ecs::SafeboxRef>(character); box && box->isOpening)
        return false;
    if (const auto* cube = g_registry.try_get<ecs::CubeWindowComponent>(character); cube && cube->pNpc)
        return false;
#ifdef ENABLE_ACCE_SYSTEM
    if (const auto* acce = g_registry.try_get<ecs::AcceWindowComponent>(character);
        acce && (acce->combinationOpen || acce->absorptionOpen))
        return false;
#endif
    return true;
}

bool NearNpc(entt::entity character, entt::entity npc)
{
    if (!ecs::PlayerRuntime::IsValid(npc) ||
        ecs::PlayerRuntime::GetMapIndex(character) != ecs::PlayerRuntime::GetMapIndex(npc))
        return false;
    const auto dx = std::abs(int64_t(ecs::PlayerRuntime::GetX(character)) - ecs::PlayerRuntime::GetX(npc));
    const auto dy = std::abs(int64_t(ecs::PlayerRuntime::GetY(character)) - ecs::PlayerRuntime::GetY(npc));
    return std::max(dx, dy) + std::min(dx, dy) / 2 < ATTR_TRANSFER_MAX_DISTANCE;
}

Window* ActiveWindow(entt::entity character)
{
    auto* window = WindowOf(character);
    if (!window || window->busy || !CanUse(character))
        return nullptr;
    if (!NearNpc(character, window->npc) || ecs::PlayerRuntime::GetQuestNPC(character) != window->npc)
    {
        AttrTransfer_close(character);
        return nullptr;
    }
    return window;
}

bool AllowedCostume(entt::entity item)
{
    if (ItemSystem::GetItemType(item) != ITEM_COSTUME)
        return false;
    constexpr uint32_t excluded[][2] = {
        {73001,73012}, {75001,75012}, {75201,75212}, {73251,73262},
        {73501,73512}, {75401,75412}, {73751,73762}, {75601,75612}
    };
    const auto vnum = ItemSystem::GetItemVnum(item);
    for (const auto& range : excluded)
        if (vnum >= range[0] && vnum <= range[1])
            return false;
    const auto subtype = ItemSystem::GetItemSubType(item);
    return subtype == COSTUME_BODY || subtype == COSTUME_HAIR || subtype == COSTUME_WEAPON
#ifdef ENABLE_STOLE_COSTUME
        || subtype == COSTUME_STOLE
#endif
        ;
}

bool ValidSelection(entt::entity character, entt::entity item, int cell, int slot)
{
    if (cell < 0 || cell >= INVENTORY_MAX_NUM ||
        ItemSystem::GetInventoryItem(character, static_cast<uint16_t>(cell)) != item ||
        !ItemSystem::CanConsumeOwnedItem(character, item) ||
        ItemSystem::GetItemWindow(item) != INVENTORY || ItemSystem::GetItemCell(item) != cell)
        return false;
    return slot == 0 ? ItemSystem::GetItemType(item) == ITEM_TRANSFER_SCROLL :
        ItemSystem::GetItemCount(item) == 1 && AllowedCostume(item);
}

void ClearSelection(Window& window)
{
    window.items.fill(entt::null);
    window.cells.fill(-1);
}

// Resolve the component again after callbacks; never retain a component reference.
struct OperationGuard {
    entt::entity character;
    ~OperationGuard() { if (auto* window = WindowOf(character)) window->busy = false; }
};
}

bool AttrTransfer_is_open(entt::entity character)
{
    const auto* window = WindowOf(character);
    return window && (window->busy || ecs::PlayerRuntime::IsValid(window->npc));
}

void AttrTransfer_open(entt::entity character)
{
    if (!CanUse(character))
        return;
    if (const auto* window = WindowOf(character); window && window->busy)
        return;
    if (AttrTransfer_is_open(character))
    {
        Info(character, 80);
        return;
    }
    const auto npc = ecs::PlayerRuntime::GetQuestNPC(character);
    if (!NearNpc(character, npc))
        return;
    auto& window = g_registry.get_or_emplace<Window>(character);
    ClearSelection(window);
    window.npc = npc;
    ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "AttrTransfer open");
}

void AttrTransfer_clean_item(entt::entity character)
{
    if (auto* window = WindowOf(character); window && !window->busy)
        ClearSelection(*window);
}

void AttrTransfer_close(entt::entity character)
{
    auto* window = WindowOf(character);
    if (!window || window->busy)
        return;
    const bool wasOpen = window->npc != entt::null;
    ClearSelection(*window);
    window->npc = entt::null;
    if (wasOpen)
        ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "AttrTransfer close");
}

void AttrTransfer_add_item(entt::entity character, int slot, int cell)
{
    auto* window = ActiveWindow(character);
    if (!window || slot < 0 || slot >= MAX_ATTR_TRANSFER_SLOT || cell < 0 || cell >= INVENTORY_MAX_NUM)
        return;
    const auto item = ItemSystem::GetInventoryItem(character, static_cast<uint16_t>(cell));
    if (!ValidSelection(character, item, cell, slot))
        return;
    for (int i = 0; i < MAX_ATTR_TRANSFER_SLOT; ++i)
        if (i != slot && window->items[i] == item)
            return;
    if (slot != 0 && !ValidSelection(character, window->items[0], window->cells[0], 0))
    {
        Info(character, 85);
        return;
    }
    if (slot == 1 && !ValidSelection(character, window->items[2], window->cells[2], 2))
    {
        Info(character, 79);
        return;
    }
    const int other = slot == 1 ? 2 : 1;
    if (slot != 0 && ItemSystem::IsValidItem(window->items[other]) &&
        ItemSystem::GetItemSubType(item) != ItemSystem::GetItemSubType(window->items[other]))
        return;
    window->items[slot] = item;
    window->cells[slot] = cell;
    if (slot == 1)
        ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "AttrTransferMessage");
}

void AttrTransfer_delete_item(entt::entity character, int slot)
{
    auto* window = WindowOf(character);
    if (!window || window->busy || slot < 0 || slot >= MAX_ATTR_TRANSFER_SLOT)
        return;
    window->items[slot] = entt::null;
    window->cells[slot] = -1;
}

bool AttrTransfer_make(entt::entity character)
{
    auto* window = ActiveWindow(character);
    if (!window)
        return false;
    const auto items = window->items;
    for (int i = 0; i < MAX_ATTR_TRANSFER_SLOT; ++i)
        if (!ValidSelection(character, items[i], window->cells[i], i))
        {
            ClearSelection(*window);
            Info(character, 83);
            return false;
        }
    if (items[0] == items[1] || items[0] == items[2] || items[1] == items[2] ||
        ItemSystem::GetItemSubType(items[1]) != ItemSystem::GetItemSubType(items[2]) ||
        !g_registry.all_of<ecs::ItemAttributes>(items[1]) || !g_registry.all_of<ecs::ItemAttributes>(items[2]))
        return false;
    auto prepared = g_registry.get<ecs::ItemAttributes>(items[2]);
#ifdef ENABLE_ATTR_COSTUMES
    prepared.attrs[5] = {};
    prepared.attrs[6] = {};
#endif
    if (std::any_of(prepared.attrs.begin(), prepared.attrs.end(), [](const auto& attr) {
        return attr.bType >= MAX_APPLY_NUM;
    }))
        return false;
    if (std::none_of(prepared.attrs.begin(), prepared.attrs.end(), [](const auto& attr) {
        return attr.bType > 0 && attr.sValue > 0;
    }))
    {
        Info(character, 86);
        return false;
    }
    const auto targetVnum = ItemSystem::GetItemVnum(items[1]);
    window->busy = true;
    OperationGuard guard {character};
    const std::array costs { ItemSystem::ItemCost{items[0], 1}, ItemSystem::ItemCost{items[2], 1} };
    if (!ItemSystem::SetItemAttributesWithItemCosts(character, items[1], prepared, costs))
        return false;
    if (auto* current = WindowOf(character)) ClearSelection(*current);
    if (!ecs::PlayerRuntime::IsValid(character))
        return true;
    ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "AttrTransfer success");
    LogManager::instance().AttrTransferLog(ecs::PlayerRuntime::GetPlayerID(character),
        ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), targetVnum);
    Info(character, 84);
    return true;
}

void AttrTransfer_command(entt::entity character, std::string_view argument)
{
    if (!ecs::PlayerRuntime::IsValid(character))
        return;
    std::array<std::string_view, 3> tokens {};
    size_t count = 0;
    while (!argument.empty())
    {
        const auto start = argument.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) break;
        argument.remove_prefix(start);
        const auto end = argument.find_first_of(" \t\r\n");
        if (count == tokens.size()) return;
        tokens[count++] = argument.substr(0, end);
        if (end == std::string_view::npos) break;
        argument.remove_prefix(end);
    }
    if (count == 1 && (tokens[0] == "close" || tokens[0] == "c")) AttrTransfer_close(character);
    else if (count == 1 && (tokens[0] == "open" || tokens[0] == "o")) AttrTransfer_open(character);
    else if (count == 1 && (tokens[0] == "make" || tokens[0] == "m")) AttrTransfer_make(character);
    else
    {
        auto parse = [](std::string_view token, int& value) {
            if (token.empty()) return false;
            const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
            return result.ec == std::errc{} && result.ptr == token.data() + token.size() && value >= 0;
        };
        int slot = 0, cell = 0;
        if (count == 3 && (tokens[0] == "add" || tokens[0] == "a") && parse(tokens[1], slot) && parse(tokens[2], cell))
            AttrTransfer_add_item(character, slot, cell);
        else if (count == 2 && (tokens[0] == "delete" || tokens[0] == "del" || tokens[0] == "d") && parse(tokens[1], slot))
            AttrTransfer_delete_item(character, slot);
    }
}
