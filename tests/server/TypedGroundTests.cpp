// Typed ground loot (F4 C7c, IC-035): the pickup rule and the
// GC_TYPED_GROUND_ADD v1 wire. The golden file pins the bytes the Unity
// decoder is tested against. --write-golden rewrites it.
#include <item/TypedGround.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace auriga::item;

namespace
{
unsigned checks = 0;
void Check(bool good, const char* why, std::source_location at = std::source_location::current())
{
	++checks;
	if (!good) throw std::runtime_error(std::string(why) + " at " + std::to_string(at.line()));
}
std::string Hex(std::span<const std::byte> bytes)
{
	std::ostringstream out;
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		static const char* digits = "0123456789abcdef";
		const auto b = std::to_integer<unsigned>(bytes[i]);
		out << digits[b >> 4] << digits[b & 15] << ((i + 1) % 32 ? "" : "\n");
	}
	return out.str();
}
std::filesystem::path Golden()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures/item_combat/typed_ground_v1.hex";
}

void PickupRule()
{
	const GroundLoot loot { { ItemKey::Create(6, 2).value(), 0, 5 }, 77, 61'000, 301'000 };
	Check(GroundClaimActive(loot, 1'000) && !GroundClaimActive(loot, 61'000), "the claim lasts until its end, exclusive");
	Check(!GroundLootExpired(loot, 300'999) && GroundLootExpired(loot, 301'000), "the loot lasts until its expiry, exclusive");
	Check(DecideGroundPickup(loot, 1'000, 77, false) == GroundPickup::Picker, "the claimant picks up its own loot");
	Check(DecideGroundPickup(loot, 1'000, 88, false) == GroundPickup::Refused, "a stranger is refused while claimed");
	Check(DecideGroundPickup(loot, 1'000, 88, true) == GroundPickup::Claimant, "a party member picks up for the claimant");
	Check(DecideGroundPickup(loot, 61'000, 88, false) == GroundPickup::Picker, "anyone takes it once the claim lapsed");
	Check(DecideGroundPickup(loot, 301'000, 77, false) == GroundPickup::Refused, "expired loot is gone");
	Check(DecideGroundPickup(loot, 1'000, 0, true) == GroundPickup::Refused, "no picker");
	GroundLoot open = loot; open.claimPid = 0;
	Check(!GroundClaimActive(open, 1'000) && DecideGroundPickup(open, 1'000, 88, false) == GroundPickup::Picker, "unclaimed loot");
}
} // namespace

int main(int argc, char** argv)
{
	try
	{
		PickupRule();
		const TypedGroundView view { 0x01020304, 123456, -654321, -15, ItemKey::Create(6, 70000).value(), 3, 200, "material.leather" };
		auto bytes = EncodeTypedGround(view);
		Check(bytes.has_value(), "encode");
		const auto& b = *bytes;
		Check(b.size() == 35 + view.code.size(), "fixed part plus the code");
		Check(std::to_integer<unsigned>(b[0]) == 226 && std::to_integer<unsigned>(b[5]) == 1, "header and version");
		const auto size = std::to_integer<std::uint32_t>(b[1]) | std::to_integer<std::uint32_t>(b[2]) << 8 |
		                  std::to_integer<std::uint32_t>(b[3]) << 16 | std::to_integer<std::uint32_t>(b[4]) << 24;
		Check(size == b.size(), "size field is the total length");
		auto back = DecodeTypedGround(b);
		Check(back && back->vid == view.vid && back->x == view.x && back->y == view.y && back->z == view.z && back->key == view.key &&
		      back->level == view.level && back->count == view.count && back->code == view.code, "round trip");

		// Every malformed form is refused, never partially read.
		Check(DecodeTypedGround(std::span(b).first(34)).error() == TypedGroundWireError::Truncated, "short fixed part");
		auto cut = b; cut.pop_back();
		Check(DecodeTypedGround(cut).error() == TypedGroundWireError::Truncated, "short code");
		auto longer = b; longer.push_back(std::byte {});
		Check(DecodeTypedGround(longer).error() == TypedGroundWireError::TrailingBytes, "trailing byte");
		auto header = b; header[0] = std::byte { 225 };
		Check(DecodeTypedGround(header).error() == TypedGroundWireError::BadHeader, "wrong header");
		auto version = b; version[5] = std::byte { 2 };
		Check(DecodeTypedGround(version).error() == TypedGroundWireError::BadVersion, "unknown version");
		auto sized = b; sized[1] = std::byte { 99 };
		Check(DecodeTypedGround(sized).error() == TypedGroundWireError::BadSize, "size disagrees with the code length");
		auto zeroCount = b; zeroCount[30] = zeroCount[31] = zeroCount[32] = zeroCount[33] = std::byte {};
		Check(DecodeTypedGround(zeroCount).error() == TypedGroundWireError::BadValue, "zero count");
		auto control = b; control[35] = std::byte { 0x0a };
		Check(DecodeTypedGround(control).error() == TypedGroundWireError::BadValue, "control character in the code");
		auto badKey = b; badKey[22] = badKey[23] = std::byte {};
		Check(DecodeTypedGround(badKey).error() == TypedGroundWireError::BadValue, "category 0 is no key");
		for (auto bad : { TypedGroundView { 0, 0, 0, 0, view.key, 0, 1, "a" }, TypedGroundView { 1, 0, 0, 0, view.key, 0, 0, "a" },
		                  TypedGroundView { 1, 0, 0, 0, view.key, 0, 1, "" }, TypedGroundView { 1, 0, 0, 0, view.key, 0, 1, std::string(97, 'a') },
		                  TypedGroundView { 1, 0, 0, 0, view.key, 0, 1, "two words" } })
			Check(EncodeTypedGround(bad).error() == TypedGroundWireError::BadValue, "invalid view refused");
		Check(EncodeTypedGround({ 1, 0, 0, 0, view.key, 0, 1, std::string(96, 'a') }).has_value(), "a 96-character code fits");

		const auto text = Hex(b);
		if (argc == 2 && std::string(argv[1]) == "--write-golden")
		{
			std::ofstream(Golden(), std::ios::binary) << text;
			std::cout << "golden written: " << Golden().string() << '\n';
		}
		else
		{
			std::ifstream in(Golden(), std::ios::binary);
			std::stringstream stored; stored << in.rdbuf();
			std::string expected = stored.str();
			std::erase(expected, '\r');
			Check(in.good() || !expected.empty(), "golden file present");
			Check(expected == text, "bytes differ from the golden file (wire format change needs a new version)");
		}
		std::cout << checks << " typed ground checks passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
