#include "CatalogSnapshotFixture.hpp"
#include <item/GatheringItems.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <source_location>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace db=auriga::item::sql;
namespace
{
unsigned checks=0;
void Check(bool value,const char* message,std::source_location loc=std::source_location::current())
{++checks;if(!value)throw std::runtime_error(std::string(message)+" line "+std::to_string(loc.line()));}
ItemKey Tool(){return ItemKey::Create(11,1).value();}
ItemKey Fish(unsigned local){return ItemKey::Create(12,local).value();}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();
    RefinementProfile refine{5,"refine.tool",RefinementKind::Tool,30,{}};
    ToolProfile training{1,"training.rod",{}};
    for(std::uint16_t i=0;i<=30;++i)
    {
        refine.levels.push_back({i,{},{}});
        training.levels.push_back({i,std::uint32_t(10+i),std::uint16_t(i*100),i==30?0u:100u,i==30?0u:1u,i==30?1u:5u,
            std::uint16_t(i==30?0:8000),ToolFailure::LosePoints,{},std::uint16_t(i==30?0:1000)});
    }
    s.refinements.push_back(refine);s.toolProfiles.push_back(training);
    for(auto [key,code]:{std::pair{Tool(),"rod"},std::pair{Fish(1),"live"},std::pair{Fish(2),"dead"},std::pair{Fish(3),"bait"}})
    {
        auto base=s.items[3];base.key=key;base.contentCode=std::string(key==Tool()?"tool.":"fishing.")+code;
        base.nameKey=std::string("item.")+code+".name";base.descriptionKey=std::string("item.")+code+".description";
        if(key==Tool()){base.maxStack=1;base.refinementProfile=5;base.usePolicy=1;}
        s.items.push_back(base);
    }
    s.tools={{Tool(),ToolKind::Rod,1,FishingPool::Second}};
    s.fishingItems={{Fish(1),FishingKind::LiveFish,3,{},0},{Fish(2),FishingKind::DeadFish,{},f::InstantPotion(),0},
                   {Fish(3),FishingKind::Bait,{},{},20}};
    s.lootProfiles.push_back({3,"loot.fish",LootPickMode::WeightedOne,{{1,LootRewardKind::Item,Fish(2),1,31,0},{2,LootRewardKind::Item,f::Leather(),1,69,0}}});
    return s;
}
GatheringContext Context(){GatheringContext c;c.actorId=7;c.rulesetRevision=2;c.operationId[0]=1;c.characterLevel=50;c.admittedByServer=true;return c;}
GatheringItemSnapshot Snapshot(ItemKey key,std::uint64_t instance=100){return {key,instance,5,7,0,1,false,false,false};}
ToolActionRequest Request(){ToolActionRequest r{Context(),Snapshot(Tool()),ToolAction::Practice,99,1};r.tool.equipped=true;return r;}
void Training()
{
    auto source=Fixture();auto catalog=BuildItemCatalog(source).value();auto r=Request();
    auto p=PlanToolAction(*catalog,r).value();Check(p.afterPoints==100 && p.afterLevel==0 && p.success,"one practice point caps exactly");
    Check(p.catalog.hash==catalog->ContentHash() && p.expected.context.operationId==r.context.operationId,"owning operation and catalog pin");
    r.recordedRoll=5;Check(PlanToolAction(*catalog,r)->afterPoints==99,"exact 1/5 failure without RNG call");
    r.trainingPoints=100;r.recordedRoll=0;Check(!PlanToolAction(*catalog,r)->success,"full training is a no-op");
    r.recordedRoll=1;Check(PlanToolAction(*catalog,r).error()==GatheringError::InvalidDecision,"no-op rejects stray recorded draw");
    r=Request();r.action=ToolAction::Refine;r.tool.equipped=false;r.trainingPoints=100;r.tool.refinementLevel=10;r.recordedRoll=8000;
    auto success=PlanToolAction(*catalog,r).value();Check(success.success && success.afterLevel==11 && success.afterPoints==0,"same prototype refines above +9");
    r.recordedRoll=8001;auto failure=PlanToolAction(*catalog,r).value();Check(!failure.success && failure.afterLevel==10 && failure.afterPoints==90,"explicit loss at same level");
    auto fractional=source;fractional.toolProfiles[0].levels[10].requiredPoints=19;
    r.trainingPoints=19;Check(PlanToolAction(*BuildItemCatalog(fractional).value(),r)->afterPoints==18,"loss rounds down before subtraction");
    auto downgrade=source;auto& level=downgrade.toolProfiles[0].levels[10];level.failure=ToolFailure::ResetAtLevel;level.failureLevel=2;level.failureLossBp=0;
    r.trainingPoints=100;auto down=PlanToolAction(*BuildItemCatalog(downgrade).value(),r).value();
    Check(down.afterLevel==2 && down.afterPoints==0,"rod downgrade resets training without another prototype");
    r.tool.refinementLevel=30;r.trainingPoints=0;r.recordedRoll=0;Check(PlanToolAction(*catalog,r).error()==GatheringError::AtMaximum,"configured maximum blocks refinement");
    r.action=ToolAction::Practice;r.tool.equipped=true;Check(PlanToolAction(*catalog,r)->afterLevel==30,"maximum cannot train further");
    for(unsigned test=0;test<14;++test)
    {
        auto r=Request();auto expected=GatheringError::InvalidSnapshot;
        switch(test){
        case 0:r.context.admittedByServer=false;expected=GatheringError::Forbidden;break;
        case 1:r.context.operationId={};expected=GatheringError::InvalidOperation;break;
        case 2:r.context.rulesetRevision=0;expected=GatheringError::InvalidOperation;break;
        case 3:r.tool.ownerId=8;break;case 4:r.tool.revision=UINT64_MAX;break;case 5:r.tool.locked=true;break;
        case 6:r.tool.exchanging=true;break;case 7:r.tool.count=2;expected=GatheringError::InvalidStack;break;
        case 8:r.tool.refinementLevel=31;expected=GatheringError::InvalidLevel;break;
        case 9:r.trainingPoints=101;expected=GatheringError::InvalidState;break;
        case 10:r.recordedRoll=0;expected=GatheringError::InvalidDecision;break;
        case 11:r.recordedRoll=6;expected=GatheringError::InvalidDecision;break;
        case 12:r.tool.equipped=false;break;
        case 13:r.action=ToolAction::Refine;r.tool.equipped=false;expected=GatheringError::NotReady;break;}
        Check(PlanToolAction(*catalog,r).error()==expected,"invalid tool action rejected");
    }
    auto large=source;large.toolProfiles[0].levels[0].requiredPoints=UINT32_MAX;
    r=Request();r.trainingPoints=UINT32_MAX-1;Check(PlanToolAction(*BuildItemCatalog(large).value(),r)->afterPoints==UINT32_MAX,"last practice point cannot overflow");
    for(unsigned numerator:{0u,5u})
    {auto endpoint=source;endpoint.toolProfiles[0].levels[0].practiceNumerator=numerator;r=Request();r.recordedRoll=0;
     Check(PlanToolAction(*BuildItemCatalog(endpoint).value(),r)->success==(numerator!=0),"explicit practice endpoints require no draw");}
    for(unsigned chance:{0u,10000u})
    {
        auto endpoint=source;endpoint.toolProfiles[0].levels[0].refineChanceBp=static_cast<std::uint16_t>(chance);
        r=Request();r.action=ToolAction::Refine;r.tool.equipped=false;r.trainingPoints=100;r.recordedRoll=0;
        Check(PlanToolAction(*BuildItemCatalog(endpoint).value(),r)->success==(chance!=0),"explicit refinement endpoints require no draw");
        r.recordedRoll=1;Check(!PlanToolAction(*BuildItemCatalog(endpoint).value(),r),"endpoints reject extraneous draws");
    }
    auto recipe=RefinementRecipe{1,"tool.bypass",Tool(),0,Tool(),1,0,10000,RefinementFailure::Keep,{}, {}};
    Check(!ValidateRefinementRecipe(recipe,*catalog),"ordinary smith recipes cannot bypass tool training");
    catalog.reset();Check(p.expected.tool.item==Tool() && p.afterPoints==100,"plan retains all input after catalog destruction");
}
void Fishing()
{
    auto catalog=BuildItemCatalog(Fixture()).value();FishProcessRequest r{Context(),Snapshot(Fish(1)),FishAction::Use,false};r.fish.count=200;
    auto dead=PlanFishProcess(*catalog,r,[](auto){return 0u;}).value();
    Check(dead.consumeUnits==1 && dead.yields[0].item==Fish(2) && dead.yields[0].count==1,"live fish consumes one and grants dead item");
    Check(PlanFishProcess(*catalog,r,[](auto){return 31u;})->yields[0].item==f::Leather(),"weighted boundary selects alternative");
    Check(PlanFishProcess(*catalog,r,[](auto){return 100u;}).error()==GatheringError::InvalidDecision,"out of range draw rejected");
    Check(PlanFishProcess(*catalog,r,{}).error()==GatheringError::InvalidDecision,"no implicit fish RNG");
    r.fish.item=Fish(2);r.action=FishAction::Grill;
    Check(PlanFishProcess(*catalog,r,{}).error()==GatheringError::CampfireRequired,"grill requires admitted campfire");
    r.campfireAvailable=true;auto grilled=PlanFishProcess(*catalog,r,{}).value();
    Check(grilled.consumeUnits==200 && grilled.yields[0].count==200 && grilled.yields[0].item==f::InstantPotion(),"grill preserves the full stack");
    auto smaller=Fixture();smaller.items[6].maxStack=100;
    Check(PlanFishProcess(*BuildItemCatalog(smaller).value(),r,{})->yields[0].count==200,"owner must split result by target stack cap, never truncate");
    r.fish.count=201;Check(PlanFishProcess(*catalog,r,{}).error()==GatheringError::InvalidStack,"oversized source stack rejected");
    r.fish.count=1;r.fish.item=Fish(1);Check(PlanFishProcess(*catalog,r,{}).error()==GatheringError::WrongAction,"live fish cannot be grilled as dead fish");
    BaitRequest b{Context(),Snapshot(Fish(3),101),Snapshot(Tool()),5,false};b.rod.equipped=true;b.bait.count=200;
    auto bait=PlanApplyBait(*catalog,b).value();Check(bait.consumeUnits==1 && bait.afterBaitPower==20,"one bait replaces, does not add power");
    b.fishingActive=true;Check(PlanApplyBait(*catalog,b).error()==GatheringError::FishingActive,"cannot replace during fishing");
    b.fishingActive=false;b.rod.ownerId=8;Check(!PlanApplyBait(*catalog,b),"cannot bait another owner's rod");
    b.rod.ownerId=7;b.rod.equipped=false;Check(!PlanApplyBait(*catalog,b),"rod must be equipped");
    b.rod.equipped=true;b.bait.instanceId=100;Check(!PlanApplyBait(*catalog,b),"source and target identities must differ");
    b.bait.instanceId=101;b.bait.item=Fish(1);Check(!PlanApplyBait(*catalog,b),"live fish is not implicit bait");
}
void Publication()
{
    auto s=Fixture();const auto bad=[&](auto mutate){auto copy=s;mutate(copy);Check(!BuildItemCatalog(copy),"invalid gathering catalog cannot publish");};
    bad([](auto& s){s.toolProfiles[0].levels[1].practiceDenominator=0;});
    bad([](auto& s){s.toolProfiles[0].levels[1].practiceNumerator=6;});
    bad([](auto& s){s.toolProfiles[0].levels[1].failureLevel=0;});
    bad([](auto& s){s.toolProfiles[0].levels[1].failure=ToolFailure::ResetAtLevel;});
    bad([](auto& s){s.toolProfiles[0].levels[1].failure=ToolFailure::ResetAtLevel;s.toolProfiles[0].levels[1].failureLevel=2;s.toolProfiles[0].levels[1].failureLossBp=0;});
    bad([](auto& s){s.toolProfiles[0].levels[1].refineChanceBp=10001;});
    bad([](auto& s){s.toolProfiles[0].levels[1].power=UINT32_MAX;});
    bad([](auto& s){s.toolProfiles[0].levels[30].requiredPoints=1;});
    bad([](auto& s){s.toolProfiles[0].levels.pop_back();});
    bad([](auto& s){s.toolProfiles.push_back(s.toolProfiles[0]);});
    bad([](auto& s){s.tools[0].fishingPool=FishingPool::None;});
    bad([](auto& s){s.tools[0].kind=ToolKind::Axe;});
    bad([](auto& s){s.tools[0].profile=999;});
    bad([](auto& s){s.fishingItems[0].useLootProfile.reset();});
    bad([](auto& s){s.fishingItems[2].baitPower=0;});
    bad([](auto& s){s.fishingItems[1].grillResult=Fish(1);});
    bad([](auto& s){s.lootProfiles.back().entries[0].rareAttributeChanceBp=1;});
    bad([](auto& s){s.lootProfiles.back().entries[0]={1,LootRewardKind::Gold,{},1,31,0};});
    for(auto kind:{ToolKind::Pick,ToolKind::Axe,ToolKind::Shears})
    {auto tool=s;tool.tools[0].kind=kind;tool.tools[0].fishingPool=FishingPool::None;Check(BuildItemCatalog(tool).has_value(),"other gathering tool kinds have no fishing pool");}
    const auto original=BuildItemCatalog(s).value();auto snapshot=catalog_snapshot_fixture::Snapshot(s);
    Check(db::DecodeSnapshot(snapshot,{1,original->ContentHash()}).has_value(),"SQL roundtrip preserves all gathering fields");
    snapshot[db::Table::FishingItems][1][5].reset();Check(!db::DecodeSnapshot(snapshot,{1,{}}),"half-NULL grill key is corrupt");
    auto hash=s;hash.toolProfiles[0].levels[10].power+=1;
    Check(BuildItemCatalog(hash).value()->ContentHash()!=original->ContentHash(),"tool level contributes to hash");
    hash=s;hash.fishingItems[2].baitPower+=1;
    Check(BuildItemCatalog(hash).value()->ContentHash()!=original->ContentHash(),"bait power contributes to hash");
}
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture")
        {const auto c=BuildItemCatalog(Fixture()).value();std::ostringstream out;for(auto b:c->ContentHash())out<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);std::cout<<out.str()<<'\n';return 0;}
        Training();Fishing();Publication();std::cout<<checks<<" gathering checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
