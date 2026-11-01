#include <item/TypedGround.hpp>
#include <item/ItemWireCodec.hpp>

namespace auriga::item
{
namespace
{
using E = TypedGroundWireError;
constexpr std::size_t FixedSize = 1 + 4 + 1 + 4 + 4 + 4 + 4 + ItemKeyWireSize + 2 + 4 + 1;

bool CodeValid(std::string_view code) noexcept
{
	if (code.empty() || code.size() > MaxGroundCodeLength) return false;
	for (const char c : code)
		if (c < 0x21 || c > 0x7e) return false;
	return true;
}

void Put(std::vector<std::byte>& out, std::uint64_t value, std::size_t bytes)
{
	for (std::size_t i = 0; i < bytes; ++i) out.push_back(std::byte((value >> (i * 8)) & 0xffu));
}

std::uint64_t Get(std::span<const std::byte> in, std::size_t at, std::size_t bytes) noexcept
{
	std::uint64_t value = 0;
	for (std::size_t i = 0; i < bytes; ++i) value |= std::to_integer<std::uint64_t>(in[at + i]) << (i * 8);
	return value;
}
} // namespace

bool GroundClaimActive(const GroundLoot& loot, std::int64_t nowMs) noexcept
{
	return loot.claimPid != 0 && nowMs < loot.claimEndsMs;
}

bool GroundLootExpired(const GroundLoot& loot, std::int64_t nowMs) noexcept
{
	return nowMs >= loot.expiresMs;
}

GroundPickup DecideGroundPickup(const GroundLoot& loot, std::int64_t nowMs, std::uint32_t pickerPid, bool claimantInPickersParty) noexcept
{
	if (!pickerPid || GroundLootExpired(loot, nowMs)) return GroundPickup::Refused;
	if (!GroundClaimActive(loot, nowMs) || loot.claimPid == pickerPid) return GroundPickup::Picker;
	return claimantInPickersParty ? GroundPickup::Claimant : GroundPickup::Refused;
}

std::expected<std::vector<std::byte>, TypedGroundWireError> EncodeTypedGround(const TypedGroundView& view)
{
	if (!view.vid || !view.count || !CodeValid(view.code)) return std::unexpected(E::BadValue);
	std::vector<std::byte> out;
	out.reserve(FixedSize + view.code.size());
	Put(out, TypedGroundHeader, 1);
	Put(out, FixedSize + view.code.size(), 4);
	Put(out, TypedGroundVersion, 1);
	Put(out, view.vid, 4);
	Put(out, static_cast<std::uint32_t>(view.x), 4);
	Put(out, static_cast<std::uint32_t>(view.y), 4);
	Put(out, static_cast<std::uint32_t>(view.z), 4);
	const auto key = EncodeItemKey(view.key);
	out.insert(out.end(), key.begin(), key.end());
	Put(out, view.level, 2);
	Put(out, view.count, 4);
	Put(out, view.code.size(), 1);
	for (const char c : view.code) out.push_back(static_cast<std::byte>(c));
	return out;
}

std::expected<TypedGroundView, TypedGroundWireError> DecodeTypedGround(std::span<const std::byte> in)
{
	if (in.size() < FixedSize) return std::unexpected(E::Truncated);
	if (Get(in, 0, 1) != TypedGroundHeader) return std::unexpected(E::BadHeader);
	const auto size = Get(in, 1, 4);
	if (Get(in, 5, 1) != TypedGroundVersion) return std::unexpected(E::BadVersion);
	const auto length = Get(in, FixedSize - 1, 1);
	if (size != FixedSize + length) return std::unexpected(E::BadSize);
	if (in.size() < size) return std::unexpected(E::Truncated);
	if (in.size() > size) return std::unexpected(E::TrailingBytes);
	const auto key = DecodeItemKey(in.subspan(22, ItemKeyWireSize));
	if (!key) return std::unexpected(E::BadValue);
	const auto coordinate = [&](std::size_t at) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(Get(in, at, 4))); };
	TypedGroundView view { static_cast<std::uint32_t>(Get(in, 6, 4)), coordinate(10), coordinate(14), coordinate(18), *key,
		static_cast<std::uint16_t>(Get(in, 28, 2)), static_cast<std::uint32_t>(Get(in, 30, 4)), {} };
	for (std::size_t i = 0; i < length; ++i) view.code.push_back(static_cast<char>(in[FixedSize + i]));
	if (!view.vid || !view.count || !CodeValid(view.code)) return std::unexpected(E::BadValue);
	return view;
}
} // namespace auriga::item
