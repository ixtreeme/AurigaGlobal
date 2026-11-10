#include "../core/stdafx.h"
#include "../core/config.h"
#include "../entity/char.h" // EPointTypes: POINT_HP, POINT_SP
#include "BenchmarkSession.hpp"
#include "BenchmarkEquipment.hpp"

#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace nexoria::benchmark {
namespace {

thread_local uint32_t currentSkillVnum = 0;

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string Csv(std::string_view source) {
    std::string out = "\"";
    for (char c : source) {
        if (c == '"') out += "\"\"";
        else if (c == '\r' || c == '\n') out += ' ';
        else out += c;
    }
    return out + '"';
}

struct Session {
    uint64_t id = 0;
    uint32_t ownerPid = 0;
    int tier = 0;
    int mapIndex = 0;
    std::string weaponType;
    std::set<uint32_t> targets;
    std::map<uint32_t, entt::entity> targetEntities;
    std::filesystem::path path;
    std::ofstream file;
    uint32_t round = 1;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t damage = 0;
};

struct Finished {
    std::string path;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t damage = 0;
};

std::mutex sessionsMutex;
std::map<uint32_t, std::unique_ptr<Session>> active;
std::map<uint32_t, Finished> finished;
uint64_t sessionSerial = 0;

bool ValidWeapon(std::string_view name) {
    return name == "sword" || name == "twohand" || name == "dagger" ||
           name == "bow" || name == "bell" || name == "fan";
}

bool IsRecordedPair(const Session& s, entt::entity attacker, entt::entity victim) {
    return s.ownerPid == ecs::PlayerRuntime::GetPlayerID(attacker) &&
           s.targets.count(ecs::PlayerRuntime::GetPlayerID(victim)) != 0 &&
           s.mapIndex == ecs::PlayerRuntime::GetMapIndex(attacker) &&
           s.mapIndex == ecs::PlayerRuntime::GetMapIndex(victim);
}

void WriteEvent(Session& s, std::string_view event, entt::entity attacker,
                entt::entity victim, int64_t hpBefore, int64_t hpAfter,
                uint8_t damageType, bool critical, bool penetrate,
                std::string_view reason) {
    if (!s.file) return;
    const auto weapon = ItemSystem::GetWearItem(attacker, WEAR_WEAPON);
    const auto weaponVnum = ItemSystem::IsValidItem(weapon) ? ItemSystem::GetItemVnum(weapon) : 0;
    const int64_t actual = std::max<int64_t>(0, hpBefore - hpAfter);
    s.file << NowMs() << ',' << s.id << ',' << s.round << ','
           << Csv(event) << ',' << s.tier << ',' << Csv(s.weaponType) << ','
           << ecs::PlayerRuntime::GetPlayerID(attacker) << ','
           << Csv(ecs::PlayerRuntime::GetName(attacker)) << ','
           << int(ecs::PlayerRuntime::GetJob(attacker)) << ','
           << int(SkillSystem::GetSkillGroup(attacker)) << ','
           << ecs::PointSystem::GetLevel(attacker) << ','
           << ecs::PlayerRuntime::GetPlayerID(victim) << ','
           << Csv(ecs::PlayerRuntime::GetName(victim)) << ','
           << int(ecs::PlayerRuntime::GetJob(victim)) << ','
           << int(SkillSystem::GetSkillGroup(victim)) << ','
           << ecs::PointSystem::GetLevel(victim) << ','
           << weaponVnum << ',' << currentSkillVnum << ','
           << int(damageType) << ',' << (critical ? 1 : 0) << ','
           << (penetrate ? 1 : 0) << ',' << hpBefore << ',' << hpAfter << ','
           << actual << ',' << Csv(reason) << '\n';
    s.file.flush();
    if (event == "HIT") {
        ++s.hits;
        s.damage += static_cast<uint64_t>(actual);
    } else if (event == "MISS") {
        ++s.misses;
    }
}

uint32_t Pid(entt::entity gm) {
    return gm == entt::null || !ecs::PlayerRuntime::IsPC(gm) ? 0 :
        ecs::PlayerRuntime::GetPlayerID(gm);
}

std::string Summary(const Session& s) {
    std::ostringstream o;
    o << "Session=" << s.id << " tier=" << s.tier << " weapon=" << s.weaponType
      << " targets=" << s.targets.size() << " hits=" << s.hits
      << " misses=" << s.misses << " damage=" << s.damage
      << " file=" << s.path.string();
    return o.str();
}

} // namespace

ScopedSkillContext::ScopedSkillContext(uint32_t vnum) noexcept : previous_(currentSkillVnum) {
    currentSkillVnum = vnum;
}
ScopedSkillContext::~ScopedSkillContext() noexcept { currentSkillVnum = previous_; }

bool Start(entt::entity gm, int tier, std::string_view weaponType, std::string& message) {
    if (!test_server || !Pid(gm) || !ValidWeapon(weaponType)) {
        message = "Usage: /benchmark start <max|level> <sword|twohand|dagger|bow|bell|fan>";
        return false;
    }
    // The verified Zodiac endgame equipment requires at least level 158.
    // The server maximum is loaded from the running core configuration.
    if (tier < 158 || tier > gPlayerMaxLevel) {
        message = "Benchmark level " + std::to_string(tier) +
                  " is unsupported for the verified final set. Allowed range: 158.." +
                  std::to_string(gPlayerMaxLevel) + ". Use /benchmark start max <weapon>.";
        return false;
    }
    const uint32_t pid = Pid(gm);
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        if (active.count(pid)) {
            message = "Benchmark already running; use /benchmark stop.";
            return false;
        }
    }
    std::string profileMessage;
    if (!equipment::Apply(gm, tier, weaponType, false, profileMessage)) {
        message = "Attacker setup failed: " + profileMessage;
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories("benchmark_logs", ec);
    if (ec) {
        equipment::Restore(gm, profileMessage);
        message = "Cannot create benchmark_logs directory: " + ec.message(); return false;
    }
    auto session = std::make_unique<Session>();
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        session->id = NowMs() * 1000 + (++sessionSerial % 1000);
    }
    session->ownerPid = pid;
    session->tier = tier;
    session->mapIndex = ecs::PlayerRuntime::GetMapIndex(gm);
    session->weaponType.assign(weaponType);
    session->path = std::filesystem::path("benchmark_logs") /
        ("pvp_" + std::to_string(pid) + "_" + std::to_string(session->id) + ".csv");
    session->file.open(session->path, std::ios::out | std::ios::trunc);
    if (!session->file) {
        equipment::Restore(gm, profileMessage);
        message = "Cannot create CSV log."; return false;
    }
    session->file << "timestamp_ms,session_id,round,event,tier,defense_weapon,attacker_pid,attacker_name,attacker_job,attacker_spec,attacker_level,victim_pid,victim_name,victim_job,victim_spec,victim_level,weapon_vnum,skill_vnum,damage_type,critical,penetrate,hp_before,hp_after,actual_hp_loss,reason\n";
    message = "GM geared to level " + std::to_string(tier) +
              " (max STR/DEX/INT/VIT and PvP items). "
              "Target via /benchmark target <name> gets level " + std::to_string(tier) +
              " and maximum " + std::string(weaponType) +
              " protection. File: " + session->path.string();
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        active[pid] = std::move(session);
    }
    return true;
}

bool AddTarget(entt::entity gm, entt::entity target, std::string& message) {
    if (!Pid(gm) || target == entt::null || !ecs::PlayerRuntime::IsPC(target)) {
        message = "Target must be a real online PC."; return false;
    }
    const auto ownerPid = Pid(gm);
    const auto targetPid = ecs::PlayerRuntime::GetPlayerID(target);
    int tier = 0;
    std::string weapon;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = active.find(ownerPid);
        if (found == active.end()) { message = "Start benchmark first."; return false; }
        const Session& s = *found->second;
        if (ecs::PlayerRuntime::GetMapIndex(target) != s.mapIndex || target == gm) {
            message = "Target must be another PC in the same benchmark map."; return false;
        }
        if (s.targets.size() >= 8 && !s.targets.count(targetPid)) {
            message = "Maximum eight targets."; return false;
        }
        if (s.targets.count(targetPid)) {
            message = "Target already registered: " + std::string(ecs::PlayerRuntime::GetName(target));
            return true;
        }
        tier = s.tier;
        weapon = s.weaponType;
    }
    if (!equipment::Apply(target, tier, weapon, true, message)) {
        message = "Target setup failed: " + message;
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = active.find(ownerPid);
        if (found == active.end()) {
            std::string rollback;
            equipment::Restore(target, rollback);
            message = "Session was stopped during target setup: " + rollback;
            return false;
        }
        found->second->targets.insert(targetPid);
        found->second->targetEntities.emplace(targetPid, target);
    }
    message = "Target geared to level " + std::to_string(tier) + ", max base stats, P skills and " +
              weapon + " resistance: " + std::string(ecs::PlayerRuntime::GetName(target));
    return true;
}

bool Reset(entt::entity gm, std::string& message) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    const auto found = active.find(Pid(gm));
    if (found == active.end()) { message = "No running benchmark."; return false; }
    // Restore the real character health and mana for repeatable manual tests.
    const auto refill = [](entt::entity actor) {
        if (!ecs::PlayerRuntime::IsPC(actor)) return;
        ecs::PointSystem::Change(actor, POINT_HP,
            static_cast<int64_t>(ecs::PointSystem::GetMaxHP(actor)) -
            ecs::PointSystem::Get(actor, POINT_HP));
        ecs::PointSystem::Change(actor, POINT_SP,
            static_cast<int64_t>(ecs::PointSystem::GetMaxSP(actor)) -
            ecs::PointSystem::Get(actor, POINT_SP));
    };
    refill(gm);
    for (const auto& [pid, target] : found->second->targetEntities) refill(target);
    ++found->second->round;
    message = "New recording round: " + std::to_string(found->second->round) +
              ". GM and online targets HP/SP refilled.";
    return true;
}

bool Stop(entt::entity gm, std::string& message) {
    const uint32_t ownerPid = Pid(gm);
    if (!ownerPid) {
        message = "Benchmark stop requires a logged-in GM PC.";
        return false;
    }

    std::unique_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = active.find(ownerPid);
        if (found != active.end()) {
            session = std::move(found->second);
            active.erase(found);
        }
    }

    // A setup/rollback failure can leave an equipment snapshot in `applied`
    // after the telemetry session is gone. Previously /benchmark stop rejected
    // this state, permanently trapping the GM profile until process restart.
    // Retry the normal snapshot-based restoration, never clear the profile map
    // or destroy items by force: the originals must remain recoverable.
    if (!session) {
        if (!equipment::IsActive(gm)) {
            message = "No running benchmark and no GM profile to restore.";
            return false;
        }
        std::string restoreStatus;
        equipment::Restore(gm, restoreStatus);
        if (equipment::IsActive(gm)) {
            message = "Orphan GM profile restore INCOMPLETE: " + restoreStatus +
                      " Do not log out or restart; resolve the restore error and retry /benchmark stop.";
            return false;
        }
        message = "Orphan GM benchmark profile restored: " + restoreStatus +
                  " You may now use /benchmark start max <weapon>.";
        return true;
    }

    // Stop telemetry before restoring items. Restoration can publish ECS events.
    Session& s = *session;
    message = Summary(s);
    s.file.flush();
    s.file.close();
    bool pendingRestore = false;
    std::string restoreStatus;
    for (const auto& [targetPid, target] : s.targetEntities) {
        if (!ecs::PlayerRuntime::IsPC(target) ||
            ecs::PlayerRuntime::GetPlayerID(target) != targetPid) {
            pendingRestore = true;
            message += " | target " + std::to_string(targetPid) +
                       ": OFFLINE / entity changed; cannot restore safely.";
            continue;
        }
        if (equipment::IsActive(target)) {
            equipment::Restore(target, restoreStatus);
            message += " | target " + std::to_string(targetPid) + ": " + restoreStatus;
            if (equipment::IsActive(target)) pendingRestore = true;
        }
    }
    if (equipment::IsActive(gm)) {
        equipment::Restore(gm, restoreStatus);
        message += " | GM: " + restoreStatus;
        if (equipment::IsActive(gm)) pendingRestore = true;
    }

    if (pendingRestore) {
        // Do not discard the session's target handles while their original
        // equipment/points are still in the in-memory equipment snapshots.
        // A second /benchmark stop can then retry restoration.
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            active[ownerPid] = std::move(session);
        }
        message += " | RESTORE INCOMPLETE; session retained for retry. "
                   "Do not log out or restart. Fix the reported issue, then /benchmark stop again.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        finished[s.ownerPid] = {s.path.string(), s.hits, s.misses, s.damage};
    }
    return true;
}

std::string Report(entt::entity gm) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    const auto pid = Pid(gm);
    const auto current = active.find(pid);
    if (current != active.end()) return Summary(*current->second);
    const auto previous = finished.find(pid);
    if (previous == finished.end()) return "No benchmark results yet.";
    std::ostringstream o;
    o << "Last report: hits=" << previous->second.hits
      << " misses=" << previous->second.misses
      << " damage=" << previous->second.damage
      << " file=" << previous->second.path;
    return o.str();
}

void RecordDamage(entt::entity attacker, entt::entity victim,
                  int64_t hpBefore, int64_t hpAfter, uint8_t damageType,
                  bool critical, bool penetrate) noexcept {
    try {
        if (!test_server || attacker == entt::null || victim == entt::null ||
            !ecs::PlayerRuntime::IsPC(attacker) || !ecs::PlayerRuntime::IsPC(victim)) return;
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto it = active.find(ecs::PlayerRuntime::GetPlayerID(attacker));
        if (it == active.end() || !IsRecordedPair(*it->second, attacker, victim)) return;
        WriteEvent(*it->second, "HIT", attacker, victim, hpBefore, hpAfter,
                   damageType, critical, penetrate, "");
    } catch (...) { /* Telemetry must never interrupt combat. */ }
}

void RecordMiss(entt::entity attacker, entt::entity victim,
                uint8_t damageType, std::string_view reason) noexcept {
    try {
        if (!test_server || attacker == entt::null || victim == entt::null ||
            !ecs::PlayerRuntime::IsPC(attacker) || !ecs::PlayerRuntime::IsPC(victim)) return;
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto it = active.find(ecs::PlayerRuntime::GetPlayerID(attacker));
        if (it == active.end() || !IsRecordedPair(*it->second, attacker, victim)) return;
        const int64_t hp = static_cast<int64_t>(ecs::PlayerRuntime::GetHP(victim));
        WriteEvent(*it->second, "MISS", attacker, victim, hp, hp,
                   damageType, false, false, reason);
    } catch (...) { /* Never interfere with the actual GameServer. */ }
}

} // namespace nexoria::benchmark
