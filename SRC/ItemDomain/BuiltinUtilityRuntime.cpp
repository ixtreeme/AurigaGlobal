#include <item/BuiltinUtilityRuntime.hpp>
#include <algorithm>
namespace auriga::item
{
std::expected<BuiltinUtilityPlan,QuestItemUseError> PrepareBuiltinUtility(const ItemCatalog& c,const PersistentItem& source,ItemOwner owner,const BuiltinUtilityFacts& facts)
{
    using E=QuestItemUseError;
    if(!facts.alive || facts.mapIndex<=0 || !PlainInventoryItem(source,owner) || !ValidatePersistentItem(c,source) || facts.use.item!=source.key || facts.use.stackUnits!=source.count || facts.characterClass>3 || facts.sex>1)return std::unexpected(E::Forbidden);
    const auto* entry=c.Find(source.key);const auto* row=entry ? std::get_if<ItemQuestRow>(&entry->category) : nullptr;const auto* policy=entry ? c.FindUsePolicy(entry->base.usePolicy) : nullptr;
    const auto handler=row ? BuiltinQuestHandler(row->groupCode) : BuiltinQuestItem::None;
    // Stateful world items have their own complete writer. The simple one-unit
    // utility API must never bypass a saved destination or UTC reuse deadline.
    if(handler>=BuiltinQuestItem::Campfire)return std::unexpected(E::HandlerUnavailable);
    if(handler==BuiltinQuestItem::None || !policy || !(policy->allowedClasses&(1u<<facts.characterClass)) || !(policy->allowedSexes&(1u<<facts.sex)))return std::unexpected(E::HandlerUnavailable);
    auto request=facts.use;request.handlerResolvedByServer=true;const auto admitted=PlanQuestItemUse(c,request);if(!admitted)return std::unexpected(admitted.error());
    if((handler==BuiltinQuestItem::Courage || handler==BuiltinQuestItem::CourageCombined) && facts.mapIndex==1)return std::unexpected(E::Forbidden);
    if((handler==BuiltinQuestItem::ChinaFirework || handler==BuiltinQuestItem::SpinningTop) && (facts.use.context==QuestItemContext::Arena || facts.pvpPotionsBlocked))return std::unexpected(E::Forbidden);
    BuiltinUtilityPlan plan{handler,static_cast<std::uint16_t>(handler==BuiltinQuestItem::TeleportRing || handler==BuiltinQuestItem::PolymorphCancel || handler==BuiltinQuestItem::SpouseWarp ? 0 : 1)};
    if(handler==BuiltinQuestItem::SpouseWarp){
        if(!facts.summonZoneAllowed || !facts.warpAllowed || !facts.marriageAvailable || facts.marriageArenaBlocked || facts.maximumSp>INT64_MAX || facts.currentSp>INT64_MAX)return std::unexpected(E::Forbidden);
        // Exact floor(maximum SP *30/100), without overflowing a signed pool.
        plan.spCost=facts.maximumSp/100*30+facts.maximumSp%100*30/100;
        if(facts.currentSp<plan.spCost)return std::unexpected(E::Forbidden);
    }
    if(handler==BuiltinQuestItem::Invisibility)plan.durationSeconds=300;
    if(handler==BuiltinQuestItem::FireworkRandom){plan.firstFirework=1;plan.lastFirework=6;}
    else if(handler>=BuiltinQuestItem::Firework1 && handler<=BuiltinQuestItem::Firework7)plan.firstFirework=plan.lastFirework=1+static_cast<unsigned>(handler)-static_cast<unsigned>(BuiltinQuestItem::Firework1);
    return plan;
}
std::expected<ItemCommit,ItemPersistenceError> MakeBuiltinUtilityCommit(const ItemCatalog& c,std::array<std::uint8_t,16> operation,ItemOwnerFence fence,
    std::span<const ItemStorageLayout> layouts,std::span<const PersistentItem> items,ItemInstanceId id,const BuiltinUtilityFacts& facts)
{
    const auto i=std::ranges::find(items,id,&PersistentItem::id);if(i==items.end())return std::unexpected(ItemPersistenceError::MissingItem);
    const auto plan=PrepareBuiltinUtility(c,*i,fence.owner,facts);if(!plan || !plan->consume)return std::unexpected(ItemPersistenceError::RequirementNotMet);
    return MakeConsumeCommit(c,operation,fence,layouts,items,id,plan->consume);
}
}
