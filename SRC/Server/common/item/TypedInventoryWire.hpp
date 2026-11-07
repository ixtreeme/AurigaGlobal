#pragma once
// GC_TYPED_INVENTORY (225), versions 1–13. V13 adds owned costume clocks and rules.
//
// Explicit little-endian byte layout, no struct/ABI dependence. The Unity
// client decodes the same layout (Metin2GamePacketCodec.ReadTypedInventory);
// tests/fixtures/item_combat/typed_inventory_v1.hex pins both sides.
//
//   u8  header (225)            u32 total size        u8 version (1)
//   u16 storages  { u8 kind, u64 id, u16 columns, u16 rows, u16 pages }
//   u8  wallets   { u8 currency, u64 balance }
//   u16 names     { key(6), u8 length, ASCII content code }   one per distinct key
//   u16 items     { id(8), key(6), u16 level, u32 count,
//                   u8 storage kind, u64 storage id, u32 anchor cell,
//                   u8 flags (1 bound, 2 accessory, 4 automatic in v2), u8 locked slot (255: none),
//                   u8 bonuses { u8 slot, u32 bonus id, i64 value },
//                   3 x { u8 state, u8 grade, u16 level, key(6) or six zero bytes },
//                   accessory if flag 2: { u8 capacity, u8 grade, u64 remaining ms, u8 permanent } }
//                   automatic if flag 4: { u8 supply, u64 charge raw, u8 active, u64 next tick ms }
//                   companion if flag 128: { u8 kind (1 pet, 2 mount, 3 growth pet),
//                                            u8 state (0 dormant, 1 following, 2 pending,
//                                                      3 expired, 4 stored, 5 riding, 6 riding pending) }
// A growth seal always has state zero here. Its birth record is read via GC253.
#include "PersistentItems.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace auriga::item
{
inline constexpr std::uint8_t TypedInventoryHeader = 225;
inline constexpr std::uint8_t TypedInventoryVersion = 1;

enum class TypedInventoryWireError : std::uint8_t
{
	TooLarge, MissingCatalogItem, Truncated, BadHeader, BadVersion, BadSize, BadValue, TrailingBytes,
};

struct TypedInventoryView
{
	std::vector<ItemStorageLayout> storages;
	std::vector<ItemWallet> wallets;
	std::map<ItemKey, std::string> names;
	std::vector<PersistentItem> items; // owner, revision and destroyed are not on the wire
	std::map<ItemInstanceId, std::pair<CompanionKind, std::uint8_t>> companions;
    struct FixedRule {CompanionClock clock;std::uint64_t durationMs;bool flying;bool operator==(const FixedRule&) const=default;};
    std::map<ItemInstanceId,FixedRule> fixedRules;
    std::map<ItemInstanceId,ItemCostumeRow> costumeRules;
    std::map<ItemInstanceId,ItemRegaliaRow> regaliaRules;
    std::map<ItemInstanceId,ItemBonusToolRow> bonusToolRules;
    std::map<ItemInstanceId,ItemSoulRow> souls;
    std::map<ItemInstanceId,SoulProfile> soulRules;
};

// The owner is the receiving character; it is not repeated on the wire.
std::expected<std::vector<std::byte>, TypedInventoryWireError> EncodeTypedInventory(
	const ItemCatalog&, std::span<const ItemStorageLayout>, std::span<const ItemWallet>, std::span<const PersistentItem>,
	std::uint64_t liveCompanion = 0);
std::expected<TypedInventoryView, TypedInventoryWireError> DecodeTypedInventory(std::span<const std::byte>, ItemOwner receiver);
} // namespace auriga::item
