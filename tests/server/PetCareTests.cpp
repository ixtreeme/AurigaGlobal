#include "PetCareFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <item/AccessoryRuntime.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace auriga::item;
namespace f=item_catalog_fixture;
namespace care=pet_care_fixture;
namespace db=auriga::item::sql;
namespace {
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
PetCareContext Context() {
    PetCareContext c;c.operationId[0]=1;c.actorId=77;c.sessionGeneration=99;c.rulesetRevision=3;
    c.characterClass=c.sex=1;c.characterLevel=90;c.admittedByServer=c.canUseItems=c.ownerAlive=true;return c;
}
PetCareItemSnapshot Snapshot(ItemKey key,unsigned id=100,unsigned count=1,bool active=false) {
    PetCareItemSnapshot s{key};s.instanceId=id;s.revision=2;s.ownerId=77;s.count=count;s.active=active;
    s.actorGeneration=active ? 9 : 0;s.hasPetState=key==care::Pet();return s;
}
PetHatchRequest Hatch() {
    PetHatchRequest r{Context(),Snapshot(care::Egg(),110,5)};
    r.walletRevision=3;r.gold=200000;r.newInstanceId=150;r.bornAtUtcMs=100000;r.name="Bodri";
    r.recordedTraits={35,15,20};r.nameAcceptedByServer=r.newInstanceReservedByServer=true;return r;
}
PetLifeRequest Life(const ItemCatalog& c) {
    return {Context(),Snapshot(care::Pet(),120,1,true),MakeGrowthPetLife(c,care::Pet()).value(),7,1000};
}
PetFeedRequest Feed(const ItemCatalog&) {
    return {Context(),Snapshot(f::Leather(),110,200),Snapshot(care::Pet(),120,1,true),
            {CompanionClock::ActiveTime,300000,100000},7,PetFeedingService::Cube,{}};
}
void Hatching() {
    const auto c=BuildItemCatalog(care::Catalog()).value();auto r=Hatch();const auto before=r;
    auto plan=PlanPetHatch(*c,r).value();r.name.clear();
    Check(plan.expected.name=="Bodri" && before.egg.count==5,"hatch owns its validated input and does not mutate an egg");
    Check(plan.target==care::Pet() && plan.newItemCount==1 && plan.refinementLevel==0,"one explicit target prototype and +0 item");
    Check(plan.progress==GrowthPetProgress{1,0,0,0} && plan.initiallyLockedSkillSlots==4,"new pet progression and locked skill seed");
    Check(plan.life==GrowthPetLife{CompanionClock::ActiveTime,300000,300000},"hatch initializes capacity only once");
    Check(plan.consumeEggUnits==1 && plan.goldCost==100000 && plan.expected.walletRevision==3,"atomic resource intent");
    Check(plan.catalogHash==c->ContentHash() && plan.catalogRevision==c->Revision(),"hatch content pin");
    for(std::uint16_t hp:{30,35,40})for(std::uint16_t monster:{10,15,20})for(std::uint16_t normal:{10,15,20}) {
        r=Hatch();r.recordedTraits={hp,monster,normal};Check(PlanPetHatch(*c,r).has_value(),"all recorded birth choices accepted without sampling RNG");
    }
    r=Hatch();r.name="\xc3\x81rv\xc3\xadz";Check(PlanPetHatch(*c,r).has_value(),"valid UTF-8 name");
    r.name.clear();for(unsigned i=0;i<25;++i)r.name+="\xc3\xa1";
    Check(PlanPetHatch(*c,r).has_value(),"name bound counts codepoints and bytes separately");
    const auto reject=[&](auto mutate,PetCareError error){auto request=Hatch();mutate(request);auto p=PlanPetHatch(*c,request);Check(!p && p.error()==error,"invalid hatch rejected");};
    reject([](auto& q){q.recordedTraits.maximumHpPercentTenths=31;},PetCareError::InvalidBirthDecision);
    reject([](auto& q){q.nameAcceptedByServer=false;},PetCareError::InvalidName);
    reject([](auto& q){q.name=std::string(26,'a');},PetCareError::InvalidName);
    for(const auto& name:std::array<std::string,8>{" leading","trailing ","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82",std::string("nul\0name",8),"line\nname"})
        reject([&](auto& q){q.name=name;},PetCareError::InvalidName);
    reject([](auto& q){q.newInstanceReservedByServer=false;},PetCareError::InvalidOperation);
    reject([](auto& q){q.newInstanceId=q.egg.instanceId;},PetCareError::InvalidOperation);
    reject([](auto& q){q.context.operationId={};},PetCareError::InvalidOperation);
    reject([](auto& q){q.walletRevision=UINT64_MAX;},PetCareError::InvalidOperation);
    reject([](auto& q){q.gold=99999;},PetCareError::InsufficientGold);
    reject([](auto& q){q.bornAtUtcMs=0;},PetCareError::InvalidTime);
    reject([](auto& q){q.bornAtUtcMs=UINT64_MAX;},PetCareError::InvalidTime);
    reject([](auto& q){q.egg.key=f::GiftBox();},PetCareError::NotPetEgg);
    reject([](auto& q){q.egg.ownerId=78;},PetCareError::InvalidSnapshot);
    reject([](auto& q){q.egg.equipped=true;},PetCareError::InvalidSnapshot);
    reject([](auto& q){q.egg.hasPetState=true;},PetCareError::PetStateSource);
    reject([](auto& q){q.context.ownerAlive=false;},PetCareError::Forbidden);
    reject([](auto& q){q.context.admittedByServer=false;},PetCareError::Forbidden);
    Check(PlanContainerOpen(*c,{care::Egg(),{},1,0},[](auto){throw std::runtime_error("egg must not draw loot");return 0u;}).error()==ContainerOpenError::PetEggRequiresHatching,"ordinary loot opener cannot bypass hatching");
}
void DurableHatching()
{
    auto catalog=care::Catalog();
    std::ranges::find(catalog.items,care::Pet(),&ItemBaseRow::key)->binding=BindingPolicy::OnAcquire;
    const auto c=BuildItemCatalog(catalog).value();
    auto request=Hatch();
    const ItemOwnerFence owner{{ItemOwnerKind::Character,77},99,7};
    const ItemStorageLayout bag{{owner.owner,ItemStorageKind::Inventory,1},2,1,1};
    ItemWallet wallet{owner.owner,WalletCurrency::Gold,request.walletRevision,request.gold};
    PersistentItem egg{ItemInstanceId::Create(request.egg.instanceId).value(),care::Egg(),owner.owner};
    egg.revision=request.egg.revision;egg.count=request.egg.count;
    egg.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
    const auto hatch=[&](const ItemStorageLayout& layout){
        return MakePetHatchCommit(*c,owner,std::span(&layout,1),std::span(&egg,1),std::span(&wallet,1),request);
    };
    const auto commit=hatch(bag);
    Check(commit && commit->items.size()==2 && commit->wallets.size()==1,"hatch has one atomic write");
    const auto& born=commit->items[1].after;
    Check(commit->items[0].after.count==4 && commit->wallets[0].after.balance==100000 &&
        born.id.Value()==request.newInstanceId && born.placement->anchor==1 && born.boundTo==owner.owner,
        "hatch spends one unit and catalog price, then binds the output in free inventory");
    Check(born.growthPet==MakeGrowthPetState(*c,PlanPetHatch(*c,request).value()).value(),
        "hatch write retains every birth field");
    const auto encoded=EncodeGrowthPetState(*c,born.key,*born.growthPet);
    Check(encoded && DecodeGrowthPetState(*c,born.key,*encoded)==born.growthPet.value(),"complete growth state codec");
    auto modified=born;
    modified.growthPet->name="Luna";
    Check(PersistentItemDigest(*c,modified)!=PersistentItemDigest(*c,born),"growth name is hashed");
    Check(!PlainInventoryItem(born,owner.owner),"growth seal cannot enter generic consumption or transfer as plain inventory");
    Check(!MakeGrantCommit(*c,{3,1},owner,std::span(&bag,1),{}, {born.key,0,1},std::array{born.id}),
        "plain grant cannot create growth state");
    auto forged=*commit;
    forged.wallets[0].after.balance+=1;
    Check(!ValidateItemCommit(*c,forged),"writer rejects altered price");
    forged=*commit;
    forged.items[1].after.growthPet->traits.maximumHpPercentTenths=31;
    Check(!ValidateItemCommit(*c,forged),"writer rejects altered birth ranges");
    forged=*commit;
    forged.items[1].after.growthPet->skills[0].unlocked=true;
    Check(!ValidateItemCommit(*c,forged),"writer rejects a forged unlocked birth slot");
    forged=*commit;
    forged.result.back()=1;
    Check(!ValidateItemCommit(*c,forged),"writer binds receipt to born identity");
    forged=*commit;
    forged.outcomeCode=1;
    Check(!ValidateItemCommit(*c,forged),"only the hatch write can create a growth seal");

    const ItemStorageLayout equipment{{owner.owner,ItemStorageKind::Equipment,1},8,1,1};
    PersistentItem accessory{ItemInstanceId::Create(99).value(),f::Bracelet(),owner.owner};
    accessory.placement=ItemPlacement{ItemStorageKind::Equipment,1,static_cast<unsigned>(EquipmentSlot::Wrist)-1};
    accessory.accessory=AccessorySocketState{1,1,86400000,false};
    auto settled=*commit;
    Check(SettleAccessoryTime(*c,settled,owner,std::array{bag,equipment},std::array{egg,accessory},500).has_value() &&
        settled.items.size()==3 && settled.items.back().after.accessory->remainingMs==86399500,
        "hatch composes with the equipped accessory clock");
    settled.items.back().after.accessory->remainingMs=86400000;
    Check(!ValidateItemCommit(*c,settled),"hatch cannot restore equipped accessory time");

    ++request.egg.revision;
    Check(hatch(bag).error()==PetHatchCommitError{ItemPersistenceError::InvalidChange},"stale egg snapshot refused");
    --request.egg.revision;
    ++request.walletRevision;
    Check(hatch(bag).error()==PetHatchCommitError{ItemPersistenceError::InvalidWallet},"stale wallet snapshot refused");
    --request.walletRevision;
    request.gold=wallet.balance=99999;
    Check(hatch(bag).error()==PetHatchCommitError{PetCareError::InsufficientGold},"not enough Gold refuses the entire hatch");
    request.gold=wallet.balance=200000;
    auto oneCell=bag;
    oneCell.columns=1;
    Check(hatch(oneCell).error()==PetHatchCommitError{ItemPersistenceError::InvalidPlacement},"a partial egg stack keeps its cell");
    request.egg.count=egg.count=1;
    const auto last=hatch(oneCell);
    Check(last && last->items[0].after.destroyed && last->items[1].after.placement==egg.placement,
        "last egg frees its cell in the same hatch");
    catalog.petHatchProfiles[0].goldCost=0;
    const auto free=BuildItemCatalog(catalog).value();
    const auto freeHatch=MakePetHatchCommit(*free,owner,std::span(&oneCell,1),std::span(&egg,1),std::span(&wallet,1),request);
    Check(freeHatch && freeHatch->wallets.empty(),"zero Gold cost leaves the wallet revision alone");
}
void Lifetime() {
    auto source=care::Catalog();auto c=BuildItemCatalog(source).value();auto r=Life(*c);
    const auto plan=PlanPetLife(*c,r).value();Check(plan.after.remainingMs==299000 && plan.consumedMs==1000,"exact active milliseconds");
    Check(r.life.remainingMs==300000 && !plan.expired,"clock proposal does not mutate state");
    r.target.active=false;r.target.actorGeneration=0;r.elapsedOwnerMs=INT64_MAX;
    Check(PlanPetLife(*c,r)->after==r.life,"offline/dormant elapsed time never charges an active clock");
    r=Life(*c);r.elapsedOwnerMs=300000;auto expired=PlanPetLife(*c,r).value();
    Check(expired.expired && expired.despawnFollower && expired.after.remainingMs==0,"expiry removes actor intent and preserves seal state");
    r.life=expired.after;r.elapsedOwnerMs=INT64_MAX;
    Check(PlanPetLife(*c,r)->consumedMs==0,"expired clock cannot underflow");
    r=Life(*c);r.life={CompanionClock::ActiveTime,600000,500000};r.context.ownerAlive=false;
    Check(PlanPetLife(*c,r)->after.remainingMs==499000,"issued capacity survives shorter prototype and death cleanup");
    r=Life(*c);r.elapsedOwnerMs=1006;const auto whole=PlanPetLife(*c,r)->after;
    r=Life(*c);for(auto elapsed:{1u,2u,3u,100u,900u}){r.elapsedOwnerMs=elapsed;r.life=PlanPetLife(*c,r)->after;}
    Check(r.life==whole,"clock splitting cannot refill or lose a remainder");
    r.life.capacityMs=1;Check(PlanPetLife(*c,r).error()==PetCareError::InvalidLife,"remaining cannot exceed capacity");
    r=Life(*c);r.elapsedOwnerMs=UINT64_MAX;Check(PlanPetLife(*c,r).error()==PetCareError::InvalidTime,"invalid elapsed time refused");
    r=Life(*c);r.petRevision=0;Check(PlanPetLife(*c,r).error()==PetCareError::InvalidSnapshot,"clock revision required");
    r=Life(*c);r.target.actorGeneration=0;Check(PlanPetLife(*c,r).error()==PetCareError::InvalidSnapshot,"active actor generation required");
    source.companions[0].clock=CompanionClock::Permanent;source.companions[0].durationMs=0;c=BuildItemCatalog(source).value();r=Life(*c);
    r.elapsedOwnerMs=INT64_MAX;auto permanent=PlanPetLife(*c,r).value();
    Check(!permanent.expired && permanent.consumedMs==0 && permanent.after.capacityMs==0,"explicit permanent clock has no sentinel duration");
    r.life={CompanionClock::ActiveTime,300000,200000};Check(PlanPetLife(*c,r).error()==PetCareError::InvalidLife,"clock type cannot silently reinterpret saved life");
}
void Feeding() {
    auto c=BuildItemCatalog(care::Catalog()).value();auto r=Feed(*c);const auto before=r;
    auto plan=PlanPetFeed(*c,r).value();
    Check(plan.consumeFoodUnits==200 && plan.restoredMs==9000 && plan.after.remainingMs==109000,"whole-stack rule restores one configured fraction");
    Check(r.life==before.life && r.food.count==200 && !plan.progress,"life feeding preserves progression and source state");
    r.food=Snapshot(care::Egg(),110,4);plan=PlanPetFeed(*c,r).value();
    Check(plan.consumeFoodUnits==1 && plan.restoredMs==15000,"egg feeding consumes exactly configured units");
    r.life.remainingMs=299999;Check(PlanPetFeed(*c,r)->after.remainingMs==300000,"feed caps at saved capacity");
    r.life.remainingMs=300000;Check(PlanPetFeed(*c,r).error()==PetCareError::NoEffect,"no consumption at full capacity");
    r=Feed(*c);r.life.capacityMs=INT64_MAX;r.life.remainingMs=0;r.target.active=false;r.target.actorGeneration=0;
    r.service=PetFeedingService::Direct;r.food=Snapshot(f::SocketJewel(),110,7);
    plan=PlanPetFeed(*c,r).value();Check(plan.revived && plan.after.remainingMs==4611686018427387903ULL && plan.consumeFoodUnits==1,"large half-capacity revival is exact without multiply overflow");
    r.life.remainingMs=1;Check(PlanPetFeed(*c,r)->after.remainingMs==INT64_MAX,"living direct target refills to saved capacity");
    r=Feed(*c);r.life.capacityMs=INT64_MAX;r.life.remainingMs=1;
    Check(PlanPetFeed(*c,r)->restoredMs==276701161105643274ULL,"three-percent INT64_MAX golden");
    r=Feed(*c);r.life={CompanionClock::ActiveTime,1,0};r.target.active=false;r.target.actorGeneration=0;r.service=PetFeedingService::Direct;r.food=Snapshot(f::SocketJewel());
    Check(PlanPetFeed(*c,r).error()==PetCareError::NoEffect,"zero rounded revival never consumes food");
    r=Feed(*c);r.food=Snapshot(f::Sword(),110);r.progress=GrowthPetProgress{1,0,0,0};
    plan=PlanPetFeed(*c,r).value();
    Check(plan.progress && plan.progress->after.itemExperience==10 && plan.progress->discardedExperience==40,"equipment grants item EXP from authoritative price");
    Check(plan.after==r.life && plan.consumeFoodUnits==1,"EXP feed preserves life");
    r.progress=GrowthPetProgress{1,0,100,0};plan=PlanPetFeed(*c,r).value();
    Check(plan.progress->levelled && plan.progress->after.level==2,"feeding completes dual-pool level with existing growth domain");
    r.progress=GrowthPetProgress{3,0,0,0};Check(PlanPetFeed(*c,r).error()==PetCareError::NoEffect,"no equipment loss at maximum level");
    r=Feed(*c);r.food.hasPetState=true;Check(PlanPetFeed(*c,r).error()==PetCareError::PetStateSource,"no orphaned pet record from food sacrifice");
    r=Feed(*c);r.food.ownerId=78;Check(PlanPetFeed(*c,r).error()==PetCareError::InvalidSnapshot,"foreign food refused");
    r=Feed(*c);r.food.lockedByOther=true;Check(PlanPetFeed(*c,r).error()==PetCareError::InvalidSnapshot,"locked source refused");
    r=Feed(*c);r.food.instanceId=r.target.instanceId;Check(PlanPetFeed(*c,r).error()==PetCareError::InvalidSnapshot,"cannot eat own summon seal");
    r=Feed(*c);r.target.active=false;r.target.actorGeneration=0;Check(PlanPetFeed(*c,r).error()==PetCareError::WrongTargetState,"cube requires live target");
    r=Feed(*c);r.life.remainingMs=0;Check(PlanPetFeed(*c,r).error()==PetCareError::WrongTargetState,"dead active marker is not a feeding authority");
    r=Feed(*c);r.target.active=false;r.target.actorGeneration=0;r.service=PetFeedingService::Direct;
    Check(PlanPetFeed(*c,r).error()==PetCareError::MissingFeedRule,"direct and cube rules are distinct");
    r=Feed(*c);r.food=Snapshot(f::Sword(),110);Check(PlanPetFeed(*c,r).error()==PetCareError::InvalidProgress,"EXP feed requires authoritative progression");
}
void CatalogAndCodec() {
    const auto source=care::Catalog();auto c=BuildItemCatalog(source).value();
    auto snapshot=catalog_snapshot_fixture::Snapshot(source);auto decoded=db::DecodeSnapshot(snapshot,{9,c->ContentHash()});
    Check(decoded && (*decoded)->FindPetHatchProfile(1)->goldCost==100000,"hatching SQL snapshot");
    Check((*decoded)->FindPetFeedRule(f::Leather(),PetFeedingService::Cube)->units==0,"whole-stack rule survives SQL NULL-free decoding");
    const auto reject=[&](auto mutate){auto s=source;mutate(s);Check(!BuildItemCatalog(s),"bad care content rejected before publication");};
    reject([](auto& s){s.petHatchProfiles[0].maximumHp.step=0;});
    reject([](auto& s){s.petHatchProfiles[0].maximumHp.step=3;});
    reject([](auto& s){s.petHatchProfiles[0].target=f::Sword();});
    reject([](auto& s){s.petHatchProfiles[0].goldCost=UINT64_MAX;});
    reject([](auto& s){s.containers.back().hatchProfile.reset();});
    reject([](auto& s){s.containers.back().lootProfile=1;});
    reject([](auto& s){s.petFeedRules.push_back(s.petFeedRules[0]);});
    auto sacrifice=source;sacrifice.petFeedRules[0].key=care::Pet();
    Check(BuildItemCatalog(sacrifice).has_value(),"explicit growth-seal lifetime feeding can be published");
    reject([](auto& s){s.petFeedRules[1].key=care::Pet();});
    reject([](auto& s){s.petFeedRules[0].lifetimeFractionBp=10001;});
    reject([](auto& s){s.petFeedRules[0].units=1;});
    reject([](auto& s){s.petFeedRules[1].key=f::Leather();});
    reject([](auto& s){s.petFeedRules[1].experiencePriceDivisor=0;});
    for(unsigned n=0;n<4;++n){auto changed=source;if(n==0)changed.petHatchProfiles[0].goldCost++;
        if(n==1)changed.petHatchProfiles[0].maximumHp.maximum+=5;if(n==2)changed.petFeedRules[0].lifetimeFractionBp++;
        if(n==3)changed.petFeedRules[1].experiencePriceDivisor++;
        Check(BuildItemCatalog(changed).value()->ContentHash()!=c->ContentHash(),"care content is release-hashed");}
    auto reversed=source;std::ranges::reverse(reversed.petFeedRules);
    Check(BuildItemCatalog(reversed).value()->ContentHash()==c->ContentHash(),"feed rule order canonicalizes");
    snapshot[db::Table::PetFeedRules][0][2]="0";Check(!db::DecodeSnapshot(snapshot,{1,{}}),"bad service rejected natively");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--hash-fixture") {
            for(auto b:BuildItemCatalog(care::Catalog()).value()->ContentHash())std::cout<<"0123456789abcdef"[b>>4]<<"0123456789abcdef"[b&15];std::cout<<'\n';return 0;
        }
        Hatching();DurableHatching();Lifetime();Feeding();CatalogAndCodec();std::cout<<checks<<" Pet care checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
