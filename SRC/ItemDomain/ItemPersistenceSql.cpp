#include "ItemPersistenceSql.hpp"
#include <item/OwnedGround.hpp>
#include <random>
#include <SQL/AsyncSQL.h>
#include <algorithm>
#include <charconv>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

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
constexpr std::string_view Columns="instance_id,category_id,local_id,owner_kind,owner_id,state_revision,refinement_level,stack_count,destroyed,bound_kind,bound_id,storage_kind,storage_id,anchor_cell,locked_attribute,accessory_capacity,accessory_grade,accessory_remaining_ms,accessory_permanent,state_sha256,automatic_remaining_raw,automatic_active,automatic_next_ms,toggle_remaining_ms,toggle_active,book_skill_id,polymorph_form_id,polymorph_book_grade,polymorph_practice,tool_training_points,tool_bait_power,companion_recall_requested,growth_pet_blob,pet_supply_blob";
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
        {"SELECT 1 FROM information_schema.TABLES WHERE (TABLE_SCHEMA='player' AND TABLE_NAME IN ('item_persistence_format','item_owner_fence','item_storage','item_wallet','item_operation_receipt','item_instance','item_instance_attribute','item_instance_socket','item_occupied_cell','item_recovery_schedule')) OR (TABLE_SCHEMA='protos' AND TABLE_NAME IN ('item_base','bonus_definition')) HAVING COUNT(*)=12 AND SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE')=12 AND (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_instance' AND COLUMN_NAME IN ('automatic_remaining_raw','automatic_active','automatic_next_ms','toggle_remaining_ms','toggle_active','book_skill_id','polymorph_form_id','polymorph_book_grade','polymorph_practice','tool_training_points','tool_bait_power','companion_recall_requested','growth_pet_blob','pet_supply_blob'))=14 AND (SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_book_learning' AND ENGINE='InnoDB')=1 AND (SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_pet_study_cooldown' AND ENGINE='InnoDB')=1",{},{},1},
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
    a.push_back(U::Blob(Bytes(*PersistentItemDigest(c,p))));
    if(p.automatic){a.push_back(U::UInt(p.automatic->remainingRaw));a.push_back(U::UInt(p.automatic->active));a.push_back(U::UInt(p.automatic->nextTickInMs));}
    else for(unsigned i=0;i<3;++i)a.push_back(U::Null());
    if(p.toggle){a.push_back(U::UInt(p.toggle->remainingMs));a.push_back(U::UInt(p.toggle->active));}
    else for(unsigned i=0;i<2;++i)a.push_back(U::Null());a.push_back(p.bookSkill ? U::UInt(*p.bookSkill) : U::Null());
    if(p.polymorph){a.push_back(p.polymorph->boundForm ? U::UInt(*p.polymorph->boundForm) : U::Null());a.push_back(U::UInt(p.polymorph->bookGrade));a.push_back(U::UInt(p.polymorph->practiceRemaining));}else for(unsigned i=0;i<3;++i)a.push_back(U::Null());
    if(p.tool){a.push_back(U::UInt(p.tool->trainingPoints));a.push_back(U::UInt(p.tool->baitPower));}else for(unsigned i=0;i<2;++i)a.push_back(U::Null());
    a.push_back(p.companion ? U::UInt(p.companion->recallRequested) : U::Null());
    // PrepareItemWrite has already validated the complete typed state.
    a.push_back(p.growthPet ? U::Blob(Bytes(*EncodeGrowthPetState(c,p.key,*p.growthPet))) : U::Null());
    a.push_back(p.petSupply ? U::Blob(Bytes(*EncodePetSupplyState(c,p.key,*p.petSupply))) : U::Null());
    return a;
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
    if(r.ground) tx.push_back({"SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME IN ('item_ground_core','item_ground_entry') HAVING COUNT(*)=2 AND SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE')=2",{},{},1});
    if(r.ground) tx.push_back({"SELECT run_generation FROM player.item_ground_core WHERE core_key=? AND run_generation=? FOR UPDATE",
        {U::Blob(r.ground->core),U::UInt(r.ground->run)},{},1});
    tx.push_back({"INSERT INTO player.item_operation_receipt(operation_id,request_sha256,catalog_revision,catalog_sha256,operation_kind,outcome_code,result_payload) VALUES(?,?,?,?,?,?,?)",
        {U::Blob(Bytes(r.operationId)),U::Blob(Bytes(*digest)),U::UInt(r.catalogRevision),U::Blob(Bytes(r.catalogHash)),U::UInt(static_cast<unsigned>(r.kind)),U::UInt(r.outcomeCode),U::Blob(Bytes(r.result))},1,{}});
    auto owners=r.owners;std::ranges::sort(owners,{},&ItemOwnerFence::owner);
    for(const auto& o:owners)
    {
        auto p=Owner(o.owner);Append(p,{U::UInt(o.sessionGeneration),U::UInt(o.revision)});
        tx.push_back({"UPDATE player.item_owner_fence SET state_revision=state_revision+1 WHERE owner_kind=? AND owner_id=? AND session_generation=? AND state_revision=?",std::move(p),1,{}});
    }
    if(r.petStudyCooldown)
    {
        const auto& cooldown=*r.petStudyCooldown;
        auto key=Owner(owners.front().owner);
        key.push_back(U::UInt(cooldown.skill));
        if(cooldown.beforeMs)
        {
            auto expected=key;
            expected.push_back(U::UInt(cooldown.beforeMs));
            tx.push_back({"SELECT 1 FROM player.item_pet_study_cooldown WHERE owner_kind=? AND owner_id=? AND skill_id=? AND ready_at_ms=? FOR UPDATE",std::move(expected),{},1});
            if(cooldown.afterMs && cooldown.afterMs!=cooldown.beforeMs)
            {
                SqlParams update{U::UInt(cooldown.afterMs)};
                update.insert(update.end(),key.begin(),key.end());
                tx.push_back({"UPDATE player.item_pet_study_cooldown SET ready_at_ms=? WHERE owner_kind=? AND owner_id=? AND skill_id=?",std::move(update),1,{}});
            }
        }
        else
        {
            tx.push_back({"SELECT 1 FROM player.item_pet_study_cooldown WHERE owner_kind=? AND owner_id=? AND skill_id=? FOR UPDATE",key,{},0});
            if(cooldown.afterMs)
            {
                key.push_back(U::UInt(cooldown.afterMs));
                tx.push_back({"INSERT INTO player.item_pet_study_cooldown(owner_kind,owner_id,skill_id,ready_at_ms) VALUES(?,?,?,?)",std::move(key),1,{}});
            }
        }
    }
    auto storages=r.storages;std::ranges::sort(storages,{},&ItemStorageLayout::storage);
    for(const auto& s:storages)
    {
        auto p=Storage(s.storage);Append(p,{U::UInt(s.columns),U::UInt(s.rows),U::UInt(s.pages)});
        if(s.storage.kind==ItemStorageKind::PetCarrier) {
            auto create=p;Append(create,Storage(s.storage));
            tx.push_back({"INSERT INTO player.item_storage(owner_kind,owner_id,storage_kind,storage_id,grid_columns,grid_rows,grid_pages) SELECT ?,?,?,?,?,?,? WHERE NOT EXISTS (SELECT 1 FROM player.item_storage WHERE owner_kind=? AND owner_id=? AND storage_kind=? AND storage_id=?)",std::move(create),{},{}});
        }
        tx.push_back({"SELECT 1 FROM player.item_storage WHERE owner_kind=? AND owner_id=? AND storage_kind=? AND storage_id=? AND grid_columns=? AND grid_rows=? AND grid_pages=? LOCK IN SHARE MODE",std::move(p),{},1});
    }
    auto wallets=r.wallets;std::ranges::sort(wallets,{},[](const auto& v){return std::pair{v.before.owner,v.before.currency};});
    for(const auto& w:wallets)
    {
        SqlParams p{U::UInt(w.after.balance),U::UInt(w.after.revision)};Append(p,Owner(w.before.owner));Append(p,{U::UInt(static_cast<unsigned>(w.before.currency)),U::UInt(w.before.revision),U::UInt(w.before.balance)});
        tx.push_back({"UPDATE player.item_wallet SET balance=?,state_revision=? WHERE owner_kind=? AND owner_id=? AND currency=? AND state_revision=? AND balance=?",std::move(p),1,{}});
    }
    auto items=r.items;std::ranges::sort(items,{},[](const auto& i){return i.after.id;});
    for(const auto& change:items)
    {
        if(RecallRequested(change.after) && change.before && !RecallRequested(*change.before))
        {
            // Owner fence is locked above. A current read also refuses a stale channel snapshot.
            // PetStateCodec v1 stores recallRequested as its final byte.
            tx.push_back({"SELECT instance_id FROM player.item_instance WHERE owner_kind=? AND owner_id=? AND "
                "(companion_recall_requested=1 OR (LEFT(growth_pet_blob,6)=X'414750540100' AND RIGHT(growth_pet_blob,1)=X'01')) FOR UPDATE",
                Owner(change.after.owner),{},0});
        }
    }
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
    for(const auto& change:r.items)if(change.before && change.before->placement &&
        change.before->placement->kind==ItemStorageKind::PetCarrier &&
        (!change.after.placement || change.after.placement->kind!=ItemStorageKind::PetCarrier)) {
        tx.push_back({"DELETE FROM player.item_storage WHERE owner_kind=? AND owner_id=? AND storage_kind=? AND storage_id=?",
            Storage({change.before->owner,ItemStorageKind::PetCarrier,change.before->placement->storageId}),1,{}});
    }
    if(r.ground)
    {
        const auto& g=*r.ground;
        if(g.create) tx.push_back({"INSERT INTO player.item_ground_entry(instance_id,core_key,run_generation) VALUES(?,?,?)",
            {U::UInt(g.instance),U::Blob(g.core),U::UInt(g.storedRun)},1,{}});
        else tx.push_back({"DELETE FROM player.item_ground_entry WHERE instance_id=? AND core_key=? AND run_generation=?",
            {U::UInt(g.instance),U::Blob(g.core),U::UInt(g.storedRun)},1,{}});
    }
    if(r.recovery)
    {
        const auto& a=*r.recovery;
        const auto before=EncodeRecoverySchedule(c,a.before),after=EncodeRecoverySchedule(c,a.after);
        const auto hash=RecoveryScheduleDigest(c,a.owner,a.after);
        if(!before || !after || !hash)return std::unexpected(E::InvalidStoredData);
        if(!a.before.revision)
        {
            tx.push_back({"SELECT 1 WHERE NOT EXISTS(SELECT 1 FROM player.item_recovery_schedule WHERE owner_kind=? AND owner_id=?)",Owner(a.owner),{},1});
            auto p=Owner(a.owner);Append(p,{U::UInt(a.after.revision),U::Blob(Bytes(*after)),U::Blob(Bytes(*hash)),U::Blob(Bytes(r.operationId))});
            tx.push_back({"INSERT INTO player.item_recovery_schedule(owner_kind,owner_id,state_revision,state_blob,state_sha256,last_operation) VALUES(?,?,?,?,?,?)",std::move(p),1,{}});
        }
        else
        {
            const auto oldHash=RecoveryScheduleDigest(c,a.owner,a.before);
            SqlParams p{U::UInt(a.after.revision),U::Blob(Bytes(*after)),U::Blob(Bytes(*hash)),U::Blob(Bytes(r.operationId))};Append(p,Owner(a.owner));Append(p,{U::UInt(a.before.revision),U::Blob(Bytes(*before)),U::Blob(Bytes(*oldHash))});
            tx.push_back({"UPDATE player.item_recovery_schedule SET state_revision=?,state_blob=?,state_sha256=?,last_operation=? WHERE owner_kind=? AND owner_id=? AND state_revision=? AND state_blob=? AND state_sha256=?",std::move(p),1,{}});
        }
    }
    if(r.learning)
    {
        const auto& a=*r.learning;const auto before=EncodeBookLearning(a.before),after=EncodeBookLearning(a.after);const auto hash=BookLearningDigest(a.owner,a.after);if(!before || !after || !hash)return std::unexpected(E::InvalidStoredData);
        if(!a.before.revision){tx.push_back({"SELECT 1 WHERE NOT EXISTS(SELECT 1 FROM player.item_book_learning WHERE owner_kind=? AND owner_id=?)",Owner(a.owner),{},1});auto p=Owner(a.owner);Append(p,{U::UInt(a.after.revision),U::Blob(Bytes(*after)),U::Blob(Bytes(*hash)),U::Blob(Bytes(r.operationId))});tx.push_back({"INSERT INTO player.item_book_learning(owner_kind,owner_id,state_revision,state_blob,state_sha256,last_operation) VALUES(?,?,?,?,?,?)",std::move(p),1,{}});}
        else{const auto oldHash=BookLearningDigest(a.owner,a.before);SqlParams p{U::UInt(a.after.revision),U::Blob(Bytes(*after)),U::Blob(Bytes(*hash)),U::Blob(Bytes(r.operationId))};Append(p,Owner(a.owner));Append(p,{U::UInt(a.before.revision),U::Blob(Bytes(*before)),U::Blob(Bytes(*oldHash))});tx.push_back({"UPDATE player.item_book_learning SET state_revision=?,state_blob=?,state_sha256=?,last_operation=? WHERE owner_kind=? AND owner_id=? AND state_revision=? AND state_blob=? AND state_sha256=?",std::move(p),1,{}});}
        const auto& c=a.after.character;
        tx.push_back({"UPDATE player.player SET typed_book_revision=?,exp=?,skill_point=?,sub_skill_point=?,horse_skill_point=?,skill_level=? WHERE id=? AND typed_book_revision=?",{U::UInt(a.after.revision),U::UInt(c.experience),U::UInt(c.points[0]),U::UInt(c.points[1]),U::UInt(c.points[2]),U::Blob(Bytes(LegacyBookSkillBytes(c))),U::UInt(a.owner.id),U::UInt(a.before.revision)},1,{}});
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
    // A current, locking read waits for an in-flight receipt INSERT to settle.
    // After CommitUnknown all write steps have run, so only then is an absent
    // receipt proof that the original transaction did not commit.
    tx.push_back({"SELECT request_sha256,catalog_revision,catalog_sha256,operation_kind,outcome_code,result_payload FROM player.item_operation_receipt WHERE operation_id=? LOCK IN SHARE MODE",{U::Blob(Bytes(r.operationId))},{},{}});
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
std::expected<bool,E> FindItemMissionReceipt(CAsyncSQL& db,const ItemCatalog& c,ItemOwnerFence fence,const ItemMission& mission,ItemMissionProof proof)
{
    const auto op=ItemMissionOperation(proof);const auto expected=ItemMissionReceipt(mission,proof);
    if(!op || !expected || proof.owner!=fence.owner || !fence.sessionGeneration)return std::unexpected(E::InvalidOperation);
    auto tx=Guards(c);auto owner=Owner(fence.owner);owner.push_back(U::UInt(fence.sessionGeneration));
    tx.push_back({"SELECT state_revision FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=? AND session_generation=? LOCK IN SHARE MODE",std::move(owner),{},1});
    tx.push_back({"SELECT request_sha256,catalog_revision,catalog_sha256,operation_kind,outcome_code,result_payload FROM player.item_operation_receipt WHERE operation_id=? LOCK IN SHARE MODE",{U::Blob(Bytes(*op))},{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    const auto rows=Result(*m,4,6,1);if(!rows)return std::unexpected(rows.error());if(rows->empty())return false;
    try
    {
        const auto& row=rows->front();(void)Array<32>(row[0]);(void)Array<32>(row[2]);
        if(!Number<std::uint64_t>(row[1]) || Number<unsigned>(row[3])!=static_cast<unsigned>(ItemOperationKind::Service) ||
           Number<std::uint16_t>(row[4])!=ItemMissionOutcome || Payload(row[5])!=*expected)return std::unexpected(E::ReceiptMismatch);
        return true;
    }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
}
std::expected<std::uint64_t,E> ClaimOwnerSession(CAsyncSQL& db,const ItemCatalog& c,ItemOwner owner)
{
    if(!ValidItemOwner(owner))return std::unexpected(E::InvalidOwner);
    auto tx=Guards(c);const auto first=tx.size();
    tx.push_back({"SELECT session_generation FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=? FOR UPDATE",Owner(owner),{},{}});
    tx.push_back({"UPDATE player.item_owner_fence SET session_generation=session_generation+1 WHERE owner_kind=? AND owner_id=? AND session_generation<18446744073709551615",Owner(owner),{},{}});
    tx.push_back({"SELECT session_generation FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=?",Owner(owner),{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto before=Result(*m,first,1,1),after=Result(*m,first+2,1,1);
    if(!before)return std::unexpected(before.error());if(!after)return std::unexpected(after.error());
    if(before->empty())return std::unexpected(E::MissingOwner);
    try
    {
        const auto old=Number<std::uint64_t>((*before)[0][0]),now=Number<std::uint64_t>((*after).at(0)[0]);
        if(now==old)return std::unexpected(E::LimitExceeded);
        if(now!=old+1)return std::unexpected(E::InvalidStoredData);
        return now;
    }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
    catch(const std::out_of_range&){return std::unexpected(E::InvalidStoredData);}
}
std::expected<ItemInstanceIdBlock,E> ReserveItemInstanceIds(CAsyncSQL& db,const ItemCatalog& c,std::uint32_t count)
{
    if(!count || count>MaxItemInstanceIdBlock)return std::unexpected(E::LimitExceeded);
    auto tx=Guards(c);const auto first=tx.size();
    tx.push_back({"SELECT next_instance_id FROM player.item_instance_id_allocator WHERE singleton=1 FOR UPDATE",{},{},{}});
    tx.push_back({"UPDATE player.item_instance_id_allocator SET next_instance_id=next_instance_id+? WHERE singleton=1 AND next_instance_id<=18446744073709551615-?",
                  {U::UInt(count),U::UInt(count)},{},{}});
    tx.push_back({"SELECT next_instance_id FROM player.item_instance_id_allocator WHERE singleton=1",{},{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto before=Result(*m,first,1,1),after=Result(*m,first+2,1,1);
    if(!before)return std::unexpected(before.error());if(!after)return std::unexpected(after.error());
    if(before->empty() || after->empty())return std::unexpected(E::InvalidStoredData);
    try
    {
        const auto start=Number<std::uint64_t>((*before)[0][0]),next=Number<std::uint64_t>((*after)[0][0]);
        if(next==start)return std::unexpected(E::LimitExceeded);
        if(start<(std::uint64_t{1}<<32) || next!=start+count)return std::unexpected(E::InvalidStoredData);
        return ItemInstanceIdBlock{start,next};
    }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
}
std::expected<bool,E> ProvisionOwner(CAsyncSQL& db,const ItemCatalog& c,ItemOwner owner,std::span<const ItemStorageLayout> storages,std::span<const ItemWallet> wallets)
{
    if(!ValidItemOwner(owner))return std::unexpected(E::InvalidOwner);
    if(storages.empty() || storages.size()>MaxItemCommitStorages)return std::unexpected(E::InvalidStorage);
    std::set<ItemStorage> uniqueStorages;
    for(const auto& l:storages)if(!ValidItemStorage(l) || l.storage.owner!=owner || !uniqueStorages.insert(l.storage).second)return std::unexpected(E::InvalidStorage);
    std::set<WalletCurrency> currencies;
    for(const auto& w:wallets)
        if(w.owner!=owner || !w.revision || w.balance>static_cast<std::uint64_t>(INT64_MAX) || !currencies.insert(w.currency).second ||
           (w.currency==WalletCurrency::Gold ? owner.kind!=ItemOwnerKind::Character : w.currency!=WalletCurrency::DragonCoin || owner.kind!=ItemOwnerKind::Account))
            return std::unexpected(E::InvalidWallet);
    const auto exists=[&]()->std::expected<bool,E>{
        auto tx=Guards(c);const auto step=tx.size();
        tx.push_back({"SELECT 1 FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=?",Owner(owner),{},{}});
        Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
        auto rows=Result(*m,step,1,1);if(!rows)return std::unexpected(rows.error());return !rows->empty();};
    if(auto found=exists();!found || *found)return found ? std::expected<bool,E>(false) : std::unexpected(found.error());
    auto tx=Guards(c);
    tx.push_back({"INSERT INTO player.item_owner_fence(owner_kind,owner_id,session_generation,state_revision) VALUES(?,?,1,1)",Owner(owner),1,{}});
    for(const auto& l:storages)
    {
        auto p=Storage(l.storage);p.push_back(U::UInt(l.columns));p.push_back(U::UInt(l.rows));p.push_back(U::UInt(l.pages));
        tx.push_back({"INSERT INTO player.item_storage(owner_kind,owner_id,storage_kind,storage_id,grid_columns,grid_rows,grid_pages) VALUES(?,?,?,?,?,?,?)",std::move(p),1,{}});
    }
    for(const auto& w:wallets)
    {
        auto p=Owner(owner);p.push_back(U::UInt(static_cast<unsigned>(w.currency)));p.push_back(U::UInt(w.revision));p.push_back(U::UInt(w.balance));
        tx.push_back({"INSERT INTO player.item_wallet(owner_kind,owner_id,currency,state_revision,balance) VALUES(?,?,?,?,?)",std::move(p),1,{}});
    }
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    if(m->bTransaction && !m->uiSQLErrno && m->transactionOutcome==SqlTransactionOutcome::Committed)return true;
    // A concurrent provisioner may have won; that owner is not ours to change.
    if(auto found=exists();found && *found)return false;
    return std::unexpected(Failure(*m));
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
    tx.push_back({"SELECT state_revision,state_blob,state_sha256 FROM player.item_recovery_schedule WHERE owner_kind=? AND owner_id=? LIMIT 2",Owner(owner),{},{}});
    tx.push_back({"SELECT state_revision,state_blob,state_sha256 FROM player.item_book_learning WHERE owner_kind=? AND owner_id=?",Owner(owner),{},{}});
    tx.push_back({"SELECT skill_id,ready_at_ms FROM player.item_pet_study_cooldown WHERE owner_kind=? AND owner_id=? LIMIT 4097",Owner(owner),{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto recoveryRows=Result(*m,10,3,1);auto learningRows=Result(*m,11,3,1);auto petCooldownRows=Result(*m,12,2,4096);
    auto fence=Result(*m,3,1,1),storageRows=Result(*m,4,5,MaxLoadedOwnerStorages),walletRows=Result(*m,5,3,2),
        itemRows=Result(*m,6,34,MaxLoadedOwnerItems),attributes=Result(*m,7,4,MaxLoadedOwnerItems*8),stones=Result(*m,8,7,MaxLoadedOwnerItems*3),cells=Result(*m,9,6,65536);
    for(const auto* r:{&fence,&storageRows,&walletRows,&itemRows,&attributes,&stones,&cells,&recoveryRows,&learningRows,&petCooldownRows})if(!*r)return std::unexpected(r->error());
    try
    {
        if(fence->size()!=1)throw Invalid{};
        LoadedItemOwner out;out.fence={owner,session,Number<std::uint64_t>((*fence)[0][0])};if(!out.fence.revision)throw Invalid{};
        for(const auto& row:*petCooldownRows)
        {
            const auto skill=Number<std::uint32_t>(row[0]);
            const auto ready=Number<std::uint64_t>(row[1]);
            if(owner.kind!=ItemOwnerKind::Character || !skill || !ready || ready>INT64_MAX ||
                !out.petStudyCooldowns.emplace(skill,ready).second)throw Invalid{};
        }
        if(!recoveryRows->empty())
        {
            const auto& row=recoveryRows->front();if(owner.kind!=ItemOwnerKind::Character || !row[1] || row[1]->size()>MaxRecoveryStateBytes)throw Invalid{};
            const auto decoded=DecodeRecoverySchedule(c,{reinterpret_cast<const std::uint8_t*>(row[1]->data()),row[1]->size()});
            if(!decoded || !decoded->revision || decoded->revision!=Number<std::uint64_t>(row[0]))throw Invalid{};
            const auto hash=RecoveryScheduleDigest(c,owner,*decoded);if(!hash || *hash!=Array<32>(row[2]))throw Invalid{};out.recovery=*decoded;
        }
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
        bool hasCompanionRecall=false;
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
            if(row[20] || row[21] || row[22])item.automatic=AutomaticRecoveryState{Number<std::int64_t>(row[20]),Boolean(row[21]),Number<std::uint64_t>(row[22])};
            if(row[23] || row[24])item.toggle=ToggleEffectState{Number<std::uint64_t>(row[23]),Boolean(row[24])};if(row[25])item.bookSkill=Number<std::uint16_t>(row[25]);
            if(row[26] || row[27] || row[28]){PolymorphInstanceState p;if(row[26])p.boundForm=Number<std::uint32_t>(row[26]);p.bookGrade=Number<std::uint16_t>(row[27]);p.practiceRemaining=Number<std::uint32_t>(row[28]);item.polymorph=p;}
            if(row[29] || row[30])item.tool=ToolInstanceState{Number<std::uint32_t>(row[29]),Number<std::uint32_t>(row[30])};
            if(row[31])
            {
                item.companion=CompanionState{};
                item.companion->recallRequested=Boolean(row[31]);
                if(item.companion->recallRequested && std::exchange(hasCompanionRecall,true))throw Invalid{};
            }
            if(row[32])
            {
                auto state=DecodeGrowthPetState(c,item.key,{reinterpret_cast<const std::uint8_t*>(row[32]->data()),row[32]->size()});
                if(!state)throw Invalid{};
                item.growthPet=std::move(*state);
                if(item.growthPet->recallRequested && std::exchange(hasCompanionRecall,true))throw Invalid{};
            }
            if(row[33]) {
                const auto state=DecodePetSupplyState(c,item.key,{reinterpret_cast<const std::uint8_t*>(row[33]->data()),row[33]->size()});
                if(!state)throw Invalid{};item.petSupply=*state;
            }
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
        if(!learningRows->empty()){const auto& row=learningRows->front();if(!row[1])throw Invalid{};const auto decoded=DecodeBookLearning({reinterpret_cast<const std::uint8_t*>(row[1]->data()),row[1]->size()});if(!decoded || !decoded->revision || decoded->revision!=Number<std::uint64_t>(row[0]) || BookLearningDigest(owner,*decoded)!=Array<32>(row[2]))throw Invalid{};out.learning=*decoded;}
        std::set<RecoveryPool> activePools;
        for(const auto& item:out.items)if(item.automatic && item.automatic->active)
        {const auto* entry=c.Find(item.key);const auto* r=entry ? std::get_if<ItemConsumableRow>(&entry->category) : nullptr;const auto* p=r ? c.FindRecovery(r->recoveryProfile) : nullptr;const auto* a=p ? std::get_if<AutomaticRecovery>(&p->Data().effect) : nullptr;if(!a || !activePools.insert(a->pool).second)throw Invalid{};}
        std::set<std::uint32_t> toggleGroups;
        for(const auto& item:out.items)if(item.toggle && item.toggle->active)
        {const auto* e=c.Find(item.key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;const auto* p=r && r->toggleEffectProfile ? c.FindToggleEffect(*r->toggleEffectProfile) : nullptr;if(!p || !toggleGroups.insert(p->group).second || toggleGroups.size()>MaxRuntimeToggleEffects)throw Invalid{};}
        if(seenStones.size()!=out.items.size()*3 || !ValidPetCarrierLinks(out.items))throw Invalid{};
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

std::expected<LoadedItemOwner,E> LoadCurrentItemOwner(CAsyncSQL& db,const ItemCatalog& c,ItemOwner owner)
{
    auto tx=Guards(c);const auto at=tx.size();
    tx.push_back({"SELECT session_generation FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=?",Owner(owner),{},{}});
    Message m(db.DirectTransactionPrepared(std::move(tx)));if(!m)return std::unexpected(E::SqlFailure);
    auto rows=Result(*m,at,1,1);if(!rows)return std::unexpected(rows.error());
    if(rows->empty())return std::unexpected(E::MissingOwner);
    try { return LoadItemOwner(db,c,owner,Number<std::uint64_t>((*rows)[0][0])); }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
}
std::expected<void,E> ProvisionGroundStorage(CAsyncSQL& db,const ItemCatalog& c,ItemOwner owner)
{
    if(owner.kind!=ItemOwnerKind::Character || !ValidItemOwner(owner))return std::unexpected(E::InvalidOwner);
    auto tx=Guards(c);
    tx.push_back({"SELECT state_revision FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=? FOR UPDATE",Owner(owner),{},1});
    auto p=Owner(owner);p.push_back(U::UInt(OwnedGroundStorageId));
    tx.push_back({"SELECT 1 FROM player.item_storage WHERE owner_kind=? AND owner_id=? HAVING COUNT(*)<16 OR SUM(storage_kind=7 AND storage_id=?)=1",p,{},1});
    tx.push_back({"INSERT IGNORE INTO player.item_storage(owner_kind,owner_id,storage_kind,storage_id,grid_columns,grid_rows,grid_pages) VALUES(?,?,7,?,256,256,1)",p,{},{}});
    tx.push_back({"SELECT 1 FROM player.item_storage WHERE owner_kind=? AND owner_id=? AND storage_kind=7 AND storage_id=? AND grid_columns=256 AND grid_rows=256 AND grid_pages=1",std::move(p),{},1});
    Message m(db.DirectTransactionPrepared(std::move(tx)));
    if(!m || !m->bTransaction || m->uiSQLErrno || m->transactionOutcome!=SqlTransactionOutcome::Committed)return std::unexpected(m ? Failure(*m) : E::SqlFailure);
    return {};
}
std::expected<std::uint64_t,E> BeginOwnedGroundRun(CAsyncSQL& db,const ItemCatalog& c,std::string_view core)
{
    if(core.empty() || core.size()>128)return std::unexpected(E::InvalidOperation);
    auto tx=Guards(c);
    tx.push_back({"INSERT IGNORE INTO player.item_ground_core(core_key,run_generation) VALUES(?,0)",{U::Blob(std::string(core))},{},{}});
    tx.push_back({"UPDATE player.item_ground_core SET run_generation=run_generation+1 WHERE core_key=? AND run_generation<18446744073709551615",{U::Blob(std::string(core))},1,{}});
    const auto at=tx.size();
    tx.push_back({"SELECT run_generation FROM player.item_ground_core WHERE core_key=? FOR UPDATE",{U::Blob(std::string(core))},{},1});
    Message begun(db.DirectTransactionPrepared(std::move(tx)));if(!begun)return std::unexpected(E::SqlFailure);
    auto generation=Result(*begun,at,1,1);if(!generation)return std::unexpected(generation.error());
    std::uint64_t run=0;
    try { run=Number<std::uint64_t>((*generation)[0][0]); }
    catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
    std::random_device random;
    for(;;)
    {
        auto list=Guards(c);const auto index=list.size();
        list.push_back({"SELECT g.instance_id,g.run_generation,i.owner_kind,i.owner_id FROM player.item_ground_entry g LEFT JOIN player.item_instance i ON i.instance_id=g.instance_id WHERE g.core_key=? AND g.run_generation<>? ORDER BY g.instance_id LIMIT 32",{U::Blob(std::string(core)),U::UInt(run)},{},{}});
        Message message(db.DirectTransactionPrepared(std::move(list)));if(!message)return std::unexpected(E::SqlFailure);
        auto rows=Result(*message,index,4,32);if(!rows)return std::unexpected(rows.error());
        if(rows->empty())return run;
        try
        {
            for(const auto& row:*rows)
            {
                auto id=ItemInstanceId::Create(Number<std::uint64_t>(row[0]));if(!id)throw Invalid{};
                const auto owner=ReadOwner(row,2);bool finished=false;
                for(unsigned retry=0;retry<8 && !finished;++retry)
                {
                    auto state=LoadCurrentItemOwner(db,c,owner);if(!state)return std::unexpected(state.error());
                    std::array<std::uint8_t,16> op{};for(auto& b:op)b=static_cast<std::uint8_t>(random());op[0]|=1;
                    auto commit=MakeGroundExpireCommit(c,op,state->fence,state->storages,state->items,*id);
                    if(!commit)return std::unexpected(commit.error());
                    commit->ground=ItemGroundChange{std::string(core),run,Number<std::uint64_t>(row[1]),id->Value(),false};
                    auto written=CommitItems(db,c,*commit);
                    if(written)finished=true;
                    else if(written.error()!=E::Conflict)return std::unexpected(written.error());
                }
                if(!finished)return std::unexpected(E::Conflict);
            }
        }
        catch(const Invalid&){return std::unexpected(E::InvalidStoredData);}
    }
}
} // namespace auriga::item::persistence
