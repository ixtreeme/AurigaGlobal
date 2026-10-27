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
template <typename Mutate> void Reject(Mutate mutate, RefinementError expected)
{
    auto source = item_catalog_fixture::WeaponRefinement();
    mutate(source);
    const auto result = ValidateRefinementProfile(std::move(source));
    Check(!result && result.error() == expected, "invalid profile must fail closed");
}
void GoldenAndOrdering()
{
    auto source = item_catalog_fixture::WeaponRefinement();
    std::ranges::reverse(source.levels);
    auto validated = ValidateRefinementProfile(source);
    Check(validated.has_value(), "unordered source rows normalize");
    const ItemBaseStats base {120000, 30000, 0, 0};
    Check(ResolveRefinedBaseStats(base, *validated, 0).value() == base, "level zero has no bonus");
    Check(ResolveRefinedBaseStats(base, *validated, 3).value() == ItemBaseStats{150000, 42000, 0, 0}, "+3 golden 150/42 points");
    Check(ResolveRefinedBaseStats(base, *validated, 9).value() == ItemBaseStats{210000, 66000, 0, 0}, "+9 golden 210/66 points");
    source.levels.clear();
    Check(validated->FindLevel(9)->gain.physicalAttackRaw == 90000, "validated profile owns source data");
    Check(!validated->FindLevel(10), "unknown level is never clamped");
    Check(ResolveRefinedBaseStats(base, *validated, 10).error() == RefinementError::InvalidLevelRange, "invalid level is a typed error");
    auto nonlinear = item_catalog_fixture::WeaponRefinement();
    nonlinear.maxLevel = 2;
    nonlinear.levels = {{0, {}, {}}, {1, {7000, 0, 0, 0}, {}}, {2, {11000, 0, 0, 0}, {}}};
    auto profile = ValidateRefinementProfile(std::move(nonlinear));
    Check(ResolveRefinedBaseStats(base, *profile, 2)->physicalAttackRaw == 131000, "cumulative gain is not summed from prior levels");
    const auto moved = std::move(*profile);
    const auto* old = profile->FindLevel(2);
    Check(!old || old->level == 2, "moved-from profile lookup remains bounded");
    Check(moved.FindLevel(2) != nullptr, "moved profile retains its rows");
    for (std::uint16_t maximum : {std::uint16_t{0}, std::uint16_t{255}})
    {
        RefinementProfile edge {20, "refine.boundary", RefinementKind::Weapon, maximum, {}};
        for (std::uint16_t i = 0; i <= maximum; ++i) edge.levels.push_back({i, {i, i, 0, 0}, {}});
        Check(ValidateRefinementProfile(std::move(edge)).has_value(), "level range 0 and 255 accepted");
    }
}
void InvalidProfiles()
{
    Reject([](auto& p) { p.id = 0; }, RefinementError::InvalidIdentity);
    Reject([](auto& p) { p.code = "invalid..code"; }, RefinementError::InvalidIdentity);
    Reject([](auto& p) { p.kind = static_cast<RefinementKind>(0); }, RefinementError::InvalidKind);
    Reject([](auto& p) { p.maxLevel = 256; }, RefinementError::InvalidLevelRange);
    Reject([](auto& p) { p.kind = RefinementKind::None; }, RefinementError::InvalidLevelRange);
    Reject([](auto& p) { p.levels.pop_back(); }, RefinementError::IncompleteLevels);
    Reject([](auto& p) { p.levels[4].level = 3; }, RefinementError::IncompleteLevels);
    Reject([](auto& p) { p.levels[4].level = 10; }, RefinementError::IncompleteLevels);
    Reject([](auto& p) { p.levels[0].gain.physicalAttackRaw = 1; }, RefinementError::NonzeroLevelZero);
    Reject([](auto& p) { p.levels[0].extraBonusProfile = 1; }, RefinementError::NonzeroLevelZero);
    Reject([](auto& p) { p.levels[1].extraBonusProfile = 999; }, RefinementError::MissingBonusProfile);
    Reject([](auto& p) { p.levels[3].gain.physicalAttackRaw = -1; }, RefinementError::InvalidGain);
    Reject([](auto& p) { p.levels[3].gain.physicalAttackRaw = MaxItemStatRaw + 1; }, RefinementError::InvalidGain);
    Reject([](auto& p) { p.levels[4].gain.physicalAttackRaw = 0; }, RefinementError::NonMonotonicGain);
    Reject([](auto& p) { p.levels[4].gain.physicalDefenseRaw = 1; }, RefinementError::WrongStatKind);
    Reject([](auto& p) { p.kind = RefinementKind::Armor; }, RefinementError::WrongStatKind);
    const auto profile = ValidateRefinementProfile(item_catalog_fixture::WeaponRefinement()).value();
    Check(ResolveRefinedBaseStats({MaxItemStatRaw, 0, 0, 0}, profile, 1).error() == RefinementError::StatOutOfRange, "base plus gain cannot overflow cap");
    Check(ResolveRefinedBaseStats({std::numeric_limits<std::int64_t>::max(), 0, 0, 0}, profile, 1).error() == RefinementError::StatOutOfRange, "int64 maximum rejected before addition");
    Check(ResolveRefinedBaseStats({std::numeric_limits<std::int64_t>::min(), 0, 0, 0}, profile, 1).error() == RefinementError::StatOutOfRange, "int64 minimum rejected before subtraction");
    Check(ResolveRefinedBaseStats({MaxItemStatRaw, 0, 0, 0}, profile, 0)->physicalAttackRaw == MaxItemStatRaw, "exact cap is valid");
    Check(ResolveRefinedBaseStats({0, 0, 100, 0}, profile, 1).error() == RefinementError::WrongStatKind, "weapon profile cannot refine armor base");
}
void ProfileDefinedCeiling()
{
    const ItemBaseStats base {120000, 30000, 0, 0};
    for (std::uint16_t maximum : {std::uint16_t{15}, std::uint16_t{30}, std::uint16_t{100}, std::uint16_t{255}})
    {
        RefinementProfile source {40, "refine.extended_weapon", RefinementKind::Weapon, maximum, {}};
        for (std::uint32_t level = 0; level <= maximum; ++level)
            source.levels.push_back({static_cast<std::uint16_t>(level), {10000LL * level, 4000LL * level, 0, 0}, {}});
        const auto profile = ValidateRefinementProfile(std::move(source)).value();
        Check(NextRefinementLevel(profile, 9).value() == 10, "+9 is not a hardcoded ceiling");
        for (std::uint16_t level = 0; level < maximum; ++level)
            Check(NextRefinementLevel(profile, level).value() == level + 1, "every configured step remains reachable");
        Check(NextRefinementLevel(profile, maximum).error() == RefinementError::AtMaximumLevel, "only this profile's configured maximum stops refinement");
        Check(NextRefinementLevel(profile, maximum + 1).error() == RefinementError::InvalidLevelRange, "invalid saved level cannot be treated as the maximum");
        Check(ResolveRefinedBaseStats(base, profile, maximum).value() == ItemBaseStats{120000 + 10000LL * maximum, 30000 + 4000LL * maximum, 0, 0},
              "higher levels use the same base and cumulative profile");
    }
    const auto none = ValidateRefinementProfile({1, "refine.none", RefinementKind::None, 0, {{0, {}, {}}}}).value();
    Check(NextRefinementLevel(none, 0).error() == RefinementError::AtMaximumLevel, "non-refinable item has an explicit zero maximum");
    const auto oldNine = ValidateRefinementProfile(item_catalog_fixture::WeaponRefinement()).value();
    Check(NextRefinementLevel(oldNine, 9).error() == RefinementError::AtMaximumLevel, "content can still explicitly choose nine without making it universal");
}
void AccessoryRefinementHasNoHiddenRawGain()
{
    const auto source = item_catalog_fixture::Catalog();
    std::vector<ValidatedBonusProfile> bonuses;
    for (const auto& row : source.bonuses) bonuses.push_back(ValidateBonusProfile(row).value());
    const auto profile = ValidateRefinementProfile(source.refinements[3], bonuses).value();
    Check(profile.Data().kind == RefinementKind::Accessory && profile.Data().maxLevel == 15, "accessory has its own refinement kind");
    Check(NextRefinementLevel(profile, 9).value() == 10, "accessory crosses level nine");
    Check(profile.FindLevel(15)->extraBonusProfile == 5, "accessory bonus is an explicit current-level reference");
    Check(ResolveRefinedBaseStats({}, profile, 15).value() == ItemBaseStats{}, "bonus-only refinement adds no implicit raw stat");
    Check(ResolveRefinedBaseStats({0, 0, 1000, 0}, profile, 15).error() == RefinementError::WrongStatKind, "accessory cannot add its bonus through an armor base path too");
    auto invalid = source.refinements[3];
    invalid.levels[1].gain.physicalDefenseRaw = 1000;
    Check(ValidateRefinementProfile(std::move(invalid), bonuses).error() == RefinementError::WrongStatKind, "accessory profile cannot hide raw defense gains");
}
}
int main()
{
    try { GoldenAndOrdering(); InvalidProfiles(); ProfileDefinedCeiling(); AccessoryRefinementHasNoHiddenRawGain(); std::cout << "Refinement checks passed: " << checks << '\n'; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
