#include <item/EffectRuntime.hpp>
#include <item/TypedItemDetails.hpp>
#include <algorithm>
namespace auriga::item
{
std::expected<std::vector<std::byte>,TypedInventoryWireError> EncodeItemEffectsRuntime(const ItemCatalog& c,const ItemEffectsRuntimeView& v)
{
    using E=TypedInventoryWireError;
    if(!v.schedule || !ValidateRecoverySchedule(c,*v.schedule) || !v.control || v.status>8 || (v.error!=255 && v.error>15 && (v.error<32 || v.error>49)) || (v.source ? !v.preview : v.preview!=0) || v.now>INT64_MAX)return std::unexpected(E::BadValue);
    const auto* timed=v.source ? TimedItemProfile(c,v.source->key) : nullptr;const auto* toggle=v.source ? ToggleItemProfile(c,v.source->key) : nullptr;if(v.source && !timed && !toggle)return std::unexpected(E::BadValue);
    std::vector<const PersistentItem*> toggles;std::vector<PersistentItem> metadata;std::vector<ItemGrant> extra;
    if(v.source)metadata.push_back(*v.source);
    for(const auto& e:v.schedule->effects)extra.push_back({e.source,0,1});
    for(const auto& item:v.items)if(!item.destroyed && item.toggle && (item.toggle->active || (v.source && item.id==v.source->id))){toggles.push_back(&item);metadata.push_back(item);}
    if(toggles.size()>MaxRuntimeToggleEffects+1)return std::unexpected(E::TooLarge);
    const auto details=EncodeTypedItemDetails(c,metadata,false,extra);if(!details)return std::unexpected(details.error());
    std::vector<std::byte> out;const auto put=[&](std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)out.push_back(std::byte((n>>(8*i))&255));};
    const auto key=[&](ItemKey k){put(static_cast<unsigned>(k.Category()),2);put(k.LocalId(),4);};
    const auto bonuses=[&](std::uint32_t profile){const auto* p=c.FindBonus(profile);put(p ? p->Data().entries.size() : 0,1);if(p)for(const auto& b:p->Data().entries){put(static_cast<unsigned>(b.bonus),4);put(static_cast<std::uint64_t>(b.valueScaled),8);}};
    const auto* group=timed ? c.FindTimedEffectGroup(timed->group) : nullptr;const auto* toggleGroup=toggle ? c.FindToggleEffectGroup(toggle->group) : nullptr;
    const PersistentItem* instance=nullptr;if(v.source){const auto found=std::ranges::find(v.items,v.source->id,&PersistentItem::id);if(found!=v.items.end() && !found->destroyed)instance=&*found;}
    const bool permanent=(toggle && !toggle->durationMs) || std::ranges::any_of(toggles,[&](const auto* i){return ToggleItemProfile(c,i->key)->durationMs==0;});
    const bool utilities=(timed && timed->utility!=TimedEffectUtility::None) || std::ranges::any_of(v.schedule->effects,[&](const auto& e){return TimedItemProfile(c,e.source)->utility!=TimedEffectUtility::None;});
    const bool energy=(timed && timed->utility==TimedEffectUtility::Energy) || std::ranges::any_of(v.schedule->effects,[](const auto& e){return e.energyPercent!=0;});
    const bool families=energy || (group && group->family!=TimedEffectFamily::Generic) || std::ranges::any_of(v.schedule->effects,[&](const auto& e){return c.FindTimedEffectGroup(TimedItemProfile(c,e.source)->group)->family!=TimedEffectFamily::Generic;});
    put(243,1);put(0,4);put(energy ? 5 : families ? 4 : utilities ? 3 : permanent ? 2 : 1,1);put(v.status,1);put(v.error,1);put(v.control,8);put(v.preview,8);put(v.source ? v.source->id.Value() : 0,8);
    put(v.schedule->revision,8);put(v.schedule->generation,8);put(v.now,8);if(v.source)key(v.source->key);else put(0,6);
    put(timed ? 1 : toggle ? 2 : 0,1);put(timed ? timed->group : toggle ? toggle->group : 0,4);put(toggle ? toggle->variant : 0,2);put(group ? static_cast<unsigned>(group->clock) : toggle ? 1 : 0,1);
    put(timed ? timed->durationMs : toggle ? toggle->durationMs : 0,8);put(toggle && instance && instance->toggle ? instance->toggle->remainingMs : 0,8);
    const auto cooldown=timed ? timed->cooldownProfile : toggle ? toggle->cooldownProfile : std::nullopt;put(cooldown ? RecoveryCooldownRemaining(*v.schedule,*cooldown) : 0,8);
    put(timed ? timed->unitsPerUse : toggle ? 1 : 0,2);put(toggle && instance && instance->toggle && instance->toggle->active,1);put(group ? static_cast<unsigned>(group->reapply) : 0,1);put(group ? group->removeOnDeath : toggleGroup ? toggleGroup->removeOnDeath : 0,1);put(toggle ? toggle->requirements : 0,1);if(utilities || families)put(timed ? static_cast<unsigned>(timed->utility) : 0,1);if(energy)put(v.source && v.source->energy ? v.source->energy->percent : 0,1);if(families){put(group ? static_cast<unsigned>(group->family) : 0,1);put(timed && timed->conflictBonus ? static_cast<unsigned>(*timed->conflictBonus) : 0,4);}bonuses(timed ? timed->bonusProfile : toggle ? toggle->bonusProfile : 0);
    put(v.schedule->effects.size(),1);for(const auto& e:v.schedule->effects){const auto* p=TimedItemProfile(c,e.source);const auto* g=c.FindTimedEffectGroup(p->group);put(e.id,8);key(e.source);put(p->group,4);put(static_cast<unsigned>(g->clock),1);put(g->removeOnDeath,1);put(std::min(p->durationMs,TimedItemRemaining(e,v.now)),8);if(utilities || families)put(static_cast<unsigned>(p->utility),1);if(energy)put(e.energyPercent,1);if(families){put(static_cast<unsigned>(g->family),1);put(p->conflictBonus ? static_cast<unsigned>(*p->conflictBonus) : 0,4);}bonuses(p->bonusProfile);}
    put(toggles.size(),2);for(const auto* item:toggles){const auto* p=ToggleItemProfile(c,item->key);if(!p)return std::unexpected(E::BadValue);put(item->id.Value(),8);key(item->key);put(p->group,4);put(p->variant,2);put(item->toggle->remainingMs,8);put(p->durationMs,8);put(item->toggle->active,1);put(p->requirements,1);put(c.FindToggleEffectGroup(p->group)->removeOnDeath,1);bonuses(p->bonusProfile);}
    put(details->size(),4);out.insert(out.end(),details->begin(),details->end());if(out.size()>1u<<20)return std::unexpected(E::TooLarge);for(unsigned i=0;i<4;++i)out[1+i]=std::byte((out.size()>>(8*i))&255);return out;
}
}
