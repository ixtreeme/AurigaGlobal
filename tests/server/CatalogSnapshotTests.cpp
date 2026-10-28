#include "CatalogSnapshotFixture.hpp"
#include <algorithm>
#include <iostream>
#include <source_location>
#include <stdexcept>

namespace db = auriga::item::sql;
namespace f = catalog_snapshot_fixture;
using namespace auriga::item;
using T = db::Table;
using E = db::ReadErrorCode;
namespace
{
std::size_t checks = 0;
void Check(bool value, const char* why, std::source_location at = std::source_location::current()) {
    ++checks; if (!value) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
void Reject(auto change, E error) {
    auto source = f::Snapshot(); change(source);
    const auto result = db::DecodeSnapshot(std::move(source),{1,{}});
    Check(!result && result.error().code == error,"bad SQL catalogue was accepted or lost its diagnostic");
}
void RoundTrip() {
    const auto expected = BuildItemCatalog(item_catalog_fixture::Catalog()).value();
    const auto loaded = db::DecodeSnapshot(f::Snapshot(),{77,expected->ContentHash()});
    Check(loaded.has_value() && (*loaded)->Revision()==77 && (*loaded)->ContentHash()==expected->ContentHash(),"SQL row projection changed the canonical catalogue");
    Check((*loaded)->Size()==item_catalog_fixture::FixtureItems && (*loaded)->Find(item_catalog_fixture::Sword()) && (*loaded)->Find(item_catalog_fixture::Armor()),"category-local ID collision lost an item");
    const auto before = (*loaded)->ContentHash();
    auto reversed = f::Snapshot(); for (auto& rows : reversed.tables) std::ranges::reverse(rows);
    Check(db::DecodeSnapshot(std::move(reversed),{78,before}).value()->ContentHash()==before,"SQL result order affected catalogue identity");
    auto hash = before; hash[0]^=1;
    auto mismatch = db::DecodeSnapshot(f::Snapshot(),{79,hash});
    Check(!mismatch && mismatch.error().code==E::HashMismatch,"wrong release hash was accepted");
    Check(!db::DecodeSnapshot(f::Snapshot(),{}),"zero revision was accepted");
    auto destroyed = f::Snapshot(); auto independent = db::DecodeSnapshot(std::move(destroyed),{1,{}}).value();
    destroyed={};
    Check(independent->ContentHash()==before,"catalogue borrowed SQL result memory");
}
void ShapeAndRegistries() {
    Reject([](auto& s){s[T::Format].clear();},E::FormatMismatch);
    Reject([](auto& s){s[T::Format][0][1]="4";},E::FormatMismatch);
    Reject([](auto& s){s[T::Format][0][2]="auriga.item_catalog.v4";},E::FormatMismatch);
    Reject([](auto& s){s[T::Categories].pop_back();},E::RegistryMismatch);
    Reject([](auto& s){s[T::Categories][1]=s[T::Categories][0];},E::RegistryMismatch);
    Reject([](auto& s){s[T::Categories][0][2]="other_table";},E::RegistryMismatch);
    Reject([](auto& s){s[T::BonusDefinitions].pop_back();},E::RegistryMismatch);
    Reject([](auto& s){s[T::BonusDefinitions][1]=s[T::BonusDefinitions][0];},E::RegistryMismatch);
    for (std::size_t column=1;column<17;++column) {
        auto s=f::Snapshot(); s[T::BonusDefinitions][0][column] = column<3 ? "tampered" : "0";
        // Several compiled filters legitimately use zero: change those to one.
        if (s[T::BonusDefinitions][0][column] == f::Snapshot()[T::BonusDefinitions][0][column]) s[T::BonusDefinitions][0][column]="1";
        auto result=db::DecodeSnapshot(std::move(s),{1,{}});
        Check(!result && result.error().code==E::RegistryMismatch,"SQL mutated a compiled bonus definition");
    }
    for (const auto& spec : db::Tables()) {
        auto s=f::Snapshot(); auto& rows=s[spec.table];
        if (rows.empty()) continue;
        rows.front().pop_back();
        auto result=db::DecodeSnapshot(std::move(s),{1,{}});
        Check(!result && result.error().code==E::ColumnCount && result.error().table==spec.table,"malformed column count lost its location");
    }
    Reject([](auto& s){s[T::UsePolicies].resize(ItemCatalogLimits::ProfilesPerKind+1);},E::RowLimit);
    Reject([](auto& s){s[T::Items][0][2]=std::string(db::CapturedByteBudget+1,'a');},E::ByteBudget);
}
void NumbersAndNulls() {
    for (const char* text : {"", "-1", "+1", " 1", "1 ", "1.0", "1e2", "01", "18446744073709551616"})
        Reject([&](auto& s){s[T::Items][0][18]=text;},E::InvalidNumber);
    for (const char* text : {"-9223372036854775809","9223372036854775808","-0"})
        Reject([&](auto& s){s[T::Weapons][0][4]=text;},E::InvalidNumber);
    Reject([](auto& s){s[T::Items][0][0]="65537";},E::InvalidNumber);
    Reject([](auto& s){s[T::Items][0][1]="4294967296";},E::InvalidNumber);
    Reject([](auto& s){s[T::Items][0][6]="256";},E::InvalidNumber);
    Reject([](auto& s){s[T::Items][0][20]="2";},E::InvalidNumber);
    Reject([](auto& s){s[T::Items][0][2]=std::nullopt;},E::NullValue);
    Reject([](auto& s){s[T::Items][0][2]=std::string("weapon\0.hidden",14);},E::InvalidText);
    Reject([](auto& s){s[T::Items][0][15]="0";},E::InvalidCatalog);
    Reject([](auto& s){s[T::Items][0][0]="0";},E::InvalidKey);
    Reject([](auto& s){s[T::Items][0][1]="0";},E::InvalidKey);
    auto minimum=f::Snapshot(); minimum[T::Weapons][0][4]="-9223372036854775808";
    auto result=db::DecodeSnapshot(std::move(minimum),{1,{}});
    Check(!result && result.error().code==E::InvalidCatalog,"INT64_MIN was narrowed or failed integer decoding");
}
void ChildCoverage() {
    Reject([](auto& s){s[T::BonusEntries][0][0]="999";},E::OrphanRow);
    Reject([](auto& s){s[T::RefinementLevels][0][0]="999";},E::OrphanRow);
    Reject([](auto& s){s[T::SocketGrades][0][0]="999";},E::OrphanRow);
    Reject([](auto& s){s[T::SocketMaterials][0][0]="999";},E::OrphanRow);
    Reject([](auto& s){s[T::Instant][0][0]="999";},E::OrphanRow);
    Reject([](auto& s){s[T::Instant].clear();},E::InvalidChild);
    Reject([](auto& s){s[T::Reserve][0][1]="1";},E::InvalidChild);
    Reject([](auto& s){s[T::Recoveries][0][2]="2";},E::InvalidChild);
    Reject([](auto& s){s[T::Instant].push_back(s[T::Instant][0]);},E::DuplicateRow);
    Reject([](auto& s){s[T::Refinements].push_back(s[T::Refinements][0]);},E::DuplicateRow);
    Reject([](auto& s){s[T::SocketProfiles].push_back(s[T::SocketProfiles][0]);},E::DuplicateRow);
    Reject([](auto& s){s[T::Weapons].clear();},E::InvalidCatalog);
    Reject([](auto& s){s[T::Weapons].push_back(s[T::Weapons][0]);},E::InvalidCatalog);
    Reject([](auto& s){s[T::SocketMaterials][0][1]="4";},E::InvalidChild);
    Reject([](auto& s){s[T::Items].push_back(s[T::Items][0]);},E::InvalidCatalog);
    Reject([](auto& s){s[T::BonusEntries].push_back(s[T::BonusEntries][0]);},E::InvalidCatalog);
    Reject([](auto& s){s[T::TimedEffects][0][2]="999";},E::InvalidCatalog);
    Reject([](auto& s){s[T::TimedEffects][0][3]="1";},E::InvalidCatalog);
    Reject([](auto& s){s[T::TimedEffects][0][4]="0";},E::InvalidCatalog);
    Reject([](auto& s){s[T::Consumables][3][2]="1";},E::InvalidCatalog);
    Reject([](auto& s){s[T::Consumables][3][2]="0";},E::InvalidNumber);
    Reject([](auto& s){s[T::Consumables][3][3]=std::nullopt;},E::InvalidCatalog);
    Reject([](auto& s){s[T::TimedEffectGroups][0][4]="2";},E::InvalidNumber);
    Reject([](auto& s){s[T::Automatic][0][0]="999";},E::OrphanRow);
    Reject([](auto& s){s[T::Automatic].clear();},E::InvalidChild);
    Reject([](auto& s){s[T::Automatic][0][1]="1";},E::InvalidChild);
    Reject([](auto& s){s[T::Automatic][0][4]="0";},E::InvalidCatalog);
    Reject([](auto& s){s[T::Automatic][0][4]="9223372036854775808";},E::InvalidNumber);
    Reject([](auto& s){s[T::Recoveries][0][7]="65536";},E::InvalidNumber);
}
void AboveNine() {
    auto source=item_catalog_fixture::Catalog();
    auto& profile=source.refinements[1]; profile.maxLevel=30;
    for (std::uint16_t level=10;level<=30;++level) profile.levels.push_back({level,{10'000LL*level,4'000LL*level,0,0},3});
    const auto loaded=db::DecodeSnapshot(f::Snapshot(source),{123,{}});
    Check(loaded.has_value(),"SQL projection retained a universal +9 ceiling");
    const auto key=item_catalog_fixture::Sword();
    Check((*loaded)->Find(key)->base.key==key && (*loaded)->Size()==item_catalog_fixture::FixtureItems,"refinement duplicated the prototype");
    for (std::uint16_t level : {9,10,15,30})
        Check((*loaded)->RefinedBaseStats(key,level)->physicalAttackRaw==120'000+10'000LL*level,"loaded level curve differs");
    Check(!(*loaded)->RefinedBaseStats(key,31),"configured maximum was ignored");
    source.refinements[1].levels.erase(source.refinements[1].levels.begin()+10);
    auto gap=db::DecodeSnapshot(f::Snapshot(source),{124,{}});
    Check(!gap && gap.error().code==E::InvalidCatalog,"missing +10 level escaped publication validation");
}
}
int main() {
    try { RoundTrip(); ShapeAndRegistries(); NumbersAndNulls(); ChildCoverage(); AboveNine();
        std::cout << "Catalog SQL snapshot: " << checks << " checks passed\n"; return 0;
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
