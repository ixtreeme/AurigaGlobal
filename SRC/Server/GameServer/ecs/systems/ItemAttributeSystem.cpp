#include "../../stdafx.h"
#include "ItemSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "../Registry.hpp"
#include "../components/inventory_components.hpp"
#include "../detail/ItemAttributeRules.hpp"
#include "../../constants.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../char.h"
#include "../../shop.h"
#include "../../log.h"
#include "../../utils.h"
#include "../../../common/stole_length.h"
#include <Core/Logging.hpp>

namespace ItemSystem {
namespace {
namespace rules = ecs::item_attributes;
using Attributes = decltype(ecs::ItemAttributes::attrs);

// Registry-scoped retirement queue. Versioned handles, never item pointers.
// Reserve before committing, so the commit itself cannot allocate or signal.
struct PendingConsumptions {
    struct Entry { entt::entity item; bool failureLogged { false }; };
    std::vector<Entry> items;
    bool processing { false };
};

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

bool IsOwnedAttributeTarget(entt::entity character, entt::entity item)
{
    if (!ecs::PlayerRuntime::IsPC(character) || !CanModifyOwnedAttributes(item) ||
        GetItemOwner(item) != character)
        return false;
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return location && GetItem(character, TItemPos(location->window, location->cell)) == item;
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
    return CanModifyOwnedAttributes(item) && item != material &&
        CanConsumeOwnedItem(GetItemOwner(item), material, amount);
}

bool SetItemAttributesEcs(entt::entity item, const ecs::ItemAttributes& attributes)
{
    if (!AttributesOf(item))
        return false;
    Commit(item, attributes.attrs, "SET_FORCE_ATTR");
    return true;
}

bool CanConsumeOwnedItem(entt::entity owner, entt::entity material, uint32_t amount)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || !IsValidItem(material) ||
        amount == 0 || IsItemConsumptionPending(material) || IsItemEquipped(material) ||
        IsItemExchanging(material) || IsItemLocked(material))
        return false;
    const auto* stack = g_registry.try_get<ecs::ItemCount>(material);
    if (!stack || stack->count <= 0 || static_cast<uint32_t>(stack->count) < amount)
        return false;
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

bool IsItemConsumptionPending(entt::entity item)
{
    const auto* pending = g_registry.ctx().find<PendingConsumptions>();
    return IsValidItem(item) && pending &&
        std::any_of(pending->items.begin(), pending->items.end(),
            [item](const auto& entry) { return entry.item == item; });
}

void PublishItemCount(entt::entity item)
{
    if (!IsValidItem(item) || IsItemConsumptionPending(item))
        return;
    SaveItem(item);
    if (IsValidItem(item) && !IsItemConsumptionPending(item))
        ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
}

void ProcessPendingItemConsumptions()
{
    auto* pending = g_registry.ctx().find<PendingConsumptions>();
    if (!pending || pending->processing || pending->items.empty())
        return;
    // Keep membership visible during callbacks, including recursive processing.
    pending->processing = true;
    struct ProcessingGuard {
        PendingConsumptions& pending;
        ~ProcessingGuard() { pending.processing = false; }
    } guard {*pending};
    // Process only the entries present on entry. Callbacks may append another
    // committed batch; use indices, not iterators invalidated by that append.
    size_t index = 0;
    for (size_t remaining = pending->items.size(); remaining > 0; --remaining)
    {
        const auto item = pending->items[index].item;
        if (IsValidItem(item) && GetItemCount(item) == 0)
            DestroyItemEntityEcs(item, "COMMITTED_ITEM_COST");
        // A failed cleanup keeps the zero stack retired and can be retried by
        // the item-manager tick. Never restore/recreate a consumed item.
        if (!IsValidItem(item))
            pending->items.erase(pending->items.begin() + index);
        else
        {
            if (!pending->items[index].failureLogged)
            {
                pending->items[index].failureLogged = true;
                LOG_ERROR("Committed item consumption cleanup pending: item {} entity {} count {}",
                    GetItemID(item), entt::to_integral(item), GetItemCount(item));
            }
            ++index;
        }
    }
}

bool SetItemAttributesWithItemCosts(entt::entity owner, entt::entity target,
    const ecs::ItemAttributes& attributes, std::span<const ItemCost> costs)
{
    constexpr size_t maxCosts = 64;
    if (costs.empty() || costs.size() > maxCosts ||
        !IsOwnedAttributeTarget(owner, target) || !CanConsumeOwnedItem(owner, target))
        return false;
    const auto desired = attributes.attrs;
    if (std::any_of(desired.begin(), desired.end(), [](const auto& attr) { return attr.bType >= MAX_APPLY_NUM; }))
        return false;
    struct PreparedCost { entt::entity item; int remaining; };
    std::array<PreparedCost, maxCosts> prepared {};
    size_t depleted = 0;
    for (size_t i = 0; i < costs.size(); ++i)
    {
        const auto cost = costs[i];
        if (cost.item == target || !CanConsumeOwnedItem(owner, cost.item, cost.amount))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (prepared[j].item == cost.item)
                return false;
        const int remaining = g_registry.get<ecs::ItemCount>(cost.item).count - static_cast<int>(cost.amount);
        prepared[i] = {cost.item, remaining};
        depleted += remaining == 0;
    }
    auto* pending = g_registry.ctx().find<PendingConsumptions>();
    if (!pending)
        pending = &g_registry.ctx().emplace<PendingConsumptions>();
    if (depleted > pending->items.max_size() - pending->items.size())
        return false;
    pending->items.reserve(pending->items.size() + depleted);
    const auto old = g_registry.get<ecs::ItemAttributes>(target).attrs;

    // Commit point: only writes to existing trivial components and reserved
    // storage. No registry signals, allocator, callback, save or network call.
    for (size_t i = 0; i < costs.size(); ++i)
    {
        g_registry.get<ecs::ItemCount>(prepared[i].item).count = prepared[i].remaining;
        if (prepared[i].remaining == 0)
            pending->items.push_back({prepared[i].item});
    }
    g_registry.get<ecs::ItemAttributes>(target).attrs = desired;

    // Everything observable from here on sees the complete committed state.
    // Reentrant callbacks may consume/move/delete entities; do not rewrite a
    // saved snapshot over their newer state or report a committed debit failed.
    SaveItem(target);
    if (IsValidItem(target) && !IsItemConsumptionPending(target))
        ecs::ItemNetworkSystem::SendItemUpdate(g_registry, target);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM && IsValidItem(target); ++i)
        if (desired[i].bType != 0 &&
            (desired[i].bType != old[i].bType || desired[i].sValue != old[i].sValue))
            LogAttribute(target, i, desired[i], "SET_FORCE_ATTR");
    for (size_t i = 0; i < costs.size(); ++i)
        if (prepared[i].remaining > 0)
            PublishItemCount(prepared[i].item);
    ProcessPendingItemConsumptions();
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

#ifdef ENABLE_STOLE_COSTUME
bool EnchantStoleWithItemCost(entt::entity character, entt::entity item, entt::entity material)
{
    if (!IsOwnedAttributeTarget(character, item) || GetItemType(item) != ITEM_COSTUME ||
        GetItemSubType(item) != COSTUME_STOLE || !CanPayItemAttributeCost(item, material) ||
        GetItemType(material) != ITEM_USE || GetItemSubType(material) != USE_ENCHANT_STOLE)
        return false;
    // Validate before narrowing: a negative proto grade must not wrap to a
    // high uint8_t grade. Positive grades above four retain the legacy cap.
    const int32_t grade = GetItemValue(item, 0);
    if (grade < 1)
        return false;
    constexpr int maxGrade = 4;
    static_assert(std::size(stoleInfoTable) == MAX_ATTR && MAX_ATTR <= ITEM_ATTRIBUTE_MAX_NUM);
    static_assert(std::size(stoleInfoTable[0]) == 1 + maxGrade * MAX_VAR_ATTR);
    const int lastVariant = std::min(grade, maxGrade) * MAX_VAR_ATTR;
    auto attrs = AttributesOf(item)->attrs;
    for (int i = 0; i < MAX_ATTR; ++i) {
        const int type = stoleInfoTable[i][0];
        const int value = stoleInfoTable[i][number(lastVariant - MAX_VAR_ATTR + 1, lastVariant)];
        if (type <= 0 || type >= MAX_APPLY_NUM || type > UINT8_MAX ||
            value < INT16_MIN || value > INT16_MAX)
            return false;
        attrs[i] = {static_cast<uint8_t>(type), static_cast<int16_t>(value)};
    }
    if (!ConsumeItemEcs(material))
        return false;
    // All six bonuses change together; the seventh slot is not part of this
    // operation. No material/component pointer survives the debit.
    Commit(item, attrs, "SET_FORCE_ATTR");
    return true;
}
#endif

short GetItemLockedAttr(entt::entity item)
{
    const auto* locked = IsValidItem(item) ? g_registry.try_get<ecs::ItemLockedAttribute>(item) : nullptr;
    return locked ? locked->index : -1;
}

#ifdef ATTR_LOCK
void SetItemLockedAttr(entt::entity item, short index)
{
    if (!IsValidItem(item))
        return;
    g_registry.emplace_or_replace<ecs::ItemLockedAttribute>(item, ecs::ItemLockedAttribute{index});
    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
    SaveItem(item);
}

AttributeLockResult UseItemAttributeLock(entt::entity character, entt::entity item, entt::entity material)
{
    using Result = AttributeLockResult;
    if (!IsOwnedAttributeTarget(character, item) || GetItemType(item) == ITEM_COSTUME ||
        GetItemType(item) == ITEM_DS)
        return Result::InvalidTarget;
    if (!CanPayItemAttributeCost(item, material) || GetItemType(material) != ITEM_USE)
        return Result::InvalidMaterial;
    const auto operation = GetItemSubType(material);
    const int current = GetItemLockedAttr(item);
    const auto attrs = AttributesOf(item)->attrs;
    std::array<int, ITEM_ATTRIBUTE_NORM_NUM> candidates{};
    int count = 0;
    short next = -1;
    switch (operation) {
        case USE_ADD_ATTRIBUTE_LOCK:
            if (GetItemWearFlags(item) & WEARABLE_PENDANT)
                return Result::InvalidTarget;
            if (rules::Count(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM) != ITEM_ATTRIBUTE_NORM_NUM)
                return Result::NotEnoughAttributes;
            if (current != -1)
                return Result::AlreadyLocked;
            for (int i = 0; i < ITEM_ATTRIBUTE_NORM_NUM; ++i)
                candidates[count++] = i;
            break;

        case USE_CHANGE_ATTRIBUTE_LOCK:
            if (current == -1)
                return Result::NotLocked;
            if (current < 0 || current >= ITEM_ATTRIBUTE_NORM_NUM || attrs[current].bType == 0)
                return Result::InvalidLock;
            for (int i = 0; i < ITEM_ATTRIBUTE_NORM_NUM; ++i)
                if (i != current && attrs[i].bType != 0)
                    candidates[count++] = i;
            if (count == 0)
                return Result::NoAlternative;
            break;

        case USE_DELETE_ATTRIBUTE_LOCK:
            if (current == -1)
                return Result::NotLocked;
            // Removing a malformed/out-of-range lock also repairs old data.
            break;

        default:
            return Result::InvalidMaterial;
    }
    // One bounded draw, including when only one alternative remains. Unlike
    // retrying rand() until it changes, this never loops on the current slot.
    if (count > 0)
        next = static_cast<short>(candidates[number(0, count - 1)]);
    if (!ConsumeItemEcs(material))
        return Result::Failed;
    SetItemLockedAttr(item, next);
    return Result::Success;
}
#endif

#ifdef ENABLE_ATTR_COSTUMES
bool SelectCostumeAttributeToRemove(entt::entity character, std::string_view slot)
{
    if (!ecs::PlayerRuntime::IsPC(character))
        return false;
    auto& selection = g_registry.get_or_emplace<ecs::CostumeAttributeSelection>(character);
    // Do not let atoi turn garbage into slot zero, or retain a prior valid
    // choice after a malformed request. Only the two client options are valid.
    selection.rareSlot = slot == "0" ? 0 : slot == "1" ? 1 : -1;
    return selection.rareSlot >= 0;
}

CostumeAttributeResult UseCostumeAttributeItem(entt::entity character,
    entt::entity item, entt::entity material)
{
    using Result = CostumeAttributeResult;
    if (!IsOwnedAttributeTarget(character, item) || GetItemType(item) != ITEM_COSTUME ||
        GetItemAttributeSetIndex(item) < 0)
        return Result::InvalidTarget;
    const auto subtype = GetItemSubType(item);
    if (subtype != COSTUME_BODY && subtype != COSTUME_HAIR && subtype != COSTUME_WEAPON)
        return Result::InvalidTarget;
    if (!CanPayItemAttributeCost(item, material) || GetItemType(material) != ITEM_USE)
        return Result::InvalidMaterial;

    auto attrs = AttributesOf(item)->attrs;
    const char* action = "SET_FORCE_ATTR";
    switch (GetItemSubType(material)) {
        case USE_CHANGE_ATTR_COSTUME:
            if (rules::Count(attrs, 0, ITEM_ATTRIBUTE_NORM_NUM) == 0)
                return Result::NoAttributes;
            if (!PrepareReroll(item, attrs, nullptr))
                return Result::Failed;
            action = "SET_ATTR";
            break;

        case USE_ADD_ATTR_COSTUME1:
        case USE_ADD_ATTR_COSTUME2: {
            const int slot = rules::FindEmpty(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END);
            if (slot < 0)
                return Result::SlotsFull;
            const auto* sockets = g_registry.try_get<ecs::ItemSockets>(material);
            if (!sockets)
                return Result::InvalidMaterial;
            const int32_t type = sockets->sockets[0];
            const int32_t value = sockets->sockets[1];
            // Validate before narrowing the socket payload into an attribute.
            if (type <= 0 || type >= MAX_APPLY_NUM || type > UINT8_MAX ||
                value == 0 || value < INT16_MIN || value > INT16_MAX)
                return Result::InvalidMaterial;
            if (rules::Has(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END, type))
                return Result::DuplicateAttribute;
            attrs[slot] = {static_cast<uint8_t>(type), static_cast<int16_t>(value)};
            break;
        }

        case USE_REMOVE_ATTR_COSTUME: {
            if (rules::Count(attrs, ITEM_ATTRIBUTE_RARE_START, ITEM_ATTRIBUTE_RARE_END) == 0)
                return Result::NoRareAttributes;
            const auto* selection = g_registry.try_get<ecs::CostumeAttributeSelection>(character);
            const int selected = selection ? selection->rareSlot : 0;
            if (selected < 0 || selected >= ITEM_ATTRIBUTE_RARE_END - ITEM_ATTRIBUTE_RARE_START)
                return Result::InvalidSelection;
            const int slot = ITEM_ATTRIBUTE_RARE_START + selected;
            if (attrs[slot].bType == 0)
                return Result::NoRareAttributes;
            // Compact only rare slots, and publish the completed result once.
            for (int i = slot; i + 1 < ITEM_ATTRIBUTE_RARE_END; ++i)
                attrs[i] = attrs[i + 1];
            attrs[ITEM_ATTRIBUTE_RARE_END - 1] = {};
            break;
        }

        default:
            return Result::InvalidMaterial;
    }
    if (!ConsumeItemEcs(material))
        return Result::Failed;
    Commit(item, attrs, action);
    return Result::Success;
}
#endif

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
