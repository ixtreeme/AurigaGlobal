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
BonusProfile Fixture()
{
    return {1, "bonus.fixture", BonusSource::ItemBase, {{BonusId::NormalOutgoing, 500}, {BonusId::PhysicalAttackFlat, 2500}}};
}
template <typename Mutate> void Reject(Mutate mutate, BonusProfileError expected)
{
    auto source = Fixture(); mutate(source);
    auto result = ValidateBonusProfile(std::move(source));
    Check(!result && result.error() == expected, "malformed bonus must be rejected");
}
void ProfileRules()
{
    auto source = Fixture();
    auto validated = ValidateBonusProfile(source).value();
    Check(validated.Data().entries.front().bonus == BonusId::PhysicalAttackFlat, "canonical bonus order");
    source.entries.clear();
    Check(validated.Data().entries.size() == 2, "profile owns its entries");
    Check(AcceptsBonusSource(validated, BonusSource::ItemBase) && !AcceptsBonusSource(validated, BonusSource::Refinement), "source kinds cannot be confused");
    Check(!FindBonusDefinition(0) && !FindBonusDefinition(999), "unregistered definition rejected");
    Check(FindBonusDefinition(BonusId::PhysicalAttackFlat)->unit == BonusUnit::Raw, "attack bonus uses raw, not old points");
    Check(FindBonusDefinition(BonusId::NormalOutgoing)->unit == BonusUnit::BasisPoints, "outgoing uses bp, not old percent");
    Check(FindBonusDefinition(BonusId::BlockReduction)->stage == BonusStage::Block, "block is deterministic mitigation metadata");
    Check(!AllowsBonusSource(BonusDefinitions[0], static_cast<BonusSource>(255)), "invalid source does not shift out of bounds");
    Reject([](auto& p) { p.id = 0; }, BonusProfileError::InvalidIdentity);
    Reject([](auto& p) { p.code = "invalid+9"; }, BonusProfileError::InvalidIdentity);
    Reject([](auto& p) { p.source = static_cast<BonusSource>(0); }, BonusProfileError::InvalidSource);
    Reject([](auto& p) { p.entries.clear(); }, BonusProfileError::EmptyProfile);
    Reject([](auto& p) { p.entries.resize(MaxBonusEntriesPerProfile + 1); }, BonusProfileError::TooManyEntries);
    Reject([](auto& p) { p.entries[0].bonus = static_cast<BonusId>(0); }, BonusProfileError::UnknownBonus);
    Reject([](auto& p) { p.entries[0].bonus = static_cast<BonusId>(4294967295u); }, BonusProfileError::UnknownBonus);
    Reject([](auto& p) { p.entries.push_back(p.entries[0]); }, BonusProfileError::DuplicateBonus);
    Reject([](auto& p) { p.entries[1].valueScaled = std::numeric_limits<std::int64_t>::min(); }, BonusProfileError::ValueOutOfRange);
    Reject([](auto& p) { p.entries[1].valueScaled = std::numeric_limits<std::int64_t>::max(); }, BonusProfileError::ValueOutOfRange);
    for (const auto& definition : BonusDefinitions)
    {
        for (const auto kind : {BonusSource::ItemBase, BonusSource::Refinement})
        {
            for (const auto value : {definition.minimum, std::int64_t{0}, definition.maximum})
            {
                BonusProfile profile{1, "bonus.boundary", kind, {{static_cast<BonusId>(definition.id), value}}};
                Check(ValidateBonusProfile(std::move(profile)).has_value(), "exact individual boundaries accepted");
            }
        }
        for (const auto value : {definition.minimum - 1, definition.maximum + 1})
        {
            BonusProfile profile{1, "bonus.boundary", BonusSource::ItemBase, {{static_cast<BonusId>(definition.id), value}}};
            Check(ValidateBonusProfile(std::move(profile)).error() == BonusProfileError::ValueOutOfRange, "boundary violation never clamps");
        }
    }
}
void RefinementContext()
{
    std::vector<ValidatedBonusProfile> bonuses;
    for (auto row : item_catalog_fixture::Catalog().bonuses) bonuses.push_back(ValidateBonusProfile(std::move(row)).value());
    auto profile = item_catalog_fixture::WeaponRefinement();
    profile.levels[3].extraBonusProfile = 2;
    Check(ValidateRefinementProfile(profile).error() == RefinementError::MissingBonusProfile, "absent validation context fails closed");
    Check(ValidateRefinementProfile(profile, bonuses).has_value(), "known refinement source resolves");
    std::ranges::reverse(bonuses);
    Check(ValidateRefinementProfile(profile, bonuses).has_value(), "unordered context has the same meaning");
    std::ranges::reverse(bonuses);
    auto duplicate = bonuses;
    duplicate.push_back(bonuses[1]);
    Check(ValidateRefinementProfile(profile, duplicate).error() == RefinementError::InvalidBonusContext, "ambiguous context cannot depend on arrival order");
    Check(!FindBonusProfile(duplicate, 2), "ambiguous direct lookup also fails closed");
    profile.levels[3].extraBonusProfile = 1;
    Check(ValidateRefinementProfile(profile, bonuses).error() == RefinementError::WrongBonusSource, "base source cannot be used as refinement source");
    profile.levels[3].extraBonusProfile = 0;
    Check(ValidateRefinementProfile(profile, bonuses).error() == RefinementError::MissingBonusProfile, "optional zero is not null");
    profile.levels[3].extraBonusProfile = 2;
    profile.levels[0].extraBonusProfile = 2;
    Check(ValidateRefinementProfile(profile, bonuses).error() == RefinementError::NonzeroLevelZero, "level zero forbids an extra profile");
    const auto moved = std::move(bonuses[1]);
    Check(AcceptsBonusSource(moved, BonusSource::Refinement), "moved-to profile retains content");
    const auto* old = FindBonusProfile(bonuses, 2);
    Check(!old || !old->Data().entries.empty(), "moved-from empty profile cannot authorize a reference");
    profile.levels[0].extraBonusProfile.reset();
    if (bonuses[1].Data().entries.empty())
        Check(ValidateRefinementProfile(profile, bonuses).error() == RefinementError::InvalidBonusContext, "moved-from context rejected before lookup");
}
}
int main()
{
    try { ProfileRules(); RefinementContext(); std::cout << "Bonus profile checks passed: " << checks << '\n'; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
