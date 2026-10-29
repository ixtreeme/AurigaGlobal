#include "ItemCatalogFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/GrowthPets.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace db=auriga::item::sql;
namespace {
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
ItemKey Key(){return ItemKey::Create(15,1).value();}
ItemCatalogSource Fixture() {
    auto s=f::Catalog();
    s.companionForms={{1,"growth.young",34041,"models/growth_young.model"},{2,"growth.adult",34042,"models/growth_adult.model"}};
    GrowthPetProfile p{1,"growth.standard",120,GrowthExpPolicy::MobOnly,{},{}};
    for(std::uint16_t level=1;level<=120;++level)p.levels.push_back({level,level==120 ? 0u : 1000u*level,level==120 ? 0u : 100u*level});
    p.stages={{0,40,1},{1,60,1},{2,80,1},{3,120,2}};s.growthPetProfiles.push_back(p);
    auto b=s.items[3];b.key=Key();b.maxStack=1;b.usePolicy=1;b.contentCode="companion.item_1";
    b.nameKey="item.companion_1.name";b.descriptionKey="item.companion_1.description";s.items.push_back(b);
    s.companions={{Key(),CompanionKind::GrowthPet,1,{},CompanionClock::ActiveTime,259200000,false,1}};
    return s;
}
GrowthPetRequest Request(const ItemCatalog& c) {
    GrowthPetRequest r{Key()};r.progress=MakeGrowthPetProgress(c,Key()).value();
    r.instanceId=17;r.revision=3;r.progressRevision=8;r.ownerId=r.actorId=77;r.rulesetRevision=5;r.operationId[0]=1;
    r.count=1;r.action=GrowthPetAction::AwardMobExperience;r.experience=100;r.admittedByServer=r.activePet=true;return r;
}
void Progression() {
    auto c=BuildItemCatalog(Fixture()).value();auto r=Request(*c);
    const auto unchanged=r.progress;auto p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==1 && p.after.mobExperience==100 && !p.levelled,"partial mob EXP retained");
    Check(r.progress==unchanged && p.expected.progressRevision==8 && p.expected.revision==3,"owned proposal does not mutate source and pins both revisions");
    Check(p.catalogHash==c->ContentHash() && p.catalogRevision==c->Revision(),"catalog pin");
    r.experience=INT64_MAX;p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==2 && p.after.mobExperience==0 && p.after.itemExperience==0,"mob-only level does not need item EXP");
    Check(p.acceptedExperience==1000 && p.discardedExperience==std::uint64_t{INT64_MAX}-1000,"bounded award explicitly reports excess");
    Check(p.beforeForm==1 && p.afterForm==1 && !p.evolved,"level is separate from evolution");
    r.progress={40,0,39000,0};r.experience=2000;p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==40 && p.after.mobExperience==40000 && p.atEvolutionGate && !p.levelled,"full EXP cannot bypass evolution gate");
    Check(p.acceptedExperience==1000 && p.discardedExperience==1000,"gate overflow visible");
    r.progress=p.after;r.action=GrowthPetAction::Evolve;r.experience=0;p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==41 && p.after.stage==1 && p.after.mobExperience==0 && p.evolved && p.levelled,"explicit evolution increments level and clears EXP");
    r.progress={80,2,0,0};p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==81 && p.after.stage==3 && p.afterForm==2 && p.beforeForm==1,"final evolution changes explicit form");
    Check(r.progress.mobExperience==0,"evolution depends on admitted stage ceiling, not invented full-EXP requirement");
    r=Request(*c);r.action=GrowthPetAction::AwardItemExperience;r.experience=50;
    p=PlanGrowthPetProgress(*c,r).value();Check(p.after.itemExperience==50 && p.after.mobExperience==0 && !p.levelled,"separate item EXP retained");
    auto s=Fixture();s.growthPetProfiles[0].experiencePolicy=GrowthExpPolicy::MobAndItem;c=BuildItemCatalog(s).value();
    r=Request(*c);r.experience=1000;p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==1 && p.after.mobExperience==1000 && !p.levelled,"dual policy requires both pools");
    r.progress=p.after;r.action=GrowthPetAction::AwardItemExperience;r.experience=120;p=PlanGrowthPetProgress(*c,r).value();
    Check(p.after.level==2 && p.levelled && p.after.itemExperience==0 && p.discardedExperience==20,"second pool completes one level");
    r.progress={120,3,0,0};r.experience=INT64_MAX;p=PlanGrowthPetProgress(*c,r).value();
    Check(p.atMaximum && !p.levelled && p.acceptedExperience==0 && p.discardedExperience==INT64_MAX,"terminal award cannot underflow or create another level");
    // Traverse all levels and all explicit gates; +9 has no special meaning.
    c=BuildItemCatalog(Fixture()).value();r=Request(*c);
    for(unsigned level=1;level<120;++level) {
        const auto& stage=c->FindGrowthPetProfile(1)->stages[r.progress.stage];
        r.action=level==stage.maximumLevel ? GrowthPetAction::Evolve : GrowthPetAction::AwardMobExperience;
        r.experience=r.action==GrowthPetAction::Evolve ? 0 : 1000u*level;
        p=PlanGrowthPetProgress(*c,r).value();Check(p.after.level==level+1,"all levels and stages reachable");r.progress=p.after;
    }
    Check(r.progress.level==120 && r.progress.stage==3,"configured terminal state");
    s=Fixture();s.growthPetProfiles[0].maximumLevel=255;s.growthPetProfiles[0].stages={{0,255,1}};
    auto& levels=s.growthPetProfiles[0].levels;levels.clear();for(std::uint16_t n=1;n<=255;++n)levels.push_back({n,n==255 ? 0u : 1u,n==255 ? 0u : 1u});
    c=BuildItemCatalog(s).value();r=Request(*c);r.progress={254,0,0,0};r.experience=1;
    Check(PlanGrowthPetProgress(*c,r)->after.level==255,"maximum255 uses the configured profile");
}
void Rejections() {
    const auto c=BuildItemCatalog(Fixture()).value();
    const auto reject=[&](auto mutate,GrowthPetError error){auto r=Request(*c);mutate(r);auto p=PlanGrowthPetProgress(*c,r);Check(!p && p.error()==error,"invalid request rejected");};
    reject([](auto& r){r.admittedByServer=false;},GrowthPetError::Forbidden);
    reject([](auto& r){r.activePet=false;},GrowthPetError::Forbidden);
    reject([](auto& r){r.lockedByOther=true;},GrowthPetError::Forbidden);
    reject([](auto& r){r.exchanging=true;},GrowthPetError::Forbidden);
    reject([](auto& r){r.ownerId=78;},GrowthPetError::InvalidOperation);
    reject([](auto& r){r.progressRevision=UINT64_MAX;},GrowthPetError::InvalidOperation);
    reject([](auto& r){r.revision=0;},GrowthPetError::InvalidOperation);
    reject([](auto& r){r.operationId={};},GrowthPetError::InvalidOperation);
    reject([](auto& r){r.count=2;},GrowthPetError::InvalidSnapshot);
    reject([](auto& r){r.refinementLevel=1;},GrowthPetError::InvalidSnapshot);
    reject([](auto& r){r.key=f::Sword();},GrowthPetError::NotGrowthPet);
    reject([](auto& r){r.experience=0;},GrowthPetError::InvalidExperience);
    reject([](auto& r){r.experience=UINT64_MAX;},GrowthPetError::InvalidExperience);
    reject([](auto& r){r.action=static_cast<GrowthPetAction>(0);},GrowthPetError::InvalidAction);
    reject([](auto& r){r.action=GrowthPetAction::Evolve;r.experience=0;},GrowthPetError::NotAtEvolutionGate);
    reject([](auto& r){r.progress={41,0,0,0};},GrowthPetError::InvalidProgress);
    reject([](auto& r){r.progress={1,1,0,0};},GrowthPetError::InvalidProgress);
    reject([](auto& r){r.progress.mobExperience=1001;},GrowthPetError::InvalidProgress);
    reject([](auto& r){r.progress={120,3,1,0};},GrowthPetError::InvalidProgress);
    reject([](auto& r){r.progress={120,3,0,0};r.action=GrowthPetAction::Evolve;r.experience=0;},GrowthPetError::NotAtEvolutionGate);
    Check(!MakeCompanionState(*c,Key(),1000),"growth seal cannot enter fixed factory");
    CompanionRequest fixed{Key()};fixed.admittedByServer=true;fixed.instanceId=1;fixed.revision=1;fixed.ownerId=fixed.actorId=1;fixed.rulesetRevision=1;fixed.operationId[0]=1;fixed.count=1;
    Check(PlanCompanion(*c,fixed).error()==CompanionError::NotCompanion,"fixed summon cannot bypass growth domain");
    const auto bad=[&](auto mutate){auto s=Fixture();mutate(s);Check(!BuildItemCatalog(s),"invalid content rejected before publication");};
    bad([](auto& s){s.growthPetProfiles[0].levels.erase(s.growthPetProfiles[0].levels.begin());});
    bad([](auto& s){s.growthPetProfiles[0].levels[1].level=1;});
    bad([](auto& s){s.growthPetProfiles[0].levels.back().mobExperience=1;});
    bad([](auto& s){s.growthPetProfiles[0].levels[0].mobExperience=UINT64_MAX;});
    bad([](auto& s){s.growthPetProfiles[0].stages[1].stage=3;});
    bad([](auto& s){s.growthPetProfiles[0].stages[1].maximumLevel=40;});
    bad([](auto& s){s.growthPetProfiles[0].stages.back().maximumLevel=119;});
    bad([](auto& s){s.growthPetProfiles[0].stages[0].form=99;});
    bad([](auto& s){s.companions[0].form=2;});
    bad([](auto& s){s.companions[0].growthProfile.reset();});
    bad([](auto& s){s.companions[0].ridingForm=2;});
    bad([](auto& s){s.items.back().refinementProfile=2;});
    bad([](auto& s){s.companions[0].kind=CompanionKind::Pet;});
    bad([](auto& s){s.companions[0].clock=CompanionClock::SinceFirstUse;});
}
void SnapshotAndHash() {
    auto s=Fixture();const auto c=BuildItemCatalog(s).value();
    auto snapshot=catalog_snapshot_fixture::Snapshot(s);
    auto decoded=db::DecodeSnapshot(snapshot,{11,c->ContentHash()});
    Check(decoded && (*decoded)->FindGrowthPetProfile(1)->levels.size()==120,"native SQL snapshot retains all levels");
    std::ranges::reverse(s.growthPetProfiles[0].levels);std::ranges::reverse(s.growthPetProfiles[0].stages);
    Check(BuildItemCatalog(s).value()->ContentHash()==c->ContentHash(),"profile input order canonicalizes");
    for(unsigned i=0;i<4;++i){auto changed=Fixture();auto& p=changed.growthPetProfiles[0];
        if(i==0)p.levels[0].mobExperience++;if(i==1)p.levels[0].itemExperience++;
        if(i==2)p.stages[1].maximumLevel++;if(i==3)p.experiencePolicy=GrowthExpPolicy::MobAndItem;
        Check(BuildItemCatalog(changed).value()->ContentHash()!=c->ContentHash(),"all progression rules are release-hashed");}
    snapshot[db::Table::GrowthPetLevels][0][0]="999";Check(!db::DecodeSnapshot(snapshot,{1,{}}),"orphan level rejected");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture") {
            for(auto b:BuildItemCatalog(Fixture()).value()->ContentHash())std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cout<<'\n';return 0;
        }
        Progression();Rejections();SnapshotAndHash();std::cout<<checks<<" Growth pet checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
