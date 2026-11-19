#include <item/BookLearning.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace auriga::item
{
bool ValidBookCharacter(const BookCharacter& c) noexcept
{
    if(c.hair && c.hair->lastDyeLevel>255)return false;
    if(c.alignment && *c.alignment>50000000)return false;
    if(c.statResetCredits && *c.statResetCredits>INT16_MAX)return false;
    if(c.progression && (!c.progression->level || c.progression->level>255 || c.progression->step>3 || c.progression->statPoints>INT16_MAX || c.progression->randomHP>INT16_MAX || c.progression->randomSP>INT16_MAX))return false;
    if(c.skills.size()!=255 || std::ranges::any_of(c.points,[](auto p){return p>INT16_MAX;}))return false;
    return std::ranges::none_of(c.skills,[](const auto& s){return s.mastery>3 || s.level>40 || s.nextReadSeconds>INT64_MAX/1000;});
}
bool ValidBookLearning(const BookLearning& s) noexcept
{
    if(!ValidBookCharacter(s.character) || s.progress.size()>255 || (!s.revision && s!=BookLearning{}))return false;
    std::uint16_t previous=0;bool first=true;
    for(const auto& p:s.progress){if(p.skill>254 || (!first && p.skill<=previous) || p.level>40 || p.readyAtMs>INT64_MAX || (!p.skill && (p.level>20 || p.reads)))return false;first=false;previous=p.skill;}
    return true;
}
std::vector<std::uint8_t> LegacyBookSkillBytes(const BookCharacter& c)
{
    std::vector<std::uint8_t> out;out.reserve(2550);
    for(const auto& s:c.skills){out.push_back(s.mastery);out.push_back(s.level);for(unsigned i=0;i<8;++i)out.push_back(static_cast<std::uint8_t>(s.nextReadSeconds>>(8*i)));}return out;
}
std::expected<std::vector<std::uint8_t>,SkillBookError> EncodeBookLearning(const BookLearning& s)
{
    if(!ValidBookLearning(s))return std::unexpected(SkillBookError::InvalidState);std::vector<std::uint8_t> out;
    const auto put=[&](std::uint64_t n,unsigned count){for(unsigned i=0;i<count;++i)out.push_back(static_cast<std::uint8_t>(n>>(8*i)));};
    put(s.character.hair ? 0x354b4f42 : s.character.statResetCredits ? 0x344b4f42 : s.character.alignment ? 0x334b4f42 : s.character.progression ? 0x324b4f42 : 0x314b4f42,4);put(s.revision,8);put(s.character.experience,4);for(auto p:s.character.points)put(p,2);
    auto skills=LegacyBookSkillBytes(s.character);out.insert(out.end(),skills.begin(),skills.end());put(s.chanceTokens,4);put(s.bypassTokens,4);put(s.progress.size(),2);
    if(s.character.hair || s.character.statResetCredits || s.character.alignment)put((s.character.hair ? 8 : 0)+(s.character.statResetCredits ? 4 : 0)+(s.character.alignment ? 2 : 0)+(s.character.progression ? 1 : 0),1);
    if(s.character.progression){const auto& p=*s.character.progression;for(auto value:{p.level,p.step,p.statPoints,p.randomHP,p.randomSP})put(value,2);}
    if(s.character.alignment)put(*s.character.alignment,4);
    if(s.character.statResetCredits)put(*s.character.statResetCredits,2);
    if(s.character.hair){put(s.character.hair->part,4);put(s.character.hair->lastDyeLevel,2);}
    for(const auto& p:s.progress){put(p.skill,2);put(p.level,1);put(p.reads,2);put(p.readyAtMs,8);}return out;
}
std::expected<BookLearning,SkillBookError> DecodeBookLearning(std::span<const std::uint8_t> bytes)
{
    if(bytes.size()<2582 || bytes.size()>8192)return std::unexpected(SkillBookError::InvalidState);
    std::size_t at=0;const auto get=[&](unsigned n){if(n>bytes.size()-at)throw std::out_of_range("book state");std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(bytes[at++])<<(8*i);return v;};
    try{const auto magic=get(4);if(magic!=0x314b4f42 && magic!=0x324b4f42 && magic!=0x334b4f42 && magic!=0x344b4f42 && magic!=0x354b4f42)return std::unexpected(SkillBookError::InvalidState);BookLearning s;s.revision=get(8);s.character.experience=static_cast<std::uint32_t>(get(4));for(auto& p:s.character.points)p=static_cast<std::uint16_t>(get(2));
        for(auto& p:s.character.skills){p.mastery=static_cast<std::uint8_t>(get(1));p.level=static_cast<std::uint8_t>(get(1));p.nextReadSeconds=get(8);}s.chanceTokens=static_cast<std::uint32_t>(get(4));s.bypassTokens=static_cast<std::uint32_t>(get(4));const auto n=get(2);if(n>255)return std::unexpected(SkillBookError::InvalidState);
        const auto flags=magic>=0x334b4f42 ? get(1) : magic==0x324b4f42 ? 1u : 0u;if((magic==0x334b4f42 && flags!=2 && flags!=3) || (magic==0x344b4f42 && (flags<4 || flags>7)) || (magic==0x354b4f42 && (flags<8 || flags>15)))return std::unexpected(SkillBookError::InvalidState);
        if(flags&1){CharacterProgression p;for(auto* v:{&p.level,&p.step,&p.statPoints,&p.randomHP,&p.randomSP})*v=static_cast<std::uint16_t>(get(2));s.character.progression=p;}
        if(flags&2)s.character.alignment=static_cast<std::uint32_t>(get(4));
        if(flags&4)s.character.statResetCredits=static_cast<std::uint16_t>(get(2));
        if(flags&8)s.character.hair=CharacterHairState{static_cast<std::uint32_t>(get(4)),static_cast<std::uint16_t>(get(2))};
        for(unsigned i=0;i<n;++i)s.progress.push_back({static_cast<std::uint16_t>(get(2)),static_cast<std::uint8_t>(get(1)),static_cast<std::uint16_t>(get(2)),get(8)});
        if(at!=bytes.size() || !ValidBookLearning(s))return std::unexpected(SkillBookError::InvalidState);return s;
    }catch(...){return std::unexpected(SkillBookError::InvalidState);}
}
}
