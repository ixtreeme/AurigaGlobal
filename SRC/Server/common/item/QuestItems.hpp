#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <string>

namespace auriga::item
{
// A passive event/crafting token is distinct from an item that dispatches a
// quest-use intent. Neither mode automatically consumes an item or grants loot.
enum class QuestItemDispatch : std::uint8_t { Token = 1, FirstHandler = 2, AllHandlers = 3 };
enum class QuestItemContext : std::uint8_t { World = 1, Duel = 2, Arena = 4, GuildWar = 8 };
struct ItemQuestRow
{
    ItemKey key;
    QuestItemDispatch dispatch = QuestItemDispatch::Token;
    // Empty: dispatch by the typed ItemKey. Otherwise a compiled quest group
    // content code. This is not a legacy item/signature ID or executable path.
    std::string groupCode;
    std::uint8_t allowedContexts = 0;
    bool allowObserver = false;
};
bool ValidQuestItem(const ItemQuestRow&) noexcept;
class ItemCatalog;
struct QuestItemUseRequest
{
    ItemKey item;
    std::uint32_t stackUnits = 0;
    std::uint16_t characterLevel = 0;
    QuestItemContext context = QuestItemContext::World;
    bool observer = false, inCombat = false;
    bool admittedByServer = false;
    bool handlerResolvedByServer = false;
};
struct QuestItemDispatchPlan
{
    ItemKey item;
    QuestItemDispatch dispatch;
    std::string groupCode;
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash {};
    std::uint16_t unitsToConsume = 0;
};
enum class QuestItemUseError
{
    Forbidden, MissingItem, NotQuestItem, RetiredItem, InvalidStack,
    NotUsable, InvalidContext, LevelTooLow, HandlerUnavailable,
};
// Copies a dispatch intent, not a committed ItemConsumed/reward event. The
// owner must resolve the compiled handler, check class/sex/session/ownership,
// and bind the concrete instance revision before running it. Any resulting
// inventory/economy mutation uses the authoritative writer independently.
std::expected<QuestItemDispatchPlan,QuestItemUseError> PlanQuestItemUse(const ItemCatalog&,const QuestItemUseRequest&);
} // namespace auriga::item
