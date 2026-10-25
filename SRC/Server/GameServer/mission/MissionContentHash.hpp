#pragma once
#include <array>
#include <cstdint>
#include <initializer_list>
#include <span>

namespace auriga::mission
{
using ContentDigest = std::array<uint8_t, 32>;
ContentDigest ContentHashParts(std::initializer_list<std::span<const uint8_t>> parts);
ContentDigest ContentHash(std::span<const uint8_t> bytes);
} // namespace auriga::mission
