#include "../../stdafx.h"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "../Registry.hpp"
#include "../detail/ItemAttributeRules.hpp"
#include "../../constants.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../char.h"
#include "../../shop.h"
#include "../../log.h"
#include "../../utils.h"

namespace ItemSystem {
namespace {
namespace rules = ecs::item_attributes;
using Attributes = decltype(ecs::ItemAttributes::attrs);

int Random(int low, int high) { return number(low, high); }

int LockedSlot(entt::entity item)
{
#ifdef ATTR_LOCK
    return GetItemLockedAttr(item);
#else
    return -1;
#endif
}

bool IsZodiacAttributeItemVnum(uint32_t vnum)
{
#ifdef DISABLE_ZODIAC_ATT
    if (vnum == 12314141)
        return true;
#else
    constexpr uint32_t ranges[][2] = {
        {19290, 19312}, {19490, 19512}, {19690, 19712}, {19890, 19912},
        {300, 319}, {1180, 1189}, {2200, 2209}, {3220, 3229},
        {5160, 5169}, {7300, 7309}, {1700, 1713}, {1720, 1733},
        {1740, 1753}, {1760, 1773}, {1780, 1793}, {1800, 1813}, {8500, 8839}
    };
    for (const auto& range : ranges)
        if (vnum >= range[0] && vnum <= range[1])
            return true;
    constexpr uint32_t singles[] = {
        329, 339, 349, 359, 369, 379, 389, 399, 1199, 1209, 1219, 1229,
        2219, 2229, 2239, 2249, 3239, 3249, 3259, 3269,
        5179, 5189, 5199, 5209, 7319, 7329, 7339, 7349
    };
    for (const uint32_t value : singles)
        if (vnum == value)
            return true;
#endif
    return false;
}

bool IgnoresBaseApplies(entt::entity item, bool allowZodiacException)
{
    if (allowZodiacException && IsZodiacAttributeItemVnum(GetItemVnum(item)))
        return true;
#ifdef ENABLE_PENDANT
    if ((GetItemType(item) == ITEM_ARMOR && GetItemSubType(item) == ARMOR_NUM_TYPES) ||
        (GetItemWearFlags(item) & WEARABLE_PENDANT))
        return true;
#endif
    return false;
}

bool HasNormal(entt::entity item, const Attributes& attrs, uint32_t type, bool allowZodiacException = true)
{
    if (rules::Has(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, type))
        return true;
    if (IgnoresBaseApplies(item, allowZodiacException))
        return false;
    if (const auto* proto = GetItemProto(item))
        for (const auto& apply : proto->aApplies)
            if (apply.bType == type)
                return true;
#if defined(ENABLE_ITEM_EXTRA_PROTO) && defined(ENABLE_NEW_EXTRA_BONUS)
    if (const auto* extra = GetItemExtraProto(item))
        for (const auto& apply : extra->ExtraBonus)
            if (apply.bType == type)
                return true;
#endif
    return false;
}

void LogAttribute(entt::entity item, int index, const TPlayerItemAttribute& attr, const char* action)
{
    auto* desc = ecs::PlayerRuntime::GetDesc(GetItemOwner(item));
    LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(
        index, attr.bType, attr.sValue, GetItemID(item), action, "",
        desc ? desc->GetHostName() : "", GetItemOriginalVnum(item)));
}

void Commit(entt::entity item, const Attributes& attrs, const char* action)
{
    const auto old = g_registry.get<ecs::ItemAttributes>(item).attrs;
    SetItemAttributes(item, attrs.data());
    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        if (attrs[i].bType != 0 &&
            (attrs[i].bType != old[i].bType || attrs[i].sValue != old[i].sValue))
            LogAttribute(item, i, attrs[i], action);
}

bool AddExplicit(entt::entity item, Attributes& attrs, uint8_t type, int16_t value)
{
    const int slot = rules::FindEmpty(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, LockedSlot(item));
    if (slot < 0 || type == 0 || value == 0 || HasNormal(item, attrs, type))
        return false;
    attrs[slot] = {type, value};
    return true;
}

bool RollNormal(entt::entity item, Attributes& attrs, const int* probabilities)
{
    const int set = GetItemAttributeSetIndex(item);
    const int slot = rules::FindEmpty(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, LockedSlot(item));
    if (set < 0 || slot < 0 || !probabilities)
        return false;
    const int level = rules::RollLevel(probabilities, ITEM_ATTRIBUTE_MAX_LEVEL, Random);
    if (level == 0)
        return false;
    const auto selected = rules::Select(g_map_itemAttr, set, true,
        [&](uint32_t type, const TItemAttrTable&) {
            return type > 0 && type < MAX_APPLY_NUM && type <= UINT8_MAX &&
                !HasNormal(item, attrs, type);
        }, Random);
    if (selected == g_map_itemAttr.end())
        return false;
    const auto& row = selected->second;
    int valueIndex = std::min(level, static_cast<int>(row.bMaxLevelBySet[set])) - 1;
#ifdef ENABLE_ATTR_COSTUMES
    if (GetItemType(item) == ITEM_COSTUME)
        valueIndex += ITEM_ATTRIBUTE_MAX_LEVEL;
#endif
    if (valueIndex < 0 || static_cast<size_t>(valueIndex) >= std::size(row.lValues))
        return false;
    return AddExplicit(item, attrs, static_cast<uint8_t>(selected->first),
        static_cast<int16_t>(row.lValues[valueIndex]));
}

bool RollRare(entt::entity item, Attributes& attrs)
{
    const int set = GetItemAttributeSetIndex(item);
    const int slot = rules::FindEmpty(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END);
    if (set < 0 || slot < 0)
        return false;
    const auto selected = rules::Select(g_map_itemRare, set, false,
        [&](uint32_t type, const TItemAttrTable& row) {
            return type < MAX_APPLY_NUM && row.dwApplyIndex > 0 && row.dwApplyIndex <= UINT8_MAX &&
                !rules::Has(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END, row.dwApplyIndex);
        }, Random);
    if (selected == g_map_itemRare.end())
        return false;
    const auto& row = selected->second;
    const int level = std::min(5, static_cast<int>(row.bMaxLevelBySet[set]));
    attrs[slot] = {static_cast<uint8_t>(row.dwApplyIndex), static_cast<int16_t>(row.lValues[level - 1])};
    return true;
}

void ApplyAddon(entt::entity item, Attributes& attrs)
{
    const int skill = std::clamp(static_cast<int>(gauss_random(0, 5) + 0.5f), -30, 30);
    const int hit = -2 * skill + (abs(skill) <= 20
        ? abs(number(-8, 8) + number(-8, 8)) + number(1, 4) : number(1, 5));
    rules::RemoveType(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, APPLY_SKILL_DAMAGE_BONUS, LockedSlot(item));
    rules::RemoveType(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, APPLY_NORMAL_HIT_DAMAGE_BONUS, LockedSlot(item));
    AddExplicit(item, attrs, APPLY_NORMAL_HIT_DAMAGE_BONUS, static_cast<int16_t>(hit));
    AddExplicit(item, attrs, APPLY_SKILL_DAMAGE_BONUS, static_cast<int16_t>(skill));
}

const ecs::ItemAttributes* AttributesOf(entt::entity item)
{
    return IsValidItem(item) ? g_registry.try_get<ecs::ItemAttributes>(item) : nullptr;
}

bool PrepareReroll(entt::entity item, Attributes& attrs, const int* probabilities)
{
    if (GetItemAttributeSetIndex(item) < 0)
        return false;
    const int count = rules::Count(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM);
    if (count == 0)
        return false;
    rules::Clear(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, LockedSlot(item));
    if (const auto* proto = GetItemProto(item); proto && proto->sAddonType)
        ApplyAddon(item, attrs);
    constexpr int defaults[ITEM_ATTRIBUTE_MAX_LEVEL] = {0, 10, 40, 35, 15};
    for (int i = rules::Count(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM); i < count; ++i)
        if (!RollNormal(item, attrs, probabilities ? probabilities : defaults))
            return false;
    return true;
}

bool CanModifyOwnedAttributes(entt::entity item)
{
    if (!AttributesOf(item) || IsItemEquipped(item) || IsItemExchanging(item) || IsItemLocked(item))
        return false;
    const entt::entity owner = GetItemOwner(item);
    return owner != entt::null && g_registry.valid(owner) && ecs::PlayerRuntime::IsPC(owner);
}
} // namespace

int GetItemAttributeSetIndex(entt::entity item)
{
    if (!IsValidItem(item))
        return -1;
    const auto type = GetItemType(item);
    const auto subType = GetItemSubType(item);
    if (type == ITEM_WEAPON)
        return subType == WEAPON_ARROW ? -1 : ATTRIBUTE_SET_WEAPON;
    if (type == ITEM_ARMOR) {
        switch (subType) {
            case ARMOR_BODY: return ATTRIBUTE_SET_BODY;
            case ARMOR_WRIST: return ATTRIBUTE_SET_WRIST;
            case ARMOR_FOOTS: return ATTRIBUTE_SET_FOOTS;
            case ARMOR_NECK: return ATTRIBUTE_SET_NECK;
            case ARMOR_HEAD: return ATTRIBUTE_SET_HEAD;
            case ARMOR_SHIELD: return ATTRIBUTE_SET_SHIELD;
            case ARMOR_EAR: return ATTRIBUTE_SET_EAR;
#if defined(ENABLE_PENDANT) && defined(ENABLE_NEW_BONUS_TALISMAN)
            case ARMOR_PENDANT: return ATTRIBUTE_SET_PENDANT;
#endif
        }
    }
#ifdef ENABLE_ATTR_COSTUMES
    if (type == ITEM_COSTUME) {
        switch (subType) {
            case COSTUME_BODY: return ATTRIBUTE_SET_COSTUME_BODY;
            case COSTUME_HAIR: return ATTRIBUTE_SET_COSTUME_HAIR;
            case COSTUME_WEAPON: return ATTRIBUTE_SET_COSTUME_WEAPON;
#ifdef ENABLE_STOLE_COSTUME
            case COSTUME_STOLE: return ATTRIBUTE_SET_COSTUME_STOLE;
#endif
        }
    }
#endif
    return -1;
}

bool HasItemAttribute(entt::entity item, uint8_t type)
{
    const auto* component = AttributesOf(item);
    // Preserve HasAttr's base-apply query; the Zodiac exception is for rolling.
    return component && HasNormal(item, component->attrs, type, false);
}

bool AddItemAttribute(entt::entity item, uint8_t type, int16_t value)
{
    const auto* component = AttributesOf(item);
    if (!component)
        return false;
    auto attrs = component->attrs;
    if (!AddExplicit(item, attrs, type, value))
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, int16_t value)
{
    const auto* component = AttributesOf(item);
    if (!component || index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM)
        return false;
    auto attrs = component->attrs;
    attrs[index] = {type, value};
    Commit(item, attrs, "SET_FORCE_ATTR");
    return true;
}

bool RemoveItemAttributeType(entt::entity item, uint8_t type)
{
    const auto* component = AttributesOf(item);
    if (!component || type == 0)
        return false;
    auto attrs = component->attrs;
    if (!rules::RemoveType(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, type, LockedSlot(item)))
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool ClearNormalItemAttributes(entt::entity item)
{
    const auto* component = AttributesOf(item);
    if (!component)
        return false;
    auto attrs = component->attrs;
    rules::Clear(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, LockedSlot(item));
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool ApplyItemAddon(entt::entity item, int /* addonType */)
{
    const auto* component = AttributesOf(item);
    if (!component)
        return false;
    auto attrs = component->attrs;
    ApplyAddon(item, attrs);
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool AddItemAttributeEcs(entt::entity item)
{
    const auto* component = AttributesOf(item);
    if (!component)
        return false;
    auto attrs = component->attrs;
    constexpr int probabilities[ITEM_ATTRIBUTE_MAX_LEVEL] = {40, 50, 10, 0, 0};
    if (!RollNormal(item, attrs, probabilities))
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool ChangeItemAttributeEcs(entt::entity item, const int* probabilities)
{
    const auto* component = AttributesOf(item);
    if (!component)
        return false;
    auto attrs = component->attrs;
    if (!PrepareReroll(item, attrs, probabilities))
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool CanPayItemAttributeCost(entt::entity item, entt::entity material, uint32_t amount)
{
    if (!CanModifyOwnedAttributes(item) || item == material || !IsValidItem(material) ||
        amount == 0 || IsItemEquipped(material) ||
        IsItemExchanging(material) || IsItemLocked(material))
        return false;
    const auto* stack = g_registry.try_get<ecs::ItemCount>(material);
    if (!stack || stack->count <= 0 || static_cast<uint32_t>(stack->count) < amount)
        return false;
    const entt::entity owner = GetItemOwner(item);
    if (GetItemOwner(material) != owner)
        return false;
    const uint8_t window = GetItemWindow(material);
    if (window != INVENTORY
#ifdef ENABLE_EXTRA_INVENTORY
        && window != EXTRA_INVENTORY
#endif
    )
        return false;
    if (GetItem(owner, TItemPos(window, GetItemCell(material))) != material)
        return false;
    if (auto* shop = ecs::SocialSystem::GetMyShop(owner); shop && shop->IsSellingItem(GetItemID(material)))
        return false;
    return true;
}

bool ChangeItemAttributeWithItemCost(entt::entity item, entt::entity material,
    uint32_t amount, const int* probabilities)
{
    if (!CanPayItemAttributeCost(item, material, amount))
        return false;
    auto attrs = AttributesOf(item)->attrs;
    if (!PrepareReroll(item, attrs, probabilities))
        return false;
    // No yield or user callback between payment validation and commit. The
    // material is distinct from the target; consuming its last unit is safe.
    if (!ConsumeItemEcs(material, amount))
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool ChangeItemAttributeWithGoldCost(entt::entity item, int64_t amount)
{
    if (!CanModifyOwnedAttributes(item) || amount <= 0)
        return false;
    const entt::entity owner = GetItemOwner(item);
    const int64_t gold = ecs::PointSystem::GetGold(owner);
    if (gold < amount)
        return false;
    auto attrs = AttributesOf(item)->attrs;
    if (!PrepareReroll(item, attrs, nullptr))
        return false;
    ecs::PointSystem::Change(owner, POINT_GOLD, -amount);
    if (ecs::PointSystem::GetGold(owner) != gold - amount)
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
}

bool ResetCostumeAttributesWithItemCost(entt::entity item, entt::entity material, uint32_t amount)
{
#ifdef ENABLE_ATTR_COSTUMES
    if (!CanPayItemAttributeCost(item, material, amount) || GetItemType(item) != ITEM_COSTUME ||
        GetItemAttributeSetIndex(item) < 0)
        return false;
    const uint8_t subtype = GetItemSubType(item);
    if (subtype != COSTUME_BODY && subtype != COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
        && subtype != COSTUME_WEAPON
#endif
    )
        return false;
    auto attrs = AttributesOf(item)->attrs;
    if (rules::Count(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM) == 0)
        return false;
    rules::Clear(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, LockedSlot(item));
    // These costumes receive three magic bonuses. All three must be prepared
    // before either the original bonuses or the reset item can be changed.
    if (!RollNormal(item, attrs, aiItemMagicAttributePercentHigh) ||
        !RollNormal(item, attrs, aiItemMagicAttributePercentLow) ||
        !RollNormal(item, attrs, aiItemMagicAttributePercentLow))
        return false;
    if (!ConsumeItemEcs(material, amount))
        return false;
    Commit(item, attrs, "SET_ATTR");
    return true;
#else
    return false;
#endif
}

bool AddItemRareAttributeEcs(entt::entity item)
{
    const auto* component = AttributesOf(item);
    if (!component)
        return false;
    auto attrs = component->attrs;
    if (!RollRare(item, attrs))
        return false;
    Commit(item, attrs, "SET_RARE");
    return true;
}

bool ChangeItemRareAttributeEcs(entt::entity item)
{
    const auto* component = AttributesOf(item);
    if (!component || GetItemAttributeSetIndex(item) < 0)
        return false;
    auto attrs = component->attrs;
    const int count = rules::Count(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END);
    if (count == 0)
        return false;
    rules::Clear(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END);
    for (int i = 0; i < count; ++i)
        if (!RollRare(item, attrs))
            return false;
    const entt::entity owner = GetItemOwner(item);
    if (owner != entt::null && ecs::PlayerRuntime::GetDesc(owner)) {
        LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLogEntity(
            owner, item, "SET_RARE_CHANGE", ""));
    } else {
        LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(
            0, 0, 0, GetItemID(item), "SET_RARE_CHANGE", "", "", GetItemOriginalVnum(item)));
    }
    Commit(item, attrs, "SET_RARE");
    return true;
}

bool AlterItemToMagicItem(entt::entity item)
{
    const auto* component = AttributesOf(item);
    if (!component || GetItemAttributeSetIndex(item) < 0)
        return false;
    int second = 0;
    int third = 0;
    switch (GetItemType(item)) {
        case ITEM_WEAPON: second = 20; third = 5; break;
        case ITEM_ARMOR: second = 10; third = GetItemSubType(item) == ARMOR_BODY ? 2 : 1; break;
#ifdef ENABLE_ATTR_COSTUMES
        case ITEM_COSTUME:
            if (GetItemSubType(item) == COSTUME_BODY || GetItemSubType(item) == COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
                || GetItemSubType(item) == COSTUME_WEAPON
#endif
            )
                second = third = 100;
            break;
#endif
    }
    if (second == 0 && third == 0)
        return false;
    auto attrs = component->attrs;
    bool changed = RollNormal(item, attrs, aiItemMagicAttributePercentHigh);
    if (number(1, 100) <= second)
        changed = RollNormal(item, attrs, aiItemMagicAttributePercentLow) || changed;
    if (number(1, 100) <= third)
        changed = RollNormal(item, attrs, aiItemMagicAttributePercentLow) || changed;
    if (changed)
        Commit(item, attrs, "SET_ATTR");
    return changed;
}

void AttrLog(entt::entity item)
{
    if (!IsValidItem(item))
        return;
    auto* desc = ecs::PlayerRuntime::GetDesc(GetItemOwner(item));
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        if (const int32_t value = GetItemSocket(item, i); value != 0) {
            LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(
                i, value, 0, GetItemID(item), "INFO_SOCKET", "",
                desc ? desc->GetHostName() : "", GetItemOriginalVnum(item)));
        }
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        if (const auto attr = GetItemAttribute(item, i); attr.bType != 0)
            LogAttribute(item, i, attr, "INFO_ATTR");
}

bool AttrLogEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;
    AttrLog(item);
    return true;
}

} // namespace ItemSystem
