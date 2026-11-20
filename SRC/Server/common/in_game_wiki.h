#pragma once

#include <cstdint>

#pragma pack(push, 1)

namespace CommonWikiData
{
    inline constexpr int MAX_REFINE_COUNT = 9;
    inline constexpr int REFINE_MATERIAL_MAX_NUM = 5;

    struct TRefineMaterialNew
    {
        std::uint32_t vnum = 0;
        std::int32_t count = 0;
    };

    struct TWikiRefineInfo
    {
        std::int32_t index = 0;
        TRefineMaterialNew materials[REFINE_MATERIAL_MAX_NUM]{};
        std::uint32_t mat_count = 0;
        std::int64_t price = 0;
    };

    static_assert(sizeof(TWikiRefineInfo) == 56);
}

namespace InGameWiki
{
    inline constexpr std::uint8_t HEADER_CG_WIKI_REFINES = 200;
    inline constexpr std::uint8_t HEADER_GC_WIKI_REFINES = 211;

    struct TCGWikiRefines
    {
        std::uint8_t header = HEADER_CG_WIKI_REFINES;
        std::uint8_t reservedMob = 0;
        std::uint32_t vnum = 0;
        std::uint64_t returnId = 0;
    };

    struct TGCWikiRefines
    {
        std::uint8_t header = HEADER_GC_WIKI_REFINES;
        std::uint16_t size = 0;
        std::uint8_t subheader = 0;
        std::uint8_t reservedCommon = 0;
        std::uint32_t refineCount = 0;
        std::uint32_t reservedChestCount = 0;
        std::uint32_t reservedOrigin = 0;
        std::uint32_t reservedOriginCount = 0;
        std::uint32_t vnum = 0;
        std::uint64_t returnId = 0;
    };

    static_assert(sizeof(TCGWikiRefines) == 14);
    static_assert(sizeof(TGCWikiRefines) == 33);
}

#pragma pack(pop)
