#pragma once
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>

namespace auriga::atlas {
inline constexpr std::uint64_t Fee=1'000'000;
inline constexpr int CooldownSeconds=10;
inline bool ParseCoordinate(std::string_view text,std::int32_t& value)
{
    if(text.empty())return false;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    return parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && value>=0;
}
inline bool Destination(std::int32_t x,std::int32_t y,std::int32_t baseX,std::int32_t baseY,
    std::int32_t endX,std::int32_t endY,std::int32_t& globalX,std::int32_t& globalY)
{
    const std::int64_t gx=std::int64_t(baseX)+std::int64_t(x)*100;
    const std::int64_t gy=std::int64_t(baseY)+std::int64_t(y)*100;
    if(x<0 || y<0 || gx<baseX || gy<baseY || gx>=endX || gy>=endY ||
       gx>std::numeric_limits<std::int32_t>::max() || gy>std::numeric_limits<std::int32_t>::max())return false;
    globalX=static_cast<std::int32_t>(gx);globalY=static_cast<std::int32_t>(gy);return true;
}
}
