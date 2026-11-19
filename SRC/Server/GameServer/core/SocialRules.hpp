#pragma once
#include <string_view>
#include <cstddef>
namespace auriga::social {
inline constexpr std::size_t NameLimit=24,PendingLimit=64,ContactLimit=256;
inline bool ValidName(std::string_view name){if(name.empty()||name.size()>NameLimit)return false;for(unsigned char c:name)if(c<=32||c==127)return false;return true;}
}
