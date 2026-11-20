#include "stdafx.h"
#include "TypedIdentityScrolls.hpp"
#include "questmanager.h"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char_manager.h"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/TypedInventorySync.hpp"
#include "../network/db.h"
#include "../network/desc.h"
#include "../core/log.h"
#include "../social/messenger_manager.h"
#include "../core/config.h"
#include <item/IdentityScrolls.hpp>
#include <openssl/rand.h>

namespace quest
{
namespace
{
namespace item = auriga::item;
namespace player = ecs::PlayerRuntime;

enum NameResult { NamePending, NameMissing, NameInvalid, NameOccupied, NameChanged, NameRefused };

std::optional<int> SubmitNameScroll(lua_State* L)
{
#ifdef ENABLE_LOCALECHECK_CHANGENAME
    return NameRefused;
#else
    auto& manager = CQuestManager::instance();
    const auto* quest = manager.GetCurrentState();
    if (!lua_isyieldable(L) || !quest || quest->co != L || !quest->typedItemUse.id || manager.IsInOtherPCBlock())
        return NameRefused;

    std::size_t length = 0;
    const char* argument = lua_type(L, 1) == LUA_TSTRING ? lua_tolstring(L, 1, &length) : nullptr;
    if (!argument || !length) return NameMissing;
    if (length > CHARACTER_NAME_MAX_LEN || std::string_view(argument, length).find('\0') != std::string_view::npos ||
        !check_name(argument)) return NameInvalid;

    const auto actor = manager.GetCurrentPCEntity();
    if (!player::IsPC(actor) || g_registry.all_of<ecs::TypedItemBusy>(actor) ||
        !InventorySystem::CanHandleItems(actor) || !player::CanWarp(actor) ||
        ecs::SocialSystem::GetGuild(actor) || ecs::SocialSystem::GetParty(actor) != entt::null)
        return NameRefused;
    if (!player::GetPendingName(actor).empty()) return NamePending;

    const auto catalog = item::runtime::CurrentCatalog();
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(actor);
    const auto* inventory = g_registry.try_get<ecs::TypedItems>(actor);
    auto* desc = player::GetDesc(actor);
    const auto source = item::ItemInstanceId::Create(quest->typedItemUse.id);
    const auto pid = player::GetPlayerID(actor);
    if (!catalog || !login || !inventory || !desc || !desc->IsPhase(PHASE_GAME) || !source || !pid ||
        inventory->state.fence.owner != item::ItemOwner{item::ItemOwnerKind::Character, pid})
        return NameRefused;

    const std::string before(player::GetName(actor));
    const std::string after(argument, length);
    std::unique_ptr<SQLMsg> name(DBManager::instance().DirectQueryPrepared(
        "SELECT 1 FROM player.player WHERE name=? LIMIT 1", {SqlParam::Text(after)}));
    if (!name || name->uiSQLErrno || !name->Get()) return NameRefused;
    if (name->Get()->uiNumRows) return NameOccupied;

    auto permit = ecs::resource_admission::Enter({actor}, auriga::resource_admission::Operation::ItemUse);
    if (!permit || !permit->Current()) return NameRefused;
    std::array<std::uint8_t, 16> operation{};
    if (RAND_bytes(operation.data(), static_cast<int>(operation.size())) != 1) return NameRefused;
    const auto& state = inventory->state;
    const auto fence = state.fence;
    auto write = item::MakeNameScrollCommit(*catalog, operation, fence, state.storages, state.items,
        *source, quest->typedItemUse.revision, {before, after});
    if (!write) return NameRefused;

    const auto token = login->token;
    const std::string host = desc->GetHostName();
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace<ecs::TypedItemBusy>(actor);
    if (!item::runtime::RequestCommit(fence.owner, fence.sessionGeneration, std::move(*write),
        [actor, token, pid, coroutine = L, before, after, host, held](item::runtime::CommitOutcome result) mutable {
            const auto current = [&] {
                const auto* active = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItemLogin>(actor) : nullptr;
                return active && active->token == token;
            };
            if (result.write)
            {
                if (current()) player::SetPendingName(actor, after);
                if (!result.write->replay)
                {
                    LogManager::instance().ChangeNameLog(pid, before.c_str(), after.c_str(), host.c_str());
                    MessengerManager::instance().PublishRemovedAccountLinks(before);
                }
            }
            if (!current()) return;
            if (result.state)
                g_registry.emplace_or_replace<ecs::TypedItems>(actor, std::move(*result.state));
            g_registry.remove<ecs::TypedItemBusy>(actor);
            held.reset();
            SendTypedInventory(actor);
            if (current()) CQuestManager::instance().ResumeTypedItems(pid, coroutine, result.write.has_value());
            if (current()) PumpTypedItemGrants(actor);
        }))
    {
        g_registry.remove<ecs::TypedItemBusy>(actor);
        return NameRefused;
    }
    return std::nullopt;
#endif
}
}

ALUA(pc_typed_change_name)
{
    if (const auto status = SubmitNameScroll(L))
    {
        lua_pushinteger(L, *status);
        return 1;
    }

    lua_settop(L, 0);
    lua_pushliteral(L, "typed_items");
    return lua_yieldk(L, 1, 0, [](lua_State* thread, int, lua_KContext) {
        const int status = lua_toboolean(thread, 1) ? NameChanged : NameRefused;
        lua_settop(thread, 0);
        lua_pushinteger(thread, status);
        return 1;
    });
}
}
