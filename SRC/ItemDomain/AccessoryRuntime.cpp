#include <item/AccessoryRuntime.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
namespace
{
const ValidatedAccessorySocketProfile* Profile(const ItemCatalog& c,const PersistentItem& item)
{
    const auto* d=c.Find(item.key);const auto* row=d ? std::get_if<ItemAccessoryRow>(&d->category) : nullptr;
    return row && row->socketProfile ? c.FindAccessorySocket(*row->socketProfile) : nullptr;
}
SocketMaintenanceError Error(SocketServiceError e)
{
    using E=SocketMaintenanceError;
    switch(e){case SocketServiceError::Forbidden:case SocketServiceError::RetiredItem:return E::Denied;
    case SocketServiceError::NoEffect:case SocketServiceError::OperationDisabled:return E::NoEffect;
    case SocketServiceError::MaterialNotAllowed:case SocketServiceError::IncompatibleTarget:case SocketServiceError::LifetimeMismatch:return E::Incompatible;
    case SocketServiceError::InvalidLevel:return E::Level;default:return E::Invalid;}
}
}
bool AccessoryMaterial(const ItemCatalog& c,ItemKey key)
{
    for(const auto& [keyIgnored,entry]:c.Entries())if(const auto* row=std::get_if<ItemAccessoryRow>(&entry.category);row && row->socketProfile)
    {
        const auto& p=c.FindAccessorySocket(*row->socketProfile)->Data();
        for(const auto* list:{&p.unlockMaterials,&p.chargeMaterials,&p.permanentChargeMaterials})if(std::ranges::binary_search(*list,key))return true;
    }
    return false;
}
bool AccessoryTarget(const ItemCatalog& c,const PersistentItem& item,ItemOwner owner)
{
    return item.owner==owner && !item.destroyed && item.count==1 && item.placement && item.placement->kind==ItemStorageKind::Inventory &&
        (!item.boundTo || item.boundTo==owner) && item.accessory && Profile(c,item) && ValidatePersistentItem(c,item).has_value();
}
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeAccessorySocketCommit(const ItemCatalog& c,
    std::array<std::uint8_t,16> operation,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,
    ItemInstanceId sourceId,ItemInstanceId targetId,SocketMaintenanceActor actor,AccessoryServiceKind kind,std::uint16_t roll)
{
    using E=SocketMaintenanceError;
    if(fence.owner.kind!=ItemOwnerKind::Character || items.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    std::set<ItemStorage> storages;for(const auto& l:layouts)if(l.storage.owner!=fence.owner || !ValidItemStorage(l) || !storages.insert(l.storage).second)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> ids;for(const auto& i:items)if(i.owner!=fence.owner || !ids.insert(i.id).second || !ValidatePersistentItem(c,i))return std::unexpected(E::Invalid);
    const auto source=std::ranges::find(items,sourceId,&PersistentItem::id),target=std::ranges::find(items,targetId,&PersistentItem::id);
    if(source==items.end() || target==items.end() || sourceId==targetId || !PlainInventoryItem(*source,fence.owner) || !AccessoryTarget(c,*target,fence.owner))return std::unexpected(E::Invalid);
    const auto snapshot=[](const PersistentItem& i){return StoneItemSnapshot{i.key,i.id.Value(),i.revision,i.owner.id,i.refinementLevel,i.count};};
    SocketItemPair pair{{operation,fence.owner.id,fence.sessionGeneration,c.Revision(),fence.revision,actor.level,actor.characterClass,actor.sex,true,actor.inCombat,false},snapshot(*source),snapshot(*target),true,true};
    const auto planned=PlanAccessoryService(c,{pair,*target->accessory,kind,roll});if(!planned)return std::unexpected(Error(planned.error()));
    const auto& profile=*Profile(c,*target);const auto& p=profile.Data();const auto index=kind==AccessoryServiceKind::Unlock ? target->accessory->unlockedCapacity : target->accessory->activeGrade;
    const auto* current=profile.FindGrade(index);const auto chance=kind==AccessoryServiceKind::Unlock ? current->unlockChanceBp : current->chargeChanceBp;
    const auto success=kind==AccessoryServiceKind::Unlock ? PlanAccessorySocketUnlock(profile,*target->accessory,source->key,SocketAttemptOutcome::Succeeded) : PlanAccessorySocketCharge(profile,*target->accessory,source->key,SocketAttemptOutcome::Succeeded);
    if(!success)return std::unexpected(E::Invalid);
    SocketMaintenancePlan result;result.succeeded=planned->succeeded;auto& q=result.quote;
    q.kind=kind==AccessoryServiceKind::Unlock ? 5 : 6;q.units=planned->consumeUnits;q.available=source->count;q.chance=chance;q.maximum=p.maxGrade;
    q.accessoryBefore=q.accessoryFailure=*target->accessory;q.accessoryAfter=success->state;
    const auto* grade=profile.FindGrade(q.accessoryAfter.activeGrade);q.increaseBp=grade->bonusIncreaseBp;q.minimumRaw=grade->minimumRaw;q.minimumBp=grade->minimumBp;q.minimumInteger=grade->minimumInteger;
    auto commit=MakeConsumeCommit(c,operation,fence,layouts,items,sourceId,planned->consumeUnits);if(!commit)return std::unexpected(E::Invalid);
    result.commit=std::move(*commit);auto& out=result.commit;out.kind=ItemOperationKind::Service;
    auto after=*target;++after.revision;after.accessory=planned->after;out.items.push_back({*target,after});
    for(const auto& l:layouts)if(l.storage==ItemStorage{fence.owner,target->placement->kind,target->placement->storageId} && std::ranges::find(out.storages,l)==out.storages.end())out.storages.push_back(l);
    out.outcomeCode=planned->succeeded ? 0x1701 : 0x1702;
    out.result={'A','C','C',1,q.kind,static_cast<std::uint8_t>(chance&255),static_cast<std::uint8_t>(chance>>8),static_cast<std::uint8_t>(roll&255),static_cast<std::uint8_t>(roll>>8),static_cast<std::uint8_t>(planned->succeeded)};
    if(!ValidateItemCommit(c,out))return std::unexpected(E::Invalid);return result;
}
std::expected<void,ItemPersistenceError> ProjectAccessoryTime(const ItemCatalog& c,std::span<PersistentItem> items,std::uint64_t elapsed)
{
    for(auto& item:items)if(!item.destroyed && item.placement && item.placement->kind==ItemStorageKind::Equipment && item.accessory)
    {
        const auto* p=Profile(c,item);if(!p)return std::unexpected(ItemPersistenceError::InvalidSockets);
        const auto advanced=AdvanceAccessorySocketTime(*p,*item.accessory,elapsed,true);if(!advanced)return std::unexpected(ItemPersistenceError::InvalidSockets);
        item.accessory=advanced->state;
    }
    return {};
}
std::expected<void,ItemPersistenceError> SettleAccessoryTime(const ItemCatalog& c,ItemCommit& commit,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,std::uint64_t elapsed)
{
    if(!elapsed)return {};
    if(std::ranges::find(commit.owners,fence)==commit.owners.end())return std::unexpected(ItemPersistenceError::InvalidFence);
    for(const auto& item:items)if(!item.destroyed && item.owner==fence.owner && item.placement && item.placement->kind==ItemStorageKind::Equipment && item.accessory && item.accessory->activeGrade && !item.accessory->permanent)
    {
        const auto* profile=Profile(c,item);if(!profile)return std::unexpected(ItemPersistenceError::InvalidSockets);
        const auto time=AdvanceAccessorySocketTime(*profile,*item.accessory,elapsed,true);if(!time)return std::unexpected(ItemPersistenceError::InvalidSockets);
        auto change=std::ranges::find(commit.items,item.id,[](const ItemChange& x){return x.after.id;});
        if(change!=commit.items.end())
        {
            if(!change->before || *change->before!=item || change->after.accessory!=item.accessory)return std::unexpected(ItemPersistenceError::Conflict);
            change->after.accessory=time->state;
        }
        else
        {
            if(item.revision==UINT64_MAX)return std::unexpected(ItemPersistenceError::LimitExceeded);
            auto after=item;++after.revision;after.accessory=time->state;commit.items.push_back({item,after});
        }
        for(const auto& l:layouts)if(l.storage==ItemStorage{item.owner,item.placement->kind,item.placement->storageId} && std::ranges::find(commit.storages,l)==commit.storages.end())commit.storages.push_back(l);
    }
    return ValidateItemCommit(c,commit);
}
}
