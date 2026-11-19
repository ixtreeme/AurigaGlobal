#include "DungeonEntrySql.hpp"
#include <SQL/AsyncSQL.h>
#include <openssl/rand.h>
#include <algorithm>
#include <charconv>
#include <map>
#include <memory>
namespace auriga::item::persistence {
namespace {
using U=SqlParam;using E=ItemPersistenceError;
template<class R>std::string Bytes(const R& r){return {reinterpret_cast<const char*>(r.data()),r.size()};}
template<class T>bool Number(const char* s,T& n){if(!s)return false;const std::string_view text(s);const auto p=std::from_chars(text.data(),text.data()+text.size(),n);return p.ec==std::errc{} && p.ptr==text.data()+text.size();}
SqlParams Identity(const DungeonEntryMutation& m,const DungeonEntryTicket& t){return {U::Blob(Bytes(m.reservation)),U::UInt(t.owner),U::UInt(t.session),U::UInt(t.instance),U::UInt(static_cast<unsigned>(t.key.Category())),U::UInt(t.key.LocalId()),U::UInt(t.level),U::UInt(t.revision),U::UInt(t.count),U::UInt(t.units),U::Blob(Bytes(t.stateHash)),U::Text(m.gate),U::UInt(t.createdAtMs)};}
}
void DungeonEntryWriteBefore(SqlTransaction& tx,const ItemCommit& r){
 if(r.dungeonEntry || !r.items.empty())tx.push_back({"SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME='item_dungeon_entry' AND ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE'",{},{},1});
 if(r.dungeonEntry){const auto& m=*r.dungeonEntry;for(const auto& t:m.tickets){
  if(m.kind==DungeonEntryMutationKind::Reserve){
   tx.push_back({"SELECT 1 FROM player.item_instance WHERE instance_id=? AND owner_kind=1 AND owner_id=? AND category_id=? AND local_id=? AND refinement_level=? AND state_revision=? AND state_sha256=? AND stack_count=? AND stack_count>=? AND destroyed=0 AND storage_kind=1 AND private_shop_operation IS NULL FOR UPDATE",{U::UInt(t.instance),U::UInt(t.owner),U::UInt(static_cast<unsigned>(t.key.Category())),U::UInt(t.key.LocalId()),U::UInt(t.level),U::UInt(t.revision),U::Blob(Bytes(t.stateHash)),U::UInt(t.count),U::UInt(t.units)},{},1});
   tx.push_back({"INSERT INTO player.item_dungeon_entry(reservation_operation,owner_id,owner_session_generation,instance_id,category_id,local_id,refinement_level,item_revision,item_count,units,item_state_hash,gate_code,reserved_at_ms) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",Identity(m,t),1,{}});
  }else {
   auto args=Identity(m,t);
   tx.push_back({"SELECT 1 FROM player.item_dungeon_entry WHERE reservation_operation=? AND owner_id=? AND owner_session_generation=? AND instance_id=? AND category_id=? AND local_id=? AND refinement_level=? AND item_revision=? AND item_count=? AND units=? AND item_state_hash=? AND BINARY gate_code=? AND reserved_at_ms=? AND resolved_operation IS NULL FOR UPDATE",std::move(args),{},1});
   tx.push_back({"UPDATE player.item_dungeon_entry SET resolved_operation=?,resolution=?,resolved_at=NOW(6) WHERE reservation_operation=? AND owner_id=? AND resolved_operation IS NULL",{U::Blob(Bytes(r.operationId)),U::UInt(m.kind==DungeonEntryMutationKind::Consume?1:2),U::Blob(Bytes(m.reservation)),U::UInt(t.owner)},1,{}});
  }
 }}
 for(const auto& ch:r.items)if(ch.before)tx.push_back({"SELECT 1 WHERE NOT EXISTS(SELECT 1 FROM player.item_dungeon_entry WHERE active_instance=? FOR UPDATE)",{U::UInt(ch.before->id.Value())},{},1});
}
std::expected<std::vector<DungeonEntryMutation>,E> LoadPendingDungeonEntries(CAsyncSQL& sql,const ItemCatalog& c,std::uint64_t owner){
 if(!owner || owner>UINT32_MAX)return std::unexpected(E::InvalidOwner);
 std::unique_ptr<SQLMsg> rows(sql.DirectQueryPrepared("SELECT reservation_operation,gate_code,category_id,local_id,refinement_level,owner_id,owner_session_generation,instance_id,item_revision,reserved_at_ms,item_count,units,item_state_hash FROM player.item_dungeon_entry WHERE active_owner=? ORDER BY reservation_operation",{U::UInt(owner)}));
 if(!rows || rows->uiSQLErrno || !rows->Get())return std::unexpected(E::SqlFailure);if(rows->Get()->uiNumRows>1)return std::unexpected(E::InvalidStoredData);std::vector<DungeonEntryMutation> out;
 for(auto row=rows->Get()->FetchRow();row;row=rows->Get()->FetchRow()){const auto lengths=rows->Get()->FetchLengths();std::uint16_t category=0,level=0;std::uint32_t local=0;if(!lengths || !row[0] || !row[1] || !row[12] || lengths[0]!=16 || lengths[12]!=32 || !Number(row[2],category)||!Number(row[3],local)||!Number(row[4],level))return std::unexpected(E::InvalidStoredData);const auto key=ItemKey::Create(category,local);if(!key)return std::unexpected(E::InvalidStoredData);DungeonEntryTicket t{*key};t.level=level;if(!Number(row[5],t.owner)||!Number(row[6],t.session)||!Number(row[7],t.instance)||!Number(row[8],t.revision)||!Number(row[9],t.createdAtMs)||!Number(row[10],t.count)||!Number(row[11],t.units))return std::unexpected(E::InvalidStoredData);std::copy_n(reinterpret_cast<const std::uint8_t*>(row[12]),32,t.stateHash.begin());DungeonEntryMutation m;m.kind=DungeonEntryMutationKind::Reserve;std::copy_n(reinterpret_cast<const std::uint8_t*>(row[0]),16,m.reservation.begin());m.gate.assign(row[1],lengths[1]);m.tickets.push_back(t);
  ItemCommit check;check.operationId=m.reservation;check.catalogRevision=c.Revision();check.catalogHash=c.ContentHash();check.kind=ItemOperationKind::Service;check.owners={{{ItemOwnerKind::Character,owner},t.session,1}};check.outcomeCode=DungeonEntryReserveOutcome;check.result={'D','E','N',1,1};check.dungeonEntry=m;if(!ValidateItemCommit(c,check))return std::unexpected(E::InvalidStoredData);out.push_back(std::move(m));
 }return out;
}
std::expected<unsigned,E> RecoverDungeonEntries(CAsyncSQL& sql,const ItemCatalog& c,ItemOwner owner,std::uint64_t session){
 if(owner.kind!=ItemOwnerKind::Character)return std::unexpected(E::InvalidOwner);const auto pending=LoadPendingDungeonEntries(sql,c,owner.id);if(!pending)return std::unexpected(pending.error());unsigned released=0;
 for(const auto& reservation:*pending){const auto state=LoadItemOwner(sql,c,owner,session);if(!state)return std::unexpected(state.error());std::array<std::uint8_t,16> operation{};if(RAND_bytes(operation.data(),operation.size())!=1)return std::unexpected(E::InvalidOperation);const std::array owners{state->fence};auto commit=MakeDungeonEntryResolution(c,operation,reservation,false,owners,{},{});if(!commit)return std::unexpected(commit.error());auto write=CommitItems(sql,c,*commit);if(!write && write.error()==E::CommitUnknown){const auto receipt=FindItemReceipt(sql,c,*commit);if(!receipt)return std::unexpected(receipt.error());if(*receipt)write=ItemWriteResult{**receipt,true};else write=CommitItems(sql,c,*commit);}if(!write)return std::unexpected(write.error());++released;
 }return released;
}
}
