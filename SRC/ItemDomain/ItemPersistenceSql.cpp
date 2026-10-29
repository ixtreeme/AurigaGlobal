#include "ItemPersistenceSql.hpp"
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <charconv>
#include <map>
#include <memory>
#include <set>
#include <string_view>

namespace auriga::item::persistence
{
namespace
{
using E=ItemPersistenceError;
using Message=std::unique_ptr<SQLMsg>;
using Cell=std::optional<std::string>;
using Row=std::vector<Cell>;
using Rows=std::vector<Row>;
using U=SqlParam;
constexpr std::string_view Columns="instance_id,category_id,local_id,owner_kind,owner_id,state_revision,refinement_level,stack_count,destroyed,bound_kind,bound_id,storage_kind,storage_id,anchor_cell,locked_attribute,accessory_capacity,accessory_grade,accessory_remaining_ms,accessory_permanent,state_sha256";
template<class Range> std::string Bytes(const Range& values){std::string out;out.reserve(values.size());for(auto b:values)out.push_back(static_cast<char>(b));return out;}
SqlParams Owner(ItemOwner o){return {U::UInt(static_cast<unsigned>(o.kind)),U::UInt(o.id)};}
void Append(SqlParams& to,SqlParams from){for(auto& p:from)to.push_back(std::move(p));}
SqlParams Storage(ItemStorage s){auto p=Owner(s.owner);p.push_back(U::UInt(static_cast<unsigned>(s.kind)));p.push_back(U::UInt(s.id));return p;}
E Failure(const SQLMsg& m)
{
    if(m.transactionOutcome==SqlTransactionOutcome::CommitUnknown)return E::CommitUnknown;
    if(m.transactionOutcome==SqlTransactionOutcome::Conflict)return E::Conflict;
    return E::SqlFailure;
}
SqlTransaction Guards(const ItemCatalog& c)
{
    // Shared catalog fence, not a global exclusive lock for all item commits.
    // The stopped publisher must replace this pin with the catalog release.
    return {
        {"SELECT 1 WHERE @@tx_isolation='REPEATABLE-READ' AND @@foreign_key_checks=1 AND @@check_constraint_checks=1",{},{},1},
        {"SELECT 1 FROM player.item_persistence_format WHERE singleton=1 AND schema_version=1 AND catalog_revision=? AND catalog_sha256=? LOCK IN SHARE MODE",
            {U::UInt(c.Revision()),U::Blob(Bytes(c.ContentHash()))},{},1},
        {"SELECT 1 FROM information_schema.TABLES WHERE (TABLE_SCHEMA='player' AND TABLE_NAME IN ('item_persistence_format','item_owner_fence','item_storage','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell')) OR (TABLE_SCHEMA='protos' AND TABLE_NAME IN ('item_base','bonus_definition')) HAVING COUNT(*)=11 AND SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE')=11",{},{},1},
    };
}
SqlParams ItemValues(const ItemCatalog& c,const PersistentItem& p)
{
    SqlParams a{U::UInt(p.id.Value()),U::UInt(static_cast<unsigned>(p.key.Category())),U::UInt(p.key.LocalId())};Append(a,Owner(p.owner));
    a.push_back(U::UInt(p.revision));a.push_back(U::UInt(p.refinementLevel));a.push_back(U::UInt(p.count));a.push_back(U::UInt(p.destroyed));
    if(p.boundTo)Append(a,Owner(*p.boundTo));else{a.push_back(U::Null());a.push_back(U::Null());}
    if(p.placement){a.push_back(U::UInt(static_cast<unsigned>(p.placement->kind)));a.push_back(U::UInt(p.placement->storageId));a.push_back(U::UInt(p.placement->anchor));}
    else for(unsigned i=0;i<3;++i)a.push_back(U::Null());
    a.push_back(p.attributes.lockedSlot ? U::UInt(*p.attributes.lockedSlot) : U::Null());
    if(p.accessory){a.push_back(U::UInt(p.accessory->unlockedCapacity));a.push_back(U::UInt(p.accessory->activeGrade));a.push_back(U::UInt(p.accessory->remainingMs));a.push_back(U::UInt(p.accessory->permanent));}
    else for(unsigned i=0;i<4;++i)a.push_back(U::Null());
    a.push_back(U::Blob(Bytes(*PersistentItemDigest(c,p))));return a;
}
std::string Marks(std::size_t n){std::string s;for(std::size_t i=0;i<n;++i){if(i)s+=",";s+="?";}return s;}
std::size_t AttributeCount(const PersistentItem& p){return std::ranges::count_if(p.attributes.slots,[](const auto& a){return a.has_value();});}
const ItemStorageLayout& Layout(const ItemCommit& r,const PersistentItem& p)
{return *std::ranges::find(r.storages,ItemStorage{p.owner,p.placement->kind,p.placement->storageId},&ItemStorageLayout::storage);}
void InsertChildren(SqlTransaction& tx,const PersistentItem& p)
{
    std::string query;SqlParams params;std::size_t count=0;
    for(std::size_t i=0;i<p.attributes.slots.size();++i)if(const auto& a=p.attributes.slots[i])
    {
        if(count++)query+=",";query+="(?,?,?,?)";Append(params,{U::UInt(p.id.Value()),U::UInt(i),U::UInt(static_cast<std::uint32_t>(a->bonus)),U::Int(a->valueScaled)});
    }
    if(count)tx.push_back({"INSERT INTO player.item_instance_attribute(instance_id,slot_index,bonus_id,value_scaled) VALUES"+query,std::move(params),count,{}});
    query.clear();params.clear();
    // All three rows are persisted, including closed/empty/broken slots.
    for(std::size_t i=0;i<p.stones.size();++i)
    {
        if(i)query+=",";query+="(?,?,?,?,?,?,?)";const auto& s=p.stones[i];
        Append(params,{U::UInt(p.id.Value()),U::UInt(i),U::UInt(static_cast<unsigned>(s.state)),U::UInt(s.grade),
            s.stone ? U::UInt(static_cast<unsigned>(s.stone->Category())) : U::Null(),s.stone ? U::UInt(s.stone->LocalId()) : U::Null(),U::UInt(s.refinementLevel)});
    }
    tx.push_back({"INSERT INTO player.item_instance_socket(instance_id,slot_index,socket_state,socket_grade,stone_category,stone_local,stone_level) VALUES"+query,std::move(params),3,{}});
}
std::expected<Rows,E> Result(SQLMsg& m,std::size_t step,std::size_t columns,std::size_t maximumRows)
{
    if(!m.bTransaction || m.uiSQLErrno || m.transactionOutcome!=SqlTransactionOutcome::Committed)return std::unexpected(Failure(m));
    if(m.transactionResultOffsets.size()!=m.transaction.size() || step>=m.transactionResultOffsets.size())return std::unexpected(E::InvalidStoredData);
    const auto pos=m.transactionResultOffsets[step];
    if(pos>=m.vec_pkResult.size() || (step+1<m.transactionResultOffsets.size() && m.transactionResultOffsets[step+1]!=pos+1))return std::unexpected(E::InvalidStoredData);
    auto* r=m.vec_pkResult[pos];
    if(!r || !r->bPrepared || r->uiNumRows>maximumRows || r->preparedRows.size()!=r->uiNumRows || r->preparedLengths.size()!=r->uiNumRows)return std::unexpected(E::InvalidStoredData);
    Rows out;std::size_t total=0;
    for(std::size_t i=0;i<r->preparedRows.size();++i)
    {
        const auto& row=r->preparedRows[i];const auto& lengths=r->preparedLengths[i];
        if(row.size()!=columns || lengths.size()!=columns)return std::unexpected(E::InvalidStoredData);
        Row next;
        for(std::size_t j=0;j<columns;++j)
        {
            if(lengths[j]>8192 || total+lengths[j]>16*1024*1024)return std::unexpected(E::LimitExceeded);total+=lengths[j];
            if(row[j])next.emplace_back(std::string(row[j],lengths[j]));else{if(lengths[j])return std::unexpected(E::InvalidStoredData);next.emplace_back();}
        }
        out.push_back(std::move(next));
    }
    return out;
}
struct Invalid {};
template<class T> T Number(const Cell& cell)
{
    if(!cell || cell->empty())throw Invalid{};T n{};
    const auto [end,error]=std::from_chars(cell->data(),cell->data()+cell->size(),n);
    if(error!=std::errc{} || end!=cell->data()+cell->size())throw Invalid{};return n;
}
bool Boolean(const Cell& cell){auto n=Number<unsigned>(cell);if(n>1)throw Invalid{};return n!=0;}
ItemOwner ReadOwner(const Row& r,std::size_t at){return {static_cast<ItemOwnerKind>(Number<std::uint8_t>(r.at(at))),Number<std::uint64_t>(r.at(at+1))};}
ItemKey ReadKey(const Cell& cat,const Cell& local)
{auto key=ItemKey::Create(Number<std::uint16_t>(cat),Number<std::uint32_t>(local));if(!key)throw Invalid{};return *key;}
template<std::size_t N> std::array<std::uint8_t,N> Array(const Cell& cell)
{
    if(!cell || cell->size()!=N)throw Invalid{};std::array<std::uint8_t,N> out{};
    for(std::size_t i=0;i<N;++i)out[i]=static_cast<std::uint8_t>(static_cast<unsigned char>((*cell)[i]));return out;
}
std::vector<std::uint8_t> Payload(const Cell& cell)
{if(!cell || cell->size()>MaxItemCommitResult)throw Invalid{};std::vector<std::uint8_t> out;for(auto b:*cell)out.push_back(static_cast<unsigned char>(b));return out;}
}
std::expected<SqlTransaction,E> PrepareItemWrite(const ItemCatalog& c,const ItemCommit& r)
{
    const auto digest=ItemCommitDigest(c,r);if(!digest)return std::unexpected(digest.error());
    auto tx=Guards(c);
    tx.push_back({"INSERT INTO player.item_operation_receipt(operation_id,request_sha256,catalog_revision,catalog_sha256,operation_kind,outcome_code,result_payload) VALUES(?,?,?,?,?,?,?)",
        {U::Blob(Bytes(r.operationId)),U::Blob(Bytes(*digest)),U::UInt(r.catalogRevision),U::Blob(Bytes(r.catalogHash)),U::UInt(static_cast<unsigned>(r.kind)),U::UInt(r.outcomeCode),U::Blob(Bytes(r.result))},1,{}});
    auto owners=r.owners;std::ranges::sort(owners,{},&ItemOwnerFence::owner);
    for(const auto& o:owners)
    {
        auto p=Owner(o.owner);Append(p,{U::UInt(o.sessionGeneration),U::UInt(o.revision)});
        tx.push_back({"UPDATE player.item_owner_fence SET state_revision=state_revision+1 WHERE owner_kind=? AND owner_id=? AND session_generation=? AND state_revision=?",std::move(p),1,{}});
    }
    auto storages=r.storages;std::ranges::sort(storages,{},&ItemStorageLayout::storage);
    for(const auto& s:storages)
    {
        auto p=Storage(s.storage);Append(p,{U::UInt(s.columns),U::UInt(s.rows),U::UInt(s.pages)});
        tx.push_back({"SELECT 1 FROM player.item_storage WHERE owner_kind=? AND owner_id=? AND storage_kind=? AND storage_id=? AND grid_columns=? AND grid_rows=? AND grid_pages=? LOCK IN SHARE MODE",std::move(p),{},1});
    }
    auto wallets=r.wallets;std::ranges::sort(wallets,{},[](const auto& v){return std::pair{v.before.owner,v.before.currency};});
    for(const auto& w:wallets)
    {
        SqlParams p{U::UInt(w.after.balance),U::UInt(w.after.revision)};Append(p,Owner(w.before.owner));Append(p,{U::UInt(static_cast<unsigned>(w.before.currency)),U::UInt(w.before.revision),U::UInt(w.before.balance)});
        tx.push_back({"UPDATE player.item_wallet SET balance=?,state_revision=? WHERE owner_kind=? AND owner_id=? AND currency=? AND state_revision=? AND balance=?",std::move(p),1,{}});
    }
    auto items=r.items;std::ranges::sort(items,{},[](const auto& i){return i.after.id;});
    std::size_t cellBudget=0;
    // Release ALL old footprints before acquiring any new one, allowing a swap
    // without a transient overlap. A failure rolls the entire release back.
    for(const auto& i:items)if(i.before)
    {
        const auto& b=*i.before;auto expectedValues=ItemValues(c,b);std::string predicates;std::size_t start=0,index=0;
        while(start<Columns.size())
        {
            const auto end=Columns.find(',',start);if(index++)predicates+=" AND ";
            predicates+=std::string(Columns.substr(start,end==Columns.npos ? end : end-start))+"<=>?";
            if(end==Columns.npos)break;start=end+1;
        }
        tx.push_back({"SELECT 1 FROM player.item_instance WHERE "+predicates+" AND destroyed=0 FOR UPDATE",std::move(expectedValues),{},1});
        auto cells=OccupiedItemCells(c,b,Layout(r,b));if(!cells)return std::unexpected(cells.error());cellBudget+=cells->size();
        auto cellParams=Owner(b.owner);Append(cellParams,{U::UInt(static_cast<unsigned>(b.placement->kind)),U::UInt(b.placement->storageId),U::UInt(b.id.Value())});
        for(auto cell:*cells)cellParams.push_back(U::UInt(cell));
        tx.push_back({"DELETE FROM player.item_occupied_cell WHERE owner_kind=? AND owner_id=? AND storage_kind=? AND storage_id=? AND instance_id=? AND cell_index IN ("+Marks(cells->size())+")",std::move(cellParams),cells->size(),{}});
        tx.push_back({"DELETE FROM player.item_occupied_cell WHERE instance_id=?",{U::UInt(b.id.Value())},0,{}});
        SqlParams attributeParams{U::UInt(b.id.Value())};std::string attributeMatch;
        for(std::size_t slot=0;slot<b.attributes.slots.size();++slot)if(const auto& a=b.attributes.slots[slot])
        {
            if(!attributeMatch.empty())attributeMatch+=",";attributeMatch+="(?,?,?)";
            Append(attributeParams,{U::UInt(slot),U::UInt(static_cast<std::uint32_t>(a->bonus)),U::Int(a->valueScaled)});
        }
        const auto attributeFilter=attributeMatch.empty() ? std::string{} : " AND (slot_index,bonus_id,value_scaled) IN ("+attributeMatch+")";
        tx.push_back({"DELETE FROM player.item_instance_attribute WHERE instance_id=?"+attributeFilter,std::move(attributeParams),AttributeCount(b),{}});
        if(AttributeCount(b))tx.push_back({"DELETE FROM player.item_instance_attribute WHERE instance_id=?",{U::UInt(b.id.Value())},0,{}});
        SqlParams socketParams{U::UInt(b.id.Value())};std::string socketMatch;
        for(std::size_t slot=0;slot<b.stones.size();++slot)
        {
            if(!socketMatch.empty())socketMatch+=" OR ";socketMatch+="(slot_index=? AND socket_state=? AND socket_grade=? AND stone_category<=>? AND stone_local<=>? AND stone_level=?)";
            const auto& s=b.stones[slot];Append(socketParams,{U::UInt(slot),U::UInt(static_cast<unsigned>(s.state)),U::UInt(s.grade),
                s.stone ? U::UInt(static_cast<unsigned>(s.stone->Category())) : U::Null(),s.stone ? U::UInt(s.stone->LocalId()) : U::Null(),U::UInt(s.refinementLevel)});
        }
        tx.push_back({"DELETE FROM player.item_instance_socket WHERE instance_id=? AND ("+socketMatch+")",std::move(socketParams),3,{}});
        tx.push_back({"DELETE FROM player.item_instance_socket WHERE instance_id=?",{U::UInt(b.id.Value())},0,{}});
    }
    for(const auto& i:items)
    {
        auto p=ItemValues(c,i.after);p.push_back(U::Blob(Bytes(r.operationId)));
        if(!i.before)tx.push_back({"INSERT INTO player.item_instance("+std::string(Columns)+",last_operation) VALUES("+Marks(p.size())+")",std::move(p),1,{}});
        else
        {
            // instance_id is immutable and is the first serialized column.
            p.erase(p.begin());p.push_back(U::UInt(i.after.id.Value()));
            std::string set;std::string columns(Columns.substr(Columns.find(',')+1));columns+=",last_operation";
            std::size_t start=0;
            while(start<columns.size()){const auto end=columns.find(',',start);if(start)set+=",";set+=columns.substr(start,end==columns.npos ? end : end-start)+"=?";if(end==columns.npos)break;start=end+1;}
            tx.push_back({"UPDATE player.item_instance SET "+set+" WHERE instance_id=?",std::move(p),1,{}});
        }
        InsertChildren(tx,i.after);
        if(i.after.placement)
        {
            auto cells=OccupiedItemCells(c,i.after,Layout(r,i.after));if(!cells)return std::unexpected(cells.error());cellBudget+=cells->size();
            if(cellBudget>512)return std::unexpected(E::LimitExceeded);
            std::string values;SqlParams pCells;
            for(auto cell:*cells){if(!values.empty())values+=",";values+="(?,?,?,?,?,?)";Append(pCells,Storage({i.after.owner,i.after.placement->kind,i.after.placement->storageId}));Append(pCells,{U::UInt(cell),U::UInt(i.after.id.Value())});}
            tx.push_back({"INSERT INTO player.item_occupied_cell(owner_kind,owner_id,storage_kind,storage_id,cell_index,instance_id) VALUES"+values,std::move(pCells),cells->size(),{}});
        }
    }
    std::size_t parameters=0,bytes=0;
    for(const auto& step:tx){parameters+=step.params.size();bytes+=step.query.size();for(const auto& p:step.params)bytes+=p.bytes.size();if(step.query.size()>SqlTransactionLimits::QueryBytes)return std::unexpected(E::LimitExceeded);}
    if(tx.size()>SqlTransactionLimits::Steps || parameters>SqlTransactionLimits::Parameters || bytes>SqlTransactionLimits::TotalBytes || cellBudget>512)return std::unexpected(E::LimitExceeded);
    return tx;
}
std::expected<ItemReceipt,E> ItemWriteCompleted(const ItemCatalog& c,const ItemCommit& r,SQLMsg& m)
{
    if(!m.bTransaction || m.uiSQLErrno || m.transactionOutcome!=SqlTransactionOutcome::Committed)return std::unexpected(Failure(m));
    const auto expected=PrepareItemWrite(c,r);if(!expected)return std::unexpected(expected.error());
    // The completion must belong to this exact queued, owning SQL job.
    if(m.transaction.size()!=expected->size())return std::unexpected(E::InvalidStoredData);
    if(m.transactionResultOffsets.size()!=expected->size() || m.vec_pkResult.size()!=expected->size())return std::unexpected(E::InvalidStoredData);
    for(std::size_t i=0;i<expected->size();++i)
    {
        const auto& a=(*expected)[i];const auto& b=m.transaction[i];
        if(m.transactionResultOffsets[i]!=i || !m.vec_pkResult[i] || !m.vec_pkResult[i]->bPrepared)return std::unexpected(E::InvalidStoredData);
        const auto& result=*m.vec_pkResult[i];
        if((a.expectedAffectedRows && result.uiAffectedRows!=*a.expectedAffectedRows) || (a.expectedResultRows && result.uiNumRows!=*a.expectedResultRows))return std::unexpected(E::InvalidStoredData);
        if(a.query!=b.query || a.params.size()!=b.params.size() || a.expectedAffectedRows!=b.expectedAffectedRows || a.expectedResultRows!=b.expectedResultRows)return std::unexpected(E::InvalidStoredData);
        for(std::size_t j=0;j<a.params.size();++j){const auto& x=a.params[j];const auto& y=b.params[j];if(x.type!=y.type || x.i64!=y.i64 || x.u64!=y.u64 || x.bytes!=y.bytes)return std::unexpected(E::InvalidStoredData);}
    }
    return ItemReceipt{*ItemCommitDigest(c,r),r.outcomeCode,r.result};
}
std::expected<std::optional<ItemReceipt>,E> FindItemReceipt(CAsyncSQL& db,const ItemCatalog& c,const ItemCommit& r)
{
    const auto digest=ItemCommitDigest(c,r);if(!digest)return std::unexpected(digest.error());
    auto tx=Guards(c);
    tx.push_back({"SELECT request_sha256,catalog_revision,catalog_sha256,operation_kind,outcome_code,result_payload FROM player.item_operation_receipt WHERE operation_id=?",{U::Blob(Bytes(r.operationId))},{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto rows=Result(*m,3,6,1);if(!rows)return std::unexpected(rows.error());if(rows->empty())return std::optional<ItemReceipt>{};
    try
    {
        const auto& row=rows->front();const auto hash=Array<32>(row[0]);const auto code=Number<std::uint16_t>(row[4]);auto payload=Payload(row[5]);
        if(hash!=*digest || Number<std::uint64_t>(row[1])!=r.catalogRevision || Array<32>(row[2])!=r.catalogHash ||
           Number<unsigned>(row[3])!=static_cast<unsigned>(r.kind) || code!=r.outcomeCode || payload!=r.result)return std::unexpected(E::ReceiptMismatch);
        return std::optional<ItemReceipt>{ItemReceipt{hash,code,std::move(payload)}};
    }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
}
std::expected<ItemWriteResult,E> CommitItems(CAsyncSQL& db,const ItemCatalog& c,const ItemCommit& r)
{
    auto receipt=FindItemReceipt(db,c,r);if(!receipt)return std::unexpected(receipt.error());if(*receipt)return ItemWriteResult{**receipt,true};
    auto tx=PrepareItemWrite(c,r);if(!tx)return std::unexpected(tx.error());Message m(db.DirectTransactionPrepared(std::move(*tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto result=ItemWriteCompleted(c,r,*m);if(result)return ItemWriteResult{*result,false};
    // A concurrent, already committed identical operation can race the initial
    // lookup. Reconcile only a confirmed rolled-back duplicate, never unknown.
    if(m->uiSQLErrno==1062 && m->transactionOutcome==SqlTransactionOutcome::RolledBack)
    {receipt=FindItemReceipt(db,c,r);if(!receipt)return std::unexpected(receipt.error());if(*receipt)return ItemWriteResult{**receipt,true};}
    return std::unexpected(result.error());
}
std::expected<LoadedItemOwner,E> LoadItemOwner(CAsyncSQL& db,const ItemCatalog& c,ItemOwner owner,std::uint64_t session)
{
    if(!ValidItemOwner(owner) || !session)return std::unexpected(E::InvalidOwner);
    auto tx=Guards(c);auto ownerParams=Owner(owner);ownerParams.push_back(U::UInt(session));
    tx.push_back({"SELECT state_revision FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=? AND session_generation=?",std::move(ownerParams),{},1});
    tx.push_back({"SELECT storage_kind,storage_id,grid_columns,grid_rows,grid_pages FROM player.item_storage WHERE owner_kind=? AND owner_id=? LIMIT 17",Owner(owner),{},{}});
    tx.push_back({"SELECT currency,state_revision,balance FROM player.item_wallet WHERE owner_kind=? AND owner_id=? LIMIT 3",Owner(owner),{},{}});
    tx.push_back({"SELECT "+std::string(Columns)+" FROM player.item_instance WHERE owner_kind=? AND owner_id=? AND destroyed=0 ORDER BY instance_id LIMIT 4097",Owner(owner),{},{}});
    tx.push_back({"SELECT a.instance_id,a.slot_index,a.bonus_id,a.value_scaled FROM player.item_instance_attribute a JOIN player.item_instance i ON i.instance_id=a.instance_id WHERE i.owner_kind=? AND i.owner_id=? AND i.destroyed=0 ORDER BY a.instance_id,a.slot_index LIMIT 32769",Owner(owner),{},{}});
    tx.push_back({"SELECT s.instance_id,s.slot_index,s.socket_state,s.socket_grade,s.stone_category,s.stone_local,s.stone_level FROM player.item_instance_socket s JOIN player.item_instance i ON i.instance_id=s.instance_id WHERE i.owner_kind=? AND i.owner_id=? AND i.destroyed=0 ORDER BY s.instance_id,s.slot_index LIMIT 12289",Owner(owner),{},{}});
    // Include orphan/wrong-owner reservations too, rather than silently hiding
    // them behind a join to an instance with a matching owner.
    tx.push_back({"SELECT c.storage_kind,c.storage_id,c.cell_index,c.instance_id,c.owner_kind,c.owner_id FROM player.item_occupied_cell c LEFT JOIN player.item_instance i ON i.instance_id=c.instance_id WHERE (c.owner_kind=? AND c.owner_id=?) OR (i.owner_kind=? AND i.owner_id=? AND i.destroyed=0) LIMIT 65537",[&]{auto p=Owner(owner);Append(p,Owner(owner));return p;}(),{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto fence=Result(*m,3,1,1),storageRows=Result(*m,4,5,MaxItemCommitStorages),walletRows=Result(*m,5,3,2),
        itemRows=Result(*m,6,20,MaxLoadedOwnerItems),attributes=Result(*m,7,4,MaxLoadedOwnerItems*8),stones=Result(*m,8,7,MaxLoadedOwnerItems*3),cells=Result(*m,9,6,65536);
    for(const auto* r:{&fence,&storageRows,&walletRows,&itemRows,&attributes,&stones,&cells})if(!*r)return std::unexpected(r->error());
    try
    {
        if(fence->size()!=1)throw Invalid{};
        LoadedItemOwner out;out.fence={owner,session,Number<std::uint64_t>((*fence)[0][0])};if(!out.fence.revision)throw Invalid{};
        std::map<ItemStorage,ItemStorageLayout> layouts;
        for(const auto& row:*storageRows)
        {
            ItemStorageLayout l{{owner,static_cast<ItemStorageKind>(Number<std::uint8_t>(row[0])),Number<std::uint64_t>(row[1])},Number<std::uint16_t>(row[2]),Number<std::uint16_t>(row[3]),Number<std::uint16_t>(row[4])};
            if(!ValidItemStorage(l) || !layouts.emplace(l.storage,l).second)throw Invalid{};out.storages.push_back(l);
        }
        for(const auto& row:*walletRows)
        {
            ItemWallet w{owner,static_cast<WalletCurrency>(Number<std::uint8_t>(row[0])),Number<std::uint64_t>(row[1]),Number<std::uint64_t>(row[2])};
            if(!w.revision || w.balance>INT64_MAX || (w.currency==WalletCurrency::Gold ? owner.kind!=ItemOwnerKind::Character : w.currency!=WalletCurrency::DragonCoin || owner.kind!=ItemOwnerKind::Account))throw Invalid{};
            if(std::ranges::any_of(out.wallets,[&](const auto& old){return old.currency==w.currency;}))throw Invalid{};out.wallets.push_back(w);
        }
        std::map<std::uint64_t,std::size_t> indexes;std::vector<ItemCatalogDigest> hashes;
        for(const auto& row:*itemRows)
        {
            const auto id=ItemInstanceId::Create(Number<std::uint64_t>(row[0]));if(!id)throw Invalid{};
            PersistentItem item{*id,ReadKey(row[1],row[2]),ReadOwner(row,3)};
            item.revision=Number<std::uint64_t>(row[5]);item.refinementLevel=Number<std::uint16_t>(row[6]);item.count=Number<std::uint32_t>(row[7]);item.destroyed=Boolean(row[8]);
            if(item.owner!=owner || item.destroyed)throw Invalid{};
            if(row[9] || row[10])item.boundTo=ReadOwner(row,9);
            if(row[11] || row[12] || row[13])item.placement=ItemPlacement{static_cast<ItemStorageKind>(Number<std::uint8_t>(row[11])),Number<std::uint64_t>(row[12]),Number<std::uint32_t>(row[13])};
            if(row[14])item.attributes.lockedSlot=Number<std::uint8_t>(row[14]);
            if(row[15] || row[16] || row[17] || row[18])item.accessory=AccessorySocketState{Number<std::uint8_t>(row[15]),Number<std::uint8_t>(row[16]),Number<std::uint64_t>(row[17]),Boolean(row[18])};
            if(!indexes.emplace(id->Value(),out.items.size()).second)throw Invalid{};out.items.push_back(item);hashes.push_back(Array<32>(row[19]));
        }
        std::set<std::pair<std::uint64_t,unsigned>> seenAttributes,seenStones;
        for(const auto& row:*attributes)
        {
            const auto id=Number<std::uint64_t>(row[0]);const auto slot=Number<unsigned>(row[1]);
            if(!indexes.contains(id) || slot>=8 || !seenAttributes.emplace(id,slot).second)throw Invalid{};
            out.items[indexes.at(id)].attributes.slots[slot]=BonusEntry{static_cast<BonusId>(Number<std::uint32_t>(row[2])),Number<std::int64_t>(row[3])};
        }
        for(const auto& row:*stones)
        {
            const auto id=Number<std::uint64_t>(row[0]);const auto slot=Number<unsigned>(row[1]);
            if(!indexes.contains(id) || slot>=3 || !seenStones.emplace(id,slot).second)throw Invalid{};
            StoneSocketSlot s{static_cast<StoneSocketState>(Number<std::uint8_t>(row[2])),Number<std::uint8_t>(row[3]),{},Number<std::uint16_t>(row[6])};
            if(row[4] || row[5])s.stone=ReadKey(row[4],row[5]);out.items[indexes.at(id)].stones[slot]=s;
        }
        if(seenStones.size()!=out.items.size()*3)throw Invalid{};
        using Reservation=std::tuple<ItemStorage,std::uint32_t,std::uint64_t>;std::set<Reservation> actual,expected;
        for(std::size_t n=0;n<out.items.size();++n)
        {
            const auto& i=out.items[n];auto hash=PersistentItemDigest(c,i);if(!hash || *hash!=hashes[n])throw Invalid{};
            const ItemStorage storage{i.owner,i.placement->kind,i.placement->storageId};const auto layout=layouts.find(storage);if(layout==layouts.end())throw Invalid{};
            auto footprint=OccupiedItemCells(c,i,layout->second);if(!footprint)throw Invalid{};
            for(auto cell:*footprint)expected.emplace(storage,cell,i.id.Value());
        }
        for(const auto& row:*cells)
        {
            ItemStorage storage{ReadOwner(row,4),static_cast<ItemStorageKind>(Number<std::uint8_t>(row[0])),Number<std::uint64_t>(row[1])};
            if(!actual.emplace(storage,Number<std::uint32_t>(row[2]),Number<std::uint64_t>(row[3])).second)throw Invalid{};
        }
        if(actual!=expected)throw Invalid{};return out;
    }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
}
} // namespace auriga::item::persistence
