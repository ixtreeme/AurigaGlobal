// Explicit OFFLINE importer. Not linked to the game, and no runtime legacy map.
#include "CatalogSqlReader.hpp"
#include "ItemPersistenceSql.hpp"
#include <SQL/AsyncSQL.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

using namespace auriga::item;
namespace p=auriga::item::persistence;
namespace s=auriga::item::sql;
using Json=nlohmann::json;
using U=SqlParam;
using Message=std::unique_ptr<SQLMsg>;
namespace {
void Require(bool test,const char* why){if(!test)throw std::runtime_error(why);}
template<typename T> T Number(const Json& j){
    Require(j.is_number_integer() || j.is_string(),"Integer required");
    const auto text=j.is_string()?j.get<std::string>():j.dump();T result{};
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),result);
    Require(error==std::errc{} && end==text.data()+text.size() && (text=="0" || text=="-1" || text.front()!='0'),"Integer range/canonical form");return result;
}
std::string Bytes(const auto& value){return {reinterpret_cast<const char*>(value.data()),value.size()};}
std::array<std::uint8_t,32> Hash(std::string_view value){
    std::array<std::uint8_t,32> out{};unsigned n=0;
    Require(EVP_Digest(value.data(),value.size(),out.data(),&n,EVP_sha256(),nullptr)==1 && n==32,"Digest failure");return out;
}
std::string Hex(const auto& data){static const char digits[]="0123456789abcdef";std::string s;for(auto b:data){s+=digits[b>>4];s+=digits[b&15];}return s;}
template<std::size_t N> std::array<std::uint8_t,N> Unhex(const std::string& text){
    Require(text.size()==2*N,"Digest length");std::array<std::uint8_t,N> result{};
    for(unsigned i=0;i<N;++i){unsigned v=0;auto [end,error]=std::from_chars(text.data()+2*i,text.data()+2*i+2,v,16);Require(error==std::errc{} && end==text.data()+2*i+2,"Digest encoding");result[i]=v;}return result;
}
ItemKey Key(const Json& row){auto k=ItemKey::Create(Number<std::uint16_t>(row.at("category_id")),Number<std::uint32_t>(row.at("local_id")));Require(k.has_value(),"Item key");return *k;}
PersistentItem Item(const Json& row,ItemOwner owner){
    const auto identity=ItemInstanceId::Create(Number<std::uint64_t>(row.at("instance_id")));Require(identity.has_value(),"Zero instance identity");
    PersistentItem i{*identity,Key(row),owner};
    i.refinementLevel=Number<std::uint16_t>(row.at("refinement_level"));i.count=Number<std::uint32_t>(row.at("count"));
    const auto& pos=row.at("placement");i.placement=ItemPlacement{static_cast<ItemStorageKind>(Number<std::uint8_t>(pos.at("kind"))),Number<std::uint64_t>(pos.at("storage_id")),Number<std::uint32_t>(pos.at("anchor"))};
    Require(row.at("attributes").is_array() && row.at("attributes").size()==8,"Eight attribute slots required");
    for(unsigned n=0;n<8;++n)if(!row["attributes"][n].is_null()){const auto& a=row["attributes"][n];i.attributes.slots[n]=BonusEntry{static_cast<BonusId>(Number<std::uint32_t>(a.at("bonus_id"))),Number<std::int64_t>(a.at("value_scaled"))};}
    if(!row.at("locked_slot").is_null())i.attributes.lockedSlot=Number<std::uint8_t>(row.at("locked_slot"));
    Require(row.at("stones").is_array() && row.at("stones").size()==3,"Three stone slots required");
    for(unsigned n=0;n<3;++n){const auto& a=row["stones"][n];auto& slot=i.stones[n];slot.state=static_cast<StoneSocketState>(Number<std::uint8_t>(a.at("state")));slot.grade=Number<std::uint8_t>(a.at("grade"));slot.refinementLevel=Number<std::uint16_t>(a.at("level"));if(!a.at("stone").is_null())slot.stone=Key(a["stone"]);}
    if(!row.at("accessory").is_null()){const auto& a=row["accessory"];Require(a.at("permanent").is_boolean(),"Accessory boolean");i.accessory=AccessorySocketState{Number<std::uint8_t>(a.at("capacity")),Number<std::uint8_t>(a.at("grade")),Number<std::uint64_t>(a.at("remaining_ms")),a["permanent"].get<bool>()};}
    if(row.contains("automatic") && !row.at("automatic").is_null()){
        const auto& a=row.at("automatic");Require(a.at("active").is_boolean(),"Automatic boolean");
        i.automatic=AutomaticRecoveryState{Number<std::int64_t>(a.at("remaining_raw")),a["active"].get<bool>(),Number<std::uint64_t>(a.at("next_tick_ms"))};
    }
    Require(i.key.Category()==ItemCategory::Weapon || i.key.Category()==ItemCategory::Armor || i.key.Category()==ItemCategory::Accessory || i.key.Category()==ItemCategory::SocketStone || i.key.Category()==ItemCategory::Consumable || i.key.Category()==ItemCategory::Material || i.key.Category()==ItemCategory::Container || i.key.Category()==ItemCategory::SkillBook || i.key.Category()==ItemCategory::Quest || i.key.Category()==ItemCategory::CurrencyToken || i.key.Category()==ItemCategory::ForgeAid,"Unreviewed import category");
    return i;
}
std::vector<std::string> Columns(){std::vector<std::string> names{"id","owner_id","window","pos","count","vnum","lockedattr","socket0","socket1","socket2"};for(unsigned i=0;i<8;++i){names.push_back("attrtype"+std::to_string(i));names.push_back("attrvalue"+std::to_string(i));}return names;}
void CompactInserts(SqlTransaction& tx){
    // Child rows contain only integers/NULL. One bound JSON array per write
    // avoids exhausting the normal 4096-parameter transaction limit on a full
    // owner. JSON_TABLE has fixed SQL types/paths; no source value becomes SQL.
    for(auto& step:tx){
        unsigned columns=0;bool attributes=false;
        if(step.query.starts_with("INSERT INTO player.item_instance_socket("))columns=7;
        else if(step.query.starts_with("INSERT INTO player.item_occupied_cell("))columns=6;
        else if(step.query.starts_with("INSERT INTO player.item_instance_attribute(")){columns=4;attributes=true;}
        if(!columns)continue;
        const auto at=step.query.find(" VALUES");Require(at!=std::string::npos && step.params.size()%columns==0,"Bulk child shape");
        Json rows=Json::array();for(std::size_t n=0;n<step.params.size();n+=columns){Json row=Json::array();for(unsigned k=0;k<columns;++k){const auto& p=step.params[n+k];if(p.type==SqlParamType::Null)row.push_back(nullptr);else if(p.type==SqlParamType::Unsigned)row.push_back(p.u64);else{Require(p.type==SqlParamType::Signed,"Non-integer child binding");row.push_back(p.i64);}}rows.push_back(std::move(row));}
        std::string select,definition;for(unsigned k=0;k<columns;++k){if(k){select+=",";definition+=",";}const auto name="c"+std::to_string(k);select+="j."+name;definition+=name+" BIGINT"+(attributes && k==3?std::string{}:" UNSIGNED")+" PATH '$["+std::to_string(k)+"]' ERROR ON ERROR";}
        step.query=step.query.substr(0,at)+" SELECT "+select+" FROM JSON_TABLE(?, '$[*]' COLUMNS("+definition+")) AS j";step.params={U::Text(rows.dump())};
    }
    // Structural NULLs do not need bound parameters. Source data is NEVER
    // interpolated; every non-null scalar/blob keeps its prepared binding.
    for(auto& step:tx)if(step.query.starts_with("INSERT INTO ") && step.query.find_first_of("'\"")==std::string::npos){
        std::string query;SqlParams params;std::size_t index=0;
        for(char ch:step.query)if(ch=='?'){Require(index<step.params.size(),"Prepared insert shape");auto& p=step.params[index++];if(p.type==SqlParamType::Null)query+="NULL";else{query+='?';params.push_back(std::move(p));}}else query+=ch;
        Require(index==step.params.size(),"Prepared parameter coverage");step.query=std::move(query);step.params=std::move(params);
    }
    SqlTransaction merged;
    for(auto& step:tx){const auto at=step.query.find(" VALUES(");
        if(!merged.empty() && at!=std::string::npos && step.query.starts_with("INSERT INTO ") && step.expectedAffectedRows && !step.expectedResultRows){
            auto& prev=merged.back();const auto prefix=step.query.substr(0,at+7);
            if(prev.query.starts_with(prefix) && prev.expectedAffectedRows && !prev.expectedResultRows){prev.query+=","+step.query.substr(at+7);*prev.expectedAffectedRows+=*step.expectedAffectedRows;for(auto& p:step.params)prev.params.push_back(std::move(p));continue;}
        }merged.push_back(std::move(step));
    }
    tx=std::move(merged);std::size_t parameters=0,bytes=0;for(const auto& step:tx){parameters+=step.params.size();bytes+=step.query.size();for(const auto& p:step.params)bytes+=p.bytes.size();Require(step.query.size()<=SqlTransactionLimits::QueryBytes,"Import query budget");}
    if(tx.size()>SqlTransactionLimits::Steps || parameters>SqlTransactionLimits::Parameters || bytes>SqlTransactionLimits::TotalBytes)throw std::runtime_error("Owner exceeds atomic import budget: steps="+std::to_string(tx.size())+", params="+std::to_string(parameters));
}
Json Run(CAsyncSQL& db,const ItemCatalog& c,Json request){
    Require(request.at("offline_owned_package")==true,"Owned offline scope required");
    if(request.at("action")=="follow_catalog"){
        auto query=[&](const char* sql,SqlParams parameters=SqlParams{}){
            Message result(db.DirectQueryPrepared(sql,std::move(parameters)));Require(result && !result->uiSQLErrno,"Offline catalog pin query failed");return result;
        };
        // Native owner loads open their own read transactions. This stopped
        // publisher follows the existing catalog editor's reversible pin switch;
        // never nest those reads inside another SQL transaction.
        const auto previous=query("SELECT catalog_revision,catalog_sha256 FROM player.item_persistence_format WHERE singleton=1");
        Require(previous->Get() && previous->Get()->uiNumRows==1,"Persistence pin missing");
        const auto oldRow=previous->Get()->FetchRow();const auto oldLengths=previous->Get()->FetchLengths();
        const auto oldRevision=std::stoull(oldRow[0]);const std::string oldHash(oldRow[1],oldLengths[1]);
        try{
            query("UPDATE player.item_persistence_format SET catalog_revision=?,catalog_sha256=? WHERE singleton=1",{U::UInt(c.Revision()),U::Blob(Bytes(c.ContentHash()))});
            auto owners=query("SELECT owner_kind,owner_id FROM player.item_owner_fence ORDER BY owner_kind,owner_id LIMIT 100001");
            Require(owners->Get() && owners->Get()->uiNumRows<=100000,"Owner validation budget");
            std::size_t count=0;
            while(const auto row=owners->Get()->FetchRow()){
                const ItemOwner owner{static_cast<ItemOwnerKind>(std::stoul(row[0])),std::stoull(row[1])};
                const auto loaded=p::LoadCurrentItemOwner(db,c,owner);
                if(!loaded)throw std::runtime_error("Existing native owner "+std::to_string(static_cast<unsigned>(owner.kind))+":"+std::to_string(owner.id)+" validation error "+std::to_string(static_cast<unsigned>(loaded.error())));++count;
            }
            return {{"validated_owners",count},{"catalog_hash",Hex(c.ContentHash())}};
        }catch(...){query("UPDATE player.item_persistence_format SET catalog_revision=?,catalog_sha256=? WHERE singleton=1",{U::UInt(oldRevision),U::Blob(oldHash)});throw;}
    }
    const auto actor=request.at("actor");ItemOwner owner{ItemOwnerKind::Character,Number<std::uint64_t>(actor.at("pid"))};Require(ValidItemOwner(owner),"Owner identity");
    Require(request.at("items").is_array() && !request["items"].empty() && request["items"].size()<=MaxLoadedOwnerItems,"Item budget");
    if(request.at("action")=="allocate"){
        const auto block=p::ReserveItemInstanceIds(db,c,static_cast<std::uint32_t>(request["items"].size()));Require(block.has_value(),"ID reservation failed");
        auto id=block->first;for(auto& row:request["items"])row["instance_id"]=std::to_string(id++);
        request.erase("connection");request["action"]="import";request["catalog_hash"]=Hex(c.ContentHash());return {{"plan",request}};
    }
    Require(request.at("action")=="import","Unknown action");
    const auto importId=Unhex<16>(request.at("import_id").get<std::string>());
    const auto checkpoint=Unhex<32>(request.at("source_checkpoint_sha256").get<std::string>());
    auto semantic=request;semantic.erase("connection");semantic.erase("inject_failure");
    const auto digest=Hash(semantic.dump());
    // Replay is checked before source reads: source rows have already been
    // archived/deleted, and the player may have relogged or changed equipment.
    Message previous(db.DirectQueryPrepared("SELECT request_sha256,item_count,total_units FROM player.item_legacy_import WHERE import_id=?",{U::Blob(Bytes(importId))}));
    Require(previous && !previous->uiSQLErrno,"Import receipt read failed");
    if(previous->Get() && previous->Get()->uiNumRows){const auto row=previous->Get()->FetchRow();const auto lengths=previous->Get()->FetchLengths();Require(row && row[0] && lengths && std::string(row[0],lengths[0])==Bytes(digest),"Import replay differs");return {{"replay",true},{"items",std::stoull(row[1])},{"units",std::stoull(row[2])}};}
    Require(request.at("catalog_hash")==Hex(c.ContentHash()),"Catalog changed after planning");
    const auto columns=Columns();std::set<std::uint32_t> sourceIds;std::set<ItemInstanceId> ids;std::vector<PersistentItem> items;std::uint64_t units=0;
    for(const auto& row:request["items"]){auto item=Item(row,owner);Require(ids.insert(item.id).second && item.id.Value()>=4294967296ULL,"Reserved native identity required");
        const auto valid=ValidatePersistentItem(c,item);
        if(!valid)throw std::runtime_error("Invalid typed instance: source "+row.at("source").at("id").dump()+", error "+std::to_string(static_cast<unsigned>(valid.error())));
        const auto& old=row.at("source");Require(old.is_object() && old.size()==columns.size(),"Complete source row required");for(const auto& name:columns)Require(old.contains(name),"Missing source field");
        Require(Number<std::uint64_t>(old["owner_id"])==owner.id && (old["window"]=="INVENTORY" || old["window"]=="EQUIPMENT" || old["window"]=="EXTRA_INVENTORY") && Number<std::uint32_t>(old["count"])==item.count && sourceIds.insert(Number<std::uint32_t>(old["id"])).second,"Source identity/count/window mismatch");units+=item.count;items.push_back(std::move(item));}
    std::vector<ItemStorageLayout> layouts;std::set<ItemStorage> seen;
    for(const auto& row:request.at("storages")){ItemStorageLayout l{{owner,static_cast<ItemStorageKind>(Number<std::uint8_t>(row.at("kind"))),Number<std::uint64_t>(row.at("id"))},Number<std::uint16_t>(row.at("columns")),Number<std::uint16_t>(row.at("rows")),Number<std::uint16_t>(row.at("pages"))};Require(ValidItemStorage(l) && seen.insert(l.storage).second,"Invalid storage");layouts.push_back(l);}
    Require(!layouts.empty() && layouts.size()<=MaxItemCommitStorages,"Storage budget");
    for(const auto& item:items){const auto found=std::ranges::find_if(layouts,[&](const auto& l){return item.placement->kind==l.storage.kind && item.placement->storageId==l.storage.id;});Require(found!=layouts.end() && OccupiedItemCells(c,item,*found).has_value(),"Invalid placement");}
    const auto gold=Number<std::uint64_t>(actor.at("gold"));Require(gold<=INT64_MAX,"Wallet bound");
    SqlTransaction tx;
    tx.push_back({"SELECT 1 FROM player.player WHERE id=? AND account_id=? AND name=? AND gold=? FOR UPDATE",{U::UInt(owner.id),U::UInt(Number<std::uint64_t>(actor.at("account_id"))),U::Text(actor.at("name").get<std::string>()),U::UInt(gold)},{},1});
    tx.push_back({"SELECT COUNT(*) FROM player.item WHERE owner_id=? AND `window` IN ('INVENTORY','EQUIPMENT','EXTRA_INVENTORY') HAVING COUNT(*)=?",{U::UInt(owner.id),U::UInt(items.size())},{},1});
    for(const auto& row:request["items"]){const auto& old=row["source"];std::string expression="CONCAT_WS('|',",canonical;
        for(std::size_t n=0;n<columns.size();++n){if(n){expression+=",";canonical+='|';}if(columns[n]=="window"){expression+="LOWER(HEX(`window`))";const auto value=old["window"].get<std::string>();canonical+=Hex(std::vector<std::uint8_t>(value.begin(),value.end()));}else{expression+="CAST(`"+columns[n]+"` AS CHAR)";canonical+=std::to_string(Number<std::int64_t>(old[columns[n]]));}}
        expression+=")";tx.push_back({"SELECT 1 FROM player.item WHERE id=? AND owner_id=? AND SHA2("+expression+",256)=? FOR UPDATE",{U::UInt(Number<std::uint32_t>(old["id"])),U::UInt(owner.id),U::Text(Hex(Hash(canonical)))},{},1});}
    tx.push_back({"INSERT INTO player.item_owner_fence VALUES(?,?,1,1)",{U::UInt(1),U::UInt(owner.id)},1,{}});
    for(const auto& l:layouts)tx.push_back({"INSERT INTO player.item_storage VALUES(?,?,?,?,?,?,?)",{U::UInt(1),U::UInt(owner.id),U::UInt(static_cast<unsigned>(l.storage.kind)),U::UInt(l.storage.id),U::UInt(l.columns),U::UInt(l.rows),U::UInt(l.pages)},1,{}});
    tx.push_back({"INSERT INTO player.item_wallet VALUES(?,?,1,1,?)",{U::UInt(1),U::UInt(owner.id),U::UInt(gold)},1,{}});
    std::uint64_t revision=1;
    for(std::size_t first=0;first<items.size();first+=MaxItemCommitItems){ItemCommit commit;const auto op=Hash(Hex(importId)+":"+std::to_string(first));std::copy_n(op.begin(),16,commit.operationId.begin());commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Grant;commit.owners={{owner,1,revision++}};commit.storages=layouts;commit.outcomeCode=1;commit.result={static_cast<std::uint8_t>(ItemOperationKind::Grant)};
        for(auto n=first;n<std::min(items.size(),first+MaxItemCommitItems);++n)commit.items.push_back({{},items[n]});
        std::erase_if(commit.storages,[&](const auto& l){return std::ranges::none_of(commit.items,[&](const auto& ch){return ch.after.placement && ch.after.placement->kind==l.storage.kind && ch.after.placement->storageId==l.storage.id;});});
        auto part=p::PrepareItemWrite(c,commit);if(!part)throw std::runtime_error("Native import batch "+std::to_string(first)+" rejected: "+std::to_string(static_cast<unsigned>(part.error())));for(auto& step:*part)tx.push_back(std::move(step));}
    tx.push_back({"INSERT INTO player.item_legacy_import VALUES(?,1,?,?,?,?,?,?)",{U::Blob(Bytes(importId)),U::UInt(owner.id),U::Blob(Bytes(checkpoint)),U::Blob(Bytes(digest)),U::Blob(Bytes(c.ContentHash())),U::UInt(items.size()),U::UInt(units)},1,{}});
    if(request.contains("source_state")){
        const auto& state=request.at("source_state");
        // Only automatic-item affects with the exact captured item link move.
        // Other skill/event/premium affects remain owned by their old subsystem.
        for(const auto& affect:state.at("automatic_affects")){
            Require(affect.is_array() && affect.size()==7,"Affect row shape");
            const auto type=Number<unsigned>(affect[1]),flag=Number<std::uint32_t>(affect[4]);
            Require(Number<std::uint64_t>(affect[0])==owner.id && (type==534 || type==535) && sourceIds.contains(flag),"Unreviewed legacy affect");
            const auto index=std::ranges::find_if(request["items"],[&](const auto& r){return Number<std::uint32_t>(r["source"]["id"])==flag;});
            Require(index!=request["items"].end() && index->contains("automatic") && !(*index)["automatic"].is_null() && (*index)["automatic"]["active"]==true,"Affect has no active automatic state");
            SqlParams values;for(const auto& value:affect)values.push_back(U::Int(Number<std::int64_t>(value)));
            tx.push_back({"DELETE FROM player.affect WHERE dwPID=? AND bType=? AND bApplyOn=? AND lApplyValue=? AND dwFlag=? AND lDuration=? AND lSPCost=?",std::move(values),1,{}});
        }
        if(state.contains("account_coins")){
            const auto account=Number<std::uint64_t>(actor.at("account_id")),coins=Number<std::uint64_t>(state["account_coins"]);Require(coins<=UINT32_MAX,"Legacy coin range");
            tx.push_back({"SELECT 1 FROM account.account WHERE id=? AND coins=? FOR UPDATE",{U::UInt(account),U::UInt(coins)},{},1});
            tx.push_back({"INSERT INTO player.item_owner_fence VALUES(2,?,1,1)",{U::UInt(account)},1,{}});
            tx.push_back({"INSERT INTO player.item_wallet VALUES(2,?,2,1,?)",{U::UInt(account),U::UInt(coins)},1,{}});
            tx.push_back({"UPDATE account.account SET coins=0 WHERE id=? AND coins=?",{U::UInt(account),U::UInt(coins)},{},{}});
        }
        tx.push_back({"INSERT INTO player.item_legacy_owner_state VALUES(?,?)",{U::Blob(Bytes(importId)),U::Blob(state.dump())},1,{}});
    }
    for(std::size_t n=0;n<items.size();++n){const auto& row=request["items"][n]["source"];const auto sourceId=Number<std::uint32_t>(row["id"]);
        tx.push_back({"INSERT INTO player.item_legacy_archive VALUES(?,?,?,?)",{U::Blob(Bytes(importId)),U::UInt(sourceId),U::UInt(items[n].id.Value()),U::Blob(row.dump())},1,{}});}
    std::string marks;SqlParams sourceDelete{U::UInt(owner.id)};for(auto id:sourceIds){if(!marks.empty())marks+=",";marks+="?";sourceDelete.push_back(U::UInt(id));}
    tx.push_back({"DELETE FROM player.item WHERE owner_id=? AND id IN ("+marks+")",std::move(sourceDelete),items.size(),{}});
    if(request.value("inject_failure",false))tx.push_back({"SELECT 1 WHERE 0",{},{},1});
    CompactInserts(tx);
    Message result(db.DirectTransactionPrepared(std::move(tx)));Require(result && result->bTransaction && !result->uiSQLErrno && result->transactionOutcome==SqlTransactionOutcome::Committed,"Atomic import not committed; reconcile/retry the same import ID");
    const auto loaded=p::LoadCurrentItemOwner(db,c,owner);Require(loaded && loaded->items.size()==items.size(),"Committed import could not be reloaded");
    for(const auto& item:items){const auto actual=std::ranges::find(loaded->items,item.id,&PersistentItem::id);Require(actual!=loaded->items.end() && *actual==item,"Reloaded instance differs");}
    return {{"replay",false},{"items",items.size()},{"units",units},{"revision",loaded->fence.revision}};
}
}
int main(){
    try{Json request;std::cin>>request;Json result;{
        const auto& auth=request.at("connection");CAsyncSQL db;
        Require(db.Setup("127.0.0.1",auth.at("user").get<std::string>().c_str(),auth.at("password").get<std::string>().c_str(),"player","utf8mb4",true,Number<unsigned>(auth.at("port"))),"Owned local SQL connection failed");
        const auto catalog=s::ReadCatalog(db,{1,{}});Require(catalog.has_value(),"Native catalogue unavailable");
        result=Run(db,**catalog,std::move(request));}
        // Join SQL worker/logging before emitting a potentially large plan.
        std::cout<<("AURIGA_IMPORT_RESULT "+Json{{"ok",true},{"result",result}}.dump()+"\n");return 0;
    }catch(const std::exception& e){std::cout<<"AURIGA_IMPORT_RESULT "<<Json{{"ok",false},{"error",e.what()}}.dump()<<'\n';return 1;}
}
