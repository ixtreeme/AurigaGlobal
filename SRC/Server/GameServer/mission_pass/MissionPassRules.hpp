#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Independent domain model. No Lua, ECS, client packets, SQL or legacy battle pass.
// Callers must serialize events per account and atomically persist the returned
// state with its sequence BEFORE acknowledging it. This header never grants items.
namespace AurigaMissionPass {
enum class Cadence { Daily, Weekly, Season };
enum class Kind { MobKill, BossKill, MetinKill, DungeonComplete, WoodGather,
    TreeFelled, HerbGather, OreGather, FishCatch, ToolTraining, ToolUpgrade,
    Explore, ActiveMinute, GuildExp };
enum class Counting { Quantity, UniqueSubject };
enum class Group { Any, Solo, Party, GuildParty };
enum class Track { Free, Premium };

struct Mission {
    uint32_t id = 0;
    std::string nameKey, descriptionKey;
    Cadence cadence = Cadence::Daily;
    Kind kind = Kind::MobKill;
    Counting counting = Counting::Quantity;
    Group group = Group::Any;
    uint32_t goal = 1, xp = 100, minimumLevel = 5;
    // Combat events only: reject trivial targets, not players/resources.
    uint32_t maximumLevelGap = 15;
    std::set<uint32_t> subjects;
};
struct Reward {
    uint32_t level = 0;
    Track track = Track::Free;
    uint32_t item = 0, count = 0;
};
struct Season {
    uint32_t id = 0, levels = 40, xpPerLevel = 1000;
    int64_t startsAt = 0, endsAt = 0;
    bool enabled = false;
    std::string titleKey;
    std::vector<Mission> missions;
    std::vector<Reward> rewards;
    uint64_t MaximumXP() const { return uint64_t(levels) * xpPerLevel; }
    bool Active(int64_t utc) const { return enabled && utc >= startsAt && utc < endsAt; }
};
struct Event {
    uint32_t account = 0;
    uint64_t sequence = 0; // Durable, strictly ordered ACCOUNT event stream, not a client value.
    int64_t occurredAt = 0;
    Kind kind = Kind::MobKill;
    uint32_t subject = 0, amount = 1, characterLevel = 5, targetLevel = 5;
    uint32_t participants = 1;
    bool guildParty = false;
    bool partyEligible = true;
};
struct Progress {
    uint32_t value = 0;
    bool xpAwarded = false;
    std::set<uint32_t> subjects;
};
using ProgressKey = std::pair<uint32_t, int64_t>; // Mission ID + UTC reset boundary.
using RewardKey = std::pair<uint32_t, Track>;
struct State {
    uint32_t account = 0, season = 0;
    uint64_t xp = 0, lastSequence = 0;
    int64_t premiumActivatedAt = 0;
    std::map<ProgressKey, Progress> progress;
    // Durable outbox reservation, NOT proof that an inventory item was delivered.
    std::set<RewardKey> reservedRewards;
};

// Called only around a confirmed durable commit. Snapshots/login never announce
// historical completions. The reset boundary also identifies repeatable dailies.
inline std::vector<ProgressKey> NewCompletions(const State& before,const State& after) {
    std::vector<ProgressKey> result;
    if(before.account!=after.account || before.season!=after.season)return result;
    for(const auto& [key,p]:after.progress)if(p.xpAwarded) {
        const auto previous=before.progress.find(key);
        if(previous==before.progress.end() || !previous->second.xpAwarded)result.push_back(key);
    }
    return result;
}

inline void Validate(const Season& season) {
    if (!season.id || season.startsAt < 0 || season.endsAt <= season.startsAt ||
        !season.levels || season.levels > 100 || !season.xpPerLevel ||
        season.titleKey.empty() || season.missions.empty() || season.missions.size() > 256)
        throw std::invalid_argument("Invalid Mission Pass season");
    std::set<uint32_t> ids;
    for (const auto& m : season.missions) {
        if (!m.id || !ids.insert(m.id).second || !m.goal || !m.xp ||
            m.nameKey.empty() || m.descriptionKey.empty() || m.subjects.size() > 256 ||
            (m.counting == Counting::UniqueSubject && (m.goal > 256 ||
                (!m.subjects.empty() && m.goal > m.subjects.size()))))
            throw std::invalid_argument("Invalid or duplicate Mission Pass mission");
    }
    std::set<RewardKey> slots;
    for (const auto& r : season.rewards)
        if (!r.level || r.level > season.levels || !r.item || !r.count || r.count > 200 ||
            !slots.emplace(r.level, r.track).second)
            throw std::invalid_argument("Invalid or duplicate Mission Pass reward");
}

inline int64_t Period(const Season& season, Cadence cadence, int64_t utc) {
    if (cadence == Cadence::Season) return season.startsAt;
    const int64_t day = utc / 86400;
    if (cadence == Cadence::Daily) return day * 86400;
    // 1970-01-01 was Thursday; Monday 00:00 UTC is the weekly boundary.
    return (day - (day + 3) % 7) * 86400;
}
inline uint32_t Level(const Season& season, const State& state) {
    return season.xpPerLevel ? static_cast<uint32_t>(std::min<uint64_t>(
        season.levels, state.xp / season.xpPerLevel)) : 0;
}
inline bool Combat(Kind kind) {
    return kind == Kind::MobKill || kind == Kind::BossKill || kind == Kind::MetinKill;
}
inline bool Matches(const Mission& m, const Event& e) {
    if (m.kind != e.kind || e.characterLevel < m.minimumLevel || !e.amount ||
        (!m.subjects.empty() && !m.subjects.count(e.subject))) return false;
    if (Combat(e.kind) && (!e.targetLevel ||
        uint64_t(e.targetLevel) + m.maximumLevelGap < e.characterLevel)) return false;
    if ((m.group == Group::Solo && e.participants != 1) ||
        ((m.group == Group::Party || m.group == Group::GuildParty) && (e.participants < 2 || !e.partyEligible)) ||
        (m.group == Group::GuildParty && !e.guildParty)) return false;
    return m.counting != Counting::UniqueSubject || e.subject != 0;
}

enum class ApplyResult { Applied, DisabledOrExpired, WrongAccount, Duplicate, InvalidEvent };
inline ApplyResult Apply(const Season& season, State& state, const Event& event, int64_t now) {
    if (!season.Active(now)) return ApplyResult::DisabledOrExpired;
    if (!state.account || state.account != event.account || state.season != season.id)
        return ApplyResult::WrongAccount;
    if (!event.sequence || event.occurredAt < season.startsAt || event.occurredAt > now ||
        event.occurredAt >= season.endsAt || !event.participants || !event.amount ||
        (event.kind == Kind::ActiveMinute && event.amount != 1)) return ApplyResult::InvalidEvent;
    if (event.sequence <= state.lastSequence) return ApplyResult::Duplicate;
    // Keep current and immediately previous buckets so a batched strike at
    // 23:59:59 can still finish yesterday's objective after midnight.
    for(auto it=state.progress.begin();it!=state.progress.end();) {
        auto m=std::find_if(season.missions.begin(),season.missions.end(),[&](const Mission& x){return x.id==it->first.first;});
        const int64_t width=m==season.missions.end()?0:m->cadence==Cadence::Daily?86400:m->cadence==Cadence::Weekly?7*86400:0;
        const int64_t current=m==season.missions.end()?0:Period(season,m->cadence,now);
        if(m==season.missions.end() || (it->first.second!=current && (!width || it->first.second!=current-width)))it=state.progress.erase(it);
        else ++it;
    }
    for (const auto& m : season.missions) {
        // Delayed events must never award a new day's or week's progress.
        const auto current=Period(season,m.cadence,now);
        const auto period=Period(season,m.cadence,event.occurredAt);
        const int64_t width=m.cadence==Cadence::Daily?86400:m.cadence==Cadence::Weekly?7*86400:0;
        if ((period!=current && (!width || period!=current-width)) || !Matches(m,event))continue;
        auto& p = state.progress[{m.id, period}];
        if (p.xpAwarded) continue;
        if (m.counting == Counting::UniqueSubject) {
            if (p.subjects.size() < m.goal) p.subjects.insert(event.subject);
            p.value = static_cast<uint32_t>(p.subjects.size());
        } else p.value = static_cast<uint32_t>(std::min<uint64_t>(m.goal, uint64_t(p.value) + event.amount));
        if (p.value >= m.goal) {
            p.xpAwarded = true;
            state.xp = std::min(season.MaximumXP(),
                std::min(state.xp, season.MaximumXP()) + m.xp);
        }
    }
    state.lastSequence = event.sequence;
    return ApplyResult::Applied;
}

enum class ClaimResult { Reserved, Unavailable, Locked, PremiumRequired, AlreadyReserved, UnknownReward };
inline ClaimResult ReserveReward(const Season& season, State& state, uint32_t level,
    Track track, int64_t now) {
    if (!season.Active(now) || state.season != season.id || !state.account) return ClaimResult::Unavailable;
    const auto found = std::find_if(season.rewards.begin(), season.rewards.end(),
        [&](const Reward& r) { return r.level == level && r.track == track; });
    if (found == season.rewards.end()) return ClaimResult::UnknownReward;
    if (Level(season, state) < level) return ClaimResult::Locked;
    if (track == Track::Premium && (state.premiumActivatedAt < season.startsAt ||
        state.premiumActivatedAt > now || state.premiumActivatedAt == 0)) return ClaimResult::PremiumRequired;
    return state.reservedRewards.emplace(level, track).second ? ClaimResult::Reserved : ClaimResult::AlreadyReserved;
}
}
