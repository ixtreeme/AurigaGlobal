#include "../../SRC/Server/GameServer/mission_pass/MissionPassState.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace AurigaMissionPass;
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("Check failed: " #condition); } while (false)

template<class F> void Rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}
int main(int argc, char** argv) {
    try {
        CHECK(argc == 2);
        std::ifstream file(argv[1]);
        CHECK(file.good());
        nlohmann::json doc; file >> doc;
        Season draft = ReadConfig(doc);
        CHECK(!draft.enabled && draft.missions.size() == 36 && draft.rewards.size()==80);
        CHECK(draft.endsAt - draft.startsAt == 42 * 86400);
        CHECK(draft.MaximumXP() == 40000);
        Season s = draft; s.enabled = true;
        State state; state.account = 7; state.season = s.id;
        const auto now = s.startsAt + 3600;
        Event e; e.account = 7; e.sequence = 1; e.occurredAt = now;
        e.kind = Kind::WoodGather; e.subject = 90011; e.amount = 15;
        CHECK(Apply(draft,state,e,now) == ApplyResult::DisabledOrExpired);
        CHECK(state.lastSequence == 0 && state.progress.empty());
        CHECK(Apply(s,state,e,now) == ApplyResult::Applied);
        CHECK(state.xp == 100);
        State empty;empty.account=7;empty.season=s.id;
        CHECK(NewCompletions(empty,state)==std::vector<ProgressKey>({{104,s.startsAt}}));
        CHECK(NewCompletions(state,state).empty()); // persisted retry / initial snapshot
        State wrong=empty;wrong.account=8;CHECK(NewCompletions(wrong,state).empty());
        wrong=empty;wrong.season++;CHECK(NewCompletions(wrong,state).empty());
        CHECK(Apply(s,state,e,now) == ApplyResult::Duplicate && state.xp == 100);
        e.sequence = 2;
        CHECK(Apply(s,state,e,now) == ApplyResult::Applied && state.xp == 100);
        CHECK(state.progress.at({307,s.startsAt}).value == 30);
        e.sequence = 3; e.amount = UINT32_MAX;
        CHECK(Apply(s,state,e,now) == ApplyResult::Applied && state.xp == 600);
        CHECK(state.progress.at({307,s.startsAt}).value == 500);

        // New UTC day: daily resets; weekly/season progress stays intact.
        e.sequence = 4; e.amount = 15; e.occurredAt = now + 86400;
        CHECK(Apply(s,state,e,e.occurredAt) == ApplyResult::Applied && state.xp == 700);
        CHECK(Period(s,Cadence::Weekly,now) == Period(s,Cadence::Weekly,now+86400));
        CHECK(Period(s,Cadence::Weekly,now+7*86400) - Period(s,Cadence::Weekly,now) == 7*86400);
        CHECK(Period(s,Cadence::Weekly,now-86400) == s.startsAt-7*86400);

        State herbs; herbs.account = 7; herbs.season = s.id;
        e.kind = Kind::HerbGather; e.amount = 1; e.occurredAt = now;
        for (uint64_t seq = 1; seq <= 10; ++seq) {
            e.sequence = seq; e.subject = 50701;
            CHECK(Apply(s,herbs,e,now) == ApplyResult::Applied);
        }
        CHECK(herbs.progress.at({105,s.startsAt}).value == 1 && herbs.xp == 0);
        for (uint32_t herb = 50702; herb <= 50712; ++herb) {
            e.sequence++; e.subject = herb;
            CHECK(Apply(s,herbs,e,now) == ApplyResult::Applied);
        }
        CHECK(herbs.xp == 950); // daily 100 + weekly 250 + full herbarium 600
        auto restored=LoadState(s,7,SaveState(herbs).dump());
        CHECK(restored.xp==herbs.xp && restored.lastSequence==herbs.lastSequence && restored.progress.size()==herbs.progress.size());
        CHECK(restored.progress.at({105,s.startsAt}).subjects.size()==3);
        Rejects([&]{LoadState(s,8,SaveState(herbs).dump());});

        State dungeons; dungeons.account = 7; dungeons.season = s.id;
        e.kind = Kind::DungeonComplete; e.sequence = 1; e.subject = 1; e.participants = 1;
        CHECK(Apply(s,dungeons,e,now) == ApplyResult::Applied);
        CHECK(dungeons.progress.at({204,s.startsAt}).value == 1);
        CHECK(dungeons.progress.count({205,s.startsAt}) == 0);
        e.sequence++; e.participants = 2;
        CHECK(Apply(s,dungeons,e,now) == ApplyResult::Applied);
        CHECK(dungeons.progress.at({205,s.startsAt}).value == 1);
        CHECK(dungeons.progress.count({206,s.startsAt}) == 0);
        e.sequence++; e.guildParty = true;
        CHECK(Apply(s,dungeons,e,now) == ApplyResult::Applied);
        CHECK(dungeons.progress.at({206,s.startsAt}).value == 1);
        e.sequence++;e.partyEligible=false;
        CHECK(Apply(s,dungeons,e,now)==ApplyResult::Applied);
        CHECK(dungeons.progress.at({206,s.startsAt}).value==1);
        e.partyEligible=true;

        State hunt; hunt.account = 7; hunt.season = s.id;
        e.kind = Kind::MobKill; e.sequence = 1; e.characterLevel = 100; e.targetLevel = 84;
        CHECK(Apply(s,hunt,e,now) == ApplyResult::Applied && hunt.progress.empty());
        e.sequence++; e.targetLevel = 85;
        CHECK(Apply(s,hunt,e,now) == ApplyResult::Applied && !hunt.progress.empty());
        e.sequence++; e.account = 8;
        CHECK(Apply(s,hunt,e,now) == ApplyResult::WrongAccount && hunt.lastSequence == 2);
        e.account = 7; e.occurredAt = now+1;
        CHECK(Apply(s,hunt,e,now) == ApplyResult::InvalidEvent);
        e.occurredAt = s.startsAt-1;
        CHECK(Apply(s,hunt,e,now) == ApplyResult::InvalidEvent);
        e.occurredAt = now;
        CHECK(Apply(s,hunt,e,s.endsAt) == ApplyResult::DisabledOrExpired);
        e.kind = Kind::ActiveMinute; e.amount = 30;
        CHECK(Apply(s,hunt,e,now) == ApplyResult::InvalidEvent);

        // Delayed event cannot complete the following day's daily quest.
        State delayed; delayed.account = 7; delayed.season = s.id;
        e.kind = Kind::WoodGather; e.amount = 15; e.subject = 90011; e.sequence = 1;
        CHECK(Apply(s,delayed,e,now+86400) == ApplyResult::Applied);
        CHECK(delayed.progress.count({104,s.startsAt+86400}) == 0);
        CHECK(delayed.progress.at({307,s.startsAt}).value == 15);
        CHECK(delayed.xp==100 && delayed.progress.at({104,s.startsAt}).xpAwarded);

        // Durable reloaded state retains dedup and XP saturation.
        State reloaded=state;
        e.sequence=reloaded.lastSequence; e.occurredAt=now;
        CHECK(Apply(s,reloaded,e,now)==ApplyResult::Duplicate);
        reloaded.xp=s.MaximumXP()-1;
        e.sequence++; e.kind=Kind::MetinKill; e.characterLevel=5; e.targetLevel=5; e.amount=200;
        CHECK(Apply(s,reloaded,e,now)==ApplyResult::Applied && reloaded.xp==s.MaximumXP());
        reloaded.season++;
        CHECK(Apply(s,reloaded,e,now)==ApplyResult::WrongAccount);

        s.rewards = {{1,Track::Free,90011,1},{1,Track::Premium,90011,2}};
        State claim; claim.account = 7; claim.season = s.id;
        CHECK(ReserveReward(s,claim,1,Track::Free,now) == ClaimResult::Locked);
        claim.xp = 1000;
        CHECK(ReserveReward(s,claim,1,Track::Free,now) == ClaimResult::Reserved);
        CHECK(ReserveReward(s,claim,1,Track::Free,now) == ClaimResult::AlreadyReserved);
        CHECK(ReserveReward(s,claim,1,Track::Premium,now) == ClaimResult::PremiumRequired);
        claim.premiumActivatedAt = now+1;
        CHECK(ReserveReward(s,claim,1,Track::Premium,now) == ClaimResult::PremiumRequired);
        claim.premiumActivatedAt = now;
        CHECK(ReserveReward(s,claim,1,Track::Premium,now) == ClaimResult::Reserved);
        CHECK(ReserveReward(s,claim,2,Track::Free,now) == ClaimResult::UnknownReward);
        CHECK(ReserveReward(s,claim,1,Track::Free,s.endsAt) == ClaimResult::Unavailable);
        claim.xp = UINT64_MAX;
        CHECK(Level(s,claim) == 40);

        auto bad = doc; bad["missions"][0]["goal"] = -1; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["missions"][0]["goal"] = 1.5; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["missions"][0]["goal"] = uint64_t(UINT32_MAX)+1; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["missions"][0]["event"] = "battlepass"; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["missions"][1]["id"] = bad["missions"][0]["id"]; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["ends_at_utc"] = bad["starts_at_utc"]; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["premium_purchase_enabled"] = true; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["missions"][4]["goal"] = 13; Rejects([&]{ReadConfig(bad);});
        bad = doc; bad["rewards"] = nlohmann::json::array({{{"level",1},{"track","free"},{"item",90011},{"count",1}},
            {{"level",1},{"track","free"},{"item",90011},{"count",1}}}); Rejects([&]{ReadConfig(bad);});
        std::cout << "MISSION_PASS_RULES=PASS missions=36 utc_resets=ok dedup=ok premium_isolated=ok claims=ok config_rejection=ok\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
