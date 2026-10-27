#include "ItemCatalogFixture.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace
{
std::size_t checks = 0;
void Check(bool condition, const char* message, const std::source_location location = std::source_location::current())
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message) + " at line " + std::to_string(location.line()));
}
template <typename Mutate> void Reject(Mutate mutate, AccessorySocketError error)
{
    auto source = item_catalog_fixture::SocketProfile(); mutate(source);
    const auto result = ValidateAccessorySocketProfile(std::move(source));
    Check(!result && result.error() == error, "malformed socket profile rejected");
}
void Profiles()
{
    auto source = item_catalog_fixture::SocketProfile();
    std::ranges::reverse(source.grades);
    const auto profile = ValidateAccessorySocketProfile(source).value();
    Check(profile.FindGrade(1)->durationMs == 86400000 && profile.FindGrade(3)->bonusIncreaseBp == 4000, "grade rows normalize and retain explicit units");
    source.grades.clear();
    Check(profile.FindGrade(3) != nullptr && !profile.FindGrade(4), "profile owns data and never clamps unknown grade");
    Reject([](auto& p) { p.id = 0; }, AccessorySocketError::InvalidIdentity);
    Reject([](auto& p) { p.code = "unsafe+9"; }, AccessorySocketError::InvalidIdentity);
    Reject([](auto& p) { p.maxGrade = 0; }, AccessorySocketError::InvalidMaximum);
    Reject([](auto& p) { p.unlockUnitsPerAttempt = 0; }, AccessorySocketError::InvalidUnits);
    Reject([](auto& p) { p.chargeUnitsPerAttempt = 0; }, AccessorySocketError::InvalidUnits);
    Reject([](auto& p) { p.grades.pop_back(); }, AccessorySocketError::IncompleteGrades);
    Reject([](auto& p) { p.grades[1].grade = 0; }, AccessorySocketError::InvalidGrade);
    Reject([](auto& p) { p.grades[3].grade = 4; }, AccessorySocketError::InvalidGrade);
    Reject([](auto& p) { p.grades[1].durationMs = 0; }, AccessorySocketError::InvalidTimer);
    Reject([](auto& p) { p.grades[1].durationMs = UINT64_MAX; }, AccessorySocketError::InvalidTimer);
    Reject([](auto& p) { p.grades[0].durationMs = 1; }, AccessorySocketError::NonzeroGradeZero);
    Reject([](auto& p) { p.grades[0].minimumBp = 1; }, AccessorySocketError::NonzeroGradeZero);
    Reject([](auto& p) { p.grades[1].minimumRaw = -1; }, AccessorySocketError::InvalidBonus);
    Reject([](auto& p) { p.grades[1].minimumRaw = MaxItemStatRaw + 1; }, AccessorySocketError::InvalidBonus);
    Reject([](auto& p) { p.grades[1].minimumBp = -1; }, AccessorySocketError::InvalidBonus);
    Reject([](auto& p) { p.grades[1].minimumInteger = 100001; }, AccessorySocketError::InvalidBonus);
    Reject([](auto& p) { p.grades[1].bonusIncreaseBp = 100001; }, AccessorySocketError::InvalidBonus);
    Reject([](auto& p) { p.grades[2].bonusIncreaseBp = 0; }, AccessorySocketError::NonMonotonicBonus);
    Reject([](auto& p) { p.grades[2].minimumRaw = 0; }, AccessorySocketError::NonMonotonicBonus);
    Reject([](auto& p) { p.grades[0].unlockChanceBp = 10001; }, AccessorySocketError::InvalidChance);
    Reject([](auto& p) { p.grades[3].chargeChanceBp = 1; }, AccessorySocketError::InvalidChance);
    Reject([](auto& p) { p.unlockMaterials.clear(); }, AccessorySocketError::InvalidCompatibility);
    Reject([](auto& p) { p.chargeMaterials = {item_catalog_fixture::Sword()}; }, AccessorySocketError::InvalidCompatibility);
    Reject([](auto& p) { p.unlockMaterials.push_back(p.unlockMaterials.front()); }, AccessorySocketError::DuplicateMaterial);
    Reject([](auto& p) { const auto material = p.chargeMaterials.front(); p.chargeMaterials.assign(MaxAccessorySocketMaterials + 1, material); }, AccessorySocketError::TooManyMaterials);
    Reject([](auto& p) { p.permanentChargeMaterials = p.chargeMaterials; }, AccessorySocketError::InvalidCompatibility);
    Reject([](auto& p) { p.permanentChargeMaterials = {item_catalog_fixture::Sword()}; }, AccessorySocketError::InvalidCompatibility);
    Reject([](auto& p) { p.permanentChargeMaterials = {item_catalog_fixture::Leather(), item_catalog_fixture::Leather()}; }, AccessorySocketError::DuplicateMaterial);
    Reject([](auto& p) { p.permanentChargeMaterials.assign(MaxAccessorySocketMaterials + 1, item_catalog_fixture::Leather()); }, AccessorySocketError::TooManyMaterials);
    auto bounded = item_catalog_fixture::SocketProfile();
    bounded.grades[3].durationMs = INT64_MAX;
    Check(ValidateAccessorySocketProfile(std::move(bounded)).has_value(), "full allowed duration bound retained");
}
void PlansAndAdmission()
{
    const auto profile = ValidateAccessorySocketProfile(item_catalog_fixture::SocketProfile()).value();
    const auto drill = item_catalog_fixture::SocketDrill(), jewel = item_catalog_fixture::SocketJewel();
    const AccessorySocketState empty;
    const auto failed = PlanAccessorySocketUnlock(profile, empty, drill, SocketAttemptOutcome::Failed).value();
    Check(failed.state == empty && failed.materialUnits == 1 && !failed.succeeded, "failed valid attempt proposes material cost without increasing capacity");
    auto state = PlanAccessorySocketUnlock(profile, empty, drill, SocketAttemptOutcome::Succeeded)->state;
    Check(state == AccessorySocketState{1,0,0} && empty == AccessorySocketState{}, "unlock proposes capacity only and leaves input untouched");
    Check(PlanAccessorySocketUnlock(profile, state, jewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::MaterialNotAllowed, "unlock requires an explicitly compatible material");
    Check(PlanAccessorySocketCharge(profile, state, drill, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::MaterialNotAllowed, "charge has a different compatibility list");
    Check(PlanAccessorySocketCharge(profile, empty, jewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::NoOpenCapacity, "charging cannot unlock capacity implicitly");
    Check(PlanAccessorySocketCharge(profile, state, jewel, SocketAttemptOutcome::Failed).error() == AccessorySocketError::InvalidDecision, "guaranteed success cannot accept a failed recorded outcome");
    Check(PlanAccessorySocketUnlock(profile, state, drill, static_cast<SocketAttemptOutcome>(0)).error() == AccessorySocketError::InvalidDecision, "unknown economic outcome rejected");
    auto charged = PlanAccessorySocketCharge(profile, state, jewel, SocketAttemptOutcome::Succeeded).value();
    Check(charged.state == AccessorySocketState{1,1,86400000} && charged.materialUnits == 1 && charged.succeeded, "first charge activates a grade and its own lifetime");
    Check(PlanAccessorySocketCharge(profile, charged.state, jewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::NoOpenCapacity, "charge cannot exceed unlocked capacity");
    state = PlanAccessorySocketUnlock(profile, charged.state, drill, SocketAttemptOutcome::Succeeded)->state;
    state = PlanAccessorySocketUnlock(profile, state, drill, SocketAttemptOutcome::Succeeded)->state;
    Check(state == AccessorySocketState{3,1,86400000}, "capacity unlock preserves active grade and remaining time");
    Check(PlanAccessorySocketUnlock(profile, state, drill, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::CapacityFull, "profile caps capacity");
    state.remainingMs = 1;
    state = PlanAccessorySocketCharge(profile, state, jewel, SocketAttemptOutcome::Succeeded)->state;
    Check(state == AccessorySocketState{3,2,43200000}, "higher charge resets to that grade's full duration");
    state = PlanAccessorySocketCharge(profile, state, jewel, SocketAttemptOutcome::Succeeded)->state;
    Check(state == AccessorySocketState{3,3,21600000}, "last configured grade reached");
    auto adjustable = item_catalog_fixture::SocketProfile();
    adjustable.grades[0].chargeChanceBp = 5000;
    adjustable.chargeUnitsPerAttempt = 2;
    const auto half = ValidateAccessorySocketProfile(adjustable).value();
    const auto noCharge = PlanAccessorySocketCharge(half, {1,0,0}, jewel, SocketAttemptOutcome::Failed).value();
    Check(noCharge.state == AccessorySocketState{1,0,0} && noCharge.materialUnits == 2 && !noCharge.succeeded, "non-guaranteed charge uses recorded outcome and explicit attempt cost");
    adjustable.grades[1].chargeChanceBp = 5000;
    const auto partial = ValidateAccessorySocketProfile(adjustable).value();
    const auto keptTime = PlanAccessorySocketCharge(partial, {3,1,123}, jewel, SocketAttemptOutcome::Failed).value();
    Check(keptTime.state == AccessorySocketState{3,1,123} && keptTime.materialUnits == 2, "failed charge cannot refresh or discard existing remaining time");
    adjustable.grades[0].chargeChanceBp = 0;
    const auto disabled = ValidateAccessorySocketProfile(std::move(adjustable)).value();
    Check(PlanAccessorySocketCharge(disabled, {1,0,0}, jewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::OperationDisabled, "disabled operation cannot spend material or invent success");
}
void EquippedTime()
{
    const auto profile = ValidateAccessorySocketProfile(item_catalog_fixture::SocketProfile()).value();
    const AccessorySocketState full {3,3,21600000};
    Check(AdvanceAccessorySocketTime(profile, full, UINT64_MAX, false)->state == full, "unequipped time is paused even for huge elapsed interval");
    Check(AdvanceAccessorySocketTime(profile, full, 0, true)->state == full, "zero elapsed is a no-op");
    const auto shortTick = AdvanceAccessorySocketTime(profile, full, 1000, true).value();
    Check(shortTick.state == AccessorySocketState{3,3,21599000} && !shortTick.gradesExpired, "equipped time subtracts integer milliseconds");
    const auto first = AdvanceAccessorySocketTime(profile, full, 21600000, true).value();
    Check(first.state == AccessorySocketState{3,2,43200000} && first.gradesExpired == 1, "exact boundary degrades to previous grade and resets its duration");
    const auto two = AdvanceAccessorySocketTime(profile, full, 21600000 + 43200000, true).value();
    Check(two.state == AccessorySocketState{3,1,86400000} && two.gradesExpired == 2, "late tick processes every expired grade");
    const auto three = AdvanceAccessorySocketTime(profile, full, 21600000 + 43200000 + 86400000, true).value();
    Check(three.state == AccessorySocketState{3,0,0} && three.gradesExpired == 3, "expiration clears grade but preserves permanent capacity");
    Check(AdvanceAccessorySocketTime(profile, full, UINT64_MAX, true)->state == three.state, "huge elapsed time saturates through lawful grade transitions, without arithmetic overflow");
    Check(AdvanceAccessorySocketTime(profile, full, 21600123, true)->state == AccessorySocketState{3,2,43199877}, "unused elapsed time carries into lower grade");
    auto sequential = AdvanceAccessorySocketTime(profile, shortTick.state, 21600000, true).value();
    auto combined = AdvanceAccessorySocketTime(profile, full, 21601000, true).value();
    Check(sequential.state == combined.state, "tick partition does not change remaining grade/time");
    for (std::uint64_t split = 0; split <= 100; ++split)
    {
        const std::uint64_t total = 200000000;
        const auto a = split * total / 100;
        const auto firstPart = AdvanceAccessorySocketTime(profile, full, a, true).value();
        Check(AdvanceAccessorySocketTime(profile, firstPart.state, total - a, true)->state ==
              AdvanceAccessorySocketTime(profile, full, total, true)->state,
              "partitioned elapsed intervals preserve all grade transitions");
    }
    auto maximum = item_catalog_fixture::SocketProfile();
    maximum.maxGrade = 255; maximum.grades.clear();
    for (std::uint16_t grade = 0; grade <= 255; ++grade)
        maximum.grades.push_back({static_cast<std::uint8_t>(grade), grade ? 1u : 0u, 0, 0, 0, 0,
            static_cast<std::uint16_t>(grade < 255 ? 10000 : 0), static_cast<std::uint16_t>(grade < 255 ? 10000 : 0)});
    const auto edge = ValidateAccessorySocketProfile(std::move(maximum)).value();
    const auto expired = AdvanceAccessorySocketTime(edge, {255,255,1}, UINT64_MAX, true).value();
    Check(expired.state == AccessorySocketState{255,0,0} && expired.gradesExpired == 255, "all 255 grades expire with a bounded loop");
}
void InvalidStateAndCatalogOwnership()
{
    auto profile = ValidateAccessorySocketProfile(item_catalog_fixture::SocketProfile()).value();
    for (auto state : {AccessorySocketState{4,0,0}, {1,2,1000}, {3,4,1}, {1,0,1}, {1,1,0}, {1,1,86400001}, {1,1,UINT64_MAX}})
    {
        Check(!ValidateAccessorySocketState(profile, state), "malformed concrete state rejected");
        Check(AdvanceAccessorySocketTime(profile, state, 1, false).error() == AccessorySocketError::InvalidState, "paused timer does not bypass state admission");
        Check(PlanAccessorySocketUnlock(profile, state, item_catalog_fixture::SocketDrill(), SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::InvalidState, "unlock cannot repair malformed state by clamping");
        Check(PlanAccessorySocketCharge(profile, state, item_catalog_fixture::SocketJewel(), SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::InvalidState, "charge cannot repair malformed state by clamping");
    }
    const auto moved = std::move(profile);
    Check(moved.FindGrade(3) != nullptr, "moved-to profile retains all levels");
    if (!profile.FindGrade(0)) Check(!ValidateAccessorySocketState(profile, {}), "moved-from profile cannot authorize a state");
    auto source = item_catalog_fixture::Catalog();
    const auto catalog = BuildItemCatalog(source).value();
    const auto* sockets = catalog->FindAccessorySocket(*std::get<ItemAccessoryRow>(catalog->Find(item_catalog_fixture::Bracelet())->category).socketProfile);
    source.accessorySockets[0].grades.clear();
    Check(sockets->FindGrade(3)->durationMs == 21600000, "catalog owns socket rows independently of caller");
    const auto hash = catalog->ContentHash();
    const std::uint16_t normalRefinement = 15;
    const AccessorySocketState first {3,3,21600000}, second {1,1,86400000};
    const auto result = AdvanceAccessorySocketTime(*sockets, first, 21600000, true).value();
    Check(result.state.activeGrade == 2 && second.activeGrade == 1 && normalRefinement == 15, "socket grade expiration is independent of another copy and normal refinement");
    Check(catalog->ContentHash() == hash, "instance socket changes cannot rewrite prototype content");
}
void PermanentCharges()
{
    // Profile validation checks structural keys; the catalog additionally checks
    // the referenced material row and requires Jewel kind for both charge modes.
    const auto permanentJewel = item_catalog_fixture::Leather();
    const auto timedJewel = item_catalog_fixture::SocketJewel();
    auto source = item_catalog_fixture::SocketProfile();
    source.permanentChargeMaterials = {permanentJewel};
    const auto profile = ValidateAccessorySocketProfile(source).value();
    const AccessorySocketState empty {2,0,0};
    const auto first = PlanAccessorySocketCharge(profile, empty, permanentJewel, SocketAttemptOutcome::Succeeded).value();
    Check(first.state == AccessorySocketState{2,1,0,true} && first.materialUnits == 1 && first.succeeded,
          "permanent charge uses an explicit lifetime flag without a duration sentinel");
    const auto second = PlanAccessorySocketCharge(profile, first.state, permanentJewel, SocketAttemptOutcome::Succeeded).value();
    Check(second.state == AccessorySocketState{2,2,0,true}, "permanent charges advance independently of timed duration");
    const auto unlocked = PlanAccessorySocketUnlock(profile, second.state, item_catalog_fixture::SocketDrill(), SocketAttemptOutcome::Succeeded).value();
    Check(unlocked.state == AccessorySocketState{3,2,0,true}, "capacity unlock preserves permanent grade and lifetime");
    for (bool equipped : {false,true})
    {
        const auto elapsed = AdvanceAccessorySocketTime(profile, unlocked.state, UINT64_MAX, equipped).value();
        Check(elapsed.state == unlocked.state && !elapsed.gradesExpired, "permanent grade never expires even across the largest elapsed interval");
    }
    Check(PlanAccessorySocketCharge(profile, unlocked.state, timedJewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::LifetimeMismatch,
          "timed material cannot overwrite active permanent charges");
    Check(PlanAccessorySocketCharge(profile, {3,1,123}, permanentJewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::LifetimeMismatch,
          "permanent material cannot overwrite an active timed charge");
    const auto full = PlanAccessorySocketCharge(profile, unlocked.state, permanentJewel, SocketAttemptOutcome::Succeeded).value();
    Check(full.state == AccessorySocketState{3,3,0,true}, "permanent charge respects the configured grade maximum");
    Check(PlanAccessorySocketCharge(profile, full.state, permanentJewel, SocketAttemptOutcome::Succeeded).error() == AccessorySocketError::NoOpenCapacity,
          "permanent charges cannot exceed unlocked capacity");
    for (auto invalid : {AccessorySocketState{1,0,0,true}, {1,1,1,true}, {0,1,0,true}})
        Check(!ValidateAccessorySocketState(profile, invalid), "permanent states require a nonzero active grade and zero remaining time");
    const auto timedOnly = ValidateAccessorySocketProfile(item_catalog_fixture::SocketProfile()).value();
    Check(!ValidateAccessorySocketState(timedOnly, first.state), "a profile without permanent capability cannot accept permanent state");
    source.grades[0].chargeChanceBp = 5000;
    source.chargeUnitsPerAttempt = 2;
    const auto probabilistic = ValidateAccessorySocketProfile(source).value();
    const auto failed = PlanAccessorySocketCharge(probabilistic, empty, permanentJewel, SocketAttemptOutcome::Failed).value();
    Check(failed.state == empty && failed.materialUnits == 2 && !failed.succeeded, "failed permanent charge proposes cost without granting permanent state");
    source.grades[1].chargeChanceBp = 5000;
    const auto later = ValidateAccessorySocketProfile(source).value();
    Check(PlanAccessorySocketCharge(later, first.state, permanentJewel, SocketAttemptOutcome::Failed)->state == first.state,
          "failed subsequent permanent charge preserves the active permanent state");
    Check(AdvanceAccessorySocketTime(profile, {3,1,86410}, 10, true)->state == AccessorySocketState{3,1,86400},
          "ordinary duration values never acquire a magic permanent meaning");
}
}
int main()
{
    try { Profiles(); PlansAndAdmission(); EquippedTime(); InvalidStateAndCatalogOwnership(); PermanentCharges(); std::cout << "Accessory socket checks passed: " << checks << '\n'; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
