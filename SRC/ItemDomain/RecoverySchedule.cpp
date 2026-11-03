#include <item/RecoverySchedule.hpp>
#include <item/ItemCatalog.hpp>
#include "RecoveryMath.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace auriga::item
{
namespace
{
using E=RecoveryUseError;
const RecoveryProfile* Profile(const ItemCatalog& c,ItemKey key)
{const auto* e=c.Find(key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;const auto* p=r && r->recoveryProfile ? c.FindRecovery(r->recoveryProfile) : nullptr;return p ? &p->Data() : nullptr;}
bool Empty(const RecoverySchedule& s){return s.doses.empty() && s.cooldowns.empty() && s.effects.empty() && s.creditRaw==std::array<std::uint16_t,2>{};}
std::uint64_t Plus(std::uint64_t a,std::uint64_t b){return b>UINT64_MAX-a ? UINT64_MAX : a+b;}
std::int64_t Product(std::int64_t value,std::uint64_t count,std::int64_t limit)
{if(value<=0 || !count || limit<=0)return 0;if(value>=limit || count>=static_cast<std::uint64_t>(limit/value+(limit%value!=0)))return limit;return value*static_cast<std::int64_t>(count);}
bool Pools(const RecoveryPools& p){return p.maxHpRaw>0 && detail::ValidRecoveryPool(p.hpRaw,p.maxHpRaw) && detail::ValidRecoveryPool(p.spRaw,p.maxSpRaw);}
std::expected<void,E> Bump(RecoverySchedule& s,std::uint64_t before,bool generation=false)
{if(before==UINT64_MAX || (generation && s.generation==UINT64_MAX))return std::unexpected(E::InvalidState);s.revision=before+1;if(generation)++s.generation;return {};}
}
bool ValidateRecoverySchedule(const ItemCatalog& c,const RecoverySchedule& s)
{
    if(s.doses.size()>MaxRecoveryDoses || s.cooldowns.size()>MaxRecoveryCooldowns || s.creditRaw[0]>999 || s.creditRaw[1]>999 || (!s.revision && (!Empty(s) || s.generation)))return false;
    std::set<std::uint64_t> ids;std::array<unsigned,2> reserves{},periodic{};std::array<std::int64_t,2> totals{};
    for(const auto& d:s.doses)
    {
        const auto* p=Profile(c,d.source);if(!d.id || !ids.insert(d.id).second || !p || p->Delivery()!=d.delivery ||
            (d.pool!=RecoveryPool::Hp && d.pool!=RecoveryPool::Sp) || !d.intervalMs || d.intervalMs>INT64_MAX || d.nextTickMs>d.intervalMs)return false;
        const auto pool=static_cast<unsigned>(d.pool)-1;
        if(d.delivery==RecoveryDelivery::Reserve)
        {if(++reserves[pool]>8 || d.remainingRaw<=0 || d.remainingRaw>MaxItemStatRaw || d.perTickRaw || d.remainingMs || !d.nextTickMs || !d.rateBp || d.rateBp>10000 || d.remainingRaw>MaxItemStatRaw-totals[pool])return false;totals[pool]+=d.remainingRaw;}
        else if(d.delivery==RecoveryDelivery::Periodic)
        {if(++periodic[pool]>1 || d.remainingRaw || d.perTickRaw<=0 || d.perTickRaw>MaxItemStatRaw || d.rateBp || d.remainingMs>INT64_MAX ||
            (!d.remainingMs && (!d.dueTicks || d.nextTickMs)) || (d.remainingMs && (!d.nextTickMs || d.remainingMs<d.nextTickMs || (d.remainingMs-d.nextTickMs)%d.intervalMs)))return false;}
        else return false;
    }
    std::uint32_t previous=0;for(const auto& cd:s.cooldowns){if(cd.profile<=previous || !c.FindConsumableCooldown(cd.profile) || !cd.remainingMs || cd.remainingMs>INT64_MAX)return false;previous=cd.profile;}
    if(s.effects.size()>MaxRuntimeTimedEffects)return false;
    std::set<std::uint32_t> groups;
    for(const auto& effect:s.effects)
    {
        const auto* e=c.Find(effect.source);const auto* row=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;
        const auto* p=row && row->timedEffectProfile ? c.FindTimedEffect(*row->timedEffectProfile) : nullptr;
        const auto* g=p ? c.FindTimedEffectGroup(p->group) : nullptr;
        if(!g || !effect.id || !ids.insert(effect.id).second || !groups.insert(g->id).second ||
           (g->clock==TimedEffectClock::OnlineTime ? !effect.remainingMs || effect.remainingMs>p->durationMs || effect.expiresAtMs!=0 : effect.remainingMs!=0 || !effect.expiresAtMs || effect.expiresAtMs>INT64_MAX))return false;
    }
    return (s.doses.empty() && s.effects.empty()) || s.generation>0;
}
std::expected<RecoverySchedule,E> AdvanceRecoverySchedule(const ItemCatalog& c,RecoverySchedule s,std::uint64_t elapsed)
{
    if(!ValidateRecoverySchedule(c,s))return std::unexpected(E::InvalidState);
    for(auto& cd:s.cooldowns)cd.remainingMs-=std::min(elapsed,cd.remainingMs);std::erase_if(s.cooldowns,[](const auto& cd){return !cd.remainingMs;});
    std::array<bool,2> first{};
    for(auto& d:s.doses)
    {
        const auto pool=static_cast<unsigned>(d.pool)-1;
        if(d.delivery==RecoveryDelivery::Reserve){if(first[pool])continue;first[pool]=true;}
        const auto step=d.delivery==RecoveryDelivery::Periodic ? std::min(elapsed,d.remainingMs) : elapsed;
        if(d.nextTickMs && step>=d.nextTickMs){d.dueTicks=Plus(d.dueTicks,1+(step-d.nextTickMs)/d.intervalMs);d.nextTickMs=d.intervalMs-(step-d.nextTickMs)%d.intervalMs;}
        else if(d.nextTickMs)d.nextTickMs-=step;
        if(d.delivery==RecoveryDelivery::Periodic){d.remainingMs-=step;if(!d.remainingMs)d.nextTickMs=0;}
    }
    for(auto& effect:s.effects)if(!effect.expiresAtMs)effect.remainingMs-=std::min(elapsed,effect.remainingMs);
    std::erase_if(s.effects,[](const auto& effect){return !effect.expiresAtMs && !effect.remainingMs;});
    return s;
}
std::uint64_t RecoveryCooldownRemaining(const RecoverySchedule& s,std::uint32_t profile)
{const auto cd=std::ranges::find(s.cooldowns,profile,&RecoveryCooldown::profile);return cd==s.cooldowns.end() ? 0 : cd->remainingMs;}
std::expected<void,E> SetRecoveryCooldown(const ItemCatalog& c,RecoverySchedule& s,std::uint32_t profile,std::uint64_t duration)
{
    if(!c.FindConsumableCooldown(profile) || !duration || duration>INT64_MAX)return std::unexpected(E::InvalidState);
    auto cd=std::ranges::find(s.cooldowns,profile,&RecoveryCooldown::profile);if(cd==s.cooldowns.end()){if(s.cooldowns.size()>=MaxRecoveryCooldowns)return std::unexpected(E::QueueFull);s.cooldowns.push_back({profile,duration});std::ranges::sort(s.cooldowns,{},&RecoveryCooldown::profile);}else cd->remainingMs=duration;return {};
}
std::expected<RecoverySchedulePlan,E> PlanScheduledRecoveryUse(const ItemCatalog& c,ItemKey key,const InstantRecoveryAdmission& actor,
    const RecoverySchedule& before,std::uint64_t elapsed,std::array<std::uint64_t,2> ids)
{
    const auto* profile=Profile(c,key);if(!profile || (profile->Delivery()!=RecoveryDelivery::Reserve && profile->Delivery()!=RecoveryDelivery::Periodic))return std::unexpected(E::WrongDelivery);
    auto advanced=AdvanceRecoverySchedule(c,before,elapsed);if(!advanced)return std::unexpected(advanced.error());auto admission=actor;
    if(profile->cooldownProfile)admission.cooldownRemainingMs=std::max(admission.cooldownRemainingMs,RecoveryCooldownRemaining(*advanced,*profile->cooldownProfile));
    const auto admitted=PlanRecoveryAdmission(c,key,admission);if(!admitted)return std::unexpected(admitted.error());
    RecoverySchedulePlan out{std::move(*advanced),0,0,admitted->unitsToConsume,admitted->arenaQuotaToConsume};auto& s=out.after;bool changed=false;
    for(unsigned pool=0;pool<2;++pool)
    {
        const auto kind=pool==0 ? RecoveryPool::Hp : RecoveryPool::Sp;const auto max=pool==0 ? actor.pools.maxHpRaw : actor.pools.maxSpRaw;const auto current=pool==0 ? actor.pools.hpRaw : actor.pools.spRaw;
        const auto* reserve=std::get_if<ReserveRecovery>(&profile->effect);const auto* periodic=std::get_if<PeriodicRecovery>(&profile->effect);
        const auto raw=reserve ? (pool==0 ? reserve->hpRaw : reserve->spRaw) : (pool==0 ? periodic->hpPerTickRaw : periodic->spPerTickRaw);if(!raw || !max)continue;
        if(!ids[pool] || (pool && ids[pool]==ids[0]) || std::ranges::any_of(s.doses,[&](const auto& d){return d.id==ids[pool];}))return std::unexpected(E::InvalidState);
        if(reserve)
        {
            std::int64_t pending=s.creditRaw[pool];unsigned count=0;for(const auto& d:s.doses)if(d.pool==kind && d.delivery==RecoveryDelivery::Reserve){pending+=d.remainingRaw;++count;}
            const auto room=std::max<std::int64_t>(0,max-current-pending);const auto amount=std::min(room,detail::ScaleRecovery(raw,10000+std::min<std::int64_t>(actor.potionBonusBp,reserve->flatBonusCapBp)));if(!amount)continue;
            auto same=std::ranges::find_if(s.doses,[&](const auto& d){return d.delivery==RecoveryDelivery::Reserve && d.pool==kind && d.source==key && d.intervalMs==reserve->tickIntervalMs && d.rateBp==reserve->maxPoolPerTickBp;});
            if(same!=s.doses.end()){same->remainingRaw+=amount;same->id=ids[pool];}
            else{if(count>=8)return std::unexpected(E::QueueFull);s.doses.push_back({ids[pool],key,RecoveryDelivery::Reserve,kind,amount,0,reserve->tickIntervalMs,reserve->tickIntervalMs,0,0,reserve->maxPoolPerTickBp});}
        }
        else
        {
            std::erase_if(s.doses,[&](const auto& d){return d.delivery==RecoveryDelivery::Periodic && d.pool==kind;});
            s.doses.push_back({ids[pool],key,RecoveryDelivery::Periodic,kind,0,raw,periodic->tickIntervalMs,periodic->tickIntervalMs,periodic->durationMs,0,0});
        }
        changed=true;
    }
    if(!changed)return std::unexpected(E::NoEffect);
    if(admitted->cooldown){if(auto cd=SetRecoveryCooldown(c,s,admitted->cooldown->profileId,admitted->cooldown->durationMs);!cd)return std::unexpected(cd.error());}
    if(auto v=Bump(s,before.revision,true);!v)return std::unexpected(v.error());if(!ValidateRecoverySchedule(c,s))return std::unexpected(E::InvalidState);return out;
}
std::expected<RecoverySchedulePlan,E> PlanRecoveryScheduleTick(const ItemCatalog& c,const RecoverySchedule& before,std::uint64_t elapsed,
    const InstantRecoveryAdmission& actor,bool dead,bool stunned)
{
    if(!Pools(actor.pools) || (actor.context!=RecoveryContext::World && actor.context!=RecoveryContext::Duel && actor.context!=RecoveryContext::Arena && actor.context!=RecoveryContext::GuildWar))return std::unexpected(E::InvalidState);auto advanced=AdvanceRecoverySchedule(c,before,elapsed);if(!advanced)return std::unexpected(advanced.error());
    RecoverySchedulePlan out{std::move(*advanced)};auto& s=out.after;
    if(dead || !actor.pools.hpRaw){const bool had=!s.doses.empty() || s.creditRaw!=std::array<std::uint16_t,2>{};s.doses.clear();s.creditRaw={};if(had){if(s.generation==UINT64_MAX)return std::unexpected(E::InvalidState);++s.generation;}}
    else
    {
        std::array<std::int64_t,2> earned{};const std::array missing{actor.pools.maxHpRaw-actor.pools.hpRaw,actor.pools.maxSpRaw-actor.pools.spRaw};std::array<bool,2> first{};
        for(unsigned pool=0;pool<2;++pool)if(!missing[pool]){s.creditRaw[pool]=0;std::erase_if(s.doses,[&](const auto& d){return d.pool==(pool==0 ? RecoveryPool::Hp : RecoveryPool::Sp) && d.delivery==RecoveryDelivery::Reserve;});}
        for(auto& d:s.doses)
        {
            const auto pool=static_cast<unsigned>(d.pool)-1;if(d.delivery==RecoveryDelivery::Reserve){if(first[pool])continue;first[pool]=true;}
            const auto* p=Profile(c,d.source);const bool allowed=!stunned && (p->allowedContexts&static_cast<unsigned>(actor.context)) && (!actor.inPvp || p->allowDuringPvp) && (actor.context!=RecoveryContext::Arena || (actor.arena && actor.arena->potionsAllowed));
            if(allowed && d.dueTicks)
            {
                const auto room=std::max<std::int64_t>(0,missing[pool]-earned[pool]-s.creditRaw[pool]);
                const auto cap=d.delivery==RecoveryDelivery::Reserve ? std::max<std::int64_t>(1,detail::ScaleRecovery(pool==0 ? actor.pools.maxHpRaw : actor.pools.maxSpRaw,d.rateBp)) : d.perTickRaw;
                auto amount=Product(cap,d.dueTicks,room);if(d.delivery==RecoveryDelivery::Reserve){amount=std::min(amount,d.remainingRaw);d.remainingRaw-=amount;}earned[pool]+=amount;
            }
            d.dueTicks=0;
        }
        std::erase_if(s.doses,[](const auto& d){return d.delivery==RecoveryDelivery::Reserve ? !d.remainingRaw : !d.remainingMs && !d.dueTicks;});
        for(unsigned pool=0;pool<2;++pool){const auto raw=std::min(missing[pool],earned[pool]+s.creditRaw[pool]);const auto whole=raw/1000*1000;s.creditRaw[pool]=static_cast<std::uint16_t>(raw-whole);(pool==0 ? out.hpRestoredRaw : out.spRestoredRaw)=whole;}
    }
    if(s==before && (!elapsed || (before.doses.empty() && before.cooldowns.empty() && before.effects.empty())))return std::unexpected(E::NoEffect);if(auto v=Bump(s,before.revision);!v)return std::unexpected(v.error());if(!ValidateRecoverySchedule(c,s))return std::unexpected(E::InvalidState);return out;
}
std::expected<RecoverySchedulePlan,E> PlanRecoveryScheduleCancel(const ItemCatalog& c,const RecoverySchedule& before,std::uint64_t elapsed,std::uint64_t generation,std::uint64_t id)
{
    if(generation!=before.generation)return std::unexpected(E::InvalidState);auto next=AdvanceRecoverySchedule(c,before,elapsed);if(!next)return std::unexpected(next.error());
    if(id){if(!std::erase_if(next->doses,[&](const auto& d){return d.id==id;}))return std::unexpected(E::NoEffect);}
    else{if(next->doses.empty() && next->creditRaw==std::array<std::uint16_t,2>{})return std::unexpected(E::NoEffect);next->doses.clear();next->creditRaw={};}
    if(auto v=Bump(*next,before.revision,true);!v)return std::unexpected(v.error());return RecoverySchedulePlan{std::move(*next)};
}
std::expected<std::vector<std::uint8_t>,E> EncodeRecoverySchedule(const ItemCatalog& c,const RecoverySchedule& s)
{
    if(!ValidateRecoverySchedule(c,s))return std::unexpected(E::InvalidState);std::vector<std::uint8_t> out;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    for(char b:std::string_view(s.effects.empty() ? "RCS1" : "RCS2"))put(b,1);put(s.revision,8);put(s.generation,8);for(auto v:s.creditRaw)put(v,2);put(s.doses.size(),1);
    for(const auto& d:s.doses){put(d.id,8);put(static_cast<unsigned>(d.source.Category()),2);put(d.source.LocalId(),4);put(static_cast<unsigned>(d.delivery),1);put(static_cast<unsigned>(d.pool),1);put(d.remainingRaw,8);put(d.perTickRaw,8);put(d.intervalMs,8);put(d.nextTickMs,8);put(d.remainingMs,8);put(d.dueTicks,8);put(d.rateBp,2);}
    put(s.cooldowns.size(),1);for(const auto& cd:s.cooldowns){put(cd.profile,4);put(cd.remainingMs,8);}if(!s.effects.empty()){put(s.effects.size(),1);for(const auto& e:s.effects){put(e.id,8);put(static_cast<unsigned>(e.source.Category()),2);put(e.source.LocalId(),4);put(e.remainingMs,8);put(e.expiresAtMs,8);}}
    if(out.size()>MaxRecoveryStateBytes)return std::unexpected(E::InvalidState);return out;
}
std::expected<RecoverySchedule,E> DecodeRecoverySchedule(const ItemCatalog& c,std::span<const std::uint8_t> bytes)
{
    if(bytes.size()<26 || bytes.size()>MaxRecoveryStateBytes)return std::unexpected(E::InvalidState);std::size_t at=0;bool bad=false;const auto get=[&](unsigned n){if(n>bytes.size()-at){bad=true;return std::uint64_t{0};}std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(bytes[at++])<<(8*i);return v;};
    const auto magic=get(4);if(magic!=0x31534352 && magic!=0x32534352)return std::unexpected(E::InvalidState);RecoverySchedule s;s.revision=get(8);s.generation=get(8);for(auto& v:s.creditRaw)v=static_cast<std::uint16_t>(get(2));const auto count=get(1);if(count>MaxRecoveryDoses)return std::unexpected(E::InvalidState);
    for(unsigned i=0;i<count;++i){const auto id=get(8),category=get(2),local=get(4);const auto key=ItemKey::Create(static_cast<std::uint16_t>(category),static_cast<std::uint32_t>(local));if(!key)return std::unexpected(E::InvalidState);RecoveryDose d{id,*key};d.delivery=static_cast<RecoveryDelivery>(get(1));d.pool=static_cast<RecoveryPool>(get(1));d.remainingRaw=static_cast<std::int64_t>(get(8));d.perTickRaw=static_cast<std::int64_t>(get(8));d.intervalMs=get(8);d.nextTickMs=get(8);d.remainingMs=get(8);d.dueTicks=get(8);d.rateBp=static_cast<std::uint16_t>(get(2));s.doses.push_back(d);}
    const auto cds=get(1);if(cds>MaxRecoveryCooldowns)return std::unexpected(E::InvalidState);for(unsigned i=0;i<cds;++i){const auto profile=get(4),remaining=get(8);s.cooldowns.push_back({static_cast<std::uint32_t>(profile),remaining});}
    if(magic==0x32534352){const auto n=get(1);if(n>MaxRuntimeTimedEffects || !n)return std::unexpected(E::InvalidState);for(unsigned i=0;i<n;++i){const auto id=get(8),category=get(2),local=get(4);const auto key=ItemKey::Create(static_cast<std::uint16_t>(category),static_cast<std::uint32_t>(local));if(!key)return std::unexpected(E::InvalidState);const auto remaining=get(8),deadline=get(8);s.effects.push_back({id,*key,remaining,deadline});}}
    if(bad || at!=bytes.size() || !ValidateRecoverySchedule(c,s))return std::unexpected(E::InvalidState);return s;
}
}
