#include "../../SRC/Server/GameServer/mission/MissionActions.hpp"
#include "../../SRC/Server/GameServer/mission/MissionResourceFence.hpp"
#include "MissionResourceFixture.hpp"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <source_location>
#include <type_traits>

using namespace auriga::mission;
namespace f = resource_fixture;
namespace
{
size_t checks = 0;
void Check(bool value, const char *message, std::source_location line = std::source_location::current())
{
    ++checks;
    if (!value)
        throw std::runtime_error(std::string(message) + " at " + std::to_string(line.line()));
}
void Rehash(std::vector<uint8_t> &bytes)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[12 + i] = uint8_t(bytes.size() >> (8 * i));
    std::fill(bytes.begin() + 24, bytes.begin() + 56, 0);
    const auto hash = ContentHash(bytes);
    std::copy(hash.begin(), hash.end(), bytes.begin() + 24);
}
std::vector<uint8_t> Changed(const PreparedResources &plan, auto change)
{
    auto json = nlohmann::json::parse(plan.Bytes().begin() + 64, plan.Bytes().end());
    change(json);
    const auto text = json.dump();
    auto bytes = plan.Bytes();
    bytes.resize(64);
    bytes.insert(bytes.end(), text.begin(), text.end());
    Rehash(bytes);
    return bytes;
}
void BundleAndRecovery()
{
    const auto pin = f::Pin();
    const auto before = f::Snapshot();
    auto prepared = PrepareResources(pin, before, f::Rules(), f::Rolls());
    Check(bool(prepared), "mixed bundle failed");
    const auto &p = *prepared.plan;
    Check(p.Before() == before && before == f::Snapshot(), "source mutated");
    Check(p.After().revision == 78 && p.After().gold == 1100 && p.After().items.size() == 1 &&
              p.After().items.front().id == 11 && p.After().items.front().count == 2,
          "wrong cost/gold transition");
    Check(p.After().progress == ResourceProgress{10, 3, 900, 13, 5, 3, 45, 22, 0, 1, false} &&
              p.RefillVitals(),
          "level/stat/skill/random HP/SP progression differs");
    Check(p.Consumption() == std::vector<ResourceConsumption>{{10, 50010, 2, 2, 0}, {11, 50010, 3, 1, 2}},
          "cost selection differs");
    auto decoded = DecodeResources(p.Bytes(), pin);
    Check(bool(decoded) && decoded.plan->Bytes() == p.Bytes() && decoded.plan->After() == p.After(),
          "recovery recomputed a different decision");
    auto reordered = before;
    std::reverse(reordered.items.begin(), reordered.items.end());
    Check(PrepareResources(pin, reordered, f::Rules(), f::Rolls()).plan->Bytes() == p.Bytes(),
          "input order changed durable identity");
    Check(InspectResourceApplication(p, before) == ResourceApplication::Ready, "before image not ready");
    Check(InspectResourceApplication(p, p.After()) == ResourceApplication::Reconcile,
          "after image fabricated application proof");
    auto current = before;
    ++current.gold;
    Check(InspectResourceApplication(p, current) == ResourceApplication::Stale, "changed source accepted");
    current = p.After();
    current.revision += 3;
    current.gold += 9;
    ResourceEffectReceipt receipt{ActionIdentity(p.Action()), p.Digest(), p.After().revision};
    Check(InspectResourceApplication(p, current, receipt) == ResourceApplication::AlreadyApplied,
          "later state lost durable application proof");
    current.progress.level = 13; // A later configuration raised the level cap.
    Check(InspectResourceApplication(p, current, receipt) == ResourceApplication::AlreadyApplied,
          "old rules invalidated a historical effect receipt");
    auto inconsistent = p.After();
    ++inconsistent.gold;
    Check(InspectResourceApplication(p, inconsistent, receipt) == ResourceApplication::Invalid,
          "same resource revision with conflicting state accepted");
    receipt.decision[0] ^= 1;
    Check(InspectResourceApplication(p, current, receipt) == ResourceApplication::Invalid,
          "wrong receipt accepted");
    receipt = {ActionIdentity(p.Action()), p.Digest(), p.After().revision};
    Check(InspectResourceApplication(p, before, receipt) == ResourceApplication::Invalid,
          "receipt ahead of durable state accepted");
    current.character = 101;
    Check(InspectResourceApplication(p, current) == ResourceApplication::Invalid,
          "cross-character state accepted");
}
void Rejections()
{
    auto reject =
        [&](ResourceSnapshot before, ResourceRules rules, std::vector<ResourceLevelRoll> rolls = f::Rolls())
    {
        const auto copy = before;
        auto result = PrepareResources(f::Pin(), before, rules, rolls);
        Check(!result && !result.plan && !result.errors.empty() && before == copy,
              "partial/invalid plan escaped");
    };
    auto before = f::Snapshot();
    for (auto window : {ResourceWindow::Equipment, ResourceWindow::DragonSoul, ResourceWindow::Belt,
                        ResourceWindow::Safebox, ResourceWindow::Mall, ResourceWindow::Ground})
    {
        auto changed = before;
        changed.items[1].window = window;
        reject(changed, f::Rules());
    }
    for (uint8_t flag : {1, 2, 4, 8})
    {
        auto changed = before;
        changed.items[1].flags = flag;
        reject(changed, f::Rules());
    }
    auto changed = before;
    changed.items[0].count = 0;
    reject(changed, f::Rules());
    changed = before;
    changed.items[0].owner = 101;
    reject(changed, f::Rules());
    changed = before;
    changed.items[1].id = changed.items[0].id;
    reject(changed, f::Rules());
    changed = before;
    changed.items[1] = changed.items[0];
    ++changed.items[1].id;
    reject(changed, f::Rules());
    changed = before;
    changed.character = 101;
    reject(changed, f::Rules());
    changed = before;
    changed.revision = 0;
    reject(changed, f::Rules());
    changed = before;
    changed.revision = UINT64_MAX - 1;
    reject(changed, f::Rules());
    changed = before;
    changed.gold = -1;
    reject(changed, f::Rules());
    changed = before;
    changed.gold = f::Rules().goldExclusiveLimit - 100;
    reject(changed, f::Rules());
    changed = before;
    changed.progress.experience = 1000;
    reject(changed, f::Rules());
    changed = before;
    changed.progress.level = 0;
    reject(changed, f::Rules());
    changed = before;
    changed.progress.job = 1;
    reject(changed, f::Rules());
    changed = before;
    changed.progress.skill = INT16_MAX;
    reject(changed, f::Rules());
    changed = before;
    changed.progress.randomHP = INT16_MAX;
    reject(changed, f::Rules());
    changed = before;
    changed.progress.experienceBlocked = true;
    reject(changed, f::Rules());
    auto rules = f::Rules();
    rules.items.erase(50010);
    reject(before, rules);
    rules = f::Rules();
    rules.items[50011].maximumGrant = 1;
    reject(before, rules);
    rules = f::Rules();
    rules.items[50010].maximumConsume = 2;
    reject(before, rules);
    rules = f::Rules();
    rules.nextExperience[3] = 0;
    reject(before, rules);
    rules = f::Rules();
    rules.nextExperience.pop_back();
    reject(before, rules);
    rules = f::Rules();
    rules.version = 2;
    reject(before, rules);
    reject(before, f::Rules(), {});
    reject(before, f::Rules(), {{8, 5, 2}});
    reject(before, f::Rules(), {{9, 9, 2}});
    reject(before, f::Rules(), {{9, 5, 2}, {10, 5, 2}});
    changed = before;
    changed.progress.experienceBlocked = true;
    rules = f::Rules();
    rules.forceExperience = true;
    Check(bool(PrepareResources(f::Pin(), changed, rules, f::Rolls())), "explicit force XP rejected");
    auto bundle = f::Pin().Payload();
    bundle.items = {{50010, 3}};
    before.items.clear();
    Check(!PrepareResources(f::Pin(bundle), before, f::Rules(), f::Rolls()), "grants financed costs");
    bundle = RewardBundle{1, {}, 0, 1, {}};
    before = f::Snapshot();
    before.progress.level = 12;
    before.progress.experience = 0;
    Check(bool(PrepareResources(f::Pin(bundle), before, f::Rules())), "max-level non-XP bundle rejected");
    bundle.experience = 1;
    Check(!PrepareResources(f::Pin(bundle), before, f::Rules()), "max-level XP silently discarded");
    bundle.experience = UINT64_C(0x7fffffffffffffff);
    Check(!PrepareResources(f::Pin(bundle), f::Snapshot(), f::Rules(), {{9, 5, 2}, {10, 5, 2}, {11, 5, 2}}),
          "huge XP overflow/truncation accepted");
}
void Decoding()
{
    const auto pin = f::Pin();
    const auto p = PrepareResources(pin, f::Snapshot(), f::Rules(), f::Rolls());
    const auto &bytes = p.plan->Bytes();
    for (size_t size : {size_t(0), size_t(8), size_t(63), bytes.size() - 1})
        Check(!DecodeResources(std::span(bytes).first(size), pin), "truncated envelope accepted");
    for (size_t offset : {0u, 8u, 10u, 12u, 16u, 24u, 56u, 64u})
    {
        auto corrupt = bytes;
        corrupt[offset] ^= 1;
        Check(!DecodeResources(corrupt, pin), "corrupt envelope accepted");
    }
    for (const auto &field : {"action", "definition", "payload", "after", "consumption", "refill_vitals"})
    {
        auto corrupt = Changed(*p.plan, [&](auto &j) { j[field] = nullptr; });
        Check(!DecodeResources(corrupt, pin), "forged derived field with valid hash accepted");
    }
    Check(!DecodeResources(Changed(*p.plan, [](auto &j) { j["before"]["gold"] = -1; }), pin),
          "negative gold decoded");
    Check(!DecodeResources(Changed(*p.plan, [](auto &j) { j["before"]["revision"] = 1.5; }), pin),
          "fraction revision decoded");
    Check(!DecodeResources(Changed(*p.plan, [](auto &j) { j["rolls"][0][0] = 10; }), pin),
          "changed roll sequence decoded");
    Check(!DecodeResources(Changed(*p.plan, [](auto &j) { j["unknown"] = 1; }), pin),
          "unknown field decoded");
    Check(!DecodeResources(bytes, f::Pin(pin.Payload(), 101)), "cross-character pin decoded");
    auto changed = pin.Payload();
    ++changed.gold;
    Check(!DecodeResources(bytes, f::Pin(changed)), "different payload decoded");
    auto spaced = bytes;
    spaced.push_back(' ');
    Rehash(spaced);
    Check(!DecodeResources(spaced, pin), "noncanonical JSON decoded");
    auto oversized = bytes;
    oversized.resize(PreparedResources::MaximumBytes + 1, 0);
    Check(!DecodeResources(oversized, pin), "oversized blob decoded");
    for (size_t i = 0; i < 256; ++i)
    {
        auto corrupt = bytes;
        corrupt[64 + i % (bytes.size() - 64)] ^= uint8_t(1 + i % 255);
        Check(!DecodeResources(corrupt, pin), "changed content decoded");
    }
}
void ProgressionBoundaries()
{
    auto before = f::Snapshot();
    before.progress = {5, 0, 0, 343, 0, 0, 0, 0, 0, 1, false};
    auto rules = f::Rules();
    const auto pin = [](uint64_t xp) { return f::Pin(RewardBundle{1, {}, xp, 0, {}}); };
    for (uint32_t xp : {1, 249, 250, 499, 500, 749, 750, 999, 1000, 2000, 2999})
    {
        std::vector<ResourceLevelRoll> rolls;
        for (uint16_t i = 0; i < xp / 1000; ++i)
            rolls.push_back({uint16_t(5 + i), 5, 2});
        auto result = PrepareResources(pin(xp), before, rules, rolls);
        Check(bool(result), "boundary XP rejected");
        const auto &p = result.plan->After().progress;
        Check(p.level == 5 + xp / 1000 && p.experience == xp % 1000 && p.step == (xp % 1000) / 250 &&
                  p.stat <= 344 && p.skill == xp / 1000 && p.subSkill == 0,
              "quarter/level/skill/cap boundary differs");
    }
    before.progress.level = 11;
    before.progress.stat = 0;
    rules.lastStatLevel = 11;
    rules.lastStatStep = 2;
    auto result = PrepareResources(pin(750), before, rules);
    Check(bool(result) && result.plan->After().progress.stat == 2, "final stat-level gate differs");
    before.progress.step = 3;
    before.progress.experience = 10;
    result = PrepareResources(pin(300), before, rules);
    Check(bool(result) && result.plan->After().progress.stat == 0 && !result.plan->RefillVitals(),
          "lost XP reissued old quarter bonuses");
    std::mt19937 random(76371);
    for (size_t i = 0; i < 600; ++i)
    {
        const uint32_t next = 4 + random() % (UINT32_MAX - 4);
        const uint32_t xp = 1 + random() % (next - 1);
        before = f::Snapshot();
        before.progress = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
        rules = f::Rules();
        rules.nextExperience[1] = next;
        const auto q = uint32_t(next / 4.0f);
        const uint8_t expected = xp >= q * 3 ? 3 : xp >= q * 2 ? 2 : xp >= q ? 1 : 0;
        result = PrepareResources(pin(xp), before, rules);
        Check(bool(result) && result.plan->After().progress.step == expected &&
                  result.plan->After().progress.stat == expected,
              "float32 legacy quarter rounding changed");
        Check(bool(DecodeResources(result.plan->Bytes(), pin(xp))), "large XP recovery differs");
    }
}
} // namespace
static_assert(!std::is_default_constructible_v<PreparedResources>);
void FenceProtocol()
{
    namespace db = auriga::mission::persistence;
    db::Token token{};
    token[0] = 8;
    db::ResourceWriterIdentity identity{{100, 1, token}, 3, 7, token};
    auto prepared = PrepareResources(f::Pin(), f::Snapshot(), f::Rules(), f::Rolls());
    auto make = [&]() { return db::ResourceFence(identity, *prepared.plan, token, {11, 12}); };
    auto fence = make();
    Check(!fence.Submitted(token), "unfenced SQL submission accepted");
    auto stale = identity;
    ++stale.sessionGeneration;
    Check(!fence.SourceStopped(stale, token), "old/new mismatched session stopped producers");
    Check(!fence.LaneDrained(0, 12, SqlTransactionOutcome::Committed), "wrong lane barrier accepted");
    Check(fence.LaneDrained(1, 12, SqlTransactionOutcome::Committed), "second drain lane rejected");
    Check(fence.SourceStopped(identity, token) && fence.Stage() == db::ResourceFenceStage::Draining,
          "one lane was sufficient");
    Check(fence.LaneDrained(0, 11, SqlTransactionOutcome::Committed) &&
              fence.Stage() == db::ResourceFenceStage::Ready,
          "two-lane freeze failed");
    Check(fence.Submitted(token) && !fence.Submitted(token), "duplicate submission allowed");
    Check(!fence.Complete(stale, token, SqlTransactionOutcome::Committed), "stale callback accepted");
    Check(fence.Complete(identity, token, SqlTransactionOutcome::CommitUnknown) && !fence.Submitted(token),
          "unknown commit resumed/retried");
    Check(!fence.Reconciled(identity, token, db::Resolution::SqlError) &&
              fence.Stage() == db::ResourceFenceStage::Reconciling,
          "SQL error thawed source");
    Check(fence.Reconciled(identity, token, db::Resolution::Absent) &&
              fence.Stage() == db::ResourceFenceStage::Ready,
          "confirmed absence did not permit retry");
    Check(fence.Submitted(token) && fence.Complete(identity, token, SqlTransactionOutcome::CommitUnknown),
          "retry submit failed");
    auto receipt =
        ResourceEffectReceipt{ActionIdentity(prepared.plan->Action()), prepared.plan->Digest(), 78};
    Check(fence.Reconciled(identity, token, db::Resolution::Committed, receipt),
          "durable effect receipt rejected");
    Check(!fence.Published(stale, prepared.plan->Digest(), 78), "stale session published effects");
    Check(!fence.Published(identity, prepared.plan->Digest(), 78), "commit alone published effects");
    Check(!fence.Rebased(stale, prepared.plan->Digest(), 78, {777, 444, 100}),
          "stale session rebased effects");
    Check(fence.Rebased(identity, prepared.plan->Digest(), 78, {777, 444, 100}),
          "confirmed postimage rebase failed");
    Check(fence.Published(identity, prepared.plan->Digest(), 78) &&
              !fence.Published(identity, prepared.plan->Digest(), 78),
          "publication is not single-use");
    fence = make();
    fence.Invalidate();
    Check(!fence.SourceStopped(identity, token) &&
              !fence.LaneDrained(0, 11, SqlTransactionOutcome::Committed),
          "invalidated fence revived");
    fence = make();
    Check(!fence.LaneDrained(0, 11, SqlTransactionOutcome::CommitUnknown) &&
              fence.Stage() == db::ResourceFenceStage::Blocked,
          "unknown drain barrier accepted");
    for (size_t order = 0; order < 6; ++order)
    {
        fence = make();
        std::array<size_t, 3> actions{0, 1, 2};
        for (size_t i = 0; i < order; ++i)
            std::next_permutation(actions.begin(), actions.end());
        for (auto action : actions)
        {
            if (action == 2)
                Check(fence.SourceStopped(identity, token), "source stop order rejected");
            else
                Check(fence.LaneDrained(action, 11 + action, SqlTransactionOutcome::Committed),
                      "lane completion order rejected");
        }
        Check(fence.Stage() == db::ResourceFenceStage::Ready, "barrier order changed readiness");
    }
}
int main()
{
    try
    {
        BundleAndRecovery();
        Rejections();
        Decoding();
        ProgressionBoundaries();
        FenceProtocol();
        std::cout << "Mission resources: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
