#include "ToolRuntimeFixture.hpp"
#include <item/EquipmentStats.hpp>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=tool_runtime_fixture;namespace pf=persistent_item_fixture;
unsigned checks=0;void Check(bool b,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!b)throw std::runtime_error(std::string(why)+" line "+std::to_string(at.line()));}
int main(int argc,char** argv){try{
 auto graph=f::Catalog();f::ExtendTransitions(graph);const auto branch=BuildItemCatalog(graph).value();
 const ItemOwnerFence branchFence{pf::Character(),7,1};const std::vector branchLayouts{pf::Inventory()};
 auto original=f::Item();original.key=f::Tool(6);original.tool=ToolInstanceState{3,50};std::vector branchItems{original};
 ToolRuntimeRequest branchRequest{original.id,{},ToolRuntimeAction::Refine,f::Actor(),5000};
 auto won=MakeToolRuntimeCommit(*branch,{91},branchFence,branchLayouts,branchItems,branchRequest);
 Check(won && won->afterItem==f::Tool(7) && won->commit.items[0].after.key==f::Tool(7) && won->commit.items[0].after.id==original.id,"cross-key tool success preserves the instance identity");
 Check(won->state==ToolInstanceState{} && won->level==0 && won->commit.items[0].after.revision==original.revision+1,"new prototype resets training and bait atomically");
 branchRequest.roll=5001;auto lost=MakeToolRuntimeCommit(*branch,{91,1},branchFence,branchLayouts,branchItems,branchRequest);
 Check(lost && lost->afterItem==f::Tool(6) && lost->state==ToolInstanceState{} && lost->commit.items[0].after.id==original.id,"explicit failure self-loop resets the old factory state");
 branchItems[0]=won->commit.items[0].after;branchRequest.roll=0;Check(CheckToolRuntime(*branch,branchFence.owner,branchItems,branchRequest).error()==GatheringError::AtMaximum,"terminal next prototype cannot refine or degrade");branchItems[0]=original;
 auto looping=graph;looping.toolProfiles[looping.toolProfiles.size()-2].levels[0].successItem=f::Tool(6);const auto loop=BuildItemCatalog(looping).value();branchRequest.roll=1;
 const auto looped=MakeToolRuntimeCommit(*loop,{91,2},branchFence,branchLayouts,branchItems,branchRequest);Check(looped && looped->afterItem==f::Tool(6) && looped->level==0,"successful self-loop is retained without invented next level");
 auto badGraph=graph;badGraph.toolProfiles[badGraph.toolProfiles.size()-2].levels[0].successItem=f::Tool(999);Check(!BuildItemCatalog(badGraph),"missing transition target rejected at catalog admission");
 badGraph=graph;std::ranges::find(badGraph.items,f::Tool(7),&ItemBaseRow::key)->height=2;Check(!BuildItemCatalog(badGraph),"transition cannot silently enlarge the inventory footprint");
 const auto transitionPreview=EncodeToolRuntime(*branch,{1,255,0,911,&original,&original,ToolRuntimeAction::Refine,987}).value();
 const auto transitionApplied=EncodeToolRuntime(*branch,{3,255,1,911,&original,&original,ToolRuntimeAction::Refine,987,5000,won->level,won->state,won->afterItem}).value();
 const auto transitionFailed=EncodeToolRuntime(*branch,{3,255,2,911,&original,&original,ToolRuntimeAction::Refine,987,5001,lost->level,lost->state,lost->afterItem}).value();
 Check(std::to_integer<unsigned>(transitionPreview[5])==5,"explicit transitions use GC254 version 5");
 const auto c=BuildItemCatalog(f::Catalog()).value();const ItemOwnerFence fence{pf::Character(),7,1};std::vector<ItemStorageLayout> layouts{pf::Inventory(),{{pf::Character(),ItemStorageKind::Equipment,1},8,1,1}};
 auto rod=f::Item(),bait=f::BaitItem();std::vector<PersistentItem> items{rod,bait};auto actor=f::Actor();
 Check(ValidatePersistentItem(*c,rod).has_value(),"explicit tool state validates");auto invalid=rod;invalid.tool.reset();Check(!ValidatePersistentItem(*c,invalid),"tool payload required");invalid=rod;invalid.tool->trainingPoints=4;Check(!ValidatePersistentItem(*c,invalid),"training cap validated");invalid=f::Item(2);invalid.tool->baitPower=1;Check(!ValidatePersistentItem(*c,invalid),"only a rod carries bait");
 auto wrong=pf::Leather();wrong.tool=ToolInstanceState{};Check(!ValidatePersistentItem(*c,wrong),"tool payload cannot attach to a material");invalid=rod;invalid.count=2;Check(!ValidatePersistentItem(*c,invalid),"tool never stacks");
 auto hash=PersistentItemDigest(*c,rod);invalid=rod;invalid.tool->baitPower=50;Check(PersistentItemDigest(*c,invalid)!=hash,"bait is hashed");invalid=rod;invalid.tool->trainingPoints=2;Check(PersistentItemDigest(*c,invalid)!=hash,"training is hashed");
 const std::array ids{ItemInstanceId::Create(27000).value()};auto grant=MakeGrantCommit(*c,{26},fence,layouts,{}, {f::Tool(),0,1},ids);Check(grant && grant->items[0].after.tool==ToolInstanceState{},"normal grant initializes empty tool state");
 auto weapon=pf::Sword();weapon.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};auto carried=rod;carried.placement=ItemPlacement{ItemStorageKind::Inventory,1,25};
 const std::array swapping{weapon,carried};auto swapped=MakeEquipCommit(*c,{26,0,1},fence,layouts,swapping,rod.id,{100,0,0});
 Check(swapped && swapped->items.size()==2 && swapped->items[0].after.tool==carried.tool && swapped->items[1].after.id==weapon.id && swapped->items[1].after.attributes==weapon.attributes && swapped->items[1].after.stones==weapon.stones,"tool swaps with weapon preserving both complete instances");
 auto equip=MakeEquipCommit(*c,{26,1},fence,layouts,items,rod.id,{100,0,0});Check(equip && equip->items[0].after.placement==ItemPlacement{ItemStorageKind::Equipment,1,0},"tool uses the hand slot");
 items[0]=equip->items[0].after;auto stats=BuildEquipmentStats(*c,fence.owner,items);Check(stats && stats->tool==ToolKind::Rod && !stats->weapon && stats->flat==ItemBaseStats{},"tool power is not combat damage");
 ToolRuntimeRequest baitRequest{bait.id,rod.id,ToolRuntimeAction::Bait,actor};auto applied=MakeToolRuntimeCommit(*c,{26,2},fence,layouts,items,baitRequest);Check(applied && applied->outcome==3 && applied->state.baitPower==20 && applied->commit.items[0].after.count==1 && applied->commit.items[1].after.tool->trainingPoints==3,"bait and rod saved together");
 items[0].tool->baitPower=50;applied=MakeToolRuntimeCommit(*c,{26,3},fence,layouts,items,baitRequest);Check(applied && applied->state.baitPower==20,"weaker bait replaces rather than adds");
 baitRequest.actor.fishingActive=true;Check(!MakeToolRuntimeCommit(*c,{26,4},fence,layouts,items,baitRequest),"active fishing rejects bait");baitRequest.actor.fishingActive=false;
 auto badBait=baitRequest;badBait.target=bait.id;Check(!CheckToolRuntime(*c,fence.owner,items,badBait),"bait cannot target itself");
 auto unequip=MakeUnequipCommit(*c,{26,5},fence,layouts,items,rod.id,{ItemStorageKind::Inventory,1,11});Check(unequip && unequip->items[0].after.tool->baitPower==50,"unequip retains full tool state");items[0]=unequip->items[0].after;
 ToolRuntimeRequest refine{rod.id,{},ToolRuntimeAction::Refine,actor};auto upgraded=MakeToolRuntimeCommit(*c,{26,6},fence,layouts,items,refine);Check(upgraded && upgraded->outcome==1 && upgraded->level==1 && upgraded->state.trainingPoints==0 && upgraded->state.baitPower==50,"upgrade preserves identity and bait, resets training");
 refine.actor.serviceAvailable=false;Check(CheckToolRuntime(*c,fence.owner,items,refine).error()==GatheringError::ServiceRequired,"trusted service required");refine.actor=actor;refine.roll=1;Check(!MakeToolRuntimeCommit(*c,{26,7},fence,layouts,items,refine),"guaranteed success has no RNG roll");refine.roll=0;
 items[0].tool->trainingPoints=2;Check(CheckToolRuntime(*c,fence.owner,items,refine).error()==GatheringError::NotReady,"incomplete practice cannot refine");items[0]=f::Item(2);refine.source=items[0].id;auto failed=MakeToolRuntimeCommit(*c,{26,8},fence,layouts,items,refine);Check(failed && failed->outcome==2 && failed->level==0 && failed->state.trainingPoints==2,"0-percent failure applies floor-based point loss");
 items[0]=rod;items[0].refinementLevel=1;items[0].tool->trainingPoints=5;refine.source=rod.id;refine.roll=5001;failed=MakeToolRuntimeCommit(*c,{26,9},fence,layouts,items,refine);Check(failed && failed->level==0 && failed->state.trainingPoints==0,"failure reset level is explicit");refine.roll=5000;upgraded=MakeToolRuntimeCommit(*c,{26,10},fence,layouts,items,refine);Check(upgraded && upgraded->level==2 && upgraded->state.trainingPoints==0,"random refine boundary exact");
 items[0]=upgraded->commit.items[0].after;refine.roll=0;Check(CheckToolRuntime(*c,fence.owner,items,refine).error()==GatheringError::AtMaximum,"maximum cannot refine");
 items[0]=rod;items[0].tool->trainingPoints=0;items[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};ToolRuntimeRequest practice{rod.id,{},ToolRuntimeAction::Practice,actor,1};
 Check(CheckToolRuntime(*c,fence.owner,items,practice).error()==GatheringError::ResourceCompletionRequired,"practice requires a completed server resource event");practice.actor.completedResourceAction=true;auto practiced=MakeToolRuntimeCommit(*c,{26,11},fence,layouts,items,practice);Check(practiced && practiced->state.trainingPoints==1 && practiced->outcome==4,"one completed event earns at most one practice point");practice.roll=3;practiced=MakeToolRuntimeCommit(*c,{26,12},fence,layouts,items,practice);Check(practiced && practiced->state.trainingPoints==0,"exact denominator roll can fail without changing points");
 const auto inventory=EncodeTypedInventory(*c,layouts,{},items).value();auto decoded=DecodeTypedInventory(inventory,fence.owner);Check(std::to_integer<unsigned>(inventory[5])==6 && decoded && decoded->items[0].tool==items[0].tool,"inventory v6 roundtrips tool state");
 auto foreign=items;foreign[0].owner.id++;Check(!CheckToolRuntime(*c,fence.owner,foreign,{rod.id,{},ToolRuntimeAction::Info,actor}),"foreign tool denied");auto high=f::Item(5);Check(!MakeEquipCommit(*c,{26,13},fence,layouts,std::span(&high,1),high.id,{100,0,0}),"tool minimum level enforced when equipping");
 const auto toolPacket=EncodeToolRuntime(*c,{1,255,0,123,&rod,&rod,ToolRuntimeAction::Refine,987}).value();const auto baitPacket=EncodeToolRuntime(*c,{1,255,0,456,&bait,&items[0],ToolRuntimeAction::Bait,0}).value();const auto closed=EncodeToolRuntime(*c,{0}).value();
 const auto savedBait=MakeToolRuntimeCommit(*c,{26,14},fence,layouts,items,{bait.id,rod.id,ToolRuntimeAction::Bait,actor}).value();
 const auto appliedPacket=EncodeToolRuntime(*c,{3,255,3,456,&bait,&items[0],ToolRuntimeAction::Bait,0,0,savedBait.level,savedBait.state}).value();
 const auto equipment=EncodeEquipmentStats(&*stats,99,1,1,{});Check(std::to_integer<unsigned>(equipment[5])==2 && std::to_integer<unsigned>(equipment.back())==2,"equipment v2 identifies the tool without a weapon subtype");
 if(argc==3 && std::string_view(argv[1])=="--golden-output")
 {const auto write=[](const std::filesystem::path& path,const auto& bytes){std::ofstream out(path);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");};const std::filesystem::path path=argv[2];write(path,toolPacket);write(path.parent_path()/"typed_tool_transition_v5.txt",transitionPreview);write(path.parent_path()/"typed_tool_transition_applied_v5.txt",transitionApplied);write(path.parent_path()/"typed_tool_transition_failed_v5.txt",transitionFailed);write(path.parent_path()/"typed_tool_bait_v2.txt",baitPacket);write(path.parent_path()/"typed_tool_applied_v2.txt",appliedPacket);write(path.parent_path()/"typed_tool_closed_v2.txt",closed);write(path.parent_path()/"typed_tool_inventory_v6.txt",inventory);write(path.parent_path()/"typed_tool_equipment_v2.txt",equipment);}
 std::cout<<"Tool runtime: "<<checks<<" checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
