#include "ItemCatalogFixture.hpp"
#include <item/RecoverySchedule.hpp>
#include <item/PersistentItems.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <item/RecoveryRuntime.hpp>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
static int checks=0;
void Check(bool b,const char* msg){++checks;if(!b)throw std::runtime_error(msg);}
int main(int argc,char** argv){try{
 auto source=f::Catalog();source.recoveries[1].cooldownProfile.reset();const auto c=BuildItemCatalog(source).value();
 InstantRecoveryAdmission a;a.pools={500000,2000000,200000,1000000};a.context=RecoveryContext::World;a.level=50;a.availableUnits=5;
 RecoverySchedule empty;Check(ValidateRecoverySchedule(*c,empty),"empty");
 auto start=PlanScheduledRecoveryUse(*c,f::ReservePotion(),a,empty,0,{1,2}).value();
 Check(start.after.doses.size()==2 && start.units==1 && start.after.generation==1,"reserve both pools");
 auto early=PlanRecoveryScheduleTick(*c,start.after,999,a,false,false).value();Check(!early.hpRestoredRaw && early.after.doses[0].nextTickMs==1,"no early tick");
 auto tick=PlanRecoveryScheduleTick(*c,early.after,1,a,false,false).value();Check(tick.hpRestoredRaw==140000 && tick.spRestoredRaw==70000,"reserve maximum pool rate");
 auto missing=a;missing.pools.hpRaw=1999000;missing.pools.spRaw=missing.pools.maxSpRaw;
 auto clamped=PlanScheduledRecoveryUse(*c,f::ReservePotion(),missing,empty,0,{1,2}).value();Check(clamped.after.doses.size()==1 && clamped.after.doses[0].remainingRaw==1000,"clamp and skip full SP");
 auto full=a;full.pools.hpRaw=full.pools.maxHpRaw;full.pools.spRaw=full.pools.maxSpRaw;
 Check(PlanScheduledRecoveryUse(*c,f::ReservePotion(),full,empty,0,{1,2}).error()==RecoveryUseError::NoEffect,"full reserve refused");
 auto period=PlanScheduledRecoveryUse(*c,f::PeriodicPotion(),full,empty,0,{3,4}).value();Check(period.after.doses.size()==1,"periodic allowed full");
 auto replaced=PlanScheduledRecoveryUse(*c,f::PeriodicPotion(),a,period.after,700,{5,6}).value();Check(replaced.after.doses.size()==1 && replaced.after.doses[0].id==5 && replaced.after.doses[0].remainingMs==60000,"replace without stacking");
 auto delayed=PlanRecoveryScheduleTick(*c,period.after,60000,a,false,false).value();Check(delayed.hpRestoredRaw==1500000 && delayed.after.doses.empty(),"final tick and cap");
 auto stunned=PlanRecoveryScheduleTick(*c,start.after,4000,a,false,true).value();Check(!stunned.hpRestoredRaw && stunned.after.doses[0].remainingRaw==500000,"stun retains reserve without overdue healing");
 auto blocked=a;blocked.context=RecoveryContext::Arena;blocked.arena=RecoveryArenaAdmission{false,{}};
 Check(!PlanRecoveryScheduleTick(*c,start.after,1000,blocked,false,false)->hpRestoredRaw,"context rechecked");
 auto dead=PlanRecoveryScheduleTick(*c,start.after,1000,a,true,false).value();Check(dead.after.doses.empty() && !dead.hpRestoredRaw && dead.after.generation==2,"death clears no revival");
 Check(PlanRecoveryScheduleCancel(*c,start.after,500,0,0).error()==RecoveryUseError::InvalidState,"stale cancel");
 auto cancel=PlanRecoveryScheduleCancel(*c,start.after,500,1,1).value();Check(cancel.after.doses.size()==1 && cancel.after.doses[0].pool==RecoveryPool::Sp,"cancel individual");
 Check(PlanRecoveryScheduleCancel(*c,cancel.after,0,2,0)->after.doses.empty(),"cancel all");
 auto merged=PlanScheduledRecoveryUse(*c,f::ReservePotion(),a,start.after,500,{7,8}).value();Check(merged.after.doses.size()==2 && merged.after.doses[0].id==7 && merged.after.doses[0].nextTickMs==500,"merge preserves cadence rotates ID");
 for(const auto* s:{&empty,&start.after,&tick.after,&period.after,&dead.after,&merged.after}){
  auto blob=EncodeRecoverySchedule(*c,*s).value();Check(DecodeRecoverySchedule(*c,blob).value()==*s,"codec roundtrip");
  for(std::size_t n=0;n<blob.size();++n)Check(!DecodeRecoverySchedule(*c,std::span(blob).first(n)),"truncation");blob.push_back(0);Check(!DecodeRecoverySchedule(*c,blob),"trailing data");
 }
 auto malformed=start.after;malformed.doses[1].id=1;Check(!ValidateRecoverySchedule(*c,malformed),"duplicate IDs");
 auto cd=empty;Check(SetRecoveryCooldown(*c,cd,1,1000).has_value(),"set CD");cd.revision=1;Check(RecoveryCooldownRemaining(*AdvanceRecoverySchedule(*c,cd,500),1)==500,"durable countdown");Check(AdvanceRecoverySchedule(*c,cd,1000)->cooldowns.empty(),"cooldown expires");
 auto withCd=source;withCd.recoveries[1].cooldownProfile=1;auto cc=BuildItemCatalog(withCd).value();auto useCd=PlanScheduledRecoveryUse(*cc,f::ReservePotion(),a,empty,0,{1,2}).value();Check(PlanScheduledRecoveryUse(*cc,f::ReservePotion(),a,useCd.after,999,{3,4}).error()==RecoveryUseError::OnCooldown,"shared cooldown admission");
 auto small=a;small.pools={1000,11000,0,0};auto fraction=PlanScheduledRecoveryUse(*c,f::ReservePotion(),small,empty,0,{1,2}).value();auto ft=PlanRecoveryScheduleTick(*c,fraction.after,1000,small,false,false).value();Check(!ft.hpRestoredRaw && ft.after.creditRaw[0]==770,"subpoint preserved");auto ft2=PlanRecoveryScheduleTick(*c,ft.after,1000,small,false,false).value();Check(ft2.hpRestoredRaw==1000 && ft2.after.creditRaw[0]==540,"subpoint carried");
 const ItemOwner owner{ItemOwnerKind::Character,42};Check(RecoveryScheduleDigest(*c,owner,start.after)!=RecoveryScheduleDigest(*c,{ItemOwnerKind::Character,43},start.after),"owner bound digest");
 auto extreme=PlanRecoveryScheduleTick(*c,start.after,UINT64_MAX,a,false,false).value();Check(extreme.hpRestoredRaw==500000 && extreme.spRestoredRaw==200000,"saturating overdue multiplication");
 auto fifo=start.after;fifo.doses.push_back({3,f::ReservePotion(),RecoveryDelivery::Reserve,RecoveryPool::Hp,50000,0,2000,2000,0,0,900});
 auto firstOnly=PlanRecoveryScheduleTick(*c,fifo,1000,a,false,false).value();Check(firstOnly.hpRestoredRaw==140000 && firstOnly.after.doses.back().remainingRaw==50000 && firstOnly.after.doses.back().nextTickMs==2000,"FIFO profiles cannot heal in parallel");
 auto finishFirst=PlanRecoveryScheduleTick(*c,fifo,100000,a,false,false).value();Check(finishFirst.hpRestoredRaw==500000 && finishFirst.after.doses.size()==1 && finishFirst.after.doses[0].remainingRaw==50000 && finishFirst.after.doses[0].nextTickMs==2000,"next reserve retains full cadence after head exhaustion");
 auto crowded=empty;crowded.revision=1;crowded.generation=1;for(unsigned i=1;i<=8;++i)crowded.doses.push_back({i,f::ReservePotion(),RecoveryDelivery::Reserve,RecoveryPool::Hp,1000,0,i,i,0,0,700});
 Check(ValidateRecoverySchedule(*c,crowded) && PlanScheduledRecoveryUse(*c,f::ReservePotion(),a,crowded,0,{99,100}).error()==RecoveryUseError::QueueFull,"bounded reserve queue refuses entire use");
 auto resumed=PlanRecoveryScheduleTick(*c,stunned.after,1000,a,false,false).value();Check(resumed.hpRestoredRaw==140000 && resumed.spRestoredRaw==70000,"blocked time is discarded even across exact whole intervals");
 auto diedCd=PlanRecoveryScheduleTick(*cc,useCd.after,500,a,true,false).value();Check(diedCd.after.doses.empty() && diedCd.after.cooldowns.size()==1 && diedCd.after.cooldowns[0].remainingMs==500,"death clears effects but keeps cooldown");
 auto invalidContext=a;invalidContext.context=static_cast<RecoveryContext>(3);Check(!PlanRecoveryScheduleTick(*c,start.after,1000,invalidContext,false,false),"invalid context rejected");
 auto elapsedAfterReload=DecodeRecoverySchedule(*c,EncodeRecoverySchedule(*c,early.after).value()).value();Check(elapsedAfterReload.doses[0].nextTickMs==1 && PlanRecoveryScheduleTick(*c,elapsedAfterReload,1,a,false,false)->hpRestoredRaw==140000,"saved sub-tick phase resumes exactly");
 PersistentItem potion{ItemInstanceId::Create(70).value(),f::ReservePotion(),owner};potion.revision=1;potion.count=10;potion.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
 const auto packet=EncodeRecoveryRuntime(*c,{1,255,123,456,&potion,a.pools,start.after,0});Check(packet.has_value(),"native recovery envelope");Check(std::to_integer<unsigned>((*packet)[0])==242,"header avoids legacy time synchronization");
 if(argc==3 && std::string_view(argv[1])=="--golden-output"){std::ofstream out(argv[2]);for(auto b:*packet)out<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);out<<'\n';}
 std::cout<<"Recovery schedule: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
