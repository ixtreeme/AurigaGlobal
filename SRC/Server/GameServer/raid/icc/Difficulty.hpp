#pragma once
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

namespace EclipseRaid::ICC {
// AzerothCore RAID_DIFFICULTY_* values / SpellDifficulty.dbc column order.
enum class Difficulty:uint8_t {Normal10,Normal25,Heroic10,Heroic25};
inline bool Heroic(Difficulty mode){return mode==Difficulty::Heroic10||mode==Difficulty::Heroic25;}
inline bool Raid25(Difficulty mode){return mode==Difficulty::Normal25||mode==Difficulty::Heroic25;}
inline uint32_t PlayerLimit(Difficulty mode){return Raid25(mode)?25:10;}
inline const char* ModeName(Difficulty mode){switch(mode){case Difficulty::Normal10:return "10_normal";case Difficulty::Normal25:return "25_normal";case Difficulty::Heroic10:return "10_heroic";default:return "25_heroic";}}
inline std::optional<Difficulty> ParseMode(std::string_view name){
 for(auto mode:{Difficulty::Normal10,Difficulty::Normal25,Difficulty::Heroic10,Difficulty::Heroic25})if(name==ModeName(mode))return mode;
 return std::nullopt;
}
template<class T>T RaidMode(Difficulty d,T n10,T n25,T h10,T h25){switch(d){case Difficulty::Normal10:return n10;case Difficulty::Normal25:return n25;case Difficulty::Heroic10:return h10;default:return h25;}}
}
