#include <item/TypedInventoryWire.hpp>
#include <item/ItemCatalog.hpp>
#include <item/ItemWireCodec.hpp>

#include <limits>
#include <algorithm>

namespace auriga::item
{
namespace
{
using E = TypedInventoryWireError;
constexpr std::size_t MaxPacket = 1u << 20; // the client's dynamic packet guard

class Writer
{
public:
	template <class T> void Put(T value, std::size_t bytes = sizeof(T))
	{
		const auto raw = static_cast<std::uint64_t>(value);
		for (std::size_t i = 0; i < bytes; ++i) out.push_back(std::byte((raw >> (i * 8)) & 0xffu));
	}
	void Bytes(std::span<const std::byte> bytes) { out.insert(out.end(), bytes.begin(), bytes.end()); }
	std::vector<std::byte> out;
};

class Reader
{
public:
	explicit Reader(std::span<const std::byte> bytes) : data(bytes) {}
	template <class T> T Get(std::size_t bytes = sizeof(T))
	{
		Need(bytes);
		std::uint64_t raw = 0;
		for (std::size_t i = 0; i < bytes; ++i) raw |= std::to_integer<std::uint64_t>(data[at + i]) << (i * 8);
		at += bytes;
		return static_cast<T>(raw);
	}
	std::span<const std::byte> Take(std::size_t bytes) { Need(bytes); auto part = data.subspan(at, bytes); at += bytes; return part; }
	bool Done() const noexcept { return at == data.size(); }
	struct Short {};
private:
	void Need(std::size_t bytes) const { if (data.size() - at < bytes) throw Short {}; }
	std::span<const std::byte> data;
	std::size_t at = 0;
};
struct Bad {};

bool Zero(std::span<const std::byte> bytes) noexcept
{
	for (auto b : bytes) if (b != std::byte {}) return false;
	return true;
}
} // namespace

std::expected<std::vector<std::byte>, TypedInventoryWireError> EncodeTypedInventory(
	const ItemCatalog& catalog, std::span<const ItemStorageLayout> storages, std::span<const ItemWallet> wallets,
	std::span<const PersistentItem> items, std::uint64_t liveCompanion)
{
	if (storages.size() > (std::numeric_limits<std::uint16_t>::max)() || wallets.size() > 255 || items.size() > (std::numeric_limits<std::uint16_t>::max)())
		return std::unexpected(E::TooLarge);
	std::map<ItemKey, std::string> names;
	for (const auto& item : items)
	{
		const auto* entry = catalog.Find(item.key);
		if (!entry) return std::unexpected(E::MissingCatalogItem);
		if (entry->base.contentCode.size() > 255) return std::unexpected(E::TooLarge);
		names.emplace(item.key, entry->base.contentCode);
	}
	Writer w;
	w.Put<std::uint8_t>(TypedInventoryHeader);
	w.Put<std::uint32_t>(0); // patched below
	const bool hasGrowthPets = std::ranges::any_of(items, [](const auto& item) { return item.growthPet.has_value(); });
	const bool hasPetSupplies=std::ranges::any_of(items,[](const auto& item){return item.petSupply.has_value();});
	const bool hasGrowthStatus = std::ranges::any_of(items, [](const auto& item) {
		return item.growthPet && (RecallRequested(item) ||
			(item.growthPet->life.clock == CompanionClock::ActiveTime && !item.growthPet->life.remainingMs));
	});
	const bool hasCompanions = std::ranges::any_of(items, [](const auto& item) { return item.companion.has_value(); });
	w.Put<std::uint8_t>(hasPetSupplies ? 10 : hasGrowthStatus ? 9 : hasGrowthPets ? 8 : hasCompanions ? 7 : std::ranges::any_of(items,[](const auto& i){return i.tool.has_value();}) ? 6 : std::ranges::any_of(items,[](const auto& i){return i.polymorph.has_value();}) ? 5 : std::ranges::any_of(items,[](const auto& i){return i.bookSkill.has_value();}) ? 4 : std::ranges::any_of(items,[](const auto& i){return i.toggle.has_value();}) ? 3 : std::ranges::any_of(items,[](const auto& i){return i.automatic.has_value();}) ? 2 : TypedInventoryVersion);
	w.Put<std::uint16_t>(static_cast<std::uint16_t>(storages.size()));
	for (const auto& s : storages)
	{
		w.Put<std::uint8_t>(static_cast<std::uint8_t>(s.storage.kind));
		w.Put<std::uint64_t>(s.storage.id);
		w.Put<std::uint16_t>(s.columns); w.Put<std::uint16_t>(s.rows); w.Put<std::uint16_t>(s.pages);
	}
	w.Put<std::uint8_t>(static_cast<std::uint8_t>(wallets.size()));
	for (const auto& wallet : wallets)
	{
		w.Put<std::uint8_t>(static_cast<std::uint8_t>(wallet.currency));
		w.Put<std::uint64_t>(wallet.balance);
	}
	w.Put<std::uint16_t>(static_cast<std::uint16_t>(names.size()));
	for (const auto& [key, code] : names)
	{
		w.Bytes(EncodeItemKey(key));
		w.Put<std::uint8_t>(static_cast<std::uint8_t>(code.size()));
		for (const char c : code) w.Put<std::uint8_t>(static_cast<std::uint8_t>(c));
	}
	w.Put<std::uint16_t>(static_cast<std::uint16_t>(items.size()));
	for (const auto& item : items)
	{
		w.Bytes(EncodeItemInstanceId(item.id));
		w.Bytes(EncodeItemKey(item.key));
		w.Put<std::uint16_t>(item.refinementLevel);
		w.Put<std::uint32_t>(item.count);
		w.Put<std::uint8_t>(item.placement ? static_cast<std::uint8_t>(item.placement->kind) : 0);
		w.Put<std::uint64_t>(item.placement ? item.placement->storageId : 0);
		w.Put<std::uint32_t>(item.placement ? item.placement->anchor : 0);
		w.Put<std::uint8_t>((item.boundTo ? 1 : 0) | (item.accessory ? 2 : 0) | (item.automatic ? 4 : 0) | (item.toggle ? 8 : 0) | (item.bookSkill ? 16 : 0) | (item.polymorph ? 32 : 0) | (item.tool ? 64 : 0) | (item.companion || item.growthPet ? 128 : 0));
		w.Put<std::uint8_t>(item.attributes.lockedSlot ? *item.attributes.lockedSlot : 255);
		std::uint8_t bonuses = 0;
		for (const auto& slot : item.attributes.slots) if (slot) ++bonuses;
		w.Put<std::uint8_t>(bonuses);
		for (std::size_t slot = 0; slot < item.attributes.slots.size(); ++slot)
		{
			const auto& bonus = item.attributes.slots[slot];
			if (!bonus) continue;
			w.Put<std::uint8_t>(static_cast<std::uint8_t>(slot));
			w.Put<std::uint32_t>(static_cast<std::uint32_t>(bonus->bonus));
			w.Put<std::uint64_t>(static_cast<std::uint64_t>(bonus->valueScaled));
		}
		for (const auto& stone : item.stones)
		{
			w.Put<std::uint8_t>(static_cast<std::uint8_t>(stone.state));
			w.Put<std::uint8_t>(stone.grade);
			w.Put<std::uint16_t>(stone.refinementLevel);
			if (stone.stone) w.Bytes(EncodeItemKey(*stone.stone));
			else w.Put<std::uint64_t>(0, ItemKeyWireSize);
		}
		if (item.accessory)
		{
			w.Put<std::uint8_t>(item.accessory->unlockedCapacity);
			w.Put<std::uint8_t>(item.accessory->activeGrade);
			w.Put<std::uint64_t>(item.accessory->remainingMs);
			w.Put<std::uint8_t>(item.accessory->permanent ? 1 : 0);
		}
        if(item.automatic)
        {
            if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);
            const auto& row=std::get<ItemConsumableRow>(catalog.Find(item.key)->category);
            const auto& effect=std::get<AutomaticRecovery>(catalog.FindRecovery(row.recoveryProfile)->Data().effect);
            w.Put<std::uint8_t>(static_cast<unsigned>(effect.supply));
            w.Put<std::uint64_t>(item.automatic->remainingRaw);w.Put<std::uint8_t>(item.automatic->active);w.Put<std::uint64_t>(item.automatic->nextTickInMs);
        }
        if(item.toggle){if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);w.Put<std::uint64_t>(item.toggle->remainingMs);w.Put<std::uint8_t>(item.toggle->active);}
        if(item.bookSkill){if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);w.Put<std::uint16_t>(*item.bookSkill);}
        if(item.polymorph){if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);w.Put<std::uint32_t>(item.polymorph->boundForm.value_or(0));w.Put<std::uint16_t>(item.polymorph->bookGrade);w.Put<std::uint32_t>(item.polymorph->practiceRemaining);}
        if(item.tool){if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);w.Put<std::uint32_t>(item.tool->trainingPoints);w.Put<std::uint32_t>(item.tool->baitPower);}
		if (item.companion || item.growthPet)
		{
			if (!ValidatePersistentItem(catalog, item) || !item.placement ||
                (item.placement->kind!=ItemStorageKind::Inventory && item.placement->kind!=ItemStorageKind::PetCarrier))
				return std::unexpected(E::BadValue);
			const auto& row = std::get<ItemCompanionRow>(catalog.Find(item.key)->category);
			w.Put<std::uint8_t>(static_cast<std::uint8_t>(row.kind));
			const bool expired = item.growthPet && item.growthPet->life.clock == CompanionClock::ActiveTime && !item.growthPet->life.remainingMs;
			w.Put<std::uint8_t>(item.growthPet && item.growthPet->carrierInstanceId ? 4 : expired ? 3 : !RecallRequested(item) ? 0 : item.id.Value() == liveCompanion ? 1 : 2);
		}

        if(hasPetSupplies) {
            const auto* row=std::get_if<ItemPetSupplyRow>(&catalog.Find(item.key)->category);
            const auto* profile=row ? catalog.FindPetSupplyProfile(row->profile) : nullptr;
            w.Put<std::uint8_t>(item.petSupply ? static_cast<unsigned>(profile->kind) : 0);
            if(item.petSupply) {
                const auto payload=EncodePetSupplyState(catalog,item.key,*item.petSupply);if(!payload)return std::unexpected(E::BadValue);
                for(auto byte:*payload)w.Put(byte);
            }
        }

	}
	if (w.out.size() > MaxPacket) return std::unexpected(E::TooLarge);
	const auto size = static_cast<std::uint32_t>(w.out.size());
	for (std::size_t i = 0; i < 4; ++i) w.out[1 + i] = std::byte((size >> (i * 8)) & 0xffu);
	return std::move(w.out);
}

std::expected<TypedInventoryView, TypedInventoryWireError> DecodeTypedInventory(std::span<const std::byte> bytes, ItemOwner receiver)
{
	try
	{
		Reader r(bytes);
		if (r.Get<std::uint8_t>() != TypedInventoryHeader) return std::unexpected(E::BadHeader);
		if (r.Get<std::uint32_t>() != bytes.size()) return std::unexpected(E::BadSize);
		const auto version=r.Get<std::uint8_t>();if (version<TypedInventoryVersion || version>10) return std::unexpected(E::BadVersion);
		TypedInventoryView view;
		for (auto n = r.Get<std::uint16_t>(); n; --n)
		{
			ItemStorageLayout s;
			s.storage = { receiver, static_cast<ItemStorageKind>(r.Get<std::uint8_t>()), r.Get<std::uint64_t>() };
			s.columns = r.Get<std::uint16_t>(); s.rows = r.Get<std::uint16_t>(); s.pages = r.Get<std::uint16_t>();
			view.storages.push_back(s);
		}
		for (auto n = r.Get<std::uint8_t>(); n; --n)
		{
			ItemWallet wallet { receiver, static_cast<WalletCurrency>(r.Get<std::uint8_t>()), 0, 0 };
			wallet.balance = r.Get<std::uint64_t>();
			view.wallets.push_back(wallet);
		}
		for (auto n = r.Get<std::uint16_t>(); n; --n)
		{
			auto key = DecodeItemKey(r.Take(ItemKeyWireSize));
			if (!key) throw Bad {};
			const auto length = r.Get<std::uint8_t>();
			const auto text = r.Take(length);
			std::string code;
			for (auto b : text) code.push_back(static_cast<char>(b));
			if (!view.names.emplace(*key, std::move(code)).second) throw Bad {};
		}
		for (auto n = r.Get<std::uint16_t>(); n; --n)
		{
			auto id = DecodeItemInstanceId(r.Take(ItemInstanceIdWireSize));
			auto key = DecodeItemKey(r.Take(ItemKeyWireSize));
			if (!id || !key || !view.names.contains(*key)) throw Bad {};
			PersistentItem item { *id, *key, receiver };
			item.refinementLevel = r.Get<std::uint16_t>();
			item.count = r.Get<std::uint32_t>();
			const auto kind = r.Get<std::uint8_t>();
			const auto storage = r.Get<std::uint64_t>();
			const auto anchor = r.Get<std::uint32_t>();
			if (kind) item.placement = ItemPlacement { static_cast<ItemStorageKind>(kind), storage, anchor };
			const auto flags = r.Get<std::uint8_t>();
			if ((flags & ~((1u << (version + 1)) - 1u)) || (flags & 12)==12) throw Bad {};
			if (flags & 1) item.boundTo = receiver;
			const auto locked = r.Get<std::uint8_t>();
			if (locked != 255) { if (locked >= AttributeSlots) throw Bad {}; item.attributes.lockedSlot = locked; }
			for (auto b = r.Get<std::uint8_t>(); b; --b)
			{
				const auto slot = r.Get<std::uint8_t>();
				if (slot >= AttributeSlots || item.attributes.slots[slot]) throw Bad {};
				const auto bonus = static_cast<BonusId>(r.Get<std::uint32_t>());
				item.attributes.slots[slot] = BonusEntry { bonus, static_cast<std::int64_t>(r.Get<std::uint64_t>()) };
			}
			for (auto& stone : item.stones)
			{
				stone.state = static_cast<StoneSocketState>(r.Get<std::uint8_t>());
				stone.grade = r.Get<std::uint8_t>();
				stone.refinementLevel = r.Get<std::uint16_t>();
				const auto raw = r.Take(ItemKeyWireSize);
				if (!Zero(raw))
				{
					auto stoneKey = DecodeItemKey(raw);
					if (!stoneKey) throw Bad {};
					stone.stone = *stoneKey;
				}
			}
			if (flags & 2)
			{
				AccessorySocketState a;
				a.unlockedCapacity = r.Get<std::uint8_t>(); a.activeGrade = r.Get<std::uint8_t>();
				a.remainingMs = r.Get<std::uint64_t>();
				const auto permanent = r.Get<std::uint8_t>();
				if (permanent > 1) throw Bad {};
				a.permanent = permanent == 1;
				item.accessory = a;
			}
            if(flags&4){const auto supply=r.Get<std::uint8_t>();const auto raw=r.Get<std::uint64_t>();const auto active=r.Get<std::uint8_t>();const auto next=r.Get<std::uint64_t>();if(supply<1 || supply>2 || (supply==2 && raw) || raw>MaxItemStatRaw || active>1 || (active ? !next || next>INT64_MAX : next!=0))throw Bad{};item.automatic=AutomaticRecoveryState{static_cast<std::int64_t>(raw),active==1,next};}
            if(flags&8){const auto remaining=r.Get<std::uint64_t>();const auto active=r.Get<std::uint8_t>();if(remaining>INT64_MAX || active>1 || (active && !remaining) || item.count!=1 || item.key.Category()!=ItemCategory::Consumable)throw Bad{};item.toggle=ToggleEffectState{remaining,active==1};}
            if(flags&32){if(item.key.Category()!=ItemCategory::Polymorph || item.refinementLevel || (flags&30))throw Bad{};const auto form=r.Get<std::uint32_t>();const auto grade=r.Get<std::uint16_t>();const auto practice=r.Get<std::uint32_t>();if(grade>255)throw Bad{};item.polymorph=PolymorphInstanceState{form ? std::optional{form} : std::nullopt,grade,practice};}
            if(flags&16){const auto skill=r.Get<std::uint16_t>();if(!skill || skill>254 || item.key.Category()!=ItemCategory::SkillBook || item.refinementLevel || (flags&14))throw Bad{};item.bookSkill=skill;}
			if(flags&64){if(item.key.Category()!=ItemCategory::Tool || item.count!=1 || item.refinementLevel>255 || (flags&62))throw Bad{};item.tool=ToolInstanceState{r.Get<std::uint32_t>(),r.Get<std::uint32_t>()};if(item.tool->baitPower>INT32_MAX)throw Bad{};}
            if(item.key.Category()==ItemCategory::Tool && !item.tool)throw Bad{};
			if (flags & 128)
			{
				const auto companion = static_cast<CompanionKind>(r.Get<std::uint8_t>());
				const auto state = r.Get<std::uint8_t>();
				const bool growth = companion == CompanionKind::GrowthPet;
				if (item.key.Category()!=ItemCategory::Companion || item.count!=1 || (kind!=static_cast<unsigned>(ItemStorageKind::Inventory) && !(version>=10 && growth && kind==10))
					|| (flags&126) || (growth ? version<8 || item.refinementLevel || state>(version==8 ? 0 : version<10 ? 3 : 4) || (state==4)!=(kind==10)
						: (companion!=CompanionKind::Pet && companion!=CompanionKind::Mount) || state>2)
					|| !view.companions.emplace(item.id, std::pair{companion, state}).second) throw Bad{};
				if (!growth)
				{
					item.companion = CompanionState{};
					item.companion->recallRequested = state != 0;
				}
			}
			if (item.key.Category()==ItemCategory::Companion && !(flags&128)) throw Bad{};
            if(version>=10) {
                const auto supply=r.Get<std::uint8_t>();
                if(supply) {
                    if((supply!=7 && supply!=8) || item.key.Category()!=ItemCategory::Special || item.count!=1 || item.refinementLevel || (flags&254) || kind!=1)throw Bad{};
                    const auto bytes=r.Take(51);
                    const auto get=[&](unsigned at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::to_integer<std::uint64_t>(bytes[at+i])<<(8*i);return v;};
                    if(get(0,4)!=0x01495350 || get(4,2)!=static_cast<unsigned>(item.key.Category()) || get(6,4)!=item.key.LocalId() || get(34,1)>3)throw Bad{};
                    PetSupplyState s{get(10,8),get(18,8),get(26,8)};
                    s.lifetime.started=get(34,1)&1;s.lifetime.expired=get(34,1)&2;s.lifetime.deadlineUtcMs=get(35,8);s.lifetime.remainingMs=get(43,8);
                    if(s.createdAtUtcMs>INT64_MAX || s.lifetime.deadlineUtcMs>INT64_MAX || s.lifetime.remainingMs>INT64_MAX ||
                        (supply==7 && (s.equippedSeal || s.createdAtUtcMs || s.lifetime!=CostumeLifetimeState{})))throw Bad{};
                    item.petSupply=s;
                }
            }
            view.items.push_back(std::move(item));
		}
        for(const auto& item:view.items) {
            if(item.placement && item.placement->kind==ItemStorageKind::PetCarrier) {
                const auto box=std::ranges::find(view.items,item.placement->storageId,[](const auto& p){return p.id.Value();});
                if(box==view.items.end() || !box->petSupply || box->petSupply->containedSeal!=item.id.Value())throw Bad{};
            }
            if(item.petSupply && item.petSupply->containedSeal) {
                const auto pet=std::ranges::find(view.items,item.petSupply->containedSeal,[](const auto& p){return p.id.Value();});
                if(pet==view.items.end() || !pet->placement || pet->placement->kind!=ItemStorageKind::PetCarrier || pet->placement->storageId!=item.id.Value())throw Bad{};
            }
        }
		if (!r.Done()) return std::unexpected(E::TrailingBytes);
		return view;
	}
	catch (const Reader::Short&) { return std::unexpected(E::Truncated); }
	catch (const Bad&) { return std::unexpected(E::BadValue); }
}
} // namespace auriga::item
