#include <item/ToolRuntime.hpp>
#include <item/AttributeRuntime.hpp>
#include <item/EquipmentStats.hpp>
#include <item/TypedInventoryWire.hpp>
#include <item/TypedItemDetails.hpp>
// GameServer boot-time typed catalog (F4, IC-034) against an owned MariaDB.
// Driven by ItemCatalogRuntimeTests.py: stdin carries port, reader password,
// expected item count and expected hash. Never a live database.
#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/core/GameConfig.h"
#include "../../SRC/Server/GameServer/item/ItemCatalogRuntime.hpp"
#include <item/ItemCatalog.hpp>
#include <item/ContainerOpening.hpp>
#include "../../SRC/Server/GameServer/item/ItemStore.hpp"
#include "../../SRC/Server/GameServer/item/TypedAccessoryClock.hpp"
#include "../../SRC/Server/GameServer/item/TypedBooks.hpp"
#include "../../SRC/Server/GameServer/item/TypedCompanions.hpp"
#include "../../SRC/Server/GameServer/item/TypedRecovery.hpp"
#include <SQL/AsyncSQL.h>
#include <LiveMobDropsSql.hpp>

#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

namespace runtime = auriga::item::runtime;

// This harness runs the item worker without an ECS world or player clocks.
bool DeferTypedBookLogin(std::uint32_t,std::function<void()>) { return false; }
void TickTypedRecovery() {}
void TickTypedAccessoryClocks() {}
namespace auriga::item::runtime
{
bool DeferRecoveryLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferDungeonEntryLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferAccessoryLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferGrowthPetLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferCostumeLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferPetSkinLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferFixedCompanionLogin(std::uint32_t,std::function<void()>) { return false; }
bool DeferSoulLogin(std::uint32_t,std::function<void()>) { return false; }
std::function<void(MultiCommitOutcome*)> BeginCostumeCommit(const std::vector<OwnerRead>&,const ItemCommit&)
{ return [](MultiCommitOutcome*) {}; }
std::function<void(MultiCommitOutcome*)> BeginPetSkinCommit(const std::vector<OwnerRead>&,const ItemCommit&)
{ return [](MultiCommitOutcome*) {}; }
std::function<void(MultiCommitOutcome*)> BeginFixedCompanionCommit(const std::vector<OwnerRead>&,const ItemCommit&)
{ return [](MultiCommitOutcome*) {}; }
std::function<void(MultiCommitOutcome*)> BeginSoulCommit(const std::vector<OwnerRead>&,const ItemCommit&)
{ return [](MultiCommitOutcome*) {}; }
std::expected<std::function<void(MultiCommitOutcome*)>,ItemPersistenceError> BeginRecoveryClockCommit(
    const std::vector<OwnerRead>&,ItemCommit&) { return [](MultiCommitOutcome*) {}; }
std::expected<std::function<void(MultiCommitOutcome*)>,ItemPersistenceError> BeginAccessoryClockCommit(
    const std::vector<OwnerRead>&,ItemCommit&) { return [](MultiCommitOutcome*) {}; }
std::function<void(MultiCommitOutcome*)> BeginGrowthPetClockCommit(const std::vector<OwnerRead>&,const ItemCommit&)
{ return [](MultiCommitOutcome*) {}; }
}

namespace
{
int checks = 0;
void Check(bool result, const std::string& message)
{
	++checks;
	if (!result) throw std::runtime_error(message);
}
}


#include "LiveDropWorkerCases.hpp"
void ToolCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& packetPath,std::uint32_t sourceLocal,std::uint32_t nextLocal,std::uint32_t failureLocal)
{
    using namespace auriga::item;
    CAsyncSQL importer;importer.SetTimeouts(5);importer.SetRetryConnectionErrors(false);
    Check(importer.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"tool importer connects to the owned sandbox");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-tool-worker").has_value(),"actual item worker starts with the converted catalog");
    const auto sourceKey=ItemKey::Create(11,sourceLocal).value(),nextKey=ItemKey::Create(11,nextLocal).value(),failureKey=ItemKey::Create(11,failureLocal).value();
    const auto& row=std::get<ItemToolRow>(catalog->Find(sourceKey)->category);const auto& terms=catalog->FindToolProfile(row.profile)->levels[0];
    Check(terms.successItem==nextKey && terms.failureItem==failureKey && terms.refineChanceBp==5000 && terms.requiredPoints==160,"worker uses the actual converted source prototype graph");
    for(const bool success:{false,true})
    {
        const ItemOwner owner{ItemOwnerKind::Character,success ? 7012u : 7011u};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},24,1,1}};
        Check(persistence::ProvisionOwner(importer,*catalog,owner,layouts,{}).has_value(),"source-tool owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(importer,*catalog,owner).value();std::optional<runtime::CommitOutcome> got;
        const auto apply=[&](const ItemCommit& commit){got.reset();Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"source-tool commit queued");for(unsigned n=0;!got && n<1000;++n){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"source-tool commit persists and reloads");state=*got->state;};
        const ItemInstanceId id=ItemInstanceId::Create(0xF200000000000000ULL+owner.id).value();
        ItemGrant spec{sourceKey,0,1};spec.tool=ToolInstanceState{terms.requiredPoints-1,50};const std::array ids{id};
        auto grant=MakeGrantCommit(*catalog,{0xF2,static_cast<std::uint8_t>(success),1},state.fence,state.storages,state.items,spec,ids).value();apply(grant);
        apply(MakeEquipCommit(*catalog,{0xF2,static_cast<std::uint8_t>(success),2},state.fence,state.storages,state.items,id,{255,0,0}).value());
        ToolRuntimeActor actor{255,0,0,true,false,false,true,true};
        auto practice=MakeToolRuntimeCommit(*catalog,{0xF2,static_cast<std::uint8_t>(success),3},state.fence,state.storages,state.items,{id,{},ToolRuntimeAction::Practice,actor,1}).value();apply(practice.commit);
        Check(state.items[0].tool->trainingPoints==160,"completed server practice reaches the source threshold");
        apply(MakeUnequipCommit(*catalog,{0xF2,static_cast<std::uint8_t>(success),4},state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0}).value());
        const auto before=state.items[0];ToolRuntimeRequest request{id,{},ToolRuntimeAction::Refine,actor,success ? 5000u : 5001u};
        const auto plan=MakeToolRuntimeCommit(*catalog,{0xF2,static_cast<std::uint8_t>(success),5},state.fence,state.storages,state.items,request).value();apply(plan.commit);
        Check(!got->write->replay && state.items.size()==1 && state.items[0].key==(success ? nextKey : failureKey) && state.items[0].tool==ToolInstanceState{},"source success/failure changes the exact prototype and clears its old bait");
        apply(plan.commit);Check(got->write->replay && state.items.size()==1,"duplicate source refinement reuses its receipt without another transform");
        const auto session=persistence::ClaimOwnerSession(importer,*catalog,owner).value();const auto relog=persistence::LoadItemOwner(importer,*catalog,owner,session).value();
        Check(relog.items.size()==1 && relog.items[0]==state.items[0],"source tool identity, state and revision survive relog");
        const auto wire=EncodeToolRuntime(*catalog,{3,255,plan.outcome,932,&before,&before,ToolRuntimeAction::Refine,987,request.roll,plan.level,plan.state,plan.afterItem}).value();
        std::ofstream file(packetPath+(success ? ".success.hex" : ".failure.hex"),std::ios::binary);
        for(auto b:wire){auto v=std::to_integer<unsigned>(b);file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}file<<'\n';file.close();Check(file.good(),"actual converted-tool worker packet exported");
    }
    store.Stop();std::cout<<"SOURCE_TOOL_WORKER=SUCCESS practice=160 success=exact failure=exact replay=reused relog=retained checks="<<checks<<"\n";
}

void SkinCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& packetPath,const std::array<std::uint32_t,4>& locals)
{
    using namespace auriga::item;
    CAsyncSQL importer;importer.SetTimeouts(5);importer.SetRetryConnectionErrors(false);
    Check(importer.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"skin importer connects to the owned sandbox");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-skin-worker").has_value(),"actual skin worker starts");
    const auto issuedAt=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for(unsigned n=0;n<locals.size();++n) {
        const auto key=ItemKey::Create(4,locals[n]).value();const auto& row=std::get<ItemCostumeRow>(catalog->Find(key)->category);
        Check(row.kind==(n<2 ? CostumeKind::PetSkin : CostumeKind::MountSkin) && row.companionForm &&
            row.clock==(n%2 ? CostumeClock::Permanent : CostumeClock::SinceFirstUse),"actual converted pet/mount and timed/permanent definitions");
        const ItemOwner owner{ItemOwnerKind::Character,7020u+n};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},EquipmentSlotCount,1,1}};
        Check(persistence::ProvisionOwner(importer,*catalog,owner,layouts,{}).has_value(),"skin test owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(importer,*catalog,owner).value();std::optional<runtime::CommitOutcome> got;
        const auto apply=[&](const ItemCommit& commit){got.reset();Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"skin commit queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"skin commit persists and reloads");state=*got->state;};
        const auto output=[&](const std::string& phase) {
            const auto bytes=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value();
            const auto decoded=DecodeTypedInventory(bytes,owner);Check(decoded.has_value(),"actual skin inventory packet decodes natively");
            std::ofstream file(packetPath+"."+std::to_string(n)+"."+phase+".hex");static constexpr char h[]="0123456789abcdef";
            for(auto b:bytes){const auto v=std::to_integer<unsigned>(b);file<<h[v>>4]<<h[v&15];}file<<'\n';
        };
        const ItemInstanceId id=ItemInstanceId::Create(0xF300000000000000ULL+owner.id).value();const std::array ids{id};
        ItemGrant spec{key,0,1};spec.createdAtUtcMs=issuedAt;
        apply(MakeGrantCommit(*catalog,{0xF3,static_cast<std::uint8_t>(n),1},state.fence,state.storages,state.items,spec,ids).value());output("bag");
        {
            const auto metadata=EncodeTypedItemDetails(*catalog,state.items).value();
            std::ofstream file(packetPath+"."+std::to_string(n)+".details.hex");static constexpr char h[]="0123456789abcdef";
            for(auto b:metadata){const auto v=std::to_integer<unsigned>(b);file<<h[v>>4]<<h[v&15];}file<<'\n';
        }
        Check(!WornCompanionCostumeForm(*catalog,state.items,row.kind),"skin in bag has no form override");
        const auto equip=MakeEquipCommit(*catalog,{0xF3,static_cast<std::uint8_t>(n),2},state.fence,state.storages,state.items,id,{255,0,0,issuedAt+1000}).value();apply(equip);
        Check(WornCompanionCostumeForm(*catalog,state.items,row.kind)==row.companionForm,"worn source skin supplies its exact form");
        auto bonuses=BuildEquipmentStats(*catalog,owner,state.items).value();Check(bonuses.equipped.size()==1,"skin bonuses are equipment without a summon");
        std::array<std::int64_t,MaximumBonusId+1> expected{};
        if(const auto profile=catalog->Find(key)->base.baseBonusProfile)for(const auto& b:catalog->FindBonus(*profile)->Data().entries)expected[static_cast<unsigned>(b.bonus)]+=b.valueScaled;
        Check(bonuses.bonuses==expected,"actual source skin grants every converted bonus exactly once");
        const auto old=state.items[0];apply(equip);Check(got->write->replay && state.items[0]==old,"equip replay never refills a skin clock");
        const auto session=persistence::ClaimOwnerSession(importer,*catalog,owner).value();state=persistence::LoadItemOwner(importer,*catalog,owner,session).value();
        Check(state.items[0]==old,"relog retains exact skin identity, slot and lifetime");output("worn");
        apply(MakeUnequipCommit(*catalog,{0xF3,static_cast<std::uint8_t>(n),3},state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0},{255,0,0,issuedAt+2000}).value());
        Check(!WornCompanionCostumeForm(*catalog,state.items,row.kind) && BuildEquipmentStats(*catalog,owner,state.items)->equipped.empty(),"unequip removes form and every skin bonus");output("unequipped");
        apply(MakeEquipCommit(*catalog,{0xF3,static_cast<std::uint8_t>(n),4},state.fence,state.storages,state.items,id,{255,0,0,issuedAt+3000}).value());
        Check(state.items[0].costume==old.costume,"re-equip does not extend first-use lifetime");
        if(n%2==0){
            apply(MakeCostumeClockCommit(*catalog,{0xF3,static_cast<std::uint8_t>(n),5},state.fence,state.storages,state.items,id,old.costume->deadlineUtcMs,0).value());
            Check(state.items.empty() && !WornCompanionCostumeForm(*catalog,state.items,row.kind) && BuildEquipmentStats(*catalog,owner,state.items)->equipped.empty(),"expiry removes the live item, form and bonuses on SQL reload");output("expired");
        }
    }
    store.Stop();std::cout<<"SOURCE_SKIN_WORKER=SUCCESS variants=4 replay=reused relog=retained checks="<<checks<<"\n";
}

void BodyEffectCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& packetPath,const std::array<std::uint32_t,3>& locals,std::uint32_t armorLocal)
{
    using namespace auriga::item;
    CAsyncSQL importer;importer.SetTimeouts(5);importer.SetRetryConnectionErrors(false);
    Check(importer.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"body effect importer connects to the owned sandbox");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-body-effect-worker").has_value(),"actual effect worker starts");
    const auto issuedAt=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const auto armorKey=ItemKey::Create(2,armorLocal).value();Check(std::get<ItemArmorRow>(catalog->Find(armorKey)->category).slot==EquipmentSlot::Body,"actual converted body armor");
    for(unsigned n=0;n<locals.size();++n) {
        const auto key=ItemKey::Create(4,locals[n]).value();const auto& row=std::get<ItemCostumeRow>(catalog->Find(key)->category);
        Check(row.kind==CostumeKind::BodyEffect && row.bodyEffect && row.clock==static_cast<CostumeClock>(n+1),"actual permanent, creation-time and first-use source effects");
        const ItemOwner owner{ItemOwnerKind::Character,7040u+n};
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},EquipmentSlotCount,1,1}};
        Check(persistence::ProvisionOwner(importer,*catalog,owner,layouts,{}).has_value(),"effect test owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(importer,*catalog,owner).value();std::optional<runtime::CommitOutcome> got;
        const auto apply=[&](const ItemCommit& commit){got.reset();Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"effect commit queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"effect commit persists and reloads");state=*got->state;};
        const auto id=ItemInstanceId::Create(0xF400000000000000ULL+owner.id*2).value(),armorId=ItemInstanceId::Create(id.Value()+1).value();
        const auto output=[&](const std::string& phase,std::uint16_t expectedEffect) {
            Check(WornBodyCostumeEffect(*catalog,state.items)==expectedEffect,"source armor visibility gate follows current saved equipment");
            const auto wire=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value();Check(DecodeTypedInventory(wire,owner).has_value(),"actual effect inventory decodes");
            const auto write=[&](std::span<const std::uint8_t> bytes,const std::string& suffix){std::ofstream file(packetPath+"."+std::to_string(n)+"."+phase+suffix);for(auto v:bytes)file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];file<<'\n';Check(file.good(),"worker packet exported");};
            const auto raw=[](const std::vector<std::byte>& bytes){std::vector<std::uint8_t> result;for(auto b:bytes)result.push_back(std::to_integer<std::uint8_t>(b));return result;};
            write(raw(wire),".inventory.hex");write(raw(EncodeTypedItemDetails(*catalog,state.items).value()),".details.hex");
            CostumeAppearance appearance{77,state.fence.revision};appearance.bodyEffect=expectedEffect;
            for(const auto& item:state.items)if(!item.destroyed && item.key==armorKey && item.placement && item.placement->kind==ItemStorageKind::Equipment)
                if(const auto profile=catalog->Find(armorKey)->base.appearanceProfile)appearance.models[1]=catalog->FindAppearance(*profile)->modelAsset;
            write(EncodeCostumeAppearance(appearance).value(),".appearance.hex");
        };
        ItemGrant spec{key,0,1};spec.createdAtUtcMs=issuedAt;const std::array ids{id},armorIds{armorId};
        apply(MakeGrantCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),1},state.fence,state.storages,state.items,spec,ids).value());
        apply(MakeGrantCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),2},state.fence,state.storages,state.items,ItemGrant{armorKey,0,1},armorIds).value());output("bag",0);
        const auto equip=MakeEquipCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),3},state.fence,state.storages,state.items,id,{255,0,0,issuedAt+1000}).value();apply(equip);output("unarmored",0);
        const auto old=state.items[0];apply(equip);Check(got->write->replay && state.items[0]==old,"effect equip replay cannot extend the clock");
        apply(MakeEquipCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),4},state.fence,state.storages,state.items,armorId,{255,0,0,issuedAt+2000}).value());
        const auto session=persistence::ClaimOwnerSession(importer,*catalog,owner).value();state=persistence::LoadItemOwner(importer,*catalog,owner,session).value();Check(state.items[0]==old,"relog preserves effect slot and exact lifetime");output("worn",row.bodyEffect);
        auto without=state.items;without[0].placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
        const auto a=BuildEquipmentStats(*catalog,owner,state.items).value(),b=BuildEquipmentStats(*catalog,owner,without).value();
        std::array<std::int64_t,MaximumBonusId+1> expected{};if(const auto profile=catalog->Find(key)->base.baseBonusProfile)for(const auto& entry:catalog->FindBonus(*profile)->Data().entries)expected[static_cast<unsigned>(entry.bonus)]+=entry.valueScaled;
        for(unsigned i=0;i<expected.size();++i)Check(a.bonuses[i]-b.bonuses[i]==expected[i],"source effect contributes only its own bonuses");
        apply(MakeUnequipCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),5},state.fence,state.storages,state.items,armorId,{ItemStorageKind::Inventory,1,0},{255,0,0,issuedAt+3000}).value());output("armor_off",0);
        apply(MakeEquipCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),6},state.fence,state.storages,state.items,armorId,{255,0,0,issuedAt+4000}).value());
        apply(MakeUnequipCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),7},state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0},{255,0,0,issuedAt+5000}).value());output("unequipped",0);
        apply(MakeEquipCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),8},state.fence,state.storages,state.items,id,{255,0,0,issuedAt+6000}).value());Check(state.items[0].costume==old.costume,"re-equip never refills the lifetime");output("reequipped",row.bodyEffect);
        if(n){apply(MakeCostumeClockCommit(*catalog,{0xF4,static_cast<std::uint8_t>(n),9},state.fence,state.storages,state.items,id,old.costume->deadlineUtcMs,0).value());Check(state.items.size()==1 && state.items[0].id==armorId,"expired effect removed while real armor remains");output("expired",0);}
    }
    store.Stop();std::cout<<"SOURCE_BODY_EFFECT_WORKER=SUCCESS variants=3 replay=reused relog=retained checks="<<checks<<"\n";
}

void WeaponEffectCatalogWorker(const GameSqlConfig& player,const std::shared_ptr<const auriga::item::ItemCatalog>& catalog,
    const std::string& password,const std::string& packetPath,const std::array<std::uint32_t,2>& effects,std::span<const auriga::item::ItemGrant> weapons,const std::array<std::uint32_t,2>& boxes)
{
    using namespace auriga::item;
    CAsyncSQL importer;importer.SetTimeouts(5);importer.SetRetryConnectionErrors(false);
    Check(importer.Setup("127.0.0.1","root",password.c_str(),"player","utf8mb4",true,player.port),"weapon effect importer uses the owned sandbox");
    auto& store=runtime::ItemStore::Instance();Check(store.Start(player,catalog,"source-weapon-effect-worker").has_value(),"actual weapon effect worker starts");
    const auto issuedAt=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for(unsigned variant=0;variant<2;++variant)for(unsigned w=0;w<weapons.size();++w) {
        const unsigned n=variant*8+w;const ItemOwner owner{ItemOwnerKind::Character,7060u+n};
        const auto key=ItemKey::Create(4,effects[variant]).value();const auto& row=std::get<ItemCostumeRow>(catalog->Find(key)->category);
        const auto& weapon=weapons[w];const auto& wr=std::get<ItemWeaponRow>(catalog->Find(weapon.key)->category);
        Check(row.kind==CostumeKind::WeaponEffect && row.clock==(variant ? CostumeClock::SinceFirstUse : CostumeClock::Permanent),"actual permanent and timed converted weapon effects");
        const auto* rule=catalog->FindDefinitionRules(weapon.key,weapon.refinementLevel);Check(rule && rule->weaponEffectAlternate==(w>=6),"actual converted normal/alternate weapon policy");
        const auto* wp=catalog->FindUsePolicy(catalog->Find(weapon.key)->base.usePolicy);const auto* ep=catalog->FindUsePolicy(catalog->Find(key)->base.usePolicy);
        EquipContext who{255,0,0,issuedAt+1000};while(who.characterClass<4 && !(wp->allowedClasses&ep->allowedClasses&(1u<<who.characterClass)))++who.characterClass;
        while(who.sex<2 && !(wp->allowedSexes&ep->allowedSexes&(1u<<who.sex)))++who.sex;Check(who.characterClass<4 && who.sex<2,"source weapon and effect admit the same real character");
        const std::array layouts{ItemStorageLayout{{owner,ItemStorageKind::Inventory,1},5,9,4},ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},EquipmentSlotCount,1,1}};
        Check(persistence::ProvisionOwner(importer,*catalog,owner,layouts,{}).has_value(),"fresh weapon effect owner provisioned");
        auto state=persistence::LoadCurrentItemOwner(importer,*catalog,owner).value();std::optional<runtime::CommitOutcome> got;
        const auto apply=[&](const ItemCommit& commit){got.reset();Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"effect action queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"action persisted and reloaded");state=*got->state;};
        const auto id=ItemInstanceId::Create(0xF500000000000000ULL+owner.id*2).value(),wid=ItemInstanceId::Create(id.Value()+1).value();const std::array ids{id},wids{wid};
        const auto expected=row.weaponEffects[w];
        const auto output=[&](const std::string& phase,std::uint16_t selected) {
            Check(WornWeaponCostumeEffect(*catalog,state.items)==selected,"server selects the exact effect from saved weapon state");
            const auto raw=[](const std::vector<std::byte>& bytes){std::vector<std::uint8_t> out;for(auto b:bytes)out.push_back(std::to_integer<std::uint8_t>(b));return out;};
            const auto write=[&](std::span<const std::uint8_t> bytes,const char* suffix){std::ofstream file(packetPath+"."+std::to_string(n)+"."+phase+suffix);for(auto v:bytes)file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];file<<'\n';Check(file.good(),"actual worker packet exported");};
            const auto inventory=EncodeTypedInventory(*catalog,state.storages,state.wallets,state.items).value();Check(DecodeTypedInventory(inventory,owner).has_value(),"v21 inventory decodes natively");write(raw(inventory),".inventory.hex");write(raw(EncodeTypedItemDetails(*catalog,state.items).value()),".details.hex");
            CostumeAppearance appearance{77,state.fence.revision};appearance.weaponEffect=selected;
            for(const auto& item:state.items)if(!item.destroyed && item.key==weapon.key && item.placement && item.placement->kind==ItemStorageKind::Equipment) {
                appearance.weapon=static_cast<std::uint8_t>(wr.subtype);if(const auto profile=catalog->Find(weapon.key)->base.appearanceProfile)appearance.models[0]=catalog->FindAppearance(*profile)->modelAsset;
            }
            write(EncodeCostumeAppearance(appearance).value(),".appearance.hex");
        };
        ItemGrant spec{key,0,1};spec.createdAtUtcMs=issuedAt;apply(MakeGrantCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),1},state.fence,state.storages,state.items,spec,ids).value());
        apply(MakeGrantCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),2},state.fence,state.storages,state.items,weapon,wids).value());output("bag",0);
        const auto equip=MakeEquipCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),3},state.fence,state.storages,state.items,id,who).value();apply(equip);output("unarmed",0);
        const auto old=state.items[0];apply(equip);Check(got->write->replay && state.items[0]==old,"replayed equip does not refill lifetime");
        apply(MakeEquipCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),4},state.fence,state.storages,state.items,wid,who).value());
        const auto session=persistence::ClaimOwnerSession(importer,*catalog,owner).value();state=persistence::LoadItemOwner(importer,*catalog,owner,session).value();Check(state.items[0]==old,"relog preserves the same effect instance and clock");output("worn",expected);
        auto without=state.items;without[0].placement=ItemPlacement{ItemStorageKind::Inventory,1,0};const auto a=BuildEquipmentStats(*catalog,owner,state.items).value(),b=BuildEquipmentStats(*catalog,owner,without).value();
        std::array<std::int64_t,MaximumBonusId+1> bonuses{};if(const auto profile=catalog->Find(key)->base.baseBonusProfile)for(const auto& entry:catalog->FindBonus(*profile)->Data().entries)bonuses[static_cast<unsigned>(entry.bonus)]+=entry.valueScaled;
        for(unsigned index=0;index<bonuses.size();++index)Check(a.bonuses[index]-b.bonuses[index]==bonuses[index],"effect contributes its own source bonuses once");
        apply(MakeUnequipCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),5},state.fence,state.storages,state.items,wid,{ItemStorageKind::Inventory,1,0},who).value());output("weapon_off",0);
        apply(MakeEquipCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),6},state.fence,state.storages,state.items,wid,who).value());
        apply(MakeUnequipCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),7},state.fence,state.storages,state.items,id,{ItemStorageKind::Inventory,1,0},who).value());output("unequipped",0);
        apply(MakeEquipCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),8},state.fence,state.storages,state.items,id,who).value());Check(state.items[0].costume==old.costume,"re-equip never extends deadline");output("reequipped",expected);
        if(variant){apply(MakeCostumeClockCommit(*catalog,{0xF5,static_cast<std::uint8_t>(n),9},state.fence,state.storages,state.items,id,old.costume->deadlineUtcMs,0).value());Check(state.items.size()==1 && state.items[0].id==wid,"expiry removes effect and leaves the real weapon");output("expired",0);}
    }
    for(unsigned n=0;n<boxes.size();++n) {
        const ItemOwner owner{ItemOwnerKind::Character,7080u+n};const ItemStorageLayout bag{{owner,ItemStorageKind::Inventory,1},5,9,4};
        Check(persistence::ProvisionOwner(importer,*catalog,owner,std::span(&bag,1),{}).has_value(),"new source chest owner provisioned");auto state=persistence::LoadCurrentItemOwner(importer,*catalog,owner).value();
        const auto apply=[&](const ItemCommit& commit){std::optional<runtime::CommitOutcome> got;Check(runtime::RequestCommit(owner,state.fence.sessionGeneration,commit,[&](auto out){got=std::move(out);}),"source chest action queued");for(unsigned spin=0;!got && spin<1000;++spin){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}Check(got && got->write && got->state,"source chest action saved and reloaded");state=*got->state;};
        const auto key=ItemKey::Create(8,boxes[n]).value();Check(ValidateContainerContents(*catalog,key).has_value(),"newly resolved source chest is fully openable");
        const auto ids=runtime::PeekInstanceIds(2);runtime::SpendInstanceIds(2);Check(ids.size()==2,"chest and award IDs reserved");
        apply(MakeGrantCommit(*catalog,{0xF6,static_cast<std::uint8_t>(n),1},state.fence,state.storages,state.items,ItemGrant{key,0,1},std::span(ids).first(1)).value());const auto before=state.items.front();
        const auto plan=MakeContainerOpeningCommit(*catalog,{0xF6,static_cast<std::uint8_t>(n),2},state.fence,state.storages,state.items,state.wallets,{ids[0],{}, {255,0,0,false},issuedAt},std::span(ids).subspan(1),[](auto){return 0;}).value();apply(plan.commit);
        Check(state.items.size()==1 && state.items[0].id==ids[1] && state.items[0].key!=key,"source box consumed and actual typed reward retained");
        const auto wire=EncodeContainerOpening(*catalog,123,&before,nullptr,state.fence.revision,ContainerOpeningStatus::Opened,{},plan.won).value();std::ofstream file(packetPath+".box."+std::to_string(n)+".hex");for(auto b:wire){const auto v=std::to_integer<unsigned>(b);file<<"0123456789abcdef"[v>>4]<<"0123456789abcdef"[v&15];}file<<'\n';
    }
    store.Stop();std::cout<<"SOURCE_WEAPON_EFFECT_WORKER=SUCCESS variants=2 weapon_cases=8 containers=2 replay=reused relog=retained checks="<<checks<<"\n";
}

#include "SourceServiceWorker.hpp"
#include "SourceBulkWorker.hpp"
#include "SourceWearableWorker.hpp"
#include "SourceProgressionWorker.hpp"
#include "SourceContextWorker.hpp"
#include "SourceStoleWorker.hpp"
#include "SourceRecoveredToolWorker.hpp"
#include "SourceBoundBookWorker.hpp"
#include "SourceCharacterServiceWorker.hpp"
#include "SourceSocketBirthWorker.hpp"
#include "SourceUniqueClockWorker.hpp"
#include "SourceAddonBirthWorker.hpp"
#include "SourceMagicBirthWorker.hpp"
#include "SourceUtilityWorker.hpp"
#include "SourceWorldRewardWorker.hpp"
#include "SourceAbsorptionSashWorker.hpp"
#include "SourceTimedUtilityWorker.hpp"
#include "SourceReusableRareWorker.hpp"
#include "SourceHairDyeWorker.hpp"
#include "SourceMultiEffectWorker.hpp"
#include "SourcePolymorphBookWorker.hpp"
#include "SourceReusableUtilityWorker.hpp"
#include "SourceDungeonKeyWorker.hpp"
#include "SourceCostumeTransferWorker.hpp"
#include "SourceEnergyCrystalWorker.hpp"
#include "SourceNativeMoneyWorker.hpp"
#include "SourcePrivateShopWorker.hpp"
#include "SourceDungeonEntryWorker.hpp"
#include "SourceDungeonUseWorker.hpp"
#include "SourceStatefulUtilityWorker.hpp"
#include "SourceOriginalPetBookWorker.hpp"
int main(int argc, char** argv)
{
	try
	{
		if (argc != 2 || (std::string(argv[1]) != "--owned-sandbox" && std::string(argv[1]) != "--tool-catalog" && std::string(argv[1]) != "--skin-catalog" && std::string(argv[1]) != "--body-effect-catalog" && std::string(argv[1]) != "--weapon-effect-catalog" && std::string(argv[1]) != "--services-catalog" && std::string(argv[1]) != "--bulk-catalog" && std::string(argv[1]) != "--wearables-catalog" && std::string(argv[1]) != "--progression-catalog" && std::string(argv[1]) != "--context-catalog" && std::string(argv[1]) != "--stole-catalog" && std::string(argv[1]) != "--recovered-tools-catalog" && std::string(argv[1]) != "--bound-books-catalog" && std::string(argv[1]) != "--character-services-catalog" && std::string(argv[1]) != "--socket-birth-catalog" && std::string(argv[1]) != "--unique-clock-catalog" && std::string(argv[1]) != "--addon-birth-catalog" && std::string(argv[1]) != "--magic-birth-catalog" && std::string(argv[1]) != "--utility-catalog" && std::string(argv[1]) != "--world-reward-catalog" && std::string(argv[1]) != "--absorption-sash-catalog" && std::string(argv[1]) != "--timed-utility-catalog" && std::string(argv[1]) != "--reusable-rare-catalog" && std::string(argv[1]) != "--hair-dye-catalog" && std::string(argv[1]) != "--multi-effect-catalog" && std::string(argv[1]) != "--polymorph-book-catalog" && std::string(argv[1]) != "--costume-transfer-catalog" && std::string(argv[1]) != "--dungeon-use-catalog" && std::string(argv[1]) != "--dungeon-entry-catalog" && std::string(argv[1]) != "--private-shops-catalog" && std::string(argv[1]) != "--native-money-catalog" && std::string(argv[1]) != "--energy-crystal-catalog" && std::string(argv[1]) != "--dungeon-key-catalog" && std::string(argv[1]) != "--original-pet-book-catalog" && std::string(argv[1]) != "--stateful-utility-catalog" && std::string(argv[1]) != "--reusable-utility-catalog")) throw std::runtime_error("run through ItemCatalogRuntimeTests.py");
		std::string port, password, items, hash, proxyPort, runtimePacketPath;
		std::getline(std::cin, port); std::getline(std::cin, password); std::getline(std::cin, items); std::getline(std::cin, hash);
		std::getline(std::cin, proxyPort);
        std::getline(std::cin,runtimePacketPath);
		GameSqlConfig player { "127.0.0.1", std::stoi(port), "game_reader", password, "player" };

		auto off = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Off, 1, {} }, player);
		Check(!off && off.error().find("retired") != std::string::npos && !runtime::CurrentCatalog(), "retired backend was accepted");

		auto loaded = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, hash }, player);
		Check(loaded.has_value() && loaded->has_value(), "required catalog did not load: " + (loaded ? std::string() : loaded.error()));
		Check((*loaded)->items == std::stoull(items) && (*loaded)->sha256 == hash && (*loaded)->revision == 1, "summary differs from the catalog");
		const auto catalog = runtime::CurrentCatalog();
		Check(catalog && catalog->Entries().size() == std::stoull(items), "loaded catalog is not published");

		auto pinned = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, std::string(64, '0') }, player);
		Check(!pinned && pinned.error().find("HashMismatch") != std::string::npos, "a different release pin was accepted");
		Check(runtime::CurrentCatalog() == catalog, "a failed load replaced the published catalog");

		auto denied = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, {} }, GameSqlConfig { "127.0.0.1", std::stoi(port), "game_reader", "wrong", "player" });
		Check(!denied && denied.error().find("cannot connect") != std::string::npos && denied.error().find("wrong") == std::string::npos,
			"a wrong password connected or leaked into the error");

		auto bad = runtime::LoadCatalogAtBoot({ runtime::CatalogMode::Required, 1, "xyz" }, player);
		Check(!bad, "an invalid pin was accepted");

        if(std::string(argv[1])=="--dungeon-use-catalog" || std::string(argv[1])=="--dungeon-entry-catalog" || std::string(argv[1])=="--private-shops-catalog" || std::string(argv[1])=="--native-money-catalog" || std::string(argv[1])=="--energy-crystal-catalog" || std::string(argv[1])=="--costume-transfer-catalog" || std::string(argv[1])=="--dungeon-key-catalog" || std::string(argv[1])=="--original-pet-book-catalog" || std::string(argv[1])=="--stateful-utility-catalog" || std::string(argv[1])=="--bulk-catalog" || std::string(argv[1])=="--wearables-catalog" || std::string(argv[1])=="--progression-catalog" || std::string(argv[1])=="--context-catalog" || std::string(argv[1])=="--stole-catalog" || std::string(argv[1])=="--recovered-tools-catalog" || std::string(argv[1])=="--bound-books-catalog" || std::string(argv[1])=="--character-services-catalog" || std::string(argv[1])=="--socket-birth-catalog" || std::string(argv[1])=="--unique-clock-catalog" || std::string(argv[1])=="--addon-birth-catalog" || std::string(argv[1])=="--magic-birth-catalog" || std::string(argv[1])=="--utility-catalog" || std::string(argv[1])=="--world-reward-catalog" || std::string(argv[1])=="--absorption-sash-catalog" || std::string(argv[1])=="--timed-utility-catalog" || std::string(argv[1])=="--reusable-rare-catalog" || std::string(argv[1])=="--hair-dye-catalog" || std::string(argv[1])=="--multi-effect-catalog" || std::string(argv[1])=="--polymorph-book-catalog" || std::string(argv[1])=="--reusable-utility-catalog") {
            using namespace auriga::item;std::string count;std::getline(std::cin,count);std::vector<BulkSource> sources;
            const auto readValue=[&](){std::string line;std::getline(std::cin,line);return std::stoll(line);};
            for(unsigned n=0;n<std::stoul(count);++n){auto category=readValue(),local=readValue(),level=readValue(),bonuses=readValue();BulkSource row{{ItemKey::Create(category,local).value(),static_cast<std::uint16_t>(level),1},{}};for(unsigned j=0;j<bonuses;++j){auto bonus=readValue(),value=readValue();row.bonuses.push_back({static_cast<BonusId>(bonus),value});}sources.push_back(std::move(row));}
            if(std::string(argv[1])=="--dungeon-use-catalog"){DungeonUseCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--dungeon-entry-catalog"){DungeonEntryCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--private-shops-catalog"){PrivateShopCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--native-money-catalog"){NativeMoneyCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--energy-crystal-catalog"){EnergyCrystalCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--costume-transfer-catalog"){CostumeTransferCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--dungeon-key-catalog"){DungeonKeyCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--original-pet-book-catalog"){OriginalPetBookCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--stateful-utility-catalog"){StatefulUtilityCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--reusable-utility-catalog"){ReusableUtilityCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--polymorph-book-catalog"){PolymorphBookCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--multi-effect-catalog"){MultiEffectCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--hair-dye-catalog"){HairDyeCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--reusable-rare-catalog"){ReusableRareCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--timed-utility-catalog"){TimedUtilityCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--absorption-sash-catalog"){const auto readKey=[&](){auto c=readValue(),l=readValue();return ItemKey::Create(c,l).value();};const auto clear=readKey(),stole=readKey();std::array<ItemGrant,2> donors{{{stole},{stole}}};for(auto& d:donors){d.key=readKey();d.refinementLevel=readValue();}AbsorptionSashCatalogWorker(player,catalog,password,runtimePacketPath,sources,clear,stole,donors);}else if(std::string(argv[1])=="--world-reward-catalog"){std::vector<ContainerWorldReward> expected;for(unsigned n=0;n<sources.size();++n){const auto kind=readValue(),definition=readValue();expected.push_back({static_cast<LootRewardKind>(kind),static_cast<std::uint32_t>(definition)});}WorldRewardCatalogWorker(player,catalog,password,runtimePacketPath,sources,expected);}else if(std::string(argv[1])=="--utility-catalog"){std::vector<std::array<unsigned,4>> expected;for(unsigned n=0;n<sources.size();++n){std::array<unsigned,4> value{};for(auto& v:value)v=static_cast<unsigned>(readValue());expected.push_back(value);}UtilityCatalogWorker(player,catalog,password,runtimePacketPath,sources,expected);}else if(std::string(argv[1])=="--magic-birth-catalog"){std::vector<std::array<std::vector<BonusEntry>,3>> expected;for(unsigned n=0;n<sources.size();++n){std::array<std::vector<BonusEntry>,3> row;for(auto& policy:row){const auto count=readValue();for(unsigned i=0;i<count;++i){auto bonus=readValue(),value=readValue();policy.push_back({static_cast<BonusId>(bonus),value});}}expected.push_back(std::move(row));}std::set<unsigned> samples;auto count=readValue();for(unsigned n=0;n<count;++n)samples.insert(static_cast<unsigned>(readValue()));MagicBirthCatalogWorker(player,catalog,password,runtimePacketPath,sources,expected,samples);}else if(std::string(argv[1])=="--addon-birth-catalog"){AddonBirthCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--unique-clock-catalog"){auto cat=readValue(),loc=readValue();UniqueClockCatalogWorker(player,catalog,password,runtimePacketPath,sources,ItemKey::Create(cat,loc).value());}else if(std::string(argv[1])=="--socket-birth-catalog"){std::vector<unsigned> counts;for(unsigned n=0;n<sources.size();++n)counts.push_back(static_cast<unsigned>(readValue()));const auto readKey=[&](){auto category=readValue(),local=readValue();return ItemKey::Create(category,local).value();};const auto scroll=readKey(),first=readKey(),second=readKey();SocketBirthCatalogWorker(player,catalog,password,runtimePacketPath,sources,counts,scroll,{first,second});}else if(std::string(argv[1])=="--character-services-catalog"){CharacterServiceCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--bound-books-catalog"){BoundBookCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--recovered-tools-catalog"){RecoveredToolCatalogWorker(player,catalog,password,runtimePacketPath,sources);}else if(std::string(argv[1])=="--stole-catalog"){auto category=readValue(),local=readValue();const auto permanent=ItemKey::Create(category,local).value();category=readValue();local=readValue();StoleCatalogWorker(player,catalog,password,runtimePacketPath,sources,permanent,ItemKey::Create(category,local).value());}else if(std::string(argv[1])=="--context-catalog"){const auto readKey=[&](){auto category=readValue(),local=readValue();return ItemKey::Create(category,local).value();};auto costume=readKey();auto level=readValue();auto seed=readKey(),donor=readKey(),book=readKey();ContextCatalogWorker(player,catalog,password,runtimePacketPath,sources,{costume,static_cast<std::uint16_t>(level),1},seed,donor,book);}else if(std::string(argv[1])=="--progression-catalog"){auto category=readValue(),local=readValue();const auto scroll=ItemKey::Create(category,local).value();const auto readGrant=[&](){auto cat=readValue(),loc=readValue(),level=readValue();return ItemGrant{ItemKey::Create(cat,loc).value(),static_cast<std::uint16_t>(level),1};};auto costume=readGrant(),weapon=readGrant();ProgressionCatalogWorker(player,catalog,password,runtimePacketPath,sources,scroll,costume,weapon);}else if(std::string(argv[1])=="--wearables-catalog"){auto category=readValue(),local=readValue(),level=readValue();ItemGrant target{ItemKey::Create(category,local).value(),static_cast<std::uint16_t>(level),1};std::array<ItemKey,5> tools{target.key,target.key,target.key,target.key,target.key};for(auto& key:tools){category=readValue();local=readValue();key=ItemKey::Create(category,local).value();}WearableCatalogWorker(player,catalog,password,runtimePacketPath,sources,target,tools);}else BulkCatalogWorker(player,catalog,password,runtimePacketPath,sources);return 0;
        }
        if(std::string(argv[1])=="--services-catalog") {
            std::string count;std::getline(std::cin,count);std::vector<auriga::item::ItemKey> keys;
            for(unsigned n=0;n<std::stoul(count);++n){std::string category,local;std::getline(std::cin,category);std::getline(std::cin,local);keys.push_back(auriga::item::ItemKey::Create(std::stoul(category),std::stoul(local)).value());}
            ServiceCatalogWorker(player,catalog,password,runtimePacketPath,keys);return 0;
        }
        if(std::string(argv[1])=="--weapon-effect-catalog") {
            std::array<std::uint32_t,2> effects{};for(auto& key:effects){std::string line;std::getline(std::cin,line);key=std::stoul(line);}
            std::vector<auriga::item::ItemGrant> weapons;for(unsigned n=0;n<8;++n){std::string local,level;std::getline(std::cin,local);std::getline(std::cin,level);weapons.push_back({auriga::item::ItemKey::Create(1,std::stoul(local)).value(),static_cast<std::uint16_t>(std::stoul(level)),1});}
            std::array<std::uint32_t,2> boxes{};for(auto& key:boxes){std::string local;std::getline(std::cin,local);key=std::stoul(local);}
            WeaponEffectCatalogWorker(player,catalog,password,runtimePacketPath,effects,weapons,boxes);return 0;
        }
        if(std::string(argv[1])=="--body-effect-catalog") {
            std::array<std::uint32_t,3> keys{};for(auto& key:keys){std::string line;std::getline(std::cin,line);key=std::stoul(line);}std::string armor;std::getline(std::cin,armor);
            BodyEffectCatalogWorker(player,catalog,password,runtimePacketPath,keys,std::stoul(armor));return 0;
        }
        if(std::string(argv[1])=="--skin-catalog") {
            std::array<std::uint32_t,4> keys{};for(auto& key:keys){std::string line;std::getline(std::cin,line);key=std::stoul(line);}
            SkinCatalogWorker(player,catalog,password,runtimePacketPath,keys);return 0;
        }
        if(std::string(argv[1])=="--tool-catalog")
        {
            std::string source,next,failure;std::getline(std::cin,source);std::getline(std::cin,next);std::getline(std::cin,failure);
            ToolCatalogWorker(player,catalog,password,runtimePacketPath,std::stoul(source),std::stoul(next),std::stoul(failure));return 0;
        }
        LiveDropWorker(player,catalog,password);
		// Item store: own thread and connection; login claim + load; ordered
		// completions applied on the calling (game) thread only; clean stop.
		{
			using namespace auriga::item;
			auto& store = runtime::ItemStore::Instance();
			Check(!store.Running() && !store.Submit([](CAsyncSQL&, const ItemCatalog&) { return runtime::ItemStore::Completion {}; }), "store accepts work before start");
			CAsyncSQL importer; importer.SetTimeouts(5); importer.SetRetryConnectionErrors(false);
			Check(importer.Setup("127.0.0.1", "root", password.c_str(), "player", "utf8mb4", true, std::stoi(port)), "importer connection");
			const ItemOwner provisioned { ItemOwnerKind::Character, 7001 };
			const ItemStorageLayout bag { { provisioned, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
			const ItemWallet gold { provisioned, WalletCurrency::Gold, 1, 777 };
			auto made = persistence::ProvisionOwner(importer, *catalog, provisioned, std::span(&bag, 1), std::span(&gold, 1));
			Check(made && *made, "owner provisioned for the store test");
            Check(persistence::ProvisionGroundStorage(importer,*catalog,provisioned).has_value(), "importer pre-provisions the ground escrow");
			Check(!store.Start(GameSqlConfig { "127.0.0.1", std::stoi(port), "game_reader", "wrong", "player" }, catalog), "store started with a wrong password");
			Check(!store.Running(), "failed start left the store running");
			Check(store.Start(player, catalog, "runtime-test-core").has_value() && store.Running(), "store did not start");
            Check(store.GroundRun() && store.GroundRun()->run==1, "store starts a fenced owned-ground run");
			Check(!store.Start(player, catalog), "store started twice");
			const auto gameThread = std::this_thread::get_id();
			std::vector<std::string> order;
			auto record = [&](std::string tag) { return [&order, tag, gameThread](runtime::LoginLoad result) {
				if (std::this_thread::get_id() != gameThread) throw std::runtime_error("completion ran off the game thread");
				order.push_back(tag + ":" + (result.loaded ? "loaded" + std::to_string(result.session) + "/" + std::to_string(result.loaded->wallets.at(0).balance)
				                                           : "error" + std::to_string(static_cast<int>(result.loaded.error()))));
			}; };
			Check(runtime::RequestLoginLoad(7001, record("first")) && runtime::RequestLoginLoad(7002, record("missing")) &&
			      runtime::RequestLoginLoad(7001, record("relog")), "login loads not queued");
			for (int spins = 0; order.size() < 3 && spins < 1000; ++spins)
			{
				store.Drain();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			const auto missing = "missing:error" + std::to_string(static_cast<int>(ItemPersistenceError::MissingOwner));
			Check(order == std::vector<std::string> { "first:loaded2/777", missing, "relog:loaded3/777" }, "login loads out of order or wrong");
			// C3: a move through the store is committed and answered with the reloaded state;
			// a move built from an outdated state is refused and the reload stays authoritative.
			{
				auto owned = persistence::LoadItemOwner(importer, *catalog, provisioned, 3);
				Check(owned.has_value(), "relogged owner loads at session 3");
				PersistentItem sword { ItemInstanceId::Create(0x00C3000000000001ULL).value(), ItemKey::Create(1, 1).value(), provisioned };
				sword.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 0 };
				ItemCommit grant; grant.operationId.fill(0x31); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { bag };
				const auto issued=MakeGrantCommit(*catalog,grant.operationId,owned->fence,owned->storages,owned->items,ItemGrant{sword.key},std::array{sword.id}).value();sword.attributes=issued.items[0].after.attributes;
				grant.items.push_back({ std::nullopt, sword }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the move test");
				auto before = persistence::LoadItemOwner(importer, *catalog, provisioned, 3);
				Check(before && before->items.size() == 1, "granted sword loads");
				std::array<std::uint8_t, 16> op {}; op.fill(0x32);
				auto move = MakeMoveCommit(*catalog, op, before->fence, before->storages, before->items, sword.id, ItemPlacement { ItemStorageKind::Inventory, 1, 7 });
				Check(move.has_value(), "move commit built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(provisioned, 3, *move, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "move not queued");
				wait();
				Check(got && got->write && !got->write->replay && got->state && got->state->items.at(0).placement->anchor == 7 &&
				      got->state->items.at(0).revision == before->items.at(0).revision + 1, "move committed and reloaded");
				op.fill(0x33);
				auto stale = MakeMoveCommit(*catalog, op, before->fence, before->storages, before->items, sword.id, ItemPlacement { ItemStorageKind::Inventory, 1, 9 });
				got.reset();
				Check(stale && runtime::RequestCommit(provisioned, 3, *stale, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "stale move not queued");
				wait();
				Check(got && !got->write && got->state && got->state->items.at(0).placement->anchor == 7, "stale move refused, reload authoritative");
				got.reset();
				auto previousLogin = *move; previousLogin.operationId.fill(0x34); previousLogin.owners[0].sessionGeneration = 2;
				Check(runtime::RequestCommit(provisioned, 2, previousLogin, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "old-session move not queued");
				wait();
				Check(got && !got->write && !got->state, "a previous login's session can neither write nor reload");
			}
			// C4: equip and unequip through the store, each answered with the reloaded state.
			{
				const ItemOwner wearer { ItemOwnerKind::Character, 7003 };
				const ItemStorageLayout layouts[] { { { wearer, ItemStorageKind::Inventory, 1 }, 5, 9, 4 }, { { wearer, ItemStorageKind::Equipment, 1 }, EquipmentSlotCount, 1, 1 } };
				const ItemWallet purse { wearer, WalletCurrency::Gold, 1, 0 };
				auto made = persistence::ProvisionOwner(importer, *catalog, wearer, layouts, std::span(&purse, 1));
				Check(made && *made, "wearer provisioned with inventory and equipment");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, wearer);
				Check(session && *session == 2, "wearer session claimed");
				auto owned = persistence::LoadItemOwner(importer, *catalog, wearer, *session);
				PersistentItem blade { ItemInstanceId::Create(0x00C4000000000001ULL).value(), ItemKey::Create(1, 1).value(), wearer };
				blade.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 4 };
				ItemCommit grant; grant.operationId.fill(0x41); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { layouts[0] };
				const auto issued=MakeGrantCommit(*catalog,grant.operationId,owned->fence,owned->storages,owned->items,ItemGrant{blade.key},std::array{blade.id}).value();blade.attributes=issued.items[0].after.attributes;
				grant.items.push_back({ std::nullopt, blade }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the equip test");
				auto before = persistence::LoadItemOwner(importer, *catalog, wearer, *session);
				std::array<std::uint8_t, 16> op {}; op.fill(0x42);
				auto equip = MakeEquipCommit(*catalog, op, before->fence, before->storages, before->items, blade.id, EquipContext { 1, 0, 0 });
				Check(equip.has_value(), "equip commit built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(wearer, *session, *equip, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "equip not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.at(0).placement == ItemPlacement { ItemStorageKind::Equipment, 1, 0 }, "equipped and reloaded");
				op.fill(0x43);
				auto off = MakeUnequipCommit(*catalog, op, got->state->fence, got->state->storages, got->state->items, blade.id, ItemPlacement { ItemStorageKind::Inventory, 1, 11 });
				Check(off.has_value(), "unequip commit built");
				got.reset();
				Check(runtime::RequestCommit(wearer, *session, *off, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "unequip not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.at(0).placement == ItemPlacement { ItemStorageKind::Inventory, 1, 11 }, "taken off and reloaded");
			}
			// C5: a flask's units are spent through the store; the last one leaves no live item.
			{
				const ItemOwner drinker { ItemOwnerKind::Character, 7005 };
				const ItemStorageLayout pack { { drinker, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
				const ItemWallet purse { drinker, WalletCurrency::Gold, 1, 0 };
				auto made = persistence::ProvisionOwner(importer, *catalog, drinker, std::span(&pack, 1), std::span(&purse, 1));
				Check(made && *made, "drinker provisioned");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, drinker);
				Check(session.has_value(), "drinker session claimed");
				auto owned = persistence::LoadItemOwner(importer, *catalog, drinker, *session);
				PersistentItem flask { ItemInstanceId::Create(0x00C5000000000001ULL).value(), ItemKey::Create(5, 1).value(), drinker };
				flask.count = 2; flask.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 6 };
				ItemCommit grant; grant.operationId.fill(0x51); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { pack };
				grant.items.push_back({ std::nullopt, flask }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the consume test");
				auto before = persistence::LoadItemOwner(importer, *catalog, drinker, *session);
				Check(before && before->items.size() == 1 && before->items[0].count == 2, "granted flask loads");
				std::array<std::uint8_t, 16> op {}; op.fill(0x52);
				auto sip = MakeConsumeCommit(*catalog, op, before->fence, before->storages, before->items, flask.id, 1);
				Check(sip.has_value(), "consume commit built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(drinker, *session, *sip, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "consume not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.at(0).count == 1, "one unit spent and reloaded");
				op.fill(0x53);
				auto last = MakeConsumeCommit(*catalog, op, got->state->fence, got->state->storages, got->state->items, flask.id, 1);
				Check(last.has_value(), "last consume commit built");
				got.reset();
				Check(runtime::RequestCommit(drinker, *session, *last, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "last consume not queued");
				wait();
				Check(got && got->write && got->state && std::ranges::none_of(got->state->items, [](const PersistentItem& i) { return !i.destroyed; }),
				      "the last unit left no live flask");
			}
			// C6: refinement through the store. +0 -> +1 spends the gold and the
			// materials; the certain failure +1 -> +2 destroys the sword and still
			// costs its gold. Each answer is the reloaded state.
			{
				const ItemOwner customer { ItemOwnerKind::Character, 7006 };
				const ItemStorageLayout pack { { customer, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
				const ItemWallet purse { customer, WalletCurrency::Gold, 1, 1000 };
				auto made = persistence::ProvisionOwner(importer, *catalog, customer, std::span(&pack, 1), std::span(&purse, 1));
				Check(made && *made, "smith customer provisioned");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, customer);
				Check(session.has_value(), "smith customer session claimed");
				auto owned = persistence::LoadItemOwner(importer, *catalog, customer, *session);
				PersistentItem blade { ItemInstanceId::Create(0x00C6000000000001ULL).value(), ItemKey::Create(1, 1).value(), customer };
				blade.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 0 };
				PersistentItem ore { ItemInstanceId::Create(0x00C6000000000002ULL).value(), ItemKey::Create(6, 1).value(), customer };
				ore.count = 3; ore.placement = ItemPlacement { ItemStorageKind::Inventory, 1, 20 };
				ItemCommit grant; grant.operationId.fill(0x61); grant.catalogRevision = catalog->Revision(); grant.catalogHash = catalog->ContentHash();
				grant.kind = ItemOperationKind::Grant; grant.owners = { owned->fence }; grant.storages = { pack };
				const auto issued=MakeGrantCommit(*catalog,grant.operationId,owned->fence,owned->storages,owned->items,ItemGrant{blade.key},std::array{blade.id}).value();blade.attributes=issued.items[0].after.attributes;
				grant.items.push_back({ std::nullopt, blade }); grant.items.push_back({ std::nullopt, ore }); grant.outcomeCode = 1; grant.result = { 1 };
				Check(persistence::CommitItems(importer, *catalog, grant).has_value(), "grant for the refine test");
				auto before = persistence::LoadItemOwner(importer, *catalog, customer, *session);
				Check(before && before->items.size() == 2, "granted sword and ore load");
				const auto first = catalog->RefinementRecipesFrom(blade.key, 0);
				Check(first.size() == 1, "one recipe from +0");
				std::array<std::uint8_t, 16> op {}; op.fill(0x62);
				auto up = MakeOwnerRefinement(*catalog, before->fence, before->storages, before->items, before->wallets, { op, blade.id, first[0]->id, 0, true });
				Check(up && up->plan.success, "refinement built");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(customer, *session, up->commit, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "refine not queued");
				wait();
				const auto live = [&](ItemInstanceId id) -> const PersistentItem* {
					const auto it = std::ranges::find_if(got->state->items, [&](const PersistentItem& i) { return i.id == id && !i.destroyed; });
					return it == got->state->items.end() ? nullptr : &*it;
				};
				Check(got && got->write && got->state && live(blade.id) && live(blade.id)->refinementLevel == 1 && live(ore.id) && live(ore.id)->count == 1 &&
				      got->state->wallets.at(0).balance == 900, "+1 committed: gold and materials spent, reloaded");
				const auto second = catalog->RefinementRecipesFrom(blade.key, 1);
				Check(second.size() == 1, "one recipe from +1");
				op.fill(0x63);
				auto lost = MakeOwnerRefinement(*catalog, got->state->fence, got->state->storages, got->state->items, got->state->wallets, { op, blade.id, second[0]->id, 0, true });
				Check(lost && !lost->plan.success, "certain failure built");
				got.reset();
				Check(runtime::RequestCommit(customer, *session, lost->commit, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "failed refine not queued");
				wait();
				Check(got && got->write && got->state && !live(blade.id) && live(ore.id) && got->state->wallets.at(0).balance == 850,
				      "the failure destroyed the sword and still cost its gold");
			}
			// C7a: the store reserved a block of instance IDs at start; a grant spends
			// them, and a low pool is refilled in the background with a disjoint block.
			{
				Check(store.AvailableIds() == runtime::ItemStore::IdBlockSize, "a full ID block reserved at start");
				const auto peeked = runtime::PeekInstanceIds(2);
				Check(peeked.size() == 2 && peeked[0].Value() >= (std::uint64_t { 1 } << 32) && peeked[1].Value() == peeked[0].Value() + 1,
				      "the pool hands out reserved IDs above the legacy ID space");
				const ItemOwner receiver { ItemOwnerKind::Character, 7007 };
				const ItemStorageLayout pack { { receiver, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
				const ItemWallet purse { receiver, WalletCurrency::Gold, 1, 0 };
				auto made = persistence::ProvisionOwner(importer, *catalog, receiver, std::span(&pack, 1), std::span(&purse, 1));
				Check(made && *made, "receiver provisioned");
				const auto session = persistence::ClaimOwnerSession(importer, *catalog, receiver);
				Check(session.has_value(), "receiver session claimed");
				auto before = persistence::LoadItemOwner(importer, *catalog, receiver, *session);
				std::array<std::uint8_t, 16> op {}; op.fill(0x71);
				auto grant = MakeGrantCommit(*catalog, op, before->fence, before->storages, before->items, { ItemKey::Create(6, 1).value(), 0, 250 }, peeked);
				Check(grant && grant->items.size() == 2 && grant->items[0].after.count == 200 && grant->items[1].after.count == 50, "250 ore in two stacks");
				runtime::SpendInstanceIds(2);
				Check(store.AvailableIds() == runtime::ItemStore::IdBlockSize - 2, "the grant's IDs are spent");
				std::optional<runtime::CommitOutcome> got;
				const auto wait = [&] { for (int spins = 0; !got && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); } };
				Check(runtime::RequestCommit(receiver, *session, *grant, [&](runtime::CommitOutcome outcome) { got = std::move(outcome); }), "grant not queued");
				wait();
				Check(got && got->write && got->state && got->state->items.size() == 2 &&
				      std::ranges::all_of(got->state->items, [&](const PersistentItem& i) { return i.id == peeked[0] || i.id == peeked[1]; }),
				      "granted with the reserved IDs and reloaded");
				const auto last = peeked[1].Value();
				runtime::SpendInstanceIds(store.AvailableIds() - 10);
				for (int spins = 0; store.AvailableIds() <= 10 && spins < 1000; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
				Check(store.AvailableIds() == 10 + runtime::ItemStore::IdBlockSize, "a low pool is refilled with a whole block");
				const auto next = runtime::PeekInstanceIds(11);
				Check(next.size() == 11 && next[0].Value() > last && next[10].Value() >= next[9].Value() + 1, "the refill is a new block after the first");
			}
            // Real worker -> durable SQL -> GC233 -> Unity validation artifact.
            {
                const ItemOwner opener{ItemOwnerKind::Character,7009};
                const ItemStorageLayout pack{{opener,ItemStorageKind::Inventory,1},5,9,4};
                const ItemWallet gold{opener,WalletCurrency::Gold,1,1000};
                Check(persistence::ProvisionOwner(importer,*catalog,opener,std::span(&pack,1),std::span(&gold,1)).has_value(),"refined-container owner provisioned");
                auto state=persistence::LoadCurrentItemOwner(importer,*catalog,opener).value();
                const auto boxId=runtime::PeekInstanceIds(1)[0];runtime::SpendInstanceIds(1);
                std::array<std::uint8_t,16> op{};op[0]='L';op[15]=1;
                auto grant=MakeGrantCommit(*catalog,op,state.fence,state.storages,state.items,{ItemKey::Create(8,1).value(),0,1},std::array{boxId}).value();
                std::optional<runtime::CommitOutcome> got;
                const auto wait=[&]{for(int spins=0;!got && spins<1000;++spins){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
                Check(runtime::RequestCommit(opener,state.fence.sessionGeneration,grant,[&](auto outcome){got=std::move(outcome);}),"container grant queued");wait();
                Check(got && got->write && got->state,"container grant committed by game item worker");state=*got->state;
                const auto before=state.items.front();const auto awardId=runtime::PeekInstanceIds(1)[0];runtime::SpendInstanceIds(1);op[15]=2;
                auto opening=MakeContainerOpeningCommit(*catalog,op,state.fence,state.storages,state.items,state.wallets,{boxId,{}, {100,0,0,false}},std::array{awardId},[](auto){return 0;}).value();
                got.reset();Check(runtime::RequestCommit(opener,state.fence.sessionGeneration,opening.commit,[&](auto outcome){got=std::move(outcome);}),"refined container opening queued");wait();
                Check(got && got->write && got->state && got->state->items.size()==1 && got->state->items[0].id==awardId && got->state->items[0].refinementLevel==2,"game item worker consumes box and reloads exact +2 reward");
                got.reset();Check(runtime::RequestCommit(opener,state.fence.sessionGeneration,opening.commit,[&](auto outcome){got=std::move(outcome);}),"same opening replay queued");wait();
                Check(got && got->write && got->write->replay && got->state && got->state->items.size()==1,"same opening receipt cannot mint a second reward");
                const auto session=persistence::ClaimOwnerSession(importer,*catalog,opener).value();
                const auto reloaded=persistence::LoadItemOwner(importer,*catalog,opener,session).value();
                Check(reloaded.items.size()==1 && reloaded.items[0].refinementLevel==2,"reward refinement survives relog");
                const auto wire=EncodeContainerOpening(*catalog,123,&before,nullptr,reloaded.fence.revision,ContainerOpeningStatus::Opened,{},opening.won).value();
                std::string hex;for(auto b:wire){const auto v=std::to_integer<unsigned>(b);hex.push_back("0123456789abcdef"[v>>4]);hex.push_back("0123456789abcdef"[v&15]);}
                std::ofstream output(runtimePacketPath,std::ios::binary);output<<hex<<'\n';output.close();
                Check(output.good(),"worker result packet exported for Unity");
                std::cout<<"REFINED_CONTAINER_WORKER=SUCCESS level=2\n";
            }

            // The actual game item worker durably stores, replays and reloads
            // the complete paired-attribute operation before exposing its UI.
            {
                const ItemOwner smith{ItemOwnerKind::Character,7010};
                const ItemStorageLayout pack{{smith,ItemStorageKind::Inventory,1},5,9,4};
                Check(persistence::ProvisionOwner(importer,*catalog,smith,std::span(&pack,1),{}).has_value(),"paired weapon owner provisioned");
                auto state=persistence::LoadCurrentItemOwner(importer,*catalog,smith).value();
                PersistentItem weapon{ItemInstanceId::Create(0x00E0000000000001ULL).value(),ItemKey::Create(1,1).value(),smith};
                weapon.placement={ItemStorageKind::Inventory,1,0};

                PersistentItem switcher{ItemInstanceId::Create(0x00E0000000000002ULL).value(),ItemKey::Create(5,2).value(),smith};
                switcher.count=2;switcher.placement={ItemStorageKind::Inventory,1,1};
                ItemCommit grant;grant.operationId.fill(0xE0);grant.catalogRevision=catalog->Revision();grant.catalogHash=catalog->ContentHash();grant=MakeGrantCommit(*catalog,grant.operationId,state.fence,state.storages,state.items,ItemGrant{weapon.key},std::array{weapon.id}).value();weapon=grant.items[0].after;grant.items.push_back({std::nullopt,switcher});
                std::optional<runtime::CommitOutcome> got;
                const auto wait=[&]{for(int spins=0;!got && spins<1000;++spins){store.Drain();std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
                Check(runtime::RequestCommit(smith,state.fence.sessionGeneration,grant,[&](auto result){got=std::move(result);}),"paired fixtures queued");wait();
                Check(got && got->write && got->state,"paired fixtures persisted");state=*got->state;
                const SocketMaintenanceActor actor{100,1,1,false};
                const auto quote=PrepareAttributeRuntime(*catalog,state.fence,state.items,switcher.id,weapon.id,actor).value();
                std::array<std::uint8_t,16> op{};op.fill(0xE1);unsigned entropy=0;
                const auto reroll=MakeAttributeRuntimeCommit(*catalog,op,state.fence,state.storages,state.items,switcher.id,weapon.id,actor,quote,[&](auto){++entropy;return 1;}).value();
                Check(entropy==5 && reroll.commit.result[3]==2,"paired operation on actual newly born weapon recorded its decisions");
                got.reset();Check(runtime::RequestCommit(smith,state.fence.sessionGeneration,reroll.commit,[&](auto result){got=std::move(result);}),"paired operation queued");wait();
                Check(got && got->write && !got->write->replay && got->state,"paired operation committed");state=*got->state;
                const auto settledEntropy=entropy;got.reset();Check(runtime::RequestCommit(smith,state.fence.sessionGeneration,reroll.commit,[&](auto result){got=std::move(result);}),"paired operation retry queued");wait();
                Check(got && got->write && got->write->replay && got->state && entropy==settledEntropy,"paired retry reuses durable receipt and draws no entropy");
                const auto session=persistence::ClaimOwnerSession(importer,*catalog,smith).value();
                const auto relog=persistence::LoadItemOwner(importer,*catalog,smith,session).value();
                const auto sword=std::ranges::find(relog.items,weapon.id,&PersistentItem::id),tool=std::ranges::find(relog.items,switcher.id,&PersistentItem::id);
                Check(sword!=relog.items.end() && sword->attributes==reroll.after && tool!=relog.items.end() && tool->count==1,"paired signed attributes and single debit survive relog");
                const std::array targets{weapon.id.Value()};const auto wire=EncodeAttributeRuntime(*catalog,321,&switcher,&weapon,relog.fence.revision,AttributeRuntimeStatus::Applied,quote,&sword->attributes,targets).value();
                std::ofstream output(runtimePacketPath+".attribute.hex",std::ios::binary);for(auto b:wire){const auto value=std::to_integer<unsigned>(b);output<<"0123456789abcdef"[value>>4]<<"0123456789abcdef"[value&15];}output<<'\n';output.close();Check(output.good(),"paired worker result exported for actual Unity UI");
                std::cout<<"PAIRED_ATTRIBUTE_WORKER=SUCCESS replay=reused relog=retained entropy=11\n";
            }
            // Queued work is finished at stop; its completion is dropped, not run late.
			bool ran = false, applied = false;
			Check(store.Submit([&ran, &applied](CAsyncSQL&, const ItemCatalog&) { ran = true; return runtime::ItemStore::Completion { [&applied] { applied = true; } }; }), "work refused");
			store.Stop();
			Check(ran && !store.Running() && store.Drain() == 0 && !applied, "stop did not finish queued work or ran a late completion");
			Check(!store.Submit([](CAsyncSQL&, const ItemCatalog&) { return runtime::ItemStore::Completion {}; }), "store accepts work after stop");
			// Lose a real write acknowledgement and the first reconciliation
			// response. No refusal may escape while the result is unknown, and
			// the retry must read the same receipt without creating another item.
			const ItemOwner picker { ItemOwnerKind::Character, 7008 };
			const ItemStorageLayout pack { { picker, ItemStorageKind::Inventory, 1 }, 5, 9, 4 };
			Check(persistence::ProvisionOwner(importer, *catalog, picker, std::span(&pack, 1), {}).has_value(), "fault-test owner provisioned");
			auto owned = persistence::LoadItemOwner(importer, *catalog, picker, 1);
			Check(owned.has_value(), "fault-test owner loads");
			GameSqlConfig proxied = player; proxied.port = std::stoi(proxyPort);
			Check(store.Start(proxied, catalog).has_value(), "store starts through the fault proxy");
			const auto ids = runtime::PeekInstanceIds(1);
			std::array<std::uint8_t, 16> op {}; op.fill(0x81);
			auto grant = MakeGrantCommit(*catalog, op, owned->fence, owned->storages, owned->items, { ItemKey::Create(6, 1).value(), 0, 5 }, ids);
			Check(grant.has_value(), "fault-test grant built");
			runtime::SpendInstanceIds(1);
			std::optional<runtime::CommitOutcome> got;
			unsigned completions = 0;
			const auto started = std::chrono::steady_clock::now();
			Check(runtime::RequestCommit(picker, 1, *grant, [&](runtime::CommitOutcome outcome) { ++completions; got = std::move(outcome); }), "fault-test grant queued");
			for (int spins = 0; !got && spins < 1500; ++spins) { store.Drain(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
			Check(got && got->write && got->write->replay && got->state && got->state->items.size() == 1 &&
			      got->state->items[0].count == 5 && got->state->fence.revision == owned->fence.revision + 1 && completions == 1,
			      "unknown write is reconciled once, with one item and one fence increment");
			Check(std::chrono::steady_clock::now() - started >= std::chrono::seconds(1), "failed receipt read must wait before retrying");
			store.Stop();
		}

		std::cout << "ItemCatalogRuntimeTests: " << checks << " checks passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cout << "FAILED: " << error.what() << '\n';
		return 1;
	}
}
