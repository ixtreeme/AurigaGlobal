#include <item/PlayerTrades.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
using E=PlayerTradeError;
const ItemWallet* Wallet(std::span<const ItemWallet> wallets,ItemOwner owner)
{
    const ItemWallet* found=nullptr;
    for(const auto& w:wallets)if(w.owner==owner && w.currency==WalletCurrency::Gold)
    {if(found || !w.revision || w.revision==UINT64_MAX || w.balance>INT64_MAX)return nullptr;found=&w;}
    return found;
}
}
std::expected<ItemCommit,E> MakePlayerTradeCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,
    const std::array<PlayerTradeOffer,2>& offers,std::span<const ItemStorageLayout> layouts,
    std::span<const PersistentItem> items,std::span<const ItemWallet> wallets,std::span<const ItemInstanceId> ids)
{
    if(offers[0].owner.owner==offers[1].owner.owner || items.size()>2*MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    ItemCommit out;out.operationId=op;out.catalogRevision=c.Revision();out.catalogHash=c.ContentHash();out.kind=ItemOperationKind::Trade;
    out.outcomeCode=1;out.result={'T','R','D',1};
    auto result=[&](std::uint64_t value,unsigned size){for(unsigned i=0;i<size;++i)out.result.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    std::set<ItemInstanceId> allIds,offeredIds,removed;
    for(const auto& item:items)if(!allIds.insert(item.id).second)return std::unexpected(E::Invalid);
    std::vector<PersistentItem> working(items.begin(),items.end()),incoming;
    std::size_t used=0;
    for(unsigned side=0;side<2;++side)
    {
        const auto& offer=offers[side];const auto receiver=offers[1-side].owner.owner;
        if(offer.owner.owner.kind!=ItemOwnerKind::Character || offer.items.size()>MaxPlayerTradeItems)return std::unexpected(E::Invalid);
        out.owners.push_back(offer.owner);result(offer.owner.owner.id,8);result(offer.gold,8);result(offer.items.size(),1);
        const auto* wallet=Wallet(wallets,offer.owner.owner);if(!wallet)return std::unexpected(E::Invalid);
        if(offer.gold>wallet->balance)return std::unexpected(E::NoMoney);
        const auto remainder=wallet->balance-offer.gold;
        if(offers[1-side].gold>std::uint64_t(INT64_MAX)-remainder)return std::unexpected(E::WalletFull);
        const auto balance=remainder+offers[1-side].gold;
        if(balance!=wallet->balance){auto after=*wallet;after.balance=balance;++after.revision;out.wallets.push_back({*wallet,after});}
        for(const auto& choice:offer.items)
        {
            const auto original=std::ranges::find(items,choice.id,&PersistentItem::id);
            if(original==items.end() || !offeredIds.insert(choice.id).second || original->destroyed || original->owner!=offer.owner.owner ||
               !original->placement || original->placement->kind!=ItemStorageKind::Inventory)return std::unexpected(E::Invalid);
            const auto* definition=c.Find(original->key);const auto* policy=definition ? c.FindTradePolicy(definition->base.tradePolicy) : nullptr;
            if(original->boundTo || !policy || !policy->allowTrade)return std::unexpected(E::Denied);
            const auto count=choice.count ? choice.count : original->count;
            if(!count || count>original->count)return std::unexpected(E::Invalid);
            result(choice.id.Value(),8);result(count,4);
            auto transferred=*original;transferred.owner=receiver;transferred.count=count;transferred.placement.reset();
            if(count==original->count)
            {removed.insert(original->id);++transferred.revision;out.items.push_back({*original,transferred});}
            else
            {
                if(used>=ids.size() || !allIds.insert(ids[used]).second)return std::unexpected(E::Unavailable);
                auto after=*original;after.count-=count;++after.revision;out.items.push_back({*original,after});
                const auto current=std::ranges::find(working,original->id,&PersistentItem::id);*current=after;
                transferred.id=ids[used++];transferred.revision=1;out.items.push_back({{},transferred});
            }
            incoming.push_back(std::move(transferred));
        }
    }
    if(out.items.size()>MaxItemCommitItems)return std::unexpected(E::Invalid);
    std::erase_if(working,[&](const auto& item){return removed.contains(item.id);});
    for(auto& item:incoming)
    {
        std::vector<ItemStorageLayout> bags;
        for(const auto& l:layouts)if(l.storage.owner==item.owner && l.storage.kind==ItemStorageKind::Inventory)bags.push_back(l);
        std::ranges::sort(bags,{},[](const auto& l){return l.storage.id;});
        bool placed=false;
        for(const auto& bag:bags)
        {
            std::set<std::uint32_t> occupied;
            for(const auto& carried:working)if(!carried.destroyed && carried.owner==item.owner && carried.placement && carried.placement->kind==ItemStorageKind::Inventory && carried.placement->storageId==bag.storage.id)
            {
                const auto cells=OccupiedItemCells(c,carried,bag);if(!cells)return std::unexpected(E::Invalid);
                for(auto cell:*cells)if(!occupied.insert(cell).second)return std::unexpected(E::Invalid);
            }
            const auto size=std::uint32_t{bag.columns}*bag.rows*bag.pages;
            for(std::uint32_t cell=0;cell<size && !placed;++cell)
            {
                item.placement=ItemPlacement{ItemStorageKind::Inventory,bag.storage.id,cell};
                const auto footprint=OccupiedItemCells(c,item,bag);
                placed=footprint && std::ranges::none_of(*footprint,[&](auto at){return occupied.contains(at);});
            }
            if(placed)break;
        }
        if(!placed)return std::unexpected(E::NoRoom);
        auto change=std::ranges::find(out.items,item.id,[](const auto& ch){return ch.after.id;});change->after=item;working.push_back(item);
    }
    for(const auto& change:out.items)for(const auto* item:{change.before ? &*change.before : nullptr,&change.after})
        if(item && item->placement)
        {
            const ItemStorage storage{item->owner,item->placement->kind,item->placement->storageId};
            if(std::ranges::find(out.storages,storage,&ItemStorageLayout::storage)!=out.storages.end())continue;
            const auto found=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);
            if(found==layouts.end())return std::unexpected(E::Invalid);out.storages.push_back(*found);
        }
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);
    return out;
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodePlayerTrade(const ItemCatalog& c,
    const PlayerTradeView* view,std::uint8_t side,std::uint64_t request,PlayerTradeStatus status,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items)
{
    std::vector<std::byte> bytes;
    const auto put=[&](std::uint64_t value,unsigned size){for(unsigned i=0;i<size;++i)bytes.push_back(std::byte((value>>(8*i))&255));};
    put(229,1);put(0,4);put(1,1);put(static_cast<unsigned>(status),1);put(view ? view->token : 0,8);put(request,8);put(view ? view->revision : 0,8);
    put(side,1);put(view ? static_cast<unsigned>(view->phase) : 0,1);put(view ? view->readyMask : 0,1);put(view ? view->confirmedMask : 0,1);
    if(view)
    {
        std::vector<PersistentItem> selected;
        for(const auto& actor:view->actors)
        {
            if(actor.name.size()>128)return std::unexpected(TypedInventoryWireError::TooLarge);
            put(actor.vid,4);put(actor.pid,4);put(actor.name.size(),2);for(unsigned char b:actor.name)put(b,1);put(actor.offer.gold,8);
        }
        std::vector<std::uint64_t> available;
        for(const auto& item:items)if(item.owner==view->actors[side].offer.owner.owner && !item.destroyed && !item.boundTo && item.placement && item.placement->kind==ItemStorageKind::Inventory)
        {
            const auto* d=c.Find(item.key);const auto* policy=d ? c.FindTradePolicy(d->base.tradePolicy) : nullptr;
            if(policy && policy->allowTrade)available.push_back(item.id.Value());
        }
        if(available.size()>MaxLoadedOwnerItems)return std::unexpected(TypedInventoryWireError::TooLarge);
        put(available.size(),2);for(auto id:available)put(id,8);
        for(const auto& actor:view->actors)
        {
            std::vector<PersistentItem> offered;std::vector<ItemStorageLayout> bags;
            for(const auto& choice:actor.offer.items)
            {
                const auto item=std::ranges::find(items,choice.id,&PersistentItem::id);
                if(item==items.end() || item->owner!=actor.offer.owner.owner)return std::unexpected(TypedInventoryWireError::BadValue);
                auto shown=*item;shown.count=choice.count ? choice.count : item->count;offered.push_back(shown);selected.push_back(std::move(shown));
            }
            for(const auto& layout:layouts)if(layout.storage.owner==actor.offer.owner.owner && layout.storage.kind==ItemStorageKind::Inventory)bags.push_back(layout);
            const auto inventory=EncodeTypedInventory(c,bags,{},offered);if(!inventory)return std::unexpected(inventory.error());
            put(inventory->size(),4);bytes.insert(bytes.end(),inventory->begin(),inventory->end());
        }
        const auto details=EncodeTypedItemDetails(c,selected);if(!details)return std::unexpected(details.error());
        put(details->size(),4);bytes.insert(bytes.end(),details->begin(),details->end());
    }
    if(bytes.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::TooLarge);
    for(unsigned i=0;i<4;++i)bytes[1+i]=std::byte((bytes.size()>>(8*i))&255);
    return bytes;
}
}
