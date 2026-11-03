#include <item/RecoveryRuntime.hpp>
#include <algorithm>
#include <item/AutomaticRuntime.hpp>
namespace auriga::item
{
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeRecoveryRuntime(const ItemCatalog& c,const RecoveryRuntimeView& v)
{
    using E=TypedInventoryWireError;
    if(!v.control || v.status>8 || (v.error!=255 && v.error>17 && (v.error<32 || v.error>47)) || v.pools.hpRaw<0 || v.pools.spRaw<0 || v.pools.maxHpRaw<=0 || v.pools.maxSpRaw<0 || v.pools.maxHpRaw>MaxItemStatRaw || v.pools.maxSpRaw>MaxItemStatRaw || v.pools.hpRaw>v.pools.maxHpRaw || v.pools.spRaw>v.pools.maxSpRaw || (v.source ? !v.preview : v.preview!=0))return std::unexpected(E::BadValue);
    const RecoveryProfile* profile=nullptr;
    if(v.source){const auto* e=c.Find(v.source->key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;const auto* p=r && r->recoveryProfile ? c.FindRecovery(r->recoveryProfile) : nullptr;if(!p || (p->Data().Delivery()!=RecoveryDelivery::Reserve && p->Data().Delivery()!=RecoveryDelivery::Periodic && p->Data().Delivery()!=RecoveryDelivery::Automatic))return std::unexpected(E::BadValue);profile=&p->Data();}
    const auto blob=EncodeRecoverySchedule(c,v.state);if(!blob)return std::unexpected(E::BadValue);
    std::vector<ItemGrant> extra;for(const auto& d:v.state.doses)extra.push_back({d.source,0,1});std::vector<PersistentItem> metadata;if(v.source)metadata.push_back(*v.source);
    for(const auto& item:v.items)if(item.automatic)metadata.push_back(item);
    const auto details=EncodeTypedItemDetails(c,metadata,false,extra);if(!details)return std::unexpected(details.error());
    std::uint64_t hp=0,sp=0,interval=0,duration=0,cooldown=0;std::uint16_t rate=0;
    if(profile){if(const auto* r=std::get_if<ReserveRecovery>(&profile->effect)){hp=r->hpRaw;sp=r->spRaw;interval=r->tickIntervalMs;rate=r->maxPoolPerTickBp;}if(const auto* p=std::get_if<PeriodicRecovery>(&profile->effect)){hp=p->hpPerTickRaw;sp=p->spPerTickRaw;interval=p->tickIntervalMs;duration=p->durationMs;}if(const auto* a=std::get_if<AutomaticRecovery>(&profile->effect)){(a->pool==RecoveryPool::Hp ? hp : sp)=a->capacityRaw;interval=a->tickIntervalMs;rate=a->maxPoolPerTickBp;}if(profile->cooldownProfile)cooldown=c.FindConsumableCooldown(*profile->cooldownProfile)->durationMs;}
    std::vector<std::byte> out;const auto put=[&](std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(std::byte((value>>(8*i))&255));};
    const bool extended=(profile && profile->Delivery()==RecoveryDelivery::Automatic) || std::ranges::any_of(v.items,[](const auto& i){return i.automatic.has_value();});
    put(242,1);put(0,4);put(extended ? 2 : 1,1);put(v.status,1);put(v.error,1);put(v.control,8);put(v.preview,8);put(v.source ? v.source->id.Value() : 0,8);
    for(auto raw:{v.pools.hpRaw,v.pools.maxHpRaw,v.pools.spRaw,v.pools.maxSpRaw})put(static_cast<std::uint64_t>(raw),8);
    put(profile ? static_cast<unsigned>(profile->Delivery()) : 0,1);put(profile ? profile->unitsPerUse : 0,2);put(hp,8);put(sp,8);put(interval,8);put(duration,8);put(rate,2);put(v.cooldownRemaining,8);put(cooldown,8);put(v.source ? static_cast<unsigned>(v.source->key.Category()) : 0,2);put(v.source ? v.source->key.LocalId() : 0,4);
    put(blob->size(),2);for(auto b:*blob)put(b,1);
    if(extended)
    {
        const auto* a=profile ? std::get_if<AutomaticRecovery>(&profile->effect) : nullptr;put(a ? static_cast<unsigned>(a->supply) : 0,1);put(a ? a->activeMaxPoolBonusBp : 0,2);
        const auto count=std::ranges::count_if(v.items,[](const auto& i){return !i.destroyed && i.automatic.has_value();});if(count>MaxLoadedOwnerItems)return std::unexpected(E::TooLarge);put(count,2);
        for(const auto& i:v.items)if(!i.destroyed && i.automatic)
        {
            const auto* p=AutomaticProfile(c,i.key);if(!p || !ValidatePersistentItem(c,i))return std::unexpected(E::BadValue);const auto& e=std::get<AutomaticRecovery>(p->effect);
            put(i.id.Value(),8);put(static_cast<unsigned>(i.key.Category()),2);put(i.key.LocalId(),4);put(i.automatic->remainingRaw,8);put(i.automatic->active,1);put(i.automatic->nextTickInMs,8);
            put(static_cast<unsigned>(e.pool),1);put(static_cast<unsigned>(e.supply),1);put(e.capacityRaw,8);put(e.tickIntervalMs,8);put(e.maxPoolPerTickBp,2);put(e.activeMaxPoolBonusBp,2);
        }
    }
    put(details->size(),4);out.insert(out.end(),details->begin(),details->end());for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(8*i))&255);return out;
}
}
