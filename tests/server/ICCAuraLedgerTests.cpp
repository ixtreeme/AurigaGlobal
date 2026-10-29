#include "../../SRC/Server/GameServer/raid/icc/AuraLedger.hpp"
#include <iostream>
#include <stdexcept>
using namespace EclipseRaid::ICC;
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
 AuraLedger book;AuraSpec impale{{69065,1,10},-1,1,{{0,1000,0}}};
 auto token=book.Apply(impale);Require(token.has_value(),"persistent impale applies");
 Require(book.Apply({{69674,2,10},12000,1,{{3000,0,0}}}).has_value(),"infection coexists with impale");
 Require(book.Size()==2,"different simultaneous debuffs retained");
 int impaleTicks=0,infectionTicks=0,expired=0;
 auto record=[&](const AuraEvent& e){if(e.removed){Require(e.reason==AuraRemoval::Expired,"natural expiry reason");expired++;}else if(e.aura.spec.key.spell==69065){Require(e.effect==1,"impale uses effect one");impaleTicks++;}else infectionTicks++;};
 book.Advance(12000,record);Require(impaleTicks==12&&infectionTicks==4&&expired==1&&book.Size()==1,"periodic timestamps and final tick on expiry");
 Require(book.RemoveTarget(10,AuraRemoval::Death)==1,"death removes permanent aura");
 int death=0;book.Advance(0,[&](const AuraEvent& e){Require(e.removed&&e.reason==AuraRemoval::Death,"death does not masquerade as dispel/expiry");death++;});Require(death==1,"one release callback");
 AuraSpec inoculation{{69291,2,10},40000,3,{}};book.Apply(inoculation,2);book.Apply(inoculation,2);Require(book.Find(inoculation.key)->stacks==3,"stack limit");
 book.Apply({{69291,3,10},40000,3,{}});Require(book.Size()==2,"caster ownership retained; spell handlers choose sharing");
 book.RemoveCaster(2,AuraRemoval::EncounterReset);book.Advance(0,[&](const AuraEvent& e){Require(e.removed&&e.reason==AuraRemoval::EncounterReset,"wipe cleanup reason");});Require(book.Size()==1,"other caster aura unchanged");
 AuraLedger refreshed;AuraSpec periodic{{1,1,1},5000,3,{{1000,0,0}}};refreshed.Apply(periodic);refreshed.Advance(900,[](auto&){});refreshed.Apply(periodic);int ticks=0;refreshed.Advance(100,[&](auto&){ticks++;});Require(ticks==1,"refresh preserves tick phase");
 periodic.resetPeriodicOnRefresh=true;refreshed.Apply(periodic);refreshed.Advance(999,[&](auto&){ticks++;});Require(ticks==1,"explicit refresh resets tick phase");refreshed.Advance(1,[&](auto&){ticks++;});Require(ticks==2,"reset tick due");
 AuraLedger reentrant;reentrant.Apply(impale);int callbacks=0;
 reentrant.Advance(10000,[&](const AuraEvent& e){callbacks++;if(!e.removed)reentrant.Remove(e.aura.spec.key,AuraRemoval::Script);});Require(callbacks==2&&reentrant.Size()==0,"callback removal cancels all later ticks without invalid iteration");
 AuraLedger bounded;bounded.Apply(impale);int count=0;Require(bounded.Advance(10000,[&](auto&){count++;},3)==3&&count==3,"bounded catchup work");bounded.Advance(0,[&](auto&){count++;});Require(count==10,"overdue ticks retained, not silently dropped");
 AuraLedger capacity;for(uint32_t i=1;i<=512;i++)Require(capacity.Apply({{1,1,i},-1,1,{}}).has_value(),"bounded slot admitted");Require(!capacity.Apply({{1,1,513},-1,1,{}}),"capacity rejects without replacing other effects");
 Require(capacity.RemoveTarget(1,AuraRemoval::Death)==1&&capacity.Apply({{1,1,513},-1,1,{}}),"released slot reusable");
 std::cout<<"ICC_AURA_LEDGER_PASS concurrent_effects periodic_expiry stacks ownership refresh death wipe reentrancy bounded_catchup\n";
}
