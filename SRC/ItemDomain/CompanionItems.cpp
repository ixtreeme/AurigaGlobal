#include <item/CompanionItems.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <limits>

namespace auriga::item {
namespace {
constexpr auto MaxTime=std::uint64_t{INT64_MAX};
}
bool ValidCompanionState(const ItemCompanionRow& row,const CompanionState& state) noexcept {
    const auto& t=state.lifetime;
    if(state.phase<CompanionPhase::Dormant || state.phase>CompanionPhase::Riding ||
       ((state.phase==CompanionPhase::Dormant)!=(state.activeFormId==0)) ||
       (row.kind==CompanionKind::Pet && state.phase==CompanionPhase::Riding) ||
       (state.ridingRequested && (row.kind!=CompanionKind::Mount || !state.recallRequested)) ||
       (state.phase!=CompanionPhase::Dormant && !state.recallRequested) ||
       (t.expired && (state.phase!=CompanionPhase::Dormant || state.recallRequested)) ||
       t.deadlineUtcMs>MaxTime || t.remainingMs>MaxTime)return false;
    if(row.clock==CompanionClock::Permanent)return !t.started && !t.expired && !t.deadlineUtcMs && !t.remainingMs;
    if(row.clock==CompanionClock::ActiveTime)return t.started && !t.deadlineUtcMs && t.remainingMs<=row.durationMs && (t.expired==!t.remainingMs);
    if(!t.started)return row.clock==CompanionClock::SinceFirstUse && state.phase==CompanionPhase::Dormant &&
        !t.deadlineUtcMs && !t.remainingMs && !t.expired && !state.recallRequested;
    return t.deadlineUtcMs && !t.remainingMs;
}
std::array<std::uint8_t,22> EncodeCompanionLifetime(const CompanionLifetime& t) {
    std::array<std::uint8_t,22> b{'C','P','L',1,static_cast<std::uint8_t>(t.started),static_cast<std::uint8_t>(t.expired)};
    for(unsigned n=0;n<8;++n){b[6+n]=static_cast<std::uint8_t>(t.deadlineUtcMs>>(8*n));b[14+n]=static_cast<std::uint8_t>(t.remainingMs>>(8*n));}return b;
}
std::optional<CompanionLifetime> DecodeCompanionLifetime(std::span<const std::uint8_t> b) {
    if(b.size()!=22 || b[0]!='C' || b[1]!='P' || b[2]!='L' || b[3]!=1 || b[4]>1 || b[5]>1)return {};
    CompanionLifetime t;t.started=b[4];t.expired=b[5];
    for(unsigned n=0;n<8;++n){t.deadlineUtcMs|=std::uint64_t{b[6+n]}<<(8*n);t.remainingMs|=std::uint64_t{b[14+n]}<<(8*n);}
    if(t.deadlineUtcMs>MaxTime || t.remainingMs>MaxTime)return {};return t;
}
bool ValidCompanion(const ItemCompanionRow& row) noexcept {
    return row.key.Category()==ItemCategory::Companion &&
        (row.kind==CompanionKind::Pet || row.kind==CompanionKind::Mount || row.kind==CompanionKind::GrowthPet) && row.form &&
        (row.kind==CompanionKind::Mount ? row.ridingForm && *row.ridingForm : !row.ridingForm && !row.flyingMount) &&
        (row.kind==CompanionKind::GrowthPet ? row.growthProfile && *row.growthProfile &&
            (row.clock==CompanionClock::Permanent || row.clock==CompanionClock::ActiveTime) : !row.growthProfile) &&
        row.clock>=CompanionClock::Permanent && row.clock<=CompanionClock::ActiveTime &&
        row.durationMs<=MaxTime && (row.clock==CompanionClock::Permanent ? !row.durationMs : row.durationMs>0);
}
std::expected<CompanionState,CompanionError> MakeCompanionState(const ItemCatalog& catalog,ItemKey key,std::uint64_t created) {
    if(created>MaxTime)return std::unexpected(CompanionError::InvalidTime);
    const auto* entry=catalog.Find(key);if(!entry)return std::unexpected(CompanionError::MissingItem);
    const auto* row=std::get_if<ItemCompanionRow>(&entry->category);if(!row)return std::unexpected(CompanionError::NotCompanion);
    if(row->kind==CompanionKind::GrowthPet)return std::unexpected(CompanionError::NotCompanion);
    CompanionState state;
    if(row->clock==CompanionClock::SinceCreation) {
        if(row->durationMs>MaxTime-created)return std::unexpected(CompanionError::TimeOverflow);
        state.lifetime.started=true;state.lifetime.deadlineUtcMs=created+row->durationMs;
    }
    if(row->clock==CompanionClock::ActiveTime){state.lifetime.started=true;state.lifetime.remainingMs=row->durationMs;}
    return state;
}
std::expected<CompanionPlan,CompanionError> PlanCompanion(const ItemCatalog& catalog,const CompanionRequest& r) {
    using E=CompanionError;using A=CompanionAction;using P=CompanionPhase;
    if(!r.admittedByServer)return std::unexpected(E::Forbidden);
    if(!r.instanceId || !r.revision || r.revision==UINT64_MAX || !r.actorId || r.ownerId!=r.actorId ||
       !r.rulesetRevision || std::ranges::all_of(r.operationId,[](auto b){return !b;}))return std::unexpected(E::InvalidOperation);
    if(r.count!=1)return std::unexpected(E::InvalidSnapshot);
    if(r.lockedByOther || r.exchanging)return std::unexpected(E::Forbidden);
    const auto* entry=catalog.Find(r.key);if(!entry)return std::unexpected(E::MissingItem);
    const auto* row=std::get_if<ItemCompanionRow>(&entry->category);if(!row)return std::unexpected(E::NotCompanion);
    if(row->kind==CompanionKind::GrowthPet)return std::unexpected(E::NotCompanion);
    const auto* refinement=catalog.FindRefinement(entry->base.refinementProfile);
    const auto* level=refinement->FindLevel(r.refinementLevel);if(!level)return std::unexpected(E::InvalidLevel);
    if(!ValidCompanionState(*row,r.state) || (r.state.activeFormId && !catalog.FindCompanionForm(r.state.activeFormId)))return std::unexpected(E::InvalidState);
    if(r.action<A::Summon || r.action>A::Maintain)return std::unexpected(E::InvalidAction);
    if(r.nowUtcMs>MaxTime || r.elapsedOwnerMs>MaxTime)return std::unexpected(E::InvalidTime);
    const bool use=r.action==A::Summon || r.action==A::Mount;
    if(use) {
        if(entry->base.retired || !r.ownerAlive || r.polymorphed || r.observer || !r.worldAllowsCompanion ||
           r.characterClass<1 || r.characterClass>4 || r.sex<1 || r.sex>2 || r.characterLevel<entry->base.minimumLevel)
            return std::unexpected(E::Forbidden);
        const auto* policy=catalog.FindUsePolicy(entry->base.usePolicy);
        if(!(policy->allowedClasses&(1u<<(r.characterClass-1))) || !(policy->allowedSexes&(1u<<(r.sex-1))) ||
           (r.inCombat && !policy->useDuringCombat))return std::unexpected(E::Forbidden);
        if(r.channelOccupiedByOther)return std::unexpected(E::ChannelOccupied);
        if(r.action==A::Summon && r.state.phase!=P::Dormant)return std::unexpected(E::AlreadyActive);
        if(r.action==A::Mount) {
            if(row->kind!=CompanionKind::Mount)return std::unexpected(E::WrongKind);
            if(r.state.phase!=P::Following)return std::unexpected(E::NotFollowing);
            if(!r.worldAllowsRiding)return std::unexpected(E::Forbidden);
        }
    }
    if(r.action==A::Dismount && r.state.phase!=P::Riding)return std::unexpected(E::NotRiding);
    CompanionPlan plan{r,r.state,*catalog.FindCompanionForm(row->form)};
    plan.catalogRevision=catalog.Revision();plan.catalogHash=catalog.ContentHash();
    plan.followerForm=*catalog.FindCompanionForm(row->form);
    if(row->ridingForm)plan.ridingForm=*catalog.FindCompanionForm(*row->ridingForm);
    auto& t=plan.after.lifetime;
    if(!t.expired && row->clock!=CompanionClock::Permanent) {
        if(row->clock==CompanionClock::ActiveTime) {
            if(r.state.phase!=P::Dormant || r.elapsedWasActive)t.remainingMs=r.elapsedOwnerMs>=t.remainingMs ? 0 : t.remainingMs-r.elapsedOwnerMs;
            if(!t.remainingMs)t.expired=true;
        } else {
            if(!t.started && use) {
                if(row->durationMs>MaxTime-r.nowUtcMs)return std::unexpected(E::TimeOverflow);
                t.started=true;t.deadlineUtcMs=r.nowUtcMs+row->durationMs;
            }
            if(t.started && r.nowUtcMs>=t.deadlineUtcMs)t.expired=true;
        }
    }
    if(t.expired || entry->base.retired) {plan.after.phase=P::Dormant;plan.after.recallRequested=false;}
    else if(r.action==A::Dismiss) {plan.after.phase=P::Dormant;plan.after.recallRequested=false;}
    else if(!r.ownerAlive || r.polymorphed || r.observer || !r.worldAllowsCompanion)plan.after.phase=P::Dormant;
    else if(r.action==A::Summon){plan.after.phase=P::Following;plan.after.recallRequested=true;}
    else if(r.action==A::Mount)plan.after.phase=P::Riding;
    else if(r.action==A::Dismount || (plan.after.phase==P::Riding && !r.worldAllowsRiding))plan.after.phase=P::Following;
    plan.after.activeFormId=plan.after.phase==P::Following ? row->form : plan.after.phase==P::Riding ? *row->ridingForm : 0;
    if(!plan.after.recallRequested)plan.after.ridingRequested=false;
    else if(r.action==A::Mount)plan.after.ridingRequested=true;
    else if(r.action==A::Dismount)plan.after.ridingRequested=false;
    const bool differentForm=plan.after.activeFormId!=r.state.activeFormId;
    plan.spawnFollower=plan.after.phase==P::Following && (r.state.phase!=P::Following || differentForm);
    plan.despawnFollower=r.state.phase==P::Following && (plan.after.phase!=P::Following || differentForm);
    plan.changeRider=(r.state.phase==P::Riding)!=(plan.after.phase==P::Riding) ||
        (r.state.phase==P::Riding && plan.after.phase==P::Riding && differentForm);
    plan.bonusEnabled=(row->kind==CompanionKind::Pet ? plan.after.phase==P::Following : plan.after.phase==P::Riding) &&
        (!row->dungeonOnlyBonus || r.inDungeon);
    plan.removeItem=t.expired;
    plan.baseBonusProfile=entry->base.baseBonusProfile;plan.refinementBonusProfile=level->extraBonusProfile;
    return plan;
}
}
