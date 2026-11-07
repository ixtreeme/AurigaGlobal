#include "SoulRuntimeFixture.hpp"
#include <item/TypedInventoryWire.hpp>
#include <item/TypedItemDetails.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
using namespace auriga::item;namespace f=soul_runtime_fixture;
namespace {
unsigned checks=0;void Check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
ItemOwner owner{ItemOwnerKind::Character,30};ItemOwnerFence fence{owner,1,1};std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},10,9,1}};std::vector<PersistentItem> items;
PersistentItem& Item(unsigned n){return items.at(n-1);}
void Apply(const ItemCommit& c){for(const auto& ch:c.items)Item(static_cast<unsigned>(ch.after.id.Value()))=ch.after;++fence.revision;}
auto Action(const ItemCatalog& c,unsigned core,SoulAction action,std::uint64_t elapsed=0,unsigned scroll=0,SoulContext context={}){context.actor.level=120;return MakeSoulCommit(c,{static_cast<std::uint8_t>(fence.revision)},fence,layouts,items,Item(core).id,scroll ? std::optional{Item(scroll).id} : std::nullopt,action,elapsed,0,context);}
void Run(const std::filesystem::path& golden){auto built=BuildItemCatalog(f::Catalog());Check(built.has_value(),"soul runtime fixture");const auto& c=**built;
    for(unsigned j=1;j<=3;++j){ItemGrant grant{f::Key(j)};grant.count=j==3 ? 4 : 1;std::array ids{ItemInstanceId::Create(j).value()};auto result=MakeGrantCommit(c,{static_cast<std::uint8_t>(j)},fence,layouts,items,grant,ids);Check(result.has_value(),"real soul factory");items.push_back(result->items.front().after);++fence.revision;}
    Check(Item(1).soul==SoulState{0,2,0,false} && !Item(3).soul,"explicit factory state and plain scroll");
    Check(!Action(c,1,SoulAction::Activate),"cannot activate empty core");auto charge=Action(c,1,SoulAction::Recharge,29999);Check(charge && charge->commit.items.front().after.soul==SoulState{5,2,4999,false},"owner clock and exact remainder");Apply(charge->commit);auto tick=Action(c,1,SoulAction::Recharge,1);Check(tick && tick->commit.items.front().after.soul==SoulState{6,2,0,false},"full capacity clears remainder");Apply(tick->commit);
    auto on=Action(c,1,SoulAction::Activate);Check(on.has_value(),"red active");Apply(on->commit);Check(!PlainInventoryItem(Item(1),owner),"active soul locks generic material paths");
    SoulContext hit;hit.channel=SoulChannel::NormalAttack;hit.targetIsNpc=true;hit.event=30;hit.target=99;hit.targetRevision=1;hit.damage=10001;
    auto strike=Action(c,1,SoulAction::Strike,0,0,hit);Check(strike && strike->damage==25002 && strike->commit.items.front().after.soul->strikes==1,"exact accepted damage and one debit");auto forged=strike->commit;forged.items.front().after.soul->strikes++;Check(!ValidateItemCommit(c,forged),"cannot suppress strike debit");forged=strike->commit;forged.result[64]++;Check(!ValidateItemCommit(c,forged),"input damage alone cannot alter the exact output state when strike state is unchanged") ;
    Apply(strike->commit);auto second=Action(c,1,SoulAction::Strike,0,0,hit);Check(second && second->commit.items.front().after.soul==SoulState{4,2,0,true},"cycle boundary spends charge and renews strikes");Apply(second->commit);
    hit.damage=0;Check(!Action(c,1,SoulAction::Strike,0,0,hit),"zero hit does not debit");hit.damage=10001;hit.channel=SoulChannel::Skill;Check(!Action(c,1,SoulAction::Strike,0,0,hit),"wrong channel does not debit");hit.channel=SoulChannel::NormalAttack;hit.targetIsNpc=false;Check(!Action(c,1,SoulAction::Strike,0,0,hit),"PvP does not debit");hit.targetIsNpc=true;hit.polymorphed=true;Check(!Action(c,1,SoulAction::Strike,0,0,hit),"polymorph does not debit");
    auto off=Action(c,1,SoulAction::Deactivate);Check(off.has_value(),"deactivation");Apply(off->commit);auto refine=Action(c,1,SoulAction::Refine,0,3);Check(refine && refine->success && refine->commit.items[0].after.refinementLevel==1 && refine->commit.items[1].after.count==3,"same identity level and atomic scroll debit");Apply(refine->commit);Check(Item(1).soul==SoulState{0,2,0,false},"successful refine resets explicit state");
    auto blueCharge=Action(c,2,SoulAction::Recharge,30000);Check(blueCharge.has_value(),"blue independent charge");Apply(blueCharge->commit);auto blueOn=Action(c,2,SoulAction::Activate);Check(blueOn.has_value(),"blue independent activation");Apply(blueOn->commit);SoulContext death;death.alive=false;auto cleanup=Action(c,2,SoulAction::Recharge,0,0,death);Check(cleanup && !cleanup->commit.items.front().after.soul->active,"death clears active intent without charge loss");Apply(cleanup->commit);
    for(const auto& i:items)if(i.soul){const auto bytes=EncodeSoulState(*i.soul);Check(DecodeSoulState(bytes)==i.soul,"state roundtrip");for(unsigned n=0;n<bytes.size();++n)Check(!DecodeSoulState(std::span(bytes).first(n)),"state truncation");}
    auto wire=EncodeTypedInventory(c,layouts,{},items);Check(wire && std::to_integer<unsigned>((*wire)[5])==15,"v15 wire");auto decoded=DecodeTypedInventory(*wire,owner);auto expected=items;for(auto& i:expected)i.revision=1;Check(decoded && decoded->items==expected,"native wire exact roundtrip");for(unsigned n=0;n<wire->size();++n)Check(!DecodeTypedInventory(std::span(*wire).first(n),owner),"wire truncation");
    if(!golden.empty()){std::filesystem::create_directories(golden);const auto save=[&](const char* name,const auto& bytes){std::ofstream out(golden/name);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';Check(out.good(),"write golden");};save("typed_soul_v15.hex",*wire);auto details=EncodeTypedItemDetails(c,items,false);Check(details.has_value(),"wide soul details");save("typed_soul_details_v3.hex",*details);}
}
}
int main(int argc,char** argv){try{Run(argc==3 && std::string_view(argv[1])=="--write-golden" ? std::filesystem::path(argv[2]) : std::filesystem::path{});std::cout<<"SOUL_RUNTIME_TESTS=SUCCESS checks="<<checks<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
