#pragma once
#include "ItemKey.hpp"
#include "ItemInstanceId.hpp"
#include <array>
#include <cstddef>
#include <expected>
#include <span>

namespace auriga::item
{
inline constexpr std::size_t ItemKeyWireSize = 6;
using ItemKeyWire = std::array<std::byte, ItemKeyWireSize>;

// Little endian: category u16, local ID u32. No ABI/struct-layout dependence.
ItemKeyWire EncodeItemKey(ItemKey key) noexcept;
// This decodes one exact key, not a framing protocol; caller selects its span.
std::expected<ItemKey, ItemKeyError> DecodeItemKey(std::span<const std::byte> bytes) noexcept;

inline constexpr std::size_t ItemInstanceIdWireSize = 8;
using ItemInstanceIdWire = std::array<std::byte, ItemInstanceIdWireSize>;
ItemInstanceIdWire EncodeItemInstanceId(ItemInstanceId id) noexcept;
std::expected<ItemInstanceId, ItemInstanceIdError> DecodeItemInstanceId(std::span<const std::byte> bytes) noexcept;
} // namespace auriga::item
