#pragma once
#include <entt/entity/entity.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
struct lua_State;
struct TypedDungeonEntryPlan {
 entt::entity dungeon=entt::null;
 std::function<void()> publish;
 std::function<void()> cancel;
};
struct TypedDungeonQuestEntry
{
    lua_State* coroutine = nullptr;
    std::string_view alternateTicket;
    std::function<void(bool)> completed;
};
bool HasTypedDungeonInventory(entt::entity);
bool TypedDungeonEntryPending(entt::entity);
void CancelTypedDungeonPreflight(entt::entity);
std::uint64_t TypedDungeonTicketCount(entt::entity,std::string_view code);
// The caller supplies the existing authored level/cooldown rules and prepares
// an empty dungeon only after all unspent entry tickets have been reserved.
bool BeginTypedDungeonEntry(entt::entity,std::string_view gate,std::string_view ticketCode,std::uint32_t units,
 std::function<bool(entt::entity)> eligible,std::function<std::optional<TypedDungeonEntryPlan>()> preflight,
 TypedDungeonQuestEntry quest = {});
namespace auriga::item::runtime {bool DeferDungeonEntryLogin(std::uint32_t,std::function<void()>);}
