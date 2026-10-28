#include "ItemCatalogFixture.hpp"
#include <functional>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f = item_catalog_fixture;
namespace
{
std::size_t checks = 0;
void Check(bool ok, const char* label, const std::source_location at = std::source_location::current())
{
    ++checks;
    if (!ok) throw std::runtime_error(std::string(label) + " line " + std::to_string(at.line()));
}
TimedEffectAdmission State() { return {10, 0, RecoveryContext::World, true, false, true, 50, 0, 0}; }
void ProfileValidation()
{
    const auto source = f::Catalog();
    Check(ValidTimedEffectGroup(source.timedEffectGroups[0]), "valid group");
    Check(ValidTimedEffectProfile(source.timedEffects[0]), "valid profile");
    for (auto mutate : std::vector<std::function<void(TimedEffectProfile&)>>{
        [](auto& p){p.id=0;}, [](auto& p){p.group=0;}, [](auto& p){p.code="bad..code";},
        [](auto& p){p.bonusProfile=0;}, [](auto& p){p.durationMs=0;}, [](auto& p){p.durationMs=UINT64_MAX;},
        [](auto& p){p.unitsPerUse=0;}, [](auto& p){p.allowedContexts=0;}, [](auto& p){p.allowedContexts=16;},
        [](auto& p){p.cooldownProfile=0;}})
    {
        auto p=source.timedEffects[0]; mutate(p); Check(!ValidTimedEffectProfile(p), "invalid profile rejected");
    }
    for (auto mutate : std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){s.timedEffects[0].bonusProfile=1;}, [](auto& s){s.timedEffects[0].bonusProfile=99;},
        [](auto& s){s.timedEffects[0].group=99;}, [](auto& s){s.timedEffects[0].cooldownProfile=99;},
        [](auto& s){s.timedEffects[0].unitsPerUse=201;}, [](auto& s){s.consumables[3].recoveryProfile=1;},
        [](auto& s){s.consumables[3].timedEffectProfile.reset();}, [](auto& s){s.consumables[3].timedEffectProfile=0;},
        [](auto& s){s.consumables[3].timedEffectProfile=99;},
        [](auto& s){s.timedEffectGroups[0].reapply=static_cast<TimedEffectReapply>(0);},
        [](auto& s){s.timedEffectGroups[0].clock=static_cast<TimedEffectClock>(3);},
        [](auto& s){for(auto& b:s.bonuses.back().entries)b.valueScaled=0;}})
    {
        auto s=source; mutate(s); Check(!BuildItemCatalog(std::move(s)), "invalid catalog never published");
    }
}
void CatalogIdentityAndCapacity()
{
    const auto source=f::Catalog();
    const auto hash=BuildItemCatalog(source).value()->ContentHash();
    for(auto mutate:std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){s.timedEffectGroups[0].reapply=TimedEffectReapply::Reject;},
        [](auto& s){s.timedEffectGroups[0].clock=TimedEffectClock::OnlineTime;},
        [](auto& s){s.timedEffectGroups[0].removeOnDeath=true;},
        [](auto& s){++s.timedEffects[0].durationMs;}, [](auto& s){s.timedEffects[0].unitsPerUse=2;},
        [](auto& s){s.timedEffects[0].allowedContexts=1;}, [](auto& s){s.timedEffects[0].allowDuringPvp=true;},
        [](auto& s){s.timedEffects[0].cooldownProfile.reset();}, [](auto& s){++s.bonuses.back().entries[0].valueScaled;}})
    {
        auto changed=source; mutate(changed);
        Check(BuildItemCatalog(std::move(changed)).value()->ContentHash()!=hash,"all timed semantics belong to content hash");
    }
    auto restricted=source;
    restricted.usePolicies[2].useDuringCombat=false;
    restricted.usePolicies[2].allowedClasses=1;
    restricted.usePolicies[2].allowedSexes=1;
    const auto catalog=BuildItemCatalog(std::move(restricted)).value();
    auto state=State();
    Check(PlanTimedEffectUse(*catalog,f::TimedPotion(),state,{}).error()==TimedEffectUseError::CombatDenied,"combat policy");
    state.inCombat=false; state.classIndex=1;
    Check(PlanTimedEffectUse(*catalog,f::TimedPotion(),state,{}).error()==TimedEffectUseError::ClassDenied,"class policy");
    state.classIndex=0; state.sexIndex=1;
    Check(PlanTimedEffectUse(*catalog,f::TimedPotion(),state,{}).error()==TimedEffectUseError::SexDenied,"sex policy");
    auto full=source; std::vector<ActiveTimedEffect> active;
    for(std::uint32_t i=2;i<=MaxActiveTimedEffects+1;++i)
    {
        full.timedEffectGroups.push_back({i,"effect.full_"+std::to_string(i),TimedEffectReapply::Reject,TimedEffectClock::OnlineTime,false});
        active.push_back({i,i,1});
    }
    const auto crowded=BuildItemCatalog(std::move(full)).value();
    Check(PlanTimedEffectUse(*crowded,f::TimedPotion(),State(),active).error()==TimedEffectUseError::ActiveLimit,"active capacity has no consume");
    active.back().group=1;
    Check(PlanTimedEffectUse(*crowded,f::TimedPotion(),State(),active).has_value(),"replacement needs no additional active slot");
    active.back().remainingMs=0;
    Check(PlanTimedEffectUse(*crowded,f::TimedPotion(),State(),active).has_value(),"expired slot frees capacity");
}
void Plans()
{
    auto catalog=BuildItemCatalog(f::Catalog()).value();
    auto result=PlanTimedEffectUse(*catalog,f::TimedPotion(),State(),{});
    Check(result.has_value(), "valid use");
    Check(result->bonuses.size()==3 && result->durationMs==60000 && result->unitsToConsume==1 &&
        result->clock==TimedEffectClock::WallTime && !result->removeOnDeath, "complete typed package");
    Check(result->cooldown && result->cooldown->durationMs==1000 && !result->replaceEffectId, "cooldown and no replacement");
    const ActiveTimedEffect active{42,1,1};
    result=PlanTimedEffectUse(*catalog,f::TimedPotion(),State(),std::span{&active,1});
    Check(result && result->replaceEffectId==42 && result->durationMs==60000, "replace full group; no duration addition");
    auto reject=f::Catalog(); reject.timedEffectGroups[0].reapply=TimedEffectReapply::Reject;
    auto noReplace=BuildItemCatalog(std::move(reject)).value();
    Check(PlanTimedEffectUse(*noReplace,f::TimedPotion(),State(),std::span{&active,1}).error()==TimedEffectUseError::AlreadyActive, "reject same group");
    auto expired=active; expired.remainingMs=0;
    Check(PlanTimedEffectUse(*noReplace,f::TimedPotion(),State(),std::span{&expired,1}).has_value(), "expired does not block");
    const ActiveTimedEffect invalids[]={{0,1,1},{1,99,1},{1,1,UINT64_MAX}};
    for(const auto& invalid:invalids)
        Check(PlanTimedEffectUse(*catalog,f::TimedPotion(),State(),std::span{&invalid,1}).error()==TimedEffectUseError::InvalidState,"invalid active record");
    const ActiveTimedEffect duplicateGroup[]={{1,1,1},{2,1,2}};
    const ActiveTimedEffect duplicateId[]={{1,1,0},{1,1,1}};
    Check(!PlanTimedEffectUse(*catalog,f::TimedPotion(),State(),duplicateGroup), "duplicate live group rejected");
    Check(!PlanTimedEffectUse(*catalog,f::TimedPotion(),State(),duplicateId), "duplicate id rejected even when expired");
    using E=TimedEffectUseError;
    for(auto [mutate,error]:std::vector<std::pair<std::function<void(TimedEffectAdmission&)>,E>>{
        {[](auto& s){s.alive=false;},E::DeadCharacter}, {[](auto& s){s.level=0;},E::LevelDenied},
        {[](auto& s){s.availableUnits=0;},E::InsufficientUnits}, {[](auto& s){s.availableUnits=201;},E::InvalidState},
        {[](auto& s){s.cooldownRemainingMs=1;},E::OnCooldown}, {[](auto& s){s.context=RecoveryContext::Arena;},E::ContextDenied},
        {[](auto& s){s.context=static_cast<RecoveryContext>(3);},E::InvalidState},
        {[](auto& s){s.classIndex=4;},E::InvalidState}, {[](auto& s){s.sexIndex=2;},E::InvalidState},
        {[](auto& s){s.inPvp=true;},E::PvpDenied}})
    {
        auto state=State(); mutate(state); const auto r=PlanTimedEffectUse(*catalog,f::TimedPotion(),state,{});
        Check(!r && r.error()==error,"admission denies without consume/grant");
    }
    Check(PlanTimedEffectUse(*catalog,f::Sword(),State(),{}).error()==E::WrongCategory,"equipment not consumable");
    Check(PlanTimedEffectUse(*catalog,f::InstantPotion(),State(),{}).error()==E::WrongEffect,"recovery not timed effect");
    InstantRecoveryAdmission recovery{};
    Check(PlanInstantRecovery(*catalog,f::TimedPotion(),recovery).error()==RecoveryUseError::WrongDelivery,"timed cannot enter recovery path");
    auto altered=f::Catalog(); for(auto& item:altered.items) if(item.key==f::TimedPotion()) item.retired=true;
    Check(PlanTimedEffectUse(*BuildItemCatalog(std::move(altered)).value(),f::TimedPotion(),State(),{}).error()==E::RetiredItem,"retired blocked");
    auto plan=*result; catalog.reset();
    Check(plan.bonuses[0].bonus==BonusId::MovementSpeedPercent,"plan owns effects after catalog released");
}
}
int main()
{
    try { ProfileValidation(); CatalogIdentityAndCapacity(); Plans(); std::cout<<checks<<" timed effect checks passed\n"; return 0; }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
