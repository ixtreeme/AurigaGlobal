#include "ItemCatalogFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/CompanionItems.hpp>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace db=auriga::item::sql;
namespace {
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
ItemKey Key(unsigned n){return ItemKey::Create(15,n).value();}
ItemCatalogSource Fixture() {
    auto s=f::Catalog();RefinementProfile p{5,"refine.companion",RefinementKind::Companion,30,{}};
    for(std::uint16_t i=0;i<=30;++i)p.levels.push_back({i,{},{}});s.refinements.push_back(p);
    s.companionForms={{1,"companion.pet",34001,"models/pet.model"},{2,"companion.mount",20110,"models/mount.model"}};
    for(unsigned i=1;i<=2;++i){auto b=s.items[3];b.key=Key(i);b.maxStack=1;b.usePolicy=1;b.refinementProfile=5;
        b.contentCode="companion.item_"+std::to_string(i);b.nameKey="item.companion_"+std::to_string(i)+".name";
        b.descriptionKey="item.companion_"+std::to_string(i)+".description";s.items.push_back(b);}
    s.companions={{Key(1),CompanionKind::Pet,1,{},CompanionClock::SinceFirstUse,3000,true},
                  {Key(2),CompanionKind::Mount,2,2,CompanionClock::ActiveTime,5000,false}};
    return s;
}
CompanionRequest Request(const ItemCatalog& c,unsigned local=1) {
    CompanionRequest r{Key(local)};r.state=MakeCompanionState(c,r.key,100).value();
    r.instanceId=10;r.revision=5;r.ownerId=r.actorId=7;r.rulesetRevision=3;r.operationId[0]=1;
    r.count=1;r.characterLevel=50;r.characterClass=r.sex=1;r.action=CompanionAction::Summon;
    r.nowUtcMs=1000;r.admittedByServer=true;r.ownerAlive=true;r.worldAllowsCompanion=r.worldAllowsRiding=true;return r;
}
void Actions() {
    auto s=Fixture();auto c=BuildItemCatalog(s).value();auto r=Request(*c);
    auto p=PlanCompanion(*c,r).value();Check(p.spawnFollower && !p.despawnFollower && !p.changeRider,"summon creates one follower");
    Check(p.after.phase==CompanionPhase::Following && p.after.recallRequested && p.after.lifetime.deadlineUtcMs==4000,"first accepted summon starts explicit UTC lifetime");
    Check(!p.bonusEnabled && p.followerForm.mobTemplateId==34001 && p.followerForm.modelAsset=="models/pet.model","dungeon-only pet has explicit owned presentation");
    r.inDungeon=true;Check(PlanCompanion(*c,r)->bonusEnabled,"dungeon-scoped pet bonus");
    r.state=p.after;r.action=CompanionAction::Maintain;r.nowUtcMs=2000;
    Check(!PlanCompanion(*c,r)->spawnFollower,"maintenance cannot spawn a duplicate actor");
    auto changedSource=s;changedSource.companions[0].form=2;
    auto changedCatalog=BuildItemCatalog(changedSource).value();auto changed=PlanCompanion(*changedCatalog,r).value();
    Check(changed.spawnFollower && changed.despawnFollower && changed.after.activeFormId==2,"new published form replaces one old follower without losing lifetime");
    Check(changed.after.lifetime==r.state.lifetime,"presentation replacement does not refill clock");
    r.action=CompanionAction::Dismiss;r.characterClass=0;r.ownerAlive=false;
    p=PlanCompanion(*c,r).value();Check(p.despawnFollower && !p.after.recallRequested && p.after.phase==CompanionPhase::Dormant,"dismiss cleans active pet regardless of death and policy eligibility");
    r=Request(*c);r.state=PlanCompanion(*c,r)->after;r.action=CompanionAction::Maintain;r.ownerAlive=false;
    p=PlanCompanion(*c,r).value();Check(p.despawnFollower && p.after.recallRequested,"temporary death releases actor but preserves recall intent");
    r.state=p.after;r.ownerAlive=true;r.action=CompanionAction::Summon;r.nowUtcMs=3500;
    p=PlanCompanion(*c,r).value();Check(p.spawnFollower && p.after.lifetime.deadlineUtcMs==4000,"recall cannot refill UTC lifetime");
    r.state=p.after;r.action=CompanionAction::Maintain;r.nowUtcMs=4000;p=PlanCompanion(*c,r).value();
    Check(p.after.lifetime.expired && p.despawnFollower && !p.after.recallRequested && !p.bonusEnabled && p.removeItem,"expiry atomically removes the seal, actor and bonuses at exact deadline");
    r.state=p.after;r.nowUtcMs=1;r.action=CompanionAction::Summon;
    Check(PlanCompanion(*c,r)->after.lifetime.expired && !PlanCompanion(*c,r)->spawnFollower,"UTC rollback cannot revive expired instance");
    r=Request(*c,2);p=PlanCompanion(*c,r).value();Check(p.spawnFollower && !p.bonusEnabled,"unridden mount follows without riding bonuses");
    r.state=p.after;r.action=CompanionAction::Mount;r.elapsedOwnerMs=700;p=PlanCompanion(*c,r).value();
    Check(p.despawnFollower && !p.spawnFollower && p.changeRider && p.bonusEnabled && p.ridingForm->id==2,"mounting hides follower and switches one rider model");
    Check(p.after.lifetime.remainingMs==4300,"active clock debits validated owner time before mounting");
    r.state=p.after;r.action=CompanionAction::Dismount;r.elapsedOwnerMs=300;p=PlanCompanion(*c,r).value();
    Check(p.spawnFollower && p.changeRider && !p.bonusEnabled && p.after.lifetime.remainingMs==4000,"dismount restores exactly one follower without refilling lifetime");
    r.state=p.after;r.action=CompanionAction::Maintain;r.elapsedOwnerMs=4000;p=PlanCompanion(*c,r).value();
    Check(p.after.lifetime.expired && p.despawnFollower,"active budget expiry cleans mount");
    r=Request(*c,2);r.elapsedOwnerMs=4000;Check(PlanCompanion(*c,r)->after.lifetime.remainingMs==5000,"dormant active clock does not debit offline elapsed time");
    r.state=PlanCompanion(*c,r)->after;r.action=CompanionAction::Mount;r.elapsedOwnerMs=0;r.state=PlanCompanion(*c,r)->after;
    r.action=CompanionAction::Maintain;r.worldAllowsRiding=false;p=PlanCompanion(*c,r).value();
    Check(p.changeRider && p.spawnFollower && p.after.phase==CompanionPhase::Following,"riding prohibition safely demotes to follower");
    r.state=p.after;r.worldAllowsCompanion=false;p=PlanCompanion(*c,r).value();
    Check(p.despawnFollower && !p.spawnFollower && p.after.recallRequested,"map prohibition cleans actor without losing intent");
    s.items.back().retired=true;auto retired=BuildItemCatalog(s).value();r=Request(*c,2);r.state=PlanCompanion(*c,r)->after;r.action=CompanionAction::Maintain;
    Check(PlanCompanion(*retired,r)->despawnFollower && !PlanCompanion(*retired,r)->after.recallRequested && !PlanCompanion(*retired,r)->removeItem,"retirement cleans active mount without deleting the owned seal");
    r=Request(*c,2);r.state=PlanCompanion(*c,r)->after;r.action=CompanionAction::Maintain;auto split=r;
    r.elapsedOwnerMs=1234;auto one=PlanCompanion(*c,r)->after;split.elapsedOwnerMs=234;split.state=PlanCompanion(*c,split)->after;split.elapsedOwnerMs=1000;
    Check(PlanCompanion(*c,split)->after==one,"active clock partition invariance");
    Check(c->FindRefinement(5)->FindLevel(30) && c->RefinedBaseStats(Key(2),30).value()==ItemBaseStats{},"one prototype accepts configured +30 bonus-only level");
    r=Request(*c,2);r.refinementLevel=30;Check(bool(PlanCompanion(*c,r)),"configured level beyond +9 is usable without new prototype");
    s.companions[0].clock=CompanionClock::SinceCreation;s.companions[0].durationMs=3000;c=BuildItemCatalog(s).value();
    Check(MakeCompanionState(*c,Key(1),100)->lifetime.deadlineUtcMs==3100,"creation clock starts at captured creation time");
    Check(MakeCompanionState(*c,Key(1),INT64_MAX).error()==CompanionError::TimeOverflow,"creation deadline overflow rejected");
    s.companions[0].clock=CompanionClock::Permanent;s.companions[0].durationMs=0;c=BuildItemCatalog(s).value();r=Request(*c);r.nowUtcMs=INT64_MAX;
    Check(!PlanCompanion(*c,r)->after.lifetime.started,"permanent companion has no inferred deadline");
}
void Rejections() {
    auto source=Fixture();auto c=BuildItemCatalog(source).value();using E=CompanionError;
    auto reject=[&](auto mutate,E error){auto r=Request(*c);mutate(r);Check(PlanCompanion(*c,r).error()==error,"invalid admission/state rejected");};
    reject([](auto& r){r.admittedByServer=false;},E::Forbidden);
    for(int field=0;field<6;++field)reject([&](auto& r){if(field==0)r.instanceId=0;if(field==1)r.revision=0;if(field==2)r.revision=UINT64_MAX;if(field==3)r.ownerId=9;if(field==4)r.rulesetRevision=0;if(field==5)r.operationId={};},E::InvalidOperation);
    reject([](auto& r){r.count=2;},E::InvalidSnapshot);reject([](auto& r){r.lockedByOther=true;},E::Forbidden);
    reject([](auto& r){r.exchanging=true;},E::Forbidden);reject([](auto& r){r.refinementLevel=31;},E::InvalidLevel);
    reject([](auto& r){r.action=CompanionAction{};},E::InvalidAction);reject([](auto& r){r.nowUtcMs=UINT64_MAX;},E::InvalidTime);
    reject([](auto& r){r.elapsedOwnerMs=UINT64_MAX;},E::InvalidTime);reject([](auto& r){r.nowUtcMs=INT64_MAX;},E::TimeOverflow);
    reject([](auto& r){r.characterClass=0;},E::Forbidden);reject([](auto& r){r.sex=0;},E::Forbidden);
    reject([](auto& r){r.polymorphed=true;},E::Forbidden);reject([](auto& r){r.observer=true;},E::Forbidden);
    reject([](auto& r){r.ownerAlive=false;},E::Forbidden);reject([](auto& r){r.worldAllowsCompanion=false;},E::Forbidden);
    reject([](auto& r){r.channelOccupiedByOther=true;},E::ChannelOccupied);
    reject([](auto& r){r.state.phase=CompanionPhase::Riding;},E::InvalidState);
    reject([&](auto& r){r.state=PlanCompanion(*c,r)->after;},E::AlreadyActive);
    reject([&](auto& r){r.state=PlanCompanion(*c,r)->after;r.action=CompanionAction::Mount;},E::WrongKind);
    reject([](auto& r){r.action=CompanionAction::Dismount;},E::NotRiding);
    reject([](auto& r){r.state.lifetime.remainingMs=1;},E::InvalidState);
    reject([](auto& r){r.state.activeFormId=1;},E::InvalidState);
    reject([&](auto& r){r.state=PlanCompanion(*c,r)->after;r.state.activeFormId=99;r.action=CompanionAction::Maintain;},E::InvalidState);
    auto invalid=[&](auto mutate){auto s=Fixture();mutate(s);Check(!BuildItemCatalog(s),"invalid catalog cannot publish");};
    invalid([](auto& s){s.companions[0].ridingForm=1;});invalid([](auto& s){s.companions[1].ridingForm.reset();});
    invalid([](auto& s){s.companions[0].form=99;});invalid([](auto& s){s.companions[1].ridingForm=99;});
    invalid([](auto& s){s.companions[0].clock=CompanionClock::Permanent;});invalid([](auto& s){s.companions[0].durationMs=0;});
    invalid([](auto& s){s.companions[0].durationMs=UINT64_MAX;});invalid([](auto& s){s.companionForms[0].modelAsset="../unsafe";});
    invalid([](auto& s){s.companionForms[0].mobTemplateId=0;});invalid([](auto& s){s.companionForms.push_back(s.companionForms[0]);});
    invalid([](auto& s){s.companions.push_back(s.companions[0]);});invalid([](auto& s){s.companions.pop_back();});
    invalid([](auto& s){s.items.pop_back();});invalid([](auto& s){s.items.back().maxStack=2;});
    invalid([](auto& s){s.items[s.items.size()-2].binding=BindingPolicy::OnEquip;});
    invalid([](auto& s){s.refinements.back().levels[1].gain.physicalAttackRaw=1000;});
    auto s=Fixture();s.companionForms[0].modelAsset="models/other.model";
    Check(BuildItemCatalog(s).value()->ContentHash()!=c->ContentHash(),"form presentation participates in catalog hash");
    auto snapshot=catalog_snapshot_fixture::Snapshot(Fixture());auto decoded=db::DecodeSnapshot(snapshot,{77,{}});
    Check(decoded && (*decoded)->ContentHash()==c->ContentHash(),"SQL snapshot preserves complete typed companion data");
    snapshot[db::Table::Companions][0][4]="0";Check(!db::DecodeSnapshot(snapshot,{77,{}}),"SQL NULL and zero riding form differ");
    Check(!ValidateRefinementRecipe({1,"refine.invalid_companion",Key(1),10,Key(1),11,0,10000,RefinementFailure::Keep,{},{}},*c),"ordinary smith cannot bypass companion state writer");
}
}
int main(int argc,char** argv) {
    try{if(argc==2 && std::string_view(argv[1])=="--hash-fixture"){auto c=BuildItemCatalog(Fixture()).value();for(auto b:c->ContentHash())std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);std::cout<<'\n';return 0;}
        Actions();Rejections();std::cout<<checks<<" Companion checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
