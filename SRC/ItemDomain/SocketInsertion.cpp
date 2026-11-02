#include <item/SocketInsertion.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeSocketInsertionCommit(const ItemCatalog& c,
    std::array<std::uint8_t,16> operation,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,
    ItemInstanceId sourceId,ItemInstanceId targetId,SocketMaintenanceActor actor,std::uint16_t chance,std::uint16_t roll)
{
    using E=SocketMaintenanceError;
    if(fence.owner.kind!=ItemOwnerKind::Character || items.size()>MaxLoadedOwnerItems || chance>10000)return std::unexpected(E::Invalid);
    std::set<ItemStorage> storages;for(const auto& layout:layouts)if(layout.storage.owner!=fence.owner || !ValidItemStorage(layout) || !storages.insert(layout.storage).second)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> ids;for(const auto& item:items)if(item.owner!=fence.owner || !ids.insert(item.id).second || !ValidatePersistentItem(c,item))return std::unexpected(E::Invalid);
    const auto source=std::ranges::find(items,sourceId,&PersistentItem::id),target=std::ranges::find(items,targetId,&PersistentItem::id);
    if(source==items.end() || target==items.end() || sourceId==targetId || !PlainInventoryItem(*source,fence.owner) || !SocketMaintenanceTarget(c,*target,fence.owner))return std::unexpected(E::Invalid);
    const auto& base=c.Find(source->key)->base;
    if(actor.level<base.minimumLevel)return std::unexpected(E::Level);
    // Embedded state cannot represent arbitrary per-instance binding. Bound
    // acquire-on-owner stones are safe only in an already bound own target;
    // extraction restores their acquire policy. Never launder binding.
    if(source->boundTo && (base.binding!=BindingPolicy::OnAcquire || target->boundTo!=source->boundTo))return std::unexpected(E::Denied);
    const auto snapshot=[](const PersistentItem& item){return StoneItemSnapshot{item.key,item.id.Value(),item.revision,item.owner.id,item.refinementLevel,item.count};};
    SocketItemPair pair{{operation,fence.owner.id,fence.sessionGeneration,c.Revision(),fence.revision,actor.level,actor.characterClass,actor.sex,true,actor.inCombat,false},snapshot(*source),snapshot(*target),true,true};
    if(const auto allowed=ValidateItemOperationPair(c,pair);!allowed)return std::unexpected(allowed.error()==SocketServiceError::Forbidden || allowed.error()==SocketServiceError::RetiredItem ? E::Denied : E::Invalid);
    SocketStoneInsertRequest request{pair.source,pair.target,target->stones,fence.owner.id,c.Revision(),operation,chance,roll,actor.level,actor.inCombat,true};
    const auto chosen=PrepareSocketStoneInsert(c,request);
    if(!chosen)
    {
        switch(chosen.error())
        {
        case SocketStoneError::NoEmptySocket:return std::unexpected(E::NoSocket);
        case SocketStoneError::ConflictingGroup:return std::unexpected(E::Conflict);
        case SocketStoneError::IncompatibleTarget:return std::unexpected(E::Incompatible);
        case SocketStoneError::Forbidden:case SocketStoneError::RetiredItem:return std::unexpected(E::Denied);
        default:return std::unexpected(E::Invalid);
        }
    }
    auto outcome=request;outcome.chanceBp=10000;outcome.recordedRoll=0;const auto success=PrepareSocketStoneInsert(c,outcome);
    outcome.chanceBp=0;const auto failure=PrepareSocketStoneInsert(c,outcome);if(!success || !failure)return std::unexpected(E::Invalid);
    SocketMaintenancePlan result;result.succeeded=chosen->success;
    auto& q=result.quote;q.kind=4;q.units=1;q.available=source->count;q.before=q.after=q.failure=target->stones;q.chance=chance;q.slot=chosen->slot;
    q.after[q.slot]=success->after;q.failure[q.slot]=failure->after;
    auto commit=MakeConsumeCommit(c,operation,fence,layouts,items,sourceId,1);if(!commit)return std::unexpected(E::Invalid);result.commit=std::move(*commit);
    auto& out=result.commit;out.kind=ItemOperationKind::Service;auto after=*target;++after.revision;after.stones[chosen->slot]=chosen->after;out.items.push_back({*target,after});
    for(const auto& layout:layouts)if(layout.storage==ItemStorage{fence.owner,target->placement->kind,target->placement->storageId} && std::ranges::find(out.storages,layout)==out.storages.end())out.storages.push_back(layout);
    out.outcomeCode=chosen->success ? SocketInsertionSuccess : SocketInsertionFailure;
    out.result={'I','N','S',1,static_cast<std::uint8_t>(chance&255),static_cast<std::uint8_t>(chance>>8),static_cast<std::uint8_t>(roll&255),static_cast<std::uint8_t>(roll>>8),chosen->slot,static_cast<std::uint8_t>(chosen->success)};
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return result;
}
}
