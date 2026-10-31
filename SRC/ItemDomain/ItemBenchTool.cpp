// Local item test bench. Drives the REAL native planners and the transactional
// persistence writer for one dedicated test owner in the package's own DB.
// Never linked into the game runtime; never touches player.item or players.
#include "CatalogSqlReader.hpp"
#include "ItemPersistenceSql.hpp"
#include <SQL/AsyncSQL.h>
#include <Core/Logging.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>

namespace p=auriga::item::persistence;
namespace s=auriga::item::sql;
using namespace auriga::item;
using Json=nlohmann::json;
using Message=std::unique_ptr<SQLMsg>;
namespace
{
// The character a request works on: the dedicated test character by default,
// or a real character (its PID) to prepare typed items for an in-game test.
constexpr std::uint32_t TestCharacter=990001;
ItemOwner BenchOwner{ItemOwnerKind::Character,TestCharacter};
// Every working request claims a new session first, like a login does.
std::uint64_t BenchSession=0;
constexpr std::uint64_t BenchCatalogRevision=1,BenchIdBase=0x00BE000000000000ULL;
ItemStorageLayout Inventory{{BenchOwner,ItemStorageKind::Inventory,1},5,9,4};
// Slots 1..8 (weapon, body, head, shield, feet, wrist, neck, ear) at anchors 0..7.
ItemStorageLayout Equipment{{BenchOwner,ItemStorageKind::Equipment,1},8,1,1};

struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };
void Require(bool condition,const std::string& reason){if(!condition)throw Failure(reason);}
std::mt19937_64& Random(){static std::mt19937_64 engine{std::random_device{}()};return engine;}
std::uint32_t Uniform(std::uint32_t low,std::uint32_t high){return std::uniform_int_distribution<std::uint32_t>(low,high)(Random());}
std::array<std::uint8_t,16> OperationId(){std::array<std::uint8_t,16> id{};for(auto& b:id)b=static_cast<std::uint8_t>(Uniform(0,255));return id;}
std::string Hex(std::span<const std::uint8_t> bytes){std::string out;for(auto b:bytes){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;}

const char* PersistenceText(ItemPersistenceError e)
{
    static const char* texts[]={"érvénytelen tulajdonos","érvénytelen tároló","érvénytelen tárgyállapot (darab vagy kötés)","hiányzó tárgy",
        "ez a kategória a mentés első fázisában még nem támogatott","a fejlesztési szint nincs a tárgy profiljában","érvénytelen bónuszállapot",
        "érvénytelen foglalatállapot","nincs hely, vagy ütközik egy másik tárggyal","érvénytelen művelet","elavult tulajdonos-kerítés",
        "érvénytelen változás","érvénytelen pénztárca-változás","túl sok adat egy műveletben","katalóguseltérés","hash-hiba","adatbázishiba",
        "ütközés egy párhuzamos mentéssel (próbáld újra)","a mentés eredménye ismeretlen","nyugtaeltérés","sérült tárolt adat",
        "nincs ilyen tulajdonos (nincs kiépítve)", "a karakter nem felel meg a feltételeknek (szint, kaszt vagy nem)" };
    const auto code=static_cast<std::size_t>(e);return code<std::size(texts)?texts[code]:"ismeretlen hiba";
}
const char* RefinementText(RefinementUseError e)
{
    static const char* texts[]={"a szabályok nem engedik","érvénytelen műveleti környezet","nincs ilyen recept",
        "a tárgy nem felel meg a receptnek (tárgy, szint vagy darabszám)","az inventory állapota érvénytelen","nincs elég Yang",
        "hiányzik egy hozzávaló, vagy nincs belőle elég","érvénytelen dobás"};
    const auto code=static_cast<std::size_t>(e);return code<std::size(texts)?texts[code]:"ismeretlen hiba";
}
Message Query(CAsyncSQL& sql,const std::string& query,SqlParams params={})
{
    Message result(sql.DirectQueryPrepared(query.c_str(),params));
    Require(result && !result->uiSQLErrno,"SQL hiba: "+query.substr(0,60));return result;
}
std::vector<std::vector<std::string>> Rows(CAsyncSQL& sql,const std::string& query,SqlParams params={})
{
    auto m=Query(sql,query,std::move(params));std::vector<std::vector<std::string>> out;
    const auto* result=m->Get();if(!result)return out;
    // Prepared results are materialized rows with explicit lengths.
    for(std::size_t r=0;r<result->preparedRows.size();++r)
    {
        std::vector<std::string> cells;
        for(std::size_t i=0;i<result->preparedRows[r].size();++i)
        {
            const auto* value=result->preparedRows[r][i];
            cells.push_back(value?std::string(value,result->preparedLengths[r][i]):std::string());
        }
        out.push_back(std::move(cells));
    }
    return out;
}
bool Installed(CAsyncSQL& sql)
{
    return Rows(sql,"SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='player' AND TABLE_NAME IN "
        "('item_persistence_format','item_owner_fence','item_storage','item_wallet','item_operation_receipt','item_instance',"
        "'item_instance_attribute','item_instance_socket','item_occupied_cell')").at(0).at(0)=="9";
}
std::shared_ptr<const ItemCatalog> Catalog(CAsyncSQL& sql)
{
    auto catalog=s::ReadCatalog(sql,{BenchCatalogRevision,{}});
    Require(catalog.has_value(),"A protos katalógus nem olvasható vagy nem érvényes.");return *catalog;
}
std::optional<std::string> PinHash(CAsyncSQL& sql)
{
    const auto rows=Rows(sql,"SELECT HEX(catalog_sha256) FROM player.item_persistence_format WHERE singleton=1");
    if(rows.empty())return std::nullopt;
    auto hex=rows[0][0];std::ranges::transform(hex,hex.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return hex;
}
bool OwnerExists(CAsyncSQL& sql)
{
    return !Rows(sql,"SELECT 1 FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=?",
        {SqlParam::UInt(static_cast<unsigned>(BenchOwner.kind)),SqlParam::UInt(BenchOwner.id)}).empty();
}
p::LoadedItemOwner Load(CAsyncSQL& sql,const ItemCatalog& c)
{
    auto loaded=p::LoadItemOwner(sql,c,BenchOwner,BenchSession);
    Require(loaded.has_value(),std::string("A próbapad állapota nem tölthető be: ")+PersistenceText(loaded.error())+".");
    return std::move(*loaded);
}
Json State(const ItemCatalog& c,const p::LoadedItemOwner& o)
{
    Json items=Json::array();
    for(const auto& i:o.items)
    {
        const auto* e=c.Find(i.key);
        Json attributes=Json::array();
        for(std::size_t slot=0;slot<i.attributes.slots.size();++slot)
            if(const auto& a=i.attributes.slots[slot])attributes.push_back({{"slot",slot},{"bonus",static_cast<unsigned>(a->bonus)},{"value",a->valueScaled}});
        Json stones=Json::array();
        for(const auto& st:i.stones)
            stones.push_back({{"state",static_cast<int>(st.state)},{"grade",st.grade},{"level",st.refinementLevel},
                {"stone",st.stone?Json::array({static_cast<unsigned>(st.stone->Category()),st.stone->LocalId()}):Json(nullptr)}});
        items.push_back({{"id",std::to_string(i.id.Value())},{"category",static_cast<unsigned>(i.key.Category())},{"local",i.key.LocalId()},
            {"content_code",e?e->base.contentCode:""},{"level",i.refinementLevel},{"count",i.count},{"revision",i.revision},
            {"destroyed",i.destroyed},{"cell",i.placement && i.placement->kind==ItemStorageKind::Inventory?Json(i.placement->anchor):Json(nullptr)},
            {"equipped_slot",i.placement && i.placement->kind==ItemStorageKind::Equipment?Json(i.placement->anchor+1):Json(nullptr)},{"attributes",attributes},
            {"locked_slot",i.attributes.lockedSlot?Json(*i.attributes.lockedSlot):Json(nullptr)},{"stones",stones},
            {"bound",i.boundTo.has_value()}});
    }
    Json wallets=Json::array();
    for(const auto& w:o.wallets)wallets.push_back({{"currency",static_cast<int>(w.currency)},{"balance",w.balance},{"revision",w.revision}});
    return {{"owner",BenchOwner.id},{"session",o.fence.sessionGeneration},{"revision",o.fence.revision},{"wallets",wallets},
            {"layout",{{"columns",Inventory.columns},{"rows",Inventory.rows},{"pages",Inventory.pages}}},{"items",items}};
}
const PersistentItem& Item(const p::LoadedItemOwner& o,const Json& request,const char* field)
{
    const auto id=std::stoull(request.at(field).get<std::string>());
    const auto it=std::ranges::find_if(o.items,[&](const auto& i){return i.id.Value()==id && !i.destroyed;});
    Require(it!=o.items.end(),"A tárgy nem található (már nem létezik?).");return *it;
}
std::set<std::uint32_t> Occupied(const ItemCatalog& c,const p::LoadedItemOwner& o,std::optional<std::uint64_t> except={})
{
    std::set<std::uint32_t> cells;
    for(const auto& i:o.items)
    {
        // Only the main inventory grid; equipped items live in their own slots.
        if(i.destroyed || !i.placement || i.placement->kind!=ItemStorageKind::Inventory || i.placement->storageId!=1 || (except && i.id.Value()==*except))continue;
        auto footprint=OccupiedItemCells(c,i,Inventory);Require(footprint.has_value(),"Hibás tárolt elhelyezés.");
        cells.insert(footprint->begin(),footprint->end());
    }
    return cells;
}
bool Fits(const ItemCatalog& c,PersistentItem item,std::uint32_t anchor,const std::set<std::uint32_t>& occupied)
{
    item.placement=ItemPlacement{ItemStorageKind::Inventory,1,anchor};
    const auto footprint=OccupiedItemCells(c,item,Inventory);
    return footprint && std::ranges::none_of(*footprint,[&](auto cell){return occupied.contains(cell);});
}
ItemCommit Base(const ItemCatalog& c,const p::LoadedItemOwner& o,ItemOperationKind kind)
{
    ItemCommit r;r.operationId=OperationId();r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();r.kind=kind;
    r.owners={o.fence};r.storages={Inventory};r.outcomeCode=1;r.result={static_cast<std::uint8_t>(kind)};return r;
}
Json Commit(CAsyncSQL& sql,const ItemCatalog& c,const ItemCommit& r,Json extra={})
{
    const auto result=p::CommitItems(sql,c,r);
    Require(result.has_value(),std::string("A mentés elutasítva: ")+PersistenceText(result.error())+". Semmi nem változott.");
    extra["operation"]=Hex(r.operationId);extra["replay"]=result->replay;
    extra["state"]=State(c,Load(sql,c));return extra;
}
SocketItemPair Pair(const PersistentItem& source,const PersistentItem& target,const ItemOwnerFence& fence)
{
    const auto snapshot=[](const PersistentItem& i){return StoneItemSnapshot{i.key,i.id.Value(),i.revision,i.owner.id,i.refinementLevel,i.count};};
    SocketItemPair pair{{},snapshot(source),snapshot(target)};auto& ctx=pair.context;ctx.operationId=OperationId();ctx.actorId=fence.owner.id;
    ctx.sessionGeneration=fence.sessionGeneration;ctx.rulesetRevision=1;ctx.inventoryRevision=fence.revision;
    ctx.characterLevel=120;ctx.characterClass=1;ctx.sex=1;ctx.admittedByServer=true;
    pair.sourceInInventory=pair.targetInInventory=true;return pair;
}

// The item editor changes protos while typed items exist. Like the stopped
// publisher, move the catalog pin only when EVERY stored owner (test and real
// characters alike) loads and validates against the new catalog. Local only.
bool FollowCatalog(CAsyncSQL& sql,const ItemCatalog& c)
{
    const std::string digest(reinterpret_cast<const char*>(c.ContentHash().data()),c.ContentHash().size());
    const auto old=Rows(sql,"SELECT catalog_revision,catalog_sha256 FROM player.item_persistence_format WHERE singleton=1");
    if(old.empty())
    {
        Query(sql,"INSERT INTO player.item_persistence_format VALUES(1,1,?,?)",{SqlParam::UInt(c.Revision()),SqlParam::Blob(digest)});
        return true;
    }
    if(PinHash(sql)==Hex(c.ContentHash()))return false;
    const auto pin=[&](std::uint64_t revision,const std::string& value){
        Query(sql,"UPDATE player.item_persistence_format SET catalog_revision=?,catalog_sha256=? WHERE singleton=1",{SqlParam::UInt(revision),SqlParam::Blob(value)});};
    pin(c.Revision(),digest);
    for(const auto& row:Rows(sql,"SELECT owner_kind,owner_id,session_generation FROM player.item_owner_fence ORDER BY owner_kind,owner_id LIMIT 100000"))
    {
        const ItemOwner owner{static_cast<ItemOwnerKind>(std::stoul(row[0])),std::stoull(row[1])};
        if(const auto loaded=p::LoadItemOwner(sql,c,owner,std::stoull(row[2]));!loaded)
        {
            pin(std::stoull(old[0][0]),old[0][1]);
            throw Failure("A katalógus változott, és a #"+row[1]+" tulajdonos tárgyai az új katalógussal nem érvényesek ("+PersistenceText(loaded.error())+
                "). Állítsd vissza azt a karaktert a próbapadon.");
        }
    }
    return true;
}
void Claim(CAsyncSQL& sql,const ItemCatalog& c)
{
    const auto session=p::ClaimOwnerSession(sql,c,BenchOwner);
    Require(session.has_value(),std::string("A karakter munkamenete nem igényelhető: ")+(session?"":PersistenceText(session.error()))+".");
    BenchSession=*session;
}

// --- actions -------------------------------------------------------------------
Json Setup(CAsyncSQL& sql,const Json& request)
{
    Require(Installed(sql),"A tartós mentési táblák nincsenek telepítve.");
    const auto c=Catalog(sql);
    const bool reset=request.value("reset",false);
    if(OwnerExists(sql) && !reset)
    {
        // Owners provisioned before equipping existed get their equipment storage now.
        Query(sql,"INSERT IGNORE INTO player.item_storage VALUES(?,?,?,?,?,?,?)",{SqlParam::UInt(static_cast<unsigned>(BenchOwner.kind)),SqlParam::UInt(BenchOwner.id),
            SqlParam::UInt(static_cast<unsigned>(ItemStorageKind::Equipment)),SqlParam::UInt(1),SqlParam::UInt(Equipment.columns),SqlParam::UInt(1),SqlParam::UInt(1)});
        const bool followed=FollowCatalog(sql,*c);
        Claim(sql,*c);
        return {{"state",State(*c,Load(sql,*c))},{"catalog_followed",followed}};
    }
    const auto gold=request.value("gold",std::uint64_t{1'000'000'000});
    Require(gold<=static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()),"Túl nagy összeg.");
    // A reset removes this character's typed state FIRST: its items may be
    // the very ones a changed catalog no longer accepts.
    const auto owner=SqlParams{SqlParam::UInt(static_cast<unsigned>(BenchOwner.kind)),SqlParam::UInt(BenchOwner.id)};
    SqlTransaction tx;
    const auto children=[&](const char* table){
        tx.push_back({std::string("DELETE x FROM player.")+table+" x JOIN player.item_instance i ON i.instance_id=x.instance_id WHERE i.owner_kind=? AND i.owner_id=?",owner,{},{}});};
    children("item_occupied_cell");children("item_instance_attribute");children("item_instance_socket");
    tx.push_back({"DELETE FROM player.item_occupied_cell WHERE owner_kind=? AND owner_id=?",owner,{},{}});
    tx.push_back({"DELETE FROM player.item_instance WHERE owner_kind=? AND owner_id=?",owner,{},{}});
    tx.push_back({"DELETE FROM player.item_wallet WHERE owner_kind=? AND owner_id=?",owner,{},{}});
    tx.push_back({"DELETE FROM player.item_storage WHERE owner_kind=? AND owner_id=?",owner,{},{}});
    tx.push_back({"DELETE FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=?",owner,{},{}});
    Message result(sql.DirectTransactionPrepared(std::move(tx)));
    Require(result && !result->uiSQLErrno && result->transactionOutcome==SqlTransactionOutcome::Committed,"A korábbi tárgyak törlése nem sikerült.");
    const bool followed=FollowCatalog(sql,*c);
    // The same provisioning the importer and character creation use.
    const ItemWallet wallet{BenchOwner,WalletCurrency::Gold,1,gold};
    const ItemStorageLayout layouts[]{Inventory,Equipment};
    auto made=p::ProvisionOwner(sql,*c,BenchOwner,layouts,std::span(&wallet,1));
    Require(made && *made,std::string("A karakter kiépítése nem sikerült: ")+(made?"már létezik":PersistenceText(made.error()))+".");
    Claim(sql,*c);
    return {{"state",State(*c,Load(sql,*c))},{"catalog_followed",followed}};
}
Json Status(CAsyncSQL& sql)
{
    Json out{{"installed",Installed(sql)}};
    const auto c=Catalog(sql);out["catalog_hash"]=Hex(c->ContentHash());
    if(!out["installed"].get<bool>())return out;
    const auto pin=PinHash(sql);out["owner"]=OwnerExists(sql);
    out["pin_matches"]=pin==Hex(c->ContentHash());
    // Status only reads: it uses the current session and never claims one.
    if(out["owner"].get<bool>() && out["pin_matches"].get<bool>())
    {
        BenchSession=std::stoull(Rows(sql,"SELECT session_generation FROM player.item_owner_fence WHERE owner_kind=? AND owner_id=?",
            {SqlParam::UInt(static_cast<unsigned>(BenchOwner.kind)),SqlParam::UInt(BenchOwner.id)}).at(0).at(0));
        out["state"]=State(*c,Load(sql,*c));
    }
    return out;
}
// Real characters of the local game DB, with whether they have typed items.
Json Characters(CAsyncSQL& sql)
{
    Json out=Json::array();
    try
    {
        for(const auto& row:Rows(sql,"SELECT p.id,p.name,f.owner_id IS NOT NULL FROM player.player p LEFT JOIN player.item_owner_fence f "
            "ON f.owner_kind=1 AND f.owner_id=p.id ORDER BY p.id LIMIT 2000"))
            out.push_back({{"id",std::stoull(row[0])},{"name",row[1]},{"provisioned",row[2]=="1"}});
    }
    catch(const Failure&){}
    return out;
}
Json Grant(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);
    const auto key=ItemKey::Create(static_cast<ItemCategory>(request.at("category").get<unsigned>()),request.at("local").get<std::uint32_t>());
    Require(key.has_value(),"Érvénytelen tárgykulcs.");
    const auto* entry=c->Find(*key);Require(entry,"Nincs ilyen tárgy a katalógusban.");
    auto maxId=Rows(sql,"SELECT COALESCE(MAX(instance_id),?) FROM player.item_instance WHERE instance_id>=?",{SqlParam::UInt(BenchIdBase),SqlParam::UInt(BenchIdBase)});
    PersistentItem item{ItemInstanceId::Create(std::stoull(maxId.at(0).at(0))+1).value(),*key,BenchOwner};
    item.count=request.value("count",1u);item.refinementLevel=static_cast<std::uint16_t>(request.value("level",0u));
    if(entry->base.binding==BindingPolicy::OnAcquire)item.boundTo=BenchOwner;
    if(const auto* a=std::get_if<ItemAccessoryRow>(&entry->category); a && a->socketProfile)item.accessory=AccessorySocketState{};
    if(auto valid=ValidatePersistentItem(*c,PersistentItem{item.id,item.key,item.owner,1,item.refinementLevel,item.count,item.boundTo,false,
        ItemPlacement{ItemStorageKind::Inventory,1,0},item.attributes,item.stones,item.accessory});!valid)
        throw Failure(std::string("A tárgy nem adható: ")+PersistenceText(valid.error())+".");
    const auto occupied=Occupied(*c,o);const auto cells=std::uint32_t{Inventory.columns}*Inventory.rows*Inventory.pages;
    std::optional<std::uint32_t> anchor;
    for(std::uint32_t cell=0;cell<cells && !anchor;++cell)if(Fits(*c,item,cell,occupied))anchor=cell;
    Require(anchor.has_value(),"Nincs szabad hely a próbapad inventoryjában.");
    item.placement=ItemPlacement{ItemStorageKind::Inventory,1,*anchor};
    auto r=Base(*c,o,ItemOperationKind::Grant);r.items.push_back({std::nullopt,item});
    return Commit(sql,*c,r,{{"granted",std::to_string(item.id.Value())}});
}
Json Move(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);const auto& item=Item(o,request,"item");
    const auto cell=request.at("cell").get<std::uint32_t>();
    Require(Fits(*c,item,cell,Occupied(*c,o,item.id.Value())),"A tárgy nem fér el ezen a helyen.");
    auto after=item;after.revision++;after.placement->anchor=cell;
    auto r=Base(*c,o,ItemOperationKind::Move);r.items.push_back({item,after});return Commit(sql,*c,r);
}
Json Destroy(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);const auto& item=Item(o,request,"item");
    auto after=item;after.revision++;after.destroyed=true;after.count=0;after.placement.reset();
    auto r=Base(*c,o,ItemOperationKind::Consume);r.items.push_back({item,after});return Commit(sql,*c,r);
}
Json Refine(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);const auto& item=Item(o,request,"item");
    const auto recipeId=request.at("recipe").get<std::uint32_t>();
    const auto* recipe=c->FindRefinementRecipe(recipeId);Require(recipe,"Nincs ilyen fejlesztési recept.");
    std::uint16_t roll=0; // 0% and 100% are decided without a roll
    if(recipe->successChanceBp!=10000 && recipe->successChanceBp!=0)roll=static_cast<std::uint16_t>(request.contains("roll")?request["roll"].get<unsigned>():Uniform(1,10000));
    std::vector<RefinementItemSnapshot> inventory;
    for(const auto& i:o.items)if(!i.destroyed)inventory.push_back({i.id.Value(),i.revision,i.owner.id,i.key,i.refinementLevel,i.count,false});
    const auto& wallet=o.wallets.at(0);
    auto plan=PrepareRefinement(*c,recipeId,{OperationId(),BenchOwner.id,wallet.revision,wallet.balance,true},
        {item.id.Value(),item.revision,item.owner.id,item.key,item.refinementLevel,item.count,false},inventory,roll);
    Require(plan.has_value(),std::string("A fejlesztés nem végezhető el: ")+RefinementText(plan.error())+".");
    auto commit=MakeRefinementCommit(*c,*plan,o.fence,o.storages,o.items,wallet);
    Require(commit.has_value(),std::string("A fejlesztés eredménye nem menthető: ")+PersistenceText(commit.error())+".");
    return Commit(sql,*c,*commit,{{"roll",roll},{"success",plan->success}});
}
Json Attribute(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);
    const auto& tool=Item(o,request,"tool");const auto& target=Item(o,request,"target");
    const auto* consumable=std::get_if<ItemConsumableRow>(&c->Find(tool.key)->category);
    Require(consumable && consumable->attributeServiceProfile,"A kiválasztott eszköz nem bónuszszolgáltatás.");
    const auto* service=c->FindAttributeService(*consumable->attributeServiceProfile);
    AttributeRequest base{Pair(tool,target,o.fence),target.attributes};
    base.successRoll=service->successBp==10000?0:static_cast<std::uint16_t>(Uniform(1,10000));
    const auto plan=[&](const AttributeRequest& r){return PlanAttributeService(*c,r);};
    std::optional<AttributePlan> chosen;
    if(service->operation==AttributeOperation::Lock || service->operation==AttributeOperation::Relock)
    {
        // Pick uniformly among the lock targets the planner accepts.
        std::vector<AttributePlan> options;
        for(std::uint8_t roll=1;roll<=AttributeSlots;++roll){auto r=base;r.lockRoll=roll;if(auto p=plan(r))options.push_back(*p);}
        Require(!options.empty(),"Nincs zárolható bónusz.");chosen=options[Uniform(0,static_cast<std::uint32_t>(options.size()-1))];
    }
    else if(auto direct=plan(base))chosen=*direct; // unlock, or a failed success roll
    else
    {
        std::uint32_t tierTotal=0;for(const auto& t:service->tiers)tierTotal+=t.chanceBp;
        Require(tierTotal>0,"A szolgáltatásnak nincs bónuszszintje.");
        const auto tierRoll=[&]{return static_cast<std::uint16_t>(Uniform(1,std::min<std::uint32_t>(tierTotal,10000)));};
        // The draw count and each bonus range follow from the current state:
        // ask the real planner, using bonusRoll=1 (always in range) as filler.
        std::vector<AttributeDraw> draws;std::optional<std::size_t> needed;
        for(std::size_t n=1;n<=AttributeSlots && !needed;++n)
        {
            auto r=base;r.draws.assign(n,{});for(auto& d:r.draws)d={tierRoll(),1};
            if(plan(r))needed=n,draws=r.draws;
        }
        Require(needed.has_value(),"A bónuszszolgáltatás erre a tárgyra nem alkalmazható.");
        for(std::size_t k=0;k<*needed;++k)
        {
            std::uint32_t low=1,high=1;
            const auto accepts=[&](std::uint32_t roll){auto r=base;r.draws=draws;r.draws[k].bonusRoll=roll;
                for(std::size_t j=k+1;j<draws.size();++j)r.draws[j].bonusRoll=1;return plan(r).has_value();};
            while(high<(1u<<30) && accepts(high*2))high*=2;
            std::uint32_t top=high*2;while(low<high || top>high+1){const auto mid=high+(top-high)/2;if(mid==high)break;if(accepts(mid))high=mid;else top=mid;}
            draws[k].bonusRoll=Uniform(1,high);
        }
        auto r=base;r.draws=draws;auto final=plan(r);Require(final.has_value(),"A sorsolt bónuszok nem érvényesek.");chosen=*final;
    }
    auto commit=MakeAttributeCommit(*c,*chosen,o.fence,o.storages,o.items);
    Require(commit.has_value(),std::string("A bónuszszolgáltatás eredménye nem menthető: ")+PersistenceText(commit.error())+".");
    return Commit(sql,*c,*commit,{{"success",chosen->succeeded}});
}
Json Socket(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);
    const auto& tool=Item(o,request,"tool");const auto& target=Item(o,request,"target");
    SocketServiceRequest r{Pair(tool,target,o.fence),target.stones};
    auto plan=PlanSocketService(*c,r);
    Require(plan.has_value(),"A foglalatszolgáltatás nem alkalmazható (hibakód "+std::to_string(static_cast<int>(plan.error()))+
        "). Kőkiemelés a próbapadon még nem támogatott.");
    auto commit=MakeSocketServiceCommit(*c,*plan,o.fence,o.storages,o.items);
    Require(commit.has_value(),std::string("A foglalatszolgáltatás eredménye nem menthető: ")+PersistenceText(commit.error())+".");
    return Commit(sql,*c,*commit);
}
Json Wallet(CAsyncSQL& sql,const Json& request)
{
    const auto c=Catalog(sql);const auto o=Load(sql,*c);
    const auto gold=request.at("gold").get<std::uint64_t>();
    Require(gold<=static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()),"Túl nagy összeg.");
    const auto it=std::ranges::find_if(o.wallets,[](const auto& w){return w.currency==WalletCurrency::Gold;});
    Require(it!=o.wallets.end(),"A próbapadnak nincs Yang-pénztárcája.");
    if(it->balance==gold)return {{"state",State(*c,o)}};
    auto after=*it;after.revision++;after.balance=gold;
    auto r=Base(*c,o,ItemOperationKind::Grant);r.storages.clear();r.wallets.push_back({*it,after});return Commit(sql,*c,r);
}
Json Dispatch(CAsyncSQL& sql,const std::string& action,const Json& request);
Json Run(const Json& request)
{
    const auto& auth=request.at("connection");
    CAsyncSQL sql;sql.SetTimeouts(5);sql.SetRetryConnectionErrors(false);
    const auto user=auth.at("user").get<std::string>(),password=auth.at("password").get<std::string>();
    Require(sql.Setup("127.0.0.1",user.c_str(),password.c_str(),"player","utf8mb4",true,auth.at("port").get<unsigned>()),"A helyi adatbázis nem érhető el.");
    const auto action=request.at("action").get<std::string>();
    const auto pid=request.value("owner",std::uint64_t{TestCharacter});
    Require(pid>0 && pid<=0xffffffffull,"Érvénytelen karakterazonosító.");
    BenchOwner={ItemOwnerKind::Character,pid};Inventory.storage.owner=BenchOwner;Equipment.storage.owner=BenchOwner;
    if(action=="characters")return {{"characters",Characters(sql)}};
    if(action=="status")return Status(sql);
    if(action=="setup")return Setup(sql,request);
    Require(Installed(sql) && OwnerExists(sql),"Előbb hozd létre a próbapadot (vagy építsd ki a karaktert).");
    const auto catalog=Catalog(sql);
    const bool followed=FollowCatalog(sql,*catalog);
    Claim(sql,*catalog);
    auto result=Dispatch(sql,action,request);
    result["catalog_followed"]=followed;return result;
}
Json Dispatch(CAsyncSQL& sql,const std::string& action,const Json& request)
{
    if(action=="load"){const auto c=Catalog(sql);return {{"state",State(*c,Load(sql,*c))}};}
    if(action=="wallet")return Wallet(sql,request);
    if(action=="grant")return Grant(sql,request);
    if(action=="move")return Move(sql,request);
    if(action=="destroy")return Destroy(sql,request);
    if(action=="refine")return Refine(sql,request);
    if(action=="attribute")return Attribute(sql,request);
    if(action=="socket")return Socket(sql,request);
    throw Failure("Ismeretlen művelet.");
}
void Reply(const Json& packet){std::cout<<"AURIGA_BENCH_RESULT "<<packet.dump()<<'\n';}
}
std::shared_ptr<spdlog::logger> logging::GetLogger(){static auto logger=std::make_shared<spdlog::logger>("item-bench");return logger;}
std::shared_ptr<spdlog::logger> logging::GetErrorLogger(){return logging::GetLogger();}
int main()
{
    try
    {
        std::string input;char chunk[8192];
        while(std::cin.read(chunk,sizeof(chunk)) || std::cin.gcount()){input.append(chunk,static_cast<std::size_t>(std::cin.gcount()));Require(input.size()<=1<<20,"Túl nagy kérés.");}
        Reply({{"ok",true},{"result",Run(Json::parse(input))}});return 0;
    }
    catch(const Json::exception&){Reply({{"ok",false},{"error","Hibás kérésszerkezet."}});return 1;}
    catch(const std::exception& error){Reply({{"ok",false},{"error",error.what()}});return 1;}
}
