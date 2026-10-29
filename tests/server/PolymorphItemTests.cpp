#include "CatalogSnapshotFixture.hpp"
#include <item/PolymorphItems.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <source_location>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace db=auriga::item::sql;
namespace
{
unsigned checks=0;
void Check(bool ok,const char* message,std::source_location l=std::source_location::current())
{++checks;if(!ok)throw std::runtime_error(std::string(message)+" line "+std::to_string(l.line()));}
ItemKey Key(unsigned local){return ItemKey::Create(13,local).value();}
ItemCatalogSource Fixture()
{
    auto s=f::Catalog();s.polymorphForms={{1,"form.wolf",101,PolymorphBonus::Movement},{2,"form.boar",102,PolymorphBonus::None}};
    PolymorphProfile orb{1,"poly.orb",PolymorphKind::Orb,20,3,10,0,0,0,{},{}};
    PolymorphProfile book{2,"poly.book",PolymorphKind::PracticeBook,0,0,1,30,50,29,{},{}};
    for(std::uint8_t i=0;i<=40;++i)
    {orb.skills.push_back({i,std::uint64_t(i==0?300000:i==40?2100000:600000),std::uint32_t(i==40?30000:20000),1000});book.skills.push_back({i,0,0,1000});}
    for(std::uint8_t i=0;i<4;++i)book.masteries.push_back({i,std::uint64_t(10000+i*5000),std::uint64_t(9000+i*5000)});
    s.polymorphProfiles={orb,book};
    for(auto [key,name]:{std::pair{Key(1),"orb"},std::pair{Key(2),"book"}})
    {auto base=s.items[3];base.key=key;base.contentCode=std::string("polymorph.")+name;base.nameKey=std::string("item.poly_")+name+".name";
     base.descriptionKey=std::string("item.poly_")+name+".description";base.maxStack=key==Key(1)?200:1;base.usePolicy=1;s.items.push_back(base);}
    s.polymorphItems={{Key(1),1,1},{Key(2),2,{}}};return s;
}
PolymorphRequest Request()
{
    PolymorphRequest r{Key(1)};r.instanceId=100;r.revision=5;r.ownerId=r.actorId=7;r.rulesetRevision=2;r.operationId[0]=1;
    r.count=10;r.characterLevel=50;r.mobLevel=10;r.mobTemplate=101;r.mobCatalogRevision=4;r.characterClass=r.sex=1;
    r.skillLevel=40;r.mastery=3;r.action=PolymorphAction::Use;r.admittedByServer=r.formAllowedByServer=true;r.nowUtcMs=1000;return r;
}
void Use()
{
    auto source=Fixture();auto catalog=BuildItemCatalog(source).value();auto r=Request();auto orb=PlanPolymorph(*catalog,r).value();
    Check(orb.success && orb.consumeUnits==1 && orb.effect->expiresAtUtcMs==2101000,"orb consumes one and uses 35 minute level-40 rule");
    Check(orb.effect->bonusMagnitude==30000 && orb.effect->bonusUnit==PolymorphBonusUnit::BasisPoints && orb.effect->powerBp==1000 && orb.effect->bonus==PolymorphBonus::Attack,"typed attack bonus and power are separate");
    Check(orb.catalogHash==catalog->ContentHash() && orb.expected.mobCatalogRevision==4,"both catalog inputs pinned");
    for(auto skill:{0u,1u,39u,40u})
    {r.skillLevel=static_cast<std::uint8_t>(skill);const auto p=PlanPolymorph(*catalog,r).value();Check(p.effect->expiresAtUtcMs==1000+(skill==0?300000:skill==40?2100000:600000),"integer skill duration thresholds");}
    r=Request();r.mobLevel=55;Check(PlanPolymorph(*catalog,r).error()==PolymorphError::MobLevelTooHigh,"orb level boundary is exclusive");
    r.mobLevel=54;Check(PlanPolymorph(*catalog,r).has_value(),"one level below boundary allowed");
    r=Request();r.item=Key(2);r.count=1;r.state={1,3,2};r.skillLevel=20;r.mastery=1;r.characterLevel=20;r.recordedRoll=6200;
    auto book=PlanPolymorph(*catalog,r).value();Check(book.success && book.chanceBp==6200 && book.consumeUnits==0 && book.after.practiceRemaining==1,"book chance and one practice success");
    Check(book.effect->expiresAtUtcMs==16000 && book.effect->bonusExpiresAtUtcMs==15000 && book.effect->bonusMagnitude==23 && book.effect->bonusUnit==PolymorphBonusUnit::MovementPoints,"book movement is additive points; bonus ends earlier");
    r.recordedRoll=6201;auto fail=PlanPolymorph(*catalog,r).value();Check(!fail.success && !fail.effect && !fail.consumeUnits && fail.after==r.state,"failed use preserves book and progress");
    r.state.practiceRemaining=0;r.recordedRoll=1;Check(PlanPolymorph(*catalog,r)->after.practiceRemaining==0,"zero practice cannot underflow");
    r.state.boundForm=2;r.mobTemplate=102;Check(PlanPolymorph(*catalog,r)->effect->bonus==PolymorphBonus::None,"form without book bonus has no phantom bonus");
    r.characterLevel=255;r.recordedRoll=0;Check(PlanPolymorph(*catalog,r)->chanceBp==10000,"book chance clamps at 100 percent without draw");
    r.characterLevel=1;r.mobLevel=255;r.recordedRoll=0;Check(!PlanPolymorph(*catalog,r)->success,"nonpositive chance cannot succeed");
    r=Request();r.nowUtcMs=std::uint64_t{INT64_MAX}-100;Check(PlanPolymorph(*catalog,r).error()==PolymorphError::TimeOverflow,"deadline cannot overflow");
    catalog.reset();source={};Check(orb.expected.item==Key(1) && orb.effect->mobTemplate==101,"plan survives catalog destruction");
}
void Admission()
{
    const auto c=BuildItemCatalog(Fixture()).value();
    for(unsigned i=0;i<19;++i)
    {
        auto r=Request();auto expected=PolymorphError::Forbidden;
        switch(i){
        case 0:r.admittedByServer=false;break;case 1:r.operationId={};expected=PolymorphError::InvalidOperation;break;
        case 2:r.ownerId=8;expected=PolymorphError::InvalidSnapshot;break;case 3:r.revision=UINT64_MAX;expected=PolymorphError::InvalidSnapshot;break;
        case 4:r.locked=true;expected=PolymorphError::InvalidSnapshot;break;case 5:r.exchanging=true;expected=PolymorphError::InvalidSnapshot;break;
        case 6:r.equipped=true;expected=PolymorphError::InvalidSnapshot;break;case 7:r.count=201;expected=PolymorphError::InvalidStack;break;
        case 8:r.state.boundForm=1;expected=PolymorphError::InvalidState;break;case 9:r.skillLevel=41;expected=PolymorphError::InvalidLevel;break;
        case 10:r.characterClass=0;expected=PolymorphError::InvalidLevel;break;case 11:r.recordedRoll=1;expected=PolymorphError::InvalidDecision;break;
        case 12:r.riding=true;break;case 13:r.alreadyPolymorphed=true;break;case 14:r.duelBlocksPolymorph=true;break;
        case 15:r.mobTemplate=102;expected=PolymorphError::MobMismatch;break;case 16:r.mobCatalogRevision=0;expected=PolymorphError::MobMismatch;break;
        case 17:r.formAllowedByServer=false;expected=PolymorphError::MobMismatch;break;
        case 18:r.item=Key(2);r.count=1;expected=PolymorphError::MissingForm;break;}
        Check(PlanPolymorph(*c,r).error()==expected,"invalid snapshot cannot transform or consume");
    }
    auto source=Fixture();source.polymorphItems[0].fixedForm.reset();auto bound=BuildItemCatalog(source).value();auto r=Request();
    Check(PlanPolymorph(*bound,r).error()==PolymorphError::MissingForm,"blank orb does not produce a consume plan");
    r.state.boundForm=999;Check(PlanPolymorph(*bound,r).error()==PolymorphError::MissingForm,"unknown bound form does not consume");
    r.state.boundForm=1;Check(PlanPolymorph(*bound,r)->success,"bound form resolves new form registry only");
    r=Request();r.item=Key(2);r.count=1;r.state={1,10,0};r.action=PolymorphAction::UpgradeBook;r.riding=true;
    auto upgrade=PlanPolymorph(*c,r).value();Check(upgrade.success && upgrade.after.bookGrade==11 && upgrade.after.practiceRemaining==500 && !upgrade.consumeUnits,"book upgrades beyond +9 with previous-grade training formula");
    r.state.practiceRemaining=1;Check(PlanPolymorph(*c,r).error()==PolymorphError::NotReady,"cannot bypass unfinished practice");
    r.state={1,30,0};Check(PlanPolymorph(*c,r).error()==PolymorphError::AtMaximum,"configured book maximum");
    r.state={1,0,0};Check(PlanPolymorph(*c,r)->after.practiceRemaining==0,"grade-zero legacy reset formula remains explicit");
}
void Publication()
{
    const auto source=Fixture();auto bad=[&](auto change){auto s=source;change(s);Check(!BuildItemCatalog(s),"invalid profile/form cannot publish");};
    bad([](auto& s){s.polymorphProfiles[0].skills.pop_back();});bad([](auto& s){s.polymorphProfiles[0].skills[1].durationMs=0;});
    bad([](auto& s){s.polymorphProfiles[0].skills[1].powerBp=10001;});bad([](auto& s){s.polymorphProfiles[0].graceLossDenominator=0;});
    bad([](auto& s){s.polymorphProfiles[1].masteries[1].bonusDurationMs=999999;});bad([](auto& s){s.polymorphProfiles[1].maxBookGrade=256;});
    bad([](auto& s){s.polymorphProfiles[1].practicePerGrade=UINT32_MAX;});bad([](auto& s){s.polymorphForms[0].mobTemplate=7;});
    bad([](auto& s){s.polymorphForms.push_back(s.polymorphForms[0]);});bad([](auto& s){s.polymorphItems[0].fixedForm=999;});
    bad([](auto& s){s.items.back().maxStack=2;});bad([](auto& s){s.polymorphProfiles[0].masteries.push_back({0,1000,0});});
    bad([](auto& s){s.polymorphItems[0].fixedForm.reset();s.lootProfiles[0].entries[0].item=Key(1);});
    bad([](auto& s){s.lootProfiles[0].entries[0].item=Key(2);s.lootProfiles[0].entries[0].amount=1;});
    auto c=BuildItemCatalog(source).value();auto snapshot=catalog_snapshot_fixture::Snapshot(source);
    Check(db::DecodeSnapshot(snapshot,{1,c->ContentHash()}).has_value(),"SQL roundtrip includes all explicit skill/mastery/form fields");
    auto shuffled=source;std::ranges::reverse(shuffled.polymorphProfiles[0].skills);Check(BuildItemCatalog(shuffled).value()->ContentHash()==c->ContentHash(),"canonical rule ordering");
    shuffled=source;shuffled.polymorphProfiles[0].skills[40].powerBp=0;Check(BuildItemCatalog(shuffled).value()->ContentHash()!=c->ContentHash(),"level-40 power cannot silently default outside digest");
    Check(!ValidateRefinementRecipe({1,"poly.bypass",Key(2),0,Key(2),0,0,10000,RefinementFailure::Keep,{},{}},*c),"ordinary recipe cannot erase bound form or book progress");
}
}
int main(int argc,char** argv)
{
    try{
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture"){const auto c=BuildItemCatalog(Fixture()).value();for(auto b:c->ContentHash())std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);std::cout<<'\n';return 0;}
        Use();Admission();Publication();std::cout<<checks<<" polymorph checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
