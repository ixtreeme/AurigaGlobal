#pragma once
// Ground loot of the typed item system (item system F4 C7c, IC-035).
//
// Memory only, like the legacy ground items: an entry holds an item state
// (key, level, count), its claim and its lifetime, and the item instance is
// made when someone picks it up. GC_TYPED_GROUND_ADD (226) shows one entry
// to the clients that see it. Removal, the claim label and the pickup request
// use the legacy VID packets (GC_ITEM_GROUND_DEL, GC_ITEM_OWNERSHIP,
// CG_ITEM_PICKUP).
#include "PersistentItems.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace auriga::item
{
inline constexpr std::int64_t GroundClaimMs = 60'000;     // the legacy instant-loot claim
inline constexpr std::int64_t GroundLifetimeMs = 300'000; // the legacy ground time

struct GroundLoot
{
	ItemGrant grant;
	std::uint32_t claimPid = 0; // 0: anyone may take it
	std::int64_t claimEndsMs = 0;
	std::int64_t expiresMs = 0;
};

bool GroundClaimActive(const GroundLoot&, std::int64_t nowMs) noexcept;
bool GroundLootExpired(const GroundLoot&, std::int64_t nowMs) noexcept;

// The legacy pickup rule (ItemSystem::PickupItem): the picker takes the loot
// when the claim is theirs or has lapsed; a party member picks it up for the
// claiming member, who receives it; anyone else is refused.
enum class GroundPickup : std::uint8_t { Refused, Picker, Claimant };
GroundPickup DecideGroundPickup(const GroundLoot&, std::int64_t nowMs, std::uint32_t pickerPid, bool claimantInPickersParty) noexcept;

// GC_TYPED_GROUND_ADD (226), version 1. Explicit little endian; the Unity
// client decodes the same layout (Metin2GamePacketCodec.ReadTypedGroundItem),
// and tests/fixtures/item_combat/typed_ground_v1.hex pins both sides.
//   u8 header (226)   u32 total size   u8 version (1)
//   u32 vid   i32 x   i32 y   i32 z   key(6)   u16 level   u32 count
//   u8 code length, ASCII content code (1..96 printable characters)
inline constexpr std::uint8_t TypedGroundHeader = 226;
inline constexpr std::uint8_t TypedGroundVersion = 1;
inline constexpr std::size_t MaxGroundCodeLength = 96;

struct TypedGroundView
{
	std::uint32_t vid = 0;
	std::int32_t x = 0, y = 0, z = 0;
	ItemKey key;
	std::uint16_t level = 0;
	std::uint32_t count = 0;
	std::string code;
};
enum class TypedGroundWireError : std::uint8_t { Truncated, BadHeader, BadVersion, BadSize, BadValue, TrailingBytes };
std::expected<std::vector<std::byte>, TypedGroundWireError> EncodeTypedGround(const TypedGroundView&);
std::expected<TypedGroundView, TypedGroundWireError> DecodeTypedGround(std::span<const std::byte>);
} // namespace auriga::item
