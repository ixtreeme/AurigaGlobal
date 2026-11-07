#include <item/NpcShops.hpp>
#include <item/ItemWireCodec.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
using E=NpcShopError;
const ItemWallet* Gold(std::span<const ItemWallet> wallets,ItemOwner owner)
{
    const ItemWallet* found=nullptr;
    for(const auto& w:wallets)if(w.owner==owner && w.currency==WalletCurrency::Gold)
    { if(found || !w.revision || w.revision==UINT64_MAX || w.balance>INT64_MAX)return nullptr; found=&w; }
    return found;
}
void Result(ItemCommit& commit,bool buy,std::uint64_t amount,std::uint32_t units)
{
    commit.kind=ItemOperationKind::Trade;
    commit.result={ 'N','P','C',static_cast<std::uint8_t>(buy ? 'B' : 'S') };
    for(unsigned i=0;i<8;++i)commit.result.push_back(static_cast<std::uint8_t>(amount>>(8*i)));
    for(unsigned i=0;i<4;++i)commit.result.push_back(static_cast<std::uint8_t>(units>>(8*i)));
}
}
std::expected<void,E> ValidateNpcShops(const ItemCatalog& c,std::span<const NpcShop> shops)
{
    if(shops.size()>MaxNpcShops)return std::unexpected(E::Invalid);
    std::set<std::uint32_t> races;
    for(const auto& shop:shops)
    {
        if(!shop.npcRace || !races.insert(shop.npcRace).second || shop.name.empty() || shop.name.size()>128 ||
           std::ranges::any_of(shop.name,[](unsigned char c){return c<32 || c==127;}) ||
           shop.offers.empty() || shop.offers.size()>MaxNpcShopOffers)return std::unexpected(E::Invalid);
        std::set<std::uint32_t> ids;
        for(const auto& offer:shop.offers)
        {
            const auto* entry=c.Find(offer.grant.key);
            if(!offer.id || !ids.insert(offer.id).second || !entry || entry->base.retired || !entry->base.buyAmount ||
               !SupportsPersistentGrant(c,*entry) || !c.RefinedBaseStats(offer.grant.key,offer.grant.refinementLevel) ||
               !offer.grant.count || offer.grant.count>entry->base.maxStack ||
               entry->base.buyAmount>std::uint64_t(INT64_MAX)/offer.grant.count)return std::unexpected(E::Invalid);
        }
    }
    return {};
}
std::expected<ItemCommit,E> MakeNpcBuyCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,
    const NpcShopOffer& offer,std::uint32_t quantity,std::span<const ItemInstanceId> ids,std::uint64_t nowUtcMs)
{
    const auto* entry=c.Find(offer.grant.key);const auto* wallet=Gold(wallets,fence.owner);
    const auto units=std::uint64_t{offer.grant.count}*quantity;
    if(!quantity || quantity>MaxShopQuantity || !offer.grant.count || !units || units>UINT32_MAX || !wallet || !entry ||
       entry->base.retired || !entry->base.buyAmount || entry->base.buyAmount>std::uint64_t(INT64_MAX)/units)return std::unexpected(E::Invalid);
    const auto cost=units*entry->base.buyAmount;
    if(wallet->balance<cost)return std::unexpected(E::NoMoney);
    auto grant=offer.grant;grant.count=static_cast<std::uint32_t>(units);grant.createdAtUtcMs=nowUtcMs;
    auto commit=MakeGrantCommit(c,op,fence,storages,items,grant,ids);
    if(!commit)return std::unexpected(commit.error()==ItemPersistenceError::InvalidPlacement ? E::NoRoom : E::Unavailable);
    auto after=*wallet;after.balance-=cost;++after.revision;
    commit->wallets.push_back({*wallet,after});Result(*commit,true,cost,grant.count);
    if(!ValidateItemCommit(c,*commit))return std::unexpected(E::Invalid);
    return std::move(*commit);
}
std::uint64_t NpcSellUnitPrice(const ItemCatalog& c,const PersistentItem& item)
{
    const auto* entry=c.Find(item.key);
    const auto* policy=entry ? c.FindTradePolicy(entry->base.tradePolicy) : nullptr;
    return !item.destroyed && item.placement && item.placement->kind==ItemStorageKind::Inventory && policy && policy->allowSell
        ? entry->base.sellAmount : 0;
}
std::expected<ItemCommit,E> MakeNpcSellCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,
    ItemInstanceId id,std::uint32_t count)
{
    const auto found=std::ranges::find(items,id,&PersistentItem::id);const auto* wallet=Gold(wallets,fence.owner);
    if(found==items.end() || found->owner!=fence.owner || !wallet)return std::unexpected(E::Invalid);
    const auto price=NpcSellUnitPrice(c,*found);if(!price)return std::unexpected(E::Denied);
    const auto units=count ? count : found->count;
    if(!units || units>found->count)return std::unexpected(E::Invalid);
    if(price>std::uint64_t(INT64_MAX)/units || units*price>std::uint64_t(INT64_MAX)-wallet->balance)return std::unexpected(E::WalletFull);
    auto commit=MakeConsumeCommit(c,op,fence,storages,items,id,units);if(!commit)return std::unexpected(E::Invalid);
    auto after=*wallet;after.balance+=units*price;++after.revision;
    commit->wallets.push_back({*wallet,after});Result(*commit,false,units*price,units);
    if(!ValidateItemCommit(c,*commit))return std::unexpected(E::Invalid);
    return std::move(*commit);
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeNpcShop(const ItemCatalog& c,const NpcShop* shop,
    std::uint32_t npc,std::uint64_t session,std::uint64_t request,NpcShopStatus status,std::span<const PersistentItem> items)
{
    std::vector<std::byte> bytes;
    const auto put=[&](std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)bytes.push_back(std::byte((value>>(8*i))&255));};
    put(228,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(npc,4);put(session,8);put(request,8);
    const std::string_view name=shop ? std::string_view(shop->name) : std::string_view{};put(name.size(),2);for(unsigned char b:name)put(b,1);
    std::vector<ItemGrant> extra;
    put(shop ? shop->offers.size() : 0,2);
    if(shop)for(const auto& offer:shop->offers)
    {
        const auto* entry=c.Find(offer.grant.key);if(!entry)return std::unexpected(TypedInventoryWireError::MissingCatalogItem);
        put(offer.id,4);for(auto b:EncodeItemKey(offer.grant.key))put(std::to_integer<unsigned char>(b),1);
        put(offer.grant.refinementLevel,2);put(offer.grant.count,4);put(entry->base.buyAmount,8);extra.push_back(offer.grant);
    }
    std::vector<std::pair<std::uint64_t,std::uint64_t>> sales;
    if(shop)for(const auto& item:items)if(const auto price=NpcSellUnitPrice(c,item))sales.emplace_back(item.id.Value(),price);
    if(sales.size()>65535)return std::unexpected(TypedInventoryWireError::TooLarge);
    put(sales.size(),2);for(const auto& [id,price]:sales){put(id,8);put(price,8);}
    auto details=shop ? EncodeTypedItemDetails(c,items,false,extra) : std::expected<std::vector<std::byte>,TypedInventoryWireError>{std::vector<std::byte>{}};
    if(!details)return std::unexpected(details.error());
    put(details->size(),4);bytes.insert(bytes.end(),details->begin(),details->end());
    if(bytes.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::TooLarge);
    for(unsigned i=0;i<4;++i)bytes[1+i]=std::byte((bytes.size()>>(8*i))&255);
    return bytes;
}
} // namespace auriga::item
