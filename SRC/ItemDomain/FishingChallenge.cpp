#include <item/FishingChallenge.hpp>
#include <array>
#include <algorithm>
namespace auriga::item
{
namespace
{
using P=std::pair<std::int32_t,std::int32_t>;
constexpr std::array<P,6> left{{{560,360},{283,1250},{243,790},{523,1820},{850,930},{2027,1257}}};
constexpr std::array<P,6> right{{{2440,1700},{2230,1130},{2300,1770},{1673,897},{2220,1417},{2530,1520}}};
std::uint32_t Sqrt(std::uint32_t n){std::uint32_t a=0,b=4096;while(a+1<b){auto m=(a+b)/2;if(m*m<=n)a=m;else b=m;}return a;}
P Point(std::uint32_t seed,unsigned n){const auto i=(seed%6+n/2)%6;return n%2 ? right[i] : left[i];}
std::uint32_t Distance(P a,P b){auto x=a.first-b.first,y=a.second-b.second;return Sqrt(static_cast<std::uint32_t>(x*x+y*y));}
}
std::pair<std::int32_t,std::int32_t> FishingPosition(std::uint32_t seed,std::uint32_t elapsed) noexcept
{
 std::array<std::uint32_t,12> lengths{};std::uint32_t total=0;for(unsigned i=0;i<12;++i){lengths[i]=Distance(Point(seed,i),Point(seed,(i+1)%12));total+=lengths[i];}
 auto distance=static_cast<std::uint32_t>((std::uint64_t{elapsed}*1800/1000)%total);
 for(unsigned i=0;i<12;++i){if(distance>=lengths[i]){distance-=lengths[i];continue;}auto a=Point(seed,i),b=Point(seed,(i+1)%12);return {a.first+static_cast<std::int32_t>(std::int64_t(b.first-a.first)*distance/lengths[i]),a.second+static_cast<std::int32_t>(std::int64_t(b.second-a.second)*distance/lengths[i])};}return Point(seed,0);
}
bool FishingInGoal(std::uint32_t seed,std::uint32_t elapsed) noexcept
{auto [x,y]=FishingPosition(seed,elapsed);return x>=740 && x<=1750 && y>=500 && y<=1410;}
FishingChallenge ApplyFishingInput(FishingChallenge s,std::uint32_t elapsed,std::uint32_t sample,std::uint32_t sequence,bool fishClick) noexcept
{
 if(!sequence || sequence>1024 || sequence!=s.sequence+1){s.input=FishingInput::BadSequence;return s;}
 s.sequence=sequence;
 if(FishingWon(s) || FishingExpired(s,elapsed)){s.input=FishingInput::Expired;return s;}
 if((elapsed>sample ? elapsed-sample : sample-elapsed)>250 || sample>=s.durationMs || (s.lastAtMs!=UINT32_MAX && sample<s.lastSampleMs)){s.input=FishingInput::BadTime;return s;}
 if(s.lastAtMs!=UINT32_MAX && (elapsed<s.lastAtMs || elapsed-s.lastAtMs<1000)){s.input=FishingInput::TooSoon;return s;}
 s.lastAtMs=elapsed;s.lastSampleMs=sample;
 if(fishClick && FishingInGoal(s.seed,sample)){++s.hits;s.input=FishingInput::Hit;}
 else{if(s.misses<120)++s.misses;s.input=FishingInput::Miss;}return s;
}
}
