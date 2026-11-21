#include "stdafx.h"
#include "TypedIdentityScrolls.hpp"
#include "questmanager.h"
#include "../combat/skill.h"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/components/status_components.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../entity/char.h"
#include "../guild/guild.h"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedBooks.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/TypedInventorySync.hpp"
#include "../network/db.h"
#include "../network/desc.h"
#include "../core/log.h"
#include "../social/messenger_manager.h"
#include "../core/config.h"
#include "../core/constants.h"
#include "../core/utils.h"
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
enum GuildResult { GuildRefused, GuildNotLeader, GuildNoSuccessor, GuildChanged, GuildMissing };
enum LevelResetResult { LevelResetRefused, LevelResetChanged, LevelResetEquipped, LevelResetWrongMap, LevelResetWrongLevel, LevelResetPoints };

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

std::optional<ScrollUse> ReadScrollUse(entt::entity actor, TypedItemUseSource selected, bool requireCanWarp)
{
    if (!player::IsPC(actor) || g_registry.any_of<ecs::TypedItemBusy, ecs::DeadTag, ecs::StunTag>(actor) ||
        !InventorySystem::CanHandleItems(actor) || (requireCanWarp && !player::CanWarp(actor)))
        return {};

    const auto* inventory = g_registry.try_get<ecs::TypedItems>(actor);
    const auto* desc = player::GetDesc(actor);
    const auto source = item::ItemInstanceId::Create(selected.id);
    const auto pid = player::GetPlayerID(actor);
    if (!g_registry.all_of<ecs::TypedItemLogin>(actor) || !inventory || !desc || !desc->IsPhase(PHASE_GAME) ||
        !source || !pid || inventory->state.fence.owner != item::ItemOwner{item::ItemOwnerKind::Character, pid})
        return {};

    ScrollUse use{actor, &inventory->state, *source, selected.revision};
    if (RAND_bytes(use.operation.data(), static_cast<int>(use.operation.size())) != 1) return {};
    return use;
}

std::optional<ScrollUse> ReadScrollUse(lua_State* L, bool requireCanWarp = true)
{
    auto& manager = CQuestManager::instance();
    const auto* quest = manager.GetCurrentState();
    if (!lua_isyieldable(L) || !quest || quest->co != L || manager.IsInOtherPCBlock()) return {};
    return ReadScrollUse(manager.GetCurrentPCEntity(), quest->typedItemUse, requireCanWarp);
}

bool QueueScroll(const ScrollUse& use, lua_State* L, item::ItemCommit write,
    std::function<void(entt::entity)> apply)
{
    const auto actor = use.actor;
    auto permit = ecs::resource_admission::Enter({actor}, auriga::resource_admission::Operation::ItemUse);
    if (!permit || !permit->Current()) return false;

    const auto pid = player::GetPlayerID(actor);
    const auto token = g_registry.get<ecs::TypedItemLogin>(actor).token;
    const auto fence = write.owners.front();
    const auto checkpoint = write.learning
        ? HoldTypedBookCheckpoint(actor, std::exchange(apply, {})) : std::function<void(bool, bool)>{};
    if (write.learning && !checkpoint) return false;
    auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
    g_registry.emplace<ecs::TypedItemBusy>(actor);
    if (item::runtime::RequestCommit(fence.owner, fence.sessionGeneration, std::move(write),
        [actor, token, pid, coroutine = L, apply = std::move(apply), checkpoint, held](item::runtime::CommitOutcome result) mutable {
            const auto current = [&] {
                const auto* active = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItemLogin>(actor) : nullptr;
                return active && active->token == token;
            };
            if (current())
            {
                if (result.state)
                    g_registry.emplace_or_replace<ecs::TypedItems>(actor, std::move(*result.state));
                g_registry.remove<ecs::TypedItemBusy>(actor);
                if (checkpoint && !result.state)
                {
                    held.reset();
                    if (auto* desc = player::GetDesc(actor)) desc->SetPhase(PHASE_CLOSE);
                    checkpoint(false, false);
                    return;
                }
            }
            held.reset();
            if (checkpoint) checkpoint(result.write.has_value() && result.state.has_value(), true);
            // A recovered receipt is also the first completion of this request.
            if (result.write && apply) apply(current() ? actor : entt::null);
            if (!current()) return;
            SendTypedInventory(actor);
            if (current() && coroutine)
                CQuestManager::instance().ResumeTypedItems(pid, coroutine, result.write.has_value());
            if (current() && !coroutine && !result.write)
                ecs::ChatSystem::Send(actor, CHAT_TYPE_INFO, "A Zeusz köve most nem használható.");
            if (current()) PumpTypedItemGrants(actor);
        }))
        return true;

    g_registry.remove<ecs::TypedItemBusy>(actor);
    if (checkpoint) checkpoint(false, true);
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
        [pid, before, after, host](entt::entity actor) {
            if (actor != entt::null) player::SetPendingName(actor, after);
            LogManager::instance().ChangeNameLog(pid, before.c_str(), after.c_str(), host.c_str());
            MessengerManager::instance().PublishRemovedAccountLinks(before);
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
        [empire = change.after](entt::entity actor) {
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

    if (!QueueScroll(*use, L, std::move(*write), [race = *after](entt::entity actor) {
        if (actor != entt::null) player::SetRace(actor, race);
    }))
        return SexRefused;
    return std::nullopt;
}

std::optional<int> SubmitGuildMasterScroll(lua_State* L)
{
    std::size_t length = 0;
    const char* name = lua_type(L, 1) == LUA_TSTRING ? lua_tolstring(L, 1, &length) : nullptr;
    if (!name || !length || length > CHARACTER_NAME_MAX_LEN ||
        std::string_view(name, length).find('\0') != std::string_view::npos)
        return GuildRefused;

    const auto use = ReadScrollUse(L);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!use || !catalog) return GuildRefused;

    auto* guild = ecs::SocialSystem::GetGuild(use->actor);
    if (!guild) return GuildMissing;
    const auto pid = player::GetPlayerID(use->actor);
    if (guild->GetMasterPID() != pid) return GuildNotLeader;
    const auto successor = guild->GetMemberPID(std::string_view(name, length));
    if (!successor || successor == pid) return GuildNoSuccessor;

    const auto& state = *use->inventory;
    auto write = item::MakeGuildMasterScrollCommit(*catalog, use->operation, state.fence, state.storages, state.items,
        use->source, use->revision, {guild->GetID(), successor});
    if (!write || !QueueScroll(*use, L, std::move(*write), {})) return GuildRefused;
    return std::nullopt;
}

bool QueueDungeonCooldownScroll(const ScrollUse& use, lua_State* L, const item::ItemCatalog& catalog,
    std::uint8_t dungeon, PC& pc)
{
    item::DungeonCooldownScrollChange change{dungeon};
    std::array<std::string, item::CooldownScrollFlags.size()> flags;
    for (std::size_t flag = 0; flag < flags.size(); ++flag)
    {
        flags[flag] = fmt::format("{}.{}", item::CooldownScrollDungeons[dungeon], item::CooldownScrollFlags[flag]);
        change.before[flag] = pc.GetFlag(flags[flag]);
    }
    if (change.before.front() <= get_global_time()) return false;

    const auto& state = *use.inventory;
    auto write = item::MakeDungeonCooldownScrollCommit(catalog, use.operation, state.fence, state.storages, state.items,
        use.source, use.revision, change);
    if (!write) return false;

    pc.Save();
    return QueueScroll(use, L, std::move(*write), [flags = std::move(flags), change, direct = L == nullptr](entt::entity actor) {
        if (actor == entt::null) return;
        auto* current = CQuestManager::instance().GetPC(player::GetPlayerID(actor));
        if (!current) return;
        for (std::size_t flag = 0; flag < flags.size(); ++flag)
            current->ApplySavedFlagDeletion(flags[flag], change.before[flag]);
        if (direct)
            ecs::ChatSystem::Send(actor, CHAT_TYPE_INFO, "A Rúna kazamata várakozási ideje törölve.");
    });
}

std::optional<int> SubmitDungeonCooldownScroll(lua_State* L)
{
    std::size_t length = 0;
    const char* name = lua_type(L, 1) == LUA_TSTRING ? lua_tolstring(L, 1, &length) : nullptr;
    if (!name) return 0;
    const auto dungeon = std::ranges::find(item::CooldownScrollDungeons, std::string_view(name, length));
    if (dungeon == item::CooldownScrollDungeons.end()) return 0;

    const auto use = ReadScrollUse(L, false);
    const auto catalog = item::runtime::CurrentCatalog();
    auto* pc = CQuestManager::instance().GetCurrentPC();
    const auto index = static_cast<std::uint8_t>(dungeon - item::CooldownScrollDungeons.begin());
    if (!use || !catalog || !pc || !QueueDungeonCooldownScroll(*use, L, *catalog, index, *pc)) return 0;
    return std::nullopt;
}

std::optional<int> SubmitLevelResetScroll(lua_State* L)
{
    const auto use = ReadScrollUse(L, false);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!use || !catalog) return LevelResetRefused;

    const auto& state = *use->inventory;
    const auto equipped = [&](const item::PersistentItem& row) {
        return !row.destroyed && row.owner == state.fence.owner && row.placement &&
            row.placement->kind == item::ItemStorageKind::Equipment &&
            std::ranges::any_of(item::LevelResetEquipmentSlots, [&](item::EquipmentSlot slot) {
                return item::FitsEquipmentSlot(slot, row.placement->anchor);
            });
    };
    if (std::ranges::any_of(state.items, equipped)) return LevelResetEquipped;
    if (player::GetMapIndex(use->actor) != item::LevelResetMap) return LevelResetWrongMap;
    if (ecs::PointSystem::GetLevel(use->actor) != item::LevelBeforeReset) return LevelResetWrongLevel;

    const auto live = TypedBookCharacter(use->actor);
    if (!live) return LevelResetRefused;
    if (!item::CanPayLevelReset(*live)) return LevelResetPoints;

    const auto job = player::GetJob(use->actor);
    if (job >= JOB_MAX_NUM) return LevelResetRefused;
    const auto& initial = JobInitialPoints[job];
    const auto randomHP = static_cast<std::uint16_t>((item::LevelAfterReset - 1) *
        number(initial.hp_per_lv_begin, initial.hp_per_lv_end));
    const auto randomSP = static_cast<std::uint16_t>((item::LevelAfterReset - 1) *
        number(initial.sp_per_lv_begin, initial.sp_per_lv_end));
    auto write = item::MakeLevelResetScrollCommit(*catalog, use->operation, state.fence, state.storages, state.items,
        use->source, use->revision, state.learning, *live, randomHP, randomSP);
    if (!write || !QueueScroll(*use, L, std::move(*write), [](entt::entity actor) {
        LogManager::instance().QuestRewardLog("leveldown_66_to_65", player::GetPlayerID(actor),
            item::LevelBeforeReset, item::LevelAfterReset, 0);
        ecs::PointSystem::PublishSavedQuestLevelChange(actor);
    }))
        return LevelResetRefused;
    return std::nullopt;
}

std::optional<int> SubmitPerfectSoulStone(lua_State* L)
{
    int isInteger = 0;
    const auto skill = lua_tointegerx(L, 1, &isInteger);
    if (!isInteger || skill <= 0 || skill >= SKILL_MAX_NUM) return 0;

    const auto use = ReadScrollUse(L);
    const auto catalog = item::runtime::CurrentCatalog();
    if (!use || !catalog) return 0;

    const auto job = player::GetJob(use->actor);
    const auto group = SkillSystem::GetSkillGroup(use->actor);
    if (job >= JOB_MAX_NUM || group < 1 || group > 2) return 0;

    const int firstSkill = 1 + job * 30 + (group - 1) * 15;
    bool inGroup = skill >= firstSkill && skill <= firstSkill + 5;
#ifdef ENABLE_NEW_PASSIVE_SKILLS
    inGroup = inGroup || skill == SKILL_HELP_PALBANG + job + (group - 1) * JOB_MAX_NUM;
    if (inGroup && !SkillSystem::CanIncreaseSkill(use->actor, static_cast<std::uint32_t>(skill))) return 0;
#endif
    if (!inGroup) return 0;

    const auto live = TypedBookCharacter(use->actor);
    if (!live) return 0;
    const auto& state = *use->inventory;
    auto write = item::MakePerfectSoulStoneCommit(*catalog, use->operation, state.fence, state.storages, state.items,
        use->source, use->revision, state.learning, *live, static_cast<std::uint8_t>(skill));
    if (!write || !QueueScroll(*use, L, std::move(*write), {})) return 0;
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

void UseTypedRuneCooldownStone(entt::entity actor, const auriga::item::PersistentItem& source)
{
    const auto use = ReadScrollUse(actor, {source.id.Value(), source.revision}, false);
    const auto catalog = item::runtime::CurrentCatalog();
    auto* pc = use ? CQuestManager::instance().GetPC(actor) : nullptr;
    if (!use || !catalog || !pc || !pc->IsLoaded())
    {
        ecs::ChatSystem::Send(actor, CHAT_TYPE_INFO, "A Zeusz köve most nem használható.");
        return;
    }
    if (pc->GetFlag("rune_zone.cooldown") <= get_global_time())
    {
        ecs::ChatSystem::Send(actor, CHAT_TYPE_INFO, "A Rúna kazamatának nincs aktív várakozási ideje.");
        return;
    }

    constexpr auto dungeon = static_cast<std::uint8_t>(item::CooldownScrollDungeons.size() - 1);
    static_assert(item::CooldownScrollDungeons[dungeon] == item::RuneDungeonCooldownQuest);
    if (!QueueDungeonCooldownScroll(*use, nullptr, *catalog, dungeon, *pc))
        ecs::ChatSystem::Send(actor, CHAT_TYPE_INFO, "A Zeusz köve most nem használható.");
}

ALUA(pc_typed_change_name) { return FinishScroll<NameChanged, NameRefused>(L, SubmitNameScroll(L)); }
ALUA(pc_typed_change_empire) { return FinishScroll<EmpireChanged, EmpireRefused>(L, SubmitEmpireScroll(L)); }
ALUA(pc_typed_change_sex) { return FinishScroll<SexChanged, SexRefused>(L, SubmitSexScroll(L)); }
ALUA(pc_typed_change_guild_master) { return FinishScroll<GuildChanged, GuildRefused>(L, SubmitGuildMasterScroll(L)); }
ALUA(pc_typed_reset_dungeon_cooldown) { return FinishScroll<1, 0>(L, SubmitDungeonCooldownScroll(L)); }
ALUA(pc_typed_reset_level) { return FinishScroll<LevelResetChanged, LevelResetRefused>(L, SubmitLevelResetScroll(L)); }
ALUA(pc_typed_perfect_soul_stone) { return FinishScroll<1, 0>(L, SubmitPerfectSoulStone(L)); }
}
