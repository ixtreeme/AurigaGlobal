#include <item/AutomaticRuntime.hpp>
#include <algorithm>
namespace auriga::item
{
const RecoveryProfile* AutomaticProfile(const ItemCatalog& c,ItemKey key)
{
    const auto* e=c.Find(key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;
    const auto* p=r && r->recoveryProfile ? c.FindRecovery(r->recoveryProfile) : nullptr;
    return p && p->Data().Delivery()==RecoveryDelivery::Automatic ? &p->Data() : nullptr;
}
AutomaticRecoveryAdmission AutomaticActor(const ItemCatalog& c,std::span<const PersistentItem> all,const PersistentItem& item,
    const InstantRecoveryAdmission& a,bool stunned,bool allowed)
{
    AutomaticRecoveryAdmission result;result.pools=a.pools;result.state=item.automatic.value_or(AutomaticRecoveryState{});
    result.availableUnits=item.count;result.context=a.context;result.inPvp=a.inPvp;result.inCombat=a.inCombat;result.stunned=stunned;
    result.level=a.level;result.classIndex=a.classIndex;result.sexIndex=a.sexIndex;result.cooldownRemainingMs=a.cooldownRemainingMs;result.recoveryAllowedByRules=allowed;
    const auto* p=AutomaticProfile(c,item.key);if(!p)return result;const auto pool=std::get<AutomaticRecovery>(p->effect).pool;
    for(const auto& other:all)if(!other.destroyed && other.automatic && other.automatic->active)
        if(const auto* profile=AutomaticProfile(c,other.key);profile && std::get<AutomaticRecovery>(profile->effect).pool==pool)result.activeForPool=other.id;
    return result;
}
std::expected<void,ItemPersistenceError> ChangeAutomatic(ItemCommit& commit,std::span<const ItemStorageLayout> layouts,const PersistentItem& before,AutomaticRecoveryState state,bool destroy)
{
    if(before.revision==UINT64_MAX || !before.automatic || !before.placement)return std::unexpected(ItemPersistenceError::InvalidItem);
    auto after=before;after.revision++;after.automatic=state;if(destroy){after.count=0;after.destroyed=true;after.placement.reset();}
    const auto existing=std::ranges::find_if(commit.items,[&](const auto& change){return change.after.id==before.id;});
    if(existing!=commit.items.end())return std::unexpected(ItemPersistenceError::InvalidChange);
    const ItemStorage storage{before.owner,before.placement->kind,before.placement->storageId};const auto layout=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);
    if(layout==layouts.end())return std::unexpected(ItemPersistenceError::InvalidStorage);
    if(std::ranges::find(commit.storages,storage,&ItemStorageLayout::storage)==commit.storages.end())commit.storages.push_back(*layout);
    commit.items.push_back({before,after});return {};
}
std::expected<void,ItemPersistenceError> SettleAutomaticTime(ItemCommit& commit,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> all,std::uint64_t elapsed)
{
    if(!elapsed)return {};
    for(const auto& item:all)if(!item.destroyed && item.automatic && item.automatic->active && std::ranges::none_of(commit.items,[&](const auto& change){return change.after.id==item.id;}))
    {
        auto next=*item.automatic;next.nextTickInMs-=std::min(next.nextTickInMs-1,elapsed);
        if(next!=*item.automatic)if(auto changed=ChangeAutomatic(commit,layouts,item,next);!changed)return changed;
    }
    return {};
}
std::expected<void,ItemPersistenceError> TickAutomaticOwner(const ItemCatalog& c,ItemCommit& commit,std::span<const ItemStorageLayout> layouts,
    std::span<const PersistentItem> all,const InstantRecoveryAdmission& actor,RecoverySchedulePlan& recovery,std::uint64_t elapsed,bool paused,bool allowed)
{
    auto current=actor;current.pools.hpRaw+=recovery.hpRestoredRaw;current.pools.spRaw+=recovery.spRestoredRaw;
    std::array<std::int64_t,2> pending{recovery.after.creditRaw[0],recovery.after.creditRaw[1]};
    for(const auto& d:recovery.after.doses)if(d.delivery==RecoveryDelivery::Reserve){auto& n=pending[static_cast<unsigned>(d.pool)-1];n+=std::min(MaxItemStatRaw-n,d.remainingRaw);}
    for(const auto& item:all)if(!item.destroyed && item.automatic && item.automatic->active)
    {
        const auto a=AutomaticActor(c,all,item,current,paused,allowed);
        const auto plan=PlanAutomaticRecoveryTick(c,item.key,item.id,{a,pending[0],pending[1],elapsed});
        if(!plan)return std::unexpected(ItemPersistenceError::InvalidItem);
        if(plan->next!=*item.automatic || plan->destroyItem)if(auto changed=ChangeAutomatic(commit,layouts,item,plan->next,plan->destroyItem);!changed)return changed;
        for(unsigned i=0;i<2;++i)
        {
            const auto raw=i==0 ? plan->hpRestoredRaw : plan->spRestoredRaw;
            const auto total=raw+recovery.after.creditRaw[i];recovery.after.creditRaw[i]=static_cast<std::uint16_t>(total%1000);
            (i==0 ? recovery.hpRestoredRaw : recovery.spRestoredRaw)+=total-total%1000;
        }
    }
    return {};
}
}
