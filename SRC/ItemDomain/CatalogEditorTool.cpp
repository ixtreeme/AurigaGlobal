// Administrative draft editor transport. Never linked into the game runtime.
// SQL values use the existing prepared engine; no SQL is accepted from the UI.
// Messages stay ASCII; the manager turns the structured "detail" into UI text.
#include "CatalogSqlReader.hpp"
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace db=auriga::item::sql;
using namespace auriga::item;
using Json=nlohmann::json;
using T=db::Table;
using Message=std::unique_ptr<SQLMsg>;
namespace
{
// A rejection the manager can explain field by field. Nothing was written.
struct Rejected : std::runtime_error
{
    Json detail;
    Rejected(const std::string& reason,Json value):std::runtime_error(reason),detail(std::move(value)) {}
};
void Require(bool condition,const std::string& reason) {if(!condition)throw std::runtime_error(reason);}

template <typename E,std::size_t N> const char* Name(E value,const std::array<const char*,N>& names)
{
    const auto index=static_cast<std::size_t>(value);
    return index<names.size()?names[index]:"Unknown";
}
// Index order equals the enum declaration order. The sentinels catch reordering.
constexpr std::array<const char*,19> ReadNames{"InvalidPin","RowLimit","ByteBudget","ColumnCount","NullValue","InvalidNumber",
    "InvalidText","InvalidKey","RegistryMismatch","FormatMismatch","OrphanRow","DuplicateRow","InvalidChild","InvalidCatalog",
    "HashMismatch","SqlFailure","IsolationRequired","SnapshotUnconfirmed","ResultShape"};
static_assert(static_cast<std::size_t>(db::ReadErrorCode::ResultShape)==ReadNames.size()-1);
constexpr std::array<const char*,27> CatalogNames{"InvalidVersion","LimitExceeded","InvalidIdentity","DuplicateProfile",
    "InvalidProfile","MissingProfile","InvalidRefinement","DuplicateItem","DuplicateContentCode","InvalidBase",
    "UnsupportedCategory","InvalidBonusProfile","WrongBonusSource","WrongCategory","DuplicateCategoryRow","MissingCategoryRow",
    "OrphanCategoryRow","WrongRefinementKind","StatOutOfRange","HashFailure","InvalidSocketProfile","InvalidSocketMaterial",
    "InvalidRecoveryProfile","InvalidLootProfile","InvalidLootReward","InvalidContainer","InvalidRefinementRecipe"};
static_assert(static_cast<std::size_t>(ItemCatalogErrorCode::InvalidRefinementRecipe)==CatalogNames.size()-1);
constexpr std::array<const char*,13> RefinementNames{"InvalidIdentity","InvalidKind","InvalidLevelRange","IncompleteLevels",
    "InvalidGain","NonzeroLevelZero","NonMonotonicGain","WrongStatKind","MissingBonusProfile","WrongBonusSource",
    "InvalidBonusContext","StatOutOfRange","AtMaximumLevel"};
static_assert(static_cast<std::size_t>(RefinementError::AtMaximumLevel)==RefinementNames.size()-1);
constexpr std::array<const char*,8> BonusNames{"InvalidIdentity","InvalidSource","EmptyProfile","TooManyEntries","UnknownBonus",
    "DuplicateBonus","ValueOutOfRange","SourceNotAllowed"};
static_assert(static_cast<std::size_t>(BonusProfileError::SourceNotAllowed)==BonusNames.size()-1);
constexpr std::array<const char*,11> RecoveryNames{"InvalidIdentity","InvalidUnits","InvalidContexts","InvalidCooldown",
    "InvalidAmount","EmptyEffect","InvalidInterval","InvalidDuration","MultiplePools","InvalidAutomatic","InvalidArenaQuota"};
static_assert(static_cast<std::size_t>(RecoveryProfileError::InvalidArenaQuota)==RecoveryNames.size()-1);
constexpr std::array<const char*,10> LootNames{"InvalidIdentity","InvalidMode","EmptyProfile","TooManyEntries","InvalidEntryOrder",
    "InvalidReward","InvalidAmount","InvalidWeight","InvalidRareChance","WeightOverflow"};
static_assert(static_cast<std::size_t>(LootProfileError::WeightOverflow)==LootNames.size()-1);

Json Detail(const db::ReadError& error)
{
    Json out{{"read",Name(error.code,ReadNames)},{"row",error.row},{"column",error.column}};
    if(error.table)out["table"]=std::string(db::Tables()[static_cast<std::size_t>(*error.table)].name);
    if(const auto& c=error.catalogError) {
        out["catalog"]=Name(c->code,CatalogNames);out["field"]=c->field;
        if(c->key)out["key"]={static_cast<unsigned>(c->key->Category()),c->key->LocalId()};
        if(c->profileId)out["profile"]=c->profileId;
        if(c->refinementError)out["refinement"]=Name(*c->refinementError,RefinementNames);
        if(c->bonusError)out["bonus"]=Name(*c->bonusError,BonusNames);
        if(c->recoveryError)out["recovery"]=Name(*c->recoveryError,RecoveryNames);
        if(c->lootError)out["loot"]=Name(*c->lootError,LootNames);
        if(c->socketError)out["socket"]=static_cast<int>(*c->socketError);
    }
    return out;
}
Message Query(CAsyncSQL& sql,const std::string& query,SqlParams params={}) {
    Message result(sql.DirectQueryPrepared(query.c_str(),params));
    Require(result && !result->uiSQLErrno && result->Get(),"SQL kapcsolat vagy jogosultsag hiba.");return result;
}
std::string Hash(const ItemCatalogDigest& digest) {
    std::string out;for(auto value:digest){out+="0123456789abcdef"[value>>4];out+="0123456789abcdef"[value&15];}return out;
}
db::CapturedCatalog Capture(CAsyncSQL& sql) {
    Message message(sql.DirectTransactionPrepared(db::PrepareRead()));
    auto result=db::CaptureCompleted(*message,{1,{}});
    if(!result)throw Rejected("A protos katalogus nem olvashato: formatum, jogosultsag vagy adatellenorzesi hiba.",Detail(result.error()));
    return std::move(*result);
}
Json Encode(const db::Snapshot& snapshot) {
    Json output=Json::object();
    for(const auto& spec:db::Tables()) {
        auto& rows=output[spec.name];rows=Json::array();
        for(const auto& row:snapshot[spec.table]) {
            Json cells=Json::array();for(const auto& cell:row)cells.push_back(cell?Json(*cell):Json(nullptr));rows.push_back(std::move(cells));
        }
    }
    return output;
}
db::Snapshot Decode(const Json& document) {
    Require(document.is_object() && document.size()==db::TableCount,"Hibas katalogus alak.");
    db::Snapshot result;std::size_t budget=0;
    for(const auto& spec:db::Tables()) {
        const auto& rows=document.at(std::string(spec.name));
        Require(rows.is_array() && rows.size()<=spec.maximumRows,"Tul sok katalogussor.");
        for(const auto& row:rows) {
            Require(row.is_array() && row.size()==db::ColumnCount(spec),"Hibas katalogus oszlopszam.");
            db::Row cells;
            for(const auto& value:row) {
                Require(value.is_null() || value.is_string(),"A cella string vagy null legyen.");
                db::Cell cell=value.is_null()?db::Cell{}:db::Cell{value.get<std::string>()};
                const auto cost=db::CellBudgetOverhead+(cell?cell->size():0);
                Require(cost<=db::CapturedByteBudget-budget,"Tul nagy katalogus.");budget+=cost;
                cells.push_back(std::move(cell));
            }
            result[spec.table].push_back(std::move(cells));
        }
    }
    return result;
}
std::shared_ptr<const ItemCatalog> Validate(const db::Snapshot& snapshot) {
    auto validated=db::DecodeSnapshot(snapshot,{1,{}});
    if(!validated)throw Rejected("Hibas targyadat. Semmi nem lett mentve.",Detail(validated.error()));
    return *validated;
}
unsigned KeyColumns(T table) {
    switch(table) {
    case T::Items:case T::Weapons:case T::Armors:case T::Accessories:case T::Materials:case T::Consumables:
    case T::BonusEntries:case T::RefinementLevels:case T::SocketGrades:case T::BookTargets:case T::BookSteps:case T::SkillBooks:
    case T::LootEntries:case T::Containers:case T::ContainerKeys:case T::CurrencyTokens:case T::QuestItems:case T::SocketStones:case T::Costumes:case T::Tools:case T::FishingItems:case T::ToolLevels:case T::PolymorphSkills:case T::PolymorphMasteries:case T::PolymorphItems:return 2;
    case T::CostumeVisuals:return 4;
    case T::SocketMaterials:case T::RefinementIngredients:return 4;
    default:return 1;
    }
}
using RowMap=std::map<db::Row,db::Row>;
RowMap Index(const db::Rows& rows,unsigned count) {
    RowMap result;for(const auto& row:rows)Require(result.emplace(db::Row(row.begin(),row.begin()+count),row).second,"Ismetelt kulcs.");return result;
}
SqlParams Parameters(const db::Row& row) {
    SqlParams out;for(const auto& cell:row)out.push_back(cell?SqlParam::Text(*cell):SqlParam::Null());return out;
}
std::vector<std::string> Columns(std::string_view text) {
    std::vector<std::string> out;
    while(true){auto end=text.find(',');out.emplace_back(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}return out;
}
template <typename V> db::Cell Cell(V value) {
    if constexpr(std::is_enum_v<V>)return std::to_string(static_cast<std::underlying_type_t<V>>(value));
    else if constexpr(std::is_convertible_v<V,std::string_view>)return std::string(std::string_view(value));
    else return std::to_string(value);
}
template <typename... V> void Add(db::Snapshot& snapshot,T table,V... values) {snapshot[table].push_back({Cell(values)...});}
// The compiled registries are the only rows a fresh protos database starts with.
db::Snapshot Registry() {
    db::Snapshot s;
    Add(s,T::Format,1,db::CatalogSchemaVersion,db::CanonicalDomain);
    for(const auto& r:ItemCategories)Add(s,T::Categories,r.category,r.code,r.table);
    for(const auto& d:BonusDefinitions)
        Add(s,T::BonusDefinitions,d.id,d.code,d.displayKey,d.unit,d.target,d.stage,d.channel,d.action,d.mode,d.family,d.rank,
            d.element,d.stacking,d.aggregateCap,d.allowedSources,d.minimum,d.maximum);
    return s;
}
Json Packet(const db::Snapshot& rows,const ItemCatalog& catalog,Json sequences) {
    Json definitions=Json::array();
    for(const auto& d:BonusDefinitions)definitions.push_back({{"id",d.id},{"code",d.code},{"display",d.displayKey},
        {"unit",static_cast<int>(d.unit)},{"minimum",d.minimum},{"maximum",d.maximum},{"sources",d.allowedSources}});
    Json columns=Json::object();for(const auto& spec:db::Tables())columns[spec.name]=Columns(spec.columns);
    return {{"hash",Hash(catalog.ContentHash())},{"tables",Encode(rows)},{"columns",columns},{"sequences",std::move(sequences)},{"bonuses",definitions}};
}
Json Result(CAsyncSQL& sql,const db::CapturedCatalog& current) {
    Json sequences=Json::object();auto message=Query(sql,"SELECT category_id,next_local_id FROM protos.item_local_id_sequence");
    while(auto row=message->Get()->FetchRow()) {Require(row[0] && row[1],"Hibas azonosito-szamlalo.");sequences[row[0]]=row[1];}
    return Packet(current.rows,*current.catalog,std::move(sequences));
}
// A bounded batch is one bound value, not one transaction step per cell/row.
// JSON is transport only; the destination remains the typed SQL columns.
// This keeps a +255 curve with distinct level bonuses within the existing
// SQL engine's step/parameter budgets. No generic SQL limits are increased.
void AddBatches(SqlTransaction& plan,const db::TableSpec& spec,const db::Rows& rows,char action) {
    constexpr std::size_t batchSize=100;
    const auto columns=Columns(spec.columns);const auto keys=KeyColumns(spec.table);
    const auto width=action=='D'?keys:columns.size();
    for(std::size_t start=0;start<rows.size();start+=batchSize) {
        Json payload=Json::array();
        for(std::size_t i=start;i<(std::min)(rows.size(),start+batchSize);++i) {
            Json row=Json::array();for(std::size_t c=0;c<width;++c)row.push_back(rows[i][c]?Json(*rows[i][c]):Json(nullptr));
            payload.push_back(std::move(row));
        }
        std::string source="JSON_TABLE(?, '$[*]' COLUMNS (";
        for(std::size_t c=0;c<width;++c) {
            if(c)source+=",";
            // Domain validation bounds all mutable text cells to <=192 bytes.
            source+="`"+columns[c]+"` VARCHAR(192) PATH '$["+std::to_string(c)+"]' ERROR ON EMPTY ERROR ON ERROR";
        }
        source+=")) AS incoming";
        const std::string table="protos.`"+std::string(spec.name)+"`";
        std::string query;
        if(action=='I') {
            query="INSERT INTO "+table+" ("+std::string(spec.columns)+") SELECT ";
            for(std::size_t c=0;c<width;++c){if(c)query+=",";query+="incoming.`"+columns[c]+"`";}
            query+=" FROM "+source;
        } else {
            query=(action=='D'?"DELETE target FROM ":"UPDATE ")+table+" AS target JOIN "+source+" ON ";
            for(std::size_t c=0;c<keys;++c){if(c)query+=" AND ";query+="target.`"+columns[c]+"`=incoming.`"+columns[c]+"`";}
            if(action=='U') {
                query+=" SET ";
                for(std::size_t c=keys;c<width;++c){if(c>keys)query+=",";query+="target.`"+columns[c]+"`=incoming.`"+columns[c]+"`";}
            }
        }
        plan.push_back({std::move(query),{SqlParam::Text(payload.dump())},payload.size(),{}});
    }
}
void Commit(CAsyncSQL& sql,const db::CapturedCatalog& before,const db::Snapshot& after) {
    // Parents before children. Deletes use the reverse order. The UI edits
    // profile contents without changing their category/delivery discriminator.
    constexpr T order[]={T::UsePolicies,T::TradePolicies,T::Appearances,T::Attacks,T::AmmoPolicies,T::Resistances,
        T::PolymorphForms,T::PolymorphProfiles,T::PolymorphSkills,T::PolymorphMasteries,T::ToolProfiles,T::ToolLevels,T::BookProfiles,T::BookTargets,T::BookSteps,T::BonusProfiles,T::BonusEntries,T::Refinements,T::RefinementLevels,T::SocketProfiles,T::SocketGrades,
        T::Cooldowns,T::TimedEffectGroups,T::TimedEffects,T::ToggleEffectGroups,T::ToggleEffects,T::Recoveries,T::Instant,T::Reserve,T::Periodic,T::Automatic,T::LootProfiles,T::Items,T::Weapons,T::Armors,T::Materials,
        T::Accessories,T::Consumables,T::SkillBooks,T::Containers,T::ContainerKeys,T::CurrencyTokens,T::QuestItems,T::SocketStones,T::Costumes,T::CostumeVisuals,T::Tools,T::FishingItems,T::PolymorphItems,T::LootEntries,T::SocketMaterials,T::RefinementRecipes,T::RefinementIngredients};
    static_assert(std::size(order)==db::TableCount-3,"every editable table needs a commit position");
    for(T table:{T::Format,T::Categories,T::BonusDefinitions})Require(before.rows[table]==after[table],"A rendszerregiszter nem szerkesztheto.");
    // Published group IDs own common conflict/lifecycle rules. Item edits may
    // add groups, but cannot silently change an existing group's behavior.
    for(T table:{T::TimedEffectGroups,T::ToggleEffectGroups}) {
        const auto oldGroups=Index(before.rows[table],1),newGroups=Index(after[table],1);
        for(const auto& [key,row]:oldGroups)
            Require(newGroups.contains(key) && row==newGroups.at(key),"A meglevo hatascsoport nem modosithato vagy torolheto; hozz letre uj csoportot.");
    }
    const auto oldForms=Index(before.rows[T::PolymorphForms],1),newForms=Index(after[T::PolymorphForms],1);
    for(const auto& [key,row]:oldForms)Require(newForms.contains(key) && row==newForms.at(key),"A meglevo polimorf forma nem modosithato vagy torolheto; hozz letre uj format.");
    SqlTransaction plan;
    const auto oldItems=Index(before.rows[T::Items],2),newItems=Index(after[T::Items],2);
    Require(newItems.size()>=oldItems.size(),"Targyat kivezetessel torolj; tartos kulcsot nem torlunk fizikailag.");
    for(const auto& [key,row]:oldItems)Require(newItems.contains(key),"A targykulcs nem torolheto vagy modosithato.");
    unsigned added=0;
    for(const auto& [key,row]:newItems)if(!oldItems.contains(key)) {
        Require(++added<=1,"Egyszerre egy uj targy hozhato letre.");
        plan.push_back({"SELECT 1 FROM protos.item_local_id_sequence WHERE category_id=? AND next_local_id=? FOR UPDATE",Parameters(key),{},1});
        plan.push_back({"UPDATE protos.item_local_id_sequence SET next_local_id=next_local_id+1 WHERE category_id=? AND next_local_id=?",Parameters(key),1,{}});
    }
    for(T table:order) {
        const auto& spec=db::Tables()[static_cast<std::size_t>(table)];
        const auto keys=KeyColumns(table);
        auto old=Index(before.rows[table],keys),fresh=Index(after[table],keys);
        db::Rows inserted,updated;
        for(const auto& [key,row]:fresh) {
            const auto found=old.find(key);
            if(found!=old.end() && found->second==row)continue;
            (found==old.end()?inserted:updated).push_back(row);
        }
        // Plain INSERT preserves unique-code conflict semantics. UPDATE has
        // an exact affected-row precondition; a missing row rolls back.
        AddBatches(plan,spec,inserted,'I');AddBatches(plan,spec,updated,'U');
    }
    for(auto it=std::rbegin(order);it!=std::rend(order);++it) {
        const auto& spec=db::Tables()[static_cast<std::size_t>(*it)];
        auto old=Index(before.rows[*it],KeyColumns(*it)),fresh=Index(after[*it],KeyColumns(*it));
        db::Rows removed;
        for(const auto& [key,row]:old)if(!fresh.contains(key)) {
            removed.push_back(row);
        }
        AddBatches(plan,spec,removed,'D');
    }
    if(plan.empty())return;
    Require(plan.size()<=SqlTransactionLimits::Steps,"Tul sok valtozas egy menteshez.");
    Message result(sql.DirectTransactionPrepared(std::move(plan)));
    Require(result && !result->uiSQLErrno && result->transactionOutcome==SqlTransactionOutcome::Committed,
        "A mentes nem igazolt. Frissitsd a listat a megismetles elott; a korabbi eredmenyt ujra ellenorizni kell.");
}
Json Run(const Json& request) {
    const auto action=request.at("action").get<std::string>();
    Require(action=="read" || action=="commit" || action=="validate" || action=="blank","Ismeretlen muvelet.");
    // Offline actions: no connection, no lock, no write.
    if(action=="validate")return {{"hash",Hash(Validate(Decode(request.at("tables")))->ContentHash())}};
    if(action=="blank") {
        const auto rows=Registry();Json sequences=Json::object();
        for(const auto& r:ItemCategories)sequences[std::to_string(static_cast<unsigned>(r.category))]="1";
        return Packet(rows,*Validate(rows),std::move(sequences));
    }
    const auto& auth=request.at("connection");
    const auto port=auth.at("port").get<unsigned>();Require(port>0 && port<=65535,"Hibas port.");
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    const auto user=auth.at("user").get<std::string>(),password=auth.at("password").get<std::string>();
    Require(sql.Setup("127.0.0.1",user.c_str(),password.c_str(),"protos","utf8mb4",true,port),"A helyi protos adatbazis nem erheto el.");
    if(action=="read")return Result(sql,Capture(sql));
    auto lock=Query(sql,"SELECT GET_LOCK('auriga:protos:editor',5)");
    auto lockRow=lock->Get()->FetchRow();Require(lockRow && lockRow[0] && std::string_view(lockRow[0])=="1","Masik targyszerkeszto ment; probald ujra.");
    // Closing this dedicated connection releases the advisory lock on every path.
    const auto current=Capture(sql);auto next=Decode(request.at("tables"));
    const auto validated=Validate(next);
    if(Hash(current.catalog->ContentHash())==Hash(validated->ContentHash()))return Result(sql,current);
    Require(request.at("expected_hash").get<std::string>()==Hash(current.catalog->ContentHash()),"Az adatokat masik szerkeszto modositotta. Frissitsd a listat.");
    Commit(sql,current,next);
    auto saved=Capture(sql);
    Require(saved.catalog->ContentHash()==validated->ContentHash(),"A visszaolvasott allapot elter. Frissites szukseges.");
    return Result(sql,saved);
}
void Reply(const Json& packet) {std::cout<<"AURIGA_CATALOG_RESULT "<<packet.dump()<<'\n';}
}
std::shared_ptr<spdlog::logger> logging::GetLogger(){static auto logger=std::make_shared<spdlog::logger>("catalog-editor");return logger;}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger(){return logging::GetLogger();}
int main() {
    try {
        std::string input;char chunk[8192];
        while(std::cin.read(chunk,sizeof(chunk)) || std::cin.gcount()) {Require(input.size()+std::cin.gcount()<=32*1024*1024,"Tul nagy keres.");input.append(chunk,static_cast<std::size_t>(std::cin.gcount()));}
        auto result=Run(Json::parse(input));Reply({{"ok",true},{"result",std::move(result)}});return 0;
    }catch(const Json::exception&){Reply({{"ok",false},{"error","Hibas keresszerkezet."}});return 1;}
    catch(const Rejected& error){Reply({{"ok",false},{"error",error.what()},{"detail",error.detail}});return 1;}
    catch(const std::exception& error){Reply({{"ok",false},{"error",error.what()}});return 1;}
}
