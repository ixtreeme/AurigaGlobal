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

bool NameValid(std::string_view name) noexcept
{
    return !name.empty() && name.size()<=160 &&
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")==std::string_view::npos;
}
bool ModelValid(std::string_view model) noexcept
{
    if(model.size()>512)return false;
    for(const unsigned char c:model)if(c<0x20||c>0x7e)return false;
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
	if (!view.vid || !view.count || !CodeValid(view.code) || (view.goldAmount && (view.goldAmount>INT64_MAX || view.key.Category()!=ItemCategory::CurrencyToken || view.level || view.count!=1))) return std::unexpected(E::BadValue);
	const bool presentation=!view.nameKey.empty();
    if((presentation&&!NameValid(view.nameKey))||!ModelValid(view.model)||(!presentation&&!view.model.empty()))
        return std::unexpected(E::BadValue);
    const auto extra=presentation ? 8+1+view.nameKey.size()+2+view.model.size() : view.goldAmount ? 8 : 0;
    std::vector<std::byte> out;
	out.reserve(FixedSize + view.code.size());
	Put(out, TypedGroundHeader, 1);
	Put(out, FixedSize + view.code.size() + extra, 4);
	Put(out, presentation ? TypedGroundVersion : view.goldAmount ? 2 : 1, 1);
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
    if(presentation || view.goldAmount)Put(out,view.goldAmount,8);
    if(presentation)
    {
        Put(out,view.nameKey.size(),1);
        for(const char c:view.nameKey)out.push_back(static_cast<std::byte>(c));
        Put(out,view.model.size(),2);
        for(const char c:view.model)out.push_back(static_cast<std::byte>(c));
    }
	return out;
}

std::expected<TypedGroundView, TypedGroundWireError> DecodeTypedGround(std::span<const std::byte> in)
{
	if (in.size() < FixedSize) return std::unexpected(E::Truncated);
	if (Get(in, 0, 1) != TypedGroundHeader) return std::unexpected(E::BadHeader);
	const auto size = Get(in, 1, 4);
	const auto version=Get(in,5,1);if(version<1 || version>TypedGroundVersion)return std::unexpected(E::BadVersion);
	const auto length = Get(in, FixedSize - 1, 1);
	if ((version<3 && size != FixedSize + length + (version==2 ? 8 : 0)) || (version==3 && size<FixedSize+length+11)) return std::unexpected(E::BadSize);
	if (in.size() < size) return std::unexpected(E::Truncated);
	if (in.size() > size) return std::unexpected(E::TrailingBytes);
	const auto key = DecodeItemKey(in.subspan(22, ItemKeyWireSize));
	if (!key) return std::unexpected(E::BadValue);
	const auto coordinate = [&](std::size_t at) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(Get(in, at, 4))); };
	TypedGroundView view { static_cast<std::uint32_t>(Get(in, 6, 4)), coordinate(10), coordinate(14), coordinate(18), *key,
		static_cast<std::uint16_t>(Get(in, 28, 2)), static_cast<std::uint32_t>(Get(in, 30, 4)), {} };
	for (std::size_t i = 0; i < length; ++i) view.code.push_back(static_cast<char>(in[FixedSize + i]));
    if(version>=2){view.goldAmount=Get(in,FixedSize+length,8);if(version==2&&!view.goldAmount)return std::unexpected(E::BadValue);}
    if(version==3)
    {
        std::size_t at=FixedSize+length+8;
        const auto nameLength=Get(in,at++,1);
        if(at+nameLength+2>in.size())return std::unexpected(E::Truncated);
        for(std::size_t i=0;i<nameLength;++i)view.nameKey.push_back(static_cast<char>(in[at++]));
        const auto modelLength=Get(in,at,2);at+=2;
        if(at+modelLength>in.size())return std::unexpected(E::Truncated);
        for(std::size_t i=0;i<modelLength;++i)view.model.push_back(static_cast<char>(in[at++]));
        if(at!=in.size())return std::unexpected(E::TrailingBytes);
        if(!NameValid(view.nameKey)||!ModelValid(view.model))return std::unexpected(E::BadValue);
    }
	if (!view.vid || !view.count || !CodeValid(view.code) || (view.goldAmount && (view.goldAmount>INT64_MAX || view.key.Category()!=ItemCategory::CurrencyToken || view.level || view.count!=1))) return std::unexpected(E::BadValue);
	return view;
}
} // namespace auriga::item
