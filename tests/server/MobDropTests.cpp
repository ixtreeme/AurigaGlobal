// Monster drop tables (F4 C7b, IC-035): validation, catalog checks and the
// planner's legacy-equivalent rolls with a scripted draw sequence.
#include "CatalogSnapshotFixture.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string>

using namespace auriga::item;
namespace f = item_catalog_fixture;
namespace
{
std::size_t checks = 0;
void Check(bool ok, const char* why, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!ok) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
MobDropEntry Entry(std::uint16_t number, ItemKey item, std::uint16_t level, std::uint32_t amount, std::uint32_t chance, std::uint32_t weight,
                   std::uint16_t rare = 0, std::uint16_t low = 1, std::uint16_t high = 65535)
{
    return {number, item, level, amount, chance, weight, rare, low, high};
}
// The same four groups as ItemCatalogDigestOracle.py's with_mob_drops fixture.
ItemCatalogSource Fixture()
{
    auto s = f::Catalog();
    s.mobDropGroups = {
        {1, "drop.common_pawn", MobDropTarget::Rank, 0, MobDropRoll::EachEntry, 0, 0, {Entry(1, f::Leather(), 0, 5, 250'000, 0, 0, 1, 30)}},
        {2, "drop.wolf_kill", MobDropTarget::Mob, 101, MobDropRoll::OneInKills, 50, 0,
         {Entry(1, f::Sword(), 3, 1, 0, 3, 500), Entry(2, f::Leather(), 0, 10, 0, 7)}},
        {3, "drop.wolf_level", MobDropTarget::Mob, 101, MobDropRoll::LevelGated, 0, 20, {Entry(1, f::Leather(), 0, 1, 1'000'000, 0)}},
        {4, "drop.wolf_gloves", MobDropTarget::Mob, 101, MobDropRoll::DoubleDrop, 0, 0, {Entry(1, f::Leather(), 0, 2, 2'500'000, 0)}},
    };
    return s;
}
// Hands out the scripted values in order and records every requested bound.
struct Script
{
    std::deque<std::uint32_t> values;
    std::vector<std::uint32_t> bounds;
    MobDropDraw Draw()
    {
        return [this](std::uint32_t bound) {
            bounds.push_back(bound);
            if (values.empty()) throw std::runtime_error("unexpected draw");
            const auto value = values.front(); values.pop_front(); return value;
        };
    }
};
MobDropContext Wolf(std::uint16_t killerLevel = 25)
{
    MobDropContext kill;
    kill.mobVnum = 101; kill.mobRank = 0; kill.killerLevel = killerLevel;
    return kill;
}
void Validation()
{
    const auto reject = [](auto mutate, MobDropGroupError expected) {
        auto source = Fixture();
        mutate(source.mobDropGroups);
        const auto built = BuildItemCatalog(source);
        Check(!built && built.error().code == ItemCatalogErrorCode::InvalidMobDrop && built.error().mobDropError == expected, "bad drop group rejected");
    };
    using G = std::vector<MobDropGroup>;
    reject([](G& g) { g[0].targetValue = 6; }, MobDropGroupError::InvalidTarget);
    reject([](G& g) { g[0].roll = MobDropRoll::OneInKills; g[0].killsPerDrop = 5; }, MobDropGroupError::InvalidTarget);
    reject([](G& g) { g[1].targetValue = 0; }, MobDropGroupError::InvalidTarget);
    reject([](G& g) { g[1].killsPerDrop = 0; }, MobDropGroupError::InvalidRoll);
    reject([](G& g) { g[2].minimumKillerLevel = 0; }, MobDropGroupError::InvalidRoll);
    reject([](G& g) { g[3].killsPerDrop = 9; }, MobDropGroupError::InvalidRoll);
    reject([](G& g) { g[0].entries.clear(); }, MobDropGroupError::EmptyGroup);
    reject([](G& g) { g[1].entries[1].entry = 3; }, MobDropGroupError::InvalidEntryOrder);
    reject([](G& g) { g[0].entries[0].chancePpb = 0; }, MobDropGroupError::InvalidChance);
    reject([](G& g) { g[0].entries[0].chancePpb = MobDropChanceScale + 1; }, MobDropGroupError::InvalidChance);
    reject([](G& g) { g[0].entries[0].weight = 1; }, MobDropGroupError::InvalidChance);
    reject([](G& g) { g[1].entries[0].weight = 0; }, MobDropGroupError::InvalidWeight);
    reject([](G& g) { g[1].entries[0].chancePpb = 5; }, MobDropGroupError::InvalidWeight);
    reject([](G& g) { g[1].entries[0].weight = 4'294'967'295u; }, MobDropGroupError::WeightOverflow);
    reject([](G& g) { g[0].entries[0].amount = 0; }, MobDropGroupError::InvalidAmount);
    reject([](G& g) { g[0].entries[0].rareAttributeChanceBp = 10'001; }, MobDropGroupError::InvalidRareChance);
    reject([](G& g) { g[0].entries[0].killerLevelMin = 31; }, MobDropGroupError::InvalidKillerLevels);
    reject([](G& g) { g[0].entries[0].killerLevelMin = 0; }, MobDropGroupError::InvalidKillerLevels);
    // Against the catalog: a live item, a whole stack, a refinement level it has.
    reject([](G& g) { g[0].entries[0].item = ItemKey::Create(1, 999).value(); }, MobDropGroupError::InvalidItem);
    reject([](G& g) { g[0].entries[0].amount = 201; }, MobDropGroupError::InvalidAmount);
    reject([](G& g) { g[1].entries[0].refinementLevel = 10; }, MobDropGroupError::InvalidLevel);
    reject([](G& g) { g[0].entries[0].refinementLevel = 1; }, MobDropGroupError::InvalidLevel);
    auto duplicate = Fixture(); duplicate.mobDropGroups[1].code = duplicate.mobDropGroups[0].code;
    Check(BuildItemCatalog(duplicate).error().code == ItemCatalogErrorCode::DuplicateProfile, "drop group codes are unique");
    // Entries arrive in any order and are normalised; the hash does not change.
    auto reversed = Fixture(); std::ranges::reverse(reversed.mobDropGroups[1].entries); std::ranges::reverse(reversed.mobDropGroups);
    Check(BuildItemCatalog(reversed).value()->ContentHash() == BuildItemCatalog(Fixture()).value()->ContentHash(), "entry and group order normalise");
    for (int field = 0; field < 4; ++field)
    {
        auto changed = Fixture(); auto& e = changed.mobDropGroups[1].entries[0];
        if (field == 0) e.weight++; if (field == 1) changed.mobDropGroups[1].entries[1].amount++; if (field == 2) e.rareAttributeChanceBp++; if (field == 3) changed.mobDropGroups[1].killsPerDrop++;
        Check(BuildItemCatalog(changed).value()->ContentHash() != BuildItemCatalog(Fixture()).value()->ContentHash(), "drop content is part of the release hash");
    }
}
void Lookup()
{
    const auto catalog = BuildItemCatalog(Fixture()).value();
    const auto groups = catalog->MobDropGroupsFor(101, 0);
    Check(groups.size() == 4 && groups[0]->id == 1 && groups[3]->id == 4, "the rank's and the mob's groups, in group order");
    Check(catalog->MobDropGroupsFor(102, 0).size() == 1 && catalog->MobDropGroupsFor(101, 1).size() == 3 && catalog->MobDropGroupsFor(7, 9).empty(),
          "other mobs and ranks see only their own groups");
    Check(catalog->FindMobDropGroup(3) && catalog->FindMobDropGroup(3)->minimumKillerLevel == 20 && !catalog->FindMobDropGroup(9), "find by group ID");
}
void SqlRows()
{
    const auto source = Fixture();
    const auto original = BuildItemCatalog(source).value();
    auto snapshot = catalog_snapshot_fixture::Snapshot(source);
    auto decoded = auriga::item::sql::DecodeSnapshot(snapshot, {88, original->ContentHash()});
    Check(decoded && (*decoded)->ContentHash() == original->ContentHash() && (*decoded)->FindMobDropGroup(2)->entries.size() == 2,
          "drop groups and entries round-trip the SQL rows");
    snapshot[auriga::item::sql::Table::MobDropEntries][0][0] = "999";
    Check(!auriga::item::sql::DecodeSnapshot(snapshot, {1, {}}), "an orphan drop entry is rejected");
}
void Planner()
{
    const auto catalog = BuildItemCatalog(Fixture()).value();
    constexpr std::uint32_t Range = 4'000'000;
    // Draw order: the rank entry, the kill group, its weighted pick, the level-gated entry.
    // Legacy equivalence: dwPct 1000 (0.1 in the file) is 250000 ppb, and
    // 1000 * 100 / 100 = 1000 is the threshold against number(1, range).
    {
        Script script{{1000, 80'000, 3, 1'000'000}};
        const auto drops = PlanMobDrops(*catalog, Wolf(), script.Draw()).value();
        Check(script.bounds == std::vector<std::uint32_t>{Range, Range, 10, MobDropChanceScale}, "one draw per roll, with the legacy bounds");
        Check(drops.size() == 3 && drops[0].group == 1 && drops[0].amount == 5 && drops[1].group == 2 && drops[1].entry == 1 &&
              drops[1].item == f::Sword() && drops[1].refinementLevel == 3 && drops[1].rareAttributeChanceBp == 500 && drops[2].group == 3,
              "at the thresholds everything drops; the weighted pick 3 of 10 is the sword");
    }
    {
        Script script{{1001, 80'001, 1'000'001}};
        Check(PlanMobDrops(*catalog, Wolf(), script.Draw()).value().empty(), "one above every threshold, nothing drops and no pick is drawn");
    }
    {
        Script script{{1, 1, 4, 1}};
        const auto drops = PlanMobDrops(*catalog, Wolf(), script.Draw()).value();
        Check(drops.size() == 3 && drops[1].entry == 2 && drops[1].amount == 10, "the pick 4 of 10 is the second entry");
    }
    {
        // A boss-level delta of 200 doubles the scaled thresholds, not the level-gated one.
        auto kill = Wolf(); kill.deltaPercent = 200;
        Script script{{2000, 160'000, 10, 1'000'001}};
        const auto drops = PlanMobDrops(*catalog, kill, script.Draw()).value();
        Check(drops.size() == 2 && drops[0].group == 1 && drops[1].group == 2, "delta scales the drop and kill thresholds only");
    }
    {
        // Killer level 19: below the level gate; 31: outside the common entry's window.
        Script low{{1, 1, 1}};
        Check(PlanMobDrops(*catalog, Wolf(19), low.Draw()).value().size() == 2 && low.bounds.size() == 3, "no level-gated draw below its level");
        Script high{{1, 1, 1}};
        const auto drops = PlanMobDrops(*catalog, Wolf(31), high.Draw()).value();
        Check(high.bounds == std::vector<std::uint32_t>{Range, 10, MobDropChanceScale} && drops.size() == 2 && drops[0].group == 2, "an entry outside its killer-level window is not drawn");
    }
    {
        // A double drop adds the gloves group: 2500000 ppb * 100 / 25000 = 10000.
        auto kill = Wolf(); kill.doubleDrop = true;
        Script script{{Range, Range, Range, 10'000}};
        const auto drops = PlanMobDrops(*catalog, kill, script.Draw()).value();
        Check(script.bounds.back() == Range && drops.size() == 1 && drops[0].group == 4 && drops[0].amount == 2, "the double drop rolls the gloves group");
    }
    {
        // A stone: no rank group, no level-gated or double drop; its own drop and kill groups stay.
        auto kill = Wolf(); kill.stone = true; kill.doubleDrop = true;
        Script script{{Range}};
        Check(PlanMobDrops(*catalog, kill, script.Draw()).value().empty() && script.bounds == std::vector<std::uint32_t>{Range}, "a stone rolls only its kill group here");
    }
    {
        Script script{{0}};
        Check(PlanMobDrops(*catalog, Wolf(), script.Draw()).error() == MobDropError::DrawOutOfRange, "a draw below 1 is refused");
        Script over{{Range + 1}};
        Check(PlanMobDrops(*catalog, Wolf(), over.Draw()).error() == MobDropError::DrawOutOfRange, "a draw above the bound is refused");
        auto kill = Wolf(); kill.randomRange = 0;
        Script none{{}};
        Check(PlanMobDrops(*catalog, kill, none.Draw()).error() == MobDropError::InvalidContext, "an empty range is refused");
    }
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--hash-fixture")
    {
        const auto hash = BuildItemCatalog(Fixture()).value()->ContentHash();
        for (auto b : hash) std::cout << "0123456789abcdef"[b >> 4] << "0123456789abcdef"[b & 15];
        std::cout << '\n';
        return 0;
    }
    try
    {
        Validation(); Lookup(); SqlRows(); Planner();
        std::cout << "Mob drop checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
