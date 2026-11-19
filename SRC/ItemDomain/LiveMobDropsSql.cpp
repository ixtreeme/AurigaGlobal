#include "LiveMobDropsSql.hpp"
#include <SQL/AsyncSQL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <charconv>
#include <map>
#include <limits>
#include <stdexcept>

namespace auriga::item::sql
{
namespace
{
using Message=std::unique_ptr<SQLMsg>;
using Json=nlohmann::json;
void Need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool Hex(std::string_view s,std::size_t size){return s.size()==size && std::ranges::all_of(s,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
Message Query(CAsyncSQL& db,const std::string& text,SqlParams args={})
{
    Message m(db.DirectQueryPrepared(text.c_str(),args));
    Need(m && !m->uiSQLErrno && m->Get(),"Drop SQL query failed");return m;
}
template<class T> T Number(const Cell& cell)
{
    Need(cell.has_value(),"NULL in drop release");std::uint64_t n=0;
    const auto parsed=std::from_chars(cell->data(),cell->data()+cell->size(),n);
    Need(parsed.ec==std::errc{} && parsed.ptr==cell->data()+cell->size() && n<=(std::numeric_limits<T>::max)(),"Invalid drop number");
    return static_cast<T>(n);
}
Rows ReadRows(SQLResult& result,std::size_t columns,std::size_t limit,bool nullableItems=false)
{
    Need(result.uiNumRows<=limit,"Too many live drop rows");Rows rows;std::size_t bytes=0;
    for(std::uint64_t n=0;n<result.uiNumRows;++n){auto raw=result.FetchRow();Need(raw!=nullptr,"Missing drop row");Row row;
        for(std::size_t c=0;c<columns;++c){if(!raw[c] && nullableItems && (c==4 || c==5)){row.emplace_back(std::nullopt);continue;}Need(raw[c]!=nullptr,"NULL in drop release");std::string value(raw[c]);Need(value.size()<=192,"Oversized drop cell");bytes+=CellBudgetOverhead+value.size();Need(bytes<=CapturedByteBudget,"Drop byte budget exceeded");row.emplace_back(std::move(value));}rows.push_back(std::move(row));}
    return rows;
}
constexpr std::size_t MaxLiveContainers=4096;
constexpr const char* ContainerGroupColumns="category_id,local_id,pick_mode";
constexpr const char* ContainerEntryColumns="category_id,local_id,entry_no,reward_kind,item_category_id,item_local_id,amount,weight,rare_attribute_chance_bp,refinement_level";
LiveMobDrops Build(std::shared_ptr<const ItemCatalog> base,Rows groups,Rows entries,std::string release,
    Rows containerGroups={},Rows containerEntries={},bool containersSupported=false)
{
    Need(base!=nullptr && Hex(release,32),"Invalid drop release identity");
    Need(groups.size()<=MaxMobDropGroups && entries.size()<=MaxMobDropGroups*MaxMobDropEntries,"Too many live drop rows");
    std::map<std::uint32_t,MobDropGroup> map;
    for(const auto& r:groups){Need(r.size()==7,"Invalid drop group columns");const auto id=Number<std::uint32_t>(r[0]);Need(r[1].has_value(),"Missing drop code");
        MobDropGroup g{id,*r[1],static_cast<MobDropTarget>(Number<std::uint8_t>(r[2])),Number<std::uint32_t>(r[3]),static_cast<MobDropRoll>(Number<std::uint8_t>(r[4])),Number<std::uint32_t>(r[5]),Number<std::uint16_t>(r[6]),{}};
        Need(map.emplace(id,std::move(g)).second,"Duplicate live drop group");
    }
    for(const auto& r:entries){Need(r.size()==11,"Invalid drop entry columns");const auto found=map.find(Number<std::uint32_t>(r[0]));Need(found!=map.end(),"Orphan live drop entry");
        const auto key=ItemKey::Create(Number<std::uint16_t>(r[2]),Number<std::uint32_t>(r[3]));Need(key.has_value(),"Invalid live drop item key");
        found->second.entries.push_back({Number<std::uint16_t>(r[1]),*key,Number<std::uint16_t>(r[4]),Number<std::uint32_t>(r[5]),Number<std::uint32_t>(r[6]),Number<std::uint32_t>(r[7]),Number<std::uint16_t>(r[8]),Number<std::uint16_t>(r[9]),Number<std::uint16_t>(r[10])});
    }
    std::vector<MobDropGroup> definitions;for(auto& [id,g]:map)definitions.push_back(std::move(g));
    Need(containerGroups.size()<=MaxLiveContainers && containerEntries.size()<=MaxLiveContainers*MaxLootEntries,"Too many live container rows");
    std::map<ItemKey,ContainerLootOverride> boxes;
    for(const auto& r:containerGroups){Need(r.size()==3,"Invalid live container group columns");
        const auto key=ItemKey::Create(Number<std::uint16_t>(r[0]),Number<std::uint32_t>(r[1]));Need(key && key->Category()==ItemCategory::Container,"Invalid live container key");
        Need(boxes.emplace(*key,ContainerLootOverride{*key,static_cast<LootPickMode>(Number<std::uint8_t>(r[2])),{}}).second,"Duplicate live container");}
    for(const auto& r:containerEntries){Need(r.size()==10,"Invalid live container entry columns");
        const auto key=ItemKey::Create(Number<std::uint16_t>(r[0]),Number<std::uint32_t>(r[1]));Need(key && boxes.contains(*key),"Orphan live container entry");
        std::optional<ItemKey> item;
        if(r[4] || r[5]){const auto parsed=ItemKey::Create(Number<std::uint16_t>(r[4]),Number<std::uint32_t>(r[5]));Need(parsed.has_value(),"Invalid live container reward key");item=*parsed;}
        boxes.at(*key).entries.push_back({Number<std::uint16_t>(r[2]),static_cast<LootRewardKind>(Number<std::uint8_t>(r[3])),item,
            Number<std::uint64_t>(r[6]),Number<std::uint32_t>(r[7]),Number<std::uint16_t>(r[8]),Number<std::uint16_t>(r[9])});}
    std::vector<ContainerLootOverride> overrides;for(auto& [key,box]:boxes)overrides.push_back(std::move(box));
    auto catalog=base->WithRewards(std::move(definitions),std::move(overrides));Need(catalog.has_value(),"Invalid live rewards: chest, item, refinement, amount, chance or group");
    return {true,std::move(release),CatalogHashHex(base->ContentHash()),CatalogHashHex((*catalog)->ContentHash()),*catalog,std::move(groups),std::move(entries),
        std::move(containerGroups),std::move(containerEntries),containersSupported};
}
const TableSpec& Spec(Table t){return Tables()[static_cast<std::size_t>(t)];}
std::vector<std::string> Columns(std::string_view value){std::vector<std::string> out;while(true){auto end=value.find(',');out.emplace_back(value.substr(0,end));if(end==value.npos)break;value.remove_prefix(end+1);}return out;}
void StoreRows(SqlTransaction& plan,const std::string& name,std::string_view columns,const Rows& rows,const std::string& release)
{
    const auto cols=Columns(columns);
    for(std::size_t at=0;at<rows.size();at+=500){Json payload=Json::array();
        for(auto i=at;i<(std::min)(rows.size(),at+500);++i){Json row=Json::array();for(const auto& c:rows[i])row.push_back(c?Json(*c):Json(nullptr));payload.push_back(std::move(row));}
        std::string query="INSERT INTO protos."+name+"(release_id,"+std::string(columns)+") SELECT ?";
        for(const auto& c:cols)query+=",incoming.`"+c+"`";
        query+=" FROM JSON_TABLE(?, '$[*]' COLUMNS (";
        for(std::size_t c=0;c<cols.size();++c){if(c)query+=",";query+="`"+cols[c]+"` VARCHAR(192) PATH '$["+std::to_string(c)+"]' ERROR ON EMPTY ERROR ON ERROR";}
        query+=")) AS incoming";plan.push_back({std::move(query),{SqlParam::Text(release),SqlParam::Text(payload.dump())},payload.size(),{}});
    }
}
void Batches(SqlTransaction& plan,Table kind,const Rows& rows,const std::string& release)
{StoreRows(plan,kind==Table::MobDropGroups?"mob_drop_live_group":"mob_drop_live_entry",Spec(kind).columns,rows,release);}
constexpr const char* Names="'mob_drop_live_head','mob_drop_live_release','mob_drop_live_group','mob_drop_live_entry'";
bool ContainersSupported(CAsyncSQL& db)
{
    auto m=Query(db,"SELECT COUNT(*),COALESCE(SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE'),0) FROM information_schema.TABLES WHERE TABLE_SCHEMA='protos' AND TABLE_NAME IN ('container_loot_live_group','container_loot_live_entry')");
    auto row=m->Get()->FetchRow();Need(row && row[0] && row[1],"Invalid live container schema metadata");
    const std::string tables=row[0],engines=row[1];
    auto columns=Query(db,"SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='protos' AND TABLE_NAME='mob_drop_live_release' AND COLUMN_NAME IN ('reward_schema_version','container_group_count','container_entry_count')");
    row=columns->Get()->FetchRow();Need(row && row[0],"Invalid live container columns");
    if(tables=="0" && std::string_view(row[0])=="0")return false;
    Need(tables=="2" && engines=="2" && std::string_view(row[0])=="3","Apply migration 044_live_container_loot.sql completely");return true;
}
}
std::string CatalogHashHex(const ItemCatalogDigest& digest)
{
    std::string out;for(auto b:digest){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;
}
std::expected<std::optional<LiveMobDrops>,std::string> ReadLiveMobDrops(CAsyncSQL& db,std::shared_ptr<const ItemCatalog> base,std::optional<std::string> known)
{
    try{
        Need(base!=nullptr,"Missing base item catalog");
        const auto baseHash=CatalogHashHex(base->ContentHash());
        auto engine=Query(db,"SELECT COUNT(*),COALESCE(SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE'),0) FROM information_schema.TABLES WHERE TABLE_SCHEMA='protos' AND TABLE_NAME IN ("+std::string(Names)+")");
        auto er=engine->Get()->FetchRow();Need(er && er[0] && er[1],"Invalid live drop schema metadata");
        if(std::string_view(er[0])=="0"){
            Need(!known || known->empty(),"Active live drop schema disappeared");
            return std::optional(LiveMobDrops{false,"",baseHash,baseHash,base,{},{}});
        }
        Need(std::string_view(er[0])=="4" && std::string_view(er[1])=="4","Apply migration 043_live_mob_drops.sql completely");
        const bool containersSupported=ContainersSupported(db);
        auto head=Query(db,"SELECT h.release_id,r.base_sha256,r.drop_sha256,r.group_count,r.entry_count"+
            std::string(containersSupported?",r.reward_schema_version,r.container_group_count,r.container_entry_count":"")+
            " FROM protos.mob_drop_live_head h LEFT JOIN protos.mob_drop_live_release r ON r.release_id=h.release_id WHERE h.singleton=1");
        Need(head->Get()->uiNumRows==1,"Missing live drop head");auto row=head->Get()->FetchRow();Need(row!=nullptr,"Missing live drop head");
        if(!row[0]){
            if(known && known->empty())return std::optional<LiveMobDrops>{};
            return std::optional(LiveMobDrops{true,"",baseHash,baseHash,base,{},{},{},{},containersSupported});
        }
        Need(row[1] && row[2] && row[3] && row[4],"Missing live drop release header");
        const std::string id(row[0]),expectedHash(row[2]);
        Need(Hex(id,32) && Hex(expectedHash,64) && std::string_view(row[1])==baseHash,"Live drops require the matching base item catalog");
        if(known && *known==id)return std::optional<LiveMobDrops>{};
        const auto groupsCount=Number<std::uint32_t>(Cell{row[3]}),entriesCount=Number<std::uint32_t>(Cell{row[4]});
        const auto version=containersSupported?Number<std::uint8_t>(Cell{row[5]}):0;
        Need(version<=1,"Unsupported live reward release version");
        const auto boxCount=containersSupported?Number<std::uint32_t>(Cell{row[6]}):0;
        const auto boxEntryCount=containersSupported?Number<std::uint32_t>(Cell{row[7]}):0;
        Need(version || (!boxCount && !boxEntryCount),"Invalid legacy reward release counts");
        auto groups=Query(db,"SELECT "+std::string(Spec(Table::MobDropGroups).columns)+" FROM protos.mob_drop_live_group WHERE release_id=? ORDER BY group_id LIMIT ?",{SqlParam::Text(id),SqlParam::UInt(MaxMobDropGroups+1)});
        auto entries=Query(db,"SELECT "+std::string(Spec(Table::MobDropEntries).columns)+" FROM protos.mob_drop_live_entry WHERE release_id=? ORDER BY group_id,entry_no LIMIT ?",{SqlParam::Text(id),SqlParam::UInt(MaxMobDropGroups*MaxMobDropEntries+1)});
        Rows boxGroups,boxEntries;
        if(version){
            auto boxes=Query(db,"SELECT "+std::string(ContainerGroupColumns)+" FROM protos.container_loot_live_group WHERE release_id=? ORDER BY category_id,local_id LIMIT ?",{SqlParam::Text(id),SqlParam::UInt(MaxLiveContainers+1)});
            auto contents=Query(db,"SELECT "+std::string(ContainerEntryColumns)+" FROM protos.container_loot_live_entry WHERE release_id=? ORDER BY category_id,local_id,entry_no LIMIT ?",{SqlParam::Text(id),SqlParam::UInt(MaxLiveContainers*MaxLootEntries+1)});
            boxGroups=ReadRows(*boxes->Get(),3,MaxLiveContainers);boxEntries=ReadRows(*contents->Get(),10,MaxLiveContainers*MaxLootEntries,true);
        }
        auto loaded=Build(base,ReadRows(*groups->Get(),7,MaxMobDropGroups),ReadRows(*entries->Get(),11,MaxMobDropGroups*MaxMobDropEntries),id,std::move(boxGroups),std::move(boxEntries),containersSupported);
        Need(loaded.groups.size()==groupsCount && loaded.entries.size()==entriesCount && loaded.hash==expectedHash,"Live drop release checksum/count mismatch");
        Need(loaded.containerGroups.size()==boxCount && loaded.containerEntries.size()==boxEntryCount,"Live container release count mismatch");
        return std::optional(std::move(loaded));
    }catch(const std::exception& e){return std::unexpected(std::string(e.what()));}
}
std::expected<LiveDropWrite,std::string> PrepareLiveDropWrite(std::shared_ptr<const ItemCatalog> base,Rows groups,Rows entries,std::string expected,std::string release,
    Rows containerGroups,Rows containerEntries,bool containersSupported)
{
    try{
        Need(expected.empty() || Hex(expected,32),"Invalid expected drop release");
        Need(containersSupported || (containerGroups.empty() && containerEntries.empty()),"Apply migration 044_live_container_loot.sql before editing chests");
        auto state=Build(base,std::move(groups),std::move(entries),std::move(release),std::move(containerGroups),std::move(containerEntries),containersSupported);
        SqlTransaction plan{
            {"SELECT 1 WHERE @@tx_isolation='REPEATABLE-READ' AND @@foreign_key_checks=1 AND @@check_constraint_checks=1",{},{},1},
            {"SELECT 1 FROM information_schema.TABLES WHERE TABLE_SCHEMA='protos' AND TABLE_NAME IN ("+std::string(Names)+") HAVING COUNT(*)=4 AND SUM(ENGINE='InnoDB' AND TABLE_TYPE='BASE TABLE')=4",{},{},1},
            {"SELECT 1 FROM protos.mob_drop_live_head WHERE singleton=1 AND release_id<=>? FOR UPDATE",{expected.empty()?SqlParam::Null():SqlParam::Text(expected)},{},1},
            {"INSERT INTO protos.mob_drop_live_release(release_id,base_sha256,drop_sha256,group_count,entry_count) VALUES(?,?,?,?,?)",{SqlParam::Text(state.release),SqlParam::Text(state.baseHash),SqlParam::Text(state.hash),SqlParam::UInt(state.groups.size()),SqlParam::UInt(state.entries.size())},1,{}}
        };
        if(containersSupported){
            plan.push_back({"UPDATE protos.mob_drop_live_release SET reward_schema_version=1,container_group_count=?,container_entry_count=? WHERE release_id=?",
                {SqlParam::UInt(state.containerGroups.size()),SqlParam::UInt(state.containerEntries.size()),SqlParam::Text(state.release)},1,{}});
            StoreRows(plan,"container_loot_live_group",ContainerGroupColumns,state.containerGroups,state.release);
            StoreRows(plan,"container_loot_live_entry",ContainerEntryColumns,state.containerEntries,state.release);
        }
        Batches(plan,Table::MobDropGroups,state.groups,state.release);Batches(plan,Table::MobDropEntries,state.entries,state.release);
        plan.push_back({"UPDATE protos.mob_drop_live_head SET release_id=? WHERE singleton=1",{SqlParam::Text(state.release)},1,{}});
        Need(plan.size()<=SqlTransactionLimits::Steps,"Too many drop release batches");
        return LiveDropWrite{std::move(state),std::move(plan)};
    }catch(const std::exception& e){return std::unexpected(std::string(e.what()));}
}
std::expected<LiveMobDrops,std::string> PublishLiveMobDrops(CAsyncSQL& db,std::shared_ptr<const ItemCatalog> base,Rows groups,Rows entries,std::string expected,std::string expectedHash,std::string release,
    std::optional<std::pair<Rows,Rows>> containers)
{
    auto current=ReadLiveMobDrops(db,base);if(!current)return std::unexpected(current.error());
    if(!*current || !(**current).supported)return std::unexpected("Apply migration 043_live_mob_drops.sql before saving drops");
    if(containers && !(**current).containersSupported)return std::unexpected("Apply migration 044_live_container_loot.sql before editing chests");
    auto next=PrepareLiveDropWrite(base,std::move(groups),std::move(entries),std::move(expected),std::move(release),
        containers?std::move(containers->first):(**current).containerGroups,containers?std::move(containers->second):(**current).containerEntries,(**current).containersSupported);if(!next)return std::unexpected(next.error());
    if(next->state.hash==(**current).hash)return **current;
    if(expectedHash!=(**current).hash)return std::unexpected("Drop tables changed in another editor; refresh before saving");
    Message result(db.DirectTransactionPrepared(std::move(next->steps)));
    // Reconcile the immutable request ID after both a confirmed and unknown COMMIT.
    auto saved=ReadLiveMobDrops(db,base);
    if(saved && *saved && (**saved).release==next->state.release && (**saved).hash==next->state.hash)return **saved;
    if(result && result->transactionOutcome==SqlTransactionOutcome::Conflict)return std::unexpected("Drop tables changed in another editor; refresh before saving");
    return std::unexpected("Drop publication not confirmed; refresh to reconcile its saved state");
}
}
