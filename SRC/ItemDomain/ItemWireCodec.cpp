#include <item/ItemWireCodec.hpp>
#include <cstdint>

namespace auriga::item
{
ItemKeyWire EncodeItemKey(ItemKey key) noexcept
{
    const auto category = static_cast<std::uint16_t>(key.Category());
    const auto local = key.LocalId();
    return {std::byte(category & 0xffu), std::byte(category >> 8),
            std::byte(local & 0xffu), std::byte((local >> 8) & 0xffu),
            std::byte((local >> 16) & 0xffu), std::byte(local >> 24)};
}

std::expected<ItemKey, ItemKeyError> DecodeItemKey(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() != ItemKeyWireSize) return std::unexpected(ItemKeyError::InvalidWireSize);
    const auto category = std::uint16_t(std::to_integer<std::uint16_t>(bytes[0]) |
                                      (std::to_integer<std::uint16_t>(bytes[1]) << 8));
    const auto local = std::to_integer<std::uint32_t>(bytes[2]) |
                       (std::to_integer<std::uint32_t>(bytes[3]) << 8) |
                       (std::to_integer<std::uint32_t>(bytes[4]) << 16) |
                       (std::to_integer<std::uint32_t>(bytes[5]) << 24);
    return ItemKey::Create(category, local);
}

ItemInstanceIdWire EncodeItemInstanceId(ItemInstanceId id) noexcept
{
    ItemInstanceIdWire result {};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = std::byte((id.Value() >> (i * 8)) & 0xffu);
    return result;
}

std::expected<ItemInstanceId, ItemInstanceIdError> DecodeItemInstanceId(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() != ItemInstanceIdWireSize) return std::unexpected(ItemInstanceIdError::InvalidWireSize);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        value |= std::to_integer<std::uint64_t>(bytes[i]) << (i * 8);
    return ItemInstanceId::Create(value);
}
} // namespace auriga::item
