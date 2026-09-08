#include "../../SRC/Server/GameServer/stdafx.h"
#include "../../SRC/Server/GameServer/char.h"
#include "../../SRC/Server/GameServer/cmd.h"
#include "../../SRC/Server/GameServer/skill.h"
#include "../../SRC/Server/GameServer/skill_power.h"
#include "../../SRC/Server/GameServer/desc.h"
#include "../../SRC/Server/GameServer/desc_client.h"
#include "../../SRC/Server/GameServer/config.h"
#include "../../SRC/Server/GameServer/New_PetSystem.h"
#include "../../SRC/Server/GameServer/packet.h"
#include "../../SRC/Server/GameServer/p2p.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/PIDRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/identity_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/skill_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/vital_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/session_components.hpp"
#include "../../SRC/Server/GameServer/ecs/components/dirty_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PlayerRuntimeSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ChatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/PointSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SkillSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ItemSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/InventorySystem.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include "../../SRC/Server/GameServer/ecs/systems/CombatSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MovementSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/AffectSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/SocialSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/QuestSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/MountSystem.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/NetworkSyncSystem.hpp"
#include "../../SRC/Server/GameServer/utils.h"
#include "../../SRC/Server/GameServer/desc_manager.h"
#include "../../SRC/Server/GameServer/char_interface.hpp"
#include "../../SRC/Server/GameServer/char_manager.h"
#include "../../SRC/Server/GameServer/item_manager.h"
#include "../../SRC/Server/GameServer/sectree_manager.h"
#include "../../SRC/Server/GameServer/mob_manager.h"
#include "../../SRC/Server/GameServer/regen.h"
#include "../../SRC/Server/GameServer/guild.h"
#include "../../SRC/Server/GameServer/guild_manager.h"
#include "../../SRC/Server/GameServer/buffer_manager.h"
#include "../../SRC/Server/GameServer/fishing.h"
#include "../../SRC/Server/GameServer/mining.h"
#include "../../SRC/Server/GameServer/questmanager.h"
#include "../../SRC/Server/GameServer/vector.h"
#include "../../SRC/Server/GameServer/affect.h"
#include "../../SRC/Server/GameServer/db.h"
#include "../../SRC/Server/GameServer/priv_manager.h"
#include "../../SRC/Server/GameServer/building.h"
#include "../../SRC/Server/GameServer/battle.h"
#include "../../SRC/Server/GameServer/arena.h"
#include "../../SRC/Server/GameServer/start_position.h"
#include "../../SRC/Server/GameServer/party.h"
#include "../../SRC/Server/GameServer/BattleArena.h"
#include "../../SRC/Server/GameServer/log.h"
#include "../../SRC/Server/GameServer/pcbang.h"
#include "../../SRC/Server/GameServer/unique_item.h"
#include "../../SRC/Server/GameServer/DragonSoul.h"
#include "../../SRC/Server/GameServer/ecs/AIHelpers.hpp"
#include "../../SRC/Server/GameServer/ecs/CharacterAccessors.hpp"
#include "../../SRC/Server/GameServer/ecs/EntityFactory.hpp"
#include "../../SRC/Server/GameServer/ecs/VIDRegistry.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/ActivitySystem.hpp"
#include "../../SRC/Server/GameServer/LostCastleDungeon.h"
#include "../../SRC/Server/GameServer/new_offlineshop.h"
#include "../../SRC/Server/GameServer/new_offlineshop_manager.h"
#include "../../SRC/Server/GameServer/ecs/systems/DragonSoulSystem.hpp"
#include "../../SRC/Server/GameServer/constants.h"
#include <Core/Logging.hpp>

ACMD(do_item_purge);
ACMD(do_set_socket);
ACMD(do_user);
ACMD(do_setskill);
ACMD(do_set_skill_group);
void SendNotice(const char*, bool);
void SendNoticeMap(const char*, int32_t, bool);
void SendNoticeNew(uint8_t, uint8_t, int32_t, uint32_t, const char*, ...);
void BroadcastNoticeNew(uint8_t, uint8_t, int32_t, uint32_t, const char*, ...);
void SendLog(const char*);

entt::registry g_registry;
namespace {
int checks = 0, socketWrites = 0, packets = 0, computes = 0, dbPackets = 0;
uint32_t nextPID = 0;
bool hasPet = false, activePet = false, rejectDestroy = false;
struct Player {
    std::string name;
    int level = 90, map = 1;
    uint8_t empire = 1, gm = GM_PLAYER, job = JOB_WARRIOR;
    bool languageRing = false;
    bool online = true;
};
struct Item { entt::entity owner; TItemPos pos; uint32_t id; };
struct Chat { entt::entity player; uint8_t type; uint32_t index; std::string text; };
std::vector<Chat> messages;
std::vector<entt::entity> destroyed;
std::vector<std::pair<uint16_t, uint16_t>> quickslots;
std::vector<char> p2p;
std::function<void(entt::entity)> onDestroy, onChat;
std::function<void()> onPacket;
std::function<void(entt::entity)> onCompute, onPointChange, onColorUpdate, onColorPayment;
int colorTokens = 0, colorPayments = 0, guildSkillLevel = 0;
bool refuseColorPayment = false, hasGuild = false;
std::vector<TSkillColor> savedColors;
std::map<std::pair<entt::entity, uint32_t>, entt::entity> slots;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
uint32_t Key(TItemPos pos) { return (uint32_t(pos.window_type) << 16) | pos.cell; }
// Opaque service tokens, never dereferenced by production code under test.
LPDESC DescToken() { static int token; return reinterpret_cast<LPDESC>(&token); }
CNewPetSystem* PetToken() { static int token; return reinterpret_cast<CNewPetSystem*>(&token); }
void Reset() {
    for (auto e : CPIDRegistry::Instance().Snapshot()) CPIDRegistry::Instance().UnregisterEntity(e);
    g_registry.clear(); slots.clear(); messages.clear(); destroyed.clear(); quickslots.clear(); p2p.clear();
    onDestroy = onChat = onCompute = onPointChange = {}; onPacket = {};
    hasPet = activePet = rejectDestroy = false; socketWrites = packets = computes = dbPackets = 0;
    onColorUpdate = onColorPayment = {};
    colorTokens = colorPayments = guildSkillLevel = 0;
    refuseColorPayment = hasGuild = false; savedColors.clear();
}
entt::entity Actor(const char* name = "GM") {
    auto e = g_registry.create();
    g_registry.emplace<Player>(e, name);
    g_registry.emplace<ecs::TagPC>(e);
    CPIDRegistry::Instance().Register(++nextPID, e);
    return e;
}
entt::entity Give(entt::entity owner, uint8_t window = INVENTORY, uint16_t cell = 0) {
    auto e = g_registry.create();
    TItemPos pos(window, cell);
    g_registry.emplace<Item>(e, owner, pos, 1234u);
    slots[{owner, Key(pos)}] = e;
    return e;
}
void PurgeChecks() {
    for (const char* arg : {"", "alligator", "invjunk", "equipmentjunk", "dsjunk", "beltjunk"}) {
        Reset(); const auto owner = Actor(), item = Give(owner);
        do_item_purge(owner, arg, 0, 0);
        Check(g_registry.valid(item) && destroyed.empty(), "invalid purge selector accepted");
    }
    for (const char* arg : {"inv", "inventory", "equip", "equipment", "ds", "dragonsoul", "belt", "extra", "all"}) {
        Reset(); const auto owner = Actor();
        const auto inv = Give(owner), equip = Give(owner, INVENTORY, INVENTORY_MAX_NUM);
        const auto belt = Give(owner, INVENTORY, BELT_INVENTORY_SLOT_START);
        const auto ds = Give(owner, DRAGON_SOUL_INVENTORY), extra = Give(owner, EXTRA_INVENTORY);
        do_item_purge(owner, arg, 0, 0);
        const std::string_view mode(arg);
        const bool all = mode == "all";
        Check(!g_registry.valid(inv) == (all || mode == "inv" || mode == "inventory"), "inventory selection");
        Check(!g_registry.valid(equip) == (all || mode == "equip" || mode == "equipment"), "equipment selection");
        Check(!g_registry.valid(belt) == (all || mode == "belt"), "belt selection");
        Check(!g_registry.valid(ds) == (all || mode == "ds" || mode == "dragonsoul"), "DS selection");
        Check(!g_registry.valid(extra) == (all || mode == "extra"), "extra selection");
        Check(quickslots.size() == (all ? 4 : mode == "ds" || mode == "dragonsoul" ? 0 : 1), "quickslot windows");
    }
    Reset(); auto owner = Actor(), item = Give(owner);
    hasPet = activePet = true;
    do_item_purge(owner, "all", 0, 0);
    Check(g_registry.valid(item) && destroyed.empty(), "active pet item destroyed");
    activePet = false; rejectDestroy = true;
    do_item_purge(owner, "all", 0, 0);
    Check(g_registry.valid(item) && quickslots.empty(), "failed deletion cleared quickslot");
    rejectDestroy = false;
    slots[{owner, Key(TItemPos(INVENTORY, 1))}] = item; // Corrupt duplicate entry.
    do_item_purge(owner, "all", 0, 0);
    Check(destroyed.size() == 1, "duplicate item destroyed twice");
    Reset(); owner = Actor(); item = Give(owner);
    g_registry.remove<ecs::TagPC>(owner);
    do_item_purge(owner, "all", 0, 0); do_item_purge(entt::null, "all", 0, 0);
    Check(g_registry.valid(item) && destroyed.empty(), "invalid/non-player owner");
    g_registry.destroy(owner); Actor();
    do_item_purge(owner, "all", 0, 0);
    Check(g_registry.valid(item), "stale owner inherited replacement");

    for (int mode = 0; mode < 6; ++mode) {
        Reset(); owner = Actor();
        const auto first = Give(owner), second = Give(owner, INVENTORY, 1);
        entt::entity replacement = entt::null;
        onDestroy = [&](entt::entity e) {
            if (e != first) return;
            if (mode == 0) { g_registry.destroy(owner); replacement = Actor(); }
            if (mode == 1) { g_registry.destroy(second); replacement = Give(owner, INVENTORY, 1); }
            if (mode == 2) { g_registry.get<Item>(second).owner = Actor("Other"); }
            if (mode == 3) { slots.erase({owner, Key(TItemPos(INVENTORY, 1))}); }
            if (mode == 4) { replacement = Give(owner, INVENTORY, 0); }
            if (mode == 5) { replacement = Give(owner, INVENTORY, 2); }
        };
        do_item_purge(owner, "all", 0, 0);
        Check(destroyed.size() == (mode >= 4 ? 2 : 1), "snapshot/ownership/generation invalidation");
        if (replacement != entt::null) Check(g_registry.valid(replacement), "purged callback replacement");
        if (mode == 0) Check(quickslots.empty(), "owner teardown followed by quickslot write");
        if (mode == 4) Check(quickslots.size() == 1 && quickslots[0].second == 1, "replacement slot cleared");
    }
}
void NoticeChecks() {
    Reset(); const auto first = Actor("100%player"), second = Actor("Second"), offline = Actor("Offline");
    g_registry.get<Player>(second).map = 2; g_registry.get<Player>(second).empire = 2;
    g_registry.get<Player>(second).gm = GM_IMPLEMENTOR;
    g_registry.get<Player>(offline).online = false;
    SendNotice("literal %s %n", false);
    Check(messages.size() == 2, "notice online recipients");
    for (auto& m : messages) Check(m.text == "literal %s %n", "notice reinterpreted format");
    messages.clear(); SendNoticeMap("map", 2, true);
    Check(messages.size() == 1 && messages[0].player == second && messages[0].type == CHAT_TYPE_BIG_NOTICE, "map filter");
    messages.clear(); SendLog("log");
    Check(messages.size() == 1 && messages[0].player == second, "GM filter");
    messages.clear(); SendNoticeNew(CHAT_TYPE_INFO, 1, 1, 42, "%s", "100% %s");
    Check(messages.size() == 1 && messages[0].player == first && messages[0].text == "100% %s" &&
        messages[0].index == 42, "localized notice filters/format");
    messages.clear(); BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 99, "%s", std::string(10000, 'x').c_str());
    Check(p2p.size() == sizeof(TPacketGGChatNew) + 255, "P2P overread/truncation size");
    TPacketGGChatNew header {}; std::memcpy(&header, p2p.data(), sizeof(header));
    Check(header.size == 255 && header.idx == 99, "P2P advertised length");
    Check(messages.size() == 2 && messages[0].text.size() == 255, "local truncation");
    messages.clear(); BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 100, "%s", "100% %n");
    Check(messages.size() == 2 && messages[0].text == "100% %n", "P2P second formatting pass");
    Reset(); Actor("A"); Actor("B"); Actor("C");
    const auto snapshot = CPIDRegistry::Instance().Snapshot();
    bool once = false;
    onChat = [&](entt::entity) {
        if (std::exchange(once, true)) return;
        g_registry.destroy(snapshot[1]);
        CPIDRegistry::Instance().UnregisterEntity(snapshot[1]);
        Actor("New");
    };
    SendNotice("snapshot", false);
    Check(messages.size() == 2, "notice iterator mutated or visited new/recycled player");
    Reset(); const auto caller = Actor("literal%n");
    Actor("B"); Actor("C"); Actor("D"); Actor("E");
    once = false;
    onChat = [&](entt::entity) { if (!std::exchange(once, true)) do_user(caller, "", 0, 0); };
    do_user(caller, "", 0, 0);
    int totals = 0, names = 0;
    for (const auto& m : messages) {
        totals += m.text == "Total 5";
        names += m.text.find("literal%n") != std::string::npos;
    }
    Check(totals == 2 && names == 2, "nested user list shared state or format");
}
void SkillChecks() {
    Reset(); auto player = Actor();
    std::array<TPlayerSkill, SKILL_MAX_NUM> levels {};
    g_registry.emplace<ecs::SkillLevels>(player, levels.data(), uint8_t{0});
    g_registry.emplace<ecs::CharacterPoints>(player);
    g_registry.emplace<ecs::NetworkSession>(player).desc = DescToken();
    SkillSystem::SetSkillGroup(player, 2);
    Check(SkillSystem::GetSkillGroup(player) == 2 && packets == 1 &&
        g_registry.get<ecs::CharacterPoints>(player).base.skill_group == 2, "native skill group/packet");
    SkillSystem::SetSkillGroup(player, 3);
    Check(SkillSystem::GetSkillGroup(player) == 2 && packets == 1, "invalid group");
    SkillSystem::SetSkillGroup(player, 0);
    Check(SkillSystem::GetSkillGroup(player) == 0 && g_registry.get<ecs::CharacterPoints>(player).base.skill_group == 0, "group zero fallback");
    for (uint8_t level : {0, 19, 20, 29, 30, 39, 40, 255}) {
        SkillSystem::SetSkillLevel(player, SKILL_PALBANG, level);
        Check(SkillSystem::GetSkillLevel(player, SKILL_PALBANG) == std::min<int>(40, level), "skill cap");
        Check(levels[SKILL_PALBANG].bMasterType == (level >= 40 ? SKILL_PERFECT_MASTER :
            level >= 30 ? SKILL_GRAND_MASTER : level >= 20 ? SKILL_MASTER : SKILL_NORMAL), "master thresholds");
    }
    levels[SKILL_PALBANG].bLevel = 39;
    SkillSystem::SetSkillLevel(player, SKILL_HELP_PALBANG, 1);
    Check(levels[SKILL_HELP_PALBANG].bLevel == 0, "helper prerequisite ignored");
    levels[SKILL_PALBANG].bLevel = 40;
    SkillSystem::SetSkillLevel(player, SKILL_HELP_PALBANG, 1);
    Check(levels[SKILL_HELP_PALBANG].bLevel == 1, "eligible helper rejected");
    g_registry.get<Player>(player).level = 89;
    SkillSystem::SetSkillLevel(player, SKILL_ANTI_PALBANG, 10);
    Check(levels[SKILL_ANTI_PALBANG].bLevel == 0, "anti level requirement ignored");
    g_registry.get<Player>(player).level = 90;
    SkillSystem::SetSkillLevel(player, SKILL_ANTI_PALBANG, 11);
    Check(levels[SKILL_ANTI_PALBANG].bLevel == 20, "anti 11-to-20 rule");
    SkillSystem::SetSkillLevel(player, SKILL_ANTI_PALBANG, 30);
    Check(!SkillSystem::CanIncreaseSkill(player, SKILL_ANTI_PALBANG, true), "anti book cap");
    g_registry.get<Player>(player).level = 1;
    SkillSystem::SetSkillLevel(player, SKILL_ANTI_PALBANG, 0);
    Check(levels[SKILL_ANTI_PALBANG].bLevel == 0, "low-level anti reset denied");
    SkillSystem::SetSkillLevel(player, NEW_SUPPORT_SKILL_ATTACK, 40);
    Check(levels[NEW_SUPPORT_SKILL_ATTACK].bLevel == 10 && levels[NEW_SUPPORT_SKILL_ATTACK].bMasterType == SKILL_NORMAL, "secondary cap");
    SkillSystem::SetSkillLevel(player, SKILL_MAX_NUM, 40);
    SkillSystem::SetSkillLevel(entt::null, SKILL_PALBANG, 40);
    SkillSystem::SendSkillLevelPacket(player);
    Check(packets == 3, "native skill level packet");
    levels[SKILL_PALBANG].bLevel = 0;
    bool once = false;
    onChat = [&](entt::entity) { if (!std::exchange(once, true)) { g_registry.destroy(player); Actor(); } };
    SkillSystem::SetSkillLevel(player, SKILL_HELP_PALBANG, 1);
    Check(!g_registry.valid(player), "eligibility callback not exercised");
    SkillSystem::SetSkillGroup(player, 1);
    Check(packets == 3, "stale skill owner packet");
}
void SkillRuntimeChecks() {
    Reset(); auto caster = Actor(), target = Actor("Target");
    Check(!SkillSystem::CheckSkillHit(caster, SKILL_PALBANG, target), "unregistered skill hit");
    Check(!SkillSystem::ConsumeSkillHit(caster, SKILL_PALBANG), "missing skill use consumed");
    Check(SkillSystem::GetSkillMainTarget(caster, SKILL_PALBANG) == entt::null &&
        SkillSystem::GetNextSkillUseTime(caster, SKILL_PALBANG) == 0, "missing skill state read");
    Check(!g_registry.all_of<ecs::SkillDamageBonus>(caster), "read-only query allocated skill state");
    for (uint32_t skill = 1; skill < std::min<uint32_t>(SKILL_MAX_NUM, 256); ++skill) {
        Check(SkillSystem::RegisterSkillUse(caster, skill, false, target, 0), "native skill registration");
        int limit = 1;
        switch (skill) {
        case SKILL_YONGKWON: case SKILL_HWAYEOMPOK: case SKILL_DAEJINGAK: case SKILL_PAERYONG: limit = 0; break;
        case SKILL_SAMYEON: case SKILL_CHARYUN:
#ifdef ENABLE_WOLFMAN_CHARACTER
        case SKILL_CHAYEOL:
#endif
            limit = 3; break;
        case SKILL_HORSE_WILDATTACK_RANGE: limit = 5; break;
        case SKILL_YEONSA: limit = 7; break;
        case SKILL_HORSE_ESCAPE: limit = 10; break;
        }
        for (int hit = 0; hit < limit; ++hit)
            Check(SkillSystem::CheckSkillHit(caster, uint8_t(skill), target), "allowed target hit rejected");
        Check(!SkillSystem::CheckSkillHit(caster, uint8_t(skill), target), "target hit limit exceeded");
        auto other = Actor("Other");
        Check(SkillSystem::CheckSkillHit(caster, uint8_t(skill), other) == (limit > 0), "targets shared a hit budget");
        Check(SkillSystem::RegisterSkillUse(caster, skill, false, target, 0), "repeat skill registration");
        Check(SkillSystem::CheckSkillHit(caster, uint8_t(skill), target) == (limit > 0), "accepted cast retained old target count");
        g_registry.destroy(other);
    }

    constexpr auto skill = SKILL_SAMYEON;
    std::array<TPlayerSkill, SKILL_MAX_NUM> levels {};
    levels[skill].bMasterType = SKILL_PERFECT_MASTER;
    g_registry.emplace<ecs::SkillLevels>(caster, levels.data(), uint8_t{1});
    Check(SkillSystem::RegisterSkillUse(caster, skill, false, target, 60000, 1, 3), "cooldown registration");
    Check(SkillSystem::GetNextSkillUseTime(caster, skill) != 0 &&
        SkillSystem::GetSkillMainTarget(caster, skill) == target, "native cast state");
    for (int i = 0; i < 3; ++i) Check(SkillSystem::CheckSkillHit(caster, skill, target), "multi-hit cast");
    Check(!SkillSystem::RegisterSkillUse(caster, skill, true, target, 60000), "cooldown accepted duplicate cast");
    Check(!SkillSystem::CheckSkillHit(caster, skill, target), "rejected cast reset target hits");
    Check(SkillSystem::GetUsedSkillMasterType(caster, skill) == SKILL_MASTER, "rejected cast changed mastery");
    auto& use = g_registry.get<ecs::SkillDamageBonus>(caster).useInfo[skill];
    use.dwNextSkillUsableTime = 0;
    Check(SkillSystem::RegisterSkillUse(caster, skill, true, target, 0, 1, 2), "new cast");
    Check(SkillSystem::GetUsedSkillMasterType(caster, skill) == SKILL_PERFECT_MASTER, "grandmaster cast mastery");
    Check(SkillSystem::ConsumeSkillHit(caster, skill) && SkillSystem::ConsumeSkillHit(caster, skill) &&
        !SkillSystem::ConsumeSkillHit(caster, skill), "shared splash hit budget");
    Check(SkillSystem::RegisterSkillUse(caster, skill, false, target, 0, 1, -1), "unlimited-hit cast");
    for (int i = 0; i < 20; ++i) Check(SkillSystem::ConsumeSkillHit(caster, skill), "unlimited hits");

    const auto deadTarget = target;
    g_registry.destroy(target); target = Actor("Replacement");
    Check(SkillSystem::GetSkillMainTarget(caster, skill) == entt::null, "main target inherited recycled entity");
    Check(!SkillSystem::CheckSkillHit(caster, skill, deadTarget) &&
        !SkillSystem::CheckSkillHit(caster, skill, entt::null) &&
        !SkillSystem::CheckSkillHit(caster, skill, caster), "invalid attack target accepted");
    SkillSystem::SetSkillMainTarget(caster, skill, target);
    SkillSystem::SetSkillMainTarget(caster, skill, deadTarget);
    Check(SkillSystem::GetSkillMainTarget(caster, skill) == target, "stale main-target update");
    Check(!SkillSystem::RegisterSkillUse(caster, skill, false, deadTarget, 0) &&
        !SkillSystem::RegisterSkillUse(caster, SKILL_MAX_NUM, false, target, 0), "invalid cast registered");
    SkillSystem::ResetSkillHitTargets(caster, skill);
    Check(g_registry.get<ecs::SkillDamageBonus>(caster).useInfo[skill].TargetVIDMap.empty(), "charge target reset");

    const auto oldCaster = caster;
    g_registry.destroy(caster); caster = Actor();
    Check(!SkillSystem::RegisterSkillUse(oldCaster, skill, false, target, 0) &&
        !SkillSystem::CheckSkillHit(oldCaster, skill, target) &&
        !SkillSystem::ConsumeSkillHit(oldCaster, skill), "stale caster");
    Check(!g_registry.all_of<ecs::SkillDamageBonus>(caster), "replacement inherited cast state");
    Check(SkillSystem::GetUsedSkillMasterType(entt::null, skill) == SKILL_NORMAL, "null mastery read");
}
void SkillPowerChecks() {
    Reset(); const auto caster = Actor();
    std::array<TPlayerSkill, SKILL_MAX_NUM> levels {};
    g_registry.emplace<ecs::SkillLevels>(caster, levels.data(), uint8_t{1});
    for (uint8_t job = 0; job < JOB_MAX_NUM; ++job) {
        g_registry.get<Player>(caster).job = job;
        for (uint8_t group : {1, 2}) {
            g_registry.get<ecs::SkillLevels>(caster).group = group;
            for (uint8_t level = 0; level <= SKILL_MAX_LEVEL; ++level) {
                levels[SKILL_PALBANG].bLevel = level;
                Check(SkillSystem::GetSkillPower(caster, SKILL_PALBANG) ==
                    (job * 2 + group - 1) * 100 + level, "native skill power table");
            }
        }
    }
    Check(SkillSystem::GetSkillPower(caster, SKILL_PALBANG, 255) == SKILL_MAX_LEVEL, "mob power level clamp");
    Check(SkillSystem::GetSkillPower(caster, SKILL_MAX_NUM + 100, 10) == 10,
        "explicit mob power incorrectly bounded by player skill array");
    g_registry.get<ecs::SkillLevels>(caster).group = 3;
    Check(SkillSystem::GetSkillPower(caster, SKILL_PALBANG) == 0, "invalid group indexed skill table");
    g_registry.get<ecs::SkillLevels>(caster).group = 0;
    Check(SkillSystem::GetSkillPower(caster, SKILL_PALBANG) == 0, "unselected group");
    for (uint32_t skill = SKILL_LANGUAGE1; skill <= SKILL_LANGUAGE3; ++skill) {
        g_registry.get<Player>(caster).languageRing = true;
        Check(SkillSystem::GetSkillPower(caster, skill) == 100, "language ring");
        g_registry.get<Player>(caster).languageRing = false;
        Check(SkillSystem::GetSkillPower(caster, skill) == 0, "unequipped language ring");
    }
    Check(SkillSystem::GetSkillPower(caster, GUILD_SKILL_START) == 0, "missing guild");
    hasGuild = true; guildSkillLevel = 7;
    Check(SkillSystem::GetSkillPower(caster, GUILD_SKILL_START) == 100 * 7 / 7 / 7, "guild skill power");
    Check(SkillSystem::GetSkillPower(entt::null, SKILL_PALBANG) == 0 &&
        SkillSystem::GetSkillPower(caster, SKILL_MAX_NUM) == 0, "invalid power input");
    g_registry.destroy(caster); Actor();
    Check(SkillSystem::GetSkillPower(caster, SKILL_PALBANG) == 0, "stale power owner");
}
void SkillColorChecks() {
    Reset(); auto caster = Actor(), target = Actor("Target");
    ecs::SkillColor initial {};
    for (size_t row = 0; row < std::size(initial.data); ++row)
        for (size_t effect = 0; effect < std::size(initial.data[row]); ++effect)
            initial.data[row][effect] = uint32_t(row * 100 + effect);
    Check(SkillSystem::SetSkillColors(caster, initial), "native color load");
    Check(dbPackets == 0 && packets == 1 && g_registry.all_of<ecs::DirtyTag>(caster), "load persisted unexpectedly");
    const std::array<uint32_t, 5> selected {1, 2, 3, 4, 5}, reset {};
    Check(!SkillSystem::ChangeSkillColor(caster, 0, selected) && colorPayments == 0 &&
        savedColors.empty(), "unpaid color change");
    colorTokens = 1; refuseColorPayment = true;
    Check(!SkillSystem::ChangeSkillColor(caster, 0, selected), "failed payment changed color");
    Check(std::memcmp(g_registry.get<ecs::SkillColor>(caster).data, initial.data, sizeof(initial.data)) == 0,
        "payment failure mutated colors");
    refuseColorPayment = false;
    for (int slot = ESkillColorLength::MAX_SKILL_COUNT; slot < 256; ++slot)
        Check(!SkillSystem::ChangeSkillColor(caster, uint8_t(slot), selected), "client wrote buff/out-of-bounds slot");
    Check(colorTokens == 1 && colorPayments == 0, "invalid slot consumed item");
    Check(SkillSystem::ChangeSkillColor(caster, 0, selected) && colorTokens == 0 && colorPayments == 1, "paid colors");
    Check(savedColors.size() == 1 && savedColors.back().player_id == 7 &&
        std::equal(selected.begin(), selected.end(), savedColors.back().dwSkillColor[0]), "persisted color row");
    Check(std::equal(std::begin(initial.data[1]), std::end(initial.data[1]),
        g_registry.get<ecs::SkillColor>(caster).data[1]), "unrelated row overwritten");
    Check(SkillSystem::ChangeSkillColor(caster, 0, reset) && colorPayments == 1, "free color reset charged");
    Check(std::equal(reset.begin(), reset.end(), savedColors.back().dwSkillColor[0]), "reset not persisted");

    SkillSystem::SetSkillColors(caster, initial);
    const std::array<uint32_t, 5> ids {94, 95, 96, 110, 111};
    const std::array<size_t, 5> source {3, 4, 5, 4, 5};
    for (size_t i = 0; i < ids.size(); ++i) {
        Check(SkillSystem::CopyBuffSkillColor(caster, target, ids[i]), "buff color copy");
        Check(std::equal(std::begin(initial.data[source[i]]), std::end(initial.data[source[i]]),
            g_registry.get<ecs::SkillColor>(target).data[ESkillColorLength::BUFF_BEGIN + i]), "buff row mapping");
    }
    Check(!SkillSystem::CopyBuffSkillColor(caster, target, SKILL_PALBANG), "non-buff copy accepted");
    Check(SkillSystem::CopyBuffSkillColor(caster, caster, 94), "self buff color copy");
    auto blank = Actor("Blank");
    Check(SkillSystem::CopyBuffSkillColor(blank, target, 94) &&
        std::equal(reset.begin(), reset.end(), savedColors.back().dwSkillColor[ESkillColorLength::BUFF_BEGIN]),
        "missing caster colors retained previous buff color");
    g_registry.destroy(blank); Actor();
    const auto saves = savedColors.size();
    Check(!SkillSystem::SetSkillColors(blank, initial, true) &&
        !SkillSystem::CopyBuffSkillColor(blank, target, 94) &&
        !SkillSystem::ChangeSkillColor(entt::null, 0, selected) &&
        savedColors.size() == saves, "stale color entities");

    for (int mode = 0; mode < 3; ++mode) {
        Reset(); caster = Actor(); target = Actor("Other");
        colorTokens = 2;
        bool nestedAccepted = false;
        onColorPayment = [&](entt::entity e) {
            if (mode == 0) nestedAccepted = SkillSystem::ChangeSkillColor(e, 1, selected);
            if (mode == 1) { g_registry.destroy(e); target = Actor("Recycled"); }
            if (mode == 2) { SkillSystem::SetSkillColors(target, initial); SkillSystem::CopyBuffSkillColor(target, e, 94); }
        };
        const bool accepted = SkillSystem::ChangeSkillColor(caster, 0, selected);
        Check(colorPayments == 1 && !nestedAccepted, "recursive purchase");
        Check(accepted == (mode != 1), "payment callback lifecycle");
        Check(!g_registry.all_of<ecs::SkillColorChangeInProgress>(mode == 1 ? target : caster), "purchase guard retained");
        if (mode == 1) Check(savedColors.empty() && !g_registry.all_of<ecs::SkillColor>(target), "replacement got stale purchase");
        if (mode == 2) Check(std::equal(std::begin(initial.data[3]), std::end(initial.data[3]),
            g_registry.get<ecs::SkillColor>(caster).data[ESkillColorLength::BUFF_BEGIN]), "payment callback buff color lost");
    }
    Reset(); caster = Actor(); colorTokens = 1;
    bool once = false;
    onColorUpdate = [&](entt::entity e) {
        Check(!savedColors.empty(), "network update before persistence");
        if (!std::exchange(once, true)) SkillSystem::SetSkillColors(e, initial, true);
    };
    Check(SkillSystem::ChangeSkillColor(caster, 0, selected) && savedColors.size() == 2 &&
        savedColors.back().dwSkillColor[0][0] == initial.data[0][0], "nested update persisted stale colors last");
    Reset(); caster = Actor(); colorTokens = 1;
    onColorUpdate = [&](entt::entity e) { g_registry.destroy(e); target = Actor("Replacement"); };
    Check(SkillSystem::ChangeSkillColor(caster, 0, selected) && savedColors.size() == 1 && messages.empty() &&
        !g_registry.all_of<ecs::SkillColorChangeInProgress>(target), "update callback stale owner access");

    Reset(); caster = Actor();
    auto* db = db_clientdesc; db_clientdesc = nullptr; colorTokens = 1;
    Check(!SkillSystem::ChangeSkillColor(caster, 0, selected) && colorTokens == 1 &&
        !SkillSystem::SetSkillColors(caster, initial, true), "missing DB consumed payment");
    Check(SkillSystem::SetSkillColors(caster, initial), "non-persistent color load needs DB");
    db_clientdesc = db;
}

void SkillCommandChecks() {
    Reset(); auto player = Actor();
    std::array<TPlayerSkill, SKILL_MAX_NUM> levels {};
    g_registry.emplace<ecs::SkillLevels>(player, levels.data(), uint8_t{2});
    g_registry.emplace<ecs::CharacterPoints>(player);
    g_registry.emplace<ecs::NetworkSession>(player).desc = DescToken();
    levels[SKILL_PALBANG].bLevel = 8;
    for (const char* arg : {"1 -1", "1 256", "1 999999999999999999999", "1 40junk"}) {
        do_setskill(player, arg, 0, 0);
        Check(levels[SKILL_PALBANG].bLevel == 8 && computes == 0 && packets == 0, "invalid skill argument mutated state");
    }
    do_setskill(player, "1 40", 0, 0);
    Check(levels[SKILL_PALBANG].bLevel == 40 && computes == 1 && packets == 1, "entity-only skill command");
    for (const char* arg : {"", "-1", "3", "256", "999999999999999999999", "1junk"}) {
        do_set_skill_group(player, arg, 0, 0);
        Check(SkillSystem::GetSkillGroup(player) == 2 && levels[SKILL_PALBANG].bLevel == 40, "invalid group cleared skills");
    }
    do_set_skill_group(player, "1", 0, 0);
    Check(SkillSystem::GetSkillGroup(player) == 1 && levels[SKILL_PALBANG].bLevel == 0 && dbPackets == 1,
        "group command reset/color persistence");
    onPacket = [&] { if (g_registry.valid(player)) { g_registry.destroy(player); Actor(); } };
    do_set_skill_group(player, "2", 0, 0);
    Check(!g_registry.valid(player) && dbPackets == 1, "group packet destroyed owner before reset");
    for (int mode = 0; mode < 3; ++mode) {
        Reset(); player = Actor();
        g_registry.emplace<ecs::SkillLevels>(player, levels.data(), uint8_t{1});
        g_registry.emplace<ecs::CharacterPoints>(player);
        g_registry.emplace<ecs::NetworkSession>(player).desc = DescToken();
        auto invalidate = [&](entt::entity) { g_registry.destroy(player); Actor(); };
        if (mode == 0) onCompute = invalidate;
        if (mode == 1) onPacket = [&] { invalidate(player); };
        if (mode == 2) onPointChange = invalidate;
        if (mode == 2) SkillSystem::ClearSubSkill(player);
        else SkillSystem::ResetSkill(player);
        Check(!g_registry.valid(player) && dbPackets == 0, "skill reset wrote after callback destruction");
    }
}
void SocketChecks() {
    Reset(); const auto owner = Actor(), item = Give(owner);
    for (const char* arg : {"", "-1 0 1", "4294967296 0 1", "1234 -1 1", "1234 999 1",
        "1234 0 2147483648", "1234junk 0 1", "1234 0 2junk"}) {
        do_set_socket(owner, arg, 0, 0);
        Check(socketWrites == 0, "malformed socket command accepted");
    }
    do_set_socket(owner, "1234 0 42", 0, 0);
    Check(socketWrites == 1, "native item-ID socket lookup");
    g_registry.destroy(item); Give(owner);
    do_set_socket(owner, "9999 0 1", 0, 0);
    Check(socketWrites == 1, "missing item socket write");
}
}
namespace ecs::PlayerRuntime {
bool IsPC(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<TagPC>(e); }
bool IsValid(entt::entity e) { return g_registry.valid(e); }
LPDESC GetDesc(entt::entity e) { const auto* p = g_registry.valid(e) ? g_registry.try_get<Player>(e) : nullptr; return p && p->online ? DescToken() : nullptr; }
CNewPetSystem* GetNewPetSystem(entt::entity e) { Check(IsPC(e), "invalid pet owner"); return hasPet ? PetToken() : nullptr; }
std::string_view GetName(entt::entity e) { return g_registry.get<Player>(e).name; }
int32_t GetMapIndex(entt::entity e) { return g_registry.get<Player>(e).map; }
uint8_t GetEmpire(entt::entity e) { return g_registry.get<Player>(e).empire; }
uint8_t GetGMLevel(entt::entity e) { return g_registry.get<Player>(e).gm; }
}
namespace ecs::PointSystem {
int32_t GetLevel(entt::entity e) { return g_registry.get<Player>(e).level; }
}
namespace ItemSystem {
bool IsValidItem(entt::entity e) { return g_registry.valid(e) && g_registry.all_of<Item>(e); }
bool IsEquipUniqueGroup(entt::entity e, uint32_t group) {
    Check(group == UNIQUE_GROUP_RING_OF_LANGUAGE, "language ring group");
    return g_registry.get<Player>(e).languageRing;
}
bool RemoveSpecifyItemEcs(entt::entity e, uint32_t vnum, uint32_t count, bool renewal) {
    Check(ecs::PlayerRuntime::IsPC(e) && vnum == 164406 && count == 1 && !renewal, "native color item consumption");
    if (refuseColorPayment || colorTokens == 0) return false;
    --colorTokens; ++colorPayments;
    if (onColorPayment) onColorPayment(e);
    return true;
}
entt::entity GetItemOwner(entt::entity e) { return g_registry.get<Item>(e).owner; }
entt::entity GetItem(entt::entity e, TItemPos pos) {
    Check(ecs::PlayerRuntime::IsPC(e), "invalid inventory owner");
    auto it = slots.find({e, Key(pos)}); return it == slots.end() ? entt::null : it->second;
}
bool DestroyItemEntityEcs(entt::entity e, const char* reason) {
    Check(IsValidItem(e) && std::string_view(reason) == "PURGE", "invalid destroy request");
    if (rejectDestroy) return false;
    destroyed.push_back(e); std::erase_if(slots, [&](const auto& p) { return p.second == e; });
    g_registry.destroy(e); if (onDestroy) onDestroy(e); return true;
}
entt::entity FindItemByID(uint32_t id) {
    for (auto [e, item] : g_registry.view<Item>().each()) if (item.id == id) return e;
    return entt::null;
}
bool SetItemSocket(entt::entity e, int index, uint32_t value, bool) {
    Check(IsValidItem(e) && index == 0 && value == 42, "socket arguments"); ++socketWrites; return true;
}
}
namespace InventorySystem {
void SyncQuickslot(entt::entity owner, uint16_t type, uint16_t oldPos, uint16_t newPos) {
    Check(ecs::PlayerRuntime::IsPC(owner) && newPos == 255, "quickslot invalid owner/destination");
    quickslots.emplace_back(type, oldPos);
}
}
size_t CNewPetSystem::CountSummoned() const { Check(this == PetToken(), "pet service token"); return activePet ? 1 : 0; }
namespace ecs {
void ChatSystem::SendV(entt::entity e, uint8_t type, const char* format, va_list args) {
    SendNewV(e, type, 0, format, args);
}
void ChatSystem::Send(entt::entity e, uint8_t type, const char* format, ...) {
    va_list args; va_start(args, format); SendV(e, type, format, args); va_end(args);
}
void ChatSystem::SendNewV(entt::entity e, uint8_t type, uint32_t index, const char* format, va_list args) {
    if (!PlayerRuntime::IsPC(e)) return;
    char text[4096] {}; vsnprintf(text, sizeof(text), format, args);
    messages.push_back({e, type, index, text}); if (onChat) onChat(e);
}
void ChatSystem::SendNew(entt::entity e, uint8_t type, uint32_t index, const char* format, ...) {
    va_list args; va_start(args, format); SendNewV(e, type, index, format, args); va_end(args);
}
}
void DESC::Packet(const void* data, int size) {
    Check(this == DescToken() && data, "descriptor token");
    Check(size == sizeof(TPacketGCChangeSkillGroup) || size == sizeof(TPacketGCSkillLevel), "packet type");
    ++packets; if (onPacket) onPacket();
}
P2P_MANAGER::P2P_MANAGER() = default;
P2P_MANAGER::~P2P_MANAGER() = default;
void P2P_MANAGER::Send(const void* data, int size, LPDESC) {
    Check(size >= sizeof(TPacketGGChatNew), "P2P header missing");
    p2p.assign(static_cast<const char*>(data), static_cast<const char*>(data) + size);
}

int test_server = 0, g_bItemCountLimit = 200, gPlayerMaxLevel = 120, passes_per_sec = 25;
uint8_t g_bChannel = 1;
CLIENT_DESC* db_clientdesc = nullptr;
int (*check_name)(const char*) = nullptr;
TJobInitialPoints JobInitialPoints[JOB_MAX_NUM] {};
const char* c_apszPrivNames[MAX_PRIV_NUM] {};
const int aiSkillBookCountForLevelUp[10] {};
const int aiGrandMasterSkillBookCountForLevelUp[10] {};
const int aiGrandMasterSkillBookMinCount[10] {};
const int aiGrandMasterSkillBookMaxCount[10] {};
const int* aiChainLightningCountBySkillLevel = nullptr;
uint32_t g_dwSkillBookNextReadMin = 1, g_dwSkillBookNextReadMax = 1;
bool g_bSkillDisable = false;
uint32_t g_start_position[4][2] {};
entt::dispatcher g_dispatcher;
namespace logging {
std::shared_ptr<spdlog::logger> GetLogger() {
    static auto logger = std::make_shared<spdlog::logger>("gm-tests");
    return logger;
}
std::shared_ptr<spdlog::logger> GetErrorLogger() { return GetLogger(); }
}
namespace {
[[noreturn]] void Unexpected() { throw std::runtime_error("unexpected legacy/live service"); }
}
// Link-only service doubles for unexecuted GM/combat/quest paths. Any accidental
// call fails the test; these are not alternate production implementations.

CMob const * CMobManager::Get(unsigned int) { Unexpected(); }
CMob const * CMobManager::Get(char const *,bool) { Unexpected(); }
unsigned int CombatSystem::GetRealAlignment(entt::entity) { Unexpected(); }
void CombatSystem::UpdateAlignment(entt::entity,int64_t) { Unexpected(); }
void Cube_init(void) { Unexpected(); }
bool InventorySystem::AddToCharacter(entt::entity,entt::entity,SItemPos,bool) { Unexpected(); }
bool InventorySystem::EquipTo(entt::entity,entt::entity,unsigned char) { Unexpected(); }
unsigned int ecs::PlayerRuntime::GetPlayerID(entt::entity e) { Check(IsPC(e), "invalid PID lookup"); return 7; }
unsigned int ecs::PlayerRuntime::GetRaceNum(entt::entity) { Unexpected(); }
unsigned char ecs::PlayerRuntime::GetSex(entt::entity) { Unexpected(); }
int64_t ecs::PlayerRuntime::GetHP(entt::entity) { Unexpected(); }
unsigned char ecs::PlayerRuntime::GetJob(entt::entity e) { return g_registry.get<Player>(e).job; }
bool ecs::PlayerRuntime::ChangeSex(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::SetRace(entt::entity,unsigned char) { Unexpected(); }
int ecs::PlayerRuntime::GetX(entt::entity) { Unexpected(); }
int ecs::PlayerRuntime::GetY(entt::entity) { Unexpected(); }
int ecs::PlayerRuntime::GetZ(entt::entity) { Unexpected(); }
SECTREE * ecs::PlayerRuntime::GetSectree(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsNPC(entt::entity) { Unexpected(); }
void ecs::PlayerRuntime::SetGold(entt::entity,int64_t) { Unexpected(); }
bool ecs::PlayerRuntime::IsStone(entt::entity) { Unexpected(); }
bool ecs::PlayerRuntime::IsObserverMode(entt::entity) { Unexpected(); }
int ecs::PlayerRuntime::GetPosition(entt::entity) { Unexpected(); }
bool ecs::MovementSystem::Show(entt::entity,int,int,int,int,bool) { Unexpected(); }
bool ecs::MovementSystem::WarpSet(entt::entity,int,int,int) { Unexpected(); }
void ecs::MovementSystem::SaveExitLocation(entt::entity) { Unexpected(); }
void ecs::MovementSystem::Stop(entt::entity) { Unexpected(); }
std::vector<std::shared_ptr<CAffect>,std::allocator<std::shared_ptr<CAffect> > > AffectSystem::Snapshot(entt::entity) { Unexpected(); }
void AffectSystem::ApplyPoison(entt::entity,entt::entity) { Unexpected(); }
bool AffectSystem::IsImmune(entt::entity,unsigned int) { Unexpected(); }
CAffect * AffectSystem::FindAffect(entt::entity,unsigned int,unsigned char) { Unexpected(); }
bool AffectSystem::IsAffectFlag(entt::entity,unsigned int) { Unexpected(); }
bool AffectSystem::AddAffect(entt::entity,unsigned int,unsigned char,int,unsigned int,int,int,bool,bool) { Unexpected(); }
bool AffectSystem::RemoveAffect(entt::entity,unsigned int) { Unexpected(); }
bool AffectSystem::RemoveAffect(entt::entity,CAffect *) { Unexpected(); }
void AffectSystem::RemoveBadAffects(entt::entity) { Unexpected(); }
void AffectSystem::RemoveGoodAffects(entt::entity) { Unexpected(); }
void AffectSystem::ClearAffect(entt::entity,bool) { Unexpected(); }
void AffectSystem::SetPolymorph(entt::entity,unsigned int,bool) { Unexpected(); }
bool AffectSystem::IsPolymorphed(entt::entity) { Unexpected(); }
CParty * ecs::SocialSystem::GetParty(entt::entity) { Unexpected(); }
CGuild * ecs::SocialSystem::GetGuild(entt::entity e) {
    Check(ecs::PlayerRuntime::IsPC(e), "guild lookup entity");
    static int token;
    return hasGuild ? reinterpret_cast<CGuild*>(&token) : nullptr;
}
bool ecs::SocialSystem::HasExchange(entt::entity) { Unexpected(); }
CShop * ecs::SocialSystem::GetShop(entt::entity) { Unexpected(); }
int ecs::QuestSystem::GetFlag(entt::entity,std::string_view) { Unexpected(); }
int64_t ecs::PointSystem::Get(entt::entity e, unsigned char type) { return g_registry.get<ecs::CharacterPoints>(e).base.points[type]; }
int64_t ecs::PointSystem::GetReal(entt::entity,unsigned char) { Unexpected(); }
int64_t ecs::PointSystem::GetGold(entt::entity) { Unexpected(); }
int ecs::PointSystem::GetMaxHP(entt::entity) { Unexpected(); }
int ecs::PointSystem::GetMaxSP(entt::entity) { Unexpected(); }
bool ecs::PointSystem::Set(entt::entity,unsigned char,int64_t) { Unexpected(); }
bool ecs::PointSystem::SetReal(entt::entity,unsigned char,int64_t) { Unexpected(); }
bool ecs::PointSystem::ResetAllPoints(entt::entity,int) { Unexpected(); }
void ecs::PointSystem::Compute(entt::entity e) { if (!g_registry.valid(e)) return; ++computes; if (onCompute) onCompute(e); }
void ecs::PointSystem::Change(entt::entity e, unsigned char type, int64_t value, bool, bool, bool) {
    Check(g_registry.valid(e), "invalid point-change owner");
    g_registry.get<ecs::CharacterPoints>(e).base.points[type] += value;
    if (onPointChange) onPointChange(e);
}
CSkillManager::CSkillManager() = default;
CSkillManager::~CSkillManager() = default;
CSkillProto * CSkillManager::Get(unsigned int vnum) {
    static CSkillProto proto {};
    proto.dwVnum = SKILL_PALBANG;
    return vnum == 1 ? &proto : nullptr;
}
CSkillProto * CSkillManager::Get(char const* name) { return std::string_view(name) == "palbang" ? Get(1) : nullptr; }
void CGuild::RequestDisband(unsigned int) { Unexpected(); }
unsigned int CGuild::UnderAnyWar(unsigned char) { Unexpected(); }
bool CEntity::IsType(int)const { Unexpected(); }
SECTREE * CEntity::GetSectree(void)const { Unexpected(); }
void CEntity::SetObserverMode(bool) { Unexpected(); }
short CHorseRider::GetHorseMaxHealth(void) { Unexpected(); }
short CHorseRider::GetHorseMaxStamina(void) { Unexpected(); }
void CHorseRider::UpdateHorseStamina(int,bool) { Unexpected(); }
void CHorseRider::UpdateHorseHealth(int,bool) { Unexpected(); }
int mining::RealRefinePick(entt::entity,entt::entity) { Unexpected(); }
void mining::CHEAT_MAX_PICK(entt::entity,entt::entity) { Unexpected(); }
void CHARACTER::SetCoward(void) { Unexpected(); }
void CHARACTER::Save(void) { Unexpected(); }
int64_t CHARACTER::GetHP(void)const { Unexpected(); }
int64_t CHARACTER::GetSP(void)const { Unexpected(); }
void CHARACTER::ComputePoints(void) { Unexpected(); }
bool CHARACTER::WarpToPID(unsigned int) { Unexpected(); }
int CHARACTER::CountSpecifyItem(unsigned int)const { Unexpected(); }
void CHARACTER::RemoveSpecifyItem(unsigned int,int,bool) { Unexpected(); }
void CHARACTER::Dead(entt::entity,bool) { Unexpected(); }
void CHARACTER::ForgetMyAttacker(void) { Unexpected(); }
void CHARACTER::AggregateMonster(void) { Unexpected(); }
void CHARACTER::AttractRanger(void) { Unexpected(); }
void CHARACTER::PullMonster(void) { Unexpected(); }
void CHARACTER::ChangeSafeboxSize(unsigned char) { Unexpected(); }
void CHARACTER::HorseSummon(bool,bool,unsigned int,char const *) { Unexpected(); }
CHARACTER * CHARACTER::GetRider(void)const { Unexpected(); }
bool CHARACTER::IsRidingMount(void) { Unexpected(); }
bool CHARACTER::IsPet(void)const { Unexpected(); }
bool CHARACTER::IsMount(void)const { Unexpected(); }
bool CHARACTER::IsNewPet(void)const { Unexpected(); }
void CPartyManager::DeleteParty(CParty *) { Unexpected(); }
void CParty::Quit(unsigned int) { Unexpected(); }
unsigned int CParty::GetMemberCount(void) { Unexpected(); }
unsigned int SECTREE::GetAttribute(int,int) { Unexpected(); }
LPENTITY SectreeLegacyEntity(entt::entity) { Unexpected(); }
bool SectreeMember(entt::entity, const SECTREE*) { Unexpected(); }
void SECTREE::Collect(FCollectEntity&) const { Unexpected(); }
FCollectEntity SECTREE::SnapshotAround(int) const { Unexpected(); }
SECTREE_MAP * SECTREE_MANAGER::GetMap(int) { Unexpected(); }
SECTREE * SECTREE_MANAGER::Get(int,int,int) { Unexpected(); }
bool SECTREE_MANAGER::GetMapBasePosition(int,int,pixel_position_s &) { Unexpected(); }
int SECTREE_MANAGER::CreatePrivateMap(int) { Unexpected(); }
bool SECTREE_MANAGER::SaveAttributeToImage(int,char const *,SECTREE_MAP *) { Unexpected(); }
void MountSystem::SummonHorse(entt::entity,bool,bool,unsigned int,char const *) { Unexpected(); }
void MountSystem::SetMountVnum(entt::entity,unsigned int) { Unexpected(); }
void NetworkSyncSystem::UpdatePacket(entt::entity e) {
    Check(ecs::PlayerRuntime::IsPC(e) && g_registry.all_of<ecs::SkillColor>(e), "invalid color update owner");
    ++packets;
    if (onColorUpdate) onColorUpdate(e);
}
void NetworkSyncSystem::BroadcastEffect(entt::basic_registry<entt::entity,std::allocator<entt::entity> > &,entt::entity,unsigned char) { Unexpected(); }
char const * get_table_postfix(void) { Unexpected(); }
void LoadStateUserCount(void) { Unexpected(); }
void CLIENT_DESC::DBPacketHeader(unsigned char header, unsigned int, unsigned int size) {
    Check(this == db_clientdesc && header == HEADER_GD_SKILL_COLOR_SAVE && size == sizeof(TSkillColor), "DB color header");
}
void CLIENT_DESC::DBPacket(unsigned char header, unsigned int handle, void const* data, unsigned int size) {
    Check(this == db_clientdesc && header == HEADER_GD_SKILL_COLOR_SAVE && handle == 0 &&
        data && size == sizeof(TSkillColor), "native color persistence packet");
    TSkillColor packet {};
    std::memcpy(&packet, data, sizeof(packet));
    savedColors.push_back(packet); ++dbPackets;
}
void CLIENT_DESC::Packet(void const* data, int size) {
    Check(this == db_clientdesc && data && size == sizeof(TSkillColor), "DB color packet"); ++dbPackets;
}
void DESC_MANAGER::DestroyDesc(DESC *,bool) { Unexpected(); }
DESC * DESC_MANAGER::FindByCharacterName(char const *) { Unexpected(); }
void DESC_MANAGER::GetUserCount(int &,int * *,int &) { Unexpected(); }
void CHARACTER_MANAGER::DestroyCharacter(CHARACTER *) { Unexpected(); }
entt::entity CHARACTER_MANAGER::SpawnMobEntity(unsigned int,int,int,int,int,bool,int,bool) { Unexpected(); }
CHARACTER * CHARACTER_MANAGER::SpawnMobRange(unsigned int,int,int,int,int,int,bool,bool,bool) { Unexpected(); }
CHARACTER * CHARACTER_MANAGER::SpawnGroup(unsigned int,int,int,int,int,int,regen *,bool,CDungeon *) { Unexpected(); }
bool CHARACTER_MANAGER::SpawnGroupGroup(unsigned int,int,int,int,int,int,regen *,bool,CDungeon *) { Unexpected(); }
CHARACTER * CHARACTER_MANAGER::SpawnMobRandomPosition(unsigned int,int) { Unexpected(); }
CHARACTER * CHARACTER_MANAGER::FindPC(char const *) { Unexpected(); }
entt::entity CHARACTER_MANAGER::FindEntity(unsigned int) { Unexpected(); }
entt::entity CHARACTER_MANAGER::FindPCEntity(char const *) { Unexpected(); }
entt::entity ITEM_MANAGER::CreateItem(unsigned int,unsigned int,unsigned int,bool,int,bool) { Unexpected(); }
bool ITEM_MANAGER::GetVnum(char const *,unsigned int &) { Unexpected(); }
bool ITEM_MANAGER::ConvSpecialDropItemFile(void) { Unexpected(); }
void regen_reset(int,int) { Unexpected(); }
unsigned int CGuildManager::CreateGuild(TGuildCreateParameter &) { Unexpected(); }
CGuild * CGuildManager::FindGuild(unsigned int) { Unexpected(); }
CGuild * CGuildManager::FindGuildByName(std::string) { Unexpected(); }
void CGuildManager::ShowGuildWarList(entt::entity) { Unexpected(); }
void CGuildManager::RequestEndWar(unsigned int,unsigned int) { Unexpected(); }
void CGuildManager::RequestCancelWar(unsigned int,unsigned int) { Unexpected(); }
_CCI * P2P_MANAGER::Find(char const *) { Unexpected(); }
void fishing::Initialize(void) { Unexpected(); }
void fishing::Simulation(int,int,int,entt::entity) { Unexpected(); }
void quest::PC::EndRunning(void) { Unexpected(); }
void quest::PC::SetQuest(std::string const &,quest::QuestState &) { Unexpected(); }
void quest::PC::ClearQuest(std::string const &) { Unexpected(); }
void quest::PC::SetFlag(std::string const &,int,bool) { Unexpected(); }
bool quest::PC::DeleteFlag(std::string const &) { Unexpected(); }
std::string const & quest::PC::GetCurrentQuestName(void)const { Unexpected(); }
void quest::PC::ClearTimer(void) { Unexpected(); }
void quest::PC::SendFlagList(entt::entity) { Unexpected(); }
quest::QuestState quest::CQuestManager::OpenState(std::string const &,int)const { Unexpected(); }
void quest::CQuestManager::CloseState(quest::QuestState &)const { Unexpected(); }
bool quest::CQuestManager::RunState(quest::QuestState &) { Unexpected(); }
quest::PC * quest::CQuestManager::GetPC(unsigned int) { Unexpected(); }
quest::PC * quest::CQuestManager::GetPCForce(unsigned int) { Unexpected(); }
int quest::CQuestManager::GetQuestStateIndex(std::string const &,std::string const &) { Unexpected(); }
unsigned int quest::CQuestManager::GetQuestIndexByName(std::string const &) { Unexpected(); }
void quest::CQuestManager::RequestSetEventFlag(std::string const &,int) { Unexpected(); }
void quest::CQuestManager::SendEventFlagList(entt::entity) { Unexpected(); }
void quest::CQuestManager::Reload(void) { Unexpected(); }
void DBManager::ReturnQuery(int,unsigned int,void *,char const *,...) { Unexpected(); }
void DBManager::SendMoneyLog(unsigned char,unsigned int,int64_t) { Unexpected(); }
void DBManager::LoadDBString(void) { Unexpected(); }
void CPrivManager::RequestGiveEmpirePriv(unsigned char,unsigned char,int,int64_t) { Unexpected(); }
int CPrivManager::GetPriv(entt::entity,unsigned char) { Unexpected(); }
int CPrivManager::GetPrivByEmpire(unsigned char,unsigned char) { Unexpected(); }
int CPrivManager::GetPrivByGuild(unsigned int,unsigned char) { Unexpected(); }
int CPrivManager::GetPrivByCharacter(unsigned int,unsigned char) { Unexpected(); }
building::CObject * building::CLand::FindObjectByGroup(unsigned int) { Unexpected(); }
bool building::CLand::RequestCreateObject(unsigned int,int,int,int,float,float,float,bool) { Unexpected(); }
void building::CLand::RequestDeleteObjectByVID(unsigned int) { Unexpected(); }
bool building::CLand::RequestCreateWall(int,float) { Unexpected(); }
void building::CLand::RequestDeleteWall(void) { Unexpected(); }
bool building::CLand::RequestCreateWallBlocks(unsigned int,int,char,bool,bool,bool,bool) { Unexpected(); }
void building::CLand::RequestDeleteWallBlocks(unsigned int) { Unexpected(); }
building::SObjectProto * building::CManager::GetObjectProto(unsigned int) { Unexpected(); }
building::CLand * building::CManager::FindLand(int,int,int) { Unexpected(); }
void building::CManager::ClearLand(unsigned int) { Unexpected(); }
bool CArenaManager::StartDuel(entt::entity,entt::entity,int,int) { Unexpected(); }
void CArenaManager::SendArenaMapListTo(entt::entity) { Unexpected(); }
void CArenaManager::EndAllDuel(void) { Unexpected(); }
bool CArenaManager::EndDuel(unsigned int) { Unexpected(); }
void LogManager::ItemLogEntity(entt::entity,entt::entity,char const *,char const *) { Unexpected(); }
void LogManager::CharLog(entt::entity,unsigned int,char const *,char const *) { Unexpected(); }
void CPCBangManager::RequestUpdateIPList(unsigned long) { Unexpected(); }
bool CPCBangManager::IsPCBangIP(char const *) { Unexpected(); }
entt::entity ItemSystem::GetInventoryItem(entt::entity,unsigned short) { Unexpected(); }
entt::entity ItemSystem::GetWearItem(entt::entity,unsigned char) { Unexpected(); }
bool ItemSystem::UnequipItemEcs(entt::entity,entt::entity) { Unexpected(); }
bool ItemSystem::UseItemEcs(entt::entity,entt::entity,SItemPos) { Unexpected(); }
entt::entity ItemSystem::AutoGiveItemEcs(entt::entity,unsigned int,unsigned int,int,bool,bool) { Unexpected(); }
bool ItemSystem::IsDragonSoulItem(entt::entity) { Unexpected(); }
bool ItemSystem::IsExtraItem(entt::entity) { Unexpected(); }
unsigned int ItemSystem::GetItemID(entt::entity) { Unexpected(); }
char const * ItemSystem::GetItemName(entt::entity) { Unexpected(); }
int ItemSystem::FindEquipCell(entt::entity,entt::entity,int) { Unexpected(); }
unsigned int ItemSystem::GetItemSocket(entt::entity,int) { Unexpected(); }
bool ItemSystem::SetItemSocketEcs(entt::entity,int,unsigned int) { Unexpected(); }
bool ItemSystem::SetItemForceAttributeEcs(entt::entity,int,unsigned char,short) { Unexpected(); }
bool ItemSystem::AddItemAttributeEcs(entt::entity) { Unexpected(); }
bool ItemSystem::AddItemRareAttributeEcs(entt::entity) { Unexpected(); }
bool ItemSystem::ChangeItemAttributeEcs(entt::entity,int const *) { Unexpected(); }
bool ItemSystem::ChangeItemRareAttributeEcs(entt::entity) { Unexpected(); }
bool ItemSystem::ClearItemAttributesEcs(entt::entity) { Unexpected(); }
unsigned short ItemSystem::GetItemCell(entt::entity) { Unexpected(); }
int ItemSystem::GetEmptyInventoryPositionEcs(entt::entity,entt::entity) { Unexpected(); }
int ItemSystem::GetEmptyDragonSoulInventory(entt::entity,entt::entity) { Unexpected(); }
int ItemSystem::GetEmptyExtraInventory(entt::entity,entt::entity) { Unexpected(); }
bool ItemSystem::ModifyItemPointsEcs(entt::entity,bool) { Unexpected(); }
int ActivitySystem::RefineFishingRod(entt::entity,entt::entity) { Unexpected(); }
char const * offlineshop::CShop::GetName(void)const { Unexpected(); }
offlineshop::CShop * offlineshop::CShopManager::GetShopByOwnerID(unsigned int) { Unexpected(); }
void offlineshop::CShopManager::SendShopForceCloseDBPacket(unsigned int) { Unexpected(); }
void offlineshop::CShopManager::SendShopChangeNameDBPacket(unsigned int,char const *) { Unexpected(); }
offlineshop::CShopManager & offlineshop::GetManager(void) { Unexpected(); }
bool DragonSoulSystem::ActivateDeck(entt::entity,int) { Unexpected(); }
void DragonSoulSystem::DeactivateAll(entt::entity) { Unexpected(); }
void intrusive_ptr_add_ref(event *) { Unexpected(); }
void intrusive_ptr_release(event *) { Unexpected(); }
boost::intrusive_ptr<event> event_create_ex(int (__cdecl*)(boost::intrusive_ptr<event>,int),event_info_data *,int) { Unexpected(); }
void event_cancel(boost::intrusive_ptr<event> *) { Unexpected(); }
unsigned int ecs::PlayerRuntime::GetAIFlag(entt::entity) { Unexpected(); }
unsigned char CombatSystem::ToggleComboIndex(entt::entity,unsigned char) { Unexpected(); }
void CSkillProto::SetPointVar(std::string_view,double) { Unexpected(); }
void CSkillProto::SetDurationVar(std::string_view,double) { Unexpected(); }
void CSkillProto::SetSPCostVar(std::string_view,double) { Unexpected(); }
int CGuild::GetSkillLevel(unsigned int) { return guildSkillLevel; }
int CEntity::GetX(void)const { Unexpected(); }
int CEntity::GetY(void)const { Unexpected(); }
pixel_position_s CEntity::GetXYZ(void)const { Unexpected(); }
char const * CHARACTER::GetName(unsigned char)const { Unexpected(); }
unsigned char CHARACTER::GetJob(void)const { Unexpected(); }
int CHARACTER::GetLevel(void)const { Unexpected(); }
void CHARACTER::SetHP(int64_t) { Unexpected(); }
int64_t CHARACTER::GetMaxHP(void)const { Unexpected(); }
int64_t CHARACTER::GetMaxSP(void)const { Unexpected(); }
int64_t CHARACTER::GetPoint(unsigned char)const { Unexpected(); }
int CHARACTER::GetLimitPoint(unsigned char)const { Unexpected(); }
unsigned int CHARACTER::GetMobDamageMin(void)const { Unexpected(); }
unsigned int CHARACTER::GetMobDamageMax(void)const { Unexpected(); }
void CHARACTER::PointChange(unsigned char,int64_t,bool,bool,bool) { Unexpected(); }
float CHARACTER::GetRotation(void)const { Unexpected(); }
void CHARACTER::CreateFly(unsigned char,entt::entity) { Unexpected(); }
bool CHARACTER::Goto(int,int) { Unexpected(); }
bool CHARACTER::CanMove(void)const { Unexpected(); }
bool CHARACTER::Sync(int,int) { Unexpected(); }
void CHARACTER::OnMove(bool) { Unexpected(); }
void CHARACTER::CalculateMoveDuration(void) { Unexpected(); }
bool CHARACTER::WarpSet(int,int,int) { Unexpected(); }
bool CHARACTER::AddAffect(unsigned int,unsigned char,int,unsigned int,int,int,bool,bool) { Unexpected(); }
bool CHARACTER::RemoveAffect(unsigned int) { Unexpected(); }
bool CHARACTER::IsAffectFlag(unsigned int)const { Unexpected(); }
bool CHARACTER::IsGoodAffect(unsigned char)const { Unexpected(); }
void CHARACTER::RemoveGoodAffect(void) { Unexpected(); }
void CHARACTER::RemoveBadAffect(void) { Unexpected(); }
CAffect * CHARACTER::FindAffect(unsigned int,unsigned char)const { Unexpected(); }
bool CHARACTER::IsEquipUniqueGroup(unsigned int)const { Unexpected(); }
bool CHARACTER::Damage(entt::entity,int64_t,EDamageType) { Unexpected(); }
bool CHARACTER::CanBeginFight(void)const { Unexpected(); }
void CHARACTER::BeginFight(entt::entity) { Unexpected(); }
bool CHARACTER::IsStun(void)const { Unexpected(); }
int CHARACTER::GetArrowAndBow(entt::entity *,entt::entity *,int) { Unexpected(); }
void CHARACTER::AttackedByPoison(entt::entity) { Unexpected(); }
void CHARACTER::AttackedByFire(entt::entity,int,int) { Unexpected(); }
void CHARACTER::SetSkillHit(bool) { Unexpected(); }
bool CHARACTER::CanUseHorseSkill(void) { Unexpected(); }
bool CHARACTER::IsRiding(void)const { Unexpected(); }
int CHARACTER::GetQuestFlag(std::string const &)const { Unexpected(); }
void CHARACTER::SetQuestFlag(std::string const &,int) { Unexpected(); }
int CHARACTER::GetSkillPowerByLevel(int,bool)const { Unexpected(); }
int CHARACTER::GetSoulItemDamage(entt::entity,int,unsigned char) { Unexpected(); }
CHARACTER * CParty::GetNextOwnership(CHARACTER *,int,int) { Unexpected(); }
bool SECTREE_MANAGER::GetRecallPositionByEmpire(int,unsigned char,pixel_position_s &) { Unexpected(); }
bool MountSystem::IsRiding(entt::entity) { Unexpected(); }
unsigned int MountSystem::GetMountVnum(entt::entity) { Unexpected(); }
void NetworkSyncSystem::BroadcastSyncPacket(entt::basic_registry<entt::entity,std::allocator<entt::entity> > &,entt::entity) { Unexpected(); }
float GetDegreeFromPositionXY(int,int,int,int) { Unexpected(); }
void GetDeltaByDegree(float,float,float *,float *) { Unexpected(); }
float GetDegreeDelta(float,float) { Unexpected(); }
CHARACTER * CHARACTER_MANAGER::FindByPID(unsigned int) { Unexpected(); }
int CalcAttBonus(entt::entity,entt::entity,int) { Unexpected(); }
int CalcBattleDamage(int,int,int) { Unexpected(); }
int CalcMeleeDamage(entt::entity,entt::entity,bool,bool) { Unexpected(); }
int CalcMagicDamage(entt::entity,entt::entity) { Unexpected(); }
int CalcArrowDamage(entt::entity,entt::entity,entt::entity,entt::entity,bool) { Unexpected(); }
float CalcAttackRating(entt::entity,entt::entity,bool) { Unexpected(); }
bool battle_is_attackable(entt::entity,entt::entity) { Unexpected(); }
int quest::PC::GetFlag(std::string const &) { Unexpected(); }
int quest::CQuestManager::GetEventFlag(std::string const &) { Unexpected(); }
bool RaceToJob(unsigned int,unsigned int *) { Unexpected(); }
unsigned char ItemSystem::GetItemType(entt::entity) { Unexpected(); }
unsigned char ItemSystem::GetItemSubType(entt::entity) { Unexpected(); }
int ItemSystem::GetItemValue(entt::entity,unsigned int) { Unexpected(); }

int main() {
    try {
        P2P_MANAGER peers;
        CSkillManager skills;
        CTableBySkill power;
        for (int index = 0; index < JOB_MAX_NUM * 2; ++index) {
            std::array<int, SKILL_MAX_LEVEL + 1> table {};
            for (size_t level = 0; level < table.size(); ++level) table[level] = index * 100 + int(level);
            power.SetSkillPowerByLevelFromType(index, table.data());
        }
        static int dbToken; db_clientdesc = reinterpret_cast<CLIENT_DESC*>(&dbToken);
        PurgeChecks(); NoticeChecks(); SkillChecks(); SkillRuntimeChecks(); SkillPowerChecks(); SkillColorChecks(); SkillCommandChecks(); SocketChecks();
        std::cout << "GM command checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
