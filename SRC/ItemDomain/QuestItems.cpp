#include <item/QuestItems.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"

namespace auriga::item
{
bool ValidQuestItem(const ItemQuestRow& row) noexcept
{
    if(row.key.Category()!=ItemCategory::Quest)return false;
    if(row.dispatch==QuestItemDispatch::Token)return row.groupCode.empty() && !row.allowedContexts && !row.allowObserver;
    if(row.dispatch!=QuestItemDispatch::FirstHandler && row.dispatch!=QuestItemDispatch::AllHandlers)return false;
    return row.allowedContexts && !(row.allowedContexts&~15u) && (row.groupCode.empty() || detail::ContentCode(row.groupCode));
}
std::expected<QuestItemDispatchPlan,QuestItemUseError> PlanQuestItemUse(const ItemCatalog& catalog,const QuestItemUseRequest& request)
{
    if(!request.admittedByServer)return std::unexpected(QuestItemUseError::Forbidden);
    const auto* entry=catalog.Find(request.item);
    if(!entry)return std::unexpected(QuestItemUseError::MissingItem);
    const auto* quest=std::get_if<ItemQuestRow>(&entry->category);
    if(!quest)return std::unexpected(QuestItemUseError::NotQuestItem);
    if(entry->base.retired)return std::unexpected(QuestItemUseError::RetiredItem);
    if(!request.stackUnits || request.stackUnits>entry->base.maxStack)return std::unexpected(QuestItemUseError::InvalidStack);
    if(quest->dispatch==QuestItemDispatch::Token)return std::unexpected(QuestItemUseError::NotUsable);
    switch(request.context)
    {
    case QuestItemContext::World:case QuestItemContext::Duel:case QuestItemContext::Arena:case QuestItemContext::GuildWar:break;
    default:return std::unexpected(QuestItemUseError::InvalidContext);
    }
    const auto* policy=catalog.FindUsePolicy(entry->base.usePolicy);
    if(!(quest->allowedContexts&static_cast<unsigned>(request.context)) || (request.observer && !quest->allowObserver) ||
       (request.inCombat && !policy->useDuringCombat))return std::unexpected(QuestItemUseError::Forbidden);
    if(request.characterLevel<entry->base.minimumLevel)return std::unexpected(QuestItemUseError::LevelTooLow);
    if(!request.handlerResolvedByServer)return std::unexpected(QuestItemUseError::HandlerUnavailable);
    return QuestItemDispatchPlan{quest->key,quest->dispatch,quest->groupCode,catalog.Revision(),catalog.ContentHash(),0};
}
} // namespace auriga::item
