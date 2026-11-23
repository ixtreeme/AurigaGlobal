#include "../../core/stdafx.h"
#include "InventorySortSystem.hpp"

#ifdef ENABLE_SORT_INVEN

#include "InventorySystem.hpp"
#include "ChatSystem.hpp"
#include "CombatSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "SessionSystem.hpp"
#include "SocialSystem.hpp"
#include "../AIHelpers.hpp"
#include "../ResourceAdmission.hpp"
#include "../components/character_runtime_components.hpp"
#include "../../entity/char_manager.h"
#include "../../item/ItemCatalogRuntime.hpp"
#include "../../item/ItemStore.hpp"
#include "../../item/TypedDrops.hpp"
#include "../../item/TypedInventorySync.hpp"
#include "../../item/TypedTools.hpp"
#include "../../network/desc.h"
#include <item/InventorySort.hpp>
#include <openssl/rand.h>

namespace InventorySortSystem
{
namespace
{
namespace items = auriga::item;
constexpr auto SortCooldownSeconds = 30;

void Reply(entt::entity character, std::uint8_t scope, const char* result)
{
    ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "typed_inventory_sort %u %s", unsigned{scope}, result);
}

std::int32_t& Cooldown(ecs::CommandCooldowns& state, std::uint8_t scope)
{
    return scope == items::SortBackpack ? state.sortInvUntil : state.sortExtraInvUntil;
}

void SortWindow(entt::entity character, std::uint8_t scope)
{
    if (!ecs::IsCharacter(character))
        return;
    const auto* descriptor = ecs::PlayerRuntime::GetDesc(character);
    if (!descriptor || !descriptor->IsPhase(PHASE_GAME))
        return;
    if (g_registry.all_of<ecs::TypedItemBusy>(character) || CombatSystem::IsDead(character) ||
        !InventorySystem::CanHandleItems(character) || TypedGatheringActive(character) ||
        ecs::SocialSystem::HasExchange(character) || ecs::SocialSystem::GetShopOwner(character) != entt::null ||
        ecs::SessionSystem::IsSafeboxOpen(character))
        return Reply(character, scope, "busy");

    if (auto* cooldowns = g_registry.try_get<ecs::CommandCooldowns>(character);
        cooldowns && get_global_time() < Cooldown(*cooldowns, scope))
        return Reply(character, scope, "cooldown");

    auto permit = ecs::resource_admission::Enter({character}, auriga::resource_admission::Operation::InventoryMove);
    if (!permit || !permit->Current() || !g_registry.valid(character))
        return;
    const auto* typed = g_registry.try_get<ecs::TypedItems>(character);
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(character);
    const auto catalog = items::runtime::CurrentCatalog();
    if (!typed || !login || !catalog || g_registry.all_of<ecs::TypedItemBusy>(character))
        return Reply(character, scope, "unavailable");
    const auto& state = typed->state;
    std::array<std::uint8_t, 16> operation{};
    if (RAND_bytes(operation.data(), static_cast<int>(operation.size())) != 1)
        return Reply(character, scope, "unavailable");
    auto plan = items::MakeInventorySortCommit(*catalog, operation, state.fence, state.storages, state.items, scope);
    if (!plan)
    {
        LOG_INFO("Typed inventory sort: owner {} plan refused ({})", state.fence.owner.id, static_cast<int>(plan.error()));
        return Reply(character, scope, "unavailable");
    }
    if (!*plan)
        return Reply(character, scope, "already_sorted");

    const auto pid = ecs::PlayerRuntime::GetPlayerID(character);
    const auto token = login->token;
    const auto fence = state.fence;
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace<ecs::TypedItemBusy>(character);
    if (!items::runtime::RequestCommit(fence.owner, fence.sessionGeneration, std::move(**plan),
        [pid, token, scope, held = std::move(held)](items::runtime::CommitOutcome outcome) mutable {
            const auto entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
            const auto* active = g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
            if (!active || active->token != token)
                return;
            g_registry.remove<ecs::TypedItemBusy>(entity);
            held.reset();
            if (outcome.state)
            {
                g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*outcome.state));
                SendTypedInventory(entity);
            }
            if (outcome.write && outcome.state)
            {
                auto& cooldowns = g_registry.get_or_emplace<ecs::CommandCooldowns>(entity);
                Cooldown(cooldowns, scope) = get_global_time() + SortCooldownSeconds;
                Reply(entity, scope, "sorted");
            }
            else
                Reply(entity, scope, "unavailable");
            PumpTypedItemGrants(entity);
        }))
    {
        g_registry.remove<ecs::TypedItemBusy>(character);
        Reply(character, scope, "unavailable");
    }
}
}

void SortInventory(entt::entity character)
{
    SortWindow(character, items::SortBackpack);
}

void SortExtraInventory(entt::entity character, std::uint8_t category)
{
    if (category < items::SpecialInventoryTypes)
        SortWindow(character, category);
}
}

#endif
