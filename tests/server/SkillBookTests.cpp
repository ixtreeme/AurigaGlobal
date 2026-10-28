#include "CatalogSnapshotFixture.hpp"
#include <item/SkillBooks.hpp>
#include <algorithm>
#include <functional>
#include <iostream>
#include <random>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace sf=catalog_snapshot_fixture;
namespace
{
std::size_t checks=0;
void Check(bool ok,const char* label,std::source_location at=std::source_location::current())
{
    ++checks;if(!ok)throw std::runtime_error(std::string(label)+" line "+std::to_string(at.line()));
}
ItemKey Book(){return ItemKey::Create(ItemCategory::SkillBook,1).value();}
ItemCatalogSource Source()
{
    auto s=f::Catalog();auto base=*std::ranges::find(s.items,f::InstantPotion(),&ItemBaseRow::key);
    base.key=Book();base.contentCode="skill_book.master";base.maxStack=20;
    s.items.push_back(base);s.skillBooks.push_back({Book(),1});
    s.skillBookProfiles.push_back({1,"book.master",SkillBookOperation::Learn,SkillBookTargetMode::Fixed,1,2,true,
        SkillBookBonus::ApplyAndConsume,{{1,SkillPointPool::None,0}},
        {{20,21,30,2,7000,10000,20000,1000,2000,true},{21,22,30,3,7000,10000,20000,1000,2000,true}}});
    return s;
}
SkillBookAdmission Actor()
{
    SkillBookAdmission a;a.skill=BookSkillState{1,20,0};a.availableUnits=1;a.experience=100000;
    a.characterLevel=50;a.empire=1;a.alive=true;a.skillSystemEnabled=true;a.targetAllowedByRules=true;return a;
}
void CatalogValidation()
{
    const auto source=Source();const auto c=BuildItemCatalog(source).value();
    Check(c->FindSkillBook(1)->targets.front().skillId==1,"typed book lookup");
    Check(!c->FindSkillBook(99),"unknown book profile absent");
    auto read=auriga::item::sql::DecodeSnapshot(sf::Snapshot(source),{1,c->ContentHash()});
    Check(read && (*read)->ContentHash()==c->ContentHash(),"book snapshot round trip");
    for(auto change:std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){s.skillBookProfiles[0].unitsPerUse=0;},[](auto& s){s.skillBookProfiles[0].operation={};},
        [](auto& s){s.skillBookProfiles[0].targetMode={};},[](auto& s){s.skillBookProfiles[0].allowedMasteries=0;},
        [](auto& s){s.skillBookProfiles[0].allowedMasteries=8;},[](auto& s){s.skillBookProfiles[0].steps.clear();},
        [](auto& s){s.skillBookProfiles[0].targets.clear();},[](auto& s){s.skillBookProfiles[0].targets[0].skillId=0;},
        [](auto& s){s.skillBookProfiles[0].targets[0].skillId=255;},[](auto& s){s.skillBookProfiles[0].targets[0].refundPool=SkillPointPool::Main;},
        [](auto& s){s.skillBookProfiles[0].targets[0].forbiddenEmpire=4;},[](auto& s){s.skillBookProfiles[0].steps[0].currentLevel=40;},
        [](auto& s){s.skillBookProfiles[0].steps[0].nextLevel=20;},[](auto& s){s.skillBookProfiles[0].steps[0].nextLevel=41;},
        [](auto& s){s.skillBookProfiles[0].steps[0].requiredSuccessfulReads=0;},[](auto& s){s.skillBookProfiles[0].steps[0].successChanceBp=10001;},
        [](auto& s){s.skillBookProfiles[0].steps[0].boostedChanceBp=6999;},[](auto& s){s.skillBookProfiles[0].steps[0].experienceCost=UINT64_MAX;},
        [](auto& s){s.skillBookProfiles[0].steps[0].delayMinimumMs=0;},[](auto& s){s.skillBookProfiles[0].steps[0].delayMaximumMs=999;},
        [](auto& s){s.skillBookProfiles[0].steps[0].delayMaximumMs=UINT64_MAX;},
        [](auto& s){s.skillBookProfiles[0].steps.push_back(s.skillBookProfiles[0].steps.front());},
        [](auto& s){s.skillBooks[0].profile=99;},[](auto& s){s.skillBooks.push_back(s.skillBooks[0]);},
        [](auto& s){s.skillBooks[0].key=f::InstantPotion();},
        [](auto& s){s.skillBookProfiles[0].unitsPerUse=21;}})
    {auto s=source;change(s);Check(!BuildItemCatalog(std::move(s)),"invalid book catalog fails closed");}
    for(auto change:std::vector<std::function<void(ItemCatalogSource&)>>{
        [](auto& s){s.skillBookProfiles[0].unitsPerUse=2;},[](auto& s){s.skillBookProfiles[0].allowDelayBypass=false;},
        [](auto& s){s.skillBookProfiles[0].targets[0].forbiddenEmpire=1;},[](auto& s){s.skillBookProfiles[0].targets[0].skillId=2;},
        [](auto& s){s.skillBookProfiles[0].targetMode=SkillBookTargetMode::InstanceBound;},
        [](auto& s){++s.skillBookProfiles[0].steps[0].nextLevel;},[](auto& s){++s.skillBookProfiles[0].steps[0].minimumCharacterLevel;},
        [](auto& s){++s.skillBookProfiles[0].steps[0].requiredSuccessfulReads;},[](auto& s){++s.skillBookProfiles[0].steps[0].successChanceBp;},
        [](auto& s){--s.skillBookProfiles[0].steps[0].boostedChanceBp;},[](auto& s){++s.skillBookProfiles[0].steps[0].experienceCost;},
        [](auto& s){++s.skillBookProfiles[0].steps[0].delayMinimumMs;},[](auto& s){++s.skillBookProfiles[0].steps[0].delayMaximumMs;},
        [](auto& s){s.skillBookProfiles[0].steps[0].delayOnFailure=false;}})
    {auto s=source;change(s);const auto changed=BuildItemCatalog(std::move(s));Check(changed && (*changed)->ContentHash()!=c->ContentHash(),"all book rules affect digest");}
    auto sorted=source;std::ranges::reverse(sorted.skillBookProfiles[0].steps);
    Check(BuildItemCatalog(sorted).value()->ContentHash()==c->ContentHash(),"step source order not semantic");
    using T=auriga::item::sql::Table;
    for(auto change:std::vector<std::function<void(auriga::item::sql::Snapshot&)>>{
        [](auto& s){s[T::BookTargets][0][1]={};},[](auto& s){s[T::BookSteps][0][7]="-1";},
        [](auto& s){s[T::BookSteps][0][10]="2";},[](auto& s){s[T::BookTargets][0][0]="99";}})
    {auto rows=sf::Snapshot(source);change(rows);Check(!auriga::item::sql::DecodeSnapshot(std::move(rows),{1,{}}),"strict typed row decoding");}
}
void LearningAndDecisions()
{
    const auto c=BuildItemCatalog(Source()).value();auto a=Actor();
    const auto request=PrepareSkillBookUse(*c,Book(),a).value();
    Check(request.SuccessChanceBp()==7000 && request.NeedsSuccessRoll(),"admission exposes exact bounded draw");
    const auto progress=ResolveSkillBookUse(request,{7000,1500}).value();
    Check(progress.item==Book() && progress.profileId==1 && progress.catalogRevision==c->Revision(),"plan retains admitted prototype and catalog identity");
    Check(progress.outcome==SkillBookOutcome::Progress && progress.nextReadCount==1 && progress.nextLevel==20,"first successful read advances count only");
    Check(progress.unitsToConsume==1 && progress.experienceToConsume==20000 && progress.nextReadInMs==1500,"accepted attempt reserves item XP and skill cooldown");
    const auto fail=ResolveSkillBookUse(request,{7001,1500}).value();
    Check(fail.outcome==SkillBookOutcome::Failed && !fail.nextReadCount && fail.nextLevel==20 && fail.unitsToConsume==1 && fail.nextReadInMs==1500,"failed roll consumes attempt without progress");
    a.skill->successfulReads=1;
    const auto level=ResolveSkillBookUse(PrepareSkillBookUse(*c,Book(),a).value(),{1,1000}).value();
    Check(level.outcome==SkillBookOutcome::Learned && level.nextLevel==21 && !level.nextReadCount,"threshold increments once and resets, no off-by-one");
    a.chanceBonusAvailable=true;a.cooldownRemainingMs=99;a.delayBypassAvailable=true;
    const auto boosted=PrepareSkillBookUse(*c,Book(),a).value();
    Check(boosted.SuccessChanceBp()==10000 && !boosted.NeedsSuccessRoll(),"boost is explicit deterministic chance");
    const auto result=ResolveSkillBookUse(boosted,{0,2000}).value();
    Check(result.consumeChanceBonus && result.consumeDelayBypass && result.nextLevel==21,"tokens reserved with valid final plan");
    Check(!ResolveSkillBookUse(boosted,{1,2000}),"no hidden random draw on guaranteed success");
    for(auto decisions:std::vector<SkillBookDecisions>{{0,1000},{10001,1000},{1,999},{1,2001}})
        Check(!ResolveSkillBookUse(request,decisions),"invalid decisions cannot produce partial result");
    auto source=Source();source.skillBookProfiles[0].steps[0].successChanceBp=0;
    auto zero=BuildItemCatalog(source).value();a=Actor();
    Check(ResolveSkillBookUse(PrepareSkillBookUse(*zero,Book(),a).value(),{0,1000}).value().outcome==SkillBookOutcome::Failed,"zero means failure, no sentinel algorithm");
    // An owning prepared request remains unchanged after the source snapshot is released.
    zero.reset();source.skillBookProfiles.clear();
    Check(ResolveSkillBookUse(request,{7000,1500}).value().nextReadCount==1,"prepared request owns its rules");
    std::mt19937 random(20261028);
    for(int i=0;i<1000;++i)
    {
        a=Actor();a.skill->successfulReads=random()%2;
        const std::uint16_t roll=1+random()%10000;
        const auto p=ResolveSkillBookUse(PrepareSkillBookUse(*c,Book(),a).value(),{roll,1000}).value();
        const bool hit=roll<=7000,up=hit && a.skill->successfulReads==1;
        const auto count=up?0:a.skill->successfulReads+(hit?1:0);
        Check(p.nextLevel==(up?21:20) && p.nextReadCount==count && p.unitsToConsume==1 && p.experienceToConsume==20000,
              "independent count, level and resource conservation");
    }
}
void AdmissionRejections()
{
    const auto c=BuildItemCatalog(Source()).value();
    for(auto [change,error]:std::vector<std::pair<std::function<void(SkillBookAdmission&)>,SkillBookError>>{
        {[](auto& a){a.skillSystemEnabled=false;},SkillBookError::Disabled},
        {[](auto& a){a.targetAllowedByRules=false;},SkillBookError::RuleDenied},
        {[](auto& a){a.alive=false;},SkillBookError::DeadCharacter},
        {[](auto& a){a.polymorphed=true;},SkillBookError::Polymorphed},
        {[](auto& a){a.characterLevel=29;},SkillBookError::LevelDenied},
        {[](auto& a){a.classIndex=4;},SkillBookError::InvalidState},
        {[](auto& a){a.sexIndex=2;},SkillBookError::InvalidState},
        {[](auto& a){a.empire=0;},SkillBookError::InvalidState},
        {[](auto& a){a.skill->skillId=2;},SkillBookError::TargetDenied},
        {[](auto& a){a.boundSkillId=1;},SkillBookError::InvalidState},
        {[](auto& a){a.skill->level=30;},SkillBookError::MasteryDenied},
        {[](auto& a){a.skill->level=40;},SkillBookError::MasteryDenied},
        {[](auto& a){a.skill->level=22;},SkillBookError::LevelNotCovered},
        {[](auto& a){a.skill->successfulReads=2;},SkillBookError::InvalidState},
        {[](auto& a){a.availableUnits=0;},SkillBookError::InsufficientUnits},
        {[](auto& a){a.availableUnits=21;},SkillBookError::InvalidState},
        {[](auto& a){a.experience=19999;},SkillBookError::InsufficientExperience},
        {[](auto& a){a.experience=UINT64_MAX;},SkillBookError::InvalidState},
        {[](auto& a){a.cooldownRemainingMs=1;},SkillBookError::OnCooldown}})
    {auto a=Actor();change(a);auto r=PrepareSkillBookUse(*c,Book(),a);Check(!r && r.error()==error,"failed admission has no consumptive plan");}
    auto source=Source();source.skillBookProfiles[0].targetMode=SkillBookTargetMode::InstanceBound;
    source.skillBookProfiles[0].targets={{1,SkillPointPool::None,1},{2,SkillPointPool::None,2}};
    const auto bound=BuildItemCatalog(source).value();auto a=Actor();a.boundSkillId=2;a.skill->skillId=2;
    Check(bool(PrepareSkillBookUse(*bound,Book(),a)),"bound skill chosen from server-owned payload allowlist");
    a.empire=2;Check(PrepareSkillBookUse(*bound,Book(),a).error()==SkillBookError::NativeLanguage,"native language denied");
    a.boundSkillId=3;a.skill->skillId=3;
    Check(PrepareSkillBookUse(*bound,Book(),a).error()==SkillBookError::TargetDenied,"unlisted binding denied");
    a.boundSkillId.reset();Check(PrepareSkillBookUse(*bound,Book(),a).error()==SkillBookError::InvalidState,"unbound generic book denied");
}
void ForgetAndHorse()
{
    auto source=Source();auto& p=source.skillBookProfiles[0];
    p.operation=SkillBookOperation::Forget;p.targetMode=SkillBookTargetMode::InstanceBound;p.allowedMasteries=1;
    p.allowDelayBypass=false;p.bonus=SkillBookBonus::Ignore;p.targets={{1,SkillPointPool::Main,0}};
    p.steps={{5,4,0,1,10000,10000,0,0,0,false}};
    auto c=BuildItemCatalog(source).value();auto a=Actor();a.boundSkillId=1;a.skill=BookSkillState{1,5,99};
    a.refundPointBalance=7;a.cooldownRemainingMs=100;
    const auto forgotten=ResolveSkillBookUse(PrepareSkillBookUse(*c,Book(),a).value(),{0,0}).value();
    Check(forgotten.outcome==SkillBookOutcome::Forgotten && forgotten.nextLevel==4 && !forgotten.nextReadCount && forgotten.pointsGranted==1 && forgotten.pointPool==SkillPointPool::Main,"forget refunds exactly one declared pool and cancels old progress");
    Check(!forgotten.nextReadInMs && !forgotten.consumeDelayBypass,"forget not blocked by learning cooldown");
    a.refundPointBalance=UINT32_MAX;Check(PrepareSkillBookUse(*c,Book(),a).error()==SkillBookError::PointLimit,"refund overflow denied");
    for(auto change:std::vector<std::function<void(SkillBookProfile&)>>{
        [](auto& p){p.targets[0].refundPool=SkillPointPool::None;},[](auto& p){p.steps[0].nextLevel=3;},
        [](auto& p){p.steps[0].successChanceBp=9999;},[](auto& p){p.steps[0].experienceCost=1;}})
    {auto s=source;change(s.skillBookProfiles[0]);Check(!BuildItemCatalog(std::move(s)),"forget semantics cannot be malformed");}
    p.operation=SkillBookOperation::HorsePoint;p.targetMode=SkillBookTargetMode::None;p.targets.clear();p.allowedMasteries=0;
    p.steps={{0,1,50,1,5000,5000,0,1000,1000,false},{19,20,50,1,5000,5000,0,1000,1000,false}};
    c=BuildItemCatalog(source).value();a=Actor();a.skill.reset();a.horse=BookHorseState{0,59,59};
    auto request=PrepareSkillBookUse(*c,Book(),a).value();
    auto pass=ResolveSkillBookUse(request,{5000,1000}).value();
    Check(pass.outcome==SkillBookOutcome::HorsePoint && !pass.skillId && pass.pointPool==SkillPointPool::Horse && pass.pointsGranted==1 && pass.nextReadInMs==1000,"horse point and timer share one proposal");
    auto fail=ResolveSkillBookUse(request,{5001,1000}).value();
    Check(!fail.pointsGranted && !fail.nextReadInMs && fail.unitsToConsume==1,"failed horse attempt consumes book without cooldown");
    a.horse->unspentPoints=19;
    Check(ResolveSkillBookUse(PrepareSkillBookUse(*c,Book(),a).value(),{1,1000}).value().nextLevel==20,"last allowed horse point");
    for(auto state:std::vector<BookHorseState>{{20,0,0},{0,60,0},{0,0,60}})
    {a.horse=state;Check(PrepareSkillBookUse(*c,Book(),a).error()==SkillBookError::PointLimit,"all horse allocation limits enforced");}
    a.horse=BookHorseState{0,0,0};a.characterLevel=49;
    Check(PrepareSkillBookUse(*c,Book(),a).error()==SkillBookError::LevelDenied,"horse level minimum enforced");
}
}
int main()
{
    try{CatalogValidation();LearningAndDecisions();AdmissionRejections();ForgetAndHorse();std::cout<<checks<<" skill book checks passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
