#include "ItemCatalogFixture.hpp"
#include <item/AutomaticRuntime.hpp>
#include <item/EquipmentStats.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/RecoveryRuntime.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
static unsigned checks=0;
void Check(bool good,const char* message){++checks;if(!good)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 auto source=f::Catalog();std::get<AutomaticRecovery>(source.recoveries.back().effect).activeMaxPoolBonusBp=400;
 const auto c=BuildItemCatalog(source).value();const ItemOwner owner{ItemOwnerKind::Character,42};const ItemOwnerFence fence{owner,7,1};
 const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4}};const std::array ids{ItemInstanceId::Create(70).value(),ItemInstanceId::Create(71).value()};
 auto grant=MakeGrantCommit(*c,{20,1},fence,layouts,{},ItemGrant{f::AutomaticPotion(),0,2},ids);
 Check(grant && grant->items.size()==2,"automatic grant creates separate charged bottles");std::vector<PersistentItem> items;for(auto& i:grant->items)items.push_back(i.after);
 Check(items[0].automatic==AutomaticRecoveryState{5000000,false,0} && ValidateItemCommit(*c,*grant).has_value(),"initial charge is durable and full");
 InstantRecoveryAdmission actor;actor.pools={500000,2000000,200000,1000000};actor.context=RecoveryContext::World;actor.level=100;actor.availableUnits=1;
 auto toggled=PlanAutomaticRecoveryToggle(*c,items[0].key,items[0].id,AutomaticActor(*c,items,items[0],actor,false,true));Check(toggled && toggled->next.active,"activation admitted");items[0].automatic=toggled->next;
 auto stats=BuildEquipmentStats(*c,owner,items);Check(stats && stats->automaticHpBonusBp==400 && !stats->automaticSpBonusBp,"active inventory bottle projects exact max bonus");
 auto replacement=PlanAutomaticRecoveryToggle(*c,items[1].key,items[1].id,AutomaticActor(*c,items,items[1],actor,false,true));Check(replacement && replacement->deactivateOther==items[0].id,"same-pool replacement identifies prior instance");
 ItemCommit commit;commit.operationId={20,2};commit.catalogRevision=c->Revision();commit.catalogHash=c->ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};commit.outcomeCode=0x2003;
 RecoverySchedulePlan recovery;recovery.after.revision=1;
 Check(TickAutomaticOwner(*c,commit,layouts,items,actor,recovery,1000,false,true).has_value(),"automatic owner tick plans");Check(recovery.hpRestoredRaw==140000 && !recovery.spRestoredRaw && commit.items[0].after.automatic->remainingRaw==4860000,"only actual missing-pool credit debits charge");
 Check(ValidateItemCommit(*c,commit).has_value(),"tick is valid atomic item change");
 auto wire=EncodeTypedInventory(*c,layouts,{},items).value();Check(std::to_integer<unsigned>(wire[5])==2 && DecodeTypedInventory(wire,owner)->items[0].automatic==items[0].automatic,"inventory v2 preserves charge and state");
 Check(!MakeMoveCommit(*c,{20,3},fence,layouts,items,items[0].id,{ItemStorageKind::Inventory,1,8}),"active bottle cannot move");
 Check(!MakeConsumeCommit(*c,{20,4},fence,layouts,items,items[0].id,1),"active bottle cannot be consumed as an ingredient");
 auto paused=commit;paused.items.clear();paused.storages.clear();RecoverySchedulePlan noHeal;noHeal.after.revision=1;Check(TickAutomaticOwner(*c,paused,layouts,items,actor,noHeal,100000,true,true).has_value() && !noHeal.hpRestoredRaw && paused.items.empty(),"long paused tick never spends or catches up");
 auto timed=paused;Check(SettleAutomaticTime(timed,layouts,items,400).has_value() && timed.items[0].after.automatic->nextTickInMs==600,"unrelated schedule boundary retains timer phase");
 auto full=actor;full.pools.hpRaw=full.pools.maxHpRaw;RecoverySchedulePlan fullPlan;fullPlan.after.revision=1;auto fullCommit=paused;Check(TickAutomaticOwner(*c,fullCommit,layouts,items,full,fullPlan,1000,false,true).has_value() && fullCommit.items.empty() && !fullPlan.hpRestoredRaw,"full pool spends no charge");
 auto fractionActor=actor;fractionActor.pools.hpRaw=1999900;RecoverySchedulePlan fraction;fraction.after.revision=1;fraction.after.creditRaw[0]=90;auto fractionCommit=paused;Check(TickAutomaticOwner(*c,fractionCommit,layouts,items,fractionActor,fraction,1000,false,true).has_value() && fractionCommit.items[0].after.automatic->remainingRaw==4999990 && fraction.after.creditRaw[0]==100,"already earned fractional credit is reserved before spending new charge");
 RecoverySchedulePlan reserve;reserve.after.revision=1;reserve.after.generation=1;reserve.after.doses.push_back({1,f::ReservePotion(),RecoveryDelivery::Reserve,RecoveryPool::Hp,1500000,0,1000,1000,0,0,700});auto pendingCommit=paused;Check(TickAutomaticOwner(*c,pendingCommit,layouts,items,actor,reserve,1000,false,true).has_value() && !reserve.hpRestoredRaw && pendingCommit.items.empty(),"accepted reserve prevents duplicate charge spending");
 auto depleted=items;depleted[0].automatic->remainingRaw=500;auto emptyCommit=paused;RecoverySchedulePlan small;small.after.revision=1;Check(TickAutomaticOwner(*c,emptyCommit,layouts,depleted,actor,small,1000,false,true).has_value() && emptyCommit.items[0].after.destroyed && !emptyCommit.items[0].after.automatic->active && small.after.creditRaw[0]==500,"last fractional charge retained while bottle tombstones");Check(ValidateItemCommit(*c,emptyCommit).has_value(),"depletion is validated");
 auto forbidden=paused;RecoverySchedulePlan denied;denied.after.revision=1;Check(TickAutomaticOwner(*c,forbidden,layouts,items,actor,denied,1,false,false).has_value() && !forbidden.items[0].after.automatic->active,"owner rule denial deactivates immediately without charge");
 const auto packet=EncodeRecoveryRuntime(*c,{1,255,123,456,&items[0],actor.pools,{},0,items});Check(packet && std::to_integer<unsigned>((*packet)[5])==2,"recovery envelope v2 carries live bottles");
 if(argc==3 && std::string_view(argv[1])=="--golden-output"){std::ofstream out(argv[2]);for(auto b:*packet)out<<std::hex<<std::setw(2)<<std::setfill('0')<<std::to_integer<unsigned>(b);out<<'\n';}
 std::cout<<"Automatic runtime: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
