#pragma once

#include <array>

namespace LostCastleLayout
{
struct Point { int x; int y; };
struct Tile { Point start; Point end; };

inline constexpr int MapWidth = 512;
inline constexpr int MapHeight = 1024;
inline constexpr int MapIndex = 380;
inline constexpr int PrivateMin = MapIndex * 10000;
inline constexpr int PrivateMax = PrivateMin + 10000;
constexpr bool IsPrivateMap(int mapIndex)
{
    return mapIndex >= PrivateMin && mapIndex < PrivateMax;
}
inline constexpr Point Entrance{281, 112};
inline constexpr Point KeyRoomEntrance{296, 380};
inline constexpr Point Statue{296, 385};
inline constexpr Point TileRoomEntrance{396, 375};
inline constexpr Point Totem{396, 379};
inline constexpr Point Exit{396, 608};
inline constexpr std::array<Tile, 4> Rooms{{
    {{244, 86}, {348, 174}},
    {{232, 354}, {347, 492}},
    {{348, 352}, {447, 478}},
    {{380, 478}, {412, 620}},
}};
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

constexpr bool Contains(const Tile& rect, Point point)
{
    return point.x >= rect.start.x && point.x < rect.end.x && point.y >= rect.start.y && point.y < rect.end.y;
}

constexpr bool Walkable(Point point)
{
    for (const auto& room : Rooms)
        if (Contains(room, point))
            return true;
    return false;
}

constexpr bool Fits(int width, int height)
{
    for (const auto point : {Entrance, KeyRoomEntrance, Statue, TileRoomEntrance, Totem, Exit})
        if (!Inside(point, width, height) || !Walkable(point))
            return false;
    for (const auto& room : Rooms)
        if (!Inside(room.start, width, height) || !Inside(room.end, width, height))
            return false;
    for (const auto& tile : Tiles)
        if (!Inside(tile.start, width, height) || !Inside(tile.end, width, height)
            || tile.start.x >= tile.end.x || tile.start.y >= tile.end.y)
            return false;
    return true;
}

static_assert(Fits(MapWidth, MapHeight));
}
