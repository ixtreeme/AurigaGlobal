#include <item/GrowthPets.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <limits>

namespace auriga::item {
bool ValidGrowthPetProfile(const GrowthPetProfile& p) noexcept {
    if(!p.maximumLevel || p.maximumLevel>255 || p.levels.size()!=p.maximumLevel ||
       p.stages.empty() || p.stages.size()>16 ||
       (p.experiencePolicy!=GrowthExpPolicy::MobOnly && p.experiencePolicy!=GrowthExpPolicy::MobAndItem))return false;
    for(std::size_t i=0;i<p.levels.size();++i) {
        const auto& l=p.levels[i];
        if(l.level!=i+1 || l.mobExperience>INT64_MAX || l.itemExperience>INT64_MAX)return false;
        if(l.level==p.maximumLevel ? l.mobExperience || l.itemExperience : !l.mobExperience || !l.itemExperience)return false;
    }
    std::uint16_t prior=0;
    for(std::size_t i=0;i<p.stages.size();++i) {
        const auto& s=p.stages[i];
        if(s.stage!=i || !s.form || s.maximumLevel<=prior || s.maximumLevel>p.maximumLevel)return false;
        prior=s.maximumLevel;
    }
    return prior==p.maximumLevel;
}
namespace {
bool ValidProgress(const GrowthPetProfile& p,const GrowthPetProgress& s) {
    if(!s.level || s.level>p.maximumLevel || s.stage>=p.stages.size())return false;
    const auto minimum=s.stage ? p.stages[s.stage-1].maximumLevel+1 : 1;
    if(s.level<minimum || s.level>p.stages[s.stage].maximumLevel)return false;
    const auto& l=p.levels[s.level-1];
    return s.mobExperience<=l.mobExperience && s.itemExperience<=l.itemExperience;
}
const GrowthPetProfile* Profile(const ItemCatalog& catalog,const ItemCatalogEntry& entry) {
    const auto* companion=std::get_if<ItemCompanionRow>(&entry.category);
    return companion && companion->kind==CompanionKind::GrowthPet && companion->growthProfile ?
        catalog.FindGrowthPetProfile(*companion->growthProfile) : nullptr;
}
}
std::expected<GrowthPetProgress,GrowthPetError> MakeGrowthPetProgress(const ItemCatalog& c,ItemKey key) {
    const auto* entry=c.Find(key);if(!entry)return std::unexpected(GrowthPetError::MissingItem);
    if(!Profile(c,*entry))return std::unexpected(GrowthPetError::NotGrowthPet);
    return GrowthPetProgress{1,0,0,0};
}
std::expected<GrowthPetPlan,GrowthPetError> PlanGrowthPetProgress(const ItemCatalog& c,const GrowthPetRequest& r) {
    using E=GrowthPetError;using A=GrowthPetAction;
    if(!r.admittedByServer || !r.activePet || r.lockedByOther || r.exchanging)return std::unexpected(E::Forbidden);
    if(!r.instanceId || !r.revision || r.revision==UINT64_MAX || !r.progressRevision || r.progressRevision==UINT64_MAX ||
       !r.actorId || r.actorId!=r.ownerId || !r.rulesetRevision ||
       std::ranges::all_of(r.operationId,[](auto v){return !v;}))return std::unexpected(E::InvalidOperation);
    const auto* entry=c.Find(r.key);if(!entry)return std::unexpected(E::MissingItem);
    const auto* p=Profile(c,*entry);if(!p)return std::unexpected(E::NotGrowthPet);
    if(r.count!=1 || r.refinementLevel || entry->base.retired)return std::unexpected(E::InvalidSnapshot);
    if(!ValidProgress(*p,r.progress))return std::unexpected(E::InvalidProgress);
    if(r.action<A::AwardMobExperience || r.action>A::Evolve)return std::unexpected(E::InvalidAction);
    if(r.action==A::Evolve ? r.experience!=0 : !r.experience || r.experience>INT64_MAX)return std::unexpected(E::InvalidExperience);
    GrowthPetPlan plan{r,r.progress};plan.catalogRevision=c.Revision();plan.catalogHash=c.ContentHash();
    plan.beforeForm=p->stages[r.progress.stage].form;
    auto& s=plan.after;const auto& stage=p->stages[s.stage];
    if(r.action==A::Evolve) {
        if(s.stage+1>=p->stages.size() || s.level!=stage.maximumLevel)return std::unexpected(E::NotAtEvolutionGate);
        ++s.stage;++s.level;s.mobExperience=s.itemExperience=0;plan.levelled=plan.evolved=true;
    } else {
        const auto& l=p->levels[s.level-1];
        auto& value=r.action==A::AwardMobExperience ? s.mobExperience : s.itemExperience;
        const auto capacity=r.action==A::AwardMobExperience ? l.mobExperience : l.itemExperience;
        plan.acceptedExperience=(std::min)(r.experience,capacity-value);
        plan.discardedExperience=r.experience-plan.acceptedExperience;value+=plan.acceptedExperience;
        const bool full=s.mobExperience==l.mobExperience &&
            (p->experiencePolicy==GrowthExpPolicy::MobOnly || s.itemExperience==l.itemExperience);
        if(s.level<stage.maximumLevel && full) {
            ++s.level;s.mobExperience=s.itemExperience=0;plan.levelled=true;
        }
    }
    plan.afterForm=p->stages[s.stage].form;plan.atMaximum=s.level==p->maximumLevel;
    plan.atEvolutionGate=!plan.atMaximum && s.level==p->stages[s.stage].maximumLevel;
    return plan;
}
}
