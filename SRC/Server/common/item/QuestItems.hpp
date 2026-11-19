#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace auriga::item
{
enum class BuiltinQuestItem : std::uint8_t {
    None, Courage, TeleportRing, Cleanse, Invisibility, WhiteFlag, CourageCombined,
    FireworkRandom, Firework1, Firework2, Firework3, Firework4, Firework5, Firework6, Firework7, ChinaFirework, SpinningTop,
    RestoreHpFull, RestoreSpFull, RestoreStaminaFull, SelfPoison, PolymorphCancel, SpouseWarp,
    Campfire, RecallTown, RecallMemory, RecallTownRestricted, RecallMemoryRestricted, RuneFragment, RuneFloorKey, PrivateShopBundle, PrivateShopSilk,
};
constexpr BuiltinQuestItem BuiltinQuestHandler(std::string_view group) noexcept
{
    return group=="utility.courage" ? BuiltinQuestItem::Courage :
        group=="utility.teleport_ring" ? BuiltinQuestItem::TeleportRing :
        group=="utility.cleanse" ? BuiltinQuestItem::Cleanse :
        group=="utility.invisibility" ? BuiltinQuestItem::Invisibility :
        group=="utility.white_flag" ? BuiltinQuestItem::WhiteFlag :
        group=="utility.courage_combined" ? BuiltinQuestItem::CourageCombined :
        group=="utility.firework_random" ? BuiltinQuestItem::FireworkRandom :
        group=="utility.firework_1" ? BuiltinQuestItem::Firework1 :
        group=="utility.firework_2" ? BuiltinQuestItem::Firework2 :
        group=="utility.firework_3" ? BuiltinQuestItem::Firework3 :
        group=="utility.firework_4" ? BuiltinQuestItem::Firework4 :
        group=="utility.firework_5" ? BuiltinQuestItem::Firework5 :
        group=="utility.firework_6" ? BuiltinQuestItem::Firework6 :
        group=="utility.firework_7" ? BuiltinQuestItem::Firework7 :
        group=="utility.china_firework" ? BuiltinQuestItem::ChinaFirework :
        group=="utility.spinning_top" ? BuiltinQuestItem::SpinningTop :
        group=="utility.restore_hp_full" ? BuiltinQuestItem::RestoreHpFull :
        group=="utility.restore_sp_full" ? BuiltinQuestItem::RestoreSpFull :
        group=="utility.restore_stamina_full" ? BuiltinQuestItem::RestoreStaminaFull :
        group=="utility.self_poison" ? BuiltinQuestItem::SelfPoison :
        group=="utility.polymorph_cancel" ? BuiltinQuestItem::PolymorphCancel :
        group=="utility.spouse_warp" ? BuiltinQuestItem::SpouseWarp :
        group=="utility.campfire" ? BuiltinQuestItem::Campfire :
        group=="utility.recall_town" ? BuiltinQuestItem::RecallTown :
        group=="utility.recall_memory" ? BuiltinQuestItem::RecallMemory :
        group=="utility.recall_town_restricted" ? BuiltinQuestItem::RecallTownRestricted :
        group=="utility.recall_memory_restricted" ? BuiltinQuestItem::RecallMemoryRestricted :
        group=="dungeon.rune.fragment" ? BuiltinQuestItem::RuneFragment :
        group=="dungeon.rune.floor_key" ? BuiltinQuestItem::RuneFloorKey :
        group=="shop.bundle" ? BuiltinQuestItem::PrivateShopBundle :
        group=="shop.silk_bundle" ? BuiltinQuestItem::PrivateShopSilk : BuiltinQuestItem::None;
}
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
