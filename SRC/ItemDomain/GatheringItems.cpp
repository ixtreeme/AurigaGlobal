#include <item/GatheringItems.hpp>
#include <item/ItemCatalog.hpp>
#include <algorithm>
#include <limits>

namespace auriga::item
{
bool ValidToolProfile(const ToolProfile& p) noexcept
{
    if(p.levels.empty() || p.levels.size()>256)return false;
    for(std::size_t i=0;i<p.levels.size();++i)
    {
        const auto& s=p.levels[i];
        if(s.level!=i || s.power>INT32_MAX || s.gatherBonusBp>10000 || !s.practiceDenominator ||
           s.practiceNumerator>s.practiceDenominator || s.refineChanceBp>10000 || s.failureLossBp>10000 ||
           s.failure<ToolFailure::LosePoints || s.failure>ToolFailure::ResetAtLevel)return false;
        if(s.failure==ToolFailure::ResetAtLevel)
        {if(!s.failureLevel || *s.failureLevel>s.level || s.failureLossBp)return false;}
        else if(s.failureLevel)return false;
        if(i+1==p.levels.size())
        {
            if(s.requiredPoints || s.practiceNumerator || s.practiceDenominator!=1 || s.refineChanceBp ||
               s.failure!=ToolFailure::LosePoints || s.failureLossBp)return false;
        }
        else if(!s.requiredPoints)return false;
    }
    return true;
}
bool ValidFishingItem(const ItemFishingRow& r) noexcept
{
    if(r.key.Category()!=ItemCategory::Fishing || r.kind<FishingKind::LiveFish || r.kind>FishingKind::Bait)return false;
    if(r.kind==FishingKind::LiveFish)return r.useLootProfile && *r.useLootProfile && !r.grillResult && !r.baitPower;
    if(r.kind==FishingKind::DeadFish)return !r.useLootProfile && !r.baitPower;
    return !r.useLootProfile && !r.grillResult && r.baitPower>0 && r.baitPower<=INT32_MAX;
}
namespace
{
using E=GatheringError;
std::expected<const ItemCatalogEntry*,E> Admit(const ItemCatalog& catalog,const GatheringContext& c,const GatheringItemSnapshot& s)
{
    if(!c.admittedByServer)return std::unexpected(E::Forbidden);
    if(!c.actorId || !c.rulesetRevision || std::ranges::all_of(c.operationId,[](auto b){return b==0;}))return std::unexpected(E::InvalidOperation);
    if(!s.instanceId || !s.revision || s.revision==UINT64_MAX || s.ownerId!=c.actorId || s.locked || s.exchanging)return std::unexpected(E::InvalidSnapshot);
    const auto* item=catalog.Find(s.item);
    if(!item)return std::unexpected(E::MissingItem);
    if(item->base.retired)return std::unexpected(E::RetiredItem);
    if(!s.count || s.count>item->base.maxStack)return std::unexpected(E::InvalidStack);
    if(!catalog.FindRefinement(item->base.refinementProfile)->FindLevel(s.refinementLevel))return std::unexpected(E::InvalidLevel);
    if(c.characterLevel<item->base.minimumLevel || (c.inCombat && !catalog.FindUsePolicy(item->base.usePolicy)->useDuringCombat))return std::unexpected(E::Forbidden);
    return item;
}
std::expected<bool,E> Decision(std::uint32_t numerator,std::uint32_t denominator,std::uint32_t roll)
{
    if(!numerator || numerator==denominator)
    {if(roll)return std::unexpected(E::InvalidDecision);return numerator!=0;}
    if(!roll || roll>denominator)return std::unexpected(E::InvalidDecision);
    return roll<=numerator;
}
GatheringPin Pin(const ItemCatalog& c){return {c.Revision(),c.ContentHash()};}
}
std::expected<ToolActionPlan,GatheringError> PlanToolAction(const ItemCatalog& c,const ToolActionRequest& r)
{
    auto admitted=Admit(c,r.context,r.tool);if(!admitted)return std::unexpected(admitted.error());
    const auto* tool=std::get_if<ItemToolRow>(&(*admitted)->category);if(!tool)return std::unexpected(E::NotTool);
    if(r.action!=ToolAction::Practice && r.action!=ToolAction::Refine)return std::unexpected(E::WrongAction);
    if(r.tool.count!=1)return std::unexpected(E::InvalidStack);
    if(r.tool.equipped!=(r.action==ToolAction::Practice))return std::unexpected(E::InvalidSnapshot);
    const auto& levels=c.FindToolProfile(tool->profile)->levels;
    const auto& level=levels[r.tool.refinementLevel];
    if(r.trainingPoints>level.requiredPoints)return std::unexpected(E::InvalidState);
    ToolActionPlan plan{r,Pin(c),r.tool.refinementLevel,r.trainingPoints,false};
    const bool maximum=std::size_t(r.tool.refinementLevel)+1==levels.size();
    if(r.action==ToolAction::Practice)
    {
        if(maximum || r.trainingPoints==level.requiredPoints)
        {if(r.recordedRoll)return std::unexpected(E::InvalidDecision);return plan;}
        auto decision=Decision(level.practiceNumerator,level.practiceDenominator,r.recordedRoll);
        if(!decision)return std::unexpected(decision.error());
        plan.success=*decision;if(plan.success)++plan.afterPoints; // Bounds above prove no uint32 overflow.
    }
    else
    {
        if(maximum)return std::unexpected(E::AtMaximum);
        if(r.trainingPoints<level.requiredPoints)return std::unexpected(E::NotReady);
        auto decision=Decision(level.refineChanceBp,10000,r.recordedRoll);if(!decision)return std::unexpected(decision.error());
        plan.success=*decision;
        if(plan.success){++plan.afterLevel;plan.afterPoints=0;}
        else if(level.failure==ToolFailure::ResetAtLevel){plan.afterLevel=*level.failureLevel;plan.afterPoints=0;}
        else plan.afterPoints-=static_cast<std::uint32_t>((std::uint64_t{r.trainingPoints}*level.failureLossBp)/10000);
    }
    return plan;
}
std::expected<FishProcessPlan,GatheringError> PlanFishProcess(const ItemCatalog& c,const FishProcessRequest& r,const LootDraw& draw)
{
    auto admitted=Admit(c,r.context,r.fish);if(!admitted)return std::unexpected(admitted.error());
    const auto* fish=std::get_if<ItemFishingRow>(&(*admitted)->category);if(!fish)return std::unexpected(E::NotFishingItem);
    if(r.fish.equipped || r.fish.refinementLevel)return std::unexpected(E::InvalidState);
    FishProcessPlan plan{r,Pin(c),0,{}};
    if(r.action==FishAction::Use && fish->kind==FishingKind::LiveFish)
    {
        if(!draw)return std::unexpected(E::InvalidDecision);
        const auto* profile=c.FindLoot(*fish->useLootProfile);auto picked=PickLoot(*profile,draw);
        if(!picked)return std::unexpected(E::InvalidDecision);
        for(auto index:*picked){const auto& reward=profile->Data().entries[index];plan.yields.push_back({*reward.item,static_cast<std::uint32_t>(reward.amount)});}
        plan.consumeUnits=1;
    }
    else if(r.action==FishAction::Grill && fish->kind==FishingKind::DeadFish && fish->grillResult)
    {
        if(!r.campfireAvailable)return std::unexpected(E::CampfireRequired);
        plan.consumeUnits=r.fish.count;plan.yields.push_back({*fish->grillResult,r.fish.count});
    }
    else return std::unexpected(E::WrongAction);
    return plan;
}
std::expected<BaitPlan,GatheringError> PlanApplyBait(const ItemCatalog& c,const BaitRequest& r)
{
    auto bait=Admit(c,r.context,r.bait);if(!bait)return std::unexpected(bait.error());
    auto rod=Admit(c,r.context,r.rod);if(!rod)return std::unexpected(rod.error());
    if(r.bait.instanceId==r.rod.instanceId || r.bait.equipped || !r.rod.equipped || r.rod.count!=1 || r.bait.refinementLevel || r.previousBaitPower>INT32_MAX)
        return std::unexpected(E::InvalidSnapshot);
    if(r.fishingActive)return std::unexpected(E::FishingActive);
    const auto* fish=std::get_if<ItemFishingRow>(&(*bait)->category);const auto* tool=std::get_if<ItemToolRow>(&(*rod)->category);
    if(!fish || fish->kind!=FishingKind::Bait || !tool || tool->kind!=ToolKind::Rod)return std::unexpected(E::WrongAction);
    return BaitPlan{r,Pin(c),fish->baitPower,1}; // Replaces the old bait; never adds powers.
}
}
