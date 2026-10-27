#pragma once
#include <compare>
#include <cstdint>
#include <expected>

namespace auriga::item
{
enum class ItemInstanceIdError
{
    Zero,
    InvalidWireSize,
};

// A persistent concrete item identity. Not interchangeable with a prototype
// ItemKey, an inventory cell, a client VID, or an entt runtime handle.
class ItemInstanceId
{
public:
    static constexpr std::expected<ItemInstanceId, ItemInstanceIdError> Create(std::uint64_t value) noexcept
    {
        if (value == 0) return std::unexpected(ItemInstanceIdError::Zero);
        return ItemInstanceId(value);
    }

    constexpr std::uint64_t Value() const noexcept { return value_; }
    constexpr auto operator<=>(const ItemInstanceId&) const = default;

private:
    explicit constexpr ItemInstanceId(std::uint64_t value) noexcept : value_(value) {}
    std::uint64_t value_;
};
} // namespace auriga::item
