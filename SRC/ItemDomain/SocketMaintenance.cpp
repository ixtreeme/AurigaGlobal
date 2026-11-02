#include <item/SocketMaintenance.hpp>
#include <algorithm>
#include <set>

namespace auriga::item
{
bool IsSocketMaintenanceRecovery(const ItemCatalog& c,const ItemCommit& commit)
{
    if(commit.kind!=ItemOperationKind::Service || commit.outcomeCode!=SocketMaintenanceOutcome || commit.owners.size()!=1 || !commit.wallets.empty() || commit.ground || commit.items.size()<3 || commit.items.size()>5)return false;
    const ItemChange *source=nullptr,*target=nullptr;std::vector<const PersistentItem*> outputs;
    const SocketServiceProfile* service=nullptr;
    for(const auto& change:commit.items)
    {
        if(!change.before){outputs.push_back(&change.after);continue;}
        const auto* entry=c.Find(change.before->key);if(!entry)return false;
        const auto* row=std::get_if<ItemConsumableRow>(&entry->category);
        if(row && row->socketServiceProfile){if(source)return false;source=&change;service=c.FindSocketService(*row->socketServiceProfile);}
        else {if(target)return false;target=&change;}
    }
    if(!source || !target || !service || service->kind==SocketServiceKind::CleanBroken || !ValidatePersistentItem(c,*source->before) || !ValidatePersistentItem(c,*target->before) || !PlainInventoryItem(*source->before,commit.owners[0].owner) || !SocketMaintenanceTarget(c,*target->before,commit.owners[0].owner))return false;
    const auto& fence=commit.owners[0];const auto snapshot=[](const PersistentItem& i){return StoneItemSnapshot{i.key,i.id.Value(),i.revision,i.owner.id,i.refinementLevel,i.count};};
    const auto& base=c.Find(source->before->key)->base;const auto* use=c.FindUsePolicy(base.usePolicy);if(!use)return false;
    std::uint8_t cls=0,sex=0;while(cls<4 && !(use->allowedClasses&(1u<<cls)))++cls;while(sex<2 && !(use->allowedSexes&(1u<<sex)))++sex;
    SocketServiceRequest request{{{commit.operationId,fence.owner.id,fence.sessionGeneration,c.Revision(),fence.revision,base.minimumLevel,cls,sex,true,false,false},snapshot(*source->before),snapshot(*target->before),true,true},target->before->stones};
    std::vector<std::uint8_t> slots;for(std::uint8_t i=0;i<EquipmentStoneSockets;++i)if(target->before->stones[i].state==StoneSocketState::Occupied)slots.push_back(i);
    if(service->kind==SocketServiceKind::ExtractLast && !slots.empty())slots={slots.back()};if(outputs.size()!=slots.size())return false;
    // Write-set order is not part of the canonical commit identity.
    auto unordered=outputs;outputs.clear();for(auto slot:slots)
    {const auto& embedded=target->before->stones[slot];const auto found=std::ranges::find_if(unordered,[&](const auto* out){return out->key==*embedded.stone && out->refinementLevel==embedded.refinementLevel;});if(found==unordered.end())return false;outputs.push_back(*found);unordered.erase(found);}
    for(std::size_t n=0;n<outputs.size();++n)request.returns.push_back({slots[n],outputs[n]->id.Value(),n+1});
    const auto plan=PlanSocketService(c,request);if(!plan)return false;
    auto expected=*source->before;if(expected.count<plan->consumeUnits)return false;expected.count-=plan->consumeUnits;++expected.revision;if(!expected.count){expected.destroyed=true;expected.placement.reset();}if(expected!=source->after)return false;
    expected=*target->before;++expected.revision;expected.stones=plan->after;if(expected!=target->after)return false;
    for(std::size_t n=0;n<outputs.size();++n)
    {
        const auto& returned=plan->returns[n];PersistentItem fresh{outputs[n]->id,returned.item,fence.owner};fresh.refinementLevel=returned.refinementLevel;fresh.placement=outputs[n]->placement;
        if(c.Find(fresh.key)->base.binding==BindingPolicy::OnAcquire)fresh.boundTo=fence.owner;
        if(!fresh.placement || fresh.placement->kind!=ItemStorageKind::Inventory || fresh!=*outputs[n])return false;
    }
    return true;
}
bool SocketMaintenanceTarget(const ItemCatalog& c,const PersistentItem& item,ItemOwner owner)
{
    if(item.owner!=owner || item.destroyed || !item.placement || item.placement->kind!=ItemStorageKind::Inventory || (item.boundTo && item.boundTo!=owner))return false;
    const auto* entry=c.Find(item.key);if(!entry || entry->base.retired)return false;
    const auto* armor=std::get_if<ItemArmorRow>(&entry->category);
    return std::holds_alternative<ItemWeaponRow>(entry->category) || (armor && armor->slot==EquipmentSlot::Body);
}
std::expected<SocketMaintenancePlan,SocketMaintenanceError> MakeSocketMaintenanceCommit(const ItemCatalog& c,
    std::array<std::uint8_t,16> op,ItemOwnerFence fence,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,
    ItemInstanceId sourceId,ItemInstanceId targetId,SocketMaintenanceActor actor,std::span<const ItemInstanceId> outputs)
{
    using E=SocketMaintenanceError;
    if(fence.owner.kind!=ItemOwnerKind::Character || items.size()>MaxLoadedOwnerItems)return std::unexpected(E::Invalid);
    std::set<ItemStorage> storages;for(const auto& layout:layouts)if(layout.storage.owner!=fence.owner || !ValidItemStorage(layout) || !storages.insert(layout.storage).second)return std::unexpected(E::Invalid);
    std::set<ItemInstanceId> ids;for(const auto& i:items)if(i.owner!=fence.owner || !ids.insert(i.id).second || !ValidatePersistentItem(c,i))return std::unexpected(E::Invalid);
    const auto source=std::ranges::find(items,sourceId,&PersistentItem::id),target=std::ranges::find(items,targetId,&PersistentItem::id);
    if(source==items.end() || target==items.end() || sourceId==targetId || !PlainInventoryItem(*source,fence.owner) || !SocketMaintenanceTarget(c,*target,fence.owner))return std::unexpected(E::Invalid);
    const auto* entry=c.Find(source->key);const auto* consumable=std::get_if<ItemConsumableRow>(&entry->category);
    const auto* profile=consumable && consumable->socketServiceProfile ? c.FindSocketService(*consumable->socketServiceProfile) : nullptr;
    if(!profile)return std::unexpected(E::Invalid);
    if(actor.level<entry->base.minimumLevel)return std::unexpected(E::Level);
    const auto snapshot=[](const PersistentItem& i){return StoneItemSnapshot{i.key,i.id.Value(),i.revision,i.owner.id,i.refinementLevel,i.count};};
    SocketItemPair pair{{op,fence.owner.id,fence.sessionGeneration,c.Revision(),fence.revision,actor.level,actor.characterClass,actor.sex,true,actor.inCombat,false},snapshot(*source),snapshot(*target),true,true};
    SocketServiceRequest request{pair,target->stones};
    std::vector<std::uint8_t> slots;
    if(profile->kind!=SocketServiceKind::CleanBroken)
    {
        for(std::uint8_t i=0;i<EquipmentStoneSockets;++i)if(target->stones[i].state==StoneSocketState::Occupied)slots.push_back(i);
        if(profile->kind==SocketServiceKind::ExtractLast && !slots.empty())slots={slots.back()};
        if(outputs.size()<slots.size())return std::unexpected(E::Busy);
        for(std::size_t i=0;i<slots.size();++i)request.returns.push_back({slots[i],outputs[i].Value(),i+1});
    }
    const auto plan=PlanSocketService(c,request);
    if(!plan)
    {
        if(plan.error()==SocketServiceError::NoEffect)return std::unexpected(E::NoEffect);
        if(plan.error()==SocketServiceError::Forbidden || plan.error()==SocketServiceError::RetiredItem)return std::unexpected(E::Denied);
        return std::unexpected(E::Invalid);
    }
    auto consumed=MakeConsumeCommit(c,op,fence,layouts,items,sourceId,plan->consumeUnits);if(!consumed)return std::unexpected(E::Invalid);
    SocketMaintenancePlan result;auto& commit=result.commit;commit=std::move(*consumed);commit.kind=ItemOperationKind::Service;
    auto next=*target;++next.revision;next.stones=plan->after;commit.items.push_back({*target,next});
    result.quote={static_cast<std::uint8_t>(profile->kind),plan->consumeUnits,source->count,target->stones,plan->after,{}};
    // Reserve complete output footprints after consumption. No merging,
    // partial return, ground fallback, or rewriting an embedded stone's level.
    std::vector<PersistentItem> working(items.begin(),items.end());
    for(const auto& change:commit.items)*std::ranges::find(working,change.after.id,&PersistentItem::id)=change.after;
    for(const auto& returned:plan->returns)
    {
        const auto id=ItemInstanceId::Create(returned.instanceId);if(!id || !ids.insert(*id).second)return std::unexpected(E::Invalid);
        PersistentItem fresh{*id,returned.item,fence.owner};fresh.refinementLevel=returned.refinementLevel;
        const auto* stone=c.Find(fresh.key);if(stone->base.binding==BindingPolicy::OnAcquire)fresh.boundTo=fence.owner;
        bool placed=false;
        for(const auto& layout:layouts)
        {
            if(layout.storage.owner!=fence.owner || layout.storage.kind!=ItemStorageKind::Inventory)continue;
            std::set<std::uint32_t> occupied;
            for(const auto& i:working)if(!i.destroyed && i.placement && i.placement->kind==ItemStorageKind::Inventory && i.placement->storageId==layout.storage.id)
            {const auto cells=OccupiedItemCells(c,i,layout);if(!cells)return std::unexpected(E::Invalid);for(auto cell:*cells)if(!occupied.insert(cell).second)return std::unexpected(E::Invalid);}
            for(std::uint32_t cell=0;cell<std::uint32_t(layout.columns)*layout.rows*layout.pages && !placed;++cell)
            {fresh.placement=ItemPlacement{ItemStorageKind::Inventory,layout.storage.id,cell};const auto cells=OccupiedItemCells(c,fresh,layout);if(cells && std::ranges::none_of(*cells,[&](auto at){return occupied.contains(at);}))placed=true;}
            if(placed){if(std::ranges::find(commit.storages,layout)==commit.storages.end())commit.storages.push_back(layout);break;}
        }
        if(!placed)return std::unexpected(E::NoRoom);
        working.push_back(fresh);commit.items.push_back({{},fresh});result.quote.returns.push_back({returned.socket,returned.item,returned.refinementLevel});
    }
    // Both target and tool storage must be protected even for cleaning.
    for(const auto& layout:layouts)if(layout.storage==ItemStorage{fence.owner,target->placement->kind,target->placement->storageId} && std::ranges::find(commit.storages,layout)==commit.storages.end())commit.storages.push_back(layout);
    commit.outcomeCode=SocketMaintenanceOutcome;commit.result={'S','O','C',1,result.quote.kind};
    for(const auto& slot:result.quote.after){commit.result.push_back(static_cast<std::uint8_t>(slot.state));commit.result.push_back(slot.grade);}
    if(!ValidateItemCommit(c,commit))return std::unexpected(E::Invalid);return result;
}
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeSocketMaintenance(const ItemCatalog& c,
    std::uint64_t nonce,const PersistentItem* source,const PersistentItem* target,std::uint64_t revision,SocketMaintenanceStatus status,
    const SocketMaintenanceQuote& q,std::span<const std::uint64_t> targets)
{
    if(static_cast<unsigned>(status)>16 || targets.size()>MaxLoadedOwnerItems || q.returns.size()>EquipmentStoneSockets || q.chance>10000 ||
       (source ? (!nonce || !revision) : (nonce || target || revision || q!=SocketMaintenanceQuote{} || !targets.empty())) ||
       ((status==SocketMaintenanceStatus::Ready || status==SocketMaintenanceStatus::Applied || status==SocketMaintenanceStatus::Broken) && (!source || !target || q.kind<1 || q.kind>6 || !q.units || q.units>q.available || (q.kind==4 && q.slot>=EquipmentStoneSockets) || (status==SocketMaintenanceStatus::Broken && q.kind<4))))return std::unexpected(TypedInventoryWireError::BadValue);
    std::vector<PersistentItem> metadata;if(source)metadata.push_back(*source);if(target)metadata.push_back(*target);
    const auto details=source ? EncodeTypedItemDetails(c,metadata,false) : std::expected<std::vector<std::byte>,TypedInventoryWireError>(std::vector<std::byte>{});if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((v>>(i*8))&255));};
    const auto key=[&](const PersistentItem* item){put(item ? static_cast<unsigned>(item->key.Category()) : 0,2);put(item ? item->key.LocalId() : 0,4);put(item ? item->refinementLevel : 0,2);};
    const auto sockets=[&](const auto& values){for(const auto& s:values){put(static_cast<unsigned>(s.state),1);put(s.grade,1);put(s.stone ? static_cast<unsigned>(s.stone->Category()) : 0,2);put(s.stone ? s.stone->LocalId() : 0,4);put(s.refinementLevel,2);}};
    put(235,1);put(0,4);put(q.kind>=5 ? 3 : 2,1);put(static_cast<unsigned>(status),1);put(nonce,8);put(source ? source->id.Value() : 0,8);put(target ? target->id.Value() : 0,8);put(revision,8);
    put(q.kind,1);put(q.units,2);put(q.available,4);key(source);key(target);sockets(q.before);sockets(q.after);
    put(q.returns.size(),1);for(const auto& r:q.returns){put(r.socket,1);put(static_cast<unsigned>(r.key.Category()),2);put(r.key.LocalId(),4);put(r.level,2);}
    put(q.chance,2);put(q.slot,1);sockets(q.failure);
    if(q.kind>=5)
    {
        const auto state=[&](const AccessorySocketState& s){put(s.unlockedCapacity,1);put(s.activeGrade,1);put(s.remainingMs,8);put(s.permanent,1);};
        state(q.accessoryBefore);state(q.accessoryAfter);state(q.accessoryFailure);put(q.maximum,1);put(q.increaseBp,4);put(q.minimumRaw,8);put(q.minimumBp,4);put(q.minimumInteger,4);
    }
    put(targets.size(),2);for(auto id:targets)put(id,8);put(details->size(),4);out.insert(out.end(),details->begin(),details->end());
    if(out.size()>(1u<<20))return std::unexpected(TypedInventoryWireError::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(i*8))&255);return out;
}
}
