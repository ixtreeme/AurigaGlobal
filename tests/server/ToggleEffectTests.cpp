#include "CatalogSnapshotFixture.hpp"
#include <item/ToggleEffects.hpp>
#include <algorithm>
#include <functional>
#include <iostream>
#include <random>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f = item_catalog_fixture;
namespace sf = catalog_snapshot_fixture;
namespace
{
std::size_t checks = 0;
void Check(bool ok, const char* label, std::source_location at = std::source_location::current())
{
    ++checks;
    if (!ok) throw std::runtime_error(std::string(label) + " line " + std::to_string(at.line()));
}
ItemInstanceId Id(std::uint64_t n) { return ItemInstanceId::Create(n).value(); }
ItemKey OtherKey() { return ItemKey::Create(ItemCategory::Consumable, 90).value(); }
ItemCatalogSource Source()
{
    auto s = f::Catalog();
    s.toggleEffectGroups = {{1,"toggle.speed",false},{2,"toggle.independent",true}};
    s.toggleEffects = {{1,"toggle.first",1,7,1,60000,0,9,false,1},
                      {2,"toggle.opposite",1,7,2,90000,0,9,false,{}},
                      {3,"toggle.other",2,7,1,60000,0,9,false,{}}};
    auto& base = *std::ranges::find(s.items, f::TimedPotion(), &ItemBaseRow::key);
    base.maxStack = 1;
    auto& child = *std::ranges::find(s.consumables, f::TimedPotion(), &ItemConsumableRow::key);
    child.timedEffectProfile.reset(); child.toggleEffectProfile = 1;
    auto copy = base; copy.key = OtherKey(); copy.contentCode = "consumable.opposite";
    s.items.push_back(copy); s.consumables.push_back({OtherKey(),0,{},2});
    return s;
}
ToggleEffectAdmission Admission()
{
    ToggleEffectAdmission a;
    a.state = {60000,false}; a.availableUnits = 1; a.context = RecoveryContext::World;
    a.alive = true; a.effectAllowedByRules = true; a.level = 50;
    return a;
}
void CatalogAndSnapshot()
{
    const auto source = Source(); const auto catalog = BuildItemCatalog(source).value();
    Check(catalog->FindToggleEffect(1)->variant == 1, "typed profile lookup");
    Check(catalog->FindToggleEffectGroup(1)->code == "toggle.speed", "group lookup");
    Check(!catalog->FindToggleEffect(99), "no unknown profile");
    auto decoded = auriga::item::sql::DecodeSnapshot(sf::Snapshot(source), {1,catalog->ContentHash()});
    Check(decoded && (*decoded)->ContentHash() == catalog->ContentHash(), "typed SQL snapshot round trip");
    for (auto change : std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){s.toggleEffects[0].variant=0;}, [](auto& s){s.toggleEffects[0].requirements=4;},
        [](auto& s){s.toggleEffects[0].durationMs=0;}, [](auto& s){s.toggleEffects[0].durationMs=UINT64_MAX;},
        [](auto& s){s.toggleEffects[0].group=99;}, [](auto& s){s.toggleEffects[0].bonusProfile=1;},
        [](auto& s){s.toggleEffects[0].cooldownProfile=0;}, [](auto& s){s.toggleEffects[0].cooldownProfile=99;},
        [](auto& s){s.toggleEffects[0].allowedContexts=0;}, [](auto& s){s.toggleEffects[0].allowedContexts=16;},
        [](auto& s){s.consumables[3].timedEffectProfile=1;}, [](auto& s){s.consumables[3].toggleEffectProfile=0;},
        [](auto& s){s.consumables[3].toggleEffectProfile=99;},
        [](auto& s){auto i=std::ranges::find(s.items,f::TimedPotion(),&ItemBaseRow::key);i->maxStack=2;},
        [](auto& s){s.toggleEffectGroups.push_back(s.toggleEffectGroups[0]);}})
    {
        auto s=source;change(s);Check(!BuildItemCatalog(std::move(s)),"invalid toggle catalog rejected");
    }
    for (auto change : std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){s.toggleEffects[0].variant=3;}, [](auto& s){s.toggleEffects[0].requirements=3;},
        [](auto& s){++s.toggleEffects[0].durationMs;}, [](auto& s){s.toggleEffects[0].group=2;},
        [](auto& s){s.toggleEffects[0].cooldownProfile.reset();}, [](auto& s){s.toggleEffects[0].allowedContexts=1;},
        [](auto& s){s.toggleEffects[0].allowDuringPvp=true;}, [](auto& s){s.toggleEffectGroups[0].removeOnDeath=true;}})
    {
        auto s=source;change(s);auto result=BuildItemCatalog(std::move(s));
        Check(result && (*result)->ContentHash()!=catalog->ContentHash(),"every toggle rule affects digest");
    }
    using T=auriga::item::sql::Table;
    for(auto change:std::vector<std::function<void(auriga::item::sql::Snapshot&)>>{
        [](auto& s){s[T::ToggleEffects][0][4]="65536";}, [](auto& s){s[T::ToggleEffects][0][5]="-1";},
        [](auto& s){s[T::ToggleEffectGroups][0][2]="2";}, [](auto& s){s[T::ToggleEffects][0][9]="0";}})
    {auto s=sf::Snapshot(source);change(s);Check(!auriga::item::sql::DecodeSnapshot(std::move(s),{1,{}}),"strict wire value decoding");}
}
void UseAndConflicts()
{
    const auto c=BuildItemCatalog(Source()).value(); auto a=Admission();
    Check(InitialToggleEffectState(*c,f::TimedPotion()).value()==a.state,"acquisition initializes once");
    auto on=PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,{}).value();
    Check(on.step==ToggleEffectStep::Activate && on.next.active && on.next.remainingMs==60000 && !on.destroyItem,"activation never consumes charge or item");
    Check(on.cooldown && on.cooldown->durationMs>0 && !on.bonuses.empty(),"activation owns bonus and cooldown proposal");
    const std::vector<ActiveToggleEffect> active{{Id(10),f::TimedPotion(),60000}};
    auto error=PlanToggleEffectUse(*c,f::TimedPotion(),Id(11),a,active);
    Check(!error && error.error()==ToggleEffectError::AlreadyActive,"same variant another instance rejected");
    a.state.remainingMs=90000;
    auto replace=PlanToggleEffectUse(*c,OtherKey(),Id(11),a,active).value();
    Check(replace.deactivateOther==Id(10) && replace.next.active,"opposite variant replaces atomically");
    a=Admission(); a.state.active=true; a.alive=false;a.married=false;a.partyLeader=false;a.effectAllowedByRules=false;a.cooldownRemainingMs=10;
    auto off=PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,active).value();
    Check(!off.next.active && off.next.remainingMs==60000 && off.bonuses.empty() && !off.cooldown,"off allowed after access lost");
    Check(!PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,{}),"missing active binding rejected");
    auto corrupt=active;corrupt.push_back(active[0]);
    Check(!PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,corrupt),"duplicate bindings rejected");
    corrupt=active;corrupt[0].remainingMs=59999;
    Check(!PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,corrupt),"stale remaining charge rejected");
    a=Admission();a.effectAllowedByRules=false;
    Check(PlanToggleEffectUse(*c,f::TimedPotion(),Id(11),a,{}).error()==ToggleEffectError::RuleDenied,"explicit owner rules required");
    a=Admission();a.availableUnits=2;
    Check(PlanToggleEffectUse(*c,f::TimedPotion(),Id(11),a,{}).error()==ToggleEffectError::InvalidState,"stack splitting prohibited");
    for (auto [change,expected] : std::vector<std::pair<std::function<void(ToggleEffectAdmission&)>,ToggleEffectError>>{
        {[](auto& x){x.alive=false;},ToggleEffectError::DeadCharacter},
        {[](auto& x){x.inPvp=true;},ToggleEffectError::PvpDenied},
        {[](auto& x){x.context=RecoveryContext::Arena;},ToggleEffectError::ContextDenied},
        {[](auto& x){x.cooldownRemainingMs=1;},ToggleEffectError::OnCooldown},
        {[](auto& x){x.state.remainingMs=0;},ToggleEffectError::Exhausted}})
    {a=Admission();change(a);Check(PlanToggleEffectUse(*c,f::TimedPotion(),Id(11),a,{}).error()==expected,"admission rejection");}
    auto source=Source();source.toggleEffects[0].requirements=3;auto required=BuildItemCatalog(source).value();
    a=Admission();a.married=true;
    Check(PlanToggleEffectUse(*required,f::TimedPotion(),Id(11),a,{}).error()==ToggleEffectError::RequirementDenied,"all declared requirements needed");
    a.partyLeader=true;Check(bool(PlanToggleEffectUse(*required,f::TimedPotion(),Id(11),a,{})),"married leader accepted");
    a.state.active=true;a.partyLeader=false;
    Check(!PlanToggleEffectUse(*required,f::TimedPotion(),Id(10),a,active).value().next.active,"lost leader can still switch off");
    auto& base=*std::ranges::find(source.items,f::TimedPotion(),&ItemBaseRow::key);base.retired=true;
    auto retired=BuildItemCatalog(source).value();
    Check(!InitialToggleEffectState(*retired,f::TimedPotion()),"retired acquisition denied");
    Check(!PlanToggleEffectUse(*retired,f::TimedPotion(),Id(10),a,active).value().next.active,"retired active item can switch off");
    Check(PlanTimedEffectUse(*c,f::TimedPotion(),{},{}).error()==TimedEffectUseError::WrongEffect,"not consumed timed effect");
}
void ClockAndCleanup()
{
    auto source=Source();auto c=BuildItemCatalog(source).value();auto a=Admission();a.state.active=true;
    auto step=PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,1234).value();
    Check(step.next.remainingMs==58766 && step.next.active && step.bonuses.empty(),"exact active milliseconds, no repeated bonus");
    auto depleted=PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,UINT64_C(9223372036854775807)).value();
    Check(depleted.destroyItem && !depleted.next.active && !depleted.next.remainingMs && depleted.step==ToggleEffectStep::Expire,"long gap saturates once");
    Check(!PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,UINT64_MAX),"invalid elapsed rejected");
    a.state.active=false;auto idle=PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,60000).value();
    Check(idle.next==a.state && !idle.destroyItem,"inactive time never consumed");
    a.state.active=true;a.alive=false;
    Check(PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,1000).value().next.active,"persistent group survives death");
    source.toggleEffectGroups[0].removeOnDeath=true;c=BuildItemCatalog(source).value();
    auto death=PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,1000).value();
    Check(!death.next.active && death.next.remainingMs==59000 && !death.destroyItem,"death settles then deactivates preserving charge");
    a.alive=true;a.effectAllowedByRules=false;
    Check(!PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,1).value().next.active,"rule change cleans active binding");
    source.toggleEffects[0].requirements=1;c=BuildItemCatalog(source).value();a=Admission();a.state.active=true;
    Check(!PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,0).value().next.active,"lost marriage cleans active binding");
    std::mt19937_64 random(0x20261028);
    c=BuildItemCatalog(Source()).value();
    for(int i=0;i<1000;++i)
    {
        a=Admission();a.state={1+random()%60000,true};const auto elapsed=random()%120000;
        const auto p=PlanToggleEffectAdvance(*c,f::TimedPotion(),Id(10),a,elapsed).value();
        const auto expected=elapsed>=a.state.remainingMs?0:a.state.remainingMs-elapsed;
        Check(p.next.remainingMs==expected && p.destroyItem==(expected==0) && p.next.active==(expected>0),"independent clock and charge conservation");
    }
}
void CapacityAndIndependentGroups()
{
    auto s=Source(); std::vector<ActiveToggleEffect> active;
    const auto base=*std::ranges::find(s.items,f::TimedPotion(),&ItemBaseRow::key);
    for(std::uint32_t i=0;i<MaxActiveToggleEffects;++i)
    {
        const auto key=ItemKey::Create(ItemCategory::Consumable,5000+i).value();
        auto copy=base;copy.key=key;copy.contentCode="consumable.limit_"+std::to_string(i);
        s.items.push_back(copy);s.consumables.push_back({key,0,{},10+i});
        s.toggleEffectGroups.push_back({10+i,"toggle.group_"+std::to_string(i),false});
        s.toggleEffects.push_back({10+i,"toggle.profile_"+std::to_string(i),10+i,7,1,60000,0,1,false,{}});
        active.push_back({Id(100+i),key,60000});
    }
    const auto c=BuildItemCatalog(std::move(s)).value();auto a=Admission();
    Check(PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,active).error()==ToggleEffectError::ActiveLimit,"full active capacity rejected");
    active[0]={Id(100),OtherKey(),60000};
    Check(PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,active).value().deactivateOther==Id(100),"replacement permitted at full capacity");
    active.pop_back();
    active[0]={Id(100),ItemKey::Create(ItemCategory::Consumable,5000).value(),60000};
    Check(!PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,active).value().deactivateOther,"independent groups coexist");
    active.push_back(active[0]);
    Check(PlanToggleEffectUse(*c,f::TimedPotion(),Id(10),a,active).error()==ToggleEffectError::InvalidState,"duplicate item and group rejected");
}
}
int main()
{
    try {CatalogAndSnapshot();UseAndConflicts();ClockAndCleanup();CapacityAndIndependentGroups();std::cout<<checks<<" toggle effect checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
