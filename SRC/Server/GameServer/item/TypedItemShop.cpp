#include "stdafx.h"
#include "TypedItemShop.hpp"
#include "TypedInventorySync.hpp"
#include "TypedDrops.hpp"
#include "TypedRecovery.hpp"
#include "ItemStore.hpp"
#include "ItemCatalogRuntime.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../entity/char_manager.h"
#include "../network/desc.h"

#include <Headers/Header_GC.h>
#include <common/DragonCoinWallet.h>
#include <item/ItemShopReceipt.hpp>
#include <item/PersistentItems.hpp>
#include <item/TypedItemDetails.hpp>
#include <StoragePersistenceSql.hpp>
#include <ItemPersistenceSql.hpp>
#include <openssl/rand.h>
#include <algorithm>

#ifdef ENABLE_ITEMSHOP
namespace
{
namespace item = auriga::item;
namespace store = item::runtime;
namespace sql = item::persistence;
using Error = item::ItemPersistenceError;
constexpr std::uint32_t HistoryPageSize = 100;

struct Product
{
    std::uint32_t id = 0, price = 0, legacy = 0;
    std::uint64_t added = 0, offer = 0, sales = 0;
    std::uint32_t week = 0, month = 0;
    std::uint8_t category = 0, subcategory = 0, discount = 0;
    bool stackable = false;
    std::string title;
    std::vector<item::ItemGrant> members;
};

struct Bytes
{
    std::vector<std::uint8_t> data;
    void Put(std::uint64_t value, unsigned size)
    {
        for (unsigned i = 0; i < size; ++i) data.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }
    void Text(std::string_view text)
    {
        Put(text.size(), 2);
        data.insert(data.end(), text.begin(), text.end());
    }
    void Members(const std::vector<item::ItemGrant>& members)
    {
        Put(members.size(), 1);
        for (const auto& member : members)
        {
            Put(static_cast<unsigned>(member.key.Category()), 2);
            Put(member.key.LocalId(), 4);
            Put(member.refinementLevel, 2);
            Put(member.count, 4);
        }
    }
};

struct Session
{
    std::uint32_t pid = 0, account = 0;
    std::uint64_t token = 0;
    entt::entity Current() const
    {
        const auto e = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
        if (!g_registry.valid(e)) return entt::null;
        const auto* login = g_registry.try_get<ecs::TypedItemLogin>(e);
        auto* desc = ecs::PlayerRuntime::GetDesc(e);
        return login && login->token == token && desc && desc->IsPhase(PHASE_GAME) &&
            desc->GetAccountTable().id == account ? e : entt::null;
    }
};

std::optional<Session> Identify(entt::entity e)
{
    if (!g_registry.valid(e)) return {};
    const auto* login = g_registry.try_get<ecs::TypedItemLogin>(e);
    auto* desc = ecs::PlayerRuntime::GetDesc(e);
    if (!login || login->mode != ecs::TypedItemLogin::Mode::Native || !desc || !desc->IsPhase(PHASE_GAME) ||
        !desc->GetAccountTable().id || !g_registry.all_of<ecs::TypedItems>(e)) return {};
    return Session{ecs::PlayerRuntime::GetPlayerID(e), desc->GetAccountTable().id, login->token};
}

void Tell(entt::entity e, const char* message)
{
    ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "%s", message);
}

std::uint64_t Number(const char* value)
{
    return value && *value && *value != '-' ? std::strtoull(value, nullptr, 10) : 0;
}

bool AddMember(Product& product, const item::ItemCatalog& catalog, MYSQL_ROW row, int at)
{
    const auto key = item::ItemKey::Create(static_cast<std::uint16_t>(Number(row[at])), static_cast<std::uint32_t>(Number(row[at+1])));
    const auto level = static_cast<std::uint16_t>(Number(row[at+2]));
    const auto quantity = static_cast<std::uint32_t>(Number(row[at+3]));
    const auto* entry = key ? catalog.Find(*key) : nullptr;
    if (!entry || entry->base.retired || !quantity || quantity > 128 ||
        !catalog.RefinedBaseStats(*key, level) || !item::SupportsPersistentGrant(catalog, *entry)) return false;
    if (const auto* rule = catalog.FindDefinitionRules(*key, level); rule && (!rule->available || !rule->allowAcquire)) return false;
    product.members.push_back({*key, level, quantity});
    product.stackable = entry->base.maxStack > 1;
    return true;
}

std::uint64_t Quote(const Product& product)
{
    Bytes value;
    value.Put(product.id, 4);
    value.Put(product.price, 4);
    value.Put(product.discount, 1);
    value.Text(product.title);
    value.Members(product.members);
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto byte : value.data) { hash ^= byte; hash *= 1099511628211ull; }
    return hash;
}

std::optional<std::vector<Product>> ReadProducts(CAsyncSQL& db, const item::ItemCatalog& catalog, std::uint32_t id)
{
    std::unique_ptr<SQLMsg> message(db.DirectQueryPrepared(
        "SELECT d.id,d.categoryType,d.categorySubType,d.itemPrice,d.discount,UNIX_TIMESTAMP(d.offerTime),"
        "UNIX_TIMESTAMP(d.addedTime),d.sellCount+COALESCE((SELECT SUM(h.units) FROM player.ishop_native_purchase h WHERE h.shop_id=d.id),0),"
        "d.week_limit,d.month_limit,n.legacy_vnum,p.title,m.slot,"
        "IF(p.shop_id IS NULL,n.category_id,m.category_id),IF(p.shop_id IS NULL,n.local_id,m.local_id),"
        "IF(p.shop_id IS NULL,n.refinement_level,m.refinement_level),IF(p.shop_id IS NULL,1,m.quantity) "
        "FROM player.ishop_data d "
        "LEFT JOIN player.ishop_native_item n ON n.shop_id=d.id AND n.legacy_vnum=d.itemVnum "
        "LEFT JOIN player.ishop_native_package p ON p.shop_id=d.id "
        "LEFT JOIN player.ishop_native_package_member m ON m.shop_id=p.shop_id "
        "WHERE (?=0 OR d.id=?) AND (n.shop_id IS NOT NULL OR p.shop_id IS NOT NULL) ORDER BY d.id,m.slot LIMIT 12289",
        {SqlParam::UInt(id), SqlParam::UInt(id)}));
    if (!message || message->uiSQLErrno || !message->Get() || message->Get()->uiNumRows > 12288) return {};
    std::vector<Product> result;
    Product current;
    bool valid = false;
    const auto finish = [&] {
        if (current.id && valid && current.members.size() == (current.title.empty() ? 1u : 6u))
            result.push_back(std::move(current));
    };
    while (MYSQL_ROW row = message->Get()->FetchRow())
    {
        const auto shop = static_cast<std::uint32_t>(Number(row[0]));
        if (current.id != shop)
        {
            finish();
            current = {};
            current.id = shop;
            current.category = static_cast<std::uint8_t>(Number(row[1]));
            current.subcategory = static_cast<std::uint8_t>(Number(row[2]));
            current.price = static_cast<std::uint32_t>(Number(row[3]));
            current.discount = static_cast<std::uint8_t>(std::min<std::uint64_t>(100, Number(row[4])));
            current.offer = Number(row[5]);
            if (current.offer == 1) current.offer = 0;
            current.added = Number(row[6]);
            current.sales = Number(row[7]);
            current.week = static_cast<std::uint32_t>(Number(row[8]));
            current.month = static_cast<std::uint32_t>(Number(row[9]));
            current.legacy = row[11] ? 0 : static_cast<std::uint32_t>(Number(row[10]));
            current.title = row[11] ? row[11] : "";
            valid = shop && (!row[11] || (!current.title.empty() && current.title.size() <= 512));
        }
        if (valid)
            valid = (current.title.empty() ? current.members.empty() : Number(row[12]) == current.members.size()+1) &&
                AddMember(current, catalog, row, 13);
    }
    finish();
    return result;
}

Bytes Frame(unsigned kind, std::uint64_t balance)
{
    Bytes p;
    p.Put(HEADER_GC_TYPED_ITEM_SHOP, 1);
    p.Put(0, 4);
    p.Put(1, 1);
    p.Put(kind, 1);
    p.Put(balance, 8);
    return p;
}

void Send(entt::entity e, Bytes packet)
{
    if (!g_registry.valid(e) || packet.data.size() > (1u << 20)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(e);
    if (!desc) return;
    for (unsigned i = 0; i < 4; ++i) packet.data[1+i] = static_cast<std::uint8_t>(packet.data.size() >> (i*8));
    if (packet.data.size() > 65535) desc->LargePacket(packet.data.data(), static_cast<int>(packet.data.size()));
    else desc->Packet(packet.data.data(), static_cast<int>(packet.data.size()));
}

bool Details(Bytes& packet, const item::ItemCatalog& c, const std::vector<item::ItemGrant>& grants)
{
    const auto data = item::EncodeTypedItemDetails(c, {}, false, grants);
    if (!data) return false;
    packet.Put(data->size(), 4);
    for (auto byte : *data) packet.Put(std::to_integer<unsigned>(byte), 1);
    return packet.data.size() <= (1u << 20);
}

std::uint64_t Bought(CAsyncSQL& db, const Session& session, const Product& product, bool month)
{
    const std::string interval = month ? "MONTH" : "WEEK";
    const std::string query =
        "SELECT COALESCE((SELECT SUM(units) FROM player.ishop_native_purchase WHERE account_id=? AND shop_id=? AND bought_at>DATE_SUB(NOW(),INTERVAL 1 "
        + interval + ")),0)+COALESCE((SELECT SUM(itemCount) FROM player.ishop_log WHERE accountID=? AND itemVnum=? AND ?>0 AND buyDate>DATE_SUB(NOW(),INTERVAL 1 "
        + interval + ")),0)";
    std::unique_ptr<SQLMsg> message(db.DirectQueryPrepared(query.c_str(),
        {SqlParam::UInt(session.account),SqlParam::UInt(product.id),SqlParam::UInt(session.account),
            SqlParam::UInt(product.legacy),SqlParam::UInt(product.legacy)}));
    if (!message || message->uiSQLErrno || !message->Get()) return UINT64_MAX;
    const auto row = message->Get()->FetchRow();
    return row ? Number(row[0]) : UINT64_MAX;
}

std::expected<item::ItemCommit, Error> PlanPurchase(const item::ItemCatalog& catalog, const sql::LoadedItemOwner& character,
    const Product& product, unsigned count, std::uint64_t issuedAt, std::span<const item::ItemInstanceId> ids)
{
    item::ItemCommit out;
    if (RAND_bytes(out.operationId.data(), static_cast<int>(out.operationId.size())) != 1)
        return std::unexpected(Error::InvalidOperation);
    out.catalogRevision = catalog.Revision();
    out.catalogHash = catalog.ContentHash();
    out.kind = item::ItemOperationKind::Grant;
    out.outcomeCode = item::ItemShopPurchaseOutcome;
    out.owners = {character.fence};
    auto working = character.items;
    std::size_t used = 0;
    for (auto grant : product.members)
    {
        grant.count *= count;
        grant.createdAtUtcMs = issuedAt;
        const auto part = item::MakeGrantCommit(catalog, out.operationId, character.fence, character.storages, working, grant, ids.subspan(used));
        if (!part) return std::unexpected(part.error());
        for (const auto& layout : part->storages)
            if (std::ranges::find(out.storages, layout.storage, &item::ItemStorageLayout::storage) == out.storages.end())
                out.storages.push_back(layout);
        for (const auto& change : part->items)
        {
            if (change.before) *std::ranges::find(working, change.after.id, &item::PersistentItem::id) = change.after;
            else { working.push_back(change.after); ++used; }
        }
    }
    for (std::size_t i = 0; i < working.size(); ++i)
        if (i >= character.items.size())
        {
            working[i].revision = 1;
            out.items.push_back({{}, working[i]});
        }
        else if (working[i] != character.items[i])
        {
            working[i].revision = character.items[i].revision + 1;
            out.items.push_back({character.items[i], working[i]});
        }
    return out;
}

struct LogRow
{
    std::string date, title;
    std::uint64_t price = 0;
    std::uint32_t count = 0;
    std::vector<item::ItemGrant> members;
};

void ReadPurchasePayload(LogRow& log, std::string_view bytes, const item::ItemCatalog& catalog)
{
    if (bytes.size() < item::ItemShopReceiptPrefix + 3 || bytes.size() > item::MaxItemShopReceipt) return;
    std::size_t at = item::ItemShopReceiptPrefix;
    const auto read = [&](unsigned size) {
        std::uint64_t value = 0;
        for (unsigned i = 0; i < size; ++i) value |= std::uint64_t(static_cast<unsigned char>(bytes[at++])) << (i * 8);
        return value;
    };
    const auto title = read(2);
    if (title > 512 || title > bytes.size() - at - 1) return;
    log.title = bytes.substr(at, title);
    at += title;
    const auto count = read(1);
    if (count > 6 || bytes.size() - at != count * 12) return;
    for (unsigned i = 0; i < count; ++i)
    {
        const auto category = static_cast<std::uint16_t>(read(2));
        const auto local = static_cast<std::uint32_t>(read(4));
        const auto key = item::ItemKey::Create(category, local);
        const auto level = static_cast<std::uint16_t>(read(2));
        const auto units = static_cast<std::uint32_t>(read(4));
        if (key && units && catalog.RefinedBaseStats(*key, level)) log.members.push_back({*key,level,units});
    }
}
}

void SendTypedItemShopCatalog(entt::entity character)
{
    const auto session = Identify(character);
    if (!session) return;
    store::ItemStore::Instance().Submit([s=*session](CAsyncSQL& db, const item::ItemCatalog& catalog) -> store::ItemStore::Completion {
        bool ok = false;
        const auto balance = auriga::dragon_coin::SweepAndRead(db, s.account, ok);
        const auto products = ReadProducts(db, catalog, 0);
        if (!ok || !products) return [s] { if (const auto e=s.Current(); e!=entt::null) Tell(e, "A SÉ-s bolt most nem érhető el."); };
        auto p = Frame(0, balance);
        p.Put(products->size(), 2);
        std::vector<const Product*> ranked;
        for (const auto& product : *products) if (product.sales) ranked.push_back(&product);
        std::ranges::sort(ranked, [](auto* a, auto* b) {return a->sales != b->sales ? a->sales > b->sales : a->id < b->id;});
        std::vector<item::ItemGrant> grants;
        for (const auto& product : *products)
        {
            p.Put(product.id,4); p.Put(product.category,1); p.Put(product.subcategory,1); p.Put(product.price,4);
            p.Put(product.discount,1); p.Put(product.offer,8); p.Put(product.added,8);
            const auto rank = std::ranges::find(ranked, &product);
            p.Put(rank == ranked.end() ? 255 : std::min<std::ptrdiff_t>(rank - ranked.begin(), 255),1);
            p.Put(product.stackable || !product.title.empty(),1);
            p.Put(Quote(product),8);
            p.Text(product.title); p.Members(product.members);
            grants.insert(grants.end(), product.members.begin(), product.members.end());
        }
        if (!Details(p, catalog, grants)) return [s] { if (const auto e=s.Current(); e!=entt::null) Tell(e,"A bolt kínálata most nem tölthető be."); };
        return [s, p=std::move(p)] { Send(s.Current(),p); };
    });
}

void SendTypedItemShopHistory(entt::entity character, std::uint32_t offset)
{
    const auto session = Identify(character);
    if (!session) return;
    store::ItemStore::Instance().Submit([s=*session,offset](CAsyncSQL& db, const item::ItemCatalog& catalog) -> store::ItemStore::Completion {
        std::unique_ptr<SQLMsg> message(db.DirectQueryPrepared(
            "SELECT bought_at,units,paid,purchase_payload,0,0,0 FROM player.ishop_native_purchase WHERE account_id=? "
            "UNION ALL "
            "SELECT l.buyDate,l.itemCount,l.itemPrice,NULL,n.category_id,n.local_id,n.refinement_level FROM player.ishop_log l "
            "LEFT JOIN (SELECT legacy_vnum,MIN(category_id) AS category_id,MIN(local_id) AS local_id,MIN(refinement_level) AS refinement_level "
            "FROM player.ishop_native_item GROUP BY legacy_vnum HAVING COUNT(DISTINCT category_id,local_id,refinement_level)=1) n ON n.legacy_vnum=l.itemVnum "
            "WHERE l.accountID=? AND l.itemCount>0 ORDER BY 1 DESC,2,3,4,5,6,7 LIMIT ? OFFSET ?",
            {SqlParam::UInt(s.account),SqlParam::UInt(s.account),SqlParam::UInt(HistoryPageSize+1),SqlParam::UInt(offset)}));
        if (!message || message->uiSQLErrno || !message->Get()) return [s] {
            if (const auto e=s.Current(); e!=entt::null) Tell(e,"A vásárlási napló most nem olvasható."); };
        auto p = Frame(1, 0);
        const auto rows = static_cast<std::uint32_t>(message->Get()->uiNumRows);
        const auto count = std::min(HistoryPageSize, rows);
        p.Put(offset,4);
        p.Put(rows > count,1);
        p.Put(count,2);
        std::vector<item::ItemGrant> grants;
        for (unsigned i = 0; i < count; ++i)
        {
            const auto row = message->Get()->FetchRow();
            if (!row) return [] {};
            LogRow log;
            log.date = row[0] ? row[0] : "";
            log.count = static_cast<std::uint32_t>(Number(row[1]));
            log.price = Number(row[2]);
            if (row[3])
            {
                const auto* lengths = message->Get()->FetchLengths();
                if (lengths) ReadPurchasePayload(log, std::string_view(row[3], lengths[3]), catalog);
            }
            else if (const auto key = item::ItemKey::Create(static_cast<std::uint16_t>(Number(row[4])),static_cast<std::uint32_t>(Number(row[5]))))
            {
                const auto level = static_cast<std::uint16_t>(Number(row[6]));
                if (catalog.RefinedBaseStats(*key,level)) log.members.push_back({*key,level,1});
            }
            p.Text(log.date); p.Put(log.count,4); p.Put(log.price,8); p.Text(log.title); p.Members(log.members);
            grants.insert(grants.end(),log.members.begin(),log.members.end());
        }
        if (!Details(p, catalog, grants)) return [s] {
            if (const auto e=s.Current(); e!=entt::null) Tell(e,"A vásárlási napló most nem tölthető be."); };
        return [s,p=std::move(p)] { Send(s.Current(),p); };
    });
}

bool BeginTypedItemShopBuy(entt::entity character, std::uint32_t shopId, int count, std::uint64_t quote)
{
    const auto session = Identify(character);
    if (!session || !shopId || count < 1 || count > 20) return false;
    if (g_registry.all_of<ecs::TypedItemBusy>(character))
    {
        Tell(character,"Az előző tárgyművelet még folyamatban van.");
        return true;
    }
    g_registry.emplace<ecs::TypedItemBusy>(character);
    const bool queued = store::ItemStore::Instance().Submit([s=*session,shopId,count,quote](CAsyncSQL& db,const item::ItemCatalog& catalog) -> store::ItemStore::Completion {
        auto products = ReadProducts(db,catalog,shopId);
        bool ok = false;
        auriga::dragon_coin::SweepAndRead(db,s.account,ok);
        const item::ItemOwner owner{item::ItemOwnerKind::Account,s.account};
        auto account = sql::EnsureRuntimeStorageOwner(db,catalog,owner)
            ? sql::LoadCurrentItemOwner(db,catalog,owner) : std::expected<sql::LoadedItemOwner,Error>(std::unexpected(Error::SqlFailure));
        bool allowed = products && products->size()==1;
        if (allowed)
        {
            const auto& p = products->front();
            const auto fits = [&](std::uint32_t limit, bool month) {
                return !limit || (limit >= static_cast<unsigned>(count) && Bought(db,s,p,month) <= limit-static_cast<unsigned>(count)); };
            allowed = fits(p.week,false) && fits(p.month,true);
        }
        return [s,count,quote,ok,products=std::move(products),account=std::move(account),allowed]() {
            const auto e=s.Current();
            if (e==entt::null) return;
            const auto fail = [&](const char* reason) {
                g_registry.remove<ecs::TypedItemBusy>(e);
                Tell(e,reason);
                PumpTypedItemGrants(e);
            };
            const auto catalog = store::CurrentCatalog();
            auto* typed = g_registry.try_get<ecs::TypedItems>(e);
            if (!catalog || !typed || !products || products->size()!=1)
                return fail("Ez a tárgybolti ajánlat most nem érhető el.");
            if (Quote(products->front()) != quote)
            {
                fail("Az ajánlat megváltozott. A bolt frissült.");
                SendTypedItemShopCatalog(e);
                return;
            }
            if (!allowed) return fail("Elérted az ajánlat heti vagy havi vásárlási korlátját.");
            if (!ok || !account) return fail("A SÉ-egyenleg most nem olvasható.");
            const auto& product = products->front();
            const auto wallet = std::ranges::find(account->wallets,item::WalletCurrency::DragonCoin,&item::ItemWallet::currency);
            const auto price = std::uint64_t(product.price)*count*(100-product.discount)/100;
            if (wallet==account->wallets.end() || wallet->balance<price) return fail("Nincs elég Sárkányérméd.");
            std::size_t maximum = 0;
            for (const auto& member : product.members)
            {
                const auto* entry = catalog->Find(member.key);
                if (!entry || !entry->base.maxStack) return fail("A tárgy most nem adható ki.");
                if (!product.stackable && product.title.empty() && count > 1)
                    return fail("Ebből az ajánlatból egyszerre egyet vásárolhatsz.");
                maximum += (std::size_t(member.count)*count + entry->base.maxStack - 1) / entry->base.maxStack;
            }
            if (maximum>item::MaxItemCommitItems) return fail("Válassz kisebb darabszámot.");
            const auto issuedAt = TypedItemEffectNow();
            auto commit = PlanPurchase(*catalog, typed->state, product, count, issuedAt, store::PeekInstanceIds(maximum));
            if (!commit) return fail("A csomag vagy tárgy nem helyezhető a leltárba.");
            commit->owners.push_back(account->fence);
            // Account fencing also serializes free offers and their purchase limits.
            auto after = *wallet;
            after.balance -= price;
            ++after.revision;
            commit->wallets.push_back({*wallet, after});
            Bytes receipt;
            receipt.Put(s.account,4); receipt.Put(product.id,4); receipt.Put(count,2); receipt.Put(price,8); receipt.Put(issuedAt,8);
            receipt.Text(product.title); receipt.Members(product.members);
            commit->result=std::move(receipt.data);
            if (!item::ValidateItemCommit(*catalog,*commit)) return fail("A tétel most nem vásárolható meg.");
            store::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(commit->items,[](const auto& c){return !c.before;})));
            std::vector<store::OwnerRead> owners{{typed->state.fence.owner,typed->state.fence.sessionGeneration},{account->fence.owner,0}};
            if (!store::RequestCommitMany(std::move(owners),std::move(*commit),[s,balance=wallet->balance-price](store::MultiCommitOutcome result) {
                const auto e=s.Current();
                if (e==entt::null) return;
                g_registry.remove<ecs::TypedItemBusy>(e);
                if (!result.states.empty() && result.states[0])
                    g_registry.emplace_or_replace<ecs::TypedItems>(e,std::move(*result.states[0]));
                if (result.write)
                {
                    Send(e,Frame(3,balance));
                    Tell(e,"Sikeres vásárlás.");
                }
                else
                {
                    Tell(e,"A vásárlást nem sikerült menteni. Frissítsd a boltot és próbáld újra.");
                    SendTypedItemShopCatalog(e);
                }
                SendTypedInventory(e);
                PumpTypedItemGrants(e);
            })) fail("A tárgytároló most nem érhető el.");
        };
    });
    if (!queued)
    {
        g_registry.remove<ecs::TypedItemBusy>(character);
        Tell(character,"A tárgytároló most nem érhető el.");
    }
    return true;
}
#else
bool BeginTypedItemShopBuy(entt::entity, std::uint32_t, int, std::uint64_t) { return false; }
void SendTypedItemShopCatalog(entt::entity) {}
void SendTypedItemShopHistory(entt::entity, std::uint32_t) {}
#endif
