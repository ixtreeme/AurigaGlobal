#include "PersistentItemFixture.hpp"
#include "CatalogSnapshotFixture.hpp"
#include <ItemPersistenceSql.hpp>
#include <item/OwnedGround.hpp>
#include <item/NpcShops.hpp>
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
    // The item writer account has no storage privilege: it can never invent an owner.
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
void ActualSql(CAsyncSQL& admin,CAsyncSQL& writer,unsigned port,const std::string& pw)
{
    const auto source=f::Catalog();Populate(admin,source);const auto c=BuildItemCatalog(source).value();
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
