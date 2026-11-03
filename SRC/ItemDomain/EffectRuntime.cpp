#include <item/EffectRuntime.hpp>
#include <algorithm>
#include <set>
namespace auriga::item
{
const TimedEffectProfile* TimedItemProfile(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;return r && r->timedEffectProfile ? c.FindTimedEffect(*r->timedEffectProfile) : nullptr;}
const ToggleEffectProfile* ToggleItemProfile(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;return r && r->toggleEffectProfile ? c.FindToggleEffect(*r->toggleEffectProfile) : nullptr;}
std::uint64_t TimedItemRemaining(const TimedItemEffect& effect,std::uint64_t now)
{return effect.expiresAtMs ? effect.expiresAtMs-std::min(effect.expiresAtMs,now) : effect.remainingMs;}
void NormalizeTimedItems(const ItemCatalog& c,RecoverySchedule& s,std::uint64_t now,const ItemEffectFacts* facts)
{
    std::erase_if(s.effects,[&](const auto& e){const auto* p=TimedItemProfile(c,e.source);if(!p || !TimedItemRemaining(e,now) || c.Find(e.source)->base.retired)return true;const auto* g=c.FindTimedEffectGroup(p->group);return facts && ((!facts->alive && g->removeOnDeath) || !facts->allowed || !(p->allowedContexts&static_cast<unsigned>(facts->actor.context)) || (facts->actor.inPvp && !p->allowDuringPvp));});
}
bool HasDeathItemEffects(const ItemCatalog& c,const RecoverySchedule& state,std::span<const PersistentItem> items)
{
    for(const auto& e:state.effects)if(const auto* p=TimedItemProfile(c,e.source);p && c.FindTimedEffectGroup(p->group)->removeOnDeath)return true;
    for(const auto& item:items)if(item.toggle && item.toggle->active)if(const auto* p=ToggleItemProfile(c,item.key);p && c.FindToggleEffectGroup(p->group)->removeOnDeath)return true;
    return false;
}
namespace
{
ToggleEffectAdmission Admission(const PersistentItem& item,const ItemEffectFacts& f,std::uint64_t cooldown=0)
{ToggleEffectAdmission a;a.state=*item.toggle;a.availableUnits=item.count;a.context=f.actor.context;a.alive=f.alive;a.inPvp=f.actor.inPvp;a.inCombat=f.actor.inCombat;a.married=f.married;a.partyLeader=f.partyLeader;a.effectAllowedByRules=f.allowed;a.level=f.actor.level;a.classIndex=f.actor.classIndex;a.sexIndex=f.actor.sexIndex;a.cooldownRemainingMs=cooldown;return a;}
std::expected<void,ItemPersistenceError> Delta(ItemCommit& commit,std::span<const ItemStorageLayout> layouts,const PersistentItem& before,PersistentItem after)
{
    if(before==after)return {};if(before.revision==UINT64_MAX || !before.placement)return std::unexpected(ItemPersistenceError::InvalidItem);
    after.revision=before.revision+1;const ItemStorage storage{before.owner,before.placement->kind,before.placement->storageId};const auto l=std::ranges::find(layouts,storage,&ItemStorageLayout::storage);if(l==layouts.end())return std::unexpected(ItemPersistenceError::InvalidStorage);
    if(std::ranges::find(commit.storages,storage,&ItemStorageLayout::storage)==commit.storages.end())commit.storages.push_back(*l);
    commit.items.push_back({before,std::move(after)});return {};
}
ItemCommit Base(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence)
{ItemCommit commit;commit.operationId=op;commit.catalogRevision=c.Revision();commit.catalogHash=c.ContentHash();commit.kind=ItemOperationKind::Service;commit.owners={fence};commit.outcomeCode=0x2101;commit.result={'E','F','T',1};return commit;}
}
std::expected<std::vector<PersistentItem>,ItemPersistenceError> ProjectToggleItems(const ItemCatalog& c,std::span<const PersistentItem> items,std::uint64_t elapsed,const ItemEffectFacts& facts)
{
    std::vector<PersistentItem> out(items.begin(),items.end());std::set<std::uint32_t> groups;
    for(auto& item:out)if(!item.destroyed && item.toggle && item.toggle->active)
    {
        const auto* p=ToggleItemProfile(c,item.key);if(!p || !groups.insert(p->group).second || groups.size()>MaxRuntimeToggleEffects)return std::unexpected(ItemPersistenceError::InvalidItem);
        const auto plan=PlanToggleEffectAdvance(c,item.key,item.id,Admission(item,facts),elapsed);if(!plan)return std::unexpected(ItemPersistenceError::InvalidItem);
        item.toggle=plan->next;if(plan->destroyItem){item.destroyed=true;item.count=0;item.placement.reset();}
    }
    return out;
}
std::expected<void,ItemPersistenceError> SettleToggleTime(const ItemCatalog& c,ItemCommit& commit,std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,std::uint64_t elapsed,const ItemEffectFacts& facts)
{
    const auto projected=ProjectToggleItems(c,items,elapsed,facts);if(!projected)return std::unexpected(projected.error());
    for(std::size_t i=0;i<items.size();++i)if(items[i].toggle && std::ranges::none_of(commit.items,[&](const auto& change){return change.after.id==items[i].id;}))
        if(auto added=Delta(commit,layouts,items[i],(*projected)[i]);!added)return added;
    return {};
}
std::expected<std::vector<BonusEntry>,ItemPersistenceError> CollectItemEffectBonuses(const ItemCatalog& c,const RecoverySchedule& schedule,std::span<const PersistentItem> items)
{
    if(!ValidateRecoverySchedule(c,schedule))return std::unexpected(ItemPersistenceError::InvalidStoredData);std::vector<BonusEntry> out;
    for(const auto& effect:schedule.effects){const auto* p=TimedItemProfile(c,effect.source);if(!p)return std::unexpected(ItemPersistenceError::InvalidStoredData);const auto& entries=c.FindBonus(p->bonusProfile)->Data().entries;out.insert(out.end(),entries.begin(),entries.end());}
    std::set<std::uint32_t> groups;
    for(const auto& item:items)if(!item.destroyed && item.toggle && item.toggle->active)
    {const auto* p=ToggleItemProfile(c,item.key);if(!p || !ValidatePersistentItem(c,item) || !groups.insert(p->group).second)return std::unexpected(ItemPersistenceError::InvalidStoredData);const auto& entries=c.FindBonus(p->bonusProfile)->Data().entries;out.insert(out.end(),entries.begin(),entries.end());}
    return out;
}
std::expected<ItemCommit,ItemEffectError> MakeItemEffectUse(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,const RecoverySchedule& before,ItemInstanceId id,const ItemEffectFacts& facts,std::uint64_t elapsed,std::uint64_t now,std::uint64_t effectId)
{
    auto state=AdvanceRecoverySchedule(c,before,elapsed);if(!state || before.revision==UINT64_MAX || before.generation==UINT64_MAX)return std::unexpected(ItemEffectError{4});NormalizeTimedItems(c,*state,now,&facts);
    const auto found=std::ranges::find(items,id,&PersistentItem::id);if(found==items.end() || found->owner!=fence.owner || (!PlainInventoryItem(*found,fence.owner) && !(found->toggle && found->toggle->active)))return std::unexpected(ItemEffectError{0});
    auto projected=ProjectToggleItems(c,items,elapsed,facts);if(!projected)return std::unexpected(ItemEffectError{4});auto target=std::ranges::find(*projected,id,&PersistentItem::id);
    ItemCommit commit=Base(c,op,fence);std::optional<ConsumableCooldownStart> cooldown;
    if(const auto* p=TimedItemProfile(c,found->key))
    {
        if(!facts.allowed)return std::unexpected(ItemEffectError{6});
        std::vector<ActiveTimedEffect> active;for(const auto& e:state->effects)active.push_back({e.id,TimedItemProfile(c,e.source)->group,std::min(TimedItemRemaining(e,now),TimedItemProfile(c,e.source)->durationMs)});
        TimedEffectAdmission actor{found->count,p->cooldownProfile ? RecoveryCooldownRemaining(*state,*p->cooldownProfile) : 0,facts.actor.context,facts.alive,facts.actor.inPvp,facts.actor.inCombat,facts.actor.level,facts.actor.classIndex,facts.actor.sexIndex};
        const auto plan=PlanTimedEffectUse(c,found->key,actor,active);if(!plan)return std::unexpected(ItemEffectError{static_cast<std::uint8_t>(plan.error())});
        if(!effectId || (plan->clock==TimedEffectClock::WallTime && (now>INT64_MAX || plan->durationMs>INT64_MAX-now)))return std::unexpected(ItemEffectError{4});
        if(plan->replaceEffectId)std::erase_if(state->effects,[&](const auto& e){return e.id==*plan->replaceEffectId;});
        if(state->effects.size()>=MaxRuntimeTimedEffects)return std::unexpected(ItemEffectError{15});
        state->effects.push_back({effectId,found->key,plan->clock==TimedEffectClock::OnlineTime ? plan->durationMs : 0,plan->clock==TimedEffectClock::WallTime ? now+plan->durationMs : 0});
        const auto consume=MakeConsumeCommit(c,op,fence,layouts,items,id,plan->unitsToConsume);if(!consume)return std::unexpected(ItemEffectError{4});commit=*consume;commit.outcomeCode=0x2101;cooldown=plan->cooldown;
    }
    else if(const auto* p=ToggleItemProfile(c,found->key))
    {
        if(!target->toggle || target->destroyed)return std::unexpected(ItemEffectError{47});std::vector<ActiveToggleEffect> active;
        for(const auto& item:*projected)if(!item.destroyed && item.toggle && item.toggle->active)active.push_back({item.id,item.key,item.toggle->remainingMs});
        const auto plan=PlanToggleEffectUse(c,found->key,id,Admission(*target,facts,p->cooldownProfile ? RecoveryCooldownRemaining(*state,*p->cooldownProfile) : 0),active);
        if(!plan)return std::unexpected(ItemEffectError{static_cast<std::uint8_t>(32+static_cast<unsigned>(plan.error()))});
        if(plan->next.active && !target->toggle->active && !plan->deactivateOther && active.size()>=MaxRuntimeToggleEffects)return std::unexpected(ItemEffectError{49});
        target->toggle=plan->next;cooldown=plan->cooldown;
        if(plan->deactivateOther){auto old=std::ranges::find(*projected,*plan->deactivateOther,&PersistentItem::id);if(old==projected->end() || !old->toggle)return std::unexpected(ItemEffectError{35});old->toggle->active=false;}
    }
    else return std::unexpected(ItemEffectError{2});
    for(std::size_t i=0;i<items.size();++i)if(items[i].toggle)if(auto added=Delta(commit,layouts,items[i],(*projected)[i]);!added)return std::unexpected(ItemEffectError{4});
    if(cooldown && !SetRecoveryCooldown(c,*state,cooldown->profileId,cooldown->durationMs))return std::unexpected(ItemEffectError{4});
    state->revision=before.revision+1;state->generation=before.generation+1;commit.recovery=ItemRecoveryChange{fence.owner,before,*state,elapsed};
    if(!ValidateItemCommit(c,commit))return std::unexpected(ItemEffectError{4});return commit;
}
std::expected<ItemCommit,ItemEffectError> MakeItemEffectCancel(const ItemCatalog& c,std::array<std::uint8_t,16> op,ItemOwnerFence fence,const RecoverySchedule& before,std::uint64_t elapsed,std::uint64_t now,std::uint64_t generation,std::uint64_t id)
{
    if(before.generation!=generation || before.generation==UINT64_MAX || before.revision==UINT64_MAX)return std::unexpected(ItemEffectError{4});auto state=AdvanceRecoverySchedule(c,before,elapsed);if(!state)return std::unexpected(ItemEffectError{4});NormalizeTimedItems(c,*state,now);
    if(id){if(!std::erase_if(state->effects,[&](const auto& e){return e.id==id;}))return std::unexpected(ItemEffectError{0});}else state->effects.clear();
    ++state->generation;++state->revision;auto commit=Base(c,op,fence);commit.outcomeCode=0x2102;commit.recovery=ItemRecoveryChange{fence.owner,before,*state,elapsed};if(!ValidateItemCommit(c,commit))return std::unexpected(ItemEffectError{4});return commit;
}
}
