#include "ItemCatalogFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace
{
std::size_t checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
ItemKey Stone(){return ItemKey::Create(7,1).value();}
ItemKey Shard(){return ItemKey::Create(7,2).value();}
ItemKey Other(){return ItemKey::Create(7,3).value();}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();
    auto base=*std::ranges::find(s.items,f::Leather(),&ItemBaseRow::key);
    base.key=Stone();base.contentCode="socket_stone.precision";base.nameKey="item.precision.name";base.descriptionKey="item.precision.description";
    base.baseBonusProfile=1;base.refinementProfile=5;
    s.items.push_back(base);
    base.key=Shard();base.contentCode="socket_stone.shard";base.nameKey="item.shard.name";base.descriptionKey="item.shard.description";
    base.baseBonusProfile.reset();base.refinementProfile=1;s.items.push_back(base);
    RefinementProfile profile{5,"refine.stone",RefinementKind::SocketStone,30,{}};
    for(std::uint16_t i=0;i<=30;++i)profile.levels.push_back({i,{},i==10 ? std::optional<std::uint32_t>{2} : std::nullopt});
    s.refinements.push_back(std::move(profile));
    s.socketStones={{Stone(),SocketStoneKind::Insertable,3,1,"stone.precision","stone.penetration"},
                    {Shard(),SocketStoneKind::BrokenShard,0,0,"",""}};
    return s;
}
void AddOther(ItemCatalogSource& s)
{
    auto base=s.items[s.items.size()-2];base.key=Other();base.contentCode="socket_stone.other";s.items.push_back(base);
    s.socketStones.push_back({Other(),SocketStoneKind::Insertable,3,1,"stone.other",""});
}
SocketStoneInsertRequest Request()
{
    SocketStoneInsertRequest r{{Stone(),100,5,7,10,4},{f::Sword(),200,8,7,9,1}};
    r.sockets[1]={StoneSocketState::Empty,1};r.sockets[2]={StoneSocketState::Empty,2};
    r.actorId=7;r.rulesetRevision=2;r.operationId[0]=1;r.chanceBp=7000;r.recordedRoll=7000;
    r.characterLevel=50;r.admittedByServer=true;
    return r;
}
void Plans()
{
    const auto catalog=BuildItemCatalog(Fixture()).value();auto r=Request();
    const auto plan=PrepareSocketStoneInsert(*catalog,r).value();
    Check(plan.success && plan.slot==1,"first compatible empty socket wins");
    Check(plan.after.state==StoneSocketState::Occupied && plan.after.stone==Stone() && plan.after.refinementLevel==10,"insert preserves typed identity and +10 level");
    Check(plan.consumeUnits==1 && r.stone.count==4 && !r.sockets[1].stone,"proposal consumes one only at commit, never mutates snapshots");
    Check(plan.expected.target.revision==8 && plan.expected.stone.revision==5 && plan.expected.sockets==r.sockets,"both revisions and full slot precondition copied");
    Check(plan.catalogRevision==catalog->Revision() && plan.catalogHash==catalog->ContentHash(),"content pin retained");
    Check(plan.expected.operationId==r.operationId && plan.expected.rulesetRevision==2,"durable operation and rules identity retained");
    r.recordedRoll=7001;const auto failed=PrepareSocketStoneInsert(*catalog,r).value();
    Check(!failed.success && failed.after.state==StoneSocketState::Broken && !failed.after.stone && !failed.after.refinementLevel,"failed insertion records a typed broken socket");
    Check(failed.consumeUnits==1 && failed.after.grade==1,"failure consumes one and preserves the socket grade");
    for(auto chance:{0,10000})
    {
        r=Request();r.chanceBp=chance;r.recordedRoll=0;
        Check(PrepareSocketStoneInsert(*catalog,r)->success==(chance==10000),"explicit endpoint decision needs no random roll");
    }
    r=Request();r.sockets[1].state=StoneSocketState::Broken;
    Check(PrepareSocketStoneInsert(*catalog,r)->slot==2,"broken sockets cannot be overwritten");
    auto gold=Fixture();gold.socketStones[0].minimumSocketGrade=2;
    Check(PrepareSocketStoneInsert(*BuildItemCatalog(gold).value(),Request())->slot==2,"gold-only stone skips normal socket");
    r=Request();r.target.item=f::Armor();r.target.refinementLevel=2;
    Check(PrepareSocketStoneInsert(*catalog,r).has_value(),"body armor accepted");
    r.target.item=f::Bracelet();
    Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::IncompatibleTarget,"accessory charges are a separate domain");
    auto helmet=Fixture();helmet.armors[0].slot=EquipmentSlot::Head;
    r=Request();r.target.item=f::Armor();r.target.refinementLevel=2;
    Check(PrepareSocketStoneInsert(*BuildItemCatalog(helmet).value(),r).error()==SocketStoneError::IncompatibleTarget,"head armor cannot take body stones");
    auto armorOnly=Fixture();armorOnly.socketStones[0].allowedTargets=2;
    Check(PrepareSocketStoneInsert(*BuildItemCatalog(armorOnly).value(),Request()).error()==SocketStoneError::IncompatibleTarget,"explicit target mask applied");
    const auto* refine=catalog->FindRefinement(5);
    Check(NextRefinementLevel(*refine,9).value()==10 && NextRefinementLevel(*refine,29).value()==30,"stone refinement has no +9 ceiling");
    Check(catalog->RefinedBaseStats(Stone(),10).value()==ItemBaseStats{},"stone does not introduce hidden weapon/armor raw stats");
    const auto bonuses=catalog->BonusProfilesAt(Stone(),10).value();
    Check(bonuses.base->Data().id==1 && bonuses.refinement->Data().id==2,"installed stone bonus sources stay explicit");
    Check(ResolveRefinedBaseStats({1000,0,0,0},*refine,10).error()==RefinementError::WrongStatKind,"stone profile cannot be evaluated as raw attack gain");
}
void EconomicRefinement()
{
    auto source=Fixture();source.refinementRecipes.push_back({1,"refine.stone_9_10",Stone(),9,Stone(),10,5000,10000,RefinementFailure::Keep,{},{}});
    const auto catalog=BuildItemCatalog(source).value();
    RefinementUseContext context;context.actorId=7;context.walletRevision=1;context.gold=6000;context.operationId[0]=1;context.allowedByRules=true;
    RefinementItemSnapshot item{100,1,7,Stone(),9,1,false};
    const auto plan=PrepareRefinement(*catalog,1,context,item,{},0).value();
    Check(plan.success && plan.result==Stone() && plan.resultLevel==10 && plan.goldCost==5000,"count-one stone refinement uses the existing economic recipe path");
    item.count=2;
    Check(PrepareRefinement(*catalog,1,context,item,{},0).error()==RefinementUseError::InvalidSource,"one recipe cannot upgrade an entire stack");
}
void Rejections()
{
    const auto catalog=BuildItemCatalog(Fixture()).value();
    for(int change=0;change<19;++change)
    {
        auto r=Request();auto expected=SocketStoneError::InvalidSnapshot;
        switch(change)
        {
        case 0:r.admittedByServer=false;expected=SocketStoneError::Forbidden;break;
        case 1:r.actorId=0;expected=SocketStoneError::InvalidOperation;break;
        case 2:r.operationId={};expected=SocketStoneError::InvalidOperation;break;
        case 3:r.rulesetRevision=0;expected=SocketStoneError::InvalidOperation;break;
        case 4:r.stone.ownerId=8;break;
        case 5:r.target.ownerId=8;break;
        case 6:r.stone.instanceId=r.target.instanceId;break;
        case 7:r.target.revision=0;break;
        case 8:r.stone.revision=std::numeric_limits<std::uint64_t>::max();break;
        case 9:r.target.equipped=true;break;
        case 10:r.stone.exchanging=true;break;
        case 11:r.target.locked=true;break;
        case 12:r.stone.count=201;expected=SocketStoneError::InvalidStack;break;
        case 13:r.stone.count=0;expected=SocketStoneError::InvalidStack;break;
        case 14:r.target.count=2;expected=SocketStoneError::InvalidStack;break;
        case 15:r.stone.refinementLevel=31;expected=SocketStoneError::InvalidLevel;break;
        case 16:r.target.refinementLevel=10;expected=SocketStoneError::InvalidLevel;break;
        case 17:r.inCombat=true;expected=SocketStoneError::Forbidden;break;
        case 18:r.stone.item=Shard();expected=SocketStoneError::NotStone;break;
        }
        Check(PrepareSocketStoneInsert(*catalog,r).error()==expected,"bad admission, identity, state, stack or level rejected");
    }
    for(auto roll:{0,10001})
    {auto r=Request();r.recordedRoll=roll;Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::InvalidDecision,"invalid recorded roll");}
    for(auto chance:{0,10000,10001})
    {auto r=Request();r.chanceBp=chance;Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::InvalidDecision,"endpoints require zero roll; out-of-range chance rejected");}
    auto r=Request();r.chanceBp.reset();Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::InvalidDecision,"missing chance never silently becomes default");
    r=Request();r.stone.item=ItemKey::Create(7,999).value();Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::MissingItem,"missing stone rejected");
    auto retired=Fixture();retired.items[retired.items.size()-2].retired=true;
    Check(PrepareSocketStoneInsert(*BuildItemCatalog(retired).value(),Request()).error()==SocketStoneError::RetiredItem,"retired source rejected");
    r=Request();r.sockets={};Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::NoEmptySocket,"closed sockets cannot accept stones");
}
void SocketStateAndGroups()
{
    auto s=Fixture();AddOther(s);
    for(int pairing=0;pairing<4;++pairing)
    {
        auto conflicting=s;auto& other=conflicting.socketStones[2];
        if(pairing<2)other.primaryGroup=pairing==0 ? "stone.precision" : "stone.penetration";
        else other.secondaryGroup=pairing==2 ? "stone.precision" : "stone.penetration";
        auto r=Request();r.sockets[0]={StoneSocketState::Occupied,1,Other(),5};
        Check(PrepareSocketStoneInsert(*BuildItemCatalog(conflicting).value(),r).error()==SocketStoneError::ConflictingGroup,"all four cross-group intersections reject");
    }
    const auto catalog=BuildItemCatalog(s).value();auto r=Request();r.sockets[0]={StoneSocketState::Occupied,1,Other(),5};
    Check(PrepareSocketStoneInsert(*catalog,r).has_value(),"different groups coexist");
    r.sockets[2]=r.sockets[0];Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::InvalidSocketState,"pre-existing duplicate groups are corrupt state");
    for(int change=0;change<8;++change)
    {
        r=Request();auto& slot=r.sockets[0];
        if(change==0)slot.grade=1;
        if(change==1)slot.state=static_cast<StoneSocketState>(0);
        if(change==2)slot={StoneSocketState::Empty,0};
        if(change==3)slot={StoneSocketState::Empty,3};
        if(change==4)slot={StoneSocketState::Broken,1,Stone(),0};
        if(change==5)slot={StoneSocketState::Occupied,1,std::nullopt,0};
        if(change==6)slot={StoneSocketState::Occupied,1,Shard(),0};
        if(change==7)slot={StoneSocketState::Occupied,1,Other(),31};
        Check(PrepareSocketStoneInsert(*catalog,r).error()==SocketStoneError::InvalidSocketState,"malformed typed slot rejected");
    }
}
void CatalogValidationAndHash()
{
    auto bad=[](auto mutate){auto s=Fixture();mutate(s);Check(!BuildItemCatalog(s),"invalid stone catalog accepted");};
    bad([](auto& s){s.socketStones[0].allowedTargets=0;});bad([](auto& s){s.socketStones[0].allowedTargets=4;});
    bad([](auto& s){s.socketStones[0].minimumSocketGrade=0;});bad([](auto& s){s.socketStones[0].minimumSocketGrade=3;});
    bad([](auto& s){s.socketStones[0].primaryGroup="../group";});bad([](auto& s){s.socketStones[0].secondaryGroup=s.socketStones[0].primaryGroup;});
    bad([](auto& s){s.socketStones[1].allowedTargets=1;});bad([](auto& s){s.socketStones[1].primaryGroup="stone.shard";});
    bad([](auto& s){s.socketStones[0].kind=static_cast<SocketStoneKind>(3);});bad([](auto& s){s.socketStones.pop_back();});
    bad([](auto& s){s.socketStones.push_back(s.socketStones[0]);});bad([](auto& s){s.socketStones[0].key=f::Sword();});
    bad([](auto& s){s.items.back().baseBonusProfile=1;});bad([](auto& s){s.items.back().refinementProfile=5;});
    bad([](auto& s){s.refinements.back().levels[1].gain.physicalDefenseRaw=1000;});
    auto source=Fixture();const auto catalog=BuildItemCatalog(source).value();
    auto snapshot=catalog_snapshot_fixture::Snapshot(source);
    auto decoded=auriga::item::sql::DecodeSnapshot(snapshot,{2,catalog->ContentHash()}).value();
    Check(std::get<ItemSocketStoneRow>(decoded->Find(Stone())->category).secondaryGroup=="stone.penetration","SQL snapshot preserves cross-group metadata");
    auto reversed=source;std::ranges::reverse(reversed.socketStones);
    Check(BuildItemCatalog(reversed).value()->ContentHash()==catalog->ContentHash(),"stone row order normalizes");
    for(int field=0;field<4;++field)
    {
        auto changed=source;auto& row=changed.socketStones[0];
        if(field==0)row.allowedTargets=1;if(field==1)row.minimumSocketGrade=2;if(field==2)row.primaryGroup="stone.other";if(field==3)row.secondaryGroup="";
        Check(BuildItemCatalog(changed).value()->ContentHash()!=catalog->ContentHash(),"every stone compatibility field is hashed");
    }
}
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture")
        {const auto hash=BuildItemCatalog(Fixture()).value()->ContentHash();for(auto b:hash)std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cout<<'\n';return 0;}
        Plans();EconomicRefinement();Rejections();SocketStateAndGroups();CatalogValidationAndHash();
        std::cout<<"Socket stone checks passed: "<<checks<<'\n';return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
