#include "PersistentItemFixture.hpp"
#include <item/SocketInsertion.hpp>
#include <item/EquipmentStats.hpp>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;namespace sf=socket_service_fixture;
namespace
{
unsigned checks=0;
void Check(bool good,const char* why,std::source_location at=std::source_location::current()){++checks;if(!good)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
std::vector<std::byte> Run()
{
    auto definition=f::Catalog();definition.bonuses.push_back({9000,"bonus.insertion_fixture",BonusSource::ItemBase,{{BonusId::PhysicalDefenseFlat,12000}}});std::ranges::find(definition.items,sf::Stone(),&ItemBaseRow::key)->baseBonusProfile=9000;
    const auto catalog=BuildItemCatalog(definition).value();const auto& c=*catalog;auto target=f::Sword();target.stones={StoneSocketSlot{},StoneSocketSlot{StoneSocketState::Empty,1},StoneSocketSlot{StoneSocketState::Empty,2}};
    auto source=f::Leather(70);source.key=sf::Stone();source.refinementLevel=20;source.count=2;std::vector<PersistentItem> items{target,source};const std::vector layouts{f::Inventory()};const ItemOwnerFence fence{f::Character(),7,2};const SocketMaintenanceActor actor{100,1,1,false};
    const auto make=[&](std::uint16_t chance,std::uint16_t roll){return MakeSocketInsertionCommit(c,{16},fence,layouts,items,source.id,target.id,actor,chance,roll);};
    auto success=make(10000,0);Check(success && success->succeeded && success->quote.kind==4 && success->quote.slot==1 && success->quote.chance==10000,"first compatible empty slot selected");
    Check(success->commit.items.size()==2 && success->commit.items[0].after.id==source.id && success->commit.items[0].after.count==1 && success->commit.items[0].after.revision==2,"exactly one stone consumed with source identity retained");
    Check(success->commit.items[1].after.id==target.id && success->commit.items[1].after.refinementLevel==9 && success->commit.items[1].after.attributes==target.attributes && success->commit.items[1].after.boundTo==target.boundTo,"full target state preserved except socket and revision");
    Check(success->quote.after[1]==StoneSocketSlot{StoneSocketState::Occupied,1,source.key,20} && success->quote.failure[1]==StoneSocketSlot{StoneSocketState::Broken,1} && success->quote.after[0]==target.stones[0] && success->quote.after[2]==target.stones[2],"preview owns both complete outcomes and exact embedded level");
    Check(success->commit.outcomeCode==SocketInsertionSuccess && success->commit.wallets.empty() && ValidateItemCommit(c,success->commit).has_value(),"success is one item-only transaction");
    auto failure=make(0,0);Check(failure && !failure->succeeded && failure->commit.outcomeCode==SocketInsertionFailure && failure->commit.items[0].after.count==1 && failure->commit.items[1].after.stones[1]==StoneSocketSlot{StoneSocketState::Broken,1},"failed attempt consumes one and preserves socket grade");
    auto low=make(3000,3000),high=make(3000,3001);Check(low && high && low->succeeded && !high->succeeded && low->quote==high->quote,"exact chance boundary and outcome-independent preview");
    Check(ItemCommitDigest(c,low->commit)!=ItemCommitDigest(c,high->commit),"receipt binds the recorded roll and actual economic outcome");
    Check(!make(10001,1) && !make(3000,0) && !make(3000,10001) && !make(10000,1),"invalid chance and draws refused before consumption");
    items[1].count=1;auto whole=make(10000,0);Check(whole && whole->commit.items[0].after.destroyed && !whole->commit.items[0].after.placement,"last source stone tombstoned once");items[1]=source;
    items[0].stones[1]={StoneSocketState::Broken,1};Check(make(10000,0)->quote.slot==2,"broken slot is not overwritten");items[0]=target;
    auto gold=definition;std::ranges::find(gold.socketStones,source.key,&ItemSocketStoneRow::key)->minimumSocketGrade=2;const auto goldCatalog=BuildItemCatalog(gold).value();
    Check(MakeSocketInsertionCommit(*goldCatalog,{16,1},fence,layouts,items,source.id,target.id,actor,10000,0)->quote.slot==2,"gold-only stone skips normal empty slot");
    items[0].stones={};Check(make(10000,0).error()==SocketMaintenanceStatus::NoSocket,"closed sockets do not open implicitly");items[0]=target;
    auto conflict=definition;conflict.socketStones[1].secondaryGroup=conflict.socketStones[0].primaryGroup;const auto conflictCatalog=BuildItemCatalog(conflict).value();items[0].stones[0]={StoneSocketState::Occupied,1,sf::Stone(2),0};
    Check(MakeSocketInsertionCommit(*conflictCatalog,{16,2},fence,layouts,items,source.id,target.id,actor,10000,0).error()==SocketMaintenanceStatus::Conflict,"cross-group conflict refuses whole insertion");items[0]=target;
    auto armorOnly=definition;armorOnly.socketStones[0].allowedTargets=2;const auto armorCatalog=BuildItemCatalog(armorOnly).value();Check(MakeSocketInsertionCommit(*armorCatalog,{16,3},fence,layouts,items,source.id,target.id,actor,10000,0).error()==SocketMaintenanceStatus::Incompatible,"target mask enforced");
    auto body=f::Leather(71);body.key=item_catalog_fixture::Armor();body.count=1;body.refinementLevel=2;body.placement->anchor=0;body.stones={StoneSocketSlot{StoneSocketState::Empty,1},StoneSocketSlot{},StoneSocketSlot{StoneSocketState::Empty,2}};auto bodyStone=source;bodyStone.placement->anchor=4;
    auto bodyPlan=MakeSocketInsertionCommit(*armorCatalog,{16,30},fence,layouts,std::array{body,bodyStone},source.id,body.id,actor,10000,0);Check(bodyPlan && bodyPlan->succeeded && bodyPlan->quote.slot==0 && bodyPlan->commit.items[1].after.stones[0].stone==source.key,"armor-only stone enters a body armor socket with exact typed identity");
    auto retired=definition;std::ranges::find(retired.items,source.key,&ItemBaseRow::key)->retired=true;const auto old=BuildItemCatalog(retired).value();Check(MakeSocketInsertionCommit(*old,{16,4},fence,layouts,items,source.id,target.id,actor,10000,0).error()==SocketMaintenanceStatus::Denied,"retired loose stone cannot be inserted");
    auto restricted=definition;std::ranges::find(restricted.items,source.key,&ItemBaseRow::key)->minimumLevel=101;const auto levelCatalog=BuildItemCatalog(restricted).value();Check(MakeSocketInsertionCommit(*levelCatalog,{16,5},fence,layouts,items,source.id,target.id,actor,10000,0).error()==SocketMaintenanceStatus::Level,"stone minimum level enforced");
    auto invalid=actor;invalid.characterClass=4;Check(!MakeSocketInsertionCommit(c,{16,6},fence,layouts,items,source.id,target.id,invalid,10000,0),"invalid actor class refused");invalid=actor;invalid.sex=2;Check(!MakeSocketInsertionCommit(c,{16,7},fence,layouts,items,source.id,target.id,invalid,10000,0),"invalid actor sex refused");
    auto bad=items;bad[1].owner.id++;Check(!MakeSocketInsertionCommit(c,{16,8},fence,layouts,bad,source.id,target.id,actor,10000,0),"foreign source refused");bad=items;bad[0].boundTo=ItemOwner{ItemOwnerKind::Character,43};Check(!MakeSocketInsertionCommit(c,{16,9},fence,layouts,bad,source.id,target.id,actor,10000,0),"foreign-bound target refused");
    bad=items;bad[0].placement=ItemPlacement{ItemStorageKind::Equipment,1,0};Check(!MakeSocketInsertionCommit(c,{16,10},fence,layouts,bad,source.id,target.id,actor,10000,0),"equipped target refused");bad=items;bad[1].placement=ItemPlacement{ItemStorageKind::TradeEscrow,1,0};Check(!MakeSocketInsertionCommit(c,{16,11},fence,layouts,bad,source.id,target.id,actor,10000,0),"escrow source refused");
    bad=items;bad[1].revision=UINT64_MAX;Check(!MakeSocketInsertionCommit(c,{16,12},fence,layouts,bad,source.id,target.id,actor,10000,0),"source revision cannot wrap");bad=items;bad[0].revision=UINT64_MAX;Check(!MakeSocketInsertionCommit(c,{16,13},fence,layouts,bad,source.id,target.id,actor,10000,0),"target revision cannot wrap");
    bad=items;bad.push_back(source);Check(!MakeSocketInsertionCommit(c,{16,14},fence,layouts,bad,source.id,target.id,actor,10000,0),"duplicate input instance refused");Check(!MakeSocketInsertionCommit(c,{},fence,layouts,items,source.id,target.id,actor,10000,0),"zero operation refused");
    auto binding=definition;std::ranges::find(binding.items,source.key,&ItemBaseRow::key)->binding=BindingPolicy::OnAcquire;const auto boundCatalog=BuildItemCatalog(binding).value();bad=items;bad[1].boundTo=fence.owner;
    Check(MakeSocketInsertionCommit(*boundCatalog,{16,15},fence,layouts,bad,source.id,target.id,actor,10000,0).error()==SocketMaintenanceStatus::Denied,"bound stone cannot launder ownership through an unbound target");bad[0].boundTo=fence.owner;
    Check(MakeSocketInsertionCommit(*boundCatalog,{16,16},fence,layouts,bad,source.id,target.id,actor,10000,0).has_value(),"acquire-bound stone can enter an already own-bound target");
    bad=items;bad[1].boundTo=fence.owner;bad[0].boundTo=fence.owner;Check(!MakeSocketInsertionCommit(c,{16,17},fence,layouts,bad,source.id,target.id,actor,10000,0),"unrepresentable custom binding never discarded");
    auto full=items;for(unsigned cell=0;cell<90;++cell)if(cell!=0 && cell!=1 && cell!=5 && cell!=10){auto material=f::Leather(200+cell);material.placement->anchor=cell;full.push_back(material);}
    Check(MakeSocketInsertionCommit(c,{16,18},fence,layouts,full,source.id,target.id,actor,10000,0).has_value() && MakeSocketInsertionCommit(c,{16,19},fence,layouts,full,source.id,target.id,actor,0,0).has_value(),"both outcomes work in a full bag without new outputs");
    auto equipped=target;equipped.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};const auto before=BuildEquipmentStats(c,fence.owner,std::array{equipped,source}).value();equipped=success->commit.items[1].after;equipped.placement=ItemPlacement{ItemStorageKind::Equipment,1,0};const auto after=BuildEquipmentStats(c,fence.owner,std::array{equipped,success->commit.items[0].after}).value();Check(after.flat.physicalDefenseRaw-before.flat.physicalDefenseRaw==12000,"only installed stone contributes its bonus when the target is equipped");
    const auto packet=EncodeSocketMaintenance(c,0x123456789abcdefULL,&source,&target,2,SocketMaintenanceStatus::Ready,low->quote,std::array{target.id.Value()});Check(packet && std::to_integer<unsigned>((*packet)[5])==2,"native insertion quote uses GC235 v2");return *packet;
}
}
int main(int argc,char** argv){try{const auto packet=Run();if(argc==3 && std::string_view(argv[1])=="--write-golden"){std::ofstream out(argv[2]);for(auto b:packet)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write failed");}std::cout<<checks<<" socket insertion checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
