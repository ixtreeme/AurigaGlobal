#include "ItemCatalogFixture.hpp"
#include <item/AutomaticRecovery.hpp>
#include <functional>
#include <iostream>
#include <random>
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
    if (!ok) throw std::runtime_error(std::string(label) + " at " + std::to_string(at.line()));
}
ItemInstanceId Id(std::uint64_t n = 101) { return ItemInstanceId::Create(n).value(); }
AutomaticRecoveryAdmission Off(const ItemCatalog& c)
{
    AutomaticRecoveryAdmission s;
    s.pools = {500000,2000000,200000,1000000}; s.availableUnits = 1;
    s.context = RecoveryContext::World; s.level = 50; s.recoveryAllowedByRules = true;
    s.state = InitialAutomaticRecoveryState(c,f::AutomaticPotion()).value(); return s;
}
AutomaticRecoveryAdmission On(const ItemCatalog& c)
{
    auto s=Off(c); s.state=PlanAutomaticRecoveryToggle(c,f::AutomaticPotion(),Id(),s)->next;
    s.activeForPool=Id(); return s;
}
void Profiles()
{
    const auto original=f::Catalog();
    for (auto mutate : std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).pool=static_cast<RecoveryPool>(0);},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).supply=static_cast<RecoverySupply>(0);},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).capacityRaw=0;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).capacityRaw=-1;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).capacityRaw=MaxItemStatRaw+1;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).supply=RecoverySupply::Unlimited;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).tickIntervalMs=0;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).tickIntervalMs=UINT64_MAX;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).maxPoolPerTickBp=0;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).maxPoolPerTickBp=10001;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).activeMaxPoolBonusBp=10001;},
        [](auto& s){s.recoveries.back().unitsPerUse=2;}, [](auto& s){s.recoveries.back().allowedContexts=15;},
        [](auto& s){s.items.back().maxStack=2;}, [](auto& s){s.recoveries.back().arenaQuotaCost=1;}})
    {
        auto s=original; mutate(s); Check(!BuildItemCatalog(std::move(s)),"invalid automatic catalogue rejected");
    }
    const auto hash=BuildItemCatalog(original).value()->ContentHash();
    for(auto mutate:std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).pool=RecoveryPool::Sp;},
        [](auto& s){auto& e=std::get<AutomaticRecovery>(s.recoveries.back().effect); e.supply=RecoverySupply::Unlimited;e.capacityRaw=0;},
        [](auto& s){++std::get<AutomaticRecovery>(s.recoveries.back().effect).capacityRaw;},
        [](auto& s){++std::get<AutomaticRecovery>(s.recoveries.back().effect).tickIntervalMs;},
        [](auto& s){++std::get<AutomaticRecovery>(s.recoveries.back().effect).maxPoolPerTickBp;},
        [](auto& s){std::get<AutomaticRecovery>(s.recoveries.back().effect).activeMaxPoolBonusBp=400;},
        [](auto& s){s.recoveries[0].arenaQuotaCost=2;}})
    {
        auto s=original; mutate(s); Check(BuildItemCatalog(std::move(s)).value()->ContentHash()!=hash,"automatic and quota semantics hashed");
    }
}
void Toggle()
{
    auto source=f::Catalog(); const auto c=BuildItemCatalog(source).value();
    auto s=Off(*c); const auto plan=PlanAutomaticRecoveryToggle(*c,f::AutomaticPotion(),Id(),s).value();
    Check(!s.state.active && s.state.remainingRaw==5000000,"proposal does not mutate instance");
    Check(plan.next.active && plan.next.remainingRaw==5000000 && plan.next.nextTickInMs==1000,"activate with full charge");
    Check(plan.cooldown && plan.cooldown->durationMs==1000 && !plan.deactivateOther,"activation cooldown");
    auto active=On(*c); active.pools.hpRaw=0; active.stunned=true;active.context=RecoveryContext::Arena;active.cooldownRemainingMs=50;active.recoveryAllowedByRules=false;
    auto off=PlanAutomaticRecoveryToggle(*c,f::AutomaticPotion(),Id(),active);
    Check(off && !off->next.active && off->next.nextTickInMs==0 && !off->cooldown,"off is always possible after valid ownership");
    s.activeForPool=Id(202);
    Check(PlanAutomaticRecoveryToggle(*c,f::AutomaticPotion(),Id(),s)->deactivateOther==Id(202),"replace another instance atomically");
    s.activeForPool=Id();
    Check(PlanAutomaticRecoveryToggle(*c,f::AutomaticPotion(),Id(),s).error()==AutomaticRecoveryError::InvalidState,"inactive state cannot also own active pool");
    using E=AutomaticRecoveryError;
    for(auto [mutate,error]:std::vector<std::pair<std::function<void(AutomaticRecoveryAdmission&)>,E>>{
        {[](auto& s){s.pools.hpRaw=0;},E::DeadCharacter}, {[](auto& s){s.stunned=true;},E::Stunned},
        {[](auto& s){s.context=RecoveryContext::Arena;},E::ContextDenied}, {[](auto& s){s.inPvp=true;},E::PvpDenied},
        {[](auto& s){s.recoveryAllowedByRules=false;},E::RuleDenied},
        {[](auto& s){s.cooldownRemainingMs=1;},E::OnCooldown}, {[](auto& s){s.level=0;},E::LevelDenied},
        {[](auto& s){s.state.remainingRaw=0;},E::Exhausted}, {[](auto& s){s.state.remainingRaw=-1;},E::InvalidState},
        {[](auto& s){s.state.remainingRaw=5000001;},E::InvalidState}, {[](auto& s){s.availableUnits=2;},E::InvalidState},
        {[](auto& s){s.state.nextTickInMs=1;},E::InvalidState}, {[](auto& s){s.context=static_cast<RecoveryContext>(3);},E::InvalidState},
        {[](auto& s){s.classIndex=4;},E::InvalidState}, {[](auto& s){s.sexIndex=2;},E::InvalidState}})
    {
        s=Off(*c);mutate(s);auto denied=PlanAutomaticRecoveryToggle(*c,f::AutomaticPotion(),Id(),s);
        Check(!denied && denied.error()==error,"activation admission denies safely");
    }
    Check(InitialAutomaticRecoveryState(*c,f::InstantPotion()).error()==E::WrongDelivery,"instant potion is not charged");
    Check(InitialAutomaticRecoveryState(*c,f::Sword()).error()==E::WrongCategory,"equipment not bottle");
    source.items.back().retired=true; const auto retired=BuildItemCatalog(std::move(source)).value();
    Check(!InitialAutomaticRecoveryState(*retired,f::AutomaticPotion()),"retired bottle not acquired");
    Check(PlanAutomaticRecoveryToggle(*retired,f::AutomaticPotion(),Id(),On(*c))->next.active==false,"retired active bottle can turn off");
}
void Ticks()
{
    const auto c=BuildItemCatalog(f::Catalog()).value();
    AutomaticRecoveryTickAdmission input{On(*c),0,0,399};
    auto step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(step.step==AutomaticRecoveryStep::Idle && step.next.nextTickInMs==601,"before deadline no heal");
    input.actor.state=step.next;input.elapsedMs=600;
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(step.next.nextTickInMs==1 && step.hpRestoredRaw==0,"fractional scheduling exact");
    input.actor.state=step.next;input.elapsedMs=1;
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(step.hpRestoredRaw==140000 && !step.spRestoredRaw && step.next.remainingRaw==4860000,"bounded HP charge conservation");
    Check(step.next.nextTickInMs==1000 && !step.destroyItem,"next deadline reset");
    input={On(*c),1499999,0,1000};
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(step.hpRestoredRaw==1 && step.next.remainingRaw==4999999,"pending recovery not charged twice");
    input.pendingHpRaw=MaxItemStatRaw;
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(!step.hpRestoredRaw && step.next.remainingRaw==5000000,"huge pending cannot overflow or waste charge");
    input={On(*c),0,0,INT64_MAX}; input.actor.state.remainingRaw=13;
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(step.hpRestoredRaw==13 && step.destroyItem && !step.next.active && !step.next.remainingRaw,"exhaustion is heal+deactivate+destroy proposal");
    input={On(*c),0,0,INT64_MAX};
    Check(PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input)->hpRestoredRaw==140000,"long pause causes one tick only");
    input.actor.stunned=true;
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(!step.hpRestoredRaw && step.next.active && step.next.remainingRaw==5000000 && step.next.nextTickInMs==1000,"stun discards due tick without charge");
    input.actor.stunned=false;input.actor.pools.hpRaw=0;
    Check(!PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input)->hpRestoredRaw,"dead character not healed");
    input={On(*c),0,0,0}; input.actor.context=RecoveryContext::Arena;
    step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
    Check(step.step==AutomaticRecoveryStep::Deactivate && !step.next.active && !step.destroyItem,"forbidden map turns off, preserves charge");
    input={On(*c),0,0,1000}; input.actor.recoveryAllowedByRules=false;
    Check(PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input)->step==AutomaticRecoveryStep::Deactivate,"global/duel rules stop an active bottle");
    for(auto mutate:std::vector<std::function<void(AutomaticRecoveryTickAdmission&)>>{
        [](auto& s){s.pendingHpRaw=-1;}, [](auto& s){s.pendingSpRaw=MaxItemStatRaw+1;}, [](auto& s){s.elapsedMs=UINT64_MAX;},
        [](auto& s){s.actor.state.nextTickInMs=0;}, [](auto& s){s.actor.state.nextTickInMs=1001;},
        [](auto& s){s.actor.activeForPool=Id(202);}, [](auto& s){s.actor.pools.hpRaw=2000001;}})
    {
        input={On(*c),0,0,1000};mutate(input);Check(!PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input),"invalid tick input rejected");
    }
    auto unlimited=f::Catalog();auto& e=std::get<AutomaticRecovery>(unlimited.recoveries.back().effect);
    e.pool=RecoveryPool::Sp;e.supply=RecoverySupply::Unlimited;e.capacityRaw=0;e.activeMaxPoolBonusBp=400;
    const auto endless=BuildItemCatalog(std::move(unlimited)).value();
    input={On(*endless),0,0,1000};
    step=PlanAutomaticRecoveryTick(*endless,f::AutomaticPotion(),Id(),input).value();
    Check(step.spRestoredRaw==70000 && !step.hpRestoredRaw && step.next.remainingRaw==0 && !step.destroyItem,"unlimited SP explicit, no phantom charge");
    Check(PlanAutomaticRecoveryToggle(*endless,f::AutomaticPotion(),Id(),Off(*endless))->activeMaxPoolBonusBp==400,"max pool bonus explicit");
    // Independent uint64 arithmetic oracle: auto bp<=10000, product<=10^19
    // fits unsigned 64 bits, while the signed implementation must not multiply.
    std::mt19937_64 random(51);
    for(unsigned i=0;i<1000;++i)
    {
        input={On(*c),0,0,1000};auto& p=input.actor.pools;
        p.maxHpRaw=1+static_cast<std::int64_t>(random()%MaxItemStatRaw);p.hpRaw=1+static_cast<std::int64_t>(random()%p.maxHpRaw);
        input.pendingHpRaw=static_cast<std::int64_t>(random()%MaxItemStatRaw);
        const auto missing=p.maxHpRaw-p.hpRaw;
        const auto want=std::min<std::uint64_t>(std::min(missing, missing-std::min(missing,input.pendingHpRaw)),
            std::min<std::uint64_t>(static_cast<std::uint64_t>(p.maxHpRaw)*700/10000,input.actor.state.remainingRaw));
        step=PlanAutomaticRecoveryTick(*c,f::AutomaticPotion(),Id(),input).value();
        Check(step.hpRestoredRaw==want && step.hpRestoredRaw+step.next.remainingRaw==input.actor.state.remainingRaw,"independent arithmetic and conservation");
    }
}
void Arena()
{
    using E=RecoveryUseError;
    Check(PlanRecoveryArena(RecoveryContext::World,{},2).value()==0,"world does not consume arena quota");
    Check(PlanRecoveryArena(RecoveryContext::Arena,RecoveryArenaAdmission{true,{}},2).value()==0,"unlimited match does not decrement");
    Check(PlanRecoveryArena(RecoveryContext::Arena,RecoveryArenaAdmission{true,2},2).value()==2,"exact quota accepted");
    Check(PlanRecoveryArena(RecoveryContext::Arena,RecoveryArenaAdmission{true,1},2).error()==E::ArenaQuotaExceeded,"insufficient quota denied");
    Check(PlanRecoveryArena(RecoveryContext::Arena,RecoveryArenaAdmission{false,{}},0).error()==E::ArenaDenied,"global ban includes quota-exempt items");
    Check(!PlanRecoveryArena(RecoveryContext::Arena,{},0),"missing arena state fails closed");
    Check(!PlanRecoveryArena(RecoveryContext::World,RecoveryArenaAdmission{true,9},1),"arena state cannot leak into world");
    auto source=f::Catalog();source.recoveries[0].arenaQuotaCost=2;
    const auto c=BuildItemCatalog(source).value();InstantRecoveryAdmission s;
    s.pools={1,1000000,0,1000000};s.context=RecoveryContext::Arena;s.level=50;s.availableUnits=1;s.arena=RecoveryArenaAdmission{true,2};
    const auto plan=PlanInstantRecovery(*c,f::InstantPotion(),s).value();
    Check(plan.arenaQuotaToConsume==2 && s.arena->remainingUses==2,"quota only in successful proposal, no mutation");
    s.pools.hpRaw=s.pools.maxHpRaw;s.pools.spRaw=s.pools.maxSpRaw;
    Check(PlanInstantRecovery(*c,f::InstantPotion(),s).error()==E::NoEffect,"full pools do not consume quota");
    source.recoveries[0].allowedContexts=1;
    Check(!BuildItemCatalog(std::move(source)),"quota requires Arena-enabled profile");
}
}
int main()
{
    try {Profiles();Toggle();Ticks();Arena();std::cout<<checks<<" automatic recovery checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
