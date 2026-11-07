// GC_TYPED_INVENTORY v1 wire format (F4 D1, IC-034). The golden file pins the
// exact bytes the Unity decoder is tested against. --write-golden rewrites it.
#include "PersistentItemFixture.hpp"
#include "PetCareFixture.hpp"
#include "PetLifecycleFixture.hpp"
#include <item/TypedInventoryWire.hpp>
#include <item/TypedItemDetails.hpp>

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
		auto version = b; version[5] = std::byte { 11 };
		Check(DecodeTypedInventory(version, f::Character()).error() == TypedInventoryWireError::BadVersion, "unknown version");
		Check(DecodeTypedInventory(std::span(b).first(3), f::Character()).error() == TypedInventoryWireError::Truncated, "short header");
		PersistentItem unknown { ItemInstanceId::Create(9).value(), ItemKey::Create(1, 999).value(), f::Character() };
		Check(EncodeTypedInventory(c, storages, wallets, std::span(&unknown, 1)).error() == TypedInventoryWireError::MissingCatalogItem, "unknown key");
		auto empty = EncodeTypedInventory(c, {}, {}, {});
		Check(empty && empty->size() == 13 && DecodeTypedInventory(*empty, f::Character()).has_value(), "empty inventory");
		auto source = f::Catalog();
		const auto sealKey = ItemKey::Create(ItemCategory::Companion, 1).value();
		source.companionForms = {{1,"companion.wire_pet",32500,"models/wire_pet.model"}};
		source.companions = {{sealKey,CompanionKind::Pet,1,{},CompanionClock::Permanent}};
		source.refinements.push_back({27001,"refine.wire_companion",RefinementKind::Companion,0,{{0,{},{}}}});
		ItemBaseRow sealBase{sealKey};
		sealBase.contentCode = "companion.wire_seal";
		sealBase.nameKey = "companion.wire_seal.name";
		sealBase.descriptionKey = "companion.wire_seal.description";
		sealBase.iconAsset = "icons/wire_pet.png";
		sealBase.refinementProfile = 27001;
		sealBase.usePolicy = sealBase.tradePolicy = 1;
		source.items.push_back(sealBase);
		auto companionCatalog = BuildItemCatalog(std::move(source));
		Check(companionCatalog.has_value(), "companion fixture catalog");
		PersistentItem seal{ItemInstanceId::Create(17).value(),sealKey,f::Character()};
		seal.count = 1;
		seal.revision = 1;
		seal.placement = ItemPlacement{ItemStorageKind::Inventory,1,0};
		seal.companion = CompanionState{};
		for (std::uint8_t phase : {0, 1, 2})
		{
			seal.companion->recallRequested = phase != 0;
			const auto live = phase == 1 ? seal.id.Value() : 0;
			auto encoded = EncodeTypedInventory(**companionCatalog, storages, {}, std::span(&seal, 1), live);
			Check(encoded && (*encoded)[5] == std::byte{7}, "companion inventory uses version seven");
			auto decoded = DecodeTypedInventory(*encoded, f::Character());
			Check(decoded && decoded->companions.at(seal.id)==std::pair{CompanionKind::Pet,phase}
				&& decoded->items[0].companion==seal.companion, "companion presentation and recall intent roundtrip");
			encoded->back() = std::byte{3};
			Check(DecodeTypedInventory(*encoded, f::Character()).error() == TypedInventoryWireError::BadValue,
				"unknown companion presentation accepted");
		}

		auto hatchCatalog = BuildItemCatalog(pet_care_fixture::Catalog());
		Check(hatchCatalog.has_value(), "hatch fixture catalog");
		const auto petKey = pet_care_fixture::Pet();
		PersistentItem growth{ItemInstanceId::Create(21).value(),petKey,f::Character()};
		growth.placement = ItemPlacement{ItemStorageKind::Inventory,1,0};
		growth.growthPet = GrowthPetState{"Arany",1,0,MakeGrowthPetProgress(**hatchCatalog, petKey).value(),
			MakeGrowthPetLife(**hatchCatalog, petKey).value(),{35,15,15}};
		auto growthWire = EncodeTypedInventory(**hatchCatalog, storages, {}, std::span(&growth, 1));
		Check(growthWire && (*growthWire)[5] == std::byte{8}, "growth seal inventory uses version eight");
		auto growthView = DecodeTypedInventory(*growthWire, f::Character());
		Check(growthView && growthView->companions.at(growth.id)==std::pair{CompanionKind::GrowthPet,std::uint8_t{0}}
			&& !growthView->items[0].companion, "growth seal marker must not authorize fixed summoning");
		growthWire->back() = std::byte{1};
		Check(DecodeTypedInventory(*growthWire, f::Character()).error() == TypedInventoryWireError::BadValue,
			"growth seal marker falsely claimed a live actor");
		growthWire->back() = std::byte{0};
		(*growthWire)[5] = std::byte{7};
		Check(DecodeTypedInventory(*growthWire, f::Character()).error() == TypedInventoryWireError::BadValue,
			"growth seal marker accepted under v7");
		growth.growthPet->everSummoned = growth.growthPet->recallRequested = true;
		for (const auto live : {growth.id.Value(), std::uint64_t{0}})
		{
			const auto wire = EncodeTypedInventory(**hatchCatalog, storages, {}, std::span(&growth, 1), live);
			Check(wire && (*wire)[5] == std::byte{9}, "active growth seal uses version nine");
			const auto view = DecodeTypedInventory(*wire, f::Character());
			Check(view && view->companions.at(growth.id) == std::pair{CompanionKind::GrowthPet,
				static_cast<std::uint8_t>(live ? 1 : 2)}, "growth follower and pending intent roundtrip");
		}
		growth.growthPet->recallRequested = false;
		growth.growthPet->life.remainingMs = 0;
		const auto expired = EncodeTypedInventory(**hatchCatalog, storages, {}, std::span(&growth, 1));
		Check(expired && (*expired)[5] == std::byte{9}, "expired growth seal uses version nine");
		const auto expiredView = DecodeTypedInventory(*expired, f::Character());
		Check(expiredView && expiredView->companions.at(growth.id) == std::pair{CompanionKind::GrowthPet,std::uint8_t{3}},
			"expired growth seal is represented without a live actor");

        auto transportCatalog=BuildItemCatalog(pet_lifecycle_fixture::Catalog());
        Check(transportCatalog.has_value(),"carrier catalog");
        PersistentItem box{ItemInstanceId::Create(22).value(),pet_lifecycle_fixture::Supply(7),f::Character()};
        box.placement=ItemPlacement{ItemStorageKind::Inventory,1,1};box.petSupply=PetSupplyState{growth.id.Value()};
        growth.growthPet->carrierInstanceId=box.id.Value();growth.growthPet->life.remainingMs=100;
        growth.placement=ItemPlacement{ItemStorageKind::PetCarrier,box.id.Value(),0};
        auto transportLayouts=storages;transportLayouts.push_back({{f::Character(),ItemStorageKind::PetCarrier,box.id.Value()},1,1,1});
        std::vector<PersistentItem> transportItems{growth,box};
        auto transportWire=EncodeTypedInventory(**transportCatalog,transportLayouts,{},transportItems);
        Check(transportWire && (*transportWire)[5]==std::byte{10},"carrier uses v10");
        auto transportView=DecodeTypedInventory(*transportWire,f::Character());
        Check(transportView && transportView->items.size()==2 && transportView->items[1].petSupply==box.petSupply && transportView->companions.at(growth.id).second==4,"packed seal and carrier links roundtrip");
        transportWire->at(transportWire->size()-41)=std::byte{0};
        Check(!DecodeTypedInventory(*transportWire,f::Character()),"broken carrier link rejected");
        PersistentItem skin{ItemInstanceId::Create(23).value(),pet_lifecycle_fixture::Supply(8),f::Character()};
        skin.placement=ItemPlacement{ItemStorageKind::Inventory,1,2};skin.petSupply=InitialPetSupplyState(**transportCatalog,skin.key,100).value();
        transportItems.push_back(skin);
        const auto skinWire=EncodeTypedInventory(**transportCatalog,transportLayouts,{},transportItems);
        const auto skinView=skinWire ? DecodeTypedInventory(*skinWire,f::Character()) : std::expected<TypedInventoryView,TypedInventoryWireError>{std::unexpected(TypedInventoryWireError::BadValue)};
        Check(skinView && skinView->items.back().petSupply==skin.petSupply,"skin v10 definition and persisted clock roundtrip");
        auto badSkin=*skinWire;std::fill(badSkin.end()-4,badSkin.end(),std::byte{});
        Check(!DecodeTypedInventory(badSkin,f::Character()),"zero skin form rejected");

        auto details=EncodeTypedItemDetails(c,items,true);
        Check(details.has_value(),"details encode with stone state and refinement recipes");
        Check(std::to_integer<unsigned>((*details)[0])==227 && std::to_integer<unsigned>((*details)[5])==1,"details envelope");
        const auto detailsPath=Golden().parent_path()/"typed_item_details_v1.hex";
        if(argc==2 && std::string(argv[1])=="--write-golden")std::ofstream(detailsPath,std::ios::binary)<<Hex(*details);
        else { std::ifstream input(detailsPath,std::ios::binary);std::stringstream stored;stored<<input.rdbuf();auto text=stored.str();std::erase(text,'\r');Check(text==Hex(*details),"details golden parity"); }
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
