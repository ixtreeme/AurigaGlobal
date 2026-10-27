#include "ItemCatalogFixture.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace
{
std::size_t checks = 0;
void Check(bool condition, const char* message, const std::source_location where = std::source_location::current())
{
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line()));
}
template <typename Mutate> void RejectProfile(Mutate mutate, RecoveryProfileError expected)
{
    auto source = item_catalog_fixture::Catalog().recoveries[0]; mutate(source);
    const auto result = ValidateRecoveryProfile(std::move(source));
    Check(!result && result.error() == expected, "invalid recovery profile rejected");
}
void Profiles()
{
    const auto fixture = item_catalog_fixture::Catalog();
    for (const auto& source : fixture.recoveries)
    {
        const auto result = ValidateRecoveryProfile(source);
        Check(result.has_value() && result->Data().Delivery() == source.Delivery(), "each typed delivery validates");
    }
    auto source = fixture.recoveries[0];
    const auto sealed = ValidateRecoveryProfile(source).value();
    source.code.clear(); source.effect = InstantRecovery{};
    Check(sealed.Data().code == "recovery.instant" && std::get<InstantRecovery>(sealed.Data().effect).hpRaw == 300000, "validated profile owns its fields");
    RejectProfile([](auto& p) { p.id=0; }, RecoveryProfileError::InvalidIdentity);
    RejectProfile([](auto& p) { p.code="invalid+9"; }, RecoveryProfileError::InvalidIdentity);
    RejectProfile([](auto& p) { p.unitsPerUse=0; }, RecoveryProfileError::InvalidUnits);
    RejectProfile([](auto& p) { p.allowedContexts=0; }, RecoveryProfileError::InvalidContexts);
    RejectProfile([](auto& p) { p.allowedContexts=16; }, RecoveryProfileError::InvalidContexts);
    RejectProfile([](auto& p) { p.cooldownProfile=0; }, RecoveryProfileError::InvalidCooldown);
    for (auto amount : std::array<std::int64_t,2>{-1, MaxItemStatRaw+1})
    {
        RejectProfile([&](auto& p) { std::get<InstantRecovery>(p.effect).hpRaw=amount; }, RecoveryProfileError::InvalidAmount);
        RejectProfile([&](auto& p) { p.effect=ReserveRecovery{0,amount,0,1000,700}; }, RecoveryProfileError::InvalidAmount);
        RejectProfile([&](auto& p) { p.effect=PeriodicRecovery{amount,0,1000,60000}; }, RecoveryProfileError::InvalidAmount);
    }
    RejectProfile([](auto& p) { p.effect=InstantRecovery{}; }, RecoveryProfileError::EmptyEffect);
    RejectProfile([](auto& p) { std::get<InstantRecovery>(p.effect).maxHpBp=10001; }, RecoveryProfileError::InvalidAmount);
    RejectProfile([](auto& p) { std::get<InstantRecovery>(p.effect).maxSpBp=10001; }, RecoveryProfileError::InvalidAmount);
    RejectProfile([](auto& p) { std::get<InstantRecovery>(p.effect).flatBonusCapBp=100001; }, RecoveryProfileError::InvalidAmount);
    RejectProfile([](auto& p) { p.effect=ReserveRecovery{0,0,0,1000,700}; }, RecoveryProfileError::EmptyEffect);
    RejectProfile([](auto& p) { p.effect=ReserveRecovery{1,0,0,0,700}; }, RecoveryProfileError::InvalidInterval);
    RejectProfile([](auto& p) { p.effect=ReserveRecovery{1,0,0,UINT64_MAX,700}; }, RecoveryProfileError::InvalidInterval);
    RejectProfile([](auto& p) { p.effect=ReserveRecovery{1,0,100001,1000,700}; }, RecoveryProfileError::InvalidAmount);
    RejectProfile([](auto& p) { p.effect=ReserveRecovery{1,0,0,1000,0}; }, RecoveryProfileError::InvalidAmount);
    RejectProfile([](auto& p) { p.effect=ReserveRecovery{1,0,0,1000,10001}; }, RecoveryProfileError::InvalidAmount);
    RejectProfile([](auto& p) { p.effect=PeriodicRecovery{0,0,1000,60000}; }, RecoveryProfileError::EmptyEffect);
    RejectProfile([](auto& p) { p.effect=PeriodicRecovery{1,1,1000,60000}; }, RecoveryProfileError::MultiplePools);
    RejectProfile([](auto& p) { p.effect=PeriodicRecovery{1,0,0,60000}; }, RecoveryProfileError::InvalidInterval);
    RejectProfile([](auto& p) { p.effect=PeriodicRecovery{1,0,1000,999}; }, RecoveryProfileError::InvalidDuration);
    RejectProfile([](auto& p) { p.effect=PeriodicRecovery{1,0,1000,60001}; }, RecoveryProfileError::InvalidDuration);
    RejectProfile([](auto& p) { p.effect=PeriodicRecovery{1,0,1000,UINT64_MAX}; }, RecoveryProfileError::InvalidDuration);
    Check(ValidConsumableCooldown({1,"cooldown.ok",static_cast<std::uint64_t>(INT64_MAX)}), "full duration bound accepted");
    for (auto p : {ConsumableCooldownProfile{0,"cooldown.bad",1000}, {1,"invalid+9",1000}, {1,"cooldown.bad",0}, {1,"cooldown.bad",UINT64_MAX}})
        Check(!ValidConsumableCooldown(p), "malformed cooldown rejected");
}
InstantRecoveryAdmission Admission()
{
    InstantRecoveryAdmission state;
    state.pools = {500000,2000000,200000,1000000};
    state.context = RecoveryContext::World; state.level=10; state.availableUnits=5;
    return state;
}
void UsePlans()
{
    auto source = item_catalog_fixture::Catalog();
    const auto catalog = BuildItemCatalog(source).value();
    const auto potion = item_catalog_fixture::InstantPotion();
    const auto state = Admission();
    const auto hash = catalog->ContentHash();
    const auto plan = PlanInstantRecovery(*catalog, potion, state).value();
    Check(plan.hpRestoredRaw==400000 && plan.spRestoredRaw==200000 && plan.unitsToConsume==1, "flat plus percent pool recovery golden");
    Check(plan.cooldown && plan.cooldown->profileId==1 && plan.cooldown->durationMs==1000, "shared cooldown is proposed with the resource use");
    Check(state.pools.hpRaw==500000 && state.availableUnits==5 && !state.cooldownRemainingMs && catalog->ContentHash()==hash,
          "proposal mutates no server input or immutable prototype");
    auto bonus = state; bonus.potionBonusBp=5000;
    const auto boosted = PlanInstantRecovery(*catalog, potion, bonus).value();
    Check(boosted.hpRestoredRaw==550000 && boosted.spRestoredRaw==250000, "potion bonus scales flat recovery only");
    bonus.potionBonusBp=100000;
    Check(PlanInstantRecovery(*catalog,potion,bonus)->hpRestoredRaw==700000, "flat bonus capped by explicit profile");
    bonus.potionBonusBp=-10000;
    const auto percentOnly = PlanInstantRecovery(*catalog,potion,bonus).value();
    Check(percentOnly.hpRestoredRaw==100000 && percentOnly.spRestoredRaw==100000, "negative flat bonus never subtracts HP or scales percentage part");
    auto almostFull = state; almostFull.pools={1999999,2000000,999998,1000000};
    const auto clamped = PlanInstantRecovery(*catalog,potion,almostFull).value();
    Check(clamped.hpRestoredRaw==1 && clamped.spRestoredRaw==2, "healing clamps independently to missing pools");
    almostFull.pools.hpRaw=almostFull.pools.maxHpRaw;
    Check(!PlanInstantRecovery(*catalog,potion,almostFull)->hpRestoredRaw, "one full pool does not cancel useful recovery of the other");
    almostFull.pools.spRaw=almostFull.pools.maxSpRaw;
    Check(PlanInstantRecovery(*catalog,potion,almostFull).error()==RecoveryUseError::NoEffect, "full character cannot consume an instant potion");
    auto noSp=state; noSp.pools.spRaw=0; noSp.pools.maxSpRaw=0;
    Check(PlanInstantRecovery(*catalog,potion,noSp)->spRestoredRaw==0, "zero-capacity SP pool is lawful");
    auto noCooldown=source; noCooldown.recoveries[0].cooldownProfile.reset();
    Check(!PlanInstantRecovery(*BuildItemCatalog(noCooldown).value(),potion,state)->cooldown, "optional cooldown has no implicit default");
    Check(PlanInstantRecovery(*catalog,item_catalog_fixture::Sword(),state).error()==RecoveryUseError::WrongCategory, "equipment cannot invoke a healing profile");
    Check(PlanInstantRecovery(*catalog,ItemKey::Create(ItemCategory::Consumable,999).value(),state).error()==RecoveryUseError::MissingItem, "unknown prototype rejected");
    for (auto key : {item_catalog_fixture::ReservePotion(),item_catalog_fixture::PeriodicPotion()})
        Check(PlanInstantRecovery(*catalog,key,state).error()==RecoveryUseError::WrongDelivery, "scheduled healing cannot execute as an instant potion");
    const std::vector<std::pair<std::function<void(InstantRecoveryAdmission&)>,RecoveryUseError>> denied {
        {[](auto& s){s.pools.hpRaw=-1;},RecoveryUseError::InvalidState},
        {[](auto& s){s.pools.maxHpRaw=0;},RecoveryUseError::InvalidState},
        {[](auto& s){s.pools.maxSpRaw=MaxItemStatRaw+1;},RecoveryUseError::InvalidState},
        {[](auto& s){s.pools.spRaw=s.pools.maxSpRaw+1;},RecoveryUseError::InvalidState},
        {[](auto& s){s.potionBonusBp=-10001;},RecoveryUseError::InvalidState},
        {[](auto& s){s.potionBonusBp=100001;},RecoveryUseError::InvalidState},
        {[](auto& s){s.classIndex=4;},RecoveryUseError::InvalidState},
        {[](auto& s){s.sexIndex=2;},RecoveryUseError::InvalidState},
        {[](auto& s){s.context=static_cast<RecoveryContext>(3);},RecoveryUseError::InvalidState},
        {[](auto& s){s.pools.hpRaw=0;},RecoveryUseError::DeadCharacter},
        {[](auto& s){s.level=9;},RecoveryUseError::LevelDenied},
        {[](auto& s){s.cooldownRemainingMs=1;},RecoveryUseError::OnCooldown},
        {[](auto& s){s.availableUnits=0;},RecoveryUseError::InsufficientUnits},
        {[](auto& s){s.availableUnits=201;},RecoveryUseError::InvalidState},
    };
    for (const auto& [mutate,error] : denied)
    {
        auto invalid=state; mutate(invalid);
        const auto result=PlanInstantRecovery(*catalog,potion,invalid);
        Check(!result && result.error()==error, "invalid/denied admission produces no partial spend or heal plan");
    }
    auto gated=source;
    gated.recoveries[0].allowedContexts=1;
    gated.recoveries[0].allowDuringPvp=false;
    gated.usePolicies[2].useDuringCombat=false;
    gated.usePolicies[2].allowedClasses=1;
    gated.usePolicies[2].allowedSexes=1;
    const auto restricted=BuildItemCatalog(gated).value();
    for (const auto& [mutate,error] : std::vector<std::pair<std::function<void(InstantRecoveryAdmission&)>,RecoveryUseError>>{
        {[](auto& s){s.context=RecoveryContext::Arena;},RecoveryUseError::ContextDenied},
        {[](auto& s){s.inPvp=true;},RecoveryUseError::PvpDenied},
        {[](auto& s){s.inCombat=true;},RecoveryUseError::CombatDenied},
        {[](auto& s){s.classIndex=1;},RecoveryUseError::ClassDenied},
        {[](auto& s){s.sexIndex=1;},RecoveryUseError::SexDenied}})
    {
        auto deniedState=state; mutate(deniedState);
        Check(PlanInstantRecovery(*restricted,potion,deniedState).error()==error, "immutable policy is used during admission");
    }
    gated.items[6].retired=true;
    Check(PlanInstantRecovery(*BuildItemCatalog(gated).value(),potion,state).error()==RecoveryUseError::RetiredItem, "retired catalog rows cannot heal");
    auto badCooldown=state; badCooldown.cooldownRemainingMs=1;
    Check(PlanInstantRecovery(*BuildItemCatalog(noCooldown).value(),potion,badCooldown).error()==RecoveryUseError::InvalidState, "unrelated cooldown state cannot be silently accepted");
}
void NumericBounds()
{
    const auto key=item_catalog_fixture::InstantPotion();
    auto source=item_catalog_fixture::Catalog();
    auto& effect=std::get<InstantRecovery>(source.recoveries[0].effect);
    effect={MaxItemStatRaw,MaxItemStatRaw,10000,10000,100000};
    auto state=Admission(); state.pools={1,MaxItemStatRaw,0,MaxItemStatRaw}; state.potionBonusBp=100000;
    const auto largest=PlanInstantRecovery(*BuildItemCatalog(source).value(),key,state).value();
    Check(largest.hpRestoredRaw==MaxItemStatRaw-1 && largest.spRestoredRaw==MaxItemStatRaw, "largest legal raw values and multiplier cannot overflow before clamp");
    effect={99999,0,0,0,100000};
    const auto catalog=BuildItemCatalog(source).value();
    for (std::int32_t bp : {-10000,-9999,-5001,-1,0,1,3333,10000,99999,100000})
    {
        state.potionBonusBp=bp;
        const auto result=PlanInstantRecovery(*catalog,key,state);
        const auto expected=99999LL*(10000+bp)/10000;
        if (expected) Check(result && result->hpRestoredRaw==expected, "bounded direct-product oracle verifies exact floor including negative bonuses");
        else Check(!result && result.error()==RecoveryUseError::NoEffect, "zero rounded recovery has no resource side effect");
    }
    effect={0,0,3333,0,0}; state.potionBonusBp=0;
    Check(PlanInstantRecovery(*BuildItemCatalog(source).value(),key,state)->hpRestoredRaw==333300000000000LL,
          "large maximum-pool percentage is exact without floating point");
    effect={1,0,1,0,0}; state.pools={1,9999,0,0};
    Check(PlanInstantRecovery(*BuildItemCatalog(source).value(),key,state)->hpRestoredRaw==1, "flat and percent are floored separately");
}
void CatalogPublication()
{
    auto source=item_catalog_fixture::Catalog();
    const auto catalog=BuildItemCatalog(source).value();
    const auto key=item_catalog_fixture::InstantPotion();
    Check(catalog->FindRecovery(1)->Data().unitsPerUse==1 && !catalog->FindRecovery(0) && !catalog->FindRecovery(999), "recovery lookup has no implicit fallback");
    Check(catalog->FindConsumableCooldown(1)->durationMs==1000 && !catalog->FindConsumableCooldown(999), "cooldown profile is explicit catalog content");
    Check(catalog->RefinedBaseStats(key,0).value()==ItemBaseStats{} && !catalog->RefinedBaseStats(key,1), "consumable cannot borrow equipment refinement");
    const std::vector<std::pair<std::function<void(ItemCatalogSource&)>,ItemCatalogErrorCode>> invalid {
        {[](auto& s){s.recoveries[0].unitsPerUse=0;},ItemCatalogErrorCode::InvalidRecoveryProfile},
        {[](auto& s){s.recoveries[0].cooldownProfile=999;},ItemCatalogErrorCode::MissingProfile},
        {[](auto& s){s.consumableCooldowns[0].durationMs=0;},ItemCatalogErrorCode::InvalidProfile},
        {[](auto& s){s.consumableCooldowns.push_back(s.consumableCooldowns[0]);},ItemCatalogErrorCode::DuplicateProfile},
        {[](auto& s){s.recoveries.push_back(s.recoveries[0]);},ItemCatalogErrorCode::DuplicateProfile},
        {[](auto& s){s.consumables[0].recoveryProfile=0;},ItemCatalogErrorCode::MissingProfile},
        {[](auto& s){s.consumables.clear();},ItemCatalogErrorCode::MissingCategoryRow},
        {[](auto& s){s.consumables.push_back(s.consumables[0]);},ItemCatalogErrorCode::DuplicateCategoryRow},
        {[](auto& s){s.consumables[0].key=item_catalog_fixture::Sword();},ItemCatalogErrorCode::WrongCategory},
        {[](auto& s){s.items.pop_back();},ItemCatalogErrorCode::OrphanCategoryRow},
        {[](auto& s){s.items[6].refinementProfile=2;},ItemCatalogErrorCode::WrongRefinementKind},
        {[](auto& s){s.items[6].maxStack=0;},ItemCatalogErrorCode::InvalidBase},
        {[](auto& s){s.recoveries[0].unitsPerUse=201;},ItemCatalogErrorCode::InvalidBase},
        {[](auto& s){s.items[6].binding=BindingPolicy::OnEquip;},ItemCatalogErrorCode::InvalidBase},
        {[](auto& s){s.items[6].baseBonusProfile=1;},ItemCatalogErrorCode::InvalidBase},
        {[](auto& s){s.items[6].appearanceProfile=1;},ItemCatalogErrorCode::InvalidBase},
    };
    for (const auto& [mutate,error] : invalid)
    {
        auto broken=source; mutate(broken); const auto result=BuildItemCatalog(std::move(broken));
        Check(!result && result.error().code==error, "incomplete typed recovery catalog cannot publish");
    }
    const auto original=catalog->ContentHash();
    const std::vector<std::function<void(ItemCatalogSource&)>> changes {
        [](auto& s){++s.consumableCooldowns[0].durationMs;},
        [](auto& s){++s.recoveries[0].unitsPerUse;}, [](auto& s){s.recoveries[0].allowedContexts=1;},
        [](auto& s){s.recoveries[0].allowDuringPvp=false;}, [](auto& s){s.recoveries[0].cooldownProfile.reset();},
        [](auto& s){++std::get<InstantRecovery>(s.recoveries[0].effect).hpRaw;},
        [](auto& s){++std::get<InstantRecovery>(s.recoveries[0].effect).maxSpBp;},
        [](auto& s){++std::get<ReserveRecovery>(s.recoveries[1].effect).tickIntervalMs;},
        [](auto& s){++std::get<ReserveRecovery>(s.recoveries[1].effect).maxPoolPerTickBp;},
        [](auto& s){std::get<PeriodicRecovery>(s.recoveries[2].effect).durationMs+=1000;},
        [](auto& s){s.consumables[0].recoveryProfile=2;},
    };
    for (const auto& change : changes)
    {
        auto changed=source; change(changed);
        Check(BuildItemCatalog(std::move(changed)).value()->ContentHash()!=original, "recovery semantics participate in canonical identity");
    }
    std::ranges::reverse(source.recoveries); std::ranges::reverse(source.consumables);
    Check(BuildItemCatalog(source).value()->ContentHash()==original, "profile and item ordering cannot change canonical digest");
    source.recoveries.clear(); source.consumableCooldowns.clear();
    Check(catalog->FindRecovery(1)->Data().code=="recovery.instant", "snapshot owns all recovery metadata");
}
}
int main()
{
    try { Profiles(); UsePlans(); NumericBounds(); CatalogPublication(); std::cout<<"Consumable checks passed: "<<checks<<'\n'; return 0; }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
