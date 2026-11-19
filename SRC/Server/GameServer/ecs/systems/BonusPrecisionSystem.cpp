#include "../../core/stdafx.h"
#include "PointSystem.hpp"
#include "PointRouter.hpp"
#include "../../entity/char.h"
#include "PlayerRuntimeSystem.hpp"
#include "../Registry.hpp"
#include "../components/character_stats_components.hpp"
#include "../../core/constants.h"
#include "../../network/desc.h"
#include <common/Headers/GC_Packet.h>
#include <cmath>
#include <limits>

namespace ecs::PointSystem {
namespace {
// Source bonuses have three decimals. Nine internal decimals also preserve
// percentage-derived bonuses without quantizing each addition back to .001.
constexpr int64_t kBonusScale = 1000000000;
bool ToRaw(double value, int64_t& raw) {
    if (!std::isfinite(value)) return false;
    const long double scaled = static_cast<long double>(value) * kBonusScale;
    if (scaled <= static_cast<long double>(std::numeric_limits<int64_t>::min()) ||
        scaled >= static_cast<long double>(std::numeric_limits<int64_t>::max())) return false;
    // Three-decimal SQL/literal sources must not inherit binary-double noise,
    // including large flat bonuses. Accumulate their exact integer thousandths.
    const long double sourceMilli = std::round(static_cast<long double>(value) * 1000);
    const long double source = sourceMilli / 1000;
    const long double tolerance = std::max(1e-12L,
        std::abs(static_cast<long double>(value)) * std::numeric_limits<double>::epsilon() * 2);
    if (std::abs(static_cast<long double>(value) - source) <= tolerance &&
        sourceMilli <= std::numeric_limits<int64_t>::max() / 1000000 &&
        sourceMilli >= std::numeric_limits<int64_t>::min() / 1000000) {
        raw = static_cast<int64_t>(sourceMilli) * 1000000;
        return true;
    }
    raw = static_cast<int64_t>(std::round(scaled));
    return true;
}
int64_t Whole(entt::entity e, uint8_t type) {
    if (type == POINT_MAX_HP || type == POINT_MAX_SP) {
        const auto* stats = g_registry.try_get<CharacterStatsComponent>(e);
        return stats ? stats->points[type] : 0;
    }
    return Get(e, type);
}
struct WriteGuard {
    entt::entity e;
    bool previousWrite;
    double previousValue;
    int previousEntryType;
    int previousActiveType;
    ~WriteGuard() {
        if (g_registry.valid(e))
            if (auto* stats = g_registry.try_get<CharacterStatsComponent>(e)) {
                stats->precisionWrite = previousWrite;
                stats->bonusPreviousValue = previousValue;
                stats->precisionEntryType = previousEntryType;
                stats->precisionActiveType = previousActiveType;
            }
    }
};
void SendOne(entt::entity e, uint8_t type) {
    auto* desc = ecs::PlayerRuntime::GetDesc(e);
    if (!desc) return;
    TPacketGCBonusPoint packet {};
    packet.header = HEADER_GC_BONUS_POINT;
    packet.type = type;
    packet.enabled = IsPrecisionPoint(type) && type != POINT_MAX_HP && type != POINT_MAX_SP;
    packet.raw = GetPreciseRaw(e, type);
    desc->Packet(&packet, sizeof(packet));
}
}
bool IsPrecisionPoint(uint8_t type) {
    if (type == POINT_NONE || type >= POINT_MAX_NUM || type == POINT_POLYMORPH || type == POINT_MOUNT ||
        type == POINT_STAT || type == POINT_STAT_RESET_COUNT || type == POINT_SKILL || type == POINT_SUB_SKILL ||
        type == POINT_HORSE_SKILL || type == POINT_LEVEL_STEP) return false;
    if (type == POINT_MAX_HP || type == POINT_MAX_SP) return true;
    return ecs::PointRouter::g_pointMap[type].source == ecs::PointRouter::PointSource::SRC_INSTANT_ARRAY;
}

int64_t GetPreciseRaw(entt::entity e, uint8_t type) {
    if (!g_registry.valid(e) || type >= POINT_MAX_NUM) return 0;
    const auto* stats = g_registry.try_get<CharacterStatsComponent>(e);
    if (!stats) return 0;
    // HP/SP remain final, integral resources on the wire, not flat modifiers.
    const int64_t whole = Get(e, type);
    if (whole > std::numeric_limits<int64_t>::max() / kBonusScale ||
        whole < std::numeric_limits<int64_t>::min() / kBonusScale) return 0;
    const int64_t fraction = type == POINT_MAX_HP || type == POINT_MAX_SP ? 0 : stats->bonusFractionRaw[type];
    const int64_t base = whole * kBonusScale;
    if ((fraction > 0 && base > std::numeric_limits<int64_t>::max() - fraction) ||
        (fraction < 0 && base < std::numeric_limits<int64_t>::min() - fraction)) return 0;
    return base + fraction;
}
double GetPrecise(entt::entity e, uint8_t type) {
    if (!IsPrecisionPoint(type) || type == POINT_MAX_HP || type == POINT_MAX_SP) return static_cast<double>(Get(e, type));
    return static_cast<double>(GetPreciseRaw(e, type)) / kBonusScale;
}
void ChangePrecise(entt::entity e, uint8_t type, double amount, bool bAmount, bool bBroadcast) {
    if (!g_registry.valid(e) || type >= POINT_MAX_NUM || !std::isfinite(amount)) return;
    auto* stats = g_registry.try_get<CharacterStatsComponent>(e);
    if (!stats) return;
    if (!IsPrecisionPoint(type)) {
        if (amount <= static_cast<double>(std::numeric_limits<int64_t>::min()) ||
            amount >= static_cast<double>(std::numeric_limits<int64_t>::max())) return;
        Change(e, type, static_cast<int64_t>(amount), bAmount, bBroadcast);
        return;
    }
    const int64_t whole = Whole(e, type);
    if (whole > std::numeric_limits<int64_t>::max() / kBonusScale ||
        whole < std::numeric_limits<int64_t>::min() / kBonusScale) return;
    const int64_t base = whole * kBonusScale;
    const int64_t fraction = stats->bonusFractionRaw[type];
    if ((fraction > 0 && base > std::numeric_limits<int64_t>::max() - fraction) ||
        (fraction < 0 && base < std::numeric_limits<int64_t>::min() - fraction)) return;
    const int64_t current = base + fraction;
    int64_t delta;
    if (!ToRaw(amount, delta) ||
        (delta > 0 && current > std::numeric_limits<int64_t>::max() - delta) ||
        (delta < 0 && current < std::numeric_limits<int64_t>::min() - delta)) return;
    int64_t next = current + delta;
    switch (type) {
    case POINT_MALL_EXPBONUS: case POINT_MALL_ITEMBONUS: case POINT_MALL_GOLDBONUS:
    case POINT_MALL_ATTBONUS: case POINT_MALL_DEFBONUS: case POINT_MELEE_MAGIC_ATT_BONUS_PER:
        next = std::min(next, 100 * kBonusScale); break;
    case POINT_EXP_DOUBLE_BONUS: case POINT_GOLD_DOUBLE_BONUS: case POINT_ITEM_DROP_BONUS: case POINT_POTION_BONUS:
        next = std::min(next, 254 * kBonusScale); break;
    default: break;
    }
    {
        WriteGuard guard {e, stats->precisionWrite, stats->bonusPreviousValue, stats->precisionEntryType, stats->precisionActiveType};
        stats->bonusPreviousValue = static_cast<double>(current) / kBonusScale;
        stats->bonusFractionRaw[type] = static_cast<int32_t>(next % kBonusScale);
        stats->precisionWrite = true;
        stats->precisionEntryType = type;
        stats->precisionActiveType = type;
        Change(e, type, next / kBonusScale - whole, bAmount, bBroadcast);
    }
    // Change can invoke gameplay callbacks and destroy the owner.
    if (!g_registry.valid(e) || !g_registry.all_of<CharacterStatsComponent>(e)) return;
    SendOne(e, type);
}
bool SetPrecise(entt::entity e, uint8_t type, double value) {
    if (!g_registry.valid(e) || type >= POINT_MAX_NUM || !std::isfinite(value)) return false;
    if (!IsPrecisionPoint(type)) {
        if (value <= static_cast<double>(std::numeric_limits<int64_t>::min()) ||
            value >= static_cast<double>(std::numeric_limits<int64_t>::max())) return false;
        return Set(e, type, static_cast<int64_t>(value));
    }
    int64_t raw;
    if (!ToRaw(value, raw)) return false;
    if (!Set(e, type, raw / kBonusScale) || !g_registry.valid(e)) return false;
    auto* stats = g_registry.try_get<CharacterStatsComponent>(e);
    if (!stats) return false;
    stats->bonusFractionRaw[type] = static_cast<int32_t>(raw % kBonusScale);
    return true;
}
void SendPrecisionPoints(entt::entity e) {
    if (!g_registry.valid(e) || !g_registry.all_of<CharacterStatsComponent>(e)) return;
    auto* desc = ecs::PlayerRuntime::GetDesc(e);
    if (!desc) return;
    TPacketGCBonusPoints packet {};
    packet.header = HEADER_GC_BONUS_POINTS;
    for (size_t i = 0; i < 255; ++i) {
        packet.enabled[i] = IsPrecisionPoint(static_cast<uint8_t>(i)) && i != POINT_MAX_HP && i != POINT_MAX_SP;
        if (packet.enabled[i]) packet.raw[i] = GetPreciseRaw(e, static_cast<uint8_t>(i));
    }
    desc->Packet(&packet, sizeof(packet));
}
} // namespace ecs::PointSystem
