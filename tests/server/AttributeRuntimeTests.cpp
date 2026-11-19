#include "AttributeRuntimeFixture.hpp"
#include <item/AttributeRuntime.hpp>
#include <item/EquipmentStats.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;namespace f=persistent_item_fixture;namespace af=attribute_runtime_fixture;
namespace
{
unsigned checks=0;
void Check(bool good,const char* why,std::source_location at=std::source_location::current()){++checks;if(!good)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}

void ReusableRareRun()
{
    auto definition=af::Catalog();auto& service=*std::ranges::find(definition.attributeServices,7,&AttributeServiceProfile::id);service.units=0;service.minimumCount=1;
    auto catalog=BuildItemCatalog(definition);Check(catalog.has_value(),"audited reusable rare reroller publishes");const auto& c=**catalog;
    const ItemOwnerFence fence{f::Character(),7,2};const SocketMaintenanceActor actor{100,1,1,false};const std::vector layouts{f::Inventory()};auto target=f::Sword();auto tool=f::Leather(70);tool.key=attribute_fixture::Tool(7);tool.count=1;std::vector items{target,tool};
    const auto q=PrepareAttributeRuntime(c,fence,items,tool.id,target.id,actor);Check(q && q->units==0 && q->available==1 && q->pool==2 && q->draws==1,"reusable tool owns a positive stack and quotes zero debit");
    unsigned calls=0;auto result=MakeAttributeRuntimeCommit(c,{98},fence,layouts,items,tool.id,target.id,actor,*q,[&](auto){++calls;return 1;});Check(result && result->succeeded && calls==2,"reusable tool executes exact independent rare draws");Check(result->commit.items.size()==1 && result->commit.items[0].before->id==target.id && result->commit.items[0].after.id==target.id,"only target mutates; tool identity count and revision stay exact");Check(result->after.slots[0]==target.attributes.slots[0] && result->after.lockedSlot==target.attributes.lockedSlot,"reusable rare reroll preserves normal rows and lock");Check(ValidateItemCommit(c,result->commit).has_value(),"zero debit has a complete verifiable durable outcome receipt");
    items[0]=result->commit.items[0].after;const auto q2=PrepareAttributeRuntime(c,fence,items,tool.id,target.id,actor);Check(q2 && MakeAttributeRuntimeCommit(c,{98,1},fence,layouts,items,tool.id,target.id,actor,*q2,[](auto max){return max;}),"same last tool can perform a second distinct operation");
    items[1].count=0;Check(!PrepareAttributeRuntime(c,fence,items,tool.id,target.id,actor),"zero debit never authorizes an empty tool instance");items[1]=tool;items[0].attributes.slots[5].reset();Check(PrepareAttributeRuntime(c,fence,items,tool.id,target.id,actor).error()==AttributeRuntimeStatus::NotEnough,"absent rare attributes do not fabricate a free add");
    for(unsigned kind:{1u,2u,3u,4u,5u,6u,8u,9u}){auto bad=af::Catalog();std::ranges::find(bad.attributeServices,kind,&AttributeServiceProfile::id)->units=0;Check(!BuildItemCatalog(bad),"zero debit cannot leak to another service family");}
    for(unsigned bit:{8u,16u}){auto bad=definition;std::ranges::find(bad.attributeServices,7,&AttributeServiceProfile::id)->targetMask|=bit;Check(!BuildItemCatalog(bad),"reusable original excludes costume and pendant target expansion");}
}

std::vector<std::byte> AddonRun()
{
    auto definition=af::Catalog();
    auto& profile=definition.attributeProfiles.front();
    for(auto bonus:{BonusId::NormalDamagePercent,BonusId::SkillDamagePercent})profile.candidates.push_back({AttributePool::Normal,bonus,0,{{1,100}}});
    ItemDefinitionRules rules{item_catalog_fixture::Sword()};rules.birthAddon=1;rules.attributeRerollGroup=2;definition.definitionRules.push_back(rules);
    std::ranges::find(definition.attributeServices,2,&AttributeServiceProfile::id)->targetRerollGroup=2;
    const auto catalog=BuildItemCatalog(definition).value();const auto& c=*catalog;
    const ItemOwnerFence fence{f::Character(),7,2};const SocketMaintenanceActor actor{100,1,1,false};const std::vector layouts{f::Inventory()};
    auto target=f::Sword();target.attributes=af::Full();target.attributes.slots[5]=BonusEntry{BonusId::CritChance,100};
    auto tool=f::Leather(70);tool.key=attribute_fixture::Tool(2);tool.count=10;std::vector items{target,tool};
    const auto quote=[&]{return PrepareAttributeRuntime(c,fence,items,tool.id,target.id,actor);};
    auto q=quote();Check(q && q->pairedAddon && q->draws==5,"addon quote declares pair regeneration");
    Check(std::ranges::none_of(q->choices,[](const auto& b){return !b.weight || b.bonus==BonusId::NormalDamagePercent || b.bonus==BonusId::SkillDamagePercent;}),"forced addon types never appear as ordinary random choices");
    unsigned entropy=0;
    auto result=MakeAttributeRuntimeCommit(c,{99},fence,layouts,items,tool.id,target.id,actor,*q,[&](auto){++entropy;return 1;});
    Check(result && entropy==11,"five recorded addon decisions and three ordinary draws");
    const auto find=[&](const InstanceAttributes& a,BonusId id){for(const auto& b:a.slots)if(b && b->bonus==id)return b->valueScaled;return std::int64_t{0};};
    Check(find(result->after,BonusId::SkillDamagePercent)==3000 && find(result->after,BonusId::NormalDamagePercent)==-5900,"source Gaussian clamp and signed paired damage values");
    Check(result->after.slots[5]==target.attributes.slots[5] && result->commit.items[1].after.count==9,"rare attributes preserved and one tool debited");
    Check(ValidateItemCommit(c,result->commit).has_value() && result->commit.result[3]==2,"versioned receipt includes addon entropy");
    unsigned call=0;
    const auto zero=MakeAttributeRuntimeCommit(c,{99,1},fence,layouts,items,tool.id,target.id,actor,*q,[&](auto maximum){++call;return call==1 ? UINT32_MAX : call==2 ? UINT32_MAX/4 : 1u;});
    Check(zero && call==13 && find(zero->after,BonusId::SkillDamagePercent)==0 && find(zero->after,BonusId::NormalDamagePercent)==1700,"zero skill bonus is omitted and fourth ordinary attribute fills its place");
    items[0].attributes.lockedSlot=2;
    const auto lockedQ=quote();const auto locked=MakeAttributeRuntimeCommit(c,{99,2},fence,layouts,items,tool.id,target.id,actor,*lockedQ,[](auto){return 1;});
    Check(locked && locked->after.slots[2]==items[0].attributes.slots[2] && locked->after.lockedSlot==2,"ordinary locked bonus survives paired regeneration");
    items[0].attributes.slots[0]=BonusEntry{BonusId::SkillDamagePercent,-500};items[0].attributes.lockedSlot=0;
    const auto pairLocked=MakeAttributeRuntimeCommit(c,{99,3},fence,layouts,items,tool.id,target.id,actor,*quote(),[](auto){return 1;});
    Check(pairLocked && find(pairLocked->after,BonusId::SkillDamagePercent)==-500 && find(pairLocked->after,BonusId::NormalDamagePercent)==-5900,"locked paired bonus is retained without duplicate");
    items[0]=target;items[0].attributes={};items[0].attributes.slots[0]=BonusEntry{BonusId::MagicalAttackFlat,100};
    const auto one=MakeAttributeRuntimeCommit(c,{99,4},fence,layouts,items,tool.id,target.id,actor,*quote(),[](auto){return 1;});
    Check(one && std::ranges::count_if(one->after.slots.begin(),one->after.slots.begin()+5,[](const auto& b){return b.has_value();})==2,"source addon can establish its pair on a one-attribute weapon");
    items[0]=target;
    auto ordinary=definition;std::ranges::find(ordinary.attributeServices,2,&AttributeServiceProfile::id)->targetRerollGroup=1;const auto ordinaryCatalog=BuildItemCatalog(ordinary).value();
    Check(ordinaryCatalog->ContentHash()!=c.ContentHash(),"switcher target group is covered by catalog digest");
    auto otherGroup=definition;otherGroup.definitionRules.back().attributeRerollGroup=1;Check(BuildItemCatalog(otherGroup).value()->ContentHash()!=c.ContentHash(),"item reroll group is covered by catalog digest");
    Check(PrepareAttributeRuntime(*ordinaryCatalog,fence,items,tool.id,target.id,actor).error()==AttributeRuntimeStatus::Incompatible,"ordinary switcher refuses zodiac target");
    auto green=definition;std::ranges::find(green.attributeServices,2,&AttributeServiceProfile::id)->maximumTargetLevel=30;std::ranges::find(green.items,item_catalog_fixture::Sword(),&ItemBaseRow::key)->minimumLevel=31;const auto greenCatalog=BuildItemCatalog(green).value();
    Check(PrepareAttributeRuntime(*greenCatalog,fence,items,tool.id,target.id,actor).error()==AttributeRuntimeStatus::Incompatible,"green switcher respects target level ceiling");
    entropy=0;const auto failed=MakeAttributeRuntimeCommit(c,{99,5},fence,layouts,items,tool.id,target.id,actor,*q,[&](auto)->std::optional<std::uint32_t>{++entropy;return entropy==3 ? std::nullopt : std::optional<std::uint32_t>{1};});
    Check(!failed && items[0]==target && items[1]==tool,"entropy failure creates no debit or partial state");
    const std::array targets{target.id.Value()};const auto packet=EncodeAttributeRuntime(c,900,&tool,&target,2,AttributeRuntimeStatus::Applied,*q,&zero->after,targets);
    Check(packet && std::to_integer<unsigned>((*packet)[5])==2,"paired outcome uses GC236 v2");return *packet;
}

std::vector<std::byte> Run()
{
    auto definition=af::Catalog();auto catalog=BuildItemCatalog(definition).value();const auto& c=*catalog;const ItemOwnerFence fence{f::Character(),7,2};const SocketMaintenanceActor actor{100,1,1,false};const std::vector layouts{f::Inventory()};
    auto target=f::Sword();auto tool=f::Leather(70);tool.key=attribute_fixture::Tool(1);tool.count=10;std::vector items{target,tool};unsigned entropy=0;
    const auto quote=[&]{return PrepareAttributeRuntime(c,fence,items,tool.id,target.id,actor);};
    const auto make=[&](const AttributeRuntimeQuote& q,const AttributeRandom& random){return MakeAttributeRuntimeCommit(c,{18},fence,layouts,items,tool.id,target.id,actor,q,[&](auto max){++entropy;return random(max);});};
    auto q=quote();Check(q && q->operation==1 && q->draws==1 && q->before==target.attributes && q->choices.size()==4 && entropy==0,"preview contains complete deterministic random domain without drawing");
    Check(std::ranges::none_of(q->choices,[](const auto& choice){return choice.bonus==BonusId::PhysicalAttackFlat || choice.bonus==BonusId::NormalOutgoing || choice.bonus==BonusId::MagicalAttackFlat || choice.bonus==BonusId::PhysicalDefenseFlat;}),"static, existing and protected normal bonus types excluded");
    auto added=make(*q,[](auto max){return max;});Check(added && added->succeeded && entropy==2 && added->after.slots[1]==BonusEntry{BonusId::SkillOutgoing,200},"independent tier/type draws use exact shrinking weight range and clamp values");
    Check(added->after.slots[0]==target.attributes.slots[0] && added->after.lockedSlot==target.attributes.lockedSlot && added->after.slots[2]==target.attributes.slots[2] && added->after.slots[5]==target.attributes.slots[5],"add preserves all old slots and protection");
    Check(added->commit.items[0].after.id==target.id && added->commit.items[0].after.stones==target.stones && added->commit.items[0].after.refinementLevel==9 && added->commit.items[1].after.count==9 && added->commit.wallets.empty(),"identity, level, stones and wallet preserved with exact debit");
    Check(added->commit.result.size()>8 && std::string(added->commit.result.begin(),added->commit.result.begin()+3)=="ATR" && ValidateItemCommit(c,added->commit).has_value(),"receipt owns recorded random decisions and after state");
    const auto golden=EncodeAttributeRuntime(c,0x123456789abcdefULL,&tool,&target,2,AttributeRuntimeStatus::Ready,*q,nullptr,std::array{target.id.Value()});Check(golden && std::to_integer<unsigned>((*golden)[0])==236,"new attribute packet embeds v2 metadata");
    auto stale=*q;++stale.units;entropy=0;Check(!make(stale,[](auto){return 1;}) && entropy==0,"quote mismatch never draws");
    Check(!MakeAttributeRuntimeCommit(c,{},fence,layouts,items,tool.id,target.id,actor,*q,[&](auto){++entropy;return 1;}) && entropy==0,"zero operation rejected before RNG");
    Check(!MakeAttributeRuntimeCommit(c,{18},fence,{},items,tool.id,target.id,actor,*q,[&](auto){++entropy;return 1;}) && entropy==0,"missing layout rejected before RNG");
    Check(!make(*q,[](auto){return 0;}) && !make(*q,[](auto max){return max+1;}) && !make(*q,[](auto)->std::optional<std::uint32_t>{return {};}) ,"entropy provider failures and out-of-range draws cannot commit");
    items[1].key=attribute_fixture::Tool(2);auto reroll=quote();Check(reroll && reroll->draws==1 && reroll->choices.size()==5,"reroll replaces only unprotected rows and makes their types eligible again");auto rolled=make(*reroll,[](auto){return 1;});Check(rolled && rolled->after.slots[0]==BonusEntry{BonusId::MagicalAttackFlat,100} && rolled->after.slots[2]==target.attributes.slots[2] && rolled->after.slots[5]==target.attributes.slots[5],"reroll retains locked value, slot and all other pools");
    items[0].attributes=af::Full();items[1].key=attribute_fixture::Tool(3);auto lock=quote();entropy=0;auto locked=make(*lock,[](auto max){return max;});Check(locked && entropy==1 && locked->after.lockedSlot==4 && locked->after.slots==items[0].attributes.slots,"lock uniformly selects one occupied normal slot without changing values");
    items[0].attributes=locked->after;items[1].key=attribute_fixture::Tool(4);auto relock=quote();Check(relock && relock->locks==std::vector<std::uint8_t>{0,1,2,3},"relock cannot choose the current protected slot");auto relocked=make(*relock,[](auto){return 3;});Check(relocked && relocked->after.lockedSlot==2 && relocked->after.slots==items[0].attributes.slots,"relock changes only protection");
    items[0].attributes=relocked->after;items[1].key=attribute_fixture::Tool(5);entropy=0;auto unlock=quote();auto unlocked=make(*unlock,[](auto){return 1;});Check(unlocked && entropy==0 && !unlocked->after.lockedSlot && unlocked->after.slots==items[0].attributes.slots,"unlock uses no entropy and preserves all values");
    items[0]=target;items[1].key=attribute_fixture::Tool(6);auto rare=quote();Check(rare && rare->pool==2 && rare->choices.size()==2,"rare add excludes existing rare IDs");auto rareAdded=make(*rare,[](auto max){return max;});Check(rareAdded && rareAdded->after.slots[6]==BonusEntry{BonusId::CritMultiplierExtra,200} && rareAdded->after.lockedSlot==target.attributes.lockedSlot,"rare add retains protected normal state");
    items[0].attributes=rareAdded->after;items[1].key=attribute_fixture::Tool(8);auto extra=quote();Check(extra && extra->pool==3 && extra->choices.size()==1 && extra->choices[0].bonus==BonusId::CritResistance,"rare and extra share a duplicate exclusion domain");auto extraAdded=make(*extra,[](auto){return 1;});Check(extraAdded && extraAdded->after.slots[7]==BonusEntry{BonusId::CritResistance,100},"extra slot has its own capacity");
    items[0].attributes=extraAdded->after;items[1].key=attribute_fixture::Tool(9);auto extraReroll=quote();Check(extraReroll && extraReroll->draws==1 && extraReroll->choices.size()==1,"extra reroll excludes both existing rare rows");Check(make(*extraReroll,[](auto){return 1;})->after.slots[5]==items[0].attributes.slots[5],"extra reroll cannot touch a rare row");
    items[0]=target;items[1]=tool;items[1].count=1;auto whole=make(*quote(),[](auto){return 1;});Check(whole && whole->commit.items[1].after.destroyed && !whole->commit.items[1].after.placement,"last tool becomes a tombstone exactly once");items[1]=tool;
    auto probabilistic=definition;probabilistic.attributeServices[0].successBp=5000;probabilistic.attributeServices[0].consumeOnFailure=true;const auto pc=BuildItemCatalog(probabilistic).value();const auto pq=PrepareAttributeRuntime(*pc,fence,items,tool.id,target.id,actor).value();entropy=0;
    auto failure=MakeAttributeRuntimeCommit(*pc,{18,1},fence,layouts,items,tool.id,target.id,actor,pq,[&](auto max){++entropy;return max;});Check(failure && !failure->succeeded && entropy==1 && failure->after==target.attributes && failure->commit.items[1].after.count==9,"failed add draws once, records consumption and preserves every old bonus");
    auto other=MakeAttributeRuntimeCommit(*pc,{18,1},fence,layouts,items,tool.id,target.id,actor,pq,[](auto){return 5001;});Check(ItemCommitDigest(*pc,failure->commit)!=ItemCommitDigest(*pc,other->commit),"same after state with a different recorded decision has a different receipt digest");
    probabilistic.attributeServices[0].consumeOnFailure=false;const auto free=BuildItemCatalog(probabilistic).value();const auto fq=PrepareAttributeRuntime(*free,fence,items,tool.id,target.id,actor).value();auto kept=MakeAttributeRuntimeCommit(*free,{18,2},fence,layouts,items,tool.id,target.id,actor,fq,[](auto max){return max;});Check(kept && !kept->succeeded && kept->commit.items.size()==1 && kept->after==target.attributes,"non-consuming failed policy still has one durable outcome receipt");
    auto gap=definition;gap.attributeServices[0].tiers={{1,4000}};const auto gc=BuildItemCatalog(gap).value();auto gq=PrepareAttributeRuntime(*gc,fence,items,tool.id,target.id,actor).value();entropy=0;auto noTier=MakeAttributeRuntimeCommit(*gc,{18,3},fence,layouts,items,tool.id,target.id,actor,gq,[&](auto max){++entropy;return max;});Check(!noTier && noTier.error()==AttributeRuntimeStatus::UnassignedTier && entropy==1,"unassigned tier is not normalized or rerolled and has no debit");
    items[0].placement={ItemStorageKind::Equipment,1,0};Check(!quote(),"equipped target is never serviced");items[0]=target;items[0].boundTo=ItemOwner{ItemOwnerKind::Character,99};Check(!quote(),"foreign bound target refused");items[0]=target;items[0].revision=UINT64_MAX;Check(!quote(),"target revision cannot wrap");items[0]=target;items.push_back(target);Check(!quote(),"duplicate input identity refused");items.pop_back();
    auto restricted=definition;std::ranges::find(restricted.items,tool.key,&ItemBaseRow::key)->minimumLevel=101;const auto rc=BuildItemCatalog(restricted).value();Check(PrepareAttributeRuntime(*rc,fence,items,tool.id,target.id,actor).error()==AttributeRuntimeStatus::Level,"source minimum level enforced before random draw");
    items[0].attributes=af::Full();Check(quote().error()==AttributeRuntimeStatus::NoEffect,"full group cannot add or consume");items[0].attributes.lockedSlot=0;items[1].key=attribute_fixture::Tool(3);Check(quote().error()==AttributeRuntimeStatus::AlreadyLocked,"second protection refused");items[0].attributes.lockedSlot.reset();items[1].key=attribute_fixture::Tool(5);Check(quote().error()==AttributeRuntimeStatus::NotLocked,"unlock without protection consumes nothing");
    items={target,tool};for(unsigned cell=0;cell<90;++cell)if(cell!=0 && cell!=1 && cell!=5 && cell!=10){auto material=f::Leather(200+cell);material.placement->anchor=cell;items.push_back(material);}Check(make(*quote(),[](auto){return 1;}).has_value(),"in-place service works in a full inventory");
    auto equipped=target;equipped.placement={ItemStorageKind::Equipment,1,0};const auto before=BuildEquipmentStats(c,fence.owner,std::span(&equipped,1)).value();auto defense=make(*quote(),[](auto){return 1;})->commit.items[0].after;defense.placement=equipped.placement;const auto after=BuildEquipmentStats(c,fence.owner,std::span(&defense,1)).value();Check(after.flat.magicalDefenseRaw-before.flat.magicalDefenseRaw==100,"new instance bonus contributes exactly once after equip");
    return *golden;
}
}
int main(int argc,char** argv){try{ReusableRareRun();const auto standard=Run();const auto addon=AddonRun();const auto& bytes=argc==3 && std::string_view(argv[1])=="--write-addon-golden" ? addon : standard;if(argc==3 && (std::string_view(argv[1])=="--write-golden" || std::string_view(argv[1])=="--write-addon-golden")){std::ofstream out(argv[2]);for(auto b:bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<std::to_integer<unsigned>(b);out<<'\n';if(!out)throw std::runtime_error("golden write");}std::cout<<checks<<" attribute runtime checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
