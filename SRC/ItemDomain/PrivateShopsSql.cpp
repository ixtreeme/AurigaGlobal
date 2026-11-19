#include "PrivateShopsSql.hpp"
#include <SQL/AsyncSQL.h>
#include <openssl/rand.h>
#include <algorithm>
#include <charconv>
#include <memory>
#include <set>
namespace auriga::item::persistence {
namespace {
using E=ItemPersistenceError;
template<class T> bool Number(const char* text,T& value){if(!text)return false;const std::string_view s(text);const auto parsed=std::from_chars(s.data(),s.data()+s.size(),value);return parsed.ec==std::errc{} && parsed.ptr==s.data()+s.size();}
std::string Bytes(const std::array<std::uint8_t,16>& bytes){return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
}
void PrivateShopWriteBefore(SqlTransaction& tx,const ItemCommit& r){
 using U=SqlParam;const bool touched=r.privateShop || std::ranges::any_of(r.items,[](const auto& ch){return ch.after.privateShop || ch.before && ch.before->privateShop;});if(!touched)return;
 tx.push_back({"SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME IN ('item_private_shop','item_private_shop_price') HAVING COUNT(*)=2 AND SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE')=2",{},{},1});
 if(!r.privateShop || r.privateShop->kind==PrivateShopMutationKind::ExpirePermit)return;const auto& m=*r.privateShop;const auto& h=m.header;
 if(m.kind==PrivateShopMutationKind::Open){
  tx.push_back({"INSERT INTO player.item_private_shop(owner_kind,owner_id,session_generation,operation_id,sign,remember_prices,created_at_ms) VALUES(1,?,?,?,?,?,?)",{U::UInt(h.owner),U::UInt(h.session),U::Blob(Bytes(h.operation)),U::Text(h.sign),U::UInt(h.rememberPrices),U::UInt(h.createdAtMs)},1,{}});
  if(h.rememberPrices)tx.push_back({"SELECT 1 FROM player.item_instance WHERE instance_id=? AND owner_kind=1 AND owner_id=? AND destroyed=0 AND private_shop_operation IS NULL AND shop_permit_deadline_ms>GREATEST(?,CAST(UNIX_TIMESTAMP(NOW(3))*1000 AS UNSIGNED)) FOR UPDATE",{U::UInt(m.subject),U::UInt(h.owner),U::UInt(m.nowAtMs)},{},1});
 }else tx.push_back({"SELECT 1 FROM player.item_private_shop WHERE owner_id=? AND session_generation=? AND operation_id=? AND BINARY sign=? AND remember_prices=? AND created_at_ms=? FOR UPDATE",{U::UInt(h.owner),U::UInt(h.session),U::Blob(Bytes(h.operation)),U::Blob(h.sign),U::UInt(h.rememberPrices),U::UInt(h.createdAtMs)},{},1});
 if(m.kind==PrivateShopMutationKind::Open || m.kind==PrivateShopMutationKind::Buy){
  tx.push_back({"SELECT 1 FROM player.item_wallet WHERE owner_kind=1 AND owner_id=? AND currency=1 AND state_revision=? AND balance=? FOR UPDATE",{U::UInt(h.owner),U::UInt(m.goldRevision),U::UInt(m.goldBalance)},{},1});
  for(const auto& ch:r.items)if(ch.before && ((m.kind==PrivateShopMutationKind::Open && ch.after.privateShop) || (m.kind==PrivateShopMutationKind::Buy && ch.after.id.Value()==m.subject)))if(const auto deadline=PrivateShopItemDeadline(*ch.before))tx.push_back({"SELECT 1 WHERE ?>GREATEST(?,CAST(UNIX_TIMESTAMP(NOW(3))*1000 AS UNSIGNED))",{U::UInt(deadline),U::UInt(m.nowAtMs)},{},1});
 }
}
void PrivateShopWriteAfter(SqlTransaction& tx,const ItemCommit& r){
 using U=SqlParam;
 if(r.privateShop && r.privateShop->kind==PrivateShopMutationKind::Open && r.privateShop->header.rememberPrices){
  std::string values;SqlParams args;for(const auto& ch:r.items)if(ch.after.privateShop && ch.after.privateShop->shop==r.privateShop->header.operation){if(!values.empty())values+=',';values+="(1,?,?,?,?,?,NOW(6))";for(auto p:{U::UInt(ch.after.owner.id),U::UInt(static_cast<unsigned>(ch.after.key.Category())),U::UInt(ch.after.key.LocalId()),U::UInt(ch.after.refinementLevel),U::UInt(ch.after.privateShop->price/ch.after.count)})args.push_back(std::move(p));}
  if(!values.empty())tx.push_back({"INSERT INTO player.item_private_shop_price(owner_kind,owner_id,category_id,local_id,refinement_level,unit_price,updated_at) VALUES"+values+" ON DUPLICATE KEY UPDATE unit_price=VALUES(unit_price),updated_at=VALUES(updated_at)",std::move(args),{},{}});
 }
 std::set<std::array<std::uint8_t,16>> cleanup;
 for(const auto& ch:r.items)if(ch.before && ch.before->privateShop && !ch.after.privateShop)cleanup.insert(ch.before->privateShop->shop);
 if(r.privateShop && r.privateShop->kind==PrivateShopMutationKind::Close){const auto& h=r.privateShop->header;cleanup.erase(h.operation);tx.push_back({"DELETE FROM player.item_private_shop WHERE owner_id=? AND operation_id=?",{U::UInt(h.owner),U::Blob(Bytes(h.operation))},1,{}});}
 for(const auto& op:cleanup)tx.push_back({"DELETE FROM player.item_private_shop WHERE operation_id=? AND NOT EXISTS(SELECT 1 FROM player.item_instance WHERE private_shop_operation=?)",{U::Blob(Bytes(op)),U::Blob(Bytes(op))},{},{}});
}
std::expected<std::optional<PrivateShopHeader>,E> LoadPrivateShopHeader(CAsyncSQL& sql,std::uint64_t owner){
 if(!owner || owner>UINT32_MAX)return std::unexpected(E::InvalidOwner);
 std::unique_ptr<SQLMsg> rows(sql.DirectQueryPrepared("SELECT owner_id,session_generation,operation_id,sign,remember_prices,created_at_ms FROM player.item_private_shop WHERE owner_id=?",{SqlParam::UInt(owner)}));
 if(!rows || rows->uiSQLErrno || !rows->Get())return std::unexpected(E::SqlFailure);if(!rows->Get()->uiNumRows)return std::optional<PrivateShopHeader>{};if(rows->Get()->uiNumRows!=1)return std::unexpected(E::InvalidStoredData);
 auto row=rows->Get()->FetchRow();auto lengths=rows->Get()->FetchLengths();PrivateShopHeader h;unsigned remember=0;
 if(!row || !lengths || lengths[2]!=16 || !row[3] || !Number(row[0],h.owner) || !Number(row[1],h.session) || !Number(row[4],remember) || remember>1 || !Number(row[5],h.createdAtMs))return std::unexpected(E::InvalidStoredData);
 std::copy_n(reinterpret_cast<const std::uint8_t*>(row[2]),16,h.operation.begin());h.sign.assign(row[3],lengths[3]);h.rememberPrices=remember!=0;if(!ValidPrivateShopHeader(h))return std::unexpected(E::InvalidStoredData);return std::optional{std::move(h)};
}
std::expected<std::vector<PrivateShopPrice>,E> LoadPrivateShopPrices(CAsyncSQL& sql,const ItemCatalog& c,std::uint64_t owner){
 if(!owner || owner>UINT32_MAX)return std::unexpected(E::InvalidOwner);std::unique_ptr<SQLMsg> rows(sql.DirectQueryPrepared("SELECT p.category_id,p.local_id,p.refinement_level,p.unit_price FROM player.item_private_shop_price p WHERE p.owner_id=? AND EXISTS(SELECT 1 FROM player.item_instance i WHERE i.owner_kind=1 AND i.owner_id=p.owner_id AND i.category_id=p.category_id AND i.local_id=p.local_id AND i.refinement_level=p.refinement_level AND i.destroyed=0 AND i.storage_kind=1) ORDER BY p.category_id,p.local_id,p.refinement_level LIMIT 4097",{SqlParam::UInt(owner)}));
 if(!rows || rows->uiSQLErrno || !rows->Get())return std::unexpected(E::SqlFailure);if(rows->Get()->uiNumRows>4096)return std::unexpected(E::LimitExceeded);std::vector<PrivateShopPrice> result;
 for(auto row=rows->Get()->FetchRow();row;row=rows->Get()->FetchRow()){std::uint16_t category=0,level=0;std::uint32_t local=0;std::uint64_t unit=0;if(!Number(row[0],category)||!Number(row[1],local)||!Number(row[2],level)||!Number(row[3],unit)||unit>INT64_MAX || level>255)return std::unexpected(E::InvalidStoredData);const auto key=ItemKey::Create(category,local);const auto* e=key?c.Find(*key):nullptr;if(!e || !c.FindRefinement(e->base.refinementProfile)->FindLevel(level))return std::unexpected(E::InvalidStoredData);result.push_back({*key,level,unit});}return result;
}
std::expected<bool,E> RecoverPrivateShop(CAsyncSQL& sql,const ItemCatalog& c,ItemOwner owner,std::uint64_t session){
 if(owner.kind!=ItemOwnerKind::Character || !ValidItemOwner(owner) || !session)return std::unexpected(E::InvalidOwner);
 auto state=LoadItemOwner(sql,c,owner,session);if(!state)return std::unexpected(state.error());const auto header=LoadPrivateShopHeader(sql,owner.id);if(!header)return std::unexpected(header.error());if(!*header)return false;
 std::array<std::uint8_t,16> operation{};if(RAND_bytes(operation.data(),operation.size())!=1)return std::unexpected(E::InvalidOperation);
 auto commit=MakePrivateShopClose(c,operation,**header,state->fence,state->storages,state->items);if(!commit)return std::unexpected(E::InvalidChange);auto written=CommitItems(sql,c,*commit);
 if(!written && written.error()==E::CommitUnknown){const auto receipt=FindItemReceipt(sql,c,*commit);if(!receipt)return std::unexpected(receipt.error());if(*receipt)written=ItemWriteResult{**receipt,true};else written=CommitItems(sql,c,*commit);}
 if(!written)return std::unexpected(written.error());return true;
}
}
