#include "PersistentItemFixture.hpp"
#include <item/WorldUtilityRuntime.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/EffectRuntime.hpp>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace {
unsigned checks=0;
void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
ItemKey Key(unsigned n){return ItemKey::Create(n==1 ? 11 : 10,62000+n).value();}
std::shared_ptr<const ItemCatalog> Catalog(){
    auto s=persistent_item_fixture::Catalog();
    const auto base=*std::ranges::find(s.items,item_catalog_fixture::InstantPotion(),&ItemBaseRow::key);
    for(unsigned n=1;n<=6;++n){auto b=base;b.key=Key(n);b.contentCode="world.utility_"+std::to_string(n);b.minimumLevel=0;b.maxStack=n==1 ? 1 : 20;s.items.push_back(b);}
    ToolLevel level;level.failure=ToolFailure::LosePoints;
    s.toolProfiles.push_back({62001,"tool.original_detector",{level}});s.tools.push_back({Key(1),ToolKind::Detector,62001,FishingPool::None,false,6});
    for(unsigned n=2;n<=6;++n)s.questItems.push_back({Key(n),QuestItemDispatch::FirstHandler,std::array{"utility.recall_town_restricted","utility.recall_memory_restricted","utility.recall_town","utility.recall_memory","utility.campfire"}[n-2],15,false});
    s.consumableCooldowns.push_back({62001,"utility.campfire",60000});return BuildItemCatalog(std::move(s)).value();
}
struct Owner {
    std::shared_ptr<const ItemCatalog> catalog=Catalog();
    ItemOwnerFence fence{persistent_item_fixture::Character(),7,1};
    std::array<ItemStorageLayout,1> layouts{persistent_item_fixture::Inventory()};
    std::vector<PersistentItem> items;RecoverySchedule recovery;
    std::uint64_t serial=0,identity=700000;
    auto Op(){return std::array<std::uint8_t,16>{0x50,static_cast<std::uint8_t>(++serial),static_cast<std::uint8_t>(serial>>8)};}
    void Apply(const ItemCommit& c){Check(ValidateItemCommit(*catalog,c).has_value(),"world plan remains a complete native commit");for(const auto& change:c.items){const auto i=std::ranges::find(items,change.after.id,&PersistentItem::id);if(change.after.destroyed){if(i!=items.end())items.erase(i);}else if(i==items.end())items.push_back(change.after);else *i=change.after;}if(c.recovery)recovery=c.recovery->after;++fence.revision;
        const auto bytes=EncodeTypedInventory(*catalog,layouts,{},items);Check(bytes.has_value(),"stateful utility inventory serializes");const auto back=DecodeTypedInventory(*bytes,fence.owner);Check(back.has_value() && back->items.size()==items.size(),"stateful utility inventory decodes");for(unsigned n=0;n<items.size();++n)Check(back->items[n].tool==items[n].tool && back->items[n].recall==items[n].recall,"exact detector and recall auxiliary state roundtrip");}
    ItemInstanceId Grant(unsigned key,unsigned count=1){auto id=ItemInstanceId::Create(++identity).value();Apply(MakeGrantCommit(*catalog,Op(),fence,layouts,items,ItemGrant{Key(key),0,count},std::array{id}).value());return id;}
    const PersistentItem& Find(ItemInstanceId id)const{return *std::ranges::find(items,id,&PersistentItem::id);}
    WorldUtilityFacts Facts(ItemKey key){WorldUtilityFacts f;f.character.level=100;f.position={21,10000,10000};f.empire=2;f.hp=f.maximumHp=1000;f.admitted=f.alive=f.mapLoaded=f.warpReady=f.summonZoneAllowed=f.savedDestinationAvailable=f.campfireSpotAllowed=f.campfirePrepared=true;f.town=RecallPosition{21,30000,30000};const auto* p=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);while(!(p->allowedClasses&(1u<<f.character.characterClass)))++f.character.characterClass;while(!(p->allowedSexes&(1u<<f.character.sex)))++f.character.sex;return f;}
    auto Use(ItemInstanceId id,WorldUtilityFacts f,std::optional<ItemInstanceId> split={},std::uint64_t now=100000){return MakeWorldUtilityCommit(*catalog,Op(),fence,layouts,items,recovery,id,f,0,now,split);}
};
void Detector(){
    Owner o;const auto id=o.Grant(1);auto facts=o.Facts(Key(1));facts.stoneVid=77;facts.stoneBearing=45;facts.stoneRange=2;
    Check(o.Find(id).tool->remainingUses==6,"detector factory initializes six independent uses");
    auto invalid=o.Find(id);invalid.tool->remainingUses.reset();Check(!ValidatePersistentItem(*o.catalog,invalid),"detector cannot load without a saved counter");invalid=o.Find(id);invalid.tool->remainingUses=7;Check(!ValidatePersistentItem(*o.catalog,invalid),"counter cannot exceed original capacity");
    for(unsigned use=1;use<=6;++use){auto plan=o.Use(id,facts).value();Check(plan.quote.remainingUses==6-use && plan.quote.consume==(use==6),"each admitted detector use spends one and only the sixth destroys");if(use==6)Check(plan.quote.stoneVid==0 && plan.quote.stoneRange==0 && plan.quote.stoneBearing==0,"final detector use clears its visible indicator");else Check(plan.quote.stoneVid==77,"detector keeps server-observed target data");o.Apply(*plan.commit);}
    Check(o.items.empty(),"sixth detector use leaves no live instance");
    Owner empty;const auto other=empty.Grant(1);auto noTarget=empty.Facts(Key(1));auto plan=empty.Use(other,noTarget).value();empty.Apply(*plan.commit);Check(empty.Find(other).tool->remainingUses==5,"search without a stone still spends its original use");noTarget.mapLoaded=false;Check(empty.Use(other,noTarget).error()==WorldUtilityError::Forbidden,"unloaded map never spends a detector use");
}
void Recall(){
    Owner o;const auto id=o.Grant(3,2);auto facts=o.Facts(Key(3));
    Check(o.Find(id).recall && !o.Find(id).recall->destination,"memory factory creates an explicit unstamped scroll");
    Check(o.Use(id,facts).error()==WorldUtilityError::NoIds,"stacked stamping needs a reserved native identity");
    const auto split=ItemInstanceId::Create(++o.identity).value();const auto first=o.Use(id,facts,split).value();Check(first.quote.action==WorldUtilityAction::Stamp && first.quote.hpCost==0 && first.quote.split,"memory first use only stamps exactly one unit");o.Apply(*first.commit);
    Check(o.Find(id).count==1 && !o.Find(id).recall->destination && o.Find(split).count==1 && o.Find(split).recall->destination==facts.position,"native split keeps old unit unbound and new destination durable");
    Check(o.Use(split,facts).error()==WorldUtilityError::TooNear,"saved destination distance gate is real rather than an all-zero placeholder");facts.position.x=20000;
    const auto back=o.Use(split,facts).value();Check(back.quote.action==WorldUtilityAction::WarpRecall && back.quote.hpCost==300 && back.quote.destination==RecallPosition{21,10000,10000},"memory return has original HP fee and exact saved target");o.Apply(*back.commit);Check(o.items.size()==1,"memory return consumes only its stamped unit");
    Owner town;const auto townId=town.Grant(2);auto actor=town.Facts(Key(2));auto enter=town.Use(townId,actor).value();Check(enter.quote.action==WorldUtilityAction::WarpTown && enter.quote.hpCost==300 && enter.quote.stamp==actor.position,"town first use stamps origin and charges original travel fee");town.Apply(*enter.commit);town.recovery.worldWarp.reset();actor.position.x=20000;const auto leave=town.Use(townId,actor).value();Check(leave.quote.hpCost==0 && leave.quote.consume,"original town return has no extra HP debit");
    Owner dungeon;const auto scroll=dungeon.Grant(2);auto d=dungeon.Facts(Key(2));d.inDungeon=true;auto exit=dungeon.Use(scroll,d).value();Check(exit.commit && exit.commit->recovery->after.worldWarp && !exit.quote.stamp && exit.quote.hpCost==300 && dungeon.Find(scroll).count==1,"dungeon town exit leaves its unstamped source unchanged");const auto memory=dungeon.Grant(3);d=dungeon.Facts(Key(3));d.inDungeon=true;Check(dungeon.Use(memory,d).error()==WorldUtilityError::DungeonDenied,"memory cannot stamp in a dungeon");
    Owner foreign;const auto source=foreign.Grant(5);auto f=foreign.Facts(Key(5));const auto stamped=foreign.Use(source,f).value();foreign.Apply(*stamped.commit);f.empire=1;f.position={1,50000,50000};const auto cleared=foreign.Use(source,f).value();Check(cleared.quote.action==WorldUtilityAction::ClearStamp && cleared.quote.hpCost==300,"original memory foreign-empire return clears stamp with its already-required HP fee");foreign.Apply(*cleared.commit);Check(!foreign.Find(source).recall->destination,"cleared source can be stamped again");
    for(unsigned gate=0;gate<8;++gate){Owner test;const auto scroll=test.Grant(2);auto f=test.Facts(Key(2));if(gate==0)f.hp=299;if(gate==1)f.arena=true;if(gate==2)f.warping=true;if(gate==3)f.warpReady=false;if(gate==4)f.summonZoneAllowed=false;if(gate==5)f.pvpPotionsBlocked=true;if(gate==6)f.position.map=200;if(gate==7)f.position.map=113;Check(!test.Use(scroll,f),"original recall health, world and portal gates fail before mutation");}
    Owner low;const auto memoryLow=low.Grant(5);auto h=low.Facts(Key(5));h.hp=300;const auto stampedLow=low.Use(memoryLow,h).value();low.Apply(*stampedLow.commit);h.position.x=20000;Check(low.Use(memoryLow,h)->quote.hpCost==200,"HP fee leaves at least original ten-percent floor");
    RecallItemState state{RecallPosition{21,100,200}};const auto blob=EncodeRecallItemState(state);Check(blob.size()==17 && DecodeRecallItemState(blob)==state,"typed saved destination roundtrips independently");for(unsigned n=0;n<blob.size();++n)Check(!DecodeRecallItemState(std::span(blob).first(n)),"every truncated recall payload rejects");
}
void Campfire(){
    Owner o;const auto id=o.Grant(6,2);auto facts=o.Facts(Key(6));facts.campfirePrepared=false;Check(o.Use(id,facts).error()==WorldUtilityError::SpawnUnprepared,"campfire cannot consume before a hidden actor is prepared");facts.campfirePrepared=true;
    const auto first=o.Use(id,facts).value();Check(first.commit && first.commit->recovery && first.quote.consume,"campfire consumes and saves UTC reuse deadline together");o.Apply(*first.commit);Check(o.Find(id).count==1 && o.recovery.worldCooldowns.size()==1 && WorldUtilityCooldownRemaining(*o.catalog,o.recovery,100000)==60000,"actual original world cooldown is sixty seconds");
    Check(o.Use(id,facts,{},159999).error()==WorldUtilityError::OnCooldown,"one millisecond before UTC deadline is denied without consumption");
    const auto data=EncodeRecoverySchedule(*o.catalog,o.recovery).value();Check(std::string(data.begin(),data.begin()+4)=="RCS4" && DecodeRecoverySchedule(*o.catalog,data).value()==o.recovery,"UTC world cooldown has its own exact persistent RCS4 state");for(unsigned n=0;n<data.size();++n)Check(!DecodeRecoverySchedule(*o.catalog,std::span(data).first(n)),"every truncated UTC cooldown packet rejects");
    const auto online=AdvanceRecoverySchedule(*o.catalog,o.recovery,500000).value();Check(online.worldCooldowns==o.recovery.worldCooldowns,"measured online time never subtracts from a UTC deadline");auto death=o.recovery;ItemEffectFacts dead;dead.alive=false;dead.allowed=true;NormalizeTimedItems(*o.catalog,death,100001,&dead);Check(death.worldCooldowns==o.recovery.worldCooldowns,"death does not clear campfire reuse deadline");
    auto invalid=o.recovery;invalid.worldCooldowns.push_back(invalid.worldCooldowns[0]);Check(!ValidateRecoverySchedule(*o.catalog,invalid),"duplicate world deadline profile rejects");
    auto next=o.Use(id,facts,{},160000).value();o.Apply(*next.commit);Check(o.items.empty() && WorldUtilityCooldownRemaining(*o.catalog,o.recovery,160000)==60000,"exact deadline admits next campfire and spends only final unit");
}
}
int main(){try{Detector();Recall();Campfire();std::cout<<"World utility runtime: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
