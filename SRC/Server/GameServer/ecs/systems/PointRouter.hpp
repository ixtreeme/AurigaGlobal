#pragma once

#include <array>
#include <cstdint>

#include "../PointSemantic.hpp"

namespace ecs::PointRouter {

enum class PointSource : uint8_t {
    SRC_NONE,
    SRC_GOLD,
    SRC_HP_CURRENT,
    SRC_HP_MAX,
    SRC_SP_CURRENT,
    SRC_SP_MAX,
    SRC_STAMINA_CURRENT,
    SRC_STAMINA_MAX,
    SRC_LEVEL,
    SRC_EXP,
    SRC_NEXT_EXP_COMPUTED,
    SRC_INSTANT_ARRAY,
    SRC_REAL_ARRAY,
    SRC_LEGACY_ONLY,
};

struct PointMapping {
    PointSource source;
    uint8_t index;
    bool readable_in_real;
};

constexpr PointMapping MakeMapping(PointSource source, uint8_t index = 0, bool readableInReal = false)
{
    return { source, index, readableInReal };
}

constexpr std::array<PointMapping, POINT_MAX_NUM> BuildPointMap()
{
    std::array<PointMapping, POINT_MAX_NUM> map {};

    for (uint32_t i = 0; i < POINT_MAX_NUM; ++i) {
        map[i] = MakeMapping(PointSource::SRC_INSTANT_ARRAY, static_cast<uint8_t>(i), true);
    }

    map[POINT_NONE] = MakeMapping(PointSource::SRC_NONE);
    map[POINT_LEVEL] = MakeMapping(PointSource::SRC_LEVEL, 0, true);
    map[POINT_EXP] = MakeMapping(PointSource::SRC_EXP);
    map[POINT_NEXT_EXP] = MakeMapping(PointSource::SRC_NEXT_EXP_COMPUTED);
    map[POINT_HP] = MakeMapping(PointSource::SRC_HP_CURRENT);
    map[POINT_MAX_HP] = MakeMapping(PointSource::SRC_HP_MAX);
    map[POINT_SP] = MakeMapping(PointSource::SRC_SP_CURRENT);
    map[POINT_MAX_SP] = MakeMapping(PointSource::SRC_SP_MAX);
    map[POINT_STAMINA] = MakeMapping(PointSource::SRC_STAMINA_CURRENT);
    map[POINT_MAX_STAMINA] = MakeMapping(PointSource::SRC_STAMINA_MAX);
    map[POINT_GOLD] = MakeMapping(PointSource::SRC_GOLD);

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    map[POINT_INVEN] = MakeMapping(PointSource::SRC_LEGACY_ONLY);
#endif
#ifdef ENABLE_GAYA_SYSTEM
    map[POINT_GAYA] = MakeMapping(PointSource::SRC_LEGACY_ONLY);
#endif

    return map;
}

inline constexpr auto g_pointMap = BuildPointMap();

constexpr bool IsLegacyOnly(uint32_t pointType)
{
    return pointType < POINT_MAX_NUM
        && g_pointMap[pointType].source == PointSource::SRC_LEGACY_ONLY;
}

constexpr const char* DescribeSource(PointSource source)
{
    switch (source) {
    case PointSource::SRC_NONE:
        return "none";
    case PointSource::SRC_GOLD:
        return "gold";
    case PointSource::SRC_HP_CURRENT:
        return "hp_current";
    case PointSource::SRC_HP_MAX:
        return "hp_max";
    case PointSource::SRC_SP_CURRENT:
        return "sp_current";
    case PointSource::SRC_SP_MAX:
        return "sp_max";
    case PointSource::SRC_STAMINA_CURRENT:
        return "stamina_current";
    case PointSource::SRC_STAMINA_MAX:
        return "stamina_max";
    case PointSource::SRC_LEVEL:
        return "level";
    case PointSource::SRC_EXP:
        return "exp";
    case PointSource::SRC_NEXT_EXP_COMPUTED:
        return "next_exp_computed";
    case PointSource::SRC_INSTANT_ARRAY:
        return "instant_array";
    case PointSource::SRC_REAL_ARRAY:
        return "real_array";
    case PointSource::SRC_LEGACY_ONLY:
        return "legacy_only";
    default:
        return "unknown";
    }
}

} // namespace ecs::PointRouter
