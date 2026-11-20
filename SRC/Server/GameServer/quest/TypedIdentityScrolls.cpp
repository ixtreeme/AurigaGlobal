#include "stdafx.h"
#include "TypedIdentityScrolls.hpp"
#include "questmanager.h"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char.h"
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
enum EmpireResult { EmpireRefused, EmpireUnchanged, EmpireHasGuild, EmpireHasMarriage, EmpireChanged = 999 };
enum SexResult { SexRefused, SexChanged, SexHasBodyCostume, SexHasHairCostume };

static_assert(item::OppositeSexRace(MAIN_RACE_WARRIOR_M) == MAIN_RACE_WARRIOR_W);
static_assert(item::OppositeSexRace(MAIN_RACE_ASSASSIN_M) == MAIN_RACE_ASSASSIN_W);
static_assert(item::OppositeSexRace(MAIN_RACE_SURA_M) == MAIN_RACE_SURA_W);
static_assert(item::OppositeSexRace(MAIN_RACE_SHAMAN_M) == MAIN_RACE_SHAMAN_W);

struct ScrollUse
{
    entt::entity actor;
    const item::persistence::LoadedItemOwner* inventory;
    item::ItemInstanceId source;
    std::uint64_t revision;
    std::array<std::uint8_t, 16> operation{};
};

std::optional<ScrollUse> ReadScrollUse(lua_State* L)
{
    auto& manager = CQuestManager::instance();
    const auto* quest = manager.GetCurrentState();
    if (!lua_isyieldable(L) || !quest || quest->co != L || !quest->typedItemUse.id || manager.IsInOtherPCBlock())
        return {};

    const auto actor = manager.GetCurrentPCEntity();
    if (!player::IsPC(actor) || g_registry.any_of<ecs::TypedItemBusy, ecs::DeadTag, ecs::StunTag>(actor) ||
        !InventorySystem::CanHandleItems(actor) || !player::CanWarp(actor))
        return {};

    const auto* inventory = g_registry.try_get<ecs::TypedItems>(actor);
    const auto* desc = player::GetDesc(actor);
    const auto source = item::ItemInstanceId::Create(quest->typedItemUse.id);
    const auto pid = player::GetPlayerID(actor);
    if (!g_registry.all_of<ecs::TypedItemLogin>(actor) || !inventory || !desc || !desc->IsPhase(PHASE_GAME) ||
        !source || !pid || inventory->state.fence.owner != item::ItemOwner{item::ItemOwnerKind::Character, pid})
        return {};

    ScrollUse use{actor, &inventory->state, *source, quest->typedItemUse.revision};
    if (RAND_bytes(use.operation.data(), static_cast<int>(use.operation.size())) != 1) return {};
    return use;
}

bool QueueScroll(const ScrollUse& use, lua_State* L, item::ItemCommit write,
    std::function<void(entt::entity, bool)> apply)
{
    const auto actor = use.actor;
    auto permit = ecs::resource_admission::Enter({actor}, auriga::resource_admission::Operation::ItemUse);
    if (!permit || !permit->Current()) return false;

    const auto pid = player::GetPlayerID(actor);
    const auto token = g_registry.get<ecs::TypedItemLogin>(actor).token;
    const auto fence = write.owners.front();
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace<ecs::TypedItemBusy>(actor);
    if (item::runtime::RequestCommit(fence.owner, fence.sessionGeneration, std::move(write),
        [actor, token, pid, coroutine = L, apply = std::move(apply), held](item::runtime::CommitOutcome result) mutable {
            const auto current = [&] {
                const auto* active = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItemLogin>(actor) : nullptr;
                return active && active->token == token;
            };
            if (result.write) apply(current() ? actor : entt::null, result.write->replay);
            if (!current()) return;
            if (result.state)
                g_registry.emplace_or_replace<ecs::TypedItems>(actor, std::move(*result.state));
            g_registry.remove<ecs::TypedItemBusy>(actor);
            held.reset();
            SendTypedInventory(actor);
            if (current()) CQuestManager::instance().ResumeTypedItems(pid, coroutine, result.write.has_value());
            if (current()) PumpTypedItemGrants(actor);
        }))
        return true;

    g_registry.remove<ecs::TypedItemBusy>(actor);
    return false;
}

std::optional<int> SubmitNameScroll(lua_State* L)
{
#ifdef ENABLE_LOCALECHECK_CHANGENAME
    return NameRefused;
#else
    std::size_t length = 0;
    const char* argument = lua_type(L, 1) == LUA_TSTRING ? lua_tolstring(L, 1, &length) : nullptr;
    if (!argument || !length) return NameMissing;
    if (length > CHARACTER_NAME_MAX_LEN || std::string_view(argument, length).find('\0') != std::string_view::npos ||
        !check_name(argument)) return NameInvalid;

    const auto use = ReadScrollUse(L);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!use || !catalog || ecs::SocialSystem::GetGuild(use->actor) ||
        ecs::SocialSystem::GetParty(use->actor) != entt::null) return NameRefused;
    if (!player::GetPendingName(use->actor).empty()) return NamePending;

    const std::string before(player::GetName(use->actor));
    const std::string after(argument, length);
    std::unique_ptr<SQLMsg> name(DBManager::instance().DirectQueryPrepared(
        "SELECT 1 FROM player.player WHERE name=? LIMIT 1", {SqlParam::Text(after)}));
    if (!name || name->uiSQLErrno || !name->Get()) return NameRefused;
    if (name->Get()->uiNumRows) return NameOccupied;

    const auto& state = *use->inventory;
    auto write = item::MakeNameScrollCommit(*catalog, use->operation, state.fence, state.storages, state.items,
        use->source, use->revision, {before, after});
    if (!write) return NameRefused;

    const auto pid = player::GetPlayerID(use->actor);
    const std::string host = player::GetDesc(use->actor)->GetHostName();
    if (!QueueScroll(*use, L, std::move(*write),
        [pid, before, after, host](entt::entity actor, bool replay) {
            if (actor != entt::null) player::SetPendingName(actor, after);
            if (!replay)
            {
                LogManager::instance().ChangeNameLog(pid, before.c_str(), after.c_str(), host.c_str());
                MessengerManager::instance().PublishRemovedAccountLinks(before);
            }
        }))
        return NameRefused;
    return std::nullopt;
#endif
}

std::optional<int> SubmitEmpireScroll(lua_State* L)
{
    const auto empire = luaL_checkinteger(L, 1);
    const auto use = ReadScrollUse(L);
    const auto catalog = item::runtime::CurrentCatalog();
    if (empire < 1 || empire > 3 || !use || !catalog || AffectSystem::IsPolymorphed(use->actor) ||
        ecs::SocialSystem::GetParty(use->actor) != entt::null)
        return EmpireRefused;

    const item::EmpireScrollChange change{
        player::GetAccountID(use->actor), player::GetEmpire(use->actor), static_cast<std::uint8_t>(empire)};
    if (change.before == change.after) return EmpireUnchanged;
    if (ecs::SocialSystem::GetGuild(use->actor)) return EmpireHasGuild;

    const auto account = SqlParam::UInt(change.account);
    std::unique_ptr<SQLMsg> eligibility(DBManager::instance().DirectQueryPrepared(
        "SELECT EXISTS (SELECT 1 FROM player.player AS p JOIN player.guild_member AS m ON m.pid=p.id WHERE p.account_id=?), "
        "EXISTS (SELECT 1 FROM player.player AS p JOIN player.marriage AS m ON m.pid1=p.id OR m.pid2=p.id WHERE p.account_id=?)",
        {account, account}));
    if (!eligibility || eligibility->uiSQLErrno || !eligibility->Get()) return EmpireRefused;
    const auto row = eligibility->Get()->FetchRow();
    if (!row || !row[0] || !row[1]) return EmpireRefused;
    if (row[0][0] == '1') return EmpireHasGuild;
    if (row[1][0] == '1') return EmpireHasMarriage;

    const auto& state = *use->inventory;
    auto write = item::MakeEmpireScrollCommit(*catalog, use->operation, state.fence, state.storages, state.items,
        use->source, use->revision, change);
    if (!write) return EmpireRefused;

    if (!QueueScroll(*use, L, std::move(*write),
        [empire = change.after](entt::entity actor, bool) {
            if (actor == entt::null) return;
            player::SetEmpire(actor, empire);
            if (auto* desc = player::GetDesc(actor)) desc->GetAccountTable().bEmpire = empire;
            NetworkSyncSystem::UpdatePacket(actor);
        }))
        return EmpireRefused;
    return std::nullopt;
}

std::optional<int> SubmitSexScroll(lua_State* L)
{
    const auto use = ReadScrollUse(L);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!use || !catalog || AffectSystem::IsPolymorphed(use->actor)) return SexRefused;

    const auto& state = *use->inventory;
    const auto equipped = [&](item::EquipmentSlot slot) {
        const item::ItemPlacement placement{item::ItemStorageKind::Equipment, 1, static_cast<unsigned>(slot) - 1};
        return std::ranges::any_of(state.items, [&](const auto& row) {
            return row.owner == state.fence.owner && !row.destroyed && row.placement == placement;
        });
    };
    if (equipped(item::EquipmentSlot::CostumeBody)) return SexHasBodyCostume;
    if (equipped(item::EquipmentSlot::CostumeHair)) return SexHasHairCostume;

    const auto before = player::GetRaceNum(use->actor);
    const auto after = item::OppositeSexRace(before);
    if (!after) return SexRefused;
    auto write = item::MakeSexScrollCommit(*catalog, use->operation, state.fence, state.storages, state.items,
        use->source, use->revision, {static_cast<std::uint8_t>(before), *after});
    if (!write) return SexRefused;

    if (!QueueScroll(*use, L, std::move(*write), [race = *after](entt::entity actor, bool) {
        if (actor != entt::null) player::SetRace(actor, race);
    }))
        return SexRefused;
    return std::nullopt;
}

template<int Changed, int Refused>
int FinishScroll(lua_State* L, std::optional<int> status)
{
    if (status)
    {
        lua_pushinteger(L, *status);
        return 1;
    }

    lua_settop(L, 0);
    lua_pushliteral(L, "typed_items");
    return lua_yieldk(L, 1, 0, [](lua_State* thread, int, lua_KContext) {
        const int status = lua_toboolean(thread, 1) ? Changed : Refused;
        lua_settop(thread, 0);
        lua_pushinteger(thread, status);
        return 1;
    });
}
}

ALUA(pc_typed_change_name) { return FinishScroll<NameChanged, NameRefused>(L, SubmitNameScroll(L)); }
ALUA(pc_typed_change_empire) { return FinishScroll<EmpireChanged, EmpireRefused>(L, SubmitEmpireScroll(L)); }
ALUA(pc_typed_change_sex) { return FinishScroll<SexChanged, SexRefused>(L, SubmitSexScroll(L)); }
}
