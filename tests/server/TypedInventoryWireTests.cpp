// GC_TYPED_INVENTORY v1 wire format (F4 D1, IC-034). The golden file pins the
// exact bytes the Unity decoder is tested against. --write-golden rewrites it.
#include "PersistentItemFixture.hpp"
#include <item/TypedInventoryWire.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>

namespace f = persistent_item_fixture;
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
	return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures/item_combat/typed_inventory_v1.hex";
}
} // namespace

int main(int argc, char** argv)
{
	try
	{
		auto catalog = BuildItemCatalog(f::Catalog());
		Check(catalog.has_value(), "fixture catalog");
		const auto& c = **catalog;
		auto ring = f::Sword(UINT64_MAX - 20);
		ring.placement->anchor = 7; ring.boundTo = f::Character(); ring.attributes.lockedSlot = 1;
		ring.accessory = AccessorySocketState { 2, 1, 3600000, false };
		const std::vector<PersistentItem> items { f::Sword(), f::Leather(), ring };
		const std::vector<ItemStorageLayout> storages { f::Inventory(), { { f::Character(), ItemStorageKind::Equipment, 1 }, 32, 1, 1 } };
		const std::vector<ItemWallet> wallets { f::Gold() };

		auto bytes = EncodeTypedInventory(c, storages, wallets, items);
		Check(bytes.has_value(), "encode");
		const auto& b = *bytes;
		Check(std::to_integer<unsigned>(b[0]) == 225 && std::to_integer<unsigned>(b[5]) == 1, "header and version");
		const auto size = std::to_integer<std::uint32_t>(b[1]) | std::to_integer<std::uint32_t>(b[2]) << 8 |
		                  std::to_integer<std::uint32_t>(b[3]) << 16 | std::to_integer<std::uint32_t>(b[4]) << 24;
		Check(size == b.size(), "size field is the total length");

		auto view = DecodeTypedInventory(b, f::Character());
		Check(view.has_value(), "decode");
		Check(view->storages == storages && view->wallets.size() == 1 && view->wallets[0].balance == f::Gold().balance &&
		      view->wallets[0].currency == WalletCurrency::Gold, "storages and wallets round trip");
		Check(view->names.size() == 2 && view->names.at(f::Sword().key) == c.Find(f::Sword().key)->base.contentCode &&
		      view->names.at(f::Leather().key) == c.Find(f::Leather().key)->base.contentCode, "one name per distinct key");
		Check(view->items.size() == items.size(), "item count");
		for (std::size_t i = 0; i < items.size(); ++i)
		{
			const auto& want = items[i]; const auto& got = view->items[i];
			Check(got.id == want.id && got.key == want.key && got.refinementLevel == want.refinementLevel && got.count == want.count &&
			      got.placement == want.placement && got.boundTo.has_value() == want.boundTo.has_value() && got.attributes == want.attributes &&
			      got.stones == want.stones && got.accessory == want.accessory, "item round trip");
		}

		// Every malformed form is refused, never partially read.
		auto cut = b; cut.pop_back();
		Check(DecodeTypedInventory(cut, f::Character()).error() == TypedInventoryWireError::BadSize, "truncated packet");
		auto longer = b; longer.push_back(std::byte {}); longer[1] = std::byte(longer.size() & 0xff); longer[2] = std::byte(longer.size() >> 8 & 0xff);
		Check(DecodeTypedInventory(longer, f::Character()).error() == TypedInventoryWireError::TrailingBytes, "trailing byte");
		auto header = b; header[0] = std::byte { 224 };
		Check(DecodeTypedInventory(header, f::Character()).error() == TypedInventoryWireError::BadHeader, "wrong header");
		auto version = b; version[5] = std::byte { 2 };
		Check(DecodeTypedInventory(version, f::Character()).error() == TypedInventoryWireError::BadVersion, "unknown version");
		Check(DecodeTypedInventory(std::span(b).first(3), f::Character()).error() == TypedInventoryWireError::Truncated, "short header");
		PersistentItem unknown { ItemInstanceId::Create(9).value(), ItemKey::Create(1, 999).value(), f::Character() };
		Check(EncodeTypedInventory(c, storages, wallets, std::span(&unknown, 1)).error() == TypedInventoryWireError::MissingCatalogItem, "unknown key");
		auto empty = EncodeTypedInventory(c, {}, {}, {});
		Check(empty && empty->size() == 13 && DecodeTypedInventory(*empty, f::Character()).has_value(), "empty inventory");

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
		std::cout << checks << " typed inventory wire checks passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
