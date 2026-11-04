#pragma once
#include "SkillBooks.hpp"
namespace auriga::item
{
enum class BookAidKind : std::uint8_t { Chance = 1, DelayBypass = 2 };
struct BookAidProfile
{
    std::uint32_t id = 0;
    std::string code;
    BookAidKind kind{};
    std::uint16_t unitsPerUse = 0;
    std::uint32_t chargesPerUse = 0, maximumCharges = 0;
};
bool ValidBookAidProfile(const BookAidProfile&) noexcept;
}
