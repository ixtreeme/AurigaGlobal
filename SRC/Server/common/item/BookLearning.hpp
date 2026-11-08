#pragma once
#include "SkillBooks.hpp"
#include <array>
#include <span>
#include <optional>
namespace auriga::item
{
// Canonical portable snapshot of the existing 255-entry character skill table.
// The SQL bridge is exactly master:u8, level:u8, next-read-seconds:u64 LE.
struct BookLevel
{
    std::uint8_t mastery=0,level=0;
    std::uint64_t nextReadSeconds=0;
    bool operator==(const BookLevel&)const=default;
};
struct CharacterProgression
{
    std::uint16_t level=1, step=0, statPoints=0, randomHP=0, randomSP=0;
    bool operator==(const CharacterProgression&) const=default;
};
struct BookCharacter
{
    std::uint32_t experience=0;
    std::optional<CharacterProgression> progression;
    std::array<std::uint16_t,3> points{};
    // Own the bounded table on the heap: ItemCommit is copied by value in many
    // existing planners, so an inline 255-entry table can exhaust their stack.
    std::vector<BookLevel> skills=std::vector<BookLevel>(255);
    bool operator==(const BookCharacter&)const=default;
};
struct BookProgress
{
    std::uint16_t skill=0; // 0 is the horse point pool, not a fabricated skill.
    std::uint8_t level=0;
    std::uint16_t reads=0;
    std::uint64_t readyAtMs=0;
    bool operator==(const BookProgress&)const=default;
};
struct BookLearning
{
    std::uint64_t revision=0;
    BookCharacter character;
    std::vector<BookProgress> progress;
    // Explicit typed charges. Legacy affect IDs are never interpreted as these.
    std::uint32_t chanceTokens=0,bypassTokens=0;
    bool operator==(const BookLearning&)const=default;
};
bool ValidBookCharacter(const BookCharacter&) noexcept;
bool ValidBookLearning(const BookLearning&) noexcept;
std::expected<std::vector<std::uint8_t>,SkillBookError> EncodeBookLearning(const BookLearning&);
std::expected<BookLearning,SkillBookError> DecodeBookLearning(std::span<const std::uint8_t>);
std::vector<std::uint8_t> LegacyBookSkillBytes(const BookCharacter&);
}
