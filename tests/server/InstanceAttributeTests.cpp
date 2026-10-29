#include "InstanceAttributeFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <iostream>
#include <algorithm>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;namespace a=attribute_fixture;
namespace
{
unsigned checks=0;
void Check(bool ok,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
template<class T,class E>T Value(std::expected<T,E> r,std::source_location at=std::source_location::current())
{if(!r)throw std::runtime_error("unexpected code "+std::to_string(static_cast<int>(r.error()))+" at "+std::to_string(at.line()));return std::move(*r);}
auto Catalog(ItemCatalogSource s=a::Catalog())
{auto r=BuildItemCatalog(std::move(s));if(!r)throw std::runtime_error("catalog field "+r.error().field);return *r;}
AttributeRequest Request(unsigned service,InstanceAttributes state={})
{
    SocketItemPair pair{{},{a::Tool(service),100,2,7,0,10},{f::Sword(),200,3,7,9,1},true,true};
    auto& c=pair.context;c.operationId[0]=1;c.actorId=7;c.sessionGeneration=8;c.rulesetRevision=2;c.inventoryRevision=4;c.characterLevel=50;c.admittedByServer=true;
    return {pair,state,0,0,{}};
}
InstanceAttributes Full(const ItemCatalog& c)
{
    InstanceAttributes s;
    for(unsigned i=0;i<5;++i){auto r=Request(1,s);r.draws={{5000,1}};auto p=Value(PlanAttributeService(c,r));Check(p.consumeUnits==1 && p.succeeded,"addition consumes exactly one");s=p.after;}
    return s;
}
void Lifecycle()
{
    const auto c=Catalog();auto s=Full(*c);auto r=Request(3,s);r.lockRoll=3;auto p=Value(PlanAttributeService(*c,r));
    Check(p.after.lockedSlot==2 && p.after.slots==s.slots,"lock changes only index");s=p.after;
    r=Request(2,s);r.draws.assign(4,{1,1});p=Value(PlanAttributeService(*c,r));
    Check(p.after.lockedSlot==2 && p.after.slots[2]==s.slots[2],"reroll leaves lock in its original position");
    Check(p.catalogHash==c->ContentHash() && p.expected.items.context.sessionGeneration==8 && p.expected.state==s,"owning snapshots and release pin");
    for(unsigned i=0;i<5;++i)Check(p.after.slots[i].has_value(),"normal count preserved");
    s=p.after;r=Request(4,s);r.lockRoll=1;p=Value(PlanAttributeService(*c,r));Check(p.after.lockedSlot==0 && p.after.slots==s.slots,"relock selects another occupied slot without retry");
    s=p.after;r=Request(5,s);p=Value(PlanAttributeService(*c,r));Check(!p.after.lockedSlot && p.after.slots==s.slots,"unlock leaves values unchanged");
    s=p.after;
    for(unsigned i=0;i<2;++i){r=Request(6,s);r.draws={{1,1}};s=Value(PlanAttributeService(*c,r)).after;}
    r=Request(8,s);r.draws={{10000,1}};s=Value(PlanAttributeService(*c,r)).after;
    Check(s.slots[5]->bonus!=s.slots[6]->bonus && s.slots[7]->bonus!=s.slots[5]->bonus && s.slots[7]->bonus!=s.slots[6]->bonus,"rare plus extra share uniqueness");
    r=Request(7,s);r.draws={{1,1},{1,1}};p=Value(PlanAttributeService(*c,r));Check(p.after.slots[7]==s.slots[7],"rare reroll preserves eighth bonus");
    for(unsigned i=0;i<5;++i)Check(p.after.slots[i]==s.slots[i],"rare service preserves normal group");
    Check(Value(InstanceAttributeContributions(*c,f::Sword(),s)).size()==8,"all eight instance contributions returned without aggregation");
    auto duplicate=s;duplicate.slots[0]=BonusEntry{BonusId::CritChance,100};
    Check(!ValidateInstanceAttributes(*c,f::Sword(),duplicate),"a stored normal value must belong to its pool allowlist");
}
void Decisions()
{
    auto c=Catalog();auto r=Request(1);r.draws={{4000,20}};auto p=Value(PlanAttributeService(*c,r));
    Check(p.after.slots[0]==BonusEntry{BonusId::MagicalAttackFlat,100},"inclusive tier and eligible weighted boundary");
    r.draws={{4001,21}};p=Value(PlanAttributeService(*c,r));Check(p.after.slots[0]==BonusEntry{BonusId::PhysicalDefenseFlat,200},"next tier and next weighted type boundary");
    auto source=a::Catalog();source.attributeProfiles[0].candidates[1].values.resize(1);c=Catalog(source);r.draws={{10000,1}};
    Check(Value(PlanAttributeService(*c,r)).after.slots[0]->valueScaled==100,"tier clamped to selected type maximum");
    source=a::Catalog();source.attributeServices[0].successBp=5000;source.attributeServices[0].consumeOnFailure=true;c=Catalog(source);r=Request(1);r.successRoll=5001;
    p=Value(PlanAttributeService(*c,r));Check(!p.succeeded && p.consumeUnits==1 && p.after==r.state,"failed consuming attempt retains state");
    r.successRoll=5000;r.draws={{1,1}};Check(Value(PlanAttributeService(*c,r)).succeeded,"inclusive success endpoint");
    source.attributeServices[0].consumeOnFailure=false;c=Catalog(source);r.successRoll=5001;r.draws.clear();Check(Value(PlanAttributeService(*c,r)).consumeUnits==0,"explicit free failure");
    source=a::Catalog();source.attributeServices[0].tiers={{1,7300}};c=Catalog(source);r=Request(1);r.draws={{7301,1}};
    Check(PlanAttributeService(*c,r).error()==AttributeError::UnassignedTier,"uncovered probability is not normalized or consumed");
    source=a::Catalog();source.attributeProfiles[0].groups[0].capacity=1;c=Catalog(source);r=Request(1);r.draws={{1,1}};auto s=Value(PlanAttributeService(*c,r)).after;
    r=Request(3,s);r.lockRoll=1;s=Value(PlanAttributeService(*c,r)).after;r=Request(4,s);
    Check(PlanAttributeService(*c,r).error()==AttributeError::NoAlternative,"relock cannot loop with no alternative");
    r=Request(2,s);Check(PlanAttributeService(*c,r).error()==AttributeError::NoEffect,"all-locked group cannot be rerolled");
}
void Rejections()
{
    const auto c=Catalog();auto full=Full(*c);
    for(unsigned i=0;i<17;++i)
    {
        auto r=Request(1);r.draws={{1,1}};auto error=AttributeError::InvalidDecision;
        switch(i){
        case 0:r.items.context.admittedByServer=false;error=AttributeError::AdmissionDenied;break;
        case 1:r.items.target.ownerId=8;error=AttributeError::AdmissionDenied;break;
        case 2:r.items.target.equipped=true;error=AttributeError::AdmissionDenied;break;
        case 3:r.items.context.sessionGeneration=0;error=AttributeError::AdmissionDenied;break;
        case 4:r.items.source.item=f::InstantPotion();error=AttributeError::WrongSource;break;
        case 5:r.items.target.item=f::Bracelet();r.items.target.refinementLevel=0;error=AttributeError::IncompatibleTarget;break;
        case 6:r.draws.clear();break;
        case 7:r.draws.push_back({1,1});break;
        case 8:r.draws[0].tierRoll=0;break;
        case 9:r.draws[0].tierRoll=10001;break;
        case 10:r.draws[0].bonusRoll=0;break;
        case 11:r.draws[0].bonusRoll=1000000;break;
        case 12:r.lockRoll=1;break;
        case 13:r.successRoll=1;break;
        case 14:r.state.lockedSlot=4;error=AttributeError::InvalidState;break;
        case 15:r.state=full;error=AttributeError::NoEffect;break;
        case 16:r.state.slots[0]=BonusEntry{BonusId::MagicalAttackFlat,0};error=AttributeError::InvalidState;break;
        }
        auto p=PlanAttributeService(*c,r);Check(!p && p.error()==error,"bad admission, state or recorded decision rejected");
    }
    auto r=Request(3,full);r.lockRoll=0;Check(PlanAttributeService(*c,r).error()==AttributeError::InvalidDecision,"lock needs one bounded choice");
    r=Request(3);r.lockRoll=1;Check(PlanAttributeService(*c,r).error()==AttributeError::NotEnoughAttributes,"first lock needs full group");
    r=Request(5,full);Check(PlanAttributeService(*c,r).error()==AttributeError::NotLocked,"unlock needs existing lock");
    auto source=a::Catalog();source.attributeServices[0].maximumTargetLevel=0;source.attributeServices[0].targetMask=2;
    Check(PlanAttributeService(*Catalog(source),Request(1)).error()==AttributeError::TargetDenied,"explicit category mask");
    source=a::Catalog();source.attributeServices[0].units=11;Check(PlanAttributeService(*Catalog(source),Request(1)).error()==AttributeError::InsufficientUnits,"no partial unit payment");
    source=a::Catalog();source.attributeServices[0].maximumTargetLevel=1;source.items[0].minimumLevel=2;
    Check(PlanAttributeService(*Catalog(source),Request(1)).error()==AttributeError::TargetDenied,"green-item target level ceiling is content");
}
void CodecAndPublication()
{
    const auto c=Catalog();auto s=a::PayloadFixture();const auto bytes=Value(EncodeInstanceAttributes(*c,f::Sword(),s));
    Check(Value(DecodeInstanceAttributes(*c,f::Sword(),bytes))==s,"signed values, gaps and lock round trip exactly");
    for(std::size_t i=0;i<bytes.size();++i)Check(!DecodeInstanceAttributes(*c,f::Sword(),std::span(bytes).first(i)),"every truncated payload rejected");
    auto bad=bytes;bad.push_back(0);Check(!DecodeInstanceAttributes(*c,f::Sword(),bad),"trailing bytes rejected");
    bad=bytes;bad[4]=2;Check(!DecodeInstanceAttributes(*c,f::Sword(),bad),"unknown codec rejected");
    bad=bytes;bad[13]=2;Check(!DecodeInstanceAttributes(*c,f::Sword(),bad),"noncanonical bool rejected");
    Check(!DecodeInstanceAttributes(*c,f::Armor(),bytes),"payload bound to exact ItemKey");
    auto empty=Value(EncodeInstanceAttributes(*c,f::Leather(),{}));Check(Value(DecodeInstanceAttributes(*c,f::Leather(),empty))==InstanceAttributes{},"unbound item accepts only empty attribute state");
    auto snapshot=catalog_snapshot_fixture::Snapshot(a::Catalog());Check(sql::DecodeSnapshot(snapshot,{1,c->ContentHash()}).has_value(),"all typed SQL tables round trip");
    auto cross=a::Catalog();cross.attributeProfiles[0].candidates.push_back({AttributePool::Normal,BonusId::CritChance,1,{{1,100}}});
    auto crossState=InstanceAttributes{};crossState.slots[0]=BonusEntry{BonusId::CritChance,100};crossState.slots[5]=crossState.slots[0];
    auto contributions=Value(InstanceAttributeContributions(*Catalog(cross),f::Sword(),crossState));
    Check(contributions.size()==2 && contributions[0]==contributions[1],"same bonus in normal and rare remains two separate sources");
    auto source=a::Catalog();std::ranges::reverse(source.attributeProfiles[0].candidates);std::ranges::reverse(source.attributeServices);std::ranges::reverse(source.attributeBindings);
    Check(Catalog(source)->ContentHash()==c->ContentHash(),"content order cannot change hash");
    for(unsigned i=0;i<10;++i){auto invalid=a::Catalog();auto& p=invalid.attributeProfiles[0];
        switch(i){case 0:p.groups[0].capacity=6;break;case 1:p.candidates[0].weight=0;break;case 2:p.candidates[0].values[0].value=0;break;
        case 3:p.candidates[0].values[1].tier=3;break;case 4:p.candidates.push_back(p.candidates[0]);break;
        case 5:invalid.attributeBindings.push_back(invalid.attributeBindings[0]);break;case 6:invalid.attributeServices[0].tiers={{1,10001}};break;
        case 7:invalid.attributeServices[2].targetMask=8;break;case 8:invalid.attributeServices[1].consumeOnFailure=true;break;
        case 9:invalid.consumables.back().recoveryProfile=1;break;}
        Check(!BuildItemCatalog(invalid),"invalid content never publishes");}
}
}
int main(int argc,char** argv)
{
    try{
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture"){for(auto b:Catalog(a::HashFixture())->ContentHash())std::printf("%02x",b);std::puts("");return 0;}
        if(argc==2 && std::string_view(argv[1])=="--state-fixture"){for(auto b:Value(EncodeInstanceAttributes(*Catalog(),f::Sword(),a::PayloadFixture())))std::printf("%02x",b);std::puts("");return 0;}
        Lifecycle();Decisions();Rejections();CodecAndPublication();std::cout<<checks<<" Instance attribute checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
