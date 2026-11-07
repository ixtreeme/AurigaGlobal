#include "PersistentItemFixture.hpp"
#include "AttributeRuntimeFixture.hpp"
#include "EffectRuntimeFixture.hpp"
#include "BookRuntimeFixture.hpp"
#include "BookAidFixture.hpp"
#include "PolymorphRuntimeFixture.hpp"
#include "FishProcessingFixture.hpp"
#include "ToolRuntimeFixture.hpp"
#include <item/ResourceGathering.hpp>
#include <item/BookSaveFence.hpp>
#include "CatalogSnapshotFixture.hpp"
#include <ItemPersistenceSql.hpp>
#include <item/OwnedGround.hpp>
#include <item/NpcShops.hpp>
#include <item/PlayerTrades.hpp>
#include <item/Crafting.hpp>
#include <item/ContainerOpening.hpp>
#include <item/TokenRedemption.hpp>
#include <item/SocketMaintenance.hpp>
#include <item/SocketInsertion.hpp>
#include <item/AccessoryRuntime.hpp>
#include <item/AttributeRuntime.hpp>
#include <item/AutomaticRuntime.hpp>
#include <CatalogSqlReader.hpp>
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <future>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace auriga::item;
namespace f=persistent_item_fixture;
namespace p=auriga::item::persistence;
namespace s=auriga::item::sql;
using U=SqlParam;using Message=std::unique_ptr<SQLMsg>;using E=ItemPersistenceError;
namespace
{
unsigned checks=0;
void Check(bool good,const char* why,std::source_location at=std::source_location::current())
{++checks;if(!good)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));}
template<class Range> std::string Bytes(const Range& r){std::string out;for(auto b:r)out.push_back(static_cast<char>(b));return out;}
Message Query(CAsyncSQL& db,const std::string& query,const SqlParams& params={})
{
    Message m(db.DirectQueryPrepared(query.c_str(),params));
    if(m && m->uiSQLErrno)std::cerr<<"SQL fixture error="<<m->uiSQLErrno<<" query="<<query<<'\n';
    Check(m && !m->uiSQLErrno && m->Get(),"isolated query failed");return m;
}
std::string Scalar(CAsyncSQL& db,const std::string& query,const SqlParams& params={})
{auto m=Query(db,query,params);auto row=m->Get()->FetchRow();auto lengths=m->Get()->FetchLengths();Check(row && row[0] && lengths,"scalar missing");return {row[0],lengths[0]};}
void Execute(CAsyncSQL& db,const std::string& query)
{Message m(db.DirectQuery(query.c_str()));if(m && m->uiSQLErrno)std::cerr<<"SQL setup error="<<m->uiSQLErrno<<" query="<<query<<'\n';Check(m && !m->uiSQLErrno,"SQL setup failed");}
void Setup(CAsyncSQL& db,unsigned port,const std::string& pw,const char* user)
{db.SetTimeouts(5);db.SetRetryConnectionErrors(false);Check(db.Setup("127.0.0.1",user,pw.c_str(),"player","utf8mb4",true,port),"owned SQL connection");}
void Completion()
{
    const auto c=BuildItemCatalog(f::Catalog()).value();const auto grant=f::Grant(*c);
    auto prepared=p::PrepareItemWrite(*c,grant);Check(prepared.has_value(),"write plan created");
    Check(prepared->size()<SqlTransactionLimits::Steps,"bounded write plan");
    for(const auto outcome:{SqlTransactionOutcome::NotStarted,SqlTransactionOutcome::RolledBack,SqlTransactionOutcome::Conflict,SqlTransactionOutcome::Failed,SqlTransactionOutcome::CommitUnknown})
    {
        SQLMsg m;m.bTransaction=true;m.transaction=*prepared;m.transactionOutcome=outcome;
        auto r=p::ItemWriteCompleted(*c,grant,m);Check(!r,"unconfirmed item write published");
        if(outcome==SqlTransactionOutcome::CommitUnknown)Check(r.error()==E::CommitUnknown,"unknown commit must reconcile");
    }
    SQLMsg good;good.bTransaction=true;good.transaction=*prepared;good.transactionOutcome=SqlTransactionOutcome::Committed;
    Check(!p::ItemWriteCompleted(*c,grant,good),"missing completion results denied");
    for(const auto& step:*prepared)
    {
        auto result=new SQLResult;result->bPrepared=true;result->uiAffectedRows=step.expectedAffectedRows.value_or(0);result->uiNumRows=step.expectedResultRows.value_or(0);
        good.transactionResultOffsets.push_back(good.vec_pkResult.size());good.vec_pkResult.push_back(result);
    }
    Check(p::ItemWriteCompleted(*c,grant,good).has_value(),"confirmed matching owning completion");
    good.transaction.back().params[0].u64++;Check(!p::ItemWriteCompleted(*c,grant,good),"completion from another request");
    auto capped=grant;capped.result.resize(MaxItemCommitResult+1);Check(!p::PrepareItemWrite(*c,capped),"bounded result payload");
}
void Populate(CAsyncSQL& admin,const ItemCatalogSource& source)
{
    const auto snapshot=catalog_snapshot_fixture::Snapshot(source);
    struct Pending{std::string query;SqlParams params;};std::vector<Pending> pending;
    for(const auto& spec:s::Tables())
    {
        if(spec.table==s::Table::Format || spec.table==s::Table::Categories || spec.table==s::Table::BonusDefinitions)continue;
        for(const auto& row:snapshot[spec.table])
        {
            std::string marks;SqlParams params;for(const auto& cell:row){if(!marks.empty())marks+=",";marks+="?";params.push_back(cell ? U::Text(*cell) : U::Null());}
            pending.push_back({"INSERT INTO protos."+std::string(spec.name)+"("+std::string(spec.columns)+") VALUES("+marks+")",std::move(params)});
        }
    }
    // Fixture-only dependency ordering: defer FK-dependent rows, keep every FK
    // enabled. Neither production writer nor importer uses this test helper.
    while(!pending.empty())
    {
        const auto old=pending.size();std::vector<Pending> next;
        for(auto& row:pending){Message m(admin.DirectQueryPrepared(row.query.c_str(),row.params));Check(m!=nullptr,"fixture message");if(m->uiSQLErrno){Check(m->uiSQLErrno==1452,"fixture error other than missing parent");next.push_back(std::move(row));}}
        Check(next.size()<old,"fixture FK dependency cycle");pending=std::move(next);
    }
}
p::LoadedItemOwner Load(CAsyncSQL& db,const ItemCatalog& c,ItemOwner owner=f::Character(),std::uint64_t session=7)
{auto r=p::LoadItemOwner(db,c,owner,session);if(!r)std::cerr<<"load error="<<static_cast<unsigned>(r.error())<<'\n';Check(r.has_value(),"owner reload failed");return *r;}
PersistentItem Find(const p::LoadedItemOwner& o,std::uint64_t id)
{const auto it=std::ranges::find(o.items,id,[](const auto& i){return i.id.Value();});Check(it!=o.items.end(),"loaded item missing");return *it;}
ItemCommit Move(const ItemCatalog& c,const p::LoadedItemOwner& o,std::uint64_t id,std::uint32_t cell,unsigned op)
{
    auto r=f::Grant(c,{});r.operationId[0]=static_cast<std::uint8_t>(op);r.kind=ItemOperationKind::Move;r.owners={o.fence};
    auto before=Find(o,id),after=before;after.revision++;after.placement->anchor=cell;r.items={{before,after}};return r;
}
std::string Fingerprint(CAsyncSQL& db)
{
    std::string result;
    for(const auto* table:{"item_owner_fence","item_wallet","item_instance","item_instance_attribute","item_instance_socket","item_occupied_cell","item_operation_receipt"})
    {
        // Test-only whole-table byte fingerprint with every column and NULL.
        auto columns=Query(db,"SELECT COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME=? ORDER BY ORDINAL_POSITION",{U::Text(table)});
        std::string expr;while(auto row=columns->Get()->FetchRow()){if(!expr.empty())expr+=",";expr+="IFNULL(HEX(`"+std::string(row[0])+"`),'NULL')";}
        result+=Scalar(db,"SELECT SHA2(COALESCE(GROUP_CONCAT(CONCAT_WS(':',"+expr+") ORDER BY 1 SEPARATOR '|'),''),256) FROM player."+table);
    }
    return result;
}
// Login owner protocol: provisioning is explicit and privileged; every login
// claims a new session generation and the previous login's writes then fail.
void Sessions(CAsyncSQL& admin,CAsyncSQL& writer,const ItemCatalog& c)
{
    using E=ItemPersistenceError;
    const ItemOwner fresh{ItemOwnerKind::Character,4242},account{ItemOwnerKind::Account,4242};
    const ItemStorageLayout bag{{fresh,ItemStorageKind::Inventory,1},5,9,4},safe{{fresh,ItemStorageKind::Equipment,1},32,1,1};
    const ItemWallet gold{fresh,WalletCurrency::Gold,1,5000};
    auto claimed=p::ClaimOwnerSession(writer,c,fresh);Check(!claimed && claimed.error()==E::MissingOwner,"claim of an unprovisioned owner");
    auto bad=p::ProvisionOwner(admin,c,fresh,{},std::span(&gold,1));Check(!bad && bad.error()==E::InvalidStorage,"owner without storage");
    const ItemStorageLayout foreign{{account,ItemStorageKind::Inventory,1},5,9,4};
    bad=p::ProvisionOwner(admin,c,fresh,std::span(&foreign,1),{});Check(!bad && bad.error()==E::InvalidStorage,"storage of another owner");
    const ItemStorageLayout twice[]{bag,bag};bad=p::ProvisionOwner(admin,c,fresh,twice,{});Check(!bad && bad.error()==E::InvalidStorage,"duplicate storage");
    const ItemWallet coins{fresh,WalletCurrency::DragonCoin,1,1};bad=p::ProvisionOwner(admin,c,fresh,std::span(&bag,1),std::span(&coins,1));
    Check(!bad && bad.error()==E::InvalidWallet,"character dragon-coin wallet");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_owner_fence WHERE owner_id=4242")=="0","rejected provisioning wrote nothing");
    // The writer may create internal carrier storage but only advances existing
    // owner fences: it still cannot invent an owner or provision its balances.
    bad=p::ProvisionOwner(writer,c,fresh,std::span(&bag,1),std::span(&gold,1));Check(!bad,"item writer cannot provision");
    Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_owner_fence WHERE owner_id=4242")=="0","failed provisioning is atomic");
    const ItemStorageLayout layouts[]{bag,safe};
    auto made=p::ProvisionOwner(admin,c,fresh,layouts,std::span(&gold,1));
    Check(made && *made,"owner provisioned");
    const ItemWallet richer{fresh,WalletCurrency::Gold,1,999};
    made=p::ProvisionOwner(admin,c,fresh,std::span(&bag,1),std::span(&richer,1));Check(made && !*made,"second provisioning reports existing owner");
    Check(Scalar(admin,"SELECT CONCAT(COUNT(*),':',(SELECT balance FROM player.item_wallet WHERE owner_id=4242 AND owner_kind=1)) FROM player.item_storage WHERE owner_kind=1 AND owner_id=4242")=="2:5000",
          "existing owner left untouched");
    auto first=p::LoadItemOwner(writer,c,fresh,1);Check(first && first->items.empty() && first->storages.size()==2 && first->wallets.at(0).balance==5000,"provisioned owner loads at session 1");
    claimed=p::ClaimOwnerSession(writer,c,fresh);Check(claimed && *claimed==2,"login claims session 2");
    Check(!p::LoadItemOwner(writer,c,fresh,1),"previous login can no longer load");
    auto second=p::LoadItemOwner(writer,c,fresh,2);Check(second.has_value(),"new login loads");
    const auto spend=[&](const ItemOwnerFence& fence,const ItemWallet& before,std::uint8_t op){
        ItemCommit r;r.operationId.fill(op);r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=ItemOperationKind::Grant;
        r.owners={fence};r.outcomeCode=1;r.result={1};auto after=before;after.revision++;after.balance-=100;r.wallets.push_back({before,after});
        return p::CommitItems(writer,c,r);};
    Check(!spend(first->fence,first->wallets.at(0),0xA1),"late write of the previous login is fenced off");
    Check(Scalar(admin,"SELECT balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id=4242")=="5000","fenced write changed nothing");
    Check(spend(second->fence,second->wallets.at(0),0xA2).has_value(),"current login writes");
    Check(Scalar(admin,"SELECT balance FROM player.item_wallet WHERE owner_kind=1 AND owner_id=4242")=="4900","current login wrote once");
    Execute(admin,"UPDATE player.item_owner_fence SET session_generation=18446744073709551615 WHERE owner_kind=1 AND owner_id=4242");
    claimed=p::ClaimOwnerSession(writer,c,fresh);Check(!claimed && claimed.error()==E::LimitExceeded,"exhausted generation is not wrapped");
}
// Runtime instance IDs (F4 C7a): disjoint blocks from one shared counter, also
// across connections; never below 2^32 and never wrapped.
void Allocator(CAsyncSQL& admin,CAsyncSQL& writer,const ItemCatalog& c,unsigned port,const std::string& pw)
{
    using E=ItemPersistenceError;
    constexpr std::uint64_t floor=std::uint64_t{1}<<32;
    Check(std::stoull(Scalar(admin,"SELECT next_instance_id FROM player.item_instance_id_allocator"))>=floor,"the counter starts above the legacy ID space");
    auto a=p::ReserveItemInstanceIds(writer,c,100),b=p::ReserveItemInstanceIds(writer,c,5);
    Check(a && b && a->first>=floor && a->end-a->first==100 && b->end-b->first==5 && b->first==a->end,"consecutive blocks are disjoint");
    CAsyncSQL other;Setup(other,port,pw,"item_writer");
    auto d=p::ReserveItemInstanceIds(other,c,7);Check(d && d->first==b->end && d->end-d->first==7,"another writer's block follows, never overlaps");
    Check(Scalar(admin,"SELECT next_instance_id FROM player.item_instance_id_allocator")==std::to_string(d->end),"the counter holds the next free ID");
    Check(p::ReserveItemInstanceIds(writer,c,0).error()==E::LimitExceeded && p::ReserveItemInstanceIds(writer,c,p::MaxItemInstanceIdBlock+1).error()==E::LimitExceeded,
          "an empty or oversized block is refused");
    Execute(admin,"UPDATE player.item_instance_id_allocator SET next_instance_id=18446744073709551610");
    Check(p::ReserveItemInstanceIds(writer,c,10).error()==E::LimitExceeded,"the counter never wraps");
    Check(Scalar(admin,"SELECT next_instance_id FROM player.item_instance_id_allocator")=="18446744073709551610","a refused block moves nothing");
}
void GrowthPetBirthSql(CAsyncSQL& admin,CAsyncSQL& writer,const ItemCatalog& c,ItemKey eggKey,ItemKey sealKey,ItemKey fixedKey,ItemKey bookKey)
{
    using E=ItemPersistenceError;
    const ItemOwner owner{ItemOwnerKind::Character,880104};
    const auto bag=f::Inventory(owner);
    const ItemWallet wallet{owner,WalletCurrency::Gold,1,150000};
    Check(p::ProvisionOwner(admin,c,owner,std::span(&bag,1),std::span(&wallet,1)).has_value(),"hatch owner provisioned");
    auto state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto ids=p::ReserveItemInstanceIds(writer,c,19).value();
    const auto eggId=ItemInstanceId::Create(ids.first).value();
    const auto sealId=ItemInstanceId::Create(ids.first+1).value();
    const auto grant=MakeGrantCommit(c,{30,1},state.fence,state.storages,state.items,{eggKey,0,2},std::array{eggId});
    Check(grant && p::CommitItems(writer,c,*grant).has_value(),"typed egg stack granted");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    PetCareContext context;
    context.operationId={30,2};context.actorId=owner.id;context.sessionGeneration=state.fence.sessionGeneration;
    context.rulesetRevision=1;context.characterClass=context.sex=1;context.characterLevel=50;
    context.admittedByServer=context.canUseItems=context.ownerAlive=true;
    auto source=Find(state,eggId.Value());
    PetHatchRequest request{context,{source.key,source.id.Value(),source.revision,source.owner.id,source.count,source.refinementLevel}};
    request.walletRevision=state.wallets.front().revision;request.gold=state.wallets.front().balance;
    request.newInstanceId=sealId.Value();request.bornAtUtcMs=123456789;
    request.name="\xc3\x81rv\xc3\xadz";request.recordedTraits={35,15,20};
    request.nameAcceptedByServer=request.newInstanceReservedByServer=true;
    const auto plan=MakePetHatchCommit(c,state.fence,state.storages,state.items,state.wallets,request);
    Check(plan.has_value(),"hatch plan built from exact SQL egg and Gold");

    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"second session fences hatch in flight");
    const auto stale=p::CommitItems(writer,c,*plan);
    Check(!stale && stale.error()==E::Conflict,"stale session cannot consume egg or Gold");
    auto relog=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(relog.items==state.items && relog.wallets==state.wallets,"failed fenced hatch changes neither resource");
    request.context.sessionGeneration=relog.fence.sessionGeneration;
    const auto current=MakePetHatchCommit(c,relog.fence,relog.storages,relog.items,relog.wallets,request);
    Check(current && p::CommitItems(writer,c,*current).has_value() && p::CommitItems(writer,c,*current)->replay,
        "hatch result and costs commit exactly once under receipt replay");

    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto hatched=Find(state,sealId.Value());
    Check(Find(state,eggId.Value()).count==1 && state.wallets.front().balance==50000 &&
        hatched.key==sealKey && hatched.growthPet && hatched.boundTo==owner && hatched.revision==1,
        "hatch debits exactly one egg and price, then saves the bound seal");
    Check(hatched.growthPet==current->items.back().after.growthPet,"SQL roundtrip preserves the entire growth state");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"hatch relog claimed");
    relog=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(relog.items==state.items && relog.wallets==state.wallets && relog.fence.sessionGeneration>state.fence.sessionGeneration,
        "active login loads the same pet identity and debits after reconnect");

    source=Find(relog,eggId.Value());
    request.context.operationId={30,3};request.context.sessionGeneration=relog.fence.sessionGeneration;
    request.egg.count=source.count;request.egg.revision=source.revision;
    request.walletRevision=relog.wallets.front().revision;request.gold=relog.wallets.front().balance;
    request.newInstanceId=ids.first+2;
    const auto poor=MakePetHatchCommit(c,relog.fence,relog.storages,relog.items,relog.wallets,request);
    Check(!poor && poor.error()==PetHatchCommitError{PetCareError::InsufficientGold},"another egg cannot hatch on insufficient Gold");

    auto altered=*hatched.growthPet;
    altered.name="Bodri";
    const auto payload=EncodeGrowthPetState(c,sealKey,altered).value();
    Query(admin,"UPDATE player.item_instance SET growth_pet_blob=? WHERE instance_id=?",{U::Blob(Bytes(payload)),U::UInt(sealId.Value())});
    Check(!p::LoadCurrentItemOwner(writer,c,owner),"valid-but-tampered birth state fails the state hash");
    const auto original=EncodeGrowthPetState(c,sealKey,*hatched.growthPet).value();
    Query(admin,"UPDATE player.item_instance SET growth_pet_blob=? WHERE instance_id=?",{U::Blob(Bytes(original)),U::UInt(sealId.Value())});
    Check(p::LoadCurrentItemOwner(writer,c,owner)->items==relog.items,"restored bytes recover exact birth state");

    const auto fixedId=ItemInstanceId::Create(ids.first+2).value();
    const auto fixedGrant=MakeGrantCommit(c,{30,4},relog.fence,relog.storages,relog.items,{fixedKey,0,1},std::array{fixedId});
    Check(fixedGrant && p::CommitItems(writer,c,*fixedGrant).has_value(),"fixed companion for shared channel");
    const auto growthWrite=[&](std::uint8_t serial,std::optional<CompanionAction> action,std::uint64_t elapsed=0) {
        auto admission=context;
        admission.operationId={30,serial};
        admission.sessionGeneration=state.fence.sessionGeneration;
        const auto pet=Find(state,sealId.Value());
        const PetLifecycleRequest use{admission,{pet.key,pet.id.Value(),pet.revision,pet.owner.id,1,0,
            false,false,false,true,elapsed!=0,elapsed ? pet.id.Value() : 0},pet.revision,request.bornAtUtcMs+1000*serial,*pet.growthPet};
        return MakeGrowthPetCompanionCommit(c,state.fence,state.storages,state.items,use,action,elapsed);
    };
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto summon=growthWrite(5,CompanionAction::Summon);
    Check(summon.has_value(),"growth summon plan");
    auto forged=*summon;
    forged.items.front().after.growthPet->name="Bodri";
    Check(!ValidateItemCommit(c,forged),"companion operation cannot overwrite immutable birth state");
    Check(p::CommitItems(writer,c,*summon).has_value() && p::CommitItems(writer,c,*summon)->replay,
        "growth summon commits exactly once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"summoned growth pet relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(RecallRequested(Find(state,sealId.Value())) && Find(state,sealId.Value()).growthPet->everSummoned,
        "growth intent survives new owner session");

    const auto fixed=Find(state,fixedId.Value());
    CompanionRequest competitor{fixed.key};
    competitor.state=*fixed.companion;
    competitor.instanceId=fixed.id.Value();competitor.revision=fixed.revision;competitor.ownerId=competitor.actorId=owner.id;
    competitor.rulesetRevision=1;competitor.operationId={30,6};competitor.count=1;
    competitor.characterLevel=50;competitor.characterClass=competitor.sex=1;competitor.action=CompanionAction::Summon;
    competitor.admittedByServer=competitor.ownerAlive=competitor.worldAllowsCompanion=true;
    Check(!MakeCompanionCommit(c,state.fence,state.storages,state.items,competitor),"fixed planner sees growth pet intent");
    const auto staleView=std::array{fixed};
    const auto staleFixed=MakeCompanionCommit(c,state.fence,state.storages,staleView,competitor);
    Check(staleFixed.has_value(),"incomplete fixed snapshot admits plan");
    const auto refused=p::CommitItems(writer,c,*staleFixed);
    Check(!refused && refused.error()==E::Conflict,"SQL fence sees growth pet intent even with an incomplete snapshot");

    const auto elapsed=growthWrite(7,std::nullopt,750);
    Check(elapsed && p::CommitItems(writer,c,*elapsed).has_value(),"settle only time the growth pet followed");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto dismiss=growthWrite(8,CompanionAction::Dismiss);
    Check(dismiss && p::CommitItems(writer,c,*dismiss).has_value(),"dismiss growth pet");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"recalled growth pet relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto saved=Find(state,sealId.Value());
    Check(!RecallRequested(saved) && saved.growthPet->everSummoned &&
        saved.growthPet->life.remainingMs==hatched.growthPet->life.remainingMs-750 && saved.boundTo==hatched.boundTo,
        "recall preserves bound seal and exact remaining life");

    const auto foodId=ItemInstanceId::Create(ids.first+3).value();
    const auto foodGrant=MakeGrantCommit(c,{31,1},state.fence,state.storages,state.items,{f::Leather().key,0,2},std::array{foodId});
    Check(foodGrant && p::CommitItems(writer,c,*foodGrant).has_value(),"direct feeding fixture");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto feed=[&](std::uint8_t serial,ItemInstanceId food,PetFeedingService service,std::uint64_t time=0) {
        auto admission=context;
        admission.operationId={31,serial};
        admission.sessionGeneration=state.fence.sessionGeneration;
        return MakePetFeedCommit(c,state.fence,state.storages,state.items,admission,food,sealId,service,time);
    };
    const auto consumed=[&](ItemInstanceId id) {
        return std::ranges::none_of(state.items,[&](const auto& item){return item.id==id;});
    };
    const auto refill=feed(2,foodId,PetFeedingService::Direct);
    Check(refill && refill->plan.restoredMs==750,"direct feed refills the exact remaining deficit");
    auto forgedFeed=refill->commit;
    forgedFeed.items.back().after.growthPet->traits.maximumHpPercentTenths++;
    Check(!ValidateItemCommit(c,forgedFeed),"food cannot rewrite birth traits");
    Check(p::CommitItems(writer,c,refill->commit).has_value() && p::CommitItems(writer,c,refill->commit)->replay,
        "atomic food debit and refill under receipt replay");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"fed pet relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto refilled=Find(state,sealId.Value());
    Check(refilled.growthPet->life.remainingMs==refilled.growthPet->life.capacityMs &&
        refilled.growthPet->traits==hatched.growthPet->traits && Find(state,foodId.Value()).count==1 &&
        !feed(3,foodId,PetFeedingService::Direct),"full life persists and cannot consume food twice");

    const auto again=growthWrite(9,CompanionAction::Summon);
    Check(again && p::CommitItems(writer,c,*again).has_value(),"growth pet can be summoned again");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto cube=feed(4,eggId,PetFeedingService::Cube,160000);
    Check(cube && cube->plan.restoredMs==150000 && PetFeedClockCharge(cube->commit,sealId.Value())==160000 &&
        p::CommitItems(writer,c,cube->commit).has_value(),"cube feeding settles active time before applying food");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(RecallRequested(Find(state,sealId.Value())) && Find(state,sealId.Value()).growthPet->life.remainingMs==290000 &&
        consumed(eggId),"cube food is consumed while the same pet remains active");
    const auto gearId=ItemInstanceId::Create(ids.first+4).value();
    const auto gear=MakeGrantCommit(c,{31,6},state.fence,state.storages,state.items,{f::Sword().key,0,1},std::array{gearId});
    Check(gear && p::CommitItems(writer,c,*gear).has_value(),"equipment feed fixture");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto experience=feed(7,gearId,PetFeedingService::Cube,2000);
    Check(experience && experience->plan.progress && p::CommitItems(writer,c,experience->commit).has_value(),
        "equipment feed writes progression and active time atomically");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto experienced=Find(state,sealId.Value());
    Check(experienced.growthPet->progress.itemExperience==5 && experienced.growthPet->life.remainingMs==288000 &&
        consumed(gearId),"equipment feeding adds EXP and never grants life");
    const auto expiry=growthWrite(10,std::nullopt,300000);
    Check(expiry && p::CommitItems(writer,c,*expiry).has_value(),"settle beyond remaining time");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(!RecallRequested(Find(state,sealId.Value())) && !Find(state,sealId.Value()).growthPet->life.remainingMs &&
        !growthWrite(11,CompanionAction::Summon),"expiry clears intent and rejects another summon");
    const auto revive=feed(5,foodId,PetFeedingService::Direct);
    Check(revive && revive->plan.revived && p::CommitItems(writer,c,revive->commit).has_value(),"direct food revives expired pet");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"revived pet relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->life.remainingMs==150000 &&
        !RecallRequested(Find(state,sealId.Value())) && consumed(foodId),
        "revival persists half capacity without summoning or retaining the food");

    const auto summonRevived=growthWrite(12,CompanionAction::Summon);
    Check(summonRevived && p::CommitItems(writer,c,*summonRevived).has_value(),"revived pet summoned for progression");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto progress=[&](std::uint8_t serial,std::span<const std::uint64_t> awards,
        std::optional<GrowthPetEvolution> evolution=std::nullopt) {
        auto admission=context;
        admission.operationId={32,serial};
        admission.sessionGeneration=state.fence.sessionGeneration;
        return MakeGrowthPetProgressCommit(c,state.fence,state.storages,state.items,admission,sealId,awards,evolution);
    };
    const GrowthPetEvolution evolution{request.bornAtUtcMs+20000,120,{}};
    Check(!progress(1,{},evolution),"early evolution is refused");
    const std::array<std::uint64_t,2> rewards{20,90};
    const auto levelUp=progress(2,rewards);
    Check(levelUp && p::CommitItems(writer,c,*levelUp).has_value() && p::CommitItems(writer,c,*levelUp)->replay,
        "reward batch levels once at the two-EXP gate and replays once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"levelled pet relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->progress==GrowthPetProgress{2,0,0,0},"level saved before evolution");
    const auto bookId=ItemInstanceId::Create(ids.first+5).value();
    const auto bookGrant=MakeGrantCommit(c,{33,1},state.fence,state.storages,state.items,{bookKey,0,4},std::array{bookId});
    Check(bookGrant && p::CommitItems(writer,c,*bookGrant).has_value(),"typed pet books granted");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto study=[&](std::uint8_t serial,std::optional<std::uint8_t> slot,std::uint16_t roll,std::uint64_t cooldown=0) {
        auto admission=context;
        admission.operationId={33,serial};
        admission.sessionGeneration=state.fence.sessionGeneration;
        return MakePetStudyCommit(c,state.fence,state.storages,state.items,admission,bookId,sealId,
            {slot,request.bornAtUtcMs+30000,10,cooldown,roll});
    };
    const auto locked=study(2,0,0);
    Check(!locked && locked.error()==PetStudyCommitError{PetLifecycleError::LockedSlot},
        "locked skill slot cannot spend a book");
    const auto evolve=progress(3,{},evolution);
    Check(evolve.has_value(),"evolution at the gate");
    auto forgedEvolution=*evolve;
    forgedEvolution.items.front().after.growthPet->name="Bodri";
    Check(!ValidateItemCommit(c,forgedEvolution),"evolution cannot overwrite the pet name");
    Check(p::CommitItems(writer,c,*evolve).has_value() && p::CommitItems(writer,c,*evolve)->replay,
        "evolution and its life settlement commit once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"evolved pet relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto evolved=Find(state,sealId.Value());
    Check(evolved.growthPet->progress==GrowthPetProgress{3,1,0,0} && evolved.growthPet->life.remainingMs==149880 &&
        evolved.growthPet->traits==hatched.growthPet->traits && RecallRequested(evolved) && !progress(4,{},evolution),
        "stage, level, life, name, traits and intent survive the new owner session");

    const auto learned=study(3,0,0);
    Check(learned && learned->plan.succeeded && !learned->commit.petStudyCooldown &&
        p::CommitItems(writer,c,learned->commit).has_value() && p::CommitItems(writer,c,learned->commit)->replay,
        "selected study learns the skill and consumes exactly one book on replay");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"learned skill relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->skills[0]==PetSkillSlot{true,1,1} && Find(state,bookId.Value()).count==3,
        "first learned rank and book debit survive a new session");
    const auto missed=study(4,std::nullopt,9999);
    Check(missed && !missed->plan.succeeded && p::CommitItems(writer,c,missed->commit).has_value(),
        "chance failure consumes a book without raising the rank");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(state.petStudyCooldowns.empty() && Find(state,bookId.Value()).count==2 &&
        Find(state,sealId.Value()).growthPet->skills[0].rank==1,"missed attempt preserves rank and cooldown");
    const auto upgrade=study(5,std::nullopt,0);
    Check(upgrade && upgrade->plan.succeeded && p::CommitItems(writer,c,upgrade->commit).has_value(),
        "known study commits a higher rank, its book and owner cooldown");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"upgraded skill relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto trained=Find(state,sealId.Value());
    const auto ready=state.petStudyCooldowns.at(1);
    Check(trained.growthPet->skills[0]==PetSkillSlot{true,1,2} && trained.growthPet->skills[1]==PetSkillSlot{} &&
        trained.growthPet->progress==evolved.growthPet->progress && trained.boundTo==evolved.boundTo &&
        Find(state,bookId.Value()).count==1 && ready==request.bornAtUtcMs+30000+10800000,
        "upgraded rank, other slots, pet identity and cooldown reload intact");
    const auto blocked=study(6,std::nullopt,0,ready);
    Check(!blocked && blocked.error()==PetStudyCommitError{PetLifecycleError::Cooldown},
        "persisted cooldown blocks another known attempt");
    const auto incomplete=study(7,std::nullopt,0);
    Check(incomplete && !p::CommitItems(writer,c,incomplete->commit),"SQL rejects an omitted owner cooldown snapshot");

    const auto resetOne=ItemKey::Create(ItemCategory::Special,96).value();
    const auto resetAll=ItemKey::Create(ItemCategory::Special,97).value();
    const auto oneId=ItemInstanceId::Create(ids.first+6).value(),allId=ItemInstanceId::Create(ids.first+7).value();
    for(const auto& [key,id]:std::array{std::pair{resetOne,oneId},std::pair{resetAll,allId}}) {
        const auto grant=MakeGrantCommit(c,{34,static_cast<std::uint8_t>(key.LocalId())},state.fence,state.storages,state.items,
            {key,0,2},std::array{id});
        Check(grant && p::CommitItems(writer,c,*grant).has_value(),"reset supply granted");
        state=p::LoadCurrentItemOwner(writer,c,owner).value();
    }
    const auto reset=[&](std::uint8_t serial,ItemInstanceId supply,std::optional<std::uint8_t> slot) {
        auto admission=context;admission.operationId={35,serial};admission.sessionGeneration=state.fence.sessionGeneration;
        return MakePetSkillResetCommit(c,state.fence,state.storages,state.items,admission,supply,sealId,
            {slot,request.bornAtUtcMs+40000,10});
    };
    Check(!reset(1,oneId,1),"empty locked slot cannot spend a reset supply");
    Check(!reset(2,allId,0),"reset-all rejects a selected slot");
    const auto forgotten=reset(3,oneId,0);
    Check(forgotten && !forgotten->commit.petStudyCooldown,"reset does not replace owner study cooldown");
    auto forgedReset=forgotten->commit;forgedReset.items.back().after.growthPet->name="Renamed";
    Check(!ValidateItemCommit(c,forgedReset),"reset cannot alter unrelated pet identity");
    Check(p::CommitItems(writer,c,forgotten->commit).has_value() && p::CommitItems(writer,c,forgotten->commit)->replay,
        "single reset and its debit commit once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"single reset relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->skills[0]==PetSkillSlot{true,0,0} &&
        !Find(state,sealId.Value()).growthPet->skills[1].unlocked && Find(state,oneId.Value()).count==1 &&
        state.petStudyCooldowns.at(1)==ready,"reset retains unlocked/locked slots, cooldown and one debit after relog");
    Check(!reset(4,oneId,0) && !reset(5,allId,std::nullopt),"empty reset consumes nothing");
    const auto relearn=study(8,0,0);
    Check(relearn && p::CommitItems(writer,c,relearn->commit).has_value(),"forgotten slot can learn again");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto cleared=reset(6,allId,std::nullopt);
    Check(cleared && p::CommitItems(writer,c,cleared->commit).has_value() && p::CommitItems(writer,c,cleared->commit)->replay,
        "all reset commits once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"all reset relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto final=Find(state,sealId.Value());
    Check(final.growthPet->skills[0]==PetSkillSlot{true,0,0} && final.growthPet->progress==trained.growthPet->progress &&
        final.growthPet->traits==trained.growthPet->traits && final.growthPet->name==trained.growthPet->name &&
        Find(state,allId.Value()).count==1 && state.petStudyCooldowns.at(1)==ready,
        "all reset persists without changing progression, birth traits or owner cooldown");
    const auto recalled=growthWrite(13,CompanionAction::Dismiss);
    Check(recalled && p::CommitItems(writer,c,*recalled).has_value(),"recall before pet item services");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    for(unsigned index=0;index<4;++index) {
        const auto key=ItemKey::Create(ItemCategory::Special,90+index).value();
        const auto id=ItemInstanceId::Create(ids.first+8+index).value();
        const auto grant=MakeGrantCommit(c,{36,static_cast<std::uint8_t>(index)},state.fence,state.storages,state.items,{key,0,2},std::array{id});
        Check(grant && p::CommitItems(writer,c,*grant).has_value(),"pet service supply granted");
        state=p::LoadCurrentItemOwner(writer,c,owner).value();
    }
    const auto service=[&](unsigned source,std::uint8_t mode,std::uint8_t serial,std::uint16_t roll=0) {
        auto admission=context;admission.operationId={37,serial};admission.sessionGeneration=state.fence.sessionGeneration;
        return MakePetServiceCommit(c,state.fence,state.storages,state.items,admission,ItemInstanceId::Create(ids.first+8+source).value(),sealId,
            {mode,request.bornAtUtcMs+50000,0,roll});
    };
    const auto remaining=Find(state,sealId.Value()).growthPet->life.remainingMs;
    const auto extended=service(0,0,1);
    Check(extended && extended->plan.after.life.remainingMs==remaining+60000 && extended->plan.after.life.capacityMs==remaining+60000 &&
        p::CommitItems(writer,c,extended->commit).has_value() && p::CommitItems(writer,c,extended->commit)->replay,"extension adds to saved remainder once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"extended life relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->life==extended->plan.after.life,"extended capacity and remainder persist");
    const auto enchanted=service(2,2,2);
    Check(enchanted && enchanted->plan.succeeded && p::CommitItems(writer,c,enchanted->commit).has_value(),"enchantment and its supply commit together");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto traits=Find(state,sealId.Value()).growthPet->traits;
    const auto missedEnchant=service(3,2,3,9999);
    Check(missedEnchant && !missedEnchant->plan.succeeded && p::CommitItems(writer,c,missedEnchant->commit).has_value(),"failed enchant consumes its supply");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->traits==traits,"failed enchant preserves birth traits");
    const auto permanent=service(1,1,4);
    Check(permanent && permanent->plan.after.life==GrowthPetLife{CompanionClock::Permanent,0,0,true} &&
        p::CommitItems(writer,c,permanent->commit).has_value() && p::CommitItems(writer,c,permanent->commit)->replay,"permanent life grants explicit durable permission once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"permanent life relog");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet->life==permanent->plan.after.life && !service(0,0,5) && !service(1,1,6),
        "permanent life persists and rejects no-effect repeat supplies");
    const auto carrierKey=ItemKey::Create(ItemCategory::Special,89).value();
    const auto carrierId=ItemInstanceId::Create(ids.first+12).value(),anotherBook=ItemInstanceId::Create(ids.first+13).value();
    for(const auto& [key,id]:std::array{std::pair{carrierKey,carrierId},std::pair{bookKey,anotherBook}}) {
        const auto grant=MakeGrantCommit(c,{38,static_cast<std::uint8_t>(key.LocalId())},state.fence,state.storages,state.items,{key,0,1},std::array{id});
        Check(grant && p::CommitItems(writer,c,*grant).has_value(),"carrier and book granted");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    }
    const auto summonForCarrier=growthWrite(14,CompanionAction::Summon);
    Check(summonForCarrier && p::CommitItems(writer,c,*summonForCarrier).has_value(),"summon pet before carrier skill test");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    auto learnContext=context;learnContext.operationId={38,1};learnContext.sessionGeneration=state.fence.sessionGeneration;
    const auto learnForCarrier=MakePetStudyCommit(c,state.fence,state.storages,state.items,learnContext,anotherBook,sealId,{0,request.bornAtUtcMs+60000});
    Check(learnForCarrier && p::CommitItems(writer,c,learnForCarrier->commit).has_value(),"carrier test pet retains a learned skill");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto recallForCarrier=growthWrite(15,CompanionAction::Dismiss);
    Check(recallForCarrier && p::CommitItems(writer,c,*recallForCarrier).has_value(),"recall before packing");
    state=p::LoadCurrentItemOwner(writer,c,owner).value();const auto beforeCarrier=*Find(state,sealId.Value()).growthPet;
    const auto packed=service(4,5,7);
    Check(packed.has_value(),"native carrier plan");
    const auto packedWrite=p::CommitItems(writer,c,packed->commit);
    if(!packedWrite) {
        const auto sql=p::PrepareItemWrite(c,packed->commit);
        Message diagnostic(writer.DirectTransactionPrepared(*sql));
        if(diagnostic && diagnostic->transactionFailedStep<sql->size())
            std::cerr<<"PET_CARRIER_SQL_FAILED_STEP="<<diagnostic->transactionFailedStep<<" query="<<(*sql)[diagnostic->transactionFailedStep].query<<'\n';
    }
    Check(packedWrite.has_value(),("pack SQL error "+std::to_string(packedWrite ? 0 : static_cast<unsigned>(packedWrite.error()))).c_str());
    const auto packedReplay=p::CommitItems(writer,c,packed->commit);
    Check(packedReplay && packedReplay->replay,"packed receipt replay");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"packed pet relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto stored=Find(state,sealId.Value());
    auto expectedStored=beforeCarrier;expectedStored.carrierInstanceId=carrierId.Value();
    Check(stored.growthPet==expectedStored && stored.placement==ItemPlacement{ItemStorageKind::PetCarrier,carrierId.Value(),0} &&
        Find(state,carrierId.Value()).petSupply->containedSeal==sealId.Value(),"full pet state and bidirectional box link reload");
    Check(!MakeConsumeCommit(c,{38,2},state.fence,state.storages,state.items,carrierId,1),"ordinary consumption cannot delete a packed pet");
    const auto unpacked=service(4,6,8);
    Check(unpacked && p::CommitItems(writer,c,unpacked->commit).has_value() && p::CommitItems(writer,c,unpacked->commit)->replay,
        "unpack consumes one box and reserves the destination once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"unpacked pet relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,sealId.Value()).growthPet==beforeCarrier && Find(state,sealId.Value()).placement->kind==ItemStorageKind::Inventory &&
        std::ranges::none_of(state.storages,[](const auto& l){return l.storage.kind==ItemStorageKind::PetCarrier;}) &&
        std::ranges::none_of(state.items,[&](const auto& p){return p.id==carrierId;}),"same seal returns intact, without refilling life or leaking its internal storage");
    const auto skinKey=ItemKey::Create(ItemCategory::Special,88).value();const auto skinId=ItemInstanceId::Create(ids.first+14).value();
    ItemGrant skinGrant{skinKey};skinGrant.createdAtUtcMs=request.bornAtUtcMs+60000;
    const auto mintedSkin=MakeGrantCommit(c,{39,1},state.fence,state.storages,state.items,skinGrant,std::array{skinId});
    Check(mintedSkin && p::CommitItems(writer,c,*mintedSkin).has_value(),"skin with factory clock granted");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto summonSkin=growthWrite(16,CompanionAction::Summon);Check(summonSkin && p::CommitItems(writer,c,*summonSkin).has_value(),"summon for appearance");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto skinService=[&](std::uint8_t mode,std::uint8_t nonce,std::uint64_t elapsed=0) {
        auto actor=context;actor.operationId={39,nonce};actor.sessionGeneration=state.fence.sessionGeneration;
        PetServiceTerms terms{mode,request.bornAtUtcMs+60001,elapsed};terms.following=true;
        return MakePetServiceCommit(c,state.fence,state.storages,state.items,actor,skinId,sealId,terms);
    };
    const auto petBeforeSkin=Find(state,sealId.Value()).growthPet;
    const auto equipSkin=skinService(7,2);Check(equipSkin && p::CommitItems(writer,c,equipSkin->commit).has_value() && p::CommitItems(writer,c,equipSkin->commit)->replay,"skin equip replay is idempotent");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"equipped skin relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(Find(state,skinId.Value()).petSupply->equippedSeal==sealId.Value() && Find(state,sealId.Value()).growthPet==petBeforeSkin,"skin relog retains binding without touching pet progression or bonuses");
    Check(!MakeConsumeCommit(c,{39,3},state.fence,state.storages,state.items,skinId,1),"equipped skin cannot be consumed normally");
    const auto removeSkin=skinService(8,4,321);Check(removeSkin && p::CommitItems(writer,c,removeSkin->commit).has_value(),"skin removal settles exactly the accrued equipped time");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"removed skin relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(!Find(state,skinId.Value()).petSupply->equippedSeal && Find(state,skinId.Value()).petSupply->lifetime.remainingMs==9679,"removed skin time survives relog");
    const auto reEquip=skinService(7,5);Check(reEquip && p::CommitItems(writer,c,reEquip->commit).has_value(),"skin re-equip");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto expireSkin=MakePetSkinClockCommit(c,{39,6},state.fence,state.storages,state.items,skinId,request.bornAtUtcMs+80000,9679,true);
    Check(expireSkin && p::CommitItems(writer,c,*expireSkin).has_value() && p::CommitItems(writer,c,*expireSkin)->replay,"expiry removes skin exactly once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"expired skin relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(std::ranges::none_of(state.items,[&](const auto& i){return i.id==skinId;}) && Find(state,sealId.Value()).growthPet==petBeforeSkin,"expired skin stays removed and the entire pet survives");
    const auto waitBook=ItemInstanceId::Create(ids.first+15).value(),aidId=ItemInstanceId::Create(ids.first+16).value();
    for(const auto& [key,id,count]:std::array{std::tuple{bookKey,waitBook,3u},std::tuple{book_aid_fixture::Key(1),aidId,2u}}) {
        const auto grant=MakeGrantCommit(c,{40,static_cast<std::uint8_t>(count)},state.fence,state.storages,state.items,{key,0,count},std::array{id});
        Check(grant && p::CommitItems(writer,c,*grant).has_value(),"waiting test book and aid granted");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    }
    Query(admin,"INSERT INTO player.player(id,exp,skill_level) VALUES(880104,4321,?)",{U::Blob(std::string(2550,'\0'))});
    BookCharacter live;live.experience=4321;
    const auto aid=MakeBookAidRuntimeCommit(c,{40,4},state.fence,state.storages,state.items,state.learning,live,aidId,book_runtime_fixture::Facts());
    Check(aid && p::CommitItems(writer,c,aid->commit).has_value(),"real book aid credits pet study bypass charges");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto waitStudy=[&](std::uint8_t serial,std::uint16_t roll,bool skip) {
        auto actor=context;actor.operationId={41,serial};actor.sessionGeneration=state.fence.sessionGeneration;
        PetStudyTerms t{{},request.bornAtUtcMs+80000+serial,0,state.petStudyCooldowns.at(1),roll};
        if(skip)t.bypass=PetStudyBypass{state.learning,live};
        return MakePetStudyCommit(c,state.fence,state.storages,state.items,actor,waitBook,sealId,t);
    };
    Check(state.learning.bypassTokens==2 && !waitStudy(1,0,false),"normal known study does not silently spend available bypass");
    const auto missWait=waitStudy(2,9999,true);Check(missWait && !missWait->plan.succeeded,"accepted cooldown-skip failure");
    Execute(admin,"UPDATE player.player SET typed_book_revision=999 WHERE id=880104");
    Check(!p::CommitItems(writer,c,missWait->commit),"legacy character CAS rolls book, pet and bypass back together");
    Execute(admin,"UPDATE player.player SET typed_book_revision=1 WHERE id=880104");
    Check(p::LoadCurrentItemOwner(writer,c,owner)->learning==state.learning && Find(p::LoadCurrentItemOwner(writer,c,owner).value(),waitBook.Value()).count==3,"failed checkpoint preserves charges and books");
    const auto oldDeadline=state.petStudyCooldowns.at(1);
    Check(p::CommitItems(writer,c,missWait->commit).has_value() && p::CommitItems(writer,c,missWait->commit)->replay,"failed bypass attempt debits exactly once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"failed bypass relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(state.learning.bypassTokens==1 && state.learning.character==live && state.petStudyCooldowns.at(1)==oldDeadline && Find(state,sealId.Value()).growthPet->skills[0].rank==1,"failed bypass retains its rank, live character and old cooldown across relog");
    const auto learnedWait=waitStudy(3,0,true);Check(learnedWait && p::CommitItems(writer,c,learnedWait->commit).has_value(),"successful cooldown bypass committed");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"successful bypass relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(!state.learning.bypassTokens && state.learning.character==live && Find(state,sealId.Value()).growthPet->skills[0].rank==2 && Find(state,waitBook.Value()).count==1 && !waitStudy(4,0,true),"rank, empty charge balance and book count survive relog; no-charge attempt is refused");
    ItemCommit credit;credit.operationId={42,1};credit.catalogRevision=c.Revision();credit.catalogHash=c.ContentHash();credit.kind=ItemOperationKind::Grant;credit.owners={state.fence};credit.outcomeCode=1;credit.result={1};
    auto gold=state.wallets.front();++gold.revision;gold.balance+=100000;credit.wallets={{state.wallets.front(),gold}};
    Check(p::CommitItems(writer,c,credit).has_value(),"second hatch fixture budget granted");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto foodEggId=ItemInstanceId::Create(ids.first+18).value();
    const auto foodEgg=MakeGrantCommit(c,{42,5},state.fence,state.storages,state.items,{eggKey,0,1},std::array{foodEggId});
    Check(foodEgg && p::CommitItems(writer,c,*foodEgg).has_value(),"second hatch egg granted");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    auto another=request;another.context.operationId={42,2};another.context.sessionGeneration=state.fence.sessionGeneration;
    const auto egg=Find(state,foodEggId.Value());another.egg={egg.key,egg.id.Value(),egg.revision,owner.id,egg.count,0};
    another.walletRevision=state.wallets.front().revision;another.gold=state.wallets.front().balance;another.newInstanceId=ids.first+17;another.name="Consumer";another.bornAtUtcMs=request.bornAtUtcMs+90000;
    const auto secondHatch=MakePetHatchCommit(c,state.fence,state.storages,state.items,state.wallets,another);
    Check(secondHatch && p::CommitItems(writer,c,*secondHatch).has_value(),"second pet hatched through the real factory");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto retireFood=growthWrite(17,CompanionAction::Dismiss);Check(retireFood && p::CommitItems(writer,c,*retireFood).has_value(),"recall the learned donor");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto consumerId=ItemInstanceId::Create(another.newInstanceId).value();const auto consumer=Find(state,consumerId.Value());
    auto actor=context;actor.operationId={42,3};actor.sessionGeneration=state.fence.sessionGeneration;
    PetLifecycleRequest use{actor,{consumer.key,consumer.id.Value(),consumer.revision,owner.id,1,0,false,false,false,true},consumer.revision,another.bornAtUtcMs,*consumer.growthPet};
    const auto callConsumer=MakeGrowthPetCompanionCommit(c,state.fence,state.storages,state.items,use,CompanionAction::Summon,0);
    Check(callConsumer && p::CommitItems(writer,c,*callConsumer).has_value(),"summon the receiving pet");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    const auto donorHistory=*Find(state,sealId.Value()).growthPet;actor.operationId={42,4};
    const auto sacrifice=MakePetFeedCommit(c,state.fence,state.storages,state.items,actor,sealId,consumerId,PetFeedingService::Cube,5000);
    Check(sacrifice && p::CommitItems(writer,c,sacrifice->commit).has_value() && p::CommitItems(writer,c,sacrifice->commit)->replay,"pet sacrifice and restored lifetime commit exactly once");
    Check(p::ClaimOwnerSession(writer,c,owner).has_value(),"sacrificed pet relog");state=p::LoadCurrentItemOwner(writer,c,owner).value();
    Check(std::ranges::none_of(state.items,[&](const auto& i){return i.id==sealId;}) && Find(state,consumerId.Value()).growthPet->life.remainingMs==300000,"donor stays removed and receiving lifetime survives relog");
    const auto historyBytes=EncodeGrowthPetState(c,sealKey,donorHistory).value();const std::string history(historyBytes.begin(),historyBytes.end());
    const auto audit=Query(admin,"SELECT destroyed,growth_pet_blob FROM player.item_instance WHERE instance_id=?",{U::UInt(sealId.Value())});
    const auto row=audit->Get()->FetchRow();const auto sizes=audit->Get()->FetchLengths();
    Check(row && std::string(row[0])=="1" && std::string(row[1],sizes[1])==history,"destroyed donor retains its exact complete birth and learned history for audit");
}

void ActualSql(CAsyncSQL& admin,CAsyncSQL& writer,unsigned port,const std::string& pw)
{
    auto source=attribute_runtime_fixture::Catalog();
    effect_runtime_fixture::Extend(source);book_runtime_fixture::Extend(source);book_aid_fixture::Extend(source);polymorph_runtime_fixture::Extend(source);fish_processing_fixture::Extend(source);tool_runtime_fixture::Extend(source);
    const auto petSeal=ItemKey::Create(ItemCategory::Companion,1).value();
    const auto mountSeal=ItemKey::Create(ItemCategory::Companion,2).value();
    const auto growthSeal=ItemKey::Create(ItemCategory::Companion,3).value();
    const auto hatchEgg=ItemKey::Create(ItemCategory::Container,98).value();
    const auto petBook=ItemKey::Create(ItemCategory::Special,98).value();
    source.companionForms={{1,"companion.sql_pet",32500,"models/sql_pet.model"},{2,"companion.sql_mount",32000,"models/sql_mount.model"}};
    source.companions={{petSeal,CompanionKind::Pet,1,{},CompanionClock::Permanent},
        {mountSeal,CompanionKind::Mount,2,2,CompanionClock::Permanent}};
    source.refinements.push_back({27001,"refine.sql_companion",RefinementKind::Companion,0,{{0,{},{}}}});
    for(const auto key:{petSeal,mountSeal})
    {
        ItemBaseRow base{key};
        base.contentCode=key==petSeal ? "companion.sql_pet_seal" : "companion.sql_mount_seal";
        base.nameKey=base.contentCode+".name";
        base.descriptionKey=base.contentCode+".description";
        base.iconAsset="icons/companion.png";
        base.refinementProfile=27001;
        base.usePolicy=base.tradePolicy=1;
        source.items.push_back(std::move(base));
    }
    source.companionForms.push_back({3,"companion.sql_growth",34041,"models/sql_growth.model"});
    source.growthPetProfiles.push_back({1,"growth.sql_birth",3,GrowthExpPolicy::MobAndItem,
        {{1,100,5},{2,200,20},{3,0,0}},{{0,2,3},{1,3,2}}});
    source.companions.push_back({growthSeal,CompanionKind::GrowthPet,3,{},CompanionClock::ActiveTime,300000,false,1,1});
    source.petTrainingProfiles.push_back({1,"training.sql_growth",0,0,0,1,6,{{0,0,0,0}},
        {{0,1,0},{1,2,0},{2,2,0},{3,0,INT64_MAX}},
        {{1,"pet.sql_skill",PetEffect::BossDamagePercent,{{1,100},{2,200},{3,300}}}}});
    ItemBaseRow petBookBase{petBook};
    petBookBase.contentCode="special.sql_pet_study";petBookBase.nameKey="item.sql_pet_study.name";
    petBookBase.descriptionKey="item.sql_pet_study.description";petBookBase.iconAsset="icons/sql_pet_study.png";
    petBookBase.refinementProfile=1;petBookBase.usePolicy=petBookBase.tradePolicy=1;petBookBase.maxStack=20;
    source.items.push_back(std::move(petBookBase));
    source.petSupplies.push_back({petBook,1});
    source.petSupplyProfiles.push_back({1,"supply.sql_pet_study",PetSupplyKind::Study,PetStudyRule{1,true,true,7100,10800000}});
    for(unsigned kind=2;kind<=3;++kind) {
        auto base=source.items.back();base.key=ItemKey::Create(ItemCategory::Special,94+kind).value();
        base.contentCode="special.sql_pet_reset_"+std::to_string(kind);
        source.items.push_back(base);source.petSupplies.push_back({base.key,kind});
        source.petSupplyProfiles.push_back({kind,"supply.sql_pet_reset_"+std::to_string(kind),static_cast<PetSupplyKind>(kind)});
    }
    for(unsigned index=0;index<4;++index) {
        auto base=source.items.back();base.key=ItemKey::Create(ItemCategory::Special,90+index).value();
        base.contentCode="special.sql_pet_service_"+std::to_string(index);source.items.push_back(base);
        const auto profile=10+index;source.petSupplies.push_back({base.key,profile});
        PetSupplyProfile rule{profile,"supply.sql_pet_service_"+std::to_string(index),
            index==0 ? PetSupplyKind::ExtendLife : index==1 ? PetSupplyKind::PermanentLife : PetSupplyKind::Enchant};
        if(index==0)rule.extension=PetExtensionRule{60000,86400000};
        if(index>=2)rule.enchant=PetEnchantRule{static_cast<std::uint16_t>(index==2 ? 10000 : 0),5,{150,100,100}};
        source.petSupplyProfiles.push_back(rule);
    }
    {
        auto base=source.items.back();base.key=ItemKey::Create(ItemCategory::Special,89).value();base.contentCode="special.sql_pet_carrier";base.maxStack=1;
        source.items.push_back(base);source.petSupplies.push_back({base.key,20});source.petSupplyProfiles.push_back({20,"supply.sql_pet_carrier",PetSupplyKind::Carrier});
    }
    {
        auto base=source.items.back();base.key=ItemKey::Create(ItemCategory::Special,88).value();base.contentCode="special.sql_pet_skin";base.maxStack=1;
        source.items.push_back(base);source.petSupplies.push_back({base.key,21});
        PetSupplyProfile skin{21,"supply.sql_pet_skin",PetSupplyKind::Skin};skin.skin=PetSkinRule{2,CostumeClock::EquippedTime,10000};source.petSupplyProfiles.push_back(skin);
    }
    source.petHatchProfiles.push_back({1,"hatch.sql_birth",growthSeal,100000,{30,40,5},{10,20,5},{10,20,5}});
    source.petFeedRules={{hatchEgg,PetFeedingService::Cube,PetFoodKind::LifetimeFraction,PetFoodConsumption::WholeStack,0,5000,0,0},
        {f::Leather().key,PetFeedingService::Direct,PetFoodKind::RefillOrRevive,PetFoodConsumption::ExactUnits,1,0,5000,0},
        {f::Sword().key,PetFeedingService::Cube,PetFoodKind::EquipmentExperience,PetFoodConsumption::WholeStack,0,0,0,20},
        {growthSeal,PetFeedingService::Cube,PetFoodKind::LifetimeFraction,PetFoodConsumption::ExactUnits,1,5000,0,0}};
    source.containers.push_back({hatchEgg,ContainerKind::PetEgg,{},0,1});
    ItemBaseRow growthBase{growthSeal};
    growthBase.contentCode="companion.sql_growth_seal";growthBase.nameKey="item.sql_growth.name";
    growthBase.descriptionKey="item.sql_growth.description";growthBase.iconAsset="icons/sql_growth.png";
    growthBase.refinementProfile=1;growthBase.usePolicy=growthBase.tradePolicy=1;
    growthBase.binding=BindingPolicy::OnAcquire;
    source.items.push_back(std::move(growthBase));
    ItemBaseRow eggBase{hatchEgg};
    eggBase.contentCode="container.sql_hatch_egg";eggBase.nameKey="item.sql_hatch_egg.name";
    eggBase.descriptionKey="item.sql_hatch_egg.description";eggBase.iconAsset="icons/sql_hatch_egg.png";
    eggBase.refinementProfile=1;eggBase.usePolicy=eggBase.tradePolicy=1;eggBase.maxStack=20;
    source.items.push_back(std::move(eggBase));
    const auto sqlBox=ItemKey::Create(8,99).value();auto boxBase=*std::ranges::find(source.items,item_catalog_fixture::GiftBox(),&ItemBaseRow::key);
    boxBase.key=sqlBox;boxBase.contentCode="container.sql_opening";source.items.push_back(boxBase);source.containers.push_back({sqlBox,ContainerKind::Locked,3,7});
    source.lootProfiles.push_back({3,"loot.sql_opening",LootPickMode::IndependentChance,{{1,LootRewardKind::Item,f::Leather().key,3,10000,0},{2,LootRewardKind::Gold,{},75,10000,0}}});
    const auto catalog=BuildItemCatalog(source);
    if(!catalog)throw std::runtime_error("SQL fixture catalog rejected: "+catalog.error().field);
    const auto c=*catalog;Populate(admin,source);
    auto loadedCatalog=s::ReadCatalog(writer,{c->Revision(),c->ContentHash()});Check(loadedCatalog.has_value(),"SQL/native catalog parity");
    Query(admin,"INSERT INTO player.item_persistence_format VALUES(1,1,?,?)",{U::UInt(c->Revision()),U::Blob(Bytes(c->ContentHash()))});
    Execute(admin,"INSERT INTO player.item_owner_fence VALUES(1,42,7,1),(1,43,8,1),(2,42,9,1),(3,42,10,1)");
    Execute(admin,"INSERT INTO player.item_storage VALUES(1,42,1,1,5,9,2),(1,43,1,1,5,9,2),(2,42,3,1,5,9,2),(3,42,5,1,5,9,2),(1,42,2,1,8,1,1)");
    Execute(admin,"INSERT INTO player.item_wallet VALUES(1,42,1,1,1000)");
    auto empty=Load(writer,*c);Check(empty.items.empty() && empty.wallets[0].balance==1000,"fresh owner");
    Message denied(writer.DirectQueryPrepared("UPDATE protos.item_base SET sell_amount=0",{}));Check(denied && denied->uiSQLErrno,"runtime writer cannot modify catalog");
    auto second=f::Sword(3);second.refinementLevel=0;second.placement->anchor=2;
    auto grant=f::Grant(*c,{f::Sword(),f::Leather(),second});
    // A receipt absent from a consistent snapshot can still be committing.
    // Reconciliation must wait for that write instead of reporting absence.
    const auto inFlight=p::PrepareItemWrite(*c,grant).value();
    Execute(admin,"START TRANSACTION");
    for(const auto& step:inFlight)Query(admin,step.query,step.params);
    Execute(writer,"SET SESSION innodb_lock_wait_timeout=1");
    auto unresolved=p::FindItemReceipt(writer,*c,grant);
    Check(!unresolved,"an uncommitted receipt must not be reported absent");
    Execute(admin,"ROLLBACK");
    auto rolledBack=p::FindItemReceipt(writer,*c,grant);
    Check(rolledBack && !*rolledBack,"a settled rollback confirms the receipt is absent");
    Execute(writer,"SET SESSION innodb_lock_wait_timeout=50");
    auto committed=p::CommitItems(writer,*c,grant);Check(committed && !committed->replay,"initial atomic grant");
    auto state=Load(writer,*c);Check(state.items.size()==3 && state.fence.revision==2,"grant reloaded");
    Check(Find(state,f::Sword().id.Value())==f::Sword() && Find(state,3)==second,"full signed attribute/socket/high64 round trip");
    auto replay=p::CommitItems(writer,*c,grant);Check(replay && replay->replay && replay->receipt.result==grant.result,"identical binary receipt replay");
    auto mismatch=grant;mismatch.result[1]--;auto badReceipt=p::CommitItems(writer,*c,mismatch);Check(!badReceipt && badReceipt.error()==E::ReceiptMismatch,"operation identity cannot change payload");
    std::array<std::uint8_t,16> op{};op[0]=2;const auto plan=f::Refine(*c,1,op,state.items,state.wallets[0]);
    auto refine=MakeRefinementCommit(*c,plan,state.fence,state.storages,state.items,state.wallets[0]);Check(refine.has_value(),"real refinement writer proposal");
    committed=p::CommitItems(writer,*c,*refine);Check(committed.has_value(),"refine+consume+gold committed together");
    state=Load(writer,*c);Check(state.items.size()==2 && state.wallets[0].balance==900 && state.fence.revision==3,"refine economy reloaded");
    auto ten=Find(state,f::Sword().id.Value());Check(ten.refinementLevel==10 && ten.key==f::Sword().key && ten.attributes==f::Sword().attributes && ten.stones==f::Sword().stones,"+9 to +10 same prototype and state");
    Check(Find(state,3).refinementLevel==0,"second instance unchanged");
    Check(Scalar(admin,"SELECT CONCAT(destroyed,':',stack_count,':',storage_kind IS NULL) FROM player.item_instance WHERE instance_id=2")=="1:0:1","consumed identity tombstoned");
    replay=p::CommitItems(writer,*c,*refine);Check(replay && replay->replay && Scalar(admin,"SELECT balance FROM player.item_wallet")=="900","refine retry never double spends");
    {
        CAsyncSQL reconnect;Setup(reconnect,port,pw,"item_writer");auto reload=Load(reconnect,*c);Check(Find(reload,ten.id.Value())==ten && reload.wallets==state.wallets,"reconnect preserves state");
        auto receipt=p::FindItemReceipt(reconnect,*c,*refine);Check(receipt && receipt->has_value(),"fresh connection durable reconciliation");
    }
    auto rejected=[&](ItemCommit r,E expected){const auto before=Fingerprint(admin);auto result=p::CommitItems(writer,*c,r);Check(!result && result.error()==expected,"expected commit rejection");Check(Fingerprint(admin)==before,"failed request changed persistent state");};
    auto stale=Move(*c,state,ten.id.Value(),15,3);stale.owners[0].revision--;rejected(stale,E::Conflict);
    stale=Move(*c,state,ten.id.Value(),15,4);stale.owners[0].sessionGeneration++;rejected(stale,E::Conflict);
    stale=Move(*c,state,ten.id.Value(),15,5);stale.items[0].before->revision--;rejected(stale,E::InvalidChange);
    auto cached=Move(*c,state,ten.id.Value(),15,7);
    Query(admin,"UPDATE player.item_instance SET refinement_level=11 WHERE instance_id=?",{U::UInt(ten.id.Value())});
    rejected(cached,E::Conflict);Query(admin,"UPDATE player.item_instance SET refinement_level=10 WHERE instance_id=?",{U::UInt(ten.id.Value())});
    Query(admin,"UPDATE player.item_instance_attribute SET value_scaled=value_scaled+1 WHERE instance_id=? AND slot_index=0",{U::UInt(ten.id.Value())});
    rejected(cached,E::Conflict);Query(admin,"UPDATE player.item_instance_attribute SET value_scaled=value_scaled-1 WHERE instance_id=? AND slot_index=0",{U::UInt(ten.id.Value())});
    Query(admin,"INSERT INTO player.item_instance_attribute VALUES(?,4,1,100)",{U::UInt(ten.id.Value())});
    rejected(cached,E::Conflict);Query(admin,"DELETE FROM player.item_instance_attribute WHERE instance_id=? AND slot_index=4",{U::UInt(ten.id.Value())});
    Query(admin,"INSERT INTO player.item_occupied_cell VALUES(1,42,1,1,4,?)",{U::UInt(ten.id.Value())});
    rejected(cached,E::Conflict);Query(admin,"DELETE FROM player.item_occupied_cell WHERE instance_id=? AND cell_index=4",{U::UInt(ten.id.Value())});
    // Collision with a NON-anchor cell of an unchanged item. Wallet update and
    // footprint release happen first, and must both roll back on that collision.
    auto collision=Move(*c,state,ten.id.Value(),7,6);auto debit=state.wallets[0];debit.revision++;debit.balance--;
    collision.wallets={{state.wallets[0],debit}};rejected(collision,E::SqlFailure);
    for(unsigned phase=0;phase<3;++phase)
    {
        const auto request=Move(*c,state,ten.id.Value(),15,10+phase);const auto before=Fingerprint(admin);
        const std::string table=phase==0 ? "item_instance_attribute" : phase==1 ? "item_instance_socket" : "item_occupied_cell";
        Execute(admin,"CREATE TRIGGER player.fail_item_write BEFORE INSERT ON player."+table+" FOR EACH ROW SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='injected item write failure'");
        auto result=p::CommitItems(writer,*c,request);Check(!result,"injected child write unexpectedly committed");
        Execute(admin,"DROP TRIGGER player.fail_item_write");Check(Fingerprint(admin)==before,"child failure left a partial commit");
    }
    // Simultaneous delivery of one operation on separate owned connections.
    auto concurrent=Move(*c,state,ten.id.Value(),15,20);
    const auto deliver=[&]{CAsyncSQL db;db.SetTimeouts(5);db.SetRetryConnectionErrors(false);if(!db.Setup("127.0.0.1","item_writer",pw.c_str(),"player","utf8mb4",true,port))return std::expected<p::ItemWriteResult,E>{std::unexpected(E::SqlFailure)};return p::CommitItems(db,*c,concurrent);};
    auto one=std::async(std::launch::async,deliver),two=std::async(std::launch::async,deliver);auto r1=one.get(),r2=two.get();
    Check(r1 && r2 && r1->replay!=r2->replay,"concurrent duplicate must produce one commit and one replay");
    state=Load(writer,*c);Check(state.fence.revision==4 && Find(state,ten.id.Value()).revision==3,"concurrent duplicate increments once");
    // Atomic swap releases both complete footprints before reacquiring either.
    auto swap=Move(*c,state,ten.id.Value(),2,21);auto beforeSecond=Find(state,3),afterSecond=beforeSecond;afterSecond.revision++;afterSecond.placement->anchor=15;
    swap.items.push_back({beforeSecond,afterSecond});Check(p::CommitItems(writer,*c,swap).has_value(),"full-cell swap committed");state=Load(writer,*c);
    // Typed account safebox transfer: account 42 is distinct from character 42.
    auto transfer=Move(*c,state,ten.id.Value(),0,22);transfer.owners.push_back({{ItemOwnerKind::Account,42},9,1});
    transfer.storages.push_back({{{ItemOwnerKind::Account,42},ItemStorageKind::Safebox,1},5,9,2});
    transfer.items[0].after.owner={ItemOwnerKind::Account,42};transfer.items[0].after.placement=ItemPlacement{ItemStorageKind::Safebox,1,0};
    Check(p::CommitItems(writer,*c,transfer).has_value(),"character to account transfer");
    Check(Load(writer,*c).items.size()==1 && Load(writer,*c,{ItemOwnerKind::Account,42},9).items[0].id==ten.id,"typed ownership reload");
    // Tampered or missing children are never replaced by empty defaults.
    const auto account=ItemOwner{ItemOwnerKind::Account,42};
    Query(admin,"UPDATE player.item_instance_attribute SET value_scaled=value_scaled+1 WHERE instance_id=? AND slot_index=0",{U::UInt(ten.id.Value())});
    auto corrupt=p::LoadItemOwner(writer,*c,account,9);Check(!corrupt && corrupt.error()==E::InvalidStoredData,"attribute checksum mismatch");
    Query(admin,"UPDATE player.item_instance_attribute SET value_scaled=value_scaled-1 WHERE instance_id=? AND slot_index=0",{U::UInt(ten.id.Value())});
    Query(admin,"DELETE FROM player.item_occupied_cell WHERE owner_kind=2 AND owner_id=42 AND cell_index=5");
    Check(!p::LoadItemOwner(writer,*c,account,9),"missing footprint cell rejected");
    Query(admin,"INSERT INTO player.item_occupied_cell VALUES(2,42,3,1,5,?)",{U::UInt(ten.id.Value())});
    Query(admin,"DELETE FROM player.item_instance_socket WHERE instance_id=? AND slot_index=2",{U::UInt(ten.id.Value())});
    Check(!p::LoadItemOwner(writer,*c,account,9),"missing empty socket row rejected");
    Query(admin,"INSERT INTO player.item_instance_socket VALUES(?,2,2,2,NULL,NULL,0)",{U::UInt(ten.id.Value())});
    Check(Load(writer,*c,account,9).items[0].stones==ten.stones,"repaired fixture reload");
    // The user's explicit cross-prototype case is durable too: +20 -> a new
    // weapon prototype at +0, with the same concrete ID, sockets and bonuses.
    state=Load(writer,*c);auto twenty=f::Sword(4);twenty.refinementLevel=20;
    auto grantTwenty=f::Grant(*c,{twenty});grantTwenty.operationId[0]=30;grantTwenty.owners={state.fence};
    Check(p::CommitItems(writer,*c,grantTwenty).has_value(),"level20 instance grant");state=Load(writer,*c);
    std::vector<RefinementItemSnapshot> inv;for(const auto& i:state.items)inv.push_back({i.id.Value(),i.revision,i.owner.id,i.key,i.refinementLevel,i.count,false});
    op[0]=31;auto transform=PrepareRefinement(*c,2,{op,42,state.wallets[0].revision,state.wallets[0].balance,true},
        {twenty.id.Value(),twenty.revision,42,twenty.key,20,1,false},inv,0);Check(transform.has_value(),"transformation plan");
    auto transformCommit=MakeRefinementCommit(*c,*transform,state.fence,state.storages,state.items,state.wallets[0]);Check(transformCommit.has_value(),"transformation write set");
    Check(p::CommitItems(writer,*c,*transformCommit).has_value(),"transformation committed");state=Load(writer,*c);
    auto transformed=Find(state,4);Check(transformed.key==f::LongSword() && transformed.refinementLevel==0 && transformed.attributes==twenty.attributes && transformed.stones==twenty.stones,"transformation complete reload");
    Check(state.wallets[0].balance==800,"transformation charged exactly once");
    auto attrTool=f::Leather(70);attrTool.key=attribute_fixture::Tool(2);attrTool.count=2;attrTool.placement->anchor=1;
    auto cleanTool=f::Leather(71);cleanTool.key=socket_service_fixture::Tool(1);cleanTool.count=1;cleanTool.placement->anchor=2;
    auto tools=f::Grant(*c,{attrTool,cleanTool});tools.operationId[0]=32;tools.owners={state.fence};
    Check(p::CommitItems(writer,*c,tools).has_value(),"typed bonus/socket service grants");state=Load(writer,*c);
    transformed=Find(state,4);AttributeRequest attribute{f::Pair(Find(state,70),transformed,state.fence,33)};attribute.state=transformed.attributes;attribute.draws={{1,1}};
    auto attributePlan=PlanAttributeService(*c,attribute);Check(attributePlan.has_value(),"real bonus reroll plan");
    auto attributeCommit=MakeAttributeCommit(*c,*attributePlan,state.fence,state.storages,state.items);Check(attributeCommit.has_value(),"real bonus write set");
    Check(p::CommitItems(writer,*c,*attributeCommit).has_value(),"bonus replacement and tool consumption committed");state=Load(writer,*c);
    Check(Find(state,4).attributes==attributePlan->after && Find(state,70).count==1,"bonus service full reload");
    const auto beforeReplay=Fingerprint(admin);replay=p::CommitItems(writer,*c,*attributeCommit);Check(replay && replay->replay && Fingerprint(admin)==beforeReplay,"bonus service replay spends no extra tool");
    SocketServiceRequest sockets{f::Pair(Find(state,71),Find(state,4),state.fence,34)};sockets.sockets=Find(state,4).stones;
    auto socketPlan=PlanSocketService(*c,sockets);Check(socketPlan.has_value(),"real broken socket cleaning plan");
    auto socketCommit=MakeSocketServiceCommit(*c,*socketPlan,state.fence,state.storages,state.items);Check(socketCommit.has_value(),"real socket write set");
    Check(p::CommitItems(writer,*c,*socketCommit).has_value(),"socket replacement+cleaner consumption committed");state=Load(writer,*c);
    Check(Find(state,4).stones==socketPlan->after && std::ranges::none_of(state.items,[](const auto& i){return i.id.Value()==71;}),"cleaner tombstone and socket replacement reload");
    // Session, engine, FK/check and catalog-pin guards fail closed.
    Check(!p::LoadItemOwner(writer,*c,account,10),"old session denied");
    Execute(admin,"UPDATE player.item_persistence_format SET catalog_revision=catalog_revision+1");Check(!p::LoadItemOwner(writer,*c,account,9),"wrong catalog pin denied");
    Execute(admin,"UPDATE player.item_persistence_format SET catalog_revision=catalog_revision-1");
    Execute(writer,"SET SESSION foreign_key_checks=0");Check(!p::LoadItemOwner(writer,*c,account,9),"FK-disabled connection denied");Execute(writer,"SET SESSION foreign_key_checks=1");
    Execute(writer,"SET SESSION check_constraint_checks=0");Check(!p::LoadItemOwner(writer,*c,account,9),"CHECK-disabled connection denied");Execute(writer,"SET SESSION check_constraint_checks=1");
    Execute(writer,"SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED");Check(!p::LoadItemOwner(writer,*c,account,9),"nonrepeatable read denied");Execute(writer,"SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    Check(Scalar(writer,"SELECT @@autocommit")=="1","transaction not left open");
    // C7d-1: full-state own-item drop/pickup/expiry through the real writer.
    {
        const ItemOwner dropper{ItemOwnerKind::Character,8101},picker{ItemOwnerKind::Character,8102};
        const ItemStorageLayout bag=f::Inventory(dropper),target=f::Inventory(picker);
        const ItemStorageLayout escrow{{dropper,ItemStorageKind::TradeEscrow,1},5,9,2};
        const ItemStorageLayout layouts[]{bag,escrow};
        Check(p::ProvisionOwner(admin,*c,dropper,layouts,{}).has_value(),"ground source provisioned");
        Check(p::ProvisionOwner(admin,*c,picker,std::span(&target,1),{}).has_value(),"ground picker provisioned");
        auto a=Load(writer,*c,dropper,1),b=Load(writer,*c,picker,1);
        auto sword=f::Sword(850001);sword.owner=dropper;
        auto leather=f::Leather(850002);leather.owner=dropper;leather.count=10;
        auto bracelet=leather;bracelet.id=ItemInstanceId::Create(850003).value();bracelet.key=item_catalog_fixture::Bracelet();
        bracelet.count=1;bracelet.placement->anchor=2;bracelet.accessory=AccessorySocketState{3,2,1000,false};
        auto give=f::Grant(*c,{sword,leather,bracelet});give.operationId.fill(0xD0);give.owners={a.fence};give.storages={bag};
        Check(p::CommitItems(writer,*c,give).has_value(),"ground source instances granted");a=Load(writer,*c,dropper,1);
        std::array<std::uint8_t,16> operation{};operation.fill(0xD1);
        auto drop=MakeGroundDropCommit(*c,operation,a.fence,a.storages,a.items,sword.id,{ItemStorageKind::TradeEscrow,1,0});
        Check(drop && p::CommitItems(writer,*c,*drop).has_value(),"own sword drop committed");
        auto replayDrop=p::CommitItems(writer,*c,*drop);Check(replayDrop && replayDrop->replay,"own drop replay is idempotent");
        a=Load(writer,*c,dropper,1);auto held=Find(a,sword.id.Value());
        auto expected=sword;expected.revision++;expected.placement=ItemPlacement{ItemStorageKind::TradeEscrow,1,0};
        Check(held==expected,"escrow reload preserves ID, level, signed bonuses and socket states");
        std::vector<ItemStorageLayout> both=a.storages;both.insert(both.end(),b.storages.begin(),b.storages.end());
        std::vector<PersistentItem> combined=a.items;combined.insert(combined.end(),b.items.begin(),b.items.end());
        operation.fill(0xD2);
        auto pickup=MakeGroundPickupCommit(*c,operation,a.fence,b.fence,both,combined,sword.id,{ItemStorageKind::Inventory,1,0});
        Check(pickup && p::CommitItems(writer,*c,*pickup).has_value(),"cross-owner pickup committed atomically");
        auto replayPickup=p::CommitItems(writer,*c,*pickup);Check(replayPickup && replayPickup->replay,"pickup replay never duplicates the instance");
        a=Load(writer,*c,dropper,1);b=Load(writer,*c,picker,1);
        expected.owner=picker;expected.revision++;expected.placement=ItemPlacement{ItemStorageKind::Inventory,1,0};
        Check(a.items.size()==2 && b.items.size()==1 && Find(b,sword.id.Value())==expected,"pickup reload moves the original identity and payload to the picker");
        auto stale=*pickup;stale.operationId.fill(0xD3);const auto fingerprint=Fingerprint(admin);
        Check(!p::CommitItems(writer,*c,stale) && Fingerprint(admin)==fingerprint,"stale pickup cannot debit or duplicate anything");
        operation.fill(0xD4);const auto splitId=ItemInstanceId::Create(850004).value();
        auto split=MakeGroundDropCommit(*c,operation,a.fence,a.storages,a.items,leather.id,{ItemStorageKind::TradeEscrow,1,1},4,splitId);
        Check(split && p::CommitItems(writer,*c,*split).has_value(),"partial drop commits the source debit and split together");
        a=Load(writer,*c,dropper,1);
        Check(Find(a,leather.id.Value()).count==6 && Find(a,splitId.Value()).count==4,"partial drop reload conserves units");
        operation.fill(0xD5);auto expire=MakeGroundExpireCommit(*c,operation,a.fence,a.storages,a.items,splitId);
        Check(expire && p::CommitItems(writer,*c,*expire).has_value(),"ground split expiry committed");
        a=Load(writer,*c,dropper,1);
        Check(a.items.size()==2 && Find(a,leather.id.Value()).count==6,"expiry only removes the escrow instance");
        operation.fill(0xD6);auto jewelry=MakeGroundDropCommit(*c,operation,a.fence,a.storages,a.items,bracelet.id,{ItemStorageKind::TradeEscrow,1,2});
        Check(jewelry && p::CommitItems(writer,*c,*jewelry).has_value(),"accessory drop committed");
        a=Load(writer,*c,dropper,1);
        Check(Find(a,bracelet.id.Value()).accessory==bracelet.accessory,"accessory lifetime and capacity survive SQL round trip");
        both=a.storages;both.insert(both.end(),b.storages.begin(),b.storages.end());
        combined=a.items;combined.insert(combined.end(),b.items.begin(),b.items.end());
        operation.fill(0xD7);
        auto jewelryPickup=MakeGroundPickupCommit(*c,operation,a.fence,b.fence,both,combined,bracelet.id,{ItemStorageKind::Inventory,1,1});
        Check(jewelryPickup && p::CommitItems(writer,*c,*jewelryPickup).has_value(),"accessory pickup committed");
        b=Load(writer,*c,picker,1);
        Check(Find(b,bracelet.id.Value()).accessory==bracelet.accessory,"accessory pickup preserves the original instance and lifetime");
        // C7d runtime registry: exact per-core crash cleanup and atomic links.
        Check(p::ProvisionGroundStorage(admin,*c,dropper).has_value(),"runtime escrow provisioned by importer");
        Check(p::ProvisionGroundStorage(admin,*c,picker).has_value(),"picker runtime escrow provisioned");
        Check(p::ProvisionGroundStorage(admin,*c,dropper).has_value(),"runtime escrow provisioning is idempotent");
        const auto coreA=p::BeginOwnedGroundRun(writer,*c,"test:core-A"),coreB=p::BeginOwnedGroundRun(writer,*c,"test:core-B");
        Check(coreA && coreB && *coreA==1 && *coreB==1,"independent core generations");
        a=p::LoadCurrentItemOwner(writer,*c,dropper).value();b=p::LoadCurrentItemOwner(writer,*c,picker).value();
        operation.fill(0xE1);
        auto registered=MakeGroundDropCommit(*c,operation,a.fence,a.storages,a.items,leather.id,{ItemStorageKind::TradeEscrow,OwnedGroundStorageId,0});
        Check(registered.has_value(),"runtime drop plan");
        registered->ground=ItemGroundChange{"test:core-A",*coreA,*coreA,leather.id.Value(),true};
        Check(p::CommitItems(writer,*c,*registered).has_value(),"runtime drop and registry committed together");
        Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_ground_entry WHERE core_key='test:core-A'")=="1","registered concrete instance");
        operation.fill(0xE2);
        auto otherCore=MakeGroundDropCommit(*c,operation,b.fence,b.storages,b.items,sword.id,{ItemStorageKind::TradeEscrow,OwnedGroundStorageId,0});
        Check(otherCore.has_value(),"second core drop plan");
        otherCore->ground=ItemGroundChange{"test:core-B",*coreB,*coreB,sword.id.Value(),true};
        Check(p::CommitItems(writer,*c,*otherCore).has_value(),"other live core ground instance");
        const auto restarted=p::BeginOwnedGroundRun(writer,*c,"test:core-A");
        Check(restarted && *restarted==2,"restart advances the core fence and cleans its old entries");
        Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_ground_entry WHERE core_key='test:core-A'")=="0","old process ground registry removed");
        Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_ground_entry WHERE core_key='test:core-B'")=="1","another live core ground untouched");
        a=p::LoadCurrentItemOwner(writer,*c,dropper).value();b=p::LoadCurrentItemOwner(writer,*c,picker).value();
        Check(std::ranges::find(a.items,leather.id,&PersistentItem::id)==a.items.end(),"old ground item expired without respawn");
        Check(Find(b,bracelet.id.Value()).accessory==bracelet.accessory,"restart cleanup leaves inventory intact");
        operation.fill(0xE3);
        auto staleCore=MakeGroundDropCommit(*c,operation,b.fence,b.storages,b.items,bracelet.id,{ItemStorageKind::TradeEscrow,OwnedGroundStorageId,1});
        Check(staleCore.has_value(),"stale process has otherwise valid proposal");
        staleCore->ground=ItemGroundChange{"test:core-A",*coreA,*coreA,bracelet.id.Value(),true};
        Check(!p::CommitItems(writer,*c,*staleCore),"old process cannot publish after replacement");
        Check(p::LoadCurrentItemOwner(writer,*c,picker)->fence==b.fence,"refused core write leaves owner fence unchanged");
        both=a.storages;both.insert(both.end(),b.storages.begin(),b.storages.end());combined=a.items;combined.insert(combined.end(),b.items.begin(),b.items.end());
        operation.fill(0xE4);
        auto registeredPickup=MakeGroundPickupCommit(*c,operation,b.fence,a.fence,both,combined,sword.id,{ItemStorageKind::Inventory,1,0});
        Check(registeredPickup.has_value(),"registered pickup across owners");
        registeredPickup->ground=ItemGroundChange{"test:core-B",*coreB,*coreB,sword.id.Value(),false};
        Check(p::CommitItems(writer,*c,*registeredPickup).has_value(),"pickup and registry deletion are atomic");
        Check(Scalar(admin,"SELECT COUNT(*) FROM player.item_ground_entry")=="0","picked up instance has no ground link");
        auto repeated=p::CommitItems(writer,*c,*registeredPickup);Check(repeated && repeated->replay,"registered pickup replay is idempotent");
        auto changed=*registeredPickup;changed.ground->core="test:core-A";
        Check(p::CommitItems(writer,*c,changed).error()==ItemPersistenceError::ReceiptMismatch,"ground context participates in receipt hash");
        auto final=p::LoadCurrentItemOwner(writer,*c,dropper).value();
        Check(Find(final,sword.id.Value()).attributes==sword.attributes && Find(final,sword.id.Value()).stones==sword.stones,"runtime registered pickup preserves complete payload");

    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880082};
        const auto bag=f::Inventory(customer);
        const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"NPC customer provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        const auto block=p::ReserveItemInstanceIds(writer,*c,32).value();
        std::vector<ItemInstanceId> ids;for(auto i=block.first;i<block.end;++i)ids.push_back(ItemInstanceId::Create(i).value());
        const NpcShopOffer offer{1,{f::Sword().key,0,1}};
        std::array<std::uint8_t,16> op{};op.fill(0xB1);
        auto buy=MakeNpcBuyCommit(*c,op,state.fence,state.storages,state.items,state.wallets,offer,3,ids);
        Check(buy && p::CommitItems(writer,*c,*buy).has_value(),"NPC purchase writes three swords and payment together");
        auto replay=p::CommitItems(writer,*c,*buy);Check(replay && replay->replay,"purchase replay is idempotent");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(state.items.size()==3 && state.wallets[0].balance==700,"purchase reload has exact quantity and cost");
        op.fill(0xB2);auto invalid=MakeNpcBuyCommit(*c,op,state.fence,state.storages,state.items,state.wallets,offer,1,std::span(ids).subspan(3));
        Check(invalid.has_value(),"second purchase plan");
        ++invalid->wallets[0].before.revision;++invalid->wallets[0].after.revision;
        Check(!p::CommitItems(writer,*c,*invalid),"wallet race rolls purchase back");
        auto untouched=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(untouched.items==state.items && untouched.wallets==state.wallets && untouched.fence==state.fence,"failed payment leaves no free item");
        op.fill(0xB3);auto sell=MakeNpcSellCommit(*c,op,state.fence,state.storages,state.items,state.wallets,state.items[0].id,0);
        Check(sell && p::CommitItems(writer,*c,*sell).has_value(),"NPC sale writes tombstone and credit together");
        replay=p::CommitItems(writer,*c,*sell);Check(replay && replay->replay,"sale replay does not double credit");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(state.items.size()==2 && state.wallets[0].balance==750,"sale reload has exact credit and remaining instances");
        op.fill(0xB4);auto stale=MakeNpcSellCommit(*c,op,state.fence,state.storages,state.items,state.wallets,state.items[0].id,0);
        Check(stale.has_value() && p::ClaimOwnerSession(writer,*c,customer).has_value(),"relogin fences pending sale");
        Check(!p::CommitItems(writer,*c,*stale),"old session cannot sell");
        auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(after.items==state.items && after.wallets==state.wallets,"stale sale changes neither items nor money");
    }
    {
        const ItemOwner left{ItemOwnerKind::Character,880083},right{ItemOwnerKind::Character,880084};
        const auto leftBag=f::Inventory(left),rightBag=f::Inventory(right);
        const ItemWallet leftWallet{left,WalletCurrency::Gold,1,1000},rightWallet{right,WalletCurrency::Gold,1,500};
        Check(p::ProvisionOwner(admin,*c,left,std::span(&leftBag,1),std::span(&leftWallet,1)).has_value(),"trade left provisioned");
        Check(p::ProvisionOwner(admin,*c,right,std::span(&rightBag,1),std::span(&rightWallet,1)).has_value(),"trade right provisioned");
        auto a=p::LoadCurrentItemOwner(writer,*c,left).value(),b=p::LoadCurrentItemOwner(writer,*c,right).value();
        const auto block=p::ReserveItemInstanceIds(writer,*c,32).value();
        auto sword=f::Sword(block.first);sword.owner=left;
        auto leather=f::Leather(block.first+1);leather.owner=right;leather.count=10;leather.placement->anchor=0;
        std::array<std::uint8_t,16> op{};op[0]='T';op[1]='R';op[15]=1;
        ItemCommit grant;grant.operationId=op;grant.catalogRevision=c->Revision();grant.catalogHash=c->ContentHash();grant.kind=ItemOperationKind::Grant;
        grant.owners={a.fence,b.fence};grant.storages={leftBag,rightBag};grant.items={{{},sword},{{},leather}};grant.outcomeCode=1;grant.result={1};
        Check(p::CommitItems(writer,*c,grant).has_value(),"complete trade fixtures granted");
        a=p::LoadCurrentItemOwner(writer,*c,left).value();b=p::LoadCurrentItemOwner(writer,*c,right).value();
        std::vector<ItemStorageLayout> layouts{leftBag,rightBag};auto items=a.items;items.insert(items.end(),b.items.begin(),b.items.end());
        auto wallets=a.wallets;wallets.insert(wallets.end(),b.wallets.begin(),b.wallets.end());
        std::array<PlayerTradeOffer,2> offers{{{a.fence,{{sword.id,0}},100},{b.fence,{{leather.id,4}},40}}};
        const std::vector<ItemInstanceId> ids{ItemInstanceId::Create(block.first+2).value()};op[15]=2;
        auto trade=MakePlayerTradeCommit(*c,op,offers,layouts,items,wallets,ids);
        Check(trade && p::CommitItems(writer,*c,*trade).has_value(),"both item directions and wallets commit together");
        auto replay=p::CommitItems(writer,*c,*trade);Check(replay && replay->replay,"trade receipt replay writes once");
        a=p::LoadCurrentItemOwner(writer,*c,left).value();b=p::LoadCurrentItemOwner(writer,*c,right).value();
        Check(a.wallets[0].balance==940 && b.wallets[0].balance==560,"net trade gold persisted");
        Check(Find(a,ids[0].Value()).count==4 && Find(b,leather.id.Value()).count==6,"partial trade persisted without regeneration");
        Check(Find(b,sword.id.Value()).attributes==sword.attributes && Find(b,sword.id.Value()).stones==sword.stones && Find(b,sword.id.Value()).refinementLevel==9,"whole trade preserves high ID and complete instance state");
        offers={PlayerTradeOffer{a.fence,{{ids[0],0}},10},PlayerTradeOffer{b.fence,{{sword.id,0}},0}};
        items=a.items;items.insert(items.end(),b.items.begin(),b.items.end());wallets=a.wallets;wallets.insert(wallets.end(),b.wallets.begin(),b.wallets.end());op[15]=3;
        auto raced=MakePlayerTradeCommit(*c,op,offers,layouts,items,wallets,{});Check(raced.has_value(),"second trade planned");
        ++raced->wallets[1].before.revision;++raced->wallets[1].after.revision;
        Check(!p::CommitItems(writer,*c,*raced),"one wallet race rejects the entire trade");
        auto unchangedA=p::LoadCurrentItemOwner(writer,*c,left).value(),unchangedB=p::LoadCurrentItemOwner(writer,*c,right).value();
        Check(unchangedA.items==a.items && unchangedA.wallets==a.wallets && unchangedA.fence==a.fence && unchangedB.items==b.items && unchangedB.wallets==b.wallets && unchangedB.fence==b.fence,"failed trade cannot debit, credit or transfer either side");
        op[15]=4;auto stale=MakePlayerTradeCommit(*c,op,offers,layouts,items,wallets,{});
        Check(stale && p::ClaimOwnerSession(writer,*c,left).has_value(),"relogin advances one trade fence");
        Check(!p::CommitItems(writer,*c,*stale),"stale trade session cannot write");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880085};const auto bag=f::Inventory(customer);
        const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"craft owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,32).value();
        auto material=f::Leather(block.first);material.owner=customer;material.count=20;material.placement->anchor=0;
        std::array<std::uint8_t,16> op{};op[0]='C';op[1]='R';op[15]=1;
        ItemCommit grant;grant.operationId=op;grant.catalogRevision=c->Revision();grant.catalogHash=c->ContentHash();grant.kind=ItemOperationKind::Grant;grant.owners={state.fence};grant.storages={bag};grant.items={{{},material}};grant.outcomeCode=1;grant.result={1};
        Check(p::CommitItems(writer,*c,grant).has_value(),"craft materials granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        CraftRecipe recipe{7,"Iron sword",{f::Sword().key,9,1},25,5000,{{material.key,0,2}}};
        const std::vector<std::uint16_t> rolls{0,4999,5000};const std::vector<ItemInstanceId> ids{ItemInstanceId::Create(block.first+1).value(),ItemInstanceId::Create(block.first+2).value(),ItemInstanceId::Create(block.first+3).value()};op[15]=2;
        auto craft=MakeCraftCommit(*c,op,state.fence,state.storages,state.items,state.wallets,recipe,3,rolls,ids);
        Check(craft && p::CommitItems(writer,*c,craft->commit).has_value(),"mixed craft batch writes inputs, output and payment atomically");
        auto replay=p::CommitItems(writer,*c,craft->commit);Check(replay && replay->replay,"craft receipt replay does not reroll or pay twice");
        auto changed=craft->commit;changed.result.back()^=1;Check(!p::CommitItems(writer,*c,changed),"same operation cannot change recorded rolls");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(state.wallets[0].balance==925 && Find(state,material.id.Value()).count==14 && Find(state,ids[0].Value()).refinementLevel==9 && Find(state,ids[1].Value()).refinementLevel==9,"craft reload retains exact identities, level, remaining inputs and gold");
        op[15]=3;auto raced=MakeCraftCommit(*c,op,state.fence,state.storages,state.items,state.wallets,recipe,1,std::span(rolls).first(1),std::span(ids).subspan(2));Check(raced.has_value(),"second craft planned");
        ++raced->commit.wallets[0].before.revision;++raced->commit.wallets[0].after.revision;
        Check(!p::CommitItems(writer,*c,raced->commit),"craft wallet race rolls all writes back");const auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(after.items==state.items && after.wallets==state.wallets && after.fence==state.fence,"no free output or missing inputs after failed payment");
        op[15]=4;recipe.chance=0;auto failure=MakeCraftCommit(*c,op,state.fence,state.storages,state.items,state.wallets,recipe,1,std::span(rolls).first(1),{});
        Check(failure && p::CommitItems(writer,*c,failure->commit).has_value(),"failed craft pays and consumes together");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(state.items.size()==3 && Find(state,material.id.Value()).count==12 && state.wallets[0].balance==900,"failed roll creates no output");
        op[15]=5;auto stale=MakeCraftCommit(*c,op,state.fence,state.storages,state.items,state.wallets,recipe,1,std::span(rolls).first(1),{});
        Check(stale && p::ClaimOwnerSession(writer,*c,customer).has_value(),"relogin fences previous craft");Check(!p::CommitItems(writer,*c,stale->commit),"stale craft cannot consume");
    }
    // Item-only Mission delivery: the immutable action receipt and all costs /
    // rewards share the native item transaction, including rollback and relog.
    {
        const ItemOwner customer{ItemOwnerKind::Character,8100012};auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"mission owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,32).value();
        const auto materialKey=f::Leather().key;const auto swordKey=f::Sword().key;
        const auto grant=MakeGrantCommit(*c,{12},state.fence,state.storages,state.items,{materialKey,0,10},std::array{ItemInstanceId::Create(block.first).value()});
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"mission source granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        ItemMission mission{7,1,"mission.sql_delivery","Delivery","Bring ore",5,{{materialKey,0,10}},{{materialKey,0,4}},{{swordKey,9,1},{materialKey,0,2}},25,100};
        ItemMissionProof proof{customer,{}};proof.action[0]=12;proof.action[31]=1;
        const auto none=p::FindItemMissionReceipt(writer,*c,state.fence,mission,proof);Check(none && !*none,"mission initially uncompleted");
        const auto id=ItemInstanceId::Create(block.first+1).value();const auto plan=MakeItemMissionCommit(*c,mission,proof,5,state.fence,state.storages,state.items,state.wallets,std::array{id});
        Check(plan && p::CommitItems(writer,*c,*plan).has_value(),"mission writes whole item and wallet bundle");
        const auto source=state.items[0].id;state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(Find(state,source.Value()).count==8 && Find(state,id.Value()).refinementLevel==9 && state.wallets[0].balance==1075,"mission result reload retains exact source, result and wallet");
        Check(CountMissionItems(state.items,customer,materialKey,0)==8 && !MakeItemMissionCommit(*c,mission,proof,5,state.fence,state.storages,state.items,state.wallets,{}),"original requirements no longer satisfied after payment");
        const auto replay=p::FindItemMissionReceipt(writer,*c,state.fence,mission,proof);Check(replay && *replay,"historical receipt succeeds without rebuilding spent inputs");
        auto changed=mission;changed.goldReward++;Check(!p::FindItemMissionReceipt(writer,*c,state.fence,changed,proof),"same mission identity cannot change its economic contract");
        auto wrong=proof;wrong.owner.id++;Check(!p::FindItemMissionReceipt(writer,*c,state.fence,mission,wrong),"foreign mission receipt denied");
        changed=mission;changed.version=2;changed.requirements.clear();auto secondProof=proof;secondProof.action[31]=2;
        auto raced=MakeItemMissionCommit(*c,changed,secondProof,5,state.fence,state.storages,state.items,state.wallets,std::array{ItemInstanceId::Create(block.first+2).value()});
        Check(raced.has_value(),"second mission planned");++raced->wallets[0].before.revision;++raced->wallets[0].after.revision;
        Check(!p::CommitItems(writer,*c,*raced),"mission payment CAS rejects whole write");const auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(after.items==state.items && after.wallets==state.wallets && after.fence==state.fence,"failed mission neither consumes nor rewards");
        const auto absent=p::FindItemMissionReceipt(writer,*c,state.fence,changed,secondProof);Check(absent && !*absent,"failed mission has no completion receipt");
        auto stale=MakeItemMissionCommit(*c,changed,secondProof,5,state.fence,state.storages,state.items,state.wallets,std::array{ItemInstanceId::Create(block.first+3).value()});
        Check(stale && p::ClaimOwnerSession(writer,*c,customer).has_value(),"mission relog creates new owner fence");
        Check(!p::CommitItems(writer,*c,*stale) && !p::FindItemMissionReceipt(writer,*c,state.fence,mission,proof),"old login cannot mutate or authorize mission completion");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto restored=p::FindItemMissionReceipt(writer,*c,state.fence,mission,proof);
        Check(restored && *restored && state.wallets[0].balance==1075 && Find(state,source.Value()).count==8,"new login restores durable completion without another reward");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880086};const auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"container owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,32).value();
        const auto boxId=ItemInstanceId::Create(block.first).value(),keyId=ItemInstanceId::Create(block.first+1).value();
        std::array<std::uint8_t,16> op{};op[0]='B';op[1]='X';op[15]=1;
        auto grant=MakeGrantCommit(*c,op,state.fence,state.storages,state.items,{sqlBox,0,2},std::array{boxId});
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"two SQL chests granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();op[15]=2;
        grant=MakeGrantCommit(*c,op,state.fence,state.storages,state.items,{item_catalog_fixture::IronKey(),0,2},std::array{keyId});
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"two SQL keys granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();op[15]=3;
        const auto resultId=ItemInstanceId::Create(block.first+2).value();ContainerOpeningRequest request{boxId,keyId,{100,0,0,false}};
        auto opening=MakeContainerOpeningCommit(*c,op,state.fence,state.storages,state.items,state.wallets,request,std::array{resultId},[](auto){return 0;});
        Check(opening && p::CommitItems(writer,*c,opening->commit).has_value(),"container consumes key and box and awards whole item/gold bundle");
        const auto replay=p::CommitItems(writer,*c,opening->commit);Check(replay && replay->replay,"container receipt replay does not draw or consume again");
        auto changed=opening->commit;changed.result.back()^=1;Check(!p::CommitItems(writer,*c,changed),"recorded container result cannot be changed on replay");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(Find(state,boxId.Value()).count==1 && Find(state,keyId.Value()).count==1 && Find(state,resultId.Value()).count==3 && state.wallets[0].balance==1075,"container reload preserves precise inputs, result ID and money");
        op[15]=4;auto raced=MakeContainerOpeningCommit(*c,op,state.fence,state.storages,state.items,state.wallets,request,{},[](auto){return 0;});Check(raced.has_value(),"last container opening planned");
        ++raced->commit.wallets[0].before.revision;++raced->commit.wallets[0].after.revision;
        Check(!p::CommitItems(writer,*c,raced->commit),"container wallet CAS rolls back whole opening");
        auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(after.items==state.items && after.wallets==state.wallets && after.fence==state.fence,"failed opening preserves key, box, items and wallet");
        const auto missing=p::FindItemReceipt(writer,*c,raced->commit);Check(missing && !*missing,"failed container opening has no success receipt");
        auto stale=MakeContainerOpeningCommit(*c,op,state.fence,state.storages,state.items,state.wallets,request,{},[](auto){return 0;});Check(stale && p::ClaimOwnerSession(writer,*c,customer).has_value(),"container relog advances owner fence");
        Check(!p::CommitItems(writer,*c,stale->commit),"old login cannot open another container");after=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(after.items==state.items && after.wallets==state.wallets,"container result survives session change");
    }
    // Token stack consumption, wallet credit and receipt have one CAS boundary.
    for(bool nearCap:{false,true})
    {
        const ItemOwner customer{ItemOwnerKind::Character,nearCap ? 880088u : 880087u};const auto bag=f::Inventory(customer);
        const ItemWallet wallet{customer,WalletCurrency::Gold,1,nearCap ? std::uint64_t(INT64_MAX)-12000000 : 1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"token owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,1).value();const auto id=ItemInstanceId::Create(block.first).value();
        std::array<std::uint8_t,16> op{};op[0]='T';op[1]=nearCap ? 2 : 1;op[15]=1;
        auto grant=MakeGrantCommit(*c,op,state.fence,state.storages,state.items,{item_catalog_fixture::GoldBar(),0,10},std::array{id});
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"token stack granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();op[15]=2;
        auto redeem=MakeTokenRedemptionCommit(*c,op,state.fence,state.storages,state.items,state.wallets,id,nearCap ? 10 : 3,INT64_MAX,{100,0,0,false});
        Check(redeem && p::CommitItems(writer,*c,redeem->commit).has_value(),"token debit and credit commit together");
        const auto replay=p::CommitItems(writer,*c,redeem->commit);Check(replay && replay->replay,"token receipt replay cannot consume or credit again");
        auto changed=redeem->commit;changed.result.back()^=1;Check(!p::CommitItems(writer,*c,changed),"token receipt cannot change its economic result");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(Find(state,id.Value()).count==(nearCap ? 8 : 7) && Find(state,id.Value()).revision==2 && state.wallets[0].balance==(nearCap ? std::uint64_t(INT64_MAX)-2000000 : 15001000),"reload retains exact remainder, revision and wallet");
        op[15]=3;
        if(nearCap)
        {
            Check(MakeTokenRedemptionCommit(*c,op,state.fence,state.storages,state.items,state.wallets,id,8,INT64_MAX,{100,0,0,false}).error()==TokenRedemptionError::WalletFull,"partial redemption leaves too little room for a whole remaining token");
        }
        else
        {
            auto raced=MakeTokenRedemptionCommit(*c,op,state.fence,state.storages,state.items,state.wallets,id,1,INT64_MAX,{100,0,0,false});Check(raced.has_value(),"second token operation planned");
            ++raced->commit.wallets[0].before.revision;++raced->commit.wallets[0].after.revision;
            Check(!p::CommitItems(writer,*c,raced->commit),"wallet CAS rolls token consumption back");
            const auto missing=p::FindItemReceipt(writer,*c,raced->commit);Check(missing && !*missing,"failed redemption has no success receipt");
            const auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(after.items==state.items && after.wallets==state.wallets && after.fence==state.fence,"failed redemption preserves every item and wallet before image");
            auto stale=MakeTokenRedemptionCommit(*c,op,state.fence,state.storages,state.items,state.wallets,id,1,INT64_MAX,{100,0,0,false});
            Check(stale && p::ClaimOwnerSession(writer,*c,customer).has_value(),"token relog advances session fence");Check(!p::CommitItems(writer,*c,stale->commit),"old login cannot redeem tokens");
        }
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880089};const auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"socket maintenance owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,6).value();
        auto target=f::Sword(block.first);target.owner=customer;target.stones[2]={StoneSocketState::Occupied,2,socket_service_fixture::Stone(2),0};
        auto tool=f::Leather(block.first+1);tool.owner=customer;tool.key=socket_service_fixture::Tool(2);tool.count=3;
        ItemCommit grant;grant.operationId={15};grant.catalogRevision=c->Revision();grant.catalogHash=c->ContentHash();grant.kind=ItemOperationKind::Grant;grant.owners={state.fence};grant.storages={bag};grant.items={{{},target},{{},tool}};grant.outcomeCode=1;
        Check(p::CommitItems(writer,*c,grant).has_value(),"socket fixture with exact embedded levels granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        const std::array outputs{ItemInstanceId::Create(block.first+2).value(),ItemInstanceId::Create(block.first+3).value()};
        auto plan=MakeSocketMaintenanceCommit(*c,{15,1},state.fence,state.storages,state.items,tool.id,target.id,{100,1,1,false},outputs);Check(plan.has_value(),"socket extraction planned");
        auto raced=plan->commit;++raced.items[1].before->revision;++raced.items[1].after.revision;
        Check(!p::CommitItems(writer,*c,raced),"target CAS failure rolls back source and every stone");
        const auto absent=p::FindItemReceipt(writer,*c,raced);Check(absent && !*absent,"failed extraction has no receipt");
        auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(after.items==state.items && after.wallets==state.wallets && after.fence==state.fence,"failed extraction preserves entire owner state");
        Check(p::CommitItems(writer,*c,plan->commit).has_value(),"source, full socket replacement and all return instances share one SQL write");
        const auto replay=p::CommitItems(writer,*c,plan->commit);Check(replay && replay->replay,"socket receipt replay cannot extract twice");
        auto changed=plan->commit;changed.items[2].after.refinementLevel=19;Check(!p::CommitItems(writer,*c,changed),"same operation cannot alter returned level");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(state.items.size()==4 && Find(state,tool.id.Value()).count==2 && Find(state,target.id.Value()).attributes==target.attributes && Find(state,target.id.Value()).stones==plan->quote.after && Find(state,outputs[0].Value()).refinementLevel==20 && Find(state,outputs[1].Value()).refinementLevel==0 && state.wallets[0].balance==1000,"all identities, grades, attributes, levels and wallet reload exactly");
        auto clean=MakeSocketMaintenanceCommit(*c,{15,2},state.fence,state.storages,state.items,tool.id,target.id,{100,1,1,false},outputs);Check(!clean && clean.error()==SocketMaintenanceStatus::NoEffect,"already extracted stones cannot be returned again");
        // Replace the extraction tool with a separately granted cleaning tool,
        // then verify an old login cannot clean the remaining broken slots.
        auto replace=MakeConsumeCommit(*c,{15,3},state.fence,state.storages,state.items,tool.id,2);Check(replace && p::CommitItems(writer,*c,*replace).has_value(),"spent extraction tool removed");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto add=MakeGrantCommit(*c,{15,4},state.fence,state.storages,state.items,{socket_service_fixture::Tool(1),0,1},std::array{ItemInstanceId::Create(block.first+4).value()});Check(add && p::CommitItems(writer,*c,*add).has_value(),"cleaning tool granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto stale=MakeSocketMaintenanceCommit(*c,{15,5},state.fence,state.storages,state.items,ItemInstanceId::Create(block.first+4).value(),target.id,{100,1,1,false},{});
        Check(stale && p::ClaimOwnerSession(writer,*c,customer).has_value(),"socket relog advances owner fence");Check(!p::CommitItems(writer,*c,stale->commit),"previous login cannot clean or consume");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880090};const auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"insertion owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,2).value();
        auto target=f::Sword(block.first);target.owner=customer;target.stones={StoneSocketSlot{StoneSocketState::Empty,1},StoneSocketSlot{StoneSocketState::Empty,2},StoneSocketSlot{}};
        auto stone=f::Leather(block.first+1);stone.owner=customer;stone.key=socket_service_fixture::Stone();stone.refinementLevel=20;stone.count=3;
        ItemCommit grant;grant.operationId={16};grant.catalogRevision=c->Revision();grant.catalogHash=c->ContentHash();grant.kind=ItemOperationKind::Grant;grant.owners={state.fence};grant.storages={bag};grant.items={{{},target},{{},stone}};grant.outcomeCode=1;
        Check(p::CommitItems(writer,*c,grant).has_value(),"insertion source and target granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto failure=MakeSocketInsertionCommit(*c,{16,1},state.fence,state.storages,state.items,stone.id,target.id,{100,1,1,false},0,0);Check(failure && !failure->succeeded,"failure endpoint plans a consuming transaction");
        auto raced=failure->commit;++raced.items[1].before->revision;++raced.items[1].after.revision;Check(!p::CommitItems(writer,*c,raced),"target race rolls source debit and socket result back");
        auto after=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(after.items==state.items && after.fence==state.fence && after.wallets==state.wallets,"failed CAS changes no owner data");const auto absent=p::FindItemReceipt(writer,*c,raced);Check(absent && !*absent,"failed CAS has no outcome receipt");
        Check(p::CommitItems(writer,*c,failure->commit).has_value(),"rolled failure atomically consumes stone and breaks one socket");
        const auto replay=p::CommitItems(writer,*c,failure->commit);Check(replay && replay->replay,"failed-roll replay cannot reroll or consume another stone");auto changed=failure->commit;changed.result[6]=1;Check(!p::CommitItems(writer,*c,changed),"recorded insertion roll cannot be changed on replay");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(state,stone.id.Value()).count==2 && Find(state,target.id.Value()).stones[0]==StoneSocketSlot{StoneSocketState::Broken,1} && Find(state,target.id.Value()).attributes==target.attributes && state.wallets[0].balance==1000,"failed roll reload has exact stack, grade, bonuses and money");
        auto stale=MakeSocketInsertionCommit(*c,{16,2},state.fence,state.storages,state.items,stone.id,target.id,{100,1,1,false},10000,0);Check(stale && p::ClaimOwnerSession(writer,*c,customer).has_value(),"relog fences pending insertion");Check(!p::CommitItems(writer,*c,stale->commit),"old login cannot insert into next socket");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto success=MakeSocketInsertionCommit(*c,{16,3},state.fence,state.storages,state.items,stone.id,target.id,{100,1,1,false},10000,0);Check(success && success->succeeded && p::CommitItems(writer,*c,success->commit).has_value(),"success endpoint writes source and embedded stone together");
        state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(state,stone.id.Value()).count==1 && Find(state,stone.id.Value()).revision==3 && Find(state,target.id.Value()).stones[0].state==StoneSocketState::Broken && Find(state,target.id.Value()).stones[1]==StoneSocketSlot{StoneSocketState::Occupied,2,stone.key,20} && Find(state,target.id.Value()).revision==3 && state.wallets[0].balance==1000,"success skips broken socket, keeps gold quality and +20 embedded level after reload");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880091};const auto bag=f::Inventory(customer);const ItemStorageLayout equipment{{customer,ItemStorageKind::Equipment,1},8,1,1};const std::array layouts{bag,equipment};const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,layouts,std::span(&wallet,1)).has_value(),"accessory owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,3).value();
        auto target=f::Leather(block.first);target.owner=customer;target.key=item_catalog_fixture::Bracelet();target.refinementLevel=15;target.count=1;target.placement->anchor=0;target.accessory=AccessorySocketState{};
        auto drill=f::Leather(block.first+1);drill.owner=customer;drill.key=item_catalog_fixture::SocketDrill();drill.count=3;
        auto jewel=f::Leather(block.first+2);jewel.owner=customer;jewel.key=item_catalog_fixture::SocketJewel();jewel.count=3;jewel.placement->anchor=2;
        ItemCommit grant;grant.operationId={17};grant.catalogRevision=c->Revision();grant.catalogHash=c->ContentHash();grant.kind=ItemOperationKind::Grant;grant.owners={state.fence};grant.storages={bag};grant.items={{{},target},{{},drill},{{},jewel}};grant.outcomeCode=1;
        Check(p::CommitItems(writer,*c,grant).has_value(),"socket accessory and explicit materials granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        const auto plan=[&](unsigned op,ItemInstanceId material,AccessoryServiceKind kind,std::uint16_t roll){return MakeAccessorySocketCommit(*c,{17,static_cast<std::uint8_t>(op)},state.fence,state.storages,state.items,material,target.id,{100,1,1,false},kind,roll);};
        auto unlock=plan(1,drill.id,AccessoryServiceKind::Unlock,5000);Check(unlock && p::CommitItems(writer,*c,unlock->commit).has_value(),"unlock writes material debit and capacity atomically");
        Check(p::CommitItems(writer,*c,unlock->commit)->replay,"unlock receipt prevents double capacity or debit");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto failure=plan(2,drill.id,AccessoryServiceKind::Unlock,5001);Check(failure && !failure->succeeded && p::CommitItems(writer,*c,failure->commit).has_value(),"failed unlock consumes material with unchanged capacity");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(Find(state,drill.id.Value()).count==1 && Find(state,target.id.Value()).accessory==AccessorySocketState{1,0,0,false},"failed unlock reloads exact prior state");
        auto charge=plan(3,jewel.id,AccessoryServiceKind::Charge,0);Check(charge && p::CommitItems(writer,*c,charge->commit).has_value(),"guaranteed charge is persisted once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto equip=MakeEquipCommit(*c,{17,4},state.fence,state.storages,state.items,target.id,{100,1,1});Check(equip && p::CommitItems(writer,*c,*equip).has_value(),"charged accessory equips");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        const auto duration=Find(state,target.id.Value()).accessory->remainingMs;
        auto unequip=MakeUnequipCommit(*c,{17,5},state.fence,state.storages,state.items,target.id,{ItemStorageKind::Inventory,1,0});Check(unequip && SettleAccessoryTime(*c,*unequip,state.fence,state.storages,state.items,1234).has_value(),"unequip settles elapsed in its own transaction");
        auto raced=*unequip;++raced.items[0].before->revision;++raced.items[0].after.revision;Check(!p::CommitItems(writer,*c,raced),"timer CAS race rolls complete move back");
        Check(p::CommitItems(writer,*c,*unequip).has_value() && p::CommitItems(writer,*c,*unequip)->replay,"elapsed debit and movement commit once across replay");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(Find(state,target.id.Value()).accessory->remainingMs==duration-1234 && Find(state,target.id.Value()).placement->kind==ItemStorageKind::Inventory && state.wallets[0].balance==1000,"remaining equipped time survives reload and wallet is unchanged");
        Check(p::ClaimOwnerSession(writer,*c,customer).has_value(),"accessory relog claims next session");auto relog=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(relog,target.id.Value()).accessory==Find(state,target.id.Value()).accessory,"offline state neither refills nor consumes time");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880092};const auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"attribute owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,5).value();
        auto target=f::Sword(block.first);target.owner=customer;target.attributes=attribute_runtime_fixture::Full();
        std::array<PersistentItem,4> tools{f::Leather(block.first+1),f::Leather(block.first+2),f::Leather(block.first+3),f::Leather(block.first+4)};
        const std::array kinds{3u,2u,4u,5u};for(unsigned i=0;i<tools.size();++i){tools[i].owner=customer;tools[i].key=attribute_fixture::Tool(kinds[i]);tools[i].count=3;tools[i].placement->anchor=i+1;}
        ItemCommit grant;grant.operationId={18};grant.catalogRevision=c->Revision();grant.catalogHash=c->ContentHash();grant.kind=ItemOperationKind::Grant;grant.owners={state.fence};grant.storages={bag};grant.items={{{},target}};for(const auto& tool:tools)grant.items.push_back({{},tool});grant.outcomeCode=1;
        Check(p::CommitItems(writer,*c,grant).has_value(),"full bonus target and four service tools granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        const auto make=[&](unsigned operation,unsigned tool,const AttributeRandom& random){const auto quote=PrepareAttributeRuntime(*c,state.fence,state.items,tools[tool].id,target.id,{100,1,1,false});Check(quote.has_value(),"SQL attribute preview admitted");return MakeAttributeRuntimeCommit(*c,{18,static_cast<std::uint8_t>(operation)},state.fence,state.storages,state.items,tools[tool].id,target.id,{100,1,1,false},*quote,random);};
        auto lock=make(1,0,[](auto max){return max;});Check(lock && lock->after.lockedSlot==4,"recorded lock proposal chooses exact slot");auto raced=lock->commit;++raced.items[0].before->revision;++raced.items[0].after.revision;
        Check(!p::CommitItems(writer,*c,raced),"attribute target CAS race refuses whole transaction");auto unchanged=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(unchanged.items==state.items && unchanged.fence==state.fence,"refused lock cannot consume or change protection");
        Check(p::CommitItems(writer,*c,lock->commit).has_value() && p::CommitItems(writer,*c,lock->commit)->replay,"lock and material debit commit once");auto changed=lock->commit;changed.result[6]=1;Check(!p::CommitItems(writer,*c,changed),"receipt rejects a different recorded lock decision");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto reroll=make(2,1,[](auto max){return max;});Check(reroll && p::CommitItems(writer,*c,reroll->commit).has_value(),"reroll replacement persists atomically");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(Find(state,target.id.Value()).attributes==reroll->after && Find(state,target.id.Value()).attributes.slots[4]==target.attributes.slots[4] && Find(state,target.id.Value()).attributes.lockedSlot==4 && Find(state,target.id.Value()).stones==target.stones && Find(state,target.id.Value()).refinementLevel==9,"full reload retains protected value, all sockets and level");
        auto relock=make(3,2,[](auto){return 1;});Check(relock && relock->after.lockedSlot==0 && p::CommitItems(writer,*c,relock->commit).has_value(),"relock changes only protected index");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto unlock=make(4,3,[](auto)->std::optional<std::uint32_t>{throw std::runtime_error("unlock must not draw");});Check(unlock && p::CommitItems(writer,*c,unlock->commit).has_value(),"unlock writes without entropy");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(!Find(state,target.id.Value()).attributes.lockedSlot && Find(state,target.id.Value()).attributes.slots==reroll->after.slots && state.wallets[0].balance==1000,"unlock leaves every bonus and Gold unchanged");
        for(const auto& tool:tools)Check(Find(state,tool.id.Value()).count==2,"each service consumes one exact tool");Check(p::ClaimOwnerSession(writer,*c,customer).has_value(),"attribute relog claims next generation");auto next=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(next,target.id.Value()).attributes==Find(state,target.id.Value()).attributes,"complete bonus state survives session change");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880093};const auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"recovery owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        Check(state.recovery==RecoverySchedule{},"missing sidecar is empty");const auto block=p::ReserveItemInstanceIds(writer,*c,1).value();
        const auto grant=MakeGrantCommit(*c,{19},state.fence,state.storages,state.items,{item_catalog_fixture::ReservePotion(),0,3},std::array{ItemInstanceId::Create(block.first).value()});
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"recovery source granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto source=state.items[0].id;
        InstantRecoveryAdmission actor;actor.pools={500000,2000000,200000,1000000};actor.context=RecoveryContext::World;actor.level=100;actor.availableUnits=3;
        auto plan=PlanScheduledRecoveryUse(*c,state.items[0].key,actor,state.recovery,0,{1,2});Check(plan.has_value(),"recovery schedule admitted");
        auto use=MakeConsumeCommit(*c,{19,1},state.fence,state.storages,state.items,source,plan->units).value();use.recovery=ItemRecoveryChange{customer,state.recovery,plan->after,0};
        auto raced=use;raced.items[0].before->count=2;raced.items[0].after.count=1;Check(!p::CommitItems(writer,*c,raced),"item CAS rolls sidecar insertion back");auto unchanged=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(unchanged.recovery==state.recovery && unchanged.items==state.items && unchanged.fence==state.fence,"failed start neither consumes nor queues");
        Check(p::CommitItems(writer,*c,use).has_value() && p::CommitItems(writer,*c,use)->replay,"source and schedule commit exactly once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(state.recovery==plan->after && state.items[0].count==2,"full recovery state reload");
        auto changed=use;changed.recovery->after.doses[0].id=3;Check(!p::CommitItems(writer,*c,changed),"receipt binds exact scheduled effect");
        auto tick=PlanRecoveryScheduleTick(*c,state.recovery,1234,actor,false,false).value();ItemCommit save;save.operationId={19,2};save.kind=ItemOperationKind::Service;save.catalogRevision=c->Revision();save.catalogHash=c->ContentHash();save.owners={state.fence};save.outcomeCode=0x1903;save.recovery=ItemRecoveryChange{customer,state.recovery,tick.after,1234};
        auto stale=save;stale.recovery->before.doses[0].remainingRaw--;Check(!p::CommitItems(writer,*c,stale),"sidecar CAS refuses stale amount");Check(p::LoadCurrentItemOwner(writer,*c,customer)->fence==state.fence,"stale sidecar rolls fence back");
        Check(p::CommitItems(writer,*c,save).has_value() && p::CommitItems(writer,*c,save)->replay,"tick debit is replay safe");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(state.recovery==tick.after && state.items[0].count==2 && state.wallets[0].balance==1000,"tick is recovery only");
        Check(p::ClaimOwnerSession(writer,*c,customer).has_value(),"recovery relog");Check(p::LoadCurrentItemOwner(writer,*c,customer)->recovery==state.recovery,"offline remainders preserved");
        Query(admin,"UPDATE player.item_recovery_schedule SET state_sha256=UNHEX(REPEAT('00',32)) WHERE owner_kind=1 AND owner_id=880093");auto corrupt=p::LoadCurrentItemOwner(writer,*c,customer);Check(!corrupt && corrupt.error()==E::InvalidStoredData,"corrupt sidecar fails closed");
        Query(admin,"UPDATE player.item_recovery_schedule SET state_sha256=? WHERE owner_kind=1 AND owner_id=880093",{U::Blob(Bytes(*RecoveryScheduleDigest(*c,customer,state.recovery)))});Check(p::LoadCurrentItemOwner(writer,*c,customer).has_value(),"verified sidecar restored");
    }
    {
        const ItemOwner customer{ItemOwnerKind::Character,880094};const auto bag=f::Inventory(customer);const ItemWallet wallet{customer,WalletCurrency::Gold,1,1000};
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),std::span(&wallet,1)).has_value(),"automatic owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,2).value();const std::array ids{ItemInstanceId::Create(block.first).value(),ItemInstanceId::Create(block.first+1).value()};
        const auto grant=MakeGrantCommit(*c,{20,1},state.fence,state.storages,state.items,{item_catalog_fixture::AutomaticPotion(),0,2},ids);Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"two automatic bottles granted full");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        const auto before=Find(state,ids[0].Value());Check(before.automatic==AutomaticRecoveryState{5000000,false,0},"charge survives SQL roundtrip");
        ItemCommit on;on.operationId={20,2};on.catalogRevision=c->Revision();on.catalogHash=c->ContentHash();on.kind=ItemOperationKind::Service;on.owners={state.fence};on.outcomeCode=0x2001;
        Check(ChangeAutomatic(on,state.storages,before,{5000000,true,1000}).has_value() && p::CommitItems(writer,*c,on).has_value(),"activation persists without consuming bottle");Check(p::CommitItems(writer,*c,on)->replay,"activation receipt replay safe");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        InstantRecoveryAdmission actor;actor.pools={500000,2000000,0,1000000};actor.level=100;actor.context=RecoveryContext::World;
        ItemCommit tick;tick.operationId={20,3};tick.catalogRevision=c->Revision();tick.catalogHash=c->ContentHash();tick.kind=ItemOperationKind::Service;tick.owners={state.fence};tick.outcomeCode=0x2003;RecoverySchedulePlan recovery;recovery.after.revision=1;
        Check(TickAutomaticOwner(*c,tick,state.storages,state.items,actor,recovery,1000,false,true).has_value(),"automatic tick planned against SQL snapshot");tick.recovery=ItemRecoveryChange{customer,state.recovery,recovery.after,1000};
        auto raced=tick;raced.items[0].before->automatic->remainingRaw--;Check(!p::CommitItems(writer,*c,raced),"stale charge CAS rejects entire tick");Check(p::LoadCurrentItemOwner(writer,*c,customer)->items==state.items,"stale tick leaves all charge unchanged");
        Check(p::CommitItems(writer,*c,tick).has_value() && p::CommitItems(writer,*c,tick)->replay,"tick debit and fractional credit commit exactly once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(state,ids[0].Value()).automatic->remainingRaw==4860000,"only committed healing debits charge");
        Check(p::ClaimOwnerSession(writer,*c,customer).has_value(),"automatic relog session");Check(p::LoadCurrentItemOwner(writer,*c,customer)->items==state.items,"relog neither refills charge nor loses activation");
        Query(admin,"UPDATE player.item_instance SET automatic_remaining_raw=automatic_remaining_raw+1 WHERE instance_id=?",{U::UInt(ids[0].Value())});Check(!p::LoadCurrentItemOwner(writer,*c,customer),"corrupt charge rejected by item digest");Query(admin,"UPDATE player.item_instance SET automatic_remaining_raw=automatic_remaining_raw-1 WHERE instance_id=?",{U::UInt(ids[0].Value())});Check(p::LoadCurrentItemOwner(writer,*c,customer).has_value(),"verified charge restored");
    }
    {
        namespace ef=effect_runtime_fixture;
        const ItemOwner customer{ItemOwnerKind::Character,880095};const auto bag=f::Inventory(customer);
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),{}).has_value(),"effect owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto ids=p::ReserveItemInstanceIds(writer,*c,3).value();
        for(unsigned i=0;i<3;++i){auto add=MakeGrantCommit(*c,{21,0,static_cast<std::uint8_t>(i+1)},state.fence,state.storages,state.items,{ef::Key(i==2 ? 4 : i),0,i==2 ? 1u : 3u},std::array{ItemInstanceId::Create(ids.first+i).value()});Check(add && p::CommitItems(writer,*c,*add).has_value(),"effect source granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();}
        const auto use=[&](unsigned step,unsigned item,std::uint64_t elapsed){return MakeItemEffectUse(*c,{21,1,static_cast<std::uint8_t>(step)},state.fence,state.storages,state.items,state.recovery,ItemInstanceId::Create(ids.first+item).value(),ef::Facts(),elapsed,1000+elapsed,step);};
        auto wall=use(1,0,0);Check(wall.has_value(),"wall use planned");auto raced=*wall;++raced.items[0].before->revision;++raced.items[0].after.revision;Check(!p::CommitItems(writer,*c,raced),"source CAS rolls new RCS2 effect back");Check(p::LoadCurrentItemOwner(writer,*c,customer)->recovery==state.recovery,"failed effect start leaves sidecar unchanged");
        Check(p::CommitItems(writer,*c,*wall).has_value() && p::CommitItems(writer,*c,*wall)->replay,"wall deadline and exact debit commit once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(state.recovery.effects[0].expiresAtMs==61000 && Find(state,ids.first).count==2,"RCS2 deadline survives SQL");
        auto online=use(2,1,123);Check(online && p::CommitItems(writer,*c,*online).has_value(),"online effect saved alongside wall");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto on=use(3,2,250);Check(on && p::CommitItems(writer,*c,*on).has_value(),"toggle activation and online elapsed saved together");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(state,ids.first+2).toggle==ToggleEffectState{60000,true},"toggle not charged before activation");
        auto off=use(4,2,137);Check(off.has_value(),"subsecond off planned");auto corrupt=*off;corrupt.items[0].before->toggle->remainingMs--;Check(!p::CommitItems(writer,*c,corrupt),"stale toggle charge rolls schedule and owner fence back");Check(p::LoadCurrentItemOwner(writer,*c,customer)->recovery==state.recovery,"stale toggle transaction changes no timer");
        Check(p::CommitItems(writer,*c,*off).has_value() && p::CommitItems(writer,*c,*off)->replay,"exact milliseconds debit once across replay");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(Find(state,ids.first+2).toggle==ToggleEffectState{59863,false} && state.recovery.effects[1].remainingMs==59613,"item timer and online schedule reload exactly");
        Check(p::ClaimOwnerSession(writer,*c,customer).has_value(),"effect owner relog");auto next=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(next.items==state.items && next.recovery==state.recovery,"offline time does not refill or advance online state");
        Query(admin,"UPDATE player.item_instance SET toggle_remaining_ms=toggle_remaining_ms+1 WHERE instance_id=?",{U::UInt(ids.first+2)});Check(!p::LoadCurrentItemOwner(writer,*c,customer),"corrupt toggle charge fails item hash");Query(admin,"UPDATE player.item_instance SET toggle_remaining_ms=toggle_remaining_ms-1 WHERE instance_id=?",{U::UInt(ids.first+2)});Check(p::LoadCurrentItemOwner(writer,*c,customer).has_value(),"verified toggle restored");
    }
    {
        namespace bf=book_runtime_fixture;
        const ItemOwner customer{ItemOwnerKind::Character,880096};const auto bag=f::Inventory(customer);
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),{}).has_value(),"book owner provisioned");
        Query(admin,"INSERT INTO player.player(id,exp,skill_level) VALUES(880096,1000,?)",{U::Blob(std::string(2550,'\0'))});
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,1).value();const auto id=ItemInstanceId::Create(block.first).value();
        auto grant=MakeGrantCommit(*c,{22,0,1},state.fence,state.storages,state.items,{bf::Key(0),0,3},std::array{id});if(!grant)throw std::runtime_error("SQL book grant planning error "+std::to_string(static_cast<unsigned>(grant.error())));auto granted=p::CommitItems(writer,*c,*grant);if(!granted)throw std::runtime_error("SQL book grant write error "+std::to_string(static_cast<unsigned>(granted.error())));Check(granted.has_value(),"book source granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();BookCharacter live;live.experience=1000;
        auto first=MakeBookRuntimeCommit(*c,{22,1},state.fence,state.storages,state.items,state.learning,live,id,bf::Facts(),1000,{0,1500});Check(first.has_value(),"SQL book progress planned");
        auto raced=first->commit;++raced.items[0].before->revision;++raced.items[0].after.revision;Check(!p::CommitItems(writer,*c,raced),"item CAS rolls book sidecar and character back");Check(Scalar(admin,"SELECT exp FROM player.player WHERE id=880096")=="1000" && p::LoadCurrentItemOwner(writer,*c,customer)->learning==BookLearning{},"refused read cannot charge XP or progress");
        Execute(admin,"UPDATE player.player SET typed_book_revision=1 WHERE id=880096");Check(!p::CommitItems(writer,*c,first->commit),"character revision CAS rolls source and sidecar back");Execute(admin,"UPDATE player.player SET typed_book_revision=0 WHERE id=880096");Check(p::LoadCurrentItemOwner(writer,*c,customer)->items==state.items,"refused participant preserves book count");
        Check(p::CommitItems(writer,*c,first->commit).has_value() && p::CommitItems(writer,*c,first->commit)->replay,"book source, XP, progress, deadline and receipt commit once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();live=state.learning.character;Check(state.learning==first->commit.learning->after && state.items[0].count==2 && Scalar(admin,"SELECT exp FROM player.player WHERE id=880096")=="900","entire learning state reloads exactly");
        auto changed=first->commit;changed.result[4]=3;Check(!p::CommitItems(writer,*c,changed),"receipt binds recorded book outcome");
        auto second=MakeBookRuntimeCommit(*c,{22,2},state.fence,state.storages,state.items,state.learning,live,id,bf::Facts(),2500,{0,1500});Check(second && p::CommitItems(writer,*c,second->commit).has_value(),"second successful read promotes once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(state.learning.character.skills[1].level==1 && state.learning.character.experience==800 && state.learning.progress[0].reads==0,"learned level and counter reload together");
        const auto save=ProtectBookSave("UPDATE player.player SET exp = 9999, skill_point = 9, sub_skill_point = 9, horse_skill_point = 9, skill_level = '', name = 'later' WHERE id=880096",0,"");Check(save.has_value(),"real legacy save protection constructed");Execute(admin,*save);Check(Scalar(admin,"SELECT exp FROM player.player WHERE id=880096")=="800" && Scalar(admin,"SELECT typed_book_revision FROM player.player WHERE id=880096")=="2" && Scalar(admin,"SELECT name FROM player.player WHERE id=880096")=="later","late legacy save preserves every book field while other fields still save");
        Check(p::ClaimOwnerSession(writer,*c,customer).has_value(),"book relog fence");Check(p::LoadCurrentItemOwner(writer,*c,customer)->learning==state.learning,"book level, reads and offline deadline survive relog");
        Query(admin,"UPDATE player.item_book_learning SET state_sha256=UNHEX(REPEAT('00',32)) WHERE owner_kind=1 AND owner_id=880096");Check(!p::LoadCurrentItemOwner(writer,*c,customer),"corrupt learning hash fails closed");Query(admin,"UPDATE player.item_book_learning SET state_sha256=? WHERE owner_kind=1 AND owner_id=880096",{U::Blob(Bytes(*BookLearningDigest(customer,state.learning)))});
    }
    {
        namespace af=book_aid_fixture;namespace bf=book_runtime_fixture;
        const ItemOwner customer{ItemOwnerKind::Character,880097};const auto bag=f::Inventory(customer);
        Check(p::ProvisionOwner(admin,*c,customer,std::span(&bag,1),{}).has_value(),"aid owner provisioned");
        Query(admin,"INSERT INTO player.player(id,exp,skill_level) VALUES(880097,654,?)",{U::Blob(std::string(2550,'\0'))});
        auto state=p::LoadCurrentItemOwner(writer,*c,customer).value();const auto block=p::ReserveItemInstanceIds(writer,*c,3).value();
        for(unsigned n=0;n<3;++n){auto grant=MakeGrantCommit(*c,{23,0,static_cast<std::uint8_t>(n+1)},state.fence,state.storages,state.items,{n<2 ? af::Key(n) : bf::Key(0),0,5},std::array{ItemInstanceId::Create(block.first+n).value()});Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"aid and book fixtures granted");state=p::LoadCurrentItemOwner(writer,*c,customer).value();}
        BookCharacter live;live.experience=654;live.points={4,5,6};const auto id=ItemInstanceId::Create(block.first).value();
        auto aid=MakeBookAidRuntimeCommit(*c,{23,1},state.fence,state.storages,state.items,state.learning,live,id,bf::Facts());Check(aid.has_value(),"aid SQL planned");
        auto raced=aid->commit;++raced.items[0].before->revision;++raced.items[0].after.revision;Check(!p::CommitItems(writer,*c,raced),"source CAS rolls aid credit back");Check(p::LoadCurrentItemOwner(writer,*c,customer)->learning==BookLearning{},"failed aid leaves no learning sidecar");
        Execute(admin,"UPDATE player.player SET typed_book_revision=1 WHERE id=880097");Check(!p::CommitItems(writer,*c,aid->commit),"character CAS rolls aid source and credit back");Execute(admin,"UPDATE player.player SET typed_book_revision=0 WHERE id=880097");
        Check(p::CommitItems(writer,*c,aid->commit).has_value() && p::CommitItems(writer,*c,aid->commit)->replay,"aid replay credits once");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(state.learning.chanceTokens==1 && state.learning.character==live && Find(state,block.first).count==4,"aid state and live character survive SQL reload");
        auto full=MakeBookAidRuntimeCommit(*c,{23,2},state.fence,state.storages,state.items,state.learning,live,id,bf::Facts());Check(!full && full.error()==SkillBookError::ChargeLimit,"SQL reloaded cap rejects next debit");
        auto bypass=MakeBookAidRuntimeCommit(*c,{23,3},state.fence,state.storages,state.items,state.learning,live,ItemInstanceId::Create(block.first+1).value(),bf::Facts());Check(bypass && p::CommitItems(writer,*c,bypass->commit).has_value(),"bypass credits independently");state=p::LoadCurrentItemOwner(writer,*c,customer).value();
        auto book=MakeBookRuntimeCommit(*c,{23,4},state.fence,state.storages,state.items,state.learning,live,ItemInstanceId::Create(block.first+2).value(),bf::Facts(),1000,{0,1500});Check(book && book->result.consumeChanceBonus && !book->result.consumeDelayBypass && p::CommitItems(writer,*c,book->commit).has_value(),"book atomically consumes item-earned bonus and preserves unused bypass");state=p::LoadCurrentItemOwner(writer,*c,customer).value();live=state.learning.character;
        auto waiting=MakeBookRuntimeCommit(*c,{23,5},state.fence,state.storages,state.items,state.learning,live,ItemInstanceId::Create(block.first+2).value(),bf::Facts(),1001,{0,1500});Check(waiting && waiting->result.consumeDelayBypass && p::CommitItems(writer,*c,waiting->commit).has_value(),"book consumes bypass within exact deadline");state=p::LoadCurrentItemOwner(writer,*c,customer).value();Check(!state.learning.chanceTokens && state.learning.bypassTokens==1 && state.learning.character.skills[1].level==1,"persisted balance and skill advance together");
        Check(p::ClaimOwnerSession(writer,*c,customer).has_value() && p::LoadCurrentItemOwner(writer,*c,customer)->learning==state.learning,"aid and book result survive session handoff");
    }
    {
        namespace poly=polymorph_runtime_fixture;
        const ItemOwner owner{ItemOwnerKind::Character,880098};const auto bag=f::Inventory(owner);
        Check(p::ProvisionOwner(admin,*c,owner,std::span(&bag,1),{}).has_value(),"poly owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,owner).value();const auto ids=p::ReserveItemInstanceIds(writer,*c,4).value();
        for(unsigned n=0;n<4;++n){const auto fixture=poly::Item(n);auto grant=MakeGrantCommit(*c,{24,0,static_cast<std::uint8_t>(n+1)},state.fence,state.storages,state.items,{fixture.key,0,fixture.count,{},fixture.polymorph},std::array{ItemInstanceId::Create(ids.first+n).value()});Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"orb and book payloads granted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();}
        auto actor=poly::Facts();actor.actorId=owner.id;
        auto use=[&](unsigned n,unsigned step){return MakePolymorphRuntimeCommit(*c,{24,1,static_cast<std::uint8_t>(step)},state.fence,state.storages,state.items,state.recovery,ItemInstanceId::Create(ids.first+n).value(),actor,0,1000,0);};
        auto orb=use(0,1);Check(orb.has_value(),"SQL orb plan");auto raced=orb->commit;++raced.items[0].before->revision;++raced.items[0].after.revision;Check(!p::CommitItems(writer,*c,raced),"orb CAS rolls RCS3 back");Check(!p::LoadCurrentItemOwner(writer,*c,owner)->recovery.polymorph,"failed orb has no active shape");
        Check(p::CommitItems(writer,*c,orb->commit).has_value() && p::CommitItems(writer,*c,orb->commit)->replay,"orb and RCS3 receipt once");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,ids.first).count==4 && state.recovery.polymorph->effect.powerBp==3333,"fractional power and source debit reload");
        Check(p::ClaimOwnerSession(writer,*c,owner).has_value(),"polymorph relog");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(state.recovery.polymorph->effect.expiresAtUtcMs==3000,"absolute deadline survives offline load");auto cancel=MakePolymorphCancel(*c,{24,2},state.fence,state.recovery,0,1500,state.recovery.generation);Check(cancel && p::CommitItems(writer,*c,*cancel).has_value(),"shape cancellation saved");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        auto failed=use(3,2);Check(failed && p::CommitItems(writer,*c,failed->commit).has_value(),"failed book receipted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,ids.first+3).polymorph->practiceRemaining==2 && !state.recovery.polymorph,"failed book changes no practice or shape");
        auto book=use(2,3);Check(book && p::CommitItems(writer,*c,book->commit).has_value(),"practice and shape saved together");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,ids.first+2).polymorph->practiceRemaining==1 && state.recovery.polymorph->effect.bonusMagnitude==21,"book practice and movement bonus reload");
        Query(admin,"UPDATE player.item_instance SET polymorph_practice=polymorph_practice+1 WHERE instance_id=?",{U::UInt(ids.first+2)});Check(!p::LoadCurrentItemOwner(writer,*c,owner),"tampered practice fails instance hash");Query(admin,"UPDATE player.item_instance SET polymorph_practice=polymorph_practice-1 WHERE instance_id=?",{U::UInt(ids.first+2)});Check(p::LoadCurrentItemOwner(writer,*c,owner).has_value(),"original practice restored");
        auto cleared=state.recovery;cleared.polymorph.reset();++cleared.revision;ItemCommit clear;clear.operationId={24,3};clear.catalogRevision=c->Revision();clear.catalogHash=c->ContentHash();clear.kind=ItemOperationKind::Service;clear.owners={state.fence};clear.recovery=ItemRecoveryChange{owner,state.recovery,cleared,0};clear.outcomeCode=0x2404;clear.result={'P','L','R',1,4};Check(p::CommitItems(writer,*c,clear).has_value(),"death/expiry clear schedule saved");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(!state.recovery.polymorph && Find(state,ids.first+1).polymorph->boundForm==24001,"clear preserves independent bound orb payload");
    }
    {
        namespace fish=fish_processing_fixture;
        const ItemOwner owner{ItemOwnerKind::Character,880099};const auto bag=f::Inventory(owner);
        Check(p::ProvisionOwner(admin,*c,owner,std::span(&bag,1),{}).has_value(),"fish owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        const auto block=p::ReserveItemInstanceIds(writer,*c,32).value();std::vector<ItemInstanceId> ids;for(unsigned n=0;n<32;++n)ids.push_back(ItemInstanceId::Create(block.first+n).value());
        auto grant=MakeGrantCommit(*c,{25,0},state.fence,state.storages,state.items,{fish::Fish(1),0,2},std::span(ids).first(1));
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"live fish persisted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        FishProcessingRequest request{ids[0],FishAction::Use,{100,0,0,true,false,false}};
        auto plan=MakeFishProcessingCommit(*c,{25,1},state.fence,state.storages,state.items,request,std::span(ids).subspan(1),[](auto){return 0;});
        Check(plan.has_value(),"SQL live-fish plan");auto raced=plan->commit;++raced.items[0].before->revision;++raced.items[0].after.revision;
        Check(!p::CommitItems(writer,*c,raced),"fish source CAS aborts reward");auto untouched=p::LoadCurrentItemOwner(writer,*c,owner).value();
        Check(untouched.items.size()==1 && Find(untouched,block.first).count==2,"CAS failure preserves fish and gives nothing");
        Check(p::CommitItems(writer,*c,plan->commit).has_value() && p::CommitItems(writer,*c,plan->commit)->replay,"fish reward and receipt commit exactly once");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,block.first).count==1 && Find(state,block.first+1).key==fish::Fish(2) && Find(state,block.first+1).count==3,"live consumption and full dead-fish yield reload");
        request={ids[1],FishAction::Grill,{100,0,0,true,false,false}};
        Check(!MakeFishProcessingCommit(*c,{25,2},state.fence,state.storages,state.items,request,std::span(ids).subspan(2),{}),"missing fire creates no SQL plan");request.actor.campfireAvailable=true;
        auto grilled=MakeFishProcessingCommit(*c,{25,3},state.fence,state.storages,state.items,request,std::span(ids).subspan(2),{});
        Check(grilled && p::CommitItems(writer,*c,grilled->commit).has_value() && p::CommitItems(writer,*c,grilled->commit)->replay,"grill all units and receipt atomically");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(state.items.size()==2 && std::ranges::none_of(state.items,[&](const auto& i){return i.id==ids[1];}) && Find(state,block.first+2).count==3,"all dead fish became exact cooked count");
        Check(p::ClaimOwnerSession(writer,*c,owner).has_value(),"fish relog session");const auto reloaded=p::LoadCurrentItemOwner(writer,*c,owner).value();
        Check(reloaded.items==state.items && reloaded.fence.sessionGeneration>state.fence.sessionGeneration,"fish and cooked reward survive session handoff");
        const auto old=p::CommitItems(writer,*c,grilled->commit);Check(!old || old->replay,"old session cannot reapply grill");
    }
    {
        namespace tool=tool_runtime_fixture;
        const ItemOwner owner{ItemOwnerKind::Character,880100};const std::array bags{f::Inventory(owner),ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},8,1,1}};
        Check(p::ProvisionOwner(admin,*c,owner,bags,{}).has_value(),"tool owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,owner).value();const auto block=p::ReserveItemInstanceIds(writer,*c,2).value();
        const auto rod=ItemInstanceId::Create(block.first).value(),bait=ItemInstanceId::Create(block.first+1).value();
        auto grant=MakeGrantCommit(*c,{26,0,1},state.fence,state.storages,state.items,{tool::Tool(),0,1,{},{},ToolInstanceState{3,0}},std::array{rod});Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"explicit tool payload granted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        grant=MakeGrantCommit(*c,{26,0,2},state.fence,state.storages,state.items,{tool::Bait(),0,2},std::array{bait});Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"bait units granted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        auto equip=MakeEquipCommit(*c,{26,1},state.fence,state.storages,state.items,rod,{100,0,0});Check(equip && p::CommitItems(writer,*c,*equip).has_value(),"tool equips with occupied-cell guard");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        auto actor=tool::Actor();ToolRuntimeRequest request{bait,rod,ToolRuntimeAction::Bait,actor};auto applied=MakeToolRuntimeCommit(*c,{26,2},state.fence,state.storages,state.items,request);Check(applied.has_value(),"SQL bait plan");
        auto raced=applied->commit;raced.items[1].before->tool->baitPower=1;Check(!p::CommitItems(writer,*c,raced),"rod before-state CAS rolls bait debit back");auto unchanged=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(unchanged,block.first+1).count==2 && Find(unchanged,block.first).tool->baitPower==0,"both items unchanged after rod CAS failure");
        Check(p::CommitItems(writer,*c,applied->commit).has_value() && p::CommitItems(writer,*c,applied->commit)->replay,"bait and rod receipt exactly once");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,block.first+1).count==1 && Find(state,block.first).tool==ToolInstanceState{3,20},"bait and practice reload independently");
        Query(admin,"UPDATE player.item_instance SET tool_bait_power=tool_bait_power+1 WHERE instance_id=?",{U::UInt(block.first)});Check(!p::LoadCurrentItemOwner(writer,*c,owner),"tampered bait fails state hash");Query(admin,"UPDATE player.item_instance SET tool_bait_power=tool_bait_power-1 WHERE instance_id=?",{U::UInt(block.first)});
        auto unequip=MakeUnequipCommit(*c,{26,3},state.fence,state.storages,state.items,rod,{ItemStorageKind::Inventory,1,0});Check(unequip && p::CommitItems(writer,*c,*unequip).has_value(),"tool unequips preserving state");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        request={rod,{},ToolRuntimeAction::Refine,actor};auto refined=MakeToolRuntimeCommit(*c,{26,4},state.fence,state.storages,state.items,request);Check(refined && p::CommitItems(writer,*c,refined->commit).has_value() && p::CommitItems(writer,*c,refined->commit)->replay,"refine and receipt atomic");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,block.first).refinementLevel==1 && Find(state,block.first).tool==ToolInstanceState{0,20},"refine keeps identity and bait while resetting practice");
        equip=MakeEquipCommit(*c,{26,5},state.fence,state.storages,state.items,rod,{100,0,0});Check(equip && p::CommitItems(writer,*c,*equip).has_value(),"practice fixture wears the same rod");state=p::LoadCurrentItemOwner(writer,*c,owner).value();request={rod,{},ToolRuntimeAction::Practice,actor,1};Check(!MakeToolRuntimeCommit(*c,{26,6},state.fence,state.storages,state.items,request),"practice without resource completion denied");request.actor.completedResourceAction=true;
        auto practiced=MakeToolRuntimeCommit(*c,{26,7},state.fence,state.storages,state.items,request);Check(practiced && p::CommitItems(writer,*c,practiced->commit).has_value() && p::CommitItems(writer,*c,practiced->commit)->replay,"completed resource receipt cannot train twice");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,block.first).tool==ToolInstanceState{1,20},"one completed event saves exactly one practice point");
        Check(p::ClaimOwnerSession(writer,*c,owner).has_value(),"tool relog");const auto reloaded=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(reloaded.items==state.items && reloaded.fence.sessionGeneration>state.fence.sessionGeneration,"equipped rod level, practice and bait survive handoff");
        Query(admin,"UPDATE player.item_instance SET tool_training_points=tool_training_points+1 WHERE instance_id=?",{U::UInt(block.first)});Check(!p::LoadCurrentItemOwner(writer,*c,owner),"tampered practice fails hash");Query(admin,"UPDATE player.item_instance SET tool_training_points=tool_training_points-1 WHERE instance_id=?",{U::UInt(block.first)});Check(p::LoadCurrentItemOwner(writer,*c,owner).has_value(),"restored tool state validates");
    }
    {
        namespace tool=tool_runtime_fixture;const ItemOwner owner{ItemOwnerKind::Character,880101};const std::array bags{f::Inventory(owner),ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},8,1,1}};
        Check(p::ProvisionOwner(admin,*c,owner,bags,{}).has_value(),"resource owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,owner).value();auto ids=p::ReserveItemInstanceIds(writer,*c,3).value();auto rod=ItemInstanceId::Create(ids.first).value();
        auto grant=MakeGrantCommit(*c,{27,1},state.fence,state.storages,state.items,{tool::Tool(),0,1,{},{},ToolInstanceState{0,50}},std::array{rod});Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"resource rod granted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        auto equip=MakeEquipCommit(*c,{27,2},state.fence,state.storages,state.items,rod,{100,0,0});Check(equip && p::CommitItems(writer,*c,*equip).has_value(),"resource rod equipped");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        GatheringSite site{1,41,9009,"Fishing site",ToolKind::Rod,FishingPool::First,900,2000,1,100,10000,0,0,{{tool::Bait(),2,1}}};
        const std::array rewardIds{ItemInstanceId::Create(ids.first+1).value(),ItemInstanceId::Create(ids.first+2).value()};auto plan=MakeResourceGatheringCommit(*c,site,{27,3},state.fence,state.storages,state.items,{rod,tool::Actor(),true},rewardIds,[](auto){return 0u;});Check(plan && plan->success,"completed event creates atomic resource plan");
        auto raced=plan->commit;raced.items[0].before->tool->baitPower=20;Check(!p::CommitItems(writer,*c,raced),"resource wrong rod CAS rolls reward back");auto unchanged=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(unchanged.items==state.items,"CAS cannot leak reward, bait or practice");
        Check(p::CommitItems(writer,*c,plan->commit).has_value() && p::CommitItems(writer,*c,plan->commit)->replay,"resource completion receipted exactly once");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(Find(state,ids.first).tool==ToolInstanceState{1,0} && Find(state,ids.first+1).count==2,"resource reward, training and bait saved together");
        Check(!MakeResourceGatheringCommit(*c,site,{27,4},state.fence,state.storages,state.items,{rod,tool::Actor(),true},rewardIds,[](auto){return 0u;}),"empty rod cannot complete a second catch");
        Check(p::ClaimOwnerSession(writer,*c,owner).has_value(),"resource relog claim");auto reloaded=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(reloaded.items==state.items,"resource reward and exact tool survive relog");
    }
    {
        namespace tool=tool_runtime_fixture;const ItemOwner owner{ItemOwnerKind::Character,880102};const std::array bags{f::Inventory(owner),ItemStorageLayout{{owner,ItemStorageKind::Equipment,1},8,1,1}};
        Check(p::ProvisionOwner(admin,*c,owner,bags,{}).has_value(),"water owner provisioned");auto state=p::LoadCurrentItemOwner(writer,*c,owner).value();auto ids=p::ReserveItemInstanceIds(writer,*c,2).value();auto rod=ItemInstanceId::Create(ids.first).value();
        auto grant=MakeGrantCommit(*c,{28,1},state.fence,state.storages,state.items,{tool::Tool(),0,1,{},{},ToolInstanceState{0,50}},std::array{rod});Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"baited water rod granted");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        auto equip=MakeEquipCommit(*c,{28,2},state.fence,state.storages,state.items,rod,{100,0,0});Check(equip && p::CommitItems(writer,*c,*equip).has_value(),"water rod equipped");state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        GatheringSite site{1001,41,0,"Free water",ToolKind::Rod,FishingPool::First,400,15000,0,100,10000,0,0,{{tool::Bait(),2,1}}};unsigned draws=0;
        auto failed=MakeResourceGatheringCommit(*c,site,{28,3},state.fence,state.storages,state.items,{rod,tool::Actor(),true,false},std::array{ItemInstanceId::Create(ids.first+1).value()},[&](auto){++draws;return 0u;});Check(failed && !failed->success && draws==0,"expired minigame plans no reward or practice RNG");
        Check(p::CommitItems(writer,*c,failed->commit).has_value() && p::CommitItems(writer,*c,failed->commit)->replay,"failed minigame is receipted once");state=p::LoadCurrentItemOwner(writer,*c,owner).value();Check(state.items.size()==1 && Find(state,ids.first).tool==ToolInstanceState{0,0},"timeout consumes bait but never grants loot or practice");
        Check(p::ClaimOwnerSession(writer,*c,owner).has_value(),"water relog claim");Check(p::LoadCurrentItemOwner(writer,*c,owner)->items==state.items,"water failure survives relog exactly");
    }
    {
        const ItemOwner owner{ItemOwnerKind::Character,880103};
        const auto bag=f::Inventory(owner);
        Check(p::ProvisionOwner(admin,*c,owner,std::span(&bag,1),{}).has_value(),"companion owner provisioned");
        auto state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        const auto ids=p::ReserveItemInstanceIds(writer,*c,2).value();
        const auto petId=ItemInstanceId::Create(ids.first).value(),mountId=ItemInstanceId::Create(ids.first+1).value();
        auto grant=MakeGrantCommit(*c,{29,1},state.fence,state.storages,state.items,{petSeal,0,1},std::array{petId});
        Check(grant && grant->items.front().after.companion==CompanionState{} &&
              p::CommitItems(writer,*c,*grant).has_value(),"permanent pet granted with explicit dormant state");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        grant=MakeGrantCommit(*c,{29,2},state.fence,state.storages,state.items,{mountSeal,0,1},std::array{mountId});
        Check(grant && p::CommitItems(writer,*c,*grant).has_value(),"permanent mount seal granted");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();

        const auto requestFor=[&](ItemInstanceId id,CompanionAction action,std::uint8_t serial){
            const auto& seal=Find(state,id.Value());
            CompanionRequest request{seal.key};
            request.state=*seal.companion;
            request.instanceId=id.Value();
            request.revision=seal.revision;
            request.ownerId=request.actorId=owner.id;
            request.rulesetRevision=1;
            request.operationId={29,serial};
            request.count=1;
            request.characterLevel=50;
            request.characterClass=request.sex=1;
            request.action=action;
            request.admittedByServer=request.ownerAlive=request.worldAllowsCompanion=true;
            return request;
        };
        const auto summon=MakeCompanionCommit(*c,state.fence,state.storages,state.items,
            requestFor(petId,CompanionAction::Summon,3));
        Check(summon && p::CommitItems(writer,*c,*summon).has_value() &&
              p::CommitItems(writer,*c,*summon)->replay,"summon intent and receipt commit once");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        const auto& called=Find(state,petId.Value());
        Check(called.count==1 && called.companion->recallRequested && called.companion->phase==CompanionPhase::Dormant &&
              !called.companion->activeFormId,"only recall intent survives, with the intact seal");
        const auto second=requestFor(mountId,CompanionAction::Summon,4);
        const auto occupied=MakeCompanionCommit(*c,state.fence,state.storages,state.items,second);
        Check(!occupied && occupied.error()==E::Conflict,
            "a pet and mount cannot occupy the same typed follower channel");
        const std::array staleView{Find(state,mountId.Value())};
        const auto stalePlan=MakeCompanionCommit(*c,state.fence,state.storages,staleView,second);
        Check(stalePlan.has_value(),"incomplete snapshot does not expose the occupied channel to the planner");
        const auto refused=p::CommitItems(writer,*c,*stalePlan);
        Check(!refused && refused.error()==E::Conflict,
            "SQL current read rejects a second recall intent even with an incomplete inventory");
        Check(!MakeConsumeCommit(*c,{29,5},state.fence,state.storages,state.items,petId,1),
            "an active seal cannot be destroyed by a generic item operation");
        const auto move=MakeMoveCommit(*c,{29,6},state.fence,state.storages,state.items,petId,{ItemStorageKind::Inventory,1,5});
        Check(move && p::CommitItems(writer,*c,*move).has_value(),"active seal can move within its owner's bag");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        Check(p::ClaimOwnerSession(writer,*c,owner).has_value(),"companion relog fence claimed");
        const auto relog=p::LoadCurrentItemOwner(writer,*c,owner).value();
        Check(relog.items==state.items && relog.fence.sessionGeneration>state.fence.sessionGeneration,
            "permanent seal identity, position and recall intent survive relog");
        state=relog;
        const auto dismiss=MakeCompanionCommit(*c,state.fence,state.storages,state.items,
            requestFor(petId,CompanionAction::Dismiss,7));
        Check(dismiss && p::CommitItems(writer,*c,*dismiss).has_value(),"dismiss clears durable intent");
        state=p::LoadCurrentItemOwner(writer,*c,owner).value();
        Check(!Find(state,petId.Value()).companion->recallRequested &&
              Find(state,petId.Value()).placement->anchor==5,"dismiss preserves owned seal at its new location");
        Query(admin,"UPDATE player.item_instance SET companion_recall_requested=1 WHERE instance_id=?",{U::UInt(petId.Value())});
        Check(!p::LoadCurrentItemOwner(writer,*c,owner),"tampered recall intent fails instance hash");
        Query(admin,"UPDATE player.item_instance SET companion_recall_requested=0 WHERE instance_id=?",{U::UInt(petId.Value())});
        Check(p::LoadCurrentItemOwner(writer,*c,owner)->items==state.items,"exact recall intent restores valid state");
    }
    GrowthPetBirthSql(admin,writer,*c,hatchEgg,growthSeal,petSeal,petBook);
    Sessions(admin,writer,*c);
    Allocator(admin,writer,*c,port,pw);
}
}
int main(int argc,char** argv)
{
    try
    {
        Completion();
        if(argc==2 && std::string_view(argv[1])=="--owned-sandbox")
        {
            std::string portText,pw,token;std::getline(std::cin,portText);std::getline(std::cin,pw);std::getline(std::cin,token);
            Check(token.size()==32,"owned token required");CAsyncSQL admin,writer;const auto port=std::stoul(portText);Setup(admin,port,pw,"root");
            Check(Scalar(admin,"SELECT token FROM protos.ic_persistence_owner")==token,"owned sandbox identity");Setup(writer,port,pw,"item_writer");ActualSql(admin,writer,port,pw);
        }
        else Check(argc==2 && std::string_view(argv[1])=="--completion-only","explicit test mode required");
        std::cout<<checks<<" Item persistence SQL checks passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
