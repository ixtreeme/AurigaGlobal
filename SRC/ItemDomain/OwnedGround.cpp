#include <item/OwnedGround.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
using E=ItemPersistenceError;
const PersistentItem* Source(std::span<const PersistentItem> items,ItemOwner owner,ItemInstanceId id)
{
    const auto it=std::ranges::find(items,id,&PersistentItem::id);
    return it==items.end() || it->destroyed || it->owner!=owner || !it->placement ? nullptr : &*it;
}

std::expected<ItemCommit,E> Checked(const ItemCatalog& c,std::array<std::uint8_t,16> op,
    std::vector<ItemOwnerFence> owners,std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> items,std::vector<ItemChange> changes,ItemOperationKind kind)
{
    if(items.size()>owners.size()*MaxLoadedOwnerItems)return std::unexpected(E::LimitExceeded);
    ItemCommit r;r.operationId=op;r.catalogRevision=c.Revision();r.catalogHash=c.ContentHash();
    r.kind=kind;r.owners=std::move(owners);r.items=std::move(changes);r.outcomeCode=1;r.result={static_cast<std::uint8_t>(kind)};
    for(const auto& ch:r.items)
        for(const auto* i:{ch.before ? &*ch.before : nullptr,&ch.after})
        {
            if(!i || !i->placement)continue;
            const ItemStorage storage{i->owner,i->placement->kind,i->placement->storageId};
            if(std::ranges::find(r.storages,storage,&ItemStorageLayout::storage)!=r.storages.end())continue;
            const auto found=std::ranges::find(storages,storage,&ItemStorageLayout::storage);
            if(found==storages.end() || std::ranges::count(storages,storage,&ItemStorageLayout::storage)!=1)
                return std::unexpected(E::InvalidStorage);
            r.storages.push_back(*found);
        }
    if(auto valid=ValidateItemCommit(c,r);!valid)return std::unexpected(valid.error());
    // Validate touched grids after every change, including unchanged items.
    std::set<std::pair<ItemStorage,std::uint32_t>> occupied;
    const auto reserve=[&](const PersistentItem& i)->std::expected<void,E>{
        if(i.destroyed || !i.placement)return {};
        const ItemStorage storage{i.owner,i.placement->kind,i.placement->storageId};
        const auto layout=std::ranges::find(r.storages,storage,&ItemStorageLayout::storage);
        if(layout==r.storages.end())return {};
        const auto cells=OccupiedItemCells(c,i,*layout);if(!cells)return std::unexpected(cells.error());
        for(const auto cell:*cells)if(!occupied.emplace(storage,cell).second)return std::unexpected(E::InvalidPlacement);
        return {};
    };
    std::set<ItemInstanceId> ids;
    for(const auto& i:items)
    {
        if(!ids.insert(i.id).second)return std::unexpected(E::InvalidChange);
        const auto changed=std::ranges::find(r.items,i.id,[](const ItemChange& ch){return ch.after.id;});
        if(auto valid=reserve(changed==r.items.end() ? i : changed->after);!valid)return std::unexpected(valid.error());
    }
    for(const auto& ch:r.items)if(!ch.before)
    {
        if(!ids.insert(ch.after.id).second)return std::unexpected(E::InvalidChange);
        if(auto valid=reserve(ch.after);!valid)return std::unexpected(valid.error());
    }
    return r;
}
}

std::expected<ItemCommit,E> MakeGroundDropCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,
    ItemInstanceId id,ItemPlacement escrow,std::uint32_t units,std::optional<ItemInstanceId> splitId)
{
    const auto* source=Source(items,owner.owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(owner.owner.kind!=ItemOwnerKind::Character || source->placement->kind!=ItemStorageKind::Inventory ||
       escrow.kind!=ItemStorageKind::TradeEscrow)return std::unexpected(E::UnsupportedState);
    const auto* entry=c.Find(source->key);if(!entry)return std::unexpected(E::MissingItem);
    const auto* policy=c.FindTradePolicy(entry->base.tradePolicy);
    // Legacy drop rejects both DROP and GIVE anti-flags.
    if(source->boundTo || !policy || !policy->allowDrop || !policy->allowTrade)return std::unexpected(E::TransferDenied);
    const auto count=units ? units : source->count;
    if(!count || count>source->count)return std::unexpected(E::InvalidChange);
    auto dropped=*source;dropped.placement=escrow;
    std::vector<ItemChange> changes;
    if(count==source->count)
    {
        if(splitId)return std::unexpected(E::InvalidChange);
        dropped.revision++;changes.push_back({*source,dropped});
    }
    else
    {
        if(!splitId || std::ranges::find(items,*splitId,&PersistentItem::id)!=items.end())return std::unexpected(E::InvalidChange);
        auto remainder=*source;remainder.revision++;remainder.count-=count;
        dropped.id=*splitId;dropped.revision=1;dropped.count=count;
        changes={{*source,remainder},{std::nullopt,dropped}};
    }
    return Checked(c,op,{owner},storages,items,std::move(changes),ItemOperationKind::Move);
}

std::expected<ItemCommit,E> MakeGroundPickupCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,
    ItemOwnerFence sourceOwner,ItemOwnerFence receiver,std::span<const ItemStorageLayout> storages,
    std::span<const PersistentItem> items,ItemInstanceId id,ItemPlacement target)
{
    const auto* source=Source(items,sourceOwner.owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(sourceOwner.owner.kind!=ItemOwnerKind::Character || receiver.owner.kind!=ItemOwnerKind::Character ||
       source->placement->kind!=ItemStorageKind::TradeEscrow || target.kind!=ItemStorageKind::Inventory)
        return std::unexpected(E::UnsupportedState);
    if(source->boundTo && source->boundTo!=receiver.owner)return std::unexpected(E::TransferDenied);
    const auto* entry=c.Find(source->key);if(!entry)return std::unexpected(E::MissingItem);
    const auto* policy=c.FindTradePolicy(entry->base.tradePolicy);
    if(sourceOwner.owner!=receiver.owner && (!policy || !policy->allowTrade))return std::unexpected(E::TransferDenied);
    auto picked=*source;picked.owner=receiver.owner;picked.placement=target;picked.revision++;
    std::vector<ItemOwnerFence> owners{sourceOwner};
    if(sourceOwner.owner!=receiver.owner)owners.push_back(receiver);
    else if(sourceOwner!=receiver)return std::unexpected(E::InvalidFence);
    return Checked(c,op,std::move(owners),storages,items,{{*source,picked}},ItemOperationKind::Trade);
}

std::expected<ItemCommit,E> MakeGroundExpireCommit(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence owner,
    std::span<const ItemStorageLayout> storages,std::span<const PersistentItem> items,ItemInstanceId id)
{
    const auto* source=Source(items,owner.owner,id);if(!source)return std::unexpected(E::MissingItem);
    if(source->placement->kind!=ItemStorageKind::TradeEscrow)return std::unexpected(E::UnsupportedState);
    auto expired=*source;expired.revision++;expired.count=0;expired.destroyed=true;expired.placement.reset();
    return Checked(c,op,{owner},storages,items,{{*source,expired}},ItemOperationKind::Consume);
}
} // namespace auriga::item
