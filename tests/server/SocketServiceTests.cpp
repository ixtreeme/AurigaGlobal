#include "SocketServiceFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace sf=socket_service_fixture;
namespace
{
unsigned checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
template<class T,class E> T Value(std::expected<T,E> result,std::source_location at=std::source_location::current())
{
    if(!result)throw std::runtime_error("unexpected rejection code="+std::to_string(static_cast<int>(result.error()))+" at "+std::to_string(at.line()));
    return std::move(*result);
}
std::shared_ptr<const ItemCatalog> CatalogOf(ItemCatalogSource source,std::source_location at=std::source_location::current())
{
    auto result=BuildItemCatalog(std::move(source));
    if(!result)throw std::runtime_error("catalog field="+result.error().field+" at "+std::to_string(at.line()));
    return *result;
}
SocketItemPair Pair(ItemKey source)
{
    SocketItemPair p{{},{source},{f::Sword()}};p.context.operationId[0]=1;p.context.actorId=7;p.context.sessionGeneration=8;
    p.context.rulesetRevision=2;p.context.inventoryRevision=3;p.context.characterLevel=50;p.context.admittedByServer=true;
    p.source={source,100,5,7,0,4};p.target={f::Sword(),200,6,7,9,1};p.sourceInInventory=p.targetInInventory=true;return p;
}
SocketServiceRequest Request(unsigned kind)
{
    SocketServiceRequest r{Pair(sf::Tool(kind))};
    r.sockets={StoneSocketSlot{StoneSocketState::Occupied,2,sf::Stone(),30},
               StoneSocketSlot{StoneSocketState::Broken,1},StoneSocketSlot{StoneSocketState::Occupied,1,sf::Stone(2),10}};
    if(kind==2)r.returns={{2,302,402},{0,300,400}}; // Reservation order is not slot order.
    if(kind==3)r.returns={{2,302,402}};
    return r;
}
void Maintenance()
{
    const auto c=CatalogOf(sf::Catalog());
    for(unsigned kind=1;kind<=3;++kind)
    {
        auto r=Request(kind);auto p=Value(PlanSocketService(*c,r));
        Check(p.consumeUnits==1 && r.items.source.count==4,"one unit proposed without input mutation");
        Check(p.catalogHash==c->ContentHash() && p.catalogRevision==c->Revision(),"catalog pin retained");
        Check(p.expected.items.context.sessionGeneration==8 && p.expected.items.context.inventoryRevision==3 && p.expected.sockets==r.sockets,"full preconditions owned");
        for(unsigned i=0;i<3;++i)Check(p.after[i].grade==r.sockets[i].grade,"socket grade is never downgraded");
        if(kind==1)
            Check(p.returns.empty() && p.after[1].state==StoneSocketState::Empty && p.after[0]==r.sockets[0] && p.after[2]==r.sockets[2],"clean only repairs broken slots in place");
        if(kind==2)
        {
            Check(p.returns.size()==2 && p.returns[0].socket==0 && p.returns[0].item==sf::Stone() && p.returns[0].refinementLevel==30,"all extraction retains first stone +30");
            Check(p.returns[1].item==sf::Stone(2) && p.returns[1].refinementLevel==10 && p.returns[1].instanceId==302,"all extraction retains second stone and reservation");
            Check(p.after[0].state==StoneSocketState::Broken && p.after[1]==r.sockets[1] && p.after[2].state==StoneSocketState::Broken,"all extraction leaves typed breakage");
        }
        if(kind==3)
            Check(p.returns.size()==1 && p.returns[0].socket==2 && p.after[0]==r.sockets[0] && p.after[2].state==StoneSocketState::Empty,"last extraction skips prior broken slot and keeps other stones");
        r.sockets={};r.returns.clear();Check(PlanSocketService(*c,r).error()==SocketServiceError::NoEffect,"no-op never consumes");
    }
    auto r=Request(3);r.sockets[2]={};r.returns={{0,300,400}};
    Check(PlanSocketService(*c,r)->returns[0].socket==0,"slot zero is valid and last selection skips closed suffix");
    auto retired=sf::Catalog();std::ranges::find(retired.items,sf::Stone(),&ItemBaseRow::key)->retired=true;
    Check(PlanSocketService(*CatalogOf(retired),Request(2))->returns[0].item==sf::Stone(),"known retired embedded stone can be recovered without losing ownership");
}
void Rejections()
{
    auto c=CatalogOf(sf::Catalog());
    for(unsigned i=0;i<31;++i)
    {
        auto r=Request(2);auto error=SocketServiceError::InvalidSnapshot;
        switch(i)
        {
        case 0:r.items.context.admittedByServer=false;error=SocketServiceError::Forbidden;break;
        case 1:r.items.context.observer=true;error=SocketServiceError::Forbidden;break;
        case 2:r.items.context.actorId=0;error=SocketServiceError::InvalidOperation;break;
        case 3:r.items.context.sessionGeneration=0;error=SocketServiceError::InvalidOperation;break;
        case 4:r.items.context.rulesetRevision=0;error=SocketServiceError::InvalidOperation;break;
        case 5:r.items.context.inventoryRevision=0;error=SocketServiceError::InvalidOperation;break;
        case 6:r.items.context.inventoryRevision=UINT64_MAX;error=SocketServiceError::InvalidOperation;break;
        case 7:r.items.context.operationId={};error=SocketServiceError::InvalidOperation;break;
        case 8:r.items.sourceInInventory=false;break;
        case 9:r.items.targetInInventory=false;break;
        case 10:r.items.source.instanceId=200;break;
        case 11:r.items.source.ownerId=8;break;
        case 12:r.items.target.revision=UINT64_MAX;break;
        case 13:r.items.target.revision=0;break;
        case 14:r.items.source.locked=true;break;
        case 15:r.items.target.equipped=true;break;
        case 16:r.items.source.exchanging=true;break;
        case 17:r.items.source.count=0;error=SocketServiceError::InvalidStack;break;
        case 18:r.items.source.count=201;error=SocketServiceError::InvalidStack;break;
        case 19:r.items.target.count=2;error=SocketServiceError::InvalidStack;break;
        case 20:r.items.source.refinementLevel=1;error=SocketServiceError::InvalidLevel;break;
        case 21:r.items.target.refinementLevel=10;error=SocketServiceError::InvalidLevel;break;
        case 22:r.items.context.characterLevel=9;error=SocketServiceError::InvalidLevel;break;
        case 23:r.items.context.characterClass=4;error=SocketServiceError::Forbidden;break;
        case 24:r.items.context.sex=2;error=SocketServiceError::Forbidden;break;
        case 25:r.items.target.item=f::Bracelet();r.items.target.refinementLevel=0;error=SocketServiceError::IncompatibleTarget;break;
        case 26:r.items.source.item=f::InstantPotion();error=SocketServiceError::WrongSource;break;
        case 27:r.items.target.item=ItemKey::Create(1,999).value();error=SocketServiceError::MissingItem;break;
        case 28:r.sockets[0].refinementLevel=31;error=SocketServiceError::InvalidState;break;
        case 29:r.sockets[1].grade=0;error=SocketServiceError::InvalidState;break;
        case 30:r.sockets[2]=r.sockets[0];error=SocketServiceError::InvalidState;break;
        }
        auto result=PlanSocketService(*c,r);Check(!result && result.error()==error,"invalid admission/state rejected");
    }
    for(unsigned i=0;i<9;++i)
    {
        auto r=Request(2);
        switch(i)
        {
        case 0:r.returns.clear();break;
        case 1:r.returns.push_back({1,301,401});break;
        case 2:r.returns[0].socket=3;break;
        case 3:r.returns[0].socket=0;break;
        case 4:r.returns[0].instanceId=100;break;
        case 5:r.returns[0].instanceId=0;break;
        case 6:r.returns[0].placementToken=0;break;
        case 7:r.returns[0].placementToken=r.returns[1].placementToken;break;
        case 8:r.returns[0].instanceId=r.returns[1].instanceId;break;
        }
        Check(PlanSocketService(*c,r).error()==SocketServiceError::InvalidReservation,"missing, duplicate, aliased or extra reservations reject entire operation");
    }
    auto r=Request(1);r.returns={{0,300,400}};Check(PlanSocketService(*c,r).error()==SocketServiceError::InvalidReservation,"clean cannot mint returned stones");
    auto source=sf::Catalog();source.socketServiceProfiles[0].unitsPerUse=5;c=CatalogOf(source);
    r=Request(1);Check(PlanSocketService(*c,r).error()==SocketServiceError::InvalidStack,"insufficient tool units rejected");
    source=sf::Catalog();source.usePolicies[2].allowedClasses=2;c=CatalogOf(source);
    Check(PlanSocketService(*c,Request(1)).error()==SocketServiceError::Forbidden,"source class policy enforced");
    source=sf::Catalog();source.usePolicies[2].allowedSexes=2;c=CatalogOf(source);
    Check(PlanSocketService(*c,Request(1)).error()==SocketServiceError::Forbidden,"source sex policy enforced");
    source=sf::Catalog();source.usePolicies[2].useDuringCombat=false;c=CatalogOf(source);
    r=Request(1);r.items.context.inCombat=true;Check(PlanSocketService(*c,r).error()==SocketServiceError::Forbidden,"combat policy enforced");
    source=sf::Catalog();std::ranges::find(source.items,sf::Tool(1),&ItemBaseRow::key)->retired=true;c=CatalogOf(source);
    Check(PlanSocketService(*c,Request(1)).error()==SocketServiceError::RetiredItem,"retired source rejected");
}
AccessoryServiceRequest Accessory()
{
    AccessoryServiceRequest r{Pair(f::SocketDrill())};r.items.target.item=f::Bracelet();r.items.target.refinementLevel=15;
    r.kind=AccessoryServiceKind::Unlock;r.recordedRoll=5000;return r;
}
void AccessoryLifecycle()
{
    auto source=sf::Catalog();source.accessorySockets[0].permanentChargeMaterials={f::Leather()};source.materials[0].kind=MaterialKind::Jewel;
    auto c=CatalogOf(source);auto r=Accessory();
    auto p=Value(PlanAccessoryService(*c,r));Check(p.succeeded && p.after.unlockedCapacity==1 && p.consumeUnits==1,"unlock uses explicit target chance");
    Check(p.catalogHash==c->ContentHash() && p.expected.items.target.refinementLevel==15,"accessory plan owns target state and catalog pin");
    r.recordedRoll=5001;p=Value(PlanAccessoryService(*c,r));Check(!p.succeeded && p.after==r.state && p.consumeUnits==1,"failed unlock consumes one without opening");
    r.recordedRoll=5000;r.state=PlanAccessoryService(*c,r)->after;
    r.kind=AccessoryServiceKind::Charge;r.items.source.item=f::SocketJewel();r.recordedRoll=0;
    p=Value(PlanAccessoryService(*c,r));Check(p.succeeded && p.after.activeGrade==1 && p.after.remainingMs==86400000 && !p.after.permanent,"timed charge sets catalog duration");
    r.state=p.after;Check(PlanAccessoryService(*c,r).error()==SocketServiceError::NoEffect,"full charge is not consumed");
    r.state={1,0,0,false};r.items.source.item=f::Leather();p=Value(PlanAccessoryService(*c,r));
    Check(p.after.permanent && p.after.remainingMs==0,"permanent charge is explicit without seconds sentinel");
    r.state={2,1,86400000,false};Check(PlanAccessoryService(*c,r).error()==SocketServiceError::LifetimeMismatch,"timed and permanent cannot mix");
    r=Accessory();r.items.source.item=f::Leather();Check(PlanAccessoryService(*c,r).error()==SocketServiceError::MaterialNotAllowed,"wrong drilling material rejected");
    r=Accessory();r.items.source.item=sf::Tool(1);Check(PlanAccessoryService(*c,r).error()==SocketServiceError::WrongSource,"service consumable cannot act as material");
    r=Accessory();r.state={1,2,1,false};Check(PlanAccessoryService(*c,r).error()==SocketServiceError::InvalidState,"capacity mismatch rejected");
    for(auto roll:{0,10001}){r=Accessory();r.recordedRoll=roll;Check(PlanAccessoryService(*c,r).error()==SocketServiceError::InvalidDecision,"invalid noncertain roll rejected");}
    r=Accessory();r.kind=static_cast<AccessoryServiceKind>(0);Check(PlanAccessoryService(*c,r).error()==SocketServiceError::InvalidOperation,"unknown accessory operation rejected");
    source.accessorySockets[0].unlockUnitsPerAttempt=5;c=CatalogOf(source);
    Check(PlanAccessoryService(*c,Accessory()).error()==SocketServiceError::InvalidStack,"insufficient accessory units rejected");
    source.accessorySockets[0].grades[0].unlockChanceBp=0;c=CatalogOf(source);r=Accessory();r.recordedRoll=0;
    Check(PlanAccessoryService(*c,r).error()==SocketServiceError::OperationDisabled,"zero chance does not consume");
}
void CompleteStonePath()
{
    const auto c=CatalogOf(sf::Catalog());
    auto r=Request(2);auto extracted=Value(PlanSocketService(*c,r));
    auto clean=Request(1);clean.sockets=extracted.after;auto repaired=Value(PlanSocketService(*c,clean));
    Check(repaired.returns.empty() && repaired.after[0].grade==2,"extraction then clean retains gold slot");
    SocketStoneInsertRequest insert{{sf::Stone()},{f::Sword()}};insert.actorId=7;insert.rulesetRevision=2;insert.operationId[0]=2;
    insert.stone={extracted.returns[0].item,300,1,7,extracted.returns[0].refinementLevel,1};
    insert.target=r.items.target;insert.sockets=repaired.after;insert.admittedByServer=true;insert.characterLevel=50;insert.chanceBp=10000;
    auto restored=Value(PrepareSocketStoneInsert(*c,insert));
    Check(restored.slot==0 && restored.after==r.sockets[0],"extract, clean and reinsert preserves key, +30, grade and slot");
    Check(extracted.returns.size()==2 && extracted.returns[0].instanceId!=extracted.returns[1].instanceId,"one returned stone per original occupied slot");
}
void Catalog()
{
    const auto s=sf::Catalog();auto c=CatalogOf(s);
    auto snap=catalog_snapshot_fixture::Snapshot(s);
    auto decoded=sql::DecodeSnapshot(snap,{1,c->ContentHash()});Check(decoded.has_value(),"new SQL table and consumable reference round trip");
    for(unsigned i=0;i<6;++i)
    {
        auto broken=s;
        if(i==0)broken.socketServiceProfiles[0].kind=static_cast<SocketServiceKind>(0);
        if(i==1)broken.socketServiceProfiles[0].unitsPerUse=0;
        if(i==2)broken.socketServiceProfiles[0].id=0;
        if(i==3)broken.socketServiceProfiles[0].code="invalid code";
        if(i==4)broken.consumables.back().recoveryProfile=1;
        if(i==5)broken.consumables.back().socketServiceProfile=999;
        Check(!BuildItemCatalog(broken),"malformed service cannot publish");
    }
    auto changed=s;changed.socketServiceProfiles[0].unitsPerUse=2;
    Check(CatalogOf(changed)->ContentHash()!=c->ContentHash(),"service units in canonical digest");
    changed=s;changed.socketServiceProfiles[0].kind=SocketServiceKind::ExtractLast;
    Check(CatalogOf(changed)->ContentHash()!=c->ContentHash(),"service kind in canonical digest");
    std::ranges::reverse(changed.socketServiceProfiles);std::ranges::reverse(changed.consumables);
    auto ordered=s;std::ranges::reverse(ordered.socketServiceProfiles);std::ranges::reverse(ordered.consumables);
    Check(CatalogOf(ordered)->ContentHash()==c->ContentHash(),"input ordering cannot change release hash");
}
}
int main(int argc,char** argv)
{
    try
    {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture")
        {auto s=f::Catalog();for(unsigned i=1;i<=3;++i){s.socketServiceProfiles.push_back({i,"socket.service_"+std::to_string(i),static_cast<SocketServiceKind>(i),1});s.consumables[i-1].recoveryProfile=0;s.consumables[i-1].socketServiceProfile=i;}const auto c=CatalogOf(s);for(auto b:c->ContentHash())std::printf("%02x",b);std::puts("");return 0;}
        Maintenance();Rejections();AccessoryLifecycle();CompleteStonePath();Catalog();
        std::cout<<checks<<" Socket service checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
