#include "PetLifecycleFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace f=pet_lifecycle_fixture;
namespace care=pet_care_fixture;
namespace {
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
PetLifecycleRequest Request(const ItemCatalog& c,bool active=true) {
    PetCareContext context;context.operationId[0]=1;context.actorId=77;context.sessionGeneration=9;context.rulesetRevision=2;
    context.characterLevel=90;context.characterClass=context.sex=1;context.admittedByServer=context.canUseItems=context.ownerAlive=true;
    PetCareItemSnapshot seal{care::Pet(),120,2,77,1,0,false,false,false,true,active,active ? 9u : 0u};
    GrowthPetState state{"Fixture",100000,0,{1,0,0,0},MakeGrowthPetLife(c,care::Pet()).value(),{35,15,20}};
    return {context,seal,3,100000,state};
}
PetSupplyRequest Supply(const ItemCatalog& c,unsigned kind,bool active=true) {
    return {Request(c,active),{f::Supply(kind),200,2,77,1}};
}
void AgeAndEvolution() {
    const auto c=BuildItemCatalog(f::Catalog()).value();auto r=Request(*c);
    auto age=PlanPetAge(*c,r).value();Check(std::ranges::none_of(age.after.skills,[](auto& slot){return slot.unlocked;}),"fresh pet has four locked slots");
    r.nowUtcMs+=1500;age=PlanPetAge(*c,r).value();Check(age.after.skills[3].unlocked && !age.after.skills[0].unlocked,"fourth slot opens by wall age alone");
    Check(r.state.ageMs==0 && !r.state.skills[3].unlocked,"age planning owns snapshots without mutation");
    r.state=age.after;r.nowUtcMs=100001;Check(PlanPetAge(*c,r)->after.ageMs==1500,"UTC rollback cannot reduce persisted age");
    r=Request(*c);auto evolved=PlanPetEvolution(*c,r,{6,0,0}).value();
    Check(evolved.after.progress==GrowthPetProgress{2,1,0,0} && evolved.after.skills[0].unlocked,"evolution commits progression and first slot together");
    Check(evolved.after.traits==PetBirthTraits{41,15,20} && evolved.after.life==r.state.life,"recorded trait growth preserves life");
    r.state=evolved.after;evolved=PlanPetEvolution(*c,r,{0,2,0}).value();
    Check(evolved.after.progress.level==3 && evolved.after.skills[1].unlocked && evolved.after.traits.monsterDamagePercentTenths==17,"next evolution uses its own cadence");
    Check(!PlanPetEvolution(*c,r,{1,2,0}),"unexpected growth in a non-due trait rejected");
    r.state=evolved.after;Check(!PlanPetEvolution(*c,r,{}),"maximum evolution cannot be repeated");
    r=Request(*c);r.seal.active=false;r.seal.actorGeneration=0;Check(!PlanPetEvolution(*c,r,{1,0,0}),"inactive pet cannot evolve");
    auto s=f::Catalog();s.petTrainingProfiles[0].hpEvolutionPeriod=1;
    const auto saturated=BuildItemCatalog(s).value();r=Request(*saturated);r.state.traits.maximumHpPercentTenths=32766;
    Check(PlanPetEvolution(*saturated,r,{6,0,0})->after.traits.maximumHpPercentTenths==32767,"trait growth clamps only its own field");
}
void StudyResetAndCooldown() {
    const auto c=BuildItemCatalog(f::Catalog()).value();auto r=Supply(*c,1);r.selectedSlot=0;
    Check(PlanPetSupply(*c,r).error()==PetLifecycleError::LockedSlot,"locked slot never consumes a book");
    r.pet.state=PlanPetEvolution(*c,r.pet,{1,0,0})->after;
    auto learned=PlanPetSupply(*c,r).value();Check(learned.succeeded && learned.after.skills[0]==PetSkillSlot{true,1,1} && learned.consumeUnits==1,"certain selected-slot study");
    Check(!learned.writeOwnerCooldown && r.pet.state.skills[0].rank==0,"selected mode does not mutate shared cooldown or input");
    r.pet.state=learned.after;r.selectedSlot.reset();r.ownerCooldownRevision=3;r.recordedRollBp=7099;
    auto raised=PlanPetSupply(*c,r).value();Check(raised.after.skills[0].rank==2 && raised.ownerSkillNotBeforeUtcMs==10900000 && raised.writeOwnerCooldown,"71 percent boundary and three-hour owner cooldown");
    r.recordedRollBp=7100;auto failed=PlanPetSupply(*c,r).value();Check(!failed.succeeded && failed.consumeUnits==1 && failed.after.skills[0].rank==1 && !failed.writeOwnerCooldown,"failed book consumes one without rank or cooldown grant");
    r.ownerSkillNotBeforeUtcMs=100001;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::Cooldown,"future owner-skill delay enforced");
    r.bypassAvailable=true;Check(!PlanPetSupply(*c,r),"delay flag without an effect revision grants no authority");
    r.bypassEffectRevision=7;failed=PlanPetSupply(*c,r).value();Check(failed.consumeBypass && !failed.succeeded,"bypass and failed book are one atomic consumption intent");
    r.ownerSkillNotBeforeUtcMs=100000;Check(!PlanPetSupply(*c,r)->consumeBypass,"unnecessary delay bypass is preserved");
    r.pet.state=raised.after;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::MaximumRank,"maximum never consumes a book");
    r=Supply(*c,1);r.pet.state.skills[0]={true,1,1};r.pet.state.skills[3].unlocked=true;r.selectedSlot=3;
    Check(PlanPetSupply(*c,r).error()==PetLifecycleError::DuplicateSkill,"same skill cannot be learned in a second slot");
    r=Supply(*c,2);r.pet.state.skills[0]={true,1,2};r.selectedSlot=0;
    auto reset=PlanPetSupply(*c,r).value();Check(reset.after.skills[0]==PetSkillSlot{true,0,0} && !reset.after.skills[2].unlocked,"reset preserves unlock/lock state");
    r.supply.key=f::Supply(3);r.selectedSlot.reset();r.pet.state.skills[3]={true,2,1};reset=PlanPetSupply(*c,r).value();
    Check(std::ranges::none_of(reset.after.skills,[](auto& slot){return slot.skill!=0;}) && !reset.writeOwnerCooldown,"reset-all leaves owner cooldown state untouched");
    r.pet.state=reset.after;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::NoEffect,"empty reset consumes nothing");
    for(unsigned i=0;i<8;++i) {
        auto bad=Supply(*c,1);bad.pet.state.skills[0].unlocked=true;bad.selectedSlot=0;
        if(i==0)bad.supply.ownerId=78;if(i==1)bad.supply.lockedByOther=true;if(i==2)bad.supply.exchanging=true;
        if(i==3)bad.supply.equipped=true;if(i==4)bad.supply.hasPetState=true;if(i==5)bad.pet.context.admittedByServer=false;
        if(i==6)bad.pet.context.ownerAlive=false;if(i==7)bad.supply.instanceId=bad.pet.seal.instanceId;
        Check(!PlanPetSupply(*c,bad),"untrusted supply/context rejected before consumption");
    }
    r=Supply(*c,1);r.pet.state.skills[0]={true,1,1};r.ownerCooldownRevision=3;r.recordedRollBp=10000;
    Check(PlanPetSupply(*c,r).error()==PetLifecycleError::InvalidDecision,"out-of-range recorded roll rejected");
    r.recordedRollBp=0;r.pet.nowUtcMs=INT64_MAX;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::InvalidTime,"cooldown overflow rejected");
}
void LifeAndEnchantment() {
    const auto c=BuildItemCatalog(f::Catalog()).value();auto r=Supply(*c,4,false);r.pet.state.life.remainingMs=1000;
    auto extended=PlanPetSupply(*c,r).value();Check(extended.after.life==GrowthPetLife{CompanionClock::ActiveTime,86401000,86401000},"extension derives a new capacity from the saved remainder");
    Check(extended.after.traits==r.pet.state.traits && extended.after.progress==r.pet.state.progress,"extension preserves progression and traits");
    r.pet.state.life={CompanionClock::ActiveTime,31535940000,31535940000};Check(!PlanPetSupply(*c,r),"extension cannot exceed its explicit cap");
    r=Supply(*c,5,false);auto forever=PlanPetSupply(*c,r).value();
    Check(forever.after.life==GrowthPetLife{CompanionClock::Permanent,0,0,true},"permanent service records a durable explicit grant");
    Check(ValidateGrowthPetState(*c,care::Pet(),forever.after).has_value(),"permanent grant validates under an active-time prototype");
    auto tick=PlanPetLife(*c,{r.pet.context,r.pet.seal,forever.after.life,3,INT64_MAX});Check(tick && !tick->expired,"permanent grant survives care clock maintenance");
    r.pet.state=forever.after;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::NoEffect,"permanent service cannot be consumed twice");
    r.pet.state.life.permanentGrant=false;Check(!ValidateGrowthPetState(*c,care::Pet(),r.pet.state),"clock type cannot change without its durable grant");
    r=Supply(*c,6,false);r.pet.state.traits.maximumHpPercentTenths=149;r.recordedRollBp=2999;
    auto enchanted=PlanPetSupply(*c,r).value();Check(enchanted.succeeded && enchanted.after.traits.maximumHpPercentTenths==150,"enchantment caps the selected trait without wrapping");
    r.recordedRollBp=3000;enchanted=PlanPetSupply(*c,r).value();Check(!enchanted.succeeded && enchanted.consumeUnits==1 && enchanted.after.traits==r.pet.state.traits,"30 percent boundary and failure consumption");
    r.pet.state.traits.maximumHpPercentTenths=150;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::NoEffect,"capped trait refuses consumption");
    r.traitIndex=3;Check(PlanPetSupply(*c,r).error()==PetLifecycleError::InvalidDecision,"invalid enchant target refused");
}
void BuffTransportAndSkin() {
    const auto c=BuildItemCatalog(f::Catalog()).value();auto r=Request(*c);r.state.skills[0]={true,1,1};r.nowUtcMs+=3000;
    auto bonus=PlanPetBonuses(*c,r,true,false).value();Check(bonus.contributions.size()==7 && bonus.replaceSource,"passive bundle replaces one source and includes age/traits/skills");
    Check(bonus.contributions[0].effect==PetEffect::MaximumHpFlat && bonus.contributions[0].value==3000,"age tier boundary uses integer HP");
    Check(bonus.contributions[3].effect==PetEffect::MaximumHpPercent && bonus.contributions[3].value==300,"trait 3.5 percent preserves source truncation to 3 percent");
    Check(bonus.contributions.back().effect==PetEffect::WarriorResistancePercent && bonus.contributions.back().value==100,"skill rank uses explicit basis points");
    for(unsigned i=0;i<5;++i) {
        auto absent=r;if(i==0){absent.seal.active=false;absent.seal.actorGeneration=0;}if(i==1)absent.context.ownerAlive=false;
        if(i==2)absent.context.polymorphed=true;if(i==3)absent.context.observer=true;if(i==4)absent.state.life.remainingMs=0;
        Check(PlanPetBonuses(*c,absent,true,false)->contributions.empty(),"inactive/invalid world state removes the source bundle");
    }
    Check(PlanPetBonuses(*c,r,false,false)->contributions.empty(),"world policy suppresses all pet contributions");
    PetTransportRequest transport{Request(*c,false),{f::Supply(7),300,4,77,1},false,false,5,0};
    transport.pet.state.skills[0]={true,1,2};transport.pet.state.life.remainingMs=1234;
    const auto original=transport.pet.state;auto packed=PlanPetTransport(*c,transport).value();
    Check(packed.after.carrierInstanceId==300 && !packed.consumeCarrier,"pack preserves the seal identity in an occupied carrier");
    transport.pet.state=packed.after;Check(!PlanPetTransport(*c,transport),"double pack refused");
    transport.unpack=true;transport.carrier.containsPet=true;transport.containedSealInstanceId=120;Check(PlanPetTransport(*c,transport).error()==PetLifecycleError::InvalidPlacement,"unpack requires reserved inventory placement");
    transport.placementReservedByServer=true;auto unpacked=PlanPetTransport(*c,transport).value();
    Check(unpacked.after==original && unpacked.consumeCarrier,"pack/unpack preserves every field without life refill or ID reconstruction");
    transport.containedSealInstanceId=121;Check(!PlanPetTransport(*c,transport),"a carrier cannot claim another seal");
    PetAppearanceRequest skin{Request(*c),{f::Supply(8),400,2,77,1},{true,false,0,10000},{100000,1000,false,false},true,true};
    auto visual=PlanPetAppearance(*c,skin).value();Check(visual.equip && visual.form==2 && visual.lifetime.after.remainingMs==10000,"skin changes only display and idle time is uncharged");
    skin.skin.equipped=true;skin.time.wasEquipped=true;skin.time.elapsedMs=10000;
    visual=PlanPetAppearance(*c,skin).value();Check(!visual.equip && visual.form==1 && visual.lifetime.removeItem && visual.lifetime.unequip,"expired skin restores the pet evolution form");
    Check(skin.pet.state==Request(*c).state,"appearance never rewrites pet stats or lifecycle state");
    skin.skin.ownerId=78;Check(!PlanPetAppearance(*c,skin),"foreign skin rejected");
}
void PersistenceAndCatalog() {
    auto source=f::Catalog();const auto c=BuildItemCatalog(source).value();auto r=Request(*c,false);
    r.state.skills[0]={true,1,2};r.state.skills[3]={true,2,1};r.state.carrierInstanceId=300;
    r.state.ageMs=123456789;r.state.life.remainingMs=1234;
    auto encoded=EncodeGrowthPetState(*c,care::Pet(),r.state).value();
    Check(DecodeGrowthPetState(*c,care::Pet(),encoded).value()==r.state,"complete persisted payload roundtrip");
    for(std::size_t n=0;n<encoded.size();++n)Check(!DecodeGrowthPetState(*c,care::Pet(),std::span(encoded).first(n)),"truncated payload rejected");
    auto bad=encoded;bad.push_back(0);Check(!DecodeGrowthPetState(*c,care::Pet(),bad),"trailing bytes rejected");
    bad=encoded;bad[4]=2;Check(!DecodeGrowthPetState(*c,care::Pet(),bad),"unknown payload version refused");
    bad=encoded;bad[6]=14;Check(!DecodeGrowthPetState(*c,care::Pet(),bad),"wrong prototype identity refused");
    r.state.skills[1]={true,1,1};Check(!EncodeGrowthPetState(*c,care::Pet(),r.state),"duplicate learned skill not persisted");
    auto decoded=auriga::item::sql::DecodeSnapshot(catalog_snapshot_fixture::Snapshot(source),{9,c->ContentHash()});
    Check(decoded && (*decoded)->FindPetTrainingProfile(1)->skills.size()==2 && (*decoded)->FindPetSupplyProfile(8)->skin->form==2,"all lifecycle SQL tables roundtrip");
    const auto reject=[&](auto mutate){auto s=source;mutate(s);Check(!BuildItemCatalog(s),"invalid lifecycle content cannot publish");};
    reject([](auto& s){s.petTrainingProfiles[0].slots.pop_back();});reject([](auto& s){s.petTrainingProfiles[0].ages[0].minimumAgeMs=1;});
    reject([](auto& s){s.petTrainingProfiles[0].skills[0].ranks[0].rank=2;});reject([](auto& s){s.petTrainingProfiles[0].skills[0].effect=PetEffect(0);});
    reject([](auto& s){s.petSupplyProfiles[0].study->skill=999;});reject([](auto& s){s.petSupplyProfiles[3].extension->addedMs=0;});
    reject([](auto& s){s.petSupplyProfiles[5].enchant->successBp=10001;});reject([](auto& s){s.petSupplyProfiles[7].skin->form=999;});
    reject([](auto& s){s.petSupplyProfiles[6].enchant=PetEnchantRule{3000,5,{150,100,100}};});
    reject([](auto& s){s.items.back().maxStack=2;});reject([](auto& s){s.companions[0].trainingProfile=999;});
    auto reordered=source;std::ranges::reverse(reordered.petTrainingProfiles[0].ages);std::ranges::reverse(reordered.petSupplyProfiles);
    Check(BuildItemCatalog(reordered).value()->ContentHash()==c->ContentHash(),"canonical lifecycle order independent of SQL row order");
    source.petTrainingProfiles[0].skills[0].ranks[0].value=150;
    Check(BuildItemCatalog(source).value()->ContentHash()!=c->ContentHash(),"passive skill content changes catalog identity");
}
void CompleteJourney() {
    const auto c=BuildItemCatalog(f::Catalog()).value();auto r=Request(*c);
    PetHatchRequest birth{r.context,{care::Egg(),100,1,77,2}};birth.walletRevision=1;birth.gold=100000;
    birth.newInstanceId=r.seal.instanceId;birth.bornAtUtcMs=100000;birth.name="Fixture";birth.recordedTraits={35,15,20};birth.nameAcceptedByServer=birth.newInstanceReservedByServer=true;
    auto hatch=PlanPetHatch(*c,birth).value();r.state=MakeGrowthPetState(*c,hatch).value();
    Check(r.state==Request(*c).state,"hatch factory initializes one complete validated pet payload");
    r.seal.active=false;r.seal.actorGeneration=0;auto summon=PlanGrowthPetSummon(*c,r,true,true,false).value();
    Check(summon.spawnFollower && summon.form==1 && summon.pet.after.everSummoned && summon.pet.after.recallRequested,"growth summon records first evocation and recall without refilling lifetime");
    Check(!PlanGrowthPetSummon(*c,r,true,true,true),"another pet occupying the channel blocks summoning");
    r.state=summon.pet.after;r.seal.active=true;r.seal.actorGeneration=9;
    Check(!PlanGrowthPetSummon(*c,r,true,true,false)->spawnFollower,"already active summon cannot spawn a duplicate");
    hatch.catalogRevision++;Check(!MakeGrowthPetState(*c,hatch),"birth factory refuses a stale catalog pin");
    r.nowUtcMs+=2000;auto maintained=PlanPetMaintenance(*c,r,10000).value();
    Check(maintained.pet.after.ageMs==2000 && maintained.pet.after.life.remainingMs==290000 && maintained.pet.after.skills[3].unlocked,"one maintenance plan combines age unlock and active life accounting");
    r.state=maintained.pet.after;r.state=PlanPetEvolution(*c,r,{1,0,0})->after;
    PetSupplyRequest study{r,{f::Supply(1),200,2,77,3},0};r.state=PlanPetSupply(*c,study)->after;
    PetFeedRequest feed{r.context,{item_catalog_fixture::Leather(),210,2,77,2},r.seal,r.state.life,r.petRevision,PetFeedingService::Cube,{}};
    const auto before=r.state;auto food=PlanPetFeed(*c,feed).value();r.state.life=food.after;
    Check(r.state.life.remainingMs==299000 && r.state.skills==before.skills && r.state.traits==before.traits,"feeding changes only the complete state's life component");
    auto dismissed=PlanGrowthPetSummon(*c,r,false,false,true).value();Check(dismissed.despawnFollower && !dismissed.pet.after.recallRequested && dismissed.pet.after.everSummoned,"dismissal preserves first evocation and is allowed after world restrictions");
    r.state=dismissed.pet.after;r.seal.active=false;r.seal.actorGeneration=0;
    PetTransportRequest pack{r,{f::Supply(7),300,2,77,1},false,false,1,0};auto packed=PlanPetTransport(*c,pack).value();
    auto saved=EncodeGrowthPetState(*c,r.seal.key,packed.after).value();pack.pet.state=DecodeGrowthPetState(*c,r.seal.key,saved).value();
    pack.unpack=true;pack.placementReservedByServer=true;pack.containedSealInstanceId=r.seal.instanceId;pack.carrier.containsPet=true;
    auto restored=PlanPetTransport(*c,pack).value();Check(restored.after==r.state,"birth/evolve/study/feed/pack/save/load/unpack preserves every pet field");
    auto idle=r;idle.state=restored.after;idle.nowUtcMs+=1000000;
    auto offline=PlanPetMaintenance(*c,idle,1000000).value();Check(offline.pet.after.life==idle.state.life && offline.pet.after.ageMs>idle.state.ageMs,"offline wall age grows without charging active lifetime");
    auto unknown=f::Catalog();auto other=unknown.petTrainingProfiles[0];other.id=2;other.code="training.conflicting";other.skills[0].effect=PetEffect::BossDamagePercent;unknown.petTrainingProfiles.push_back(other);
    Check(!BuildItemCatalog(unknown),"issued pet skill IDs retain one semantic meaning across species profiles");
    auto occupied=feed;occupied.food.containsPet=true;Check(PlanPetFeed(*c,occupied).error()==PetCareError::PetStateSource,"occupied transport carrier state cannot be destroyed as food");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture") {
            for(auto b:BuildItemCatalog(f::Catalog()).value()->ContentHash())std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cout<<'\n';return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="--state-fixture") {
            const auto c=BuildItemCatalog(f::Catalog()).value();auto r=Request(*c,false);r.state.skills[0]={true,1,2};r.state.skills[3]={true,2,1};r.state.carrierInstanceId=300;r.state.ageMs=123456789;r.state.life.remainingMs=1234;
            for(auto b:EncodeGrowthPetState(*c,care::Pet(),r.state).value())std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cout<<'\n';return 0;
        }
        AgeAndEvolution();StudyResetAndCooldown();LifeAndEnchantment();BuffTransportAndSkin();PersistenceAndCatalog();CompleteJourney();
        std::cout<<checks<<" Pet lifecycle checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
