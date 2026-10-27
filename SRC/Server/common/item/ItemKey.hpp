#pragma once
#include "ItemCategory.hpp"
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace auriga::item
{
enum class ItemKeyError
{
    InvalidCategory,
    ZeroLocalId,
    ReservedBits,
    InvalidWireSize,
};

// A prototype key is distinct from an instance ID and an ECS handle. Only
// validated registry categories and nonzero local IDs can construct this type.
// Catalog lookup must still prove that the referenced prototype exists.
class ItemKey
{
public:
    static constexpr std::expected<ItemKey, ItemKeyError> Create(std::uint16_t category, std::uint32_t local) noexcept
    {
        if (!FindItemCategory(category)) return std::unexpected(ItemKeyError::InvalidCategory);
        if (local == 0) return std::unexpected(ItemKeyError::ZeroLocalId);
        return ItemKey(static_cast<ItemCategory>(category), local);
    }

    static constexpr std::expected<ItemKey, ItemKeyError> Create(ItemCategory category, std::uint32_t local) noexcept
    {
        return Create(static_cast<std::uint16_t>(category), local);
    }

    constexpr ItemCategory Category() const noexcept { return category_; }
    constexpr std::uint32_t LocalId() const noexcept { return local_; }
    constexpr auto operator<=>(const ItemKey&) const = default;

private:
    constexpr ItemKey(ItemCategory category, std::uint32_t local) noexcept : category_(category), local_(local) {}
    ItemCategory category_;
    std::uint32_t local_;
};

// Lossless representation of the composite key, never an old prototype ID.
constexpr std::uint64_t PackItemKey(ItemKey key) noexcept
{
    return (std::uint64_t{static_cast<std::uint16_t>(key.Category())} << 32) | key.LocalId();
}

constexpr std::expected<ItemKey, ItemKeyError> UnpackItemKey(std::uint64_t packed) noexcept
{
    if ((packed >> 48) != 0) return std::unexpected(ItemKeyError::ReservedBits);
    return ItemKey::Create(static_cast<std::uint16_t>(packed >> 32), static_cast<std::uint32_t>(packed));
}

struct ItemKeyHash
{
    constexpr std::size_t operator()(ItemKey key) const noexcept
    {
        const auto packed = PackItemKey(key);
        if constexpr (sizeof(std::size_t) >= sizeof(packed)) return static_cast<std::size_t>(packed);
        return static_cast<std::size_t>(packed ^ (packed >> 32));
    }
};
} // namespace auriga::item
