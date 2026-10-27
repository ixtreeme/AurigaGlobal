#include <item/ItemWireCodec.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

using namespace auriga::item;
namespace
{
std::size_t checks = 0;
void Check(bool value, const char* message, const std::source_location where = std::source_location::current())
{
    ++checks;
    if (!value) throw std::runtime_error(std::string(message) + " at line " + std::to_string(where.line()));
}
constexpr auto Sword = ItemKey::Create(ItemCategory::Weapon, 1).value();
static_assert(PackItemKey(Sword) == 0x0000000100000001ULL);
static_assert(UnpackItemKey(0x0000000100000001ULL).value() == Sword);
static_assert(ItemKey::Create(ItemCategory::Weapon, 0).error() == ItemKeyError::ZeroLocalId);
static_assert(!std::is_default_constructible_v<ItemKey>);
static_assert(!std::is_aggregate_v<ItemKey>);
static_assert(!std::is_convertible_v<std::uint64_t, ItemKey>);
static_assert(!std::is_default_constructible_v<ItemInstanceId>);
static_assert(!std::is_convertible_v<ItemInstanceId, ItemKey>);
static_assert(!std::is_convertible_v<ItemKey, ItemInstanceId>);
static_assert(!std::is_convertible_v<std::uint64_t, ItemInstanceId>);

void InstanceIdentity()
{
    constexpr auto zero = ItemInstanceId::Create(0);
    static_assert(!zero && zero.error() == ItemInstanceIdError::Zero);
    const auto id = ItemInstanceId::Create(0x0123456789abcdefULL).value();
    const ItemInstanceIdWire golden {std::byte{0xef}, std::byte{0xcd}, std::byte{0xab}, std::byte{0x89},
                                     std::byte{0x67}, std::byte{0x45}, std::byte{0x23}, std::byte{0x01}};
    Check(EncodeItemInstanceId(id) == golden, "64-bit instance ID little-endian fixture");
    Check(DecodeItemInstanceId(golden).value() == id, "instance ID is not truncated to 32 bits");
    const ItemInstanceIdWire zeros {};
    const auto invalid = DecodeItemInstanceId(zeros);
    Check(!invalid && invalid.error() == ItemInstanceIdError::Zero, "zero instance ID rejected from wire");
    for (auto value : {1ULL, 0xffffffffULL, 0x100000000ULL, 0x8000000000000000ULL, 0xffffffffffffffffULL})
    {
        const auto instance = ItemInstanceId::Create(value).value();
        Check(instance.Value() == value, "unsigned instance ID preserved across signed and 32-bit limits");
        Check(DecodeItemInstanceId(EncodeItemInstanceId(instance)).value() == instance, "instance boundary round-trip");
    }
    std::array<std::byte, 10> framed {};
    std::copy(golden.begin(), golden.end(), framed.begin() + 1);
    Check(DecodeItemInstanceId(std::span<const std::byte>(framed).subspan(1, 8)).value() == id, "unaligned instance span");
    for (std::size_t length = 0; length <= framed.size(); ++length)
    {
        if (length == ItemInstanceIdWireSize) continue;
        const auto result = DecodeItemInstanceId(std::span<const std::byte>(framed).first(length));
        Check(!result && result.error() == ItemInstanceIdError::InvalidWireSize, "invalid instance wire length");
    }
}

void IdentityAndGoldenBytes()
{
    const auto armor = ItemKey::Create(ItemCategory::Armor, 1).value();
    Check(Sword != armor, "same local ID in different categories must remain distinct");
    Check(Sword.LocalId() == armor.LocalId(), "local ID is category-scoped");
    const std::map<ItemKey, int> ordered {{armor, 20}, {Sword, 10}};
    Check(ordered.size() == 2 && ordered.begin()->first == Sword, "composite ordering");
    const std::unordered_map<ItemKey, int, ItemKeyHash> hashed {{armor, 20}, {Sword, 10}};
    Check(hashed.size() == 2 && hashed.at(Sword) == 10 && hashed.at(armor) == 20, "composite lookup");

    const auto key = ItemKey::Create(ItemCategory::Weapon, 0x78563412u).value();
    const ItemKeyWire golden {std::byte{0x01}, std::byte{0x00}, std::byte{0x12},
                              std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    Check(EncodeItemKey(key) == golden, "six-byte little-endian protocol fixture");
    Check(DecodeItemKey(golden).value() == key, "decode external golden fixture");
    Check(PackItemKey(key) == 0x0000000178563412ULL, "packed key is distinct from six-byte wire order");
    const ItemKeyWire armorGolden {std::byte{0x02}, std::byte{0x00}, std::byte{0x01},
                                   std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    Check(DecodeItemKey(armorGolden).value() == armor, "category is present on wire");

    std::array<std::byte, 8> framed {};
    std::copy(golden.begin(), golden.end(), framed.begin() + 1);
    Check(DecodeItemKey(std::span<const std::byte>(framed).subspan(1, 6)).value() == key,
          "unaligned subspan decodes without reinterpret_cast");
    for (std::size_t length = 0; length <= framed.size(); ++length)
    {
        if (length == ItemKeyWireSize) continue;
        const auto result = DecodeItemKey(std::span<const std::byte>(framed).first(length));
        Check(!result && result.error() == ItemKeyError::InvalidWireSize, "truncated and oversized wire input rejected");
    }
}

void CategoriesAndBoundaries()
{
    std::set<std::uint16_t> known;
    for (const auto& descriptor : ItemCategories)
    {
        const auto category = static_cast<std::uint16_t>(descriptor.category);
        Check(known.insert(category).second, "category IDs unique");
        Check(!descriptor.code.empty() && !descriptor.table.empty(), "category metadata present");
        const auto zero = ItemKey::Create(category, 0);
        Check(!zero && zero.error() == ItemKeyError::ZeroLocalId, "zero local ID rejected per category");
        const auto packedZero = UnpackItemKey(std::uint64_t{category} << 32);
        Check(!packedZero && packedZero.error() == ItemKeyError::ZeroLocalId, "packed zero local ID rejected");
        const std::array<std::uint32_t, 9> locals {1, 255, 256, 65535, 65536, 0x7fffffffu,
                                                   0x80000000u, 0xffffffffu, 0x78563412u};
        for (auto local : locals)
        {
            const auto key = ItemKey::Create(category, local).value();
            Check(key.LocalId() == local && key.Category() == descriptor.category, "uint32 local ID preserved");
            Check(UnpackItemKey(PackItemKey(key)).value() == key, "packed boundaries round-trip");
            Check(DecodeItemKey(EncodeItemKey(key)).value() == key, "wire boundaries round-trip");
        }
        for (unsigned bit = 48; bit < 64; ++bit)
        {
            const auto result = UnpackItemKey(PackItemKey(ItemKey::Create(category, 1).value()) | (1ULL << bit));
            Check(!result && result.error() == ItemKeyError::ReservedBits, "reserved high bits never truncated");
        }
    }
    Check(known.size() == 18 && known.contains(1) && known.contains(18), "v1 draft registry fixture");
    for (std::uint32_t category = 0; category <= std::numeric_limits<std::uint16_t>::max(); ++category)
    {
        if (known.contains(static_cast<std::uint16_t>(category))) continue;
        const auto direct = ItemKey::Create(static_cast<std::uint16_t>(category), 1);
        Check(!direct && direct.error() == ItemKeyError::InvalidCategory, "every unknown uint16 category rejected");
        const auto packed = UnpackItemKey((std::uint64_t{category} << 32) | 1);
        Check(!packed && packed.error() == ItemKeyError::InvalidCategory, "packed unknown category rejected");
        const ItemKeyWire wire {std::byte(category & 0xffu), std::byte(category >> 8),
                                std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
        const auto decoded = DecodeItemKey(wire);
        Check(!decoded && decoded.error() == ItemKeyError::InvalidCategory, "wire unknown category rejected");
    }
}
}

int main()
{
    try
    {
        IdentityAndGoldenBytes();
        CategoriesAndBoundaries();
        InstanceIdentity();
        std::cout << "Item key checks passed: " << checks << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
