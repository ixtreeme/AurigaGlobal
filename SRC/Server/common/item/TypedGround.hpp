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

// GC_TYPED_GROUND_ADD (226), explicit little endian.
// v1: header, u32 total size, u8 version, u32 VID, i32 x/y/z,
//     key(6), u16 level, u32 count, u8 code length + ASCII code.
// v2 adds u64 ground Yang. v3 always carries u64 Yang (0 for ordinary
// items), u8 localization-key length + key (1..160), u16 model-path
// length + path (0..512). The server emits v3 with authored presentation.
// Historical typed v1/v2 fixtures remain readable; raw item-add is rejected.
// Native and Unity decoders share typed_ground_v1/v3.hex golden fixtures.
inline constexpr std::uint8_t TypedGroundHeader = 226;
inline constexpr std::uint8_t TypedGroundVersion = 3;
inline constexpr std::size_t MaxGroundCodeLength = 96;

struct TypedGroundView
{
	std::uint32_t vid = 0;
	std::int32_t x = 0, y = 0, z = 0;
	ItemKey key;
	std::uint16_t level = 0;
	std::uint32_t count = 0;
	std::string code;
    std::uint64_t goldAmount=0; // v2+: ground Yang directly credits the wallet.
    // v3: authoritative localization and authored model path. Older typed frames remain readable.
    std::string nameKey, model;
};
enum class TypedGroundWireError : std::uint8_t { Truncated, BadHeader, BadVersion, BadSize, BadValue, TrailingBytes };
std::expected<std::vector<std::byte>, TypedGroundWireError> EncodeTypedGround(const TypedGroundView&);
std::expected<TypedGroundView, TypedGroundWireError> DecodeTypedGround(std::span<const std::byte>);
} // namespace auriga::item
