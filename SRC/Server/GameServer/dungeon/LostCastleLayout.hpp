#pragma once

#include <array>

namespace LostCastleLayout
{
struct Point { int x; int y; };
struct Tile { Point start; Point end; };

inline constexpr int MapWidth = 512;
inline constexpr int MapHeight = 1024;
inline constexpr Point Entrance{281, 112};
inline constexpr Point KeyRoomEntrance{296, 380};
inline constexpr Point Statue{296, 385};
inline constexpr Point TileRoomEntrance{396, 375};
inline constexpr Point Totem{396, 379};
inline constexpr std::array<Tile, 10> Tiles{{
    {{380, 478}, {412, 490}},
    {{380, 490}, {412, 502}},
    {{380, 502}, {412, 514}},
    {{380, 514}, {412, 526}},
    {{380, 526}, {412, 538}},
    {{380, 538}, {412, 550}},
    {{380, 550}, {412, 562}},
    {{380, 562}, {412, 574}},
    {{380, 574}, {412, 586}},
    {{380, 586}, {412, 598}},
}};

constexpr bool Inside(Point point, int width = MapWidth, int height = MapHeight)
{
    return point.x >= 0 && point.x < width && point.y >= 0 && point.y < height;
}

constexpr Point WorldCell(Point point, int baseX, int baseY)
{
    return {baseX / 100 + point.x, baseY / 100 + point.y};
}

constexpr bool Fits(int width, int height)
{
    for (const auto point : {Entrance, KeyRoomEntrance, Statue, TileRoomEntrance, Totem})
        if (!Inside(point, width, height))
            return false;
    for (const auto& tile : Tiles)
        if (!Inside(tile.start, width, height) || !Inside(tile.end, width, height)
            || tile.start.x >= tile.end.x || tile.start.y >= tile.end.y)
            return false;
    return true;
}

static_assert(Fits(MapWidth, MapHeight));
}
