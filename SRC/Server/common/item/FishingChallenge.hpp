#pragma once
#include <cstdint>
#include <utility>
namespace auriga::item
{
// Original fish-box waypoints, fixed tenths of a pixel; 180 pixels/second.
std::pair<std::int32_t,std::int32_t> FishingPosition(std::uint32_t seed,std::uint32_t elapsedMs) noexcept;
bool FishingInGoal(std::uint32_t seed,std::uint32_t elapsedMs) noexcept;
enum class FishingInput:std::uint8_t { None,Hit,Miss,TooSoon,BadTime,BadSequence,Expired };
struct FishingChallenge
{
 std::uint32_t seed=0,durationMs=15000,sequence=0,lastAtMs=UINT32_MAX,lastSampleMs=0;
 std::uint8_t need=3,hits=0,misses=0;FishingInput input=FishingInput::None;
};
inline bool FishingWon(const FishingChallenge& s) noexcept {return s.hits>=s.need;}
inline bool FishingExpired(const FishingChallenge& s,std::uint32_t elapsed) noexcept
{return std::uint64_t{elapsed}+std::uint64_t{s.misses}*1000>=s.durationMs;}
FishingChallenge ApplyFishingInput(FishingChallenge,std::uint32_t serverElapsed,std::uint32_t sampleElapsed,std::uint32_t sequence,bool fishClick) noexcept;
struct FishingRuntimeView { FishingChallenge state;std::uint32_t elapsedMs=0;std::int32_t castX=0,castY=0; };
}
