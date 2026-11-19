#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>
namespace DungeonUi {
inline bool Parse(std::string_view text,unsigned& value){if(text.empty()||text.size()>10)return false;auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);return error==std::errc{}&&end==text.data()+text.size();}
inline std::uint64_t Add(std::uint64_t a,std::uint64_t b){return a+std::min(b,std::numeric_limits<std::uint64_t>::max()-a);}
// Display evaluation only; never alters entry rules, drops, or currency.
inline unsigned Grade(unsigned elapsed,unsigned limit,unsigned deaths){if(!limit)return deaths==0?3:deaths<=2?2:1;return elapsed<=limit/2&&deaths==0?3:elapsed<=limit*3ULL/4&&deaths<=2?2:1;}
inline bool EntryOpen(unsigned status,unsigned level,unsigned minimum,unsigned maximum,std::uint64_t wait,std::uint64_t now){return status==0&&level>=minimum&&level<=maximum&&wait<=now;}
}
