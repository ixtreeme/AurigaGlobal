#include "../../stdafx.h"
#include "ItemSystem.hpp"
#include "InventorySystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "SocialSystem.hpp"
#include "AffectSystem.hpp"
#include "ChatSystem.hpp"
#include "../Registry.hpp"
#include "../components/inventory_components.hpp"
#include "../components/item_proto_components.hpp"
#include "../detail/ItemAttributeRules.hpp"
#include "../../constants.h"
#include "../../config.h"
#include "../../desc.h"
#include "../../char.h"
#include "../../shop.h"
#include "../../log.h"
#include "../../utils.h"
#include "../../../common/stole_length.h"
#ifdef ENABLE_RUNE_SYSTEM
#include "../../../common/rune_length.h"
#endif
#include <Core/Logging.hpp>

namespace ItemSystem {
#ifdef ENABLE_RUNE_SYSTEM
int32_t GetRuneAttributeType(entt::entity item, int index)
{
    if (!IsRuneItem(item) || index < 0 || index >= RUNE_ATTR_EACH) return 0;
    return aApplyRuneInfo[(GetItemSubType(item) - RUNE_SLOT1) * RUNE_ATTR_EACH + index][0];
}

int32_t GetRuneAttributeValue(entt::entity item, int index, int32_t remainingTime)
{
    if (!IsRuneItem(item) || index < 0 || index >= RUNE_ATTR_EACH) return 0;
    const int onePercent = GetItemValue(item, 0) / 100;
    // The legacy calculation divided by zero for missing/sub-100 durations.
    if (onePercent <= 0) return 0;
    const int remaining = remainingTime / onePercent;
    const int tier = remaining >= 81 ? 7 : remaining >= 61 ? 6 : remaining >= 41 ? 5 :
        remaining >= 21 ? 4 : remaining >= 11 ? 3 : remaining >= 6 ? 2 : 1;
    return aApplyRuneInfo[(GetItemSubType(item) - RUNE_SLOT1) * RUNE_ATTR_EACH + index][tier];
}

bool InitializeRuneItem(entt::entity item)
{
    if (!IsValidItem(item) || !g_registry.all_of<ecs::ItemSockets, ecs::ItemAttributes>(item)) return false;
    if (GetItemType(item) == ITEM_USE && GetItemSubType(item) == USE_RUNE_PERC_CHARGE)
    {
        g_registry.get<ecs::ItemSockets>(item).sockets[0] = GetItemValue(item, 0);
        return true;
    }
    if (!IsRuneItem(item)) return true;
    if (GetItemValue(item, 0) < 100) return false;
    const int32_t time = g_registry.get<ecs::ItemSockets>(item).sockets[0];
    for (int index = 0; index < RUNE_ATTR_EACH; ++index)
    {
        const auto type = GetRuneAttributeType(item, index), value = GetRuneAttributeValue(item, index, time);
        if (type > 0 && type <= UINT8_MAX && value > 0 && value <= INT16_MAX)
            g_registry.get<ecs::ItemAttributes>(item).attrs[index] = {static_cast<uint8_t>(type), static_cast<int16_t>(value)};
    }
    return true;
}

namespace {
void LogAttribute(entt::entity item, int index, const TPlayerItemAttribute& attr, const char* action);

bool HasRuneState(entt::entity item)
{
    return IsRuneItem(item) && GetItemProto(item) &&
        g_registry.all_of<ecs::ItemSockets, ecs::ItemAttributes>(item) && GetItemSocket(item, 1) <= 1;
}

struct RuneContext {
    entt::entity item, owner;
    uint8_t subtype;
    bool worn;
    explicit RuneContext(entt::entity e, bool requireWear = true)
        : item(e), owner(GetItemOwner(e)), subtype(GetItemSubType(e)), worn(requireWear) {}

    bool Valid() const
    {
        if (!HasRuneState(item) || GetItemSubType(item) != subtype || GetItemOwner(item) != owner)
            return false;
        if (owner != entt::null && !g_registry.valid(owner)) return false;
        return !worn || (owner != entt::null && IsItemEquipped(item) &&
            GetWearItem(owner, WEAR_RUNE1 + subtype - RUNE_SLOT1) == item);
    }
};

// Shared by all seven slots. No component pointer is kept across services.
struct RuneOperations { std::vector<entt::entity> busy; };
class RuneOperation {
    std::array<entt::entity, 2> keys;
    bool entered = false;
public:
    explicit RuneOperation(const RuneContext& context) : keys{context.item, context.owner}
    {
        if (!context.Valid()) return;
        auto* state = g_registry.ctx().find<RuneOperations>();
        if (!state) state = &g_registry.ctx().emplace<RuneOperations>();
        for (const auto key : keys)
            if (key != entt::null && std::find(state->busy.begin(), state->busy.end(), key) != state->busy.end())
                return;
        state->busy.reserve(state->busy.size() + keys.size());
        for (const auto key : keys) if (key != entt::null) state->busy.push_back(key);
        entered = true;
    }
    RuneOperation(const RuneOperation&) = delete;
    RuneOperation& operator=(const RuneOperation&) = delete;
    ~RuneOperation()
    {
        if (entered)
            if (auto* state = g_registry.ctx().find<RuneOperations>())
                for (const auto key : keys) std::erase(state->busy, key);
    }
    explicit operator bool() const { return entered; }
};

bool PublishRune(const RuneContext& context)
{
    if (!context.Valid()) return false;
    SaveItem(context.item);
    if (!context.Valid()) return false;
    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, context.item);
    return context.Valid();
}

bool RuneMessage(const RuneContext& context, uint32_t message)
{
    if (!context.Valid()) return false;
#ifdef TEXTS_IMPROVEMENT
    if (context.owner != entt::null) {
        const std::string name = GetItemName(context.item, 0);
        ecs::ChatSystem::SendNew(context.owner, CHAT_TYPE_INFO, message, "%s", name.c_str());
    }
#endif
    return context.Valid();
}

bool SetRuneAffect(const RuneContext& context, uint32_t type, bool enabled)
{
    if (!context.Valid() || context.owner == entt::null) return false;
    const bool present = AffectSystem::FindAffect(context.owner, type) != nullptr;
    if (enabled && !present) {
        if (!AffectSystem::AddAffect(context.owner, type, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false))
            return false;
    } else if (!enabled && present) {
        AffectSystem::RemoveAffect(context.owner, type);
    }
    return context.Valid();
}

bool SetRuneActive(const RuneContext& context, bool active)
{
    if (!context.Valid()) return false;
    if (GetItemSocket(context.item, 1) == static_cast<uint32_t>(active)) return true;
    // Commit before point/packet callbacks: repeat calls cannot apply twice and
    // unequip sees the new active state. Generic point calculation stays shared.
    g_registry.get<ecs::ItemSockets>(context.item).sockets[1] = active;
    ModifyPoints(context.item, active);
    return context.Valid() && GetItemSocket(context.item, 1) == static_cast<uint32_t>(active) &&
        PublishRune(context);
}

entt::entity BonusRune(const RuneContext& source)
{
    if (!source.Valid() || source.owner == entt::null) return entt::null;
    const auto item = GetWearItem(source.owner, WEAR_RUNE7);
    const RuneContext bonus(item);
    return bonus.Valid() && bonus.owner == source.owner && bonus.subtype == RUNE_SLOT7 ? item : entt::null;
}

bool CanActivateRuneBonus(entt::entity owner)
{
    for (int index = 0; index < RUNE_SUBTYPES - 1; ++index) {
        const auto item = GetWearItem(owner, WEAR_RUNE1 + index);
        const RuneContext context(item);
        if (!context.Valid() || context.owner != owner || context.subtype != RUNE_SLOT1 + index ||
            GetItemSocket(item, 1) != 1) return false;
        const int step = GetItemValue(item, 0) / 100;
        const int remaining = g_registry.get<ecs::ItemSockets>(item).sockets[0];
        if (step <= 0 || remaining / step < 50) return false;
    }
    return true;
}

bool ActivateRuneBonusImpl(const RuneContext& source)
{
    const auto item = BonusRune(source);
    if (item == entt::null) return source.Valid(); // A bonus rune is optional.
    const RuneContext bonus(item);
    if (GetItemSocket(item, 1) == 1) return true;
    if (!CanActivateRuneBonus(source.owner))
        return SetRuneAffect(bonus, AFFECT_RUNE2, false) && source.Valid() &&
            SetRuneAffect(bonus, AFFECT_RUNE1, true) && source.Valid();
    if (!SetRuneAffect(bonus, AFFECT_RUNE1, false) || !source.Valid() ||
        !CanActivateRuneBonus(source.owner)) return false;
    if (!SetRuneAffect(bonus, AFFECT_RUNE2, true) || !source.Valid()) return false;
    // Affect publication can change another slot. Never activate from a stale snapshot.
    if (!CanActivateRuneBonus(source.owner)) {
        SetRuneAffect(bonus, AFFECT_RUNE2, false);
        return false;
    }
    return SetRuneActive(bonus, true) && source.Valid() && RuneMessage(bonus, 31) && source.Valid();
}

bool DeactivateRuneBonusImpl(const RuneContext& source)
{
    const auto item = BonusRune(source);
    if (item == entt::null) return source.Valid();
    const RuneContext bonus(item);
    if (GetItemSocket(item, 1) != 1) return true;
    return SetRuneAffect(bonus, AFFECT_RUNE2, false) && source.Valid() &&
        SetRuneActive(bonus, false) && source.Valid() && RuneMessage(bonus, 901) && source.Valid();
}

bool RefreshRuneAffect(const RuneContext& source)
{
    if (!source.Valid()) return false;
    bool enabled = false;
    for (int index = 0; index < RUNE_SUBTYPES - 1; ++index) {
        const auto item = GetWearItem(source.owner, WEAR_RUNE1 + index);
        // Preserve the old visual-affect rule: missing slots also keep RUNE1.
        if (!HasRuneState(item) || GetItemSocket(item, 1) != 0) { enabled = true; break; }
    }
    return SetRuneAffect(source, AFFECT_RUNE1, enabled);
}
} // namespace

bool ActivateRune(entt::entity item)
{
    const RuneContext context(item);
    RuneOperation operation(context);
    if (!operation || context.subtype == RUNE_SLOT7 || GetItemValue(item, 0) < 100) return false;
    if (g_registry.get<ecs::ItemSockets>(item).sockets[0] <= 0) {
        RuneMessage(context, 30);
        return false;
    }
    // Exhaustion stops the wear event. Charging an equipped rune must be able
    // to restart it before points are applied; allocation failure stays inactive.
    if (!StartTimerBasedOnWearExpireEventEcs(item) || !context.Valid()) return false;
    if (GetItemSocket(item, 1) == 1) return true;
    return SetRuneActive(context, true) && RuneMessage(context, 31) && ActivateRuneBonusImpl(context);
}

namespace {
bool DeactivateRuneImpl(const RuneContext& context)
{
    const auto item = context.item;
    if (!context.Valid()) return false;
    if (GetItemSocket(item, 1) == 0) return true;
    if (!DeactivateRuneBonusImpl(context)) return false;
    // The bonus itself was already disabled above; SetRuneActive is idempotent.
    return SetRuneActive(context, false) && RefreshRuneAffect(context) && RuneMessage(context, 32);
}
} // namespace

bool DeactivateRune(entt::entity item)
{
    const RuneContext context(item);
    RuneOperation operation(context);
    return operation && DeactivateRuneImpl(context);
}

bool ActivateRuneBonus(entt::entity item)
{
    const RuneContext context(item);
    RuneOperation operation(context);
    return operation && ActivateRuneBonusImpl(context);
}

bool DeactivateRuneBonus(entt::entity item)
{
    const RuneContext context(item);
    RuneOperation operation(context);
    return operation && DeactivateRuneBonusImpl(context);
}

namespace {
bool ChangeRuneAttributesImpl(const RuneContext& context, int32_t time)
{
    const auto item = context.item;
    if (!HasRuneState(item) || time < 0 || GetItemValue(item, 0) < 100) return false;
    const bool active = GetItemSocket(item, 1) == 1;
    if (!context.Valid()) return false;
    const auto old = g_registry.get<ecs::ItemAttributes>(item).attrs;
    auto next = old;
    bool changed = false;
    for (int index = 0; index < RUNE_ATTR_EACH; ++index) {
        const auto value = GetRuneAttributeValue(item, index, time);
        if (value < 0 || value > INT16_MAX) return false;
        next[index].sValue = static_cast<int16_t>(value);
        changed |= next[index].sValue != old[index].sValue;
    }
    if (!changed) return true;
    if (active) {
        g_registry.get<ecs::ItemSockets>(item).sockets[1] = 0;
        ModifyPoints(item, false);
        if (!context.Valid() || GetItemSocket(item, 1) != 0) return false;
        // Do not overwrite attributes changed by another callback.
        const auto current = g_registry.get<ecs::ItemAttributes>(item).attrs;
        for (int index = 0; index < ITEM_ATTRIBUTE_MAX_NUM; ++index)
            if (current[index].bType != old[index].bType || current[index].sValue != old[index].sValue) {
                PublishRune(context); // Persist the safely disabled state, not the stale snapshot.
                return false;
            }
    }
    g_registry.get<ecs::ItemAttributes>(item).attrs = next;
    if (active) {
        g_registry.get<ecs::ItemSockets>(item).sockets[1] = 1;
        ModifyPoints(item, true);
        if (!context.Valid() || GetItemSocket(item, 1) != 1) return false;
    }
    if (!PublishRune(context)) return false;
    for (int index = 0; index < RUNE_ATTR_EACH; ++index) {
        if (next[index].bType != 0 && next[index].sValue != old[index].sValue)
            LogAttribute(item, index, next[index], "SET_FORCE_ATTR");
        if (!context.Valid()) return false;
    }
    return true;
}
} // namespace

bool ChangeRuneAttributes(entt::entity item, int32_t time)
{
    const RuneContext context(item, GetItemSocket(item, 1) == 1);
    RuneOperation operation(context);
    return operation && ChangeRuneAttributesImpl(context, time);
}

int UpdateRuneWearTime(entt::entity item, int32_t elapsedSeconds)
{
    const RuneContext context(item);
    RuneOperation operation(context);
    if (!operation || elapsedSeconds < 0) return 0;
    const int current = g_registry.get<ecs::ItemSockets>(item).sockets[0];
    const int remaining = static_cast<int>(std::max<int64_t>(0, static_cast<int64_t>(current) - elapsedSeconds));
    if (remaining == 0) {
        g_registry.get<ecs::ItemSockets>(item).sockets[0] = 0;
        DeactivateRuneImpl(context);
        if (context.Valid()) PublishRune(context);
        return 0;
    }
    const int step = GetItemValue(item, 0) / 100;
    if (step <= 0) {
        DeactivateRuneImpl(context);
        return 0;
    }
    if (remaining / step < 50) {
        if (!DeactivateRuneBonusImpl(context) || !context.Valid()) return 0;
    }
    // Inactive runes and the seventh (bonus-only) rune retain their charge.
    if (context.subtype == RUNE_SLOT7 || GetItemSocket(item, 1) != 1)
        return std::min(60, remaining);
    g_registry.get<ecs::ItemSockets>(item).sockets[0] = remaining;
    if (!ChangeRuneAttributesImpl(context, remaining) || !context.Valid()) return 0;
    if (!PublishRune(context)) return 0;
    return std::min(60, remaining);
}
#endif
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

struct PreparedCosts {
    struct Entry { entt::entity item; int remaining; };
    std::array<Entry, 64> entries {};
    size_t size {};
    PendingConsumptions* pending {};

    bool Prepare(entt::entity owner, std::span<const ItemCost> costs, entt::entity excluded = entt::null)
    {
        if (costs.empty() || costs.size() > entries.size()) return false;
        size_t depleted = 0;
        for (size_t i = 0; i < costs.size(); ++i)
        {
            const auto cost = costs[i];
            if (cost.item == excluded || !CanConsumeOwnedItem(owner, cost.item, cost.amount, cost.storage))
                return false;
            for (size_t j = 0; j < i; ++j)
                if (entries[j].item == cost.item) return false;
            const int remaining = g_registry.get<ecs::ItemCount>(cost.item).count - static_cast<int>(cost.amount);
            entries[i] = {cost.item, remaining};
            depleted += remaining == 0;
        }
        pending = g_registry.ctx().find<PendingConsumptions>();
        if (!pending) pending = &g_registry.ctx().emplace<PendingConsumptions>();
        if (depleted > pending->items.max_size() - pending->items.size()) return false;
        pending->items.reserve(pending->items.size() + depleted);
        size = costs.size();
        return true;
    }

    // No signals, allocations or callbacks between validation and this commit.
    void Commit()
    {
        for (size_t i = 0; i < size; ++i)
        {
            g_registry.get<ecs::ItemCount>(entries[i].item).count = entries[i].remaining;
            if (entries[i].remaining == 0) pending->items.push_back({entries[i].item});
        }
    }

    void Publish() const
    {
        for (size_t i = 0; i < size; ++i)
            if (entries[i].remaining > 0) PublishItemCount(entries[i].item);
        ProcessPendingItemConsumptions();
    }
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

bool CanConsumeOwnedItem(entt::entity owner, entt::entity material, uint32_t amount, ItemCostStorage storage)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || !IsValidItem(material) ||
        !g_registry.all_of<ecs::ItemOwner, ecs::ItemLocation>(material) ||
        amount == 0 || IsItemConsumptionPending(material) || IsItemEquipped(material) ||
        IsItemExchanging(material) || IsItemLocked(material))
        return false;
    const auto* stack = g_registry.try_get<ecs::ItemCount>(material);
    if (!stack || stack->count <= 0 || static_cast<uint32_t>(stack->count) < amount)
        return false;
    if (GetItemOwner(material) != owner)
        return false;
    const uint8_t window = GetItemWindow(material);
    if (storage == ItemCostStorage::DragonSoulInventory)
    {
        if (window != DRAGON_SOUL_INVENTORY || !IsDragonSoulItem(material)) return false;
    }
    else if (storage != ItemCostStorage::Inventory || (window != INVENTORY
#ifdef ENABLE_EXTRA_INVENTORY
        && window != EXTRA_INVENTORY
#endif
    ))
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
    const auto publishable = [item] {
        return IsValidItem(item) && GetItemCount(item) > 0 && !IsItemConsumptionPending(item);
    };
    if (!publishable())
        return;
    SaveItem(item);
    if (publishable())
        ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
}

uint32_t GetItemCount(entt::entity item)
{
    if (const auto* count = g_registry.try_get<ecs::ItemCount>(item))
        return count->count > 0 ? static_cast<uint32_t>(count->count) : 0;
    return 0;
}

// Stack updates and batch cost retirement share the same publication and
// pending-item rules. The component must already exist: count writes must not
// invoke on_construct/on_update while committing a stack change.
bool SetItemCountEcs(entt::entity item, uint32_t count)
{
    if (!IsValidItem(item) || IsItemConsumptionPending(item) ||
        !g_registry.all_of<ecs::ItemCount>(item))
        return false;
    if (count == 0)
        return DestroyItemEntityEcs(item, "SET_ITEM_COUNT_ZERO");

    const int limit = GetItemType(item) == ITEM_ELK ? INT_MAX : g_bItemCountLimit;
    if (limit <= 0) return false;
    g_registry.get<ecs::ItemCount>(item).count =
        static_cast<int>(std::min(count, static_cast<uint32_t>(limit)));
    PublishItemCount(item);
    // True means this change committed, not that a callback kept the item alive.
    return true;
}

void SetItemCount(entt::entity item, uint32_t count)
{
    SetItemCountEcs(item, count);
}

bool AddItemCountEcs(entt::entity item, int delta)
{
    if (!IsValidItem(item) || IsItemConsumptionPending(item) ||
        !g_registry.all_of<ecs::ItemCount>(item))
        return false;
    const int64_t next = int64_t(GetItemCount(item)) + delta;
    return SetItemCountEcs(item, next > 0 ? static_cast<uint32_t>(next) : 0);
}

namespace {
bool InventoryStackPosition(entt::entity item)
{
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    if (!location) return false;
    if (location->window == INVENTORY) return location->cell < INVENTORY_MAX_NUM;
#ifdef ENABLE_EXTRA_INVENTORY
    if (location->window == EXTRA_INVENTORY) return location->cell < EXTRA_INVENTORY_MAX_NUM;
#endif
    return false;
}

bool DetachedStackSource(entt::entity item)
{
    if (!IsValidItem(item) || IsItemConsumptionPending(item) || GetItemCount(item) == 0 ||
        IsItemEquipped(item) || IsItemExchanging(item) || IsItemLocked(item)) return false;
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
    return owner && owner->owner == entt::null && owner->ownerPID == 0 && location &&
        location->window == RESERVED_WINDOW && location->cell == 0;
}

bool StackablePayload(entt::entity item, bool reward)
{
    if (!g_registry.all_of<ecs::ItemIdentity, ecs::ItemPrototypeMeta, ecs::ItemProtoRef,
        ecs::ItemFlags, ecs::ItemSockets, ecs::ItemAttributes>(item)) return false;
    const auto& meta = g_registry.get<ecs::ItemPrototypeMeta>(item);
    const auto& flags = g_registry.get<ecs::ItemFlags>(item);
    const auto& proto = g_registry.get<ecs::ItemProtoRef>(item);
    return meta.type != ITEM_NONE && meta.type != ITEM_ELK && meta.type != ITEM_DS && meta.type != ITEM_SPECIAL_DS &&
        !(proto.anti_flags & ITEM_ANTIFLAG_STACK) &&
        ((flags.flags & ITEM_FLAG_STACKABLE) || (reward && meta.type == ITEM_BLEND));
}

bool SameStackPayload(entt::entity source, entt::entity target, bool reward)
{
    if (!StackablePayload(source, reward) || !StackablePayload(target, reward)) return false;
    const auto& a = g_registry.get<ecs::ItemIdentity>(source);
    const auto& b = g_registry.get<ecs::ItemIdentity>(target);
    if (a.vnum != b.vnum || a.originalVnum != b.originalVnum || a.maskVnum != b.maskVnum ||
        a.sigVnum != b.sigVnum || a.specialGroup != b.specialGroup || a.transmutationVnum != b.transmutationVnum)
        return false;
    const auto& aType = g_registry.get<ecs::ItemPrototypeMeta>(source);
    const auto& bType = g_registry.get<ecs::ItemPrototypeMeta>(target);
    if (aType.type != bType.type || aType.subType != bType.subType ||
        g_registry.get<ecs::ItemFlags>(source).flags != g_registry.get<ecs::ItemFlags>(target).flags ||
        g_registry.get<ecs::ItemSockets>(source).sockets != g_registry.get<ecs::ItemSockets>(target).sockets ||
        GetItemLockedAttr(source) != GetItemLockedAttr(target)) return false;
    const auto& aAttrs = g_registry.get<ecs::ItemAttributes>(source).attrs;
    const auto& bAttrs = g_registry.get<ecs::ItemAttributes>(target).attrs;
    for (size_t i = 0; i < aAttrs.size(); ++i)
        if (aAttrs[i].bType != bAttrs[i].bType || aAttrs[i].sValue != bAttrs[i].sValue) return false;
    const auto* aExtra = g_registry.try_get<ecs::ItemExtraProtoRef>(source);
    const auto* bExtra = g_registry.try_get<ecs::ItemExtraProtoRef>(target);
    return (aExtra ? aExtra->proto : nullptr) == (bExtra ? bExtra->proto : nullptr);
}

struct StackDeliveries { std::set<entt::entity> active; };
}

StackMergeResult MergeItemStacksEcs(entt::entity owner, entt::entity source,
    entt::entity target, uint32_t amount, StackSource storage)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || source == target || g_bItemCountLimit <= 0 ||
        !IsValidItem(source) || !IsValidItem(target) || !InventoryStackPosition(target) ||
        !CanConsumeOwnedItem(owner, target)) return {};
    if (storage == StackSource::Inventory) {
        if (!InventoryStackPosition(source) || !CanConsumeOwnedItem(owner, source)) return {};
    } else if (storage != StackSource::DetachedReward || !DetachedStackSource(source)) return {};
    if (!SameStackPayload(source, target, storage == StackSource::DetachedReward)) return {};

    const uint32_t sourceCount = GetItemCount(source), targetCount = GetItemCount(target);
    const uint32_t limit = static_cast<uint32_t>(g_bItemCountLimit);
    if (amount > sourceCount || targetCount >= limit) return {};
    const uint32_t moved = std::min(amount ? amount : sourceCount, limit - targetCount);
    if (!moved) return {};
    const uint32_t remaining = sourceCount - moved;
    PendingConsumptions* pending = nullptr;
    if (remaining == 0) {
        pending = g_registry.ctx().find<PendingConsumptions>();
        if (!pending) pending = &g_registry.ctx().emplace<PendingConsumptions>();
        if (pending->items.size() == pending->items.max_size()) return {};
        pending->items.reserve(pending->items.size() + 1);
    }

    // Reserve before committing. No signals, allocations, saves, deletion or
    // packets can expose only one half of the transfer.
    g_registry.get<ecs::ItemCount>(source).count = static_cast<int>(remaining);
    g_registry.get<ecs::ItemCount>(target).count = static_cast<int>(targetCount + moved);
    if (pending) pending->items.push_back({source});

    PublishItemCount(target);
    if (remaining) PublishItemCount(source);
    ProcessPendingItemConsumptions();
    return {moved, remaining == 0};
}

entt::entity MergeItemIntoInventoryEcs(entt::entity owner, entt::entity item)
{
    if (!ecs::PlayerRuntime::IsPC(owner) || !DetachedStackSource(item)) return entt::null;
    auto* deliveries = g_registry.ctx().find<StackDeliveries>();
    if (!deliveries) deliveries = &g_registry.ctx().emplace<StackDeliveries>();
    if (!deliveries->active.insert(item).second) return entt::null;
    struct Guard {
        StackDeliveries& deliveries;
        entt::entity item;
        ~Guard() { deliveries.active.erase(item); }
    } guard {*deliveries, item};
    // Nonstackable equipment/DS rewards need no inventory scan. Keep this after
    // the delivery guard so a callback cannot bypass it by changing item flags.
    if (!StackablePayload(item, true)) return item;
    const uint8_t window =
#ifdef ENABLE_EXTRA_INVENTORY
        IsExtraItem(item) ? EXTRA_INVENTORY :
#endif
        INVENTORY;
    // No view, component pointer or iterator survives a publication callback.
    // Only initially anchored candidates are considered, in stable slot order.
    std::vector<std::pair<uint16_t, entt::entity>> candidates;
    for (const auto candidate : g_registry.view<ecs::ItemOwner, ecs::ItemLocation>()) {
        const auto& binding = g_registry.get<ecs::ItemOwner>(candidate);
        const auto& location = g_registry.get<ecs::ItemLocation>(candidate);
        if (candidate != item && binding.owner == owner && location.window == window &&
            InventoryStackPosition(candidate) && GetItem(owner, TItemPos(window, location.cell)) == candidate)
            candidates.emplace_back(location.cell, candidate);
    }
    std::sort(candidates.begin(), candidates.end());
    for (const auto [cell, candidate] : candidates) {
        if (!ecs::PlayerRuntime::IsPC(owner) || !DetachedStackSource(item)) return entt::null;
        if (!IsValidItem(candidate) || GetItemOwner(candidate) != owner ||
            GetItemWindow(candidate) != window || GetItemCell(candidate) != cell ||
            GetItem(owner, TItemPos(window, cell)) != candidate) continue;
        const auto result = MergeItemStacksEcs(owner, item, candidate, 0, StackSource::DetachedReward);
        if (result.sourceDepleted) {
            return ecs::PlayerRuntime::IsPC(owner) && IsValidItem(candidate) &&
                GetItemOwner(candidate) == owner && GetItemWindow(candidate) == window &&
                GetItemCell(candidate) == cell && GetItem(owner, TItemPos(window, cell)) == candidate &&
                GetItemCount(candidate) > 0 && !IsItemConsumptionPending(candidate) ? candidate : entt::null;
        }
    }
    return ecs::PlayerRuntime::IsPC(owner) && DetachedStackSource(item) ? item : entt::null;
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
    if (!IsOwnedAttributeTarget(owner, target) || !CanConsumeOwnedItem(owner, target))
        return false;
    const auto desired = attributes.attrs;
    if (std::any_of(desired.begin(), desired.end(), [](const auto& attr) { return attr.bType >= MAX_APPLY_NUM; }))
        return false;
    PreparedCosts prepared;
    if (!prepared.Prepare(owner, costs, target)) return false;
    const auto old = g_registry.get<ecs::ItemAttributes>(target).attrs;

    prepared.Commit();
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
    prepared.Publish();
    return true;
}

#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
bool ChangeItemHitDamageBonuses(entt::entity owner, entt::entity item, entt::entity scroll)
{
    if (!IsOwnedAttributeTarget(owner, item) || GetItemAttributeSetIndex(item) < 0 ||
        GetItemType(item) == ITEM_COSTUME || !CanConsumeOwnedItem(owner, scroll) ||
        GetItemVnum(scroll) != 70251)
        return false;

    auto desired = g_registry.get<ecs::ItemAttributes>(item);
    constexpr uint8_t types[] = {APPLY_NORMAL_HIT_DAMAGE_BONUS, APPLY_SKILL_DAMAGE_BONUS};
    int slots[2] = {-1, -1};
    for (int bonus = 0; bonus < 2; ++bonus)
    {
        for (int index = 0; index < ITEM_ATTRIBUTE_NORM_NUM; ++index)
            if (desired.attrs[index].bType == types[bonus])
            {
                // Malformed duplicates and locked bonuses must not be rewritten.
                if (slots[bonus] != -1 || index == LockedSlot(item))
                    return false;
                slots[bonus] = index;
            }
        if (slots[bonus] < 0)
        {
            if (HasNormal(item, desired.attrs, types[bonus], false))
                return false;
            slots[bonus] = rules::FindEmpty(desired.attrs, 0, ITEM_ATTRIBUTE_NORM_NUM, LockedSlot(item));
            if (slots[bonus] < 0)
                return false;
            desired.attrs[slots[bonus]].bType = types[bonus];
        }
    }

    const int skill = std::clamp(static_cast<int>(gauss_random(0, 5) + 0.5f), -30, 30);
    const int hit = -2 * skill + (abs(skill) <= 20
        ? abs(number(-8, 8) + number(-8, 8)) + number(1, 4) : number(1, 5));
    desired.attrs[slots[0]].sValue = static_cast<int16_t>(hit);
    desired.attrs[slots[1]].sValue = static_cast<int16_t>(skill);
    const ItemCost cost {scroll, 1};
    return SetItemAttributesWithItemCosts(owner, item, desired, std::span(&cost, 1));
}
#endif

bool ConsumeOwnedItemCosts(entt::entity owner, std::span<const ItemCost> costs)
{
    PreparedCosts prepared;
    if (!prepared.Prepare(owner, costs)) return false;
    prepared.Commit();
    prepared.Publish();
    return true;
}

#ifdef ENABLE_RUNE_SYSTEM
bool ChargeRune(entt::entity owner, entt::entity rune, entt::entity bottle)
{
    const RuneContext context(rune);
    RuneOperation operation(context);
    const auto allowed = [&] {
        return context.Valid() && context.owner == owner && context.subtype != RUNE_SLOT7 &&
            ecs::PlayerRuntime::IsPC(owner) && InventorySystem::CanHandleItems(owner) &&
            !IsItemLocked(rune) && !IsItemExchanging(rune) && !IsItemConsumptionPending(rune) &&
            CanConsumeOwnedItem(owner, bottle) && GetItemWindow(bottle) == INVENTORY &&
            GetItemCell(bottle) < INVENTORY_MAX_NUM &&
            GetItemType(bottle) == ITEM_USE && GetItemSubType(bottle) == USE_RUNE_PERC_CHARGE &&
            g_registry.all_of<ecs::ItemSockets>(bottle);
    };
    if (!operation || !allowed()) return false;

    const auto runeSockets = g_registry.get<ecs::ItemSockets>(rune).sockets;
    const auto runeAttributes = g_registry.get<ecs::ItemAttributes>(rune).attrs;
    const auto bottleSockets = g_registry.get<ecs::ItemSockets>(bottle).sockets;
    const auto bottleCount = GetItemCount(bottle);
    const auto bottleCell = GetItemCell(bottle);
    const auto bottleVnum = GetItemVnum(bottle);
    const int maxTime = GetItemValue(rune, 0), step = maxTime / 100;
    const int time = runeSockets[ITEM_SOCKET_REMAIN_SEC], available = bottleSockets[0];
    if (step <= 0 || time < 0 || available <= 0) return false;
    const int percent = time / step;
    if (percent >= 100) { RuneMessage(context, 33); return false; }
    const int used = std::min(100 - percent, available);
    const int remaining = available - used;
    // Non-multiple-of-100 durations retain the old percentage steps, but cannot
    // overflow or charge past the configured duration near INT32_MAX.
    const int chargedTime = static_cast<int>(std::min<int64_t>(maxTime, int64_t(time) + int64_t(step) * used));
    const bool split = bottleCount > 1 && remaining > 0;
    const auto sameRune = [&](int expectedTime) {
        if (!context.Valid() || GetItemValue(rune, 0) != maxTime) return false;
        auto expected = runeSockets;
        expected[ITEM_SOCKET_REMAIN_SEC] = expectedTime;
        if (g_registry.get<ecs::ItemSockets>(rune).sockets != expected) return false;
        const auto& current = g_registry.get<ecs::ItemAttributes>(rune).attrs;
        return std::equal(current.begin(), current.end(), runeAttributes.begin(),
            [](const auto& a, const auto& b) { return a.bType == b.bType && a.sValue == b.sValue; });
    };
    const auto unchanged = [&] {
        return allowed() && sameRune(time) && GetItemCount(bottle) == bottleCount &&
            GetItemCell(bottle) == bottleCell && GetItemVnum(bottle) == bottleVnum &&
            g_registry.get<ecs::ItemSockets>(bottle).sockets == bottleSockets;
    };

    PreparedCosts cost;
    bool committed = false;
    const auto commit = [&](entt::entity output) {
        if (!unchanged()) return false;
        if (split) {
            if (output == bottle || output == rune || !IsValidItem(output) ||
                !g_registry.all_of<ecs::ItemSockets>(output) || GetItemCount(output) != 1 ||
                GetItemOwner(output) != entt::null || GetItemVnum(output) != bottleVnum) return false;
        } else if (remaining == 0) {
            const ItemCost debit {bottle, 1};
            if (!cost.Prepare(owner, std::span(&debit, 1), rune)) return false;
        }
        // Existing components only: charge and payment commit before any service.
        if (split) g_registry.get<ecs::ItemSockets>(output).sockets[0] = remaining;
        else if (remaining == 0) cost.Commit();
        else g_registry.get<ecs::ItemSockets>(bottle).sockets[0] = remaining;
        g_registry.get<ecs::ItemSockets>(rune).sockets[ITEM_SOCKET_REMAIN_SEC] = chargedTime;
        committed = true;
        return true;
    };

    if (split) {
        const int cell = InventorySystem::GetEmptyInventory(owner, GetItemSize(bottle));
        if (cell < 0) {
#ifdef TEXTS_IMPROVEMENT
            if (context.Valid()) ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 366, "");
#endif
            return false;
        }
        if (cell >= INVENTORY_MAX_NUM || !InventorySystem::SplitItemWithCommit(owner, bottle, 1,
            TItemPos(INVENTORY, static_cast<uint16_t>(cell)), commit)) return false;
    } else if (!commit(entt::null)) return false;

    // Publication may retire, move or mutate any participant. Never replay the
    // debit or restore snapshots over that newer state. The runtime refresh uses
    // the already-held rune operation guard (public reentry stays blocked).
    if (sameRune(chargedTime)) {
        if (ChangeRuneAttributesImpl(context, chargedTime) && context.Valid()) {
            if (GetItemSocket(rune, 1) == 1 && chargedTime / step >= 50)
                ActivateRuneBonusImpl(context);
            if (context.Valid()) PublishRune(context);
        }
    }
    if (remaining == 0) cost.Publish(); // Zero stacks remain retired if cleanup must retry.
    else if (IsValidItem(bottle) && GetItemOwner(bottle) == owner) PublishItemCount(bottle);
#ifdef TEXTS_IMPROVEMENT
    if (context.Valid()) {
        const std::string name = GetItemName(rune, 0);
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 34, "%s#%d", name.c_str(), used);
    }
#endif
    return committed;
}
#endif

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
