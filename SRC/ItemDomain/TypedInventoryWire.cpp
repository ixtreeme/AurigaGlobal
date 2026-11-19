#include <item/TypedInventoryWire.hpp>
#include <item/PrivateShops.hpp>
#include <item/ItemCatalog.hpp>
#include <item/ItemWireCodec.hpp>
#include <item/SoulRuntime.hpp>

#include <limits>
#include <set>
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
	const bool hasGaya=std::ranges::any_of(wallets,[](const auto& wallet){return wallet.currency==WalletCurrency::Gaya;});
	const bool hasGrowthPets = std::ranges::any_of(items, [](const auto& item) { return item.growthPet.has_value(); });
	const bool hasPetSupplies=std::ranges::any_of(items,[](const auto& item){return item.petSupply.has_value();});
	const bool hasGrowthStatus = std::ranges::any_of(items, [](const auto& item) {
		return item.growthPet && (RecallRequested(item) ||
			(item.growthPet->life.clock == CompanionClock::ActiveTime && !item.growthPet->life.remainingMs));
	});
    const bool hasForge=std::ranges::any_of(items,[](const auto& i){return i.key.Category()==ItemCategory::ForgeAid;});
    const bool hasAmmo=std::ranges::any_of(items,[&](const auto& i){const auto& row=catalog.Find(i.key)->category;const auto* w=std::get_if<ItemWeaponRow>(&row);return std::holds_alternative<ItemAmmoRow>(row) || (w && w->subtype==WeaponSubtype::Bow);});
    const bool hasSoul=std::ranges::any_of(items,[&](const auto& i){return i.key.Category()==ItemCategory::Soul;});
    const bool hasSpecial=std::ranges::any_of(items,[](const auto& i){return i.regalia || i.bonusTool;});
    const auto permanentToggle=[&](const auto& item){if(!item.toggle)return false;const auto* row=std::get_if<ItemConsumableRow>(&catalog.Find(item.key)->category);const auto* profile=row && row->toggleEffectProfile ? catalog.FindToggleEffect(*row->toggleEffectProfile) : nullptr;return profile && profile->durationMs==0;};
    const bool hasPrivateShops=hasGaya || std::ranges::any_of(items,[](const auto& i){return i.shopPermitDeadline || i.privateShop;});
    const bool hasMoney=hasPrivateShops || std::ranges::any_of(items,[](const auto& i){return i.currencyAmount.has_value();});
    const bool hasEnergy=hasMoney || std::ranges::any_of(items,[](const auto& i){return i.energy.has_value();});
    const bool hasCostumeTransfer=hasEnergy || std::ranges::any_of(items,[&](const auto& i){const auto* tool=std::get_if<ItemBonusToolRow>(&catalog.Find(i.key)->category);return (tool && tool->operation==SpecialBonusOperation::CostumeTransfer) || (i.regalia && std::get<ItemRegaliaRow>(catalog.Find(i.key)->category).kind==RegaliaKind::Stole && !i.regalia->bonuses[5]);});
    const bool hasStatefulUtilities=hasCostumeTransfer || std::ranges::any_of(items,[](const auto& i){return i.recall.has_value() || (i.tool && i.tool->remainingUses.has_value());});
    const bool hasSashes=hasStatefulUtilities || std::ranges::any_of(items,[&](const auto& i){const auto* tool=std::get_if<ItemBonusToolRow>(&catalog.Find(i.key)->category);return i.sash.has_value() || (tool && tool->operation==SpecialBonusOperation::SashClear);});
    const bool hasUniqueClocks=hasSashes || std::ranges::any_of(items,[](const auto& i){return i.key.Category()==ItemCategory::Accessory && i.costume.has_value();});
    const bool hasStoleClocks=hasUniqueClocks || std::ranges::any_of(items,[](const auto& i){return i.key.Category()==ItemCategory::Regalia && i.costume.has_value();});
    const bool hasSoulTransitions=hasStoleClocks || std::ranges::any_of(items,[&](const auto& i){const auto* row=std::get_if<ItemSoulRow>(&catalog.Find(i.key)->category);const auto* rules=catalog.FindDefinitionRules(i.key,i.refinementLevel);return row && (row->nextCore.has_value() || (rules && rules->displayRefinementOffset));});
    const bool hasPermanentToggles=hasStoleClocks || hasSoulTransitions || std::ranges::any_of(items,permanentToggle);
    const bool hasWeaponCostumeEffects=hasPermanentToggles || std::ranges::any_of(items,[&](const auto& i){const auto* e=catalog.Find(i.key);const auto* row=e ? std::get_if<ItemCostumeRow>(&e->category) : nullptr;return row && row->kind==CostumeKind::WeaponEffect;});
    const bool hasCostumeEffects=hasWeaponCostumeEffects || std::ranges::any_of(items,[&](const auto& i){const auto* e=catalog.Find(i.key);const auto* row=e ? std::get_if<ItemCostumeRow>(&e->category) : nullptr;return row && row->bodyEffect!=0;});
    const bool hasCompanionCostumes=hasCostumeEffects || std::ranges::any_of(items,[&](const auto& i){const auto* e=catalog.Find(i.key);const auto* row=e ? std::get_if<ItemCostumeRow>(&e->category) : nullptr;return row && row->companionForm.has_value();});
    const bool hasCostumes=std::ranges::any_of(items,[](const auto& i){return i.costume.has_value();});
	const bool hasCompanions = std::ranges::any_of(items, [](const auto& item) { return item.companion.has_value(); });
	// v18 (C47): the character special inventory storage (kind 11) is present.
	const bool hasSpecialInventory=hasCompanionCostumes || std::ranges::any_of(storages,[](const auto& s){return s.storage.kind==ItemStorageKind::SpecialInventory;});
	w.Put<std::uint8_t>(hasGaya ? 32 : hasPrivateShops ? 31 : hasMoney ? 30 : hasEnergy ? 29 : hasCostumeTransfer ? 28 : hasStatefulUtilities ? 27 : hasSashes ? 26 : hasUniqueClocks ? 25 : hasStoleClocks ? 24 : hasSoulTransitions ? 23 : hasPermanentToggles ? 22 : hasWeaponCostumeEffects ? 21 : hasCostumeEffects ? 20 : hasCompanionCostumes ? 19 : hasSpecialInventory ? 18 : hasForge ? 17 : hasAmmo ? 16 : hasSoul ? 15 : hasSpecial ? 14 : hasCostumes ? 13 : hasCompanions ? 12 : hasPetSupplies ? 10 : hasGrowthStatus ? 9 : hasGrowthPets ? 8 : std::ranges::any_of(items,[](const auto& i){return i.tool.has_value();}) ? 6 : std::ranges::any_of(items,[](const auto& i){return i.polymorph.has_value();}) ? 5 : std::ranges::any_of(items,[](const auto& i){return i.bookSkill.has_value();}) ? 4 : std::ranges::any_of(items,[](const auto& i){return i.toggle.has_value();}) ? 3 : std::ranges::any_of(items,[](const auto& i){return i.automatic.has_value();}) ? 2 : TypedInventoryVersion);
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
        if(item.toggle){if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);w.Put<std::uint64_t>(item.toggle->remainingMs);w.Put<std::uint8_t>((item.toggle->active ? 1 : 0)|(permanentToggle(item) ? 2 : 0));}
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
			w.Put<std::uint8_t>(item.growthPet && item.growthPet->carrierInstanceId ? 4 : expired ? 3 : !RecallRequested(item) ? 0 :
                item.companion && item.companion->ridingRequested ? item.id.Value()==liveCompanion ? 5 : 6 : item.id.Value() == liveCompanion ? 1 : 2);
            if(item.companion) {
                const auto& life=item.companion->lifetime;w.Put<std::uint8_t>(static_cast<unsigned>(row.clock));w.Put<std::uint8_t>(life.started);
                w.Put(row.durationMs);w.Put(life.deadlineUtcMs);w.Put(life.remainingMs);w.Put<std::uint8_t>(row.flyingMount);
            }
		}

        if(hasPetSupplies || hasCompanions || hasCostumes || hasSpecial || hasSoul || hasAmmo || hasForge || hasSpecialInventory) {
            const auto* row=std::get_if<ItemPetSupplyRow>(&catalog.Find(item.key)->category);
            const auto* profile=row ? catalog.FindPetSupplyProfile(row->profile) : nullptr;
            w.Put<std::uint8_t>(item.petSupply ? static_cast<unsigned>(profile->kind) : 0);
            if(item.petSupply) {
                const auto payload=EncodePetSupplyState(catalog,item.key,*item.petSupply);if(!payload)return std::unexpected(E::BadValue);
                for(auto byte:*payload)w.Put(byte);
                if(profile->skin){w.Put<std::uint8_t>(static_cast<unsigned>(profile->skin->clock));w.Put(profile->skin->durationMs);w.Put(profile->skin->form);}
            }
        }

        if(hasCostumes || hasSpecial || hasSoul || hasAmmo || hasForge || hasSpecialInventory) {
            w.Put<std::uint8_t>(item.costume.has_value() && item.key.Category()==ItemCategory::Costume);
            if(item.costume && item.key.Category()==ItemCategory::Costume){const auto& row=std::get<ItemCostumeRow>(catalog.Find(item.key)->category);w.Put<std::uint8_t>(static_cast<unsigned>(row.kind));w.Put<std::uint8_t>(row.weapon ? static_cast<unsigned>(*row.weapon) : 0);w.Put<std::uint8_t>(static_cast<unsigned>(row.clock));w.Put(row.durationMs);if(hasCompanionCostumes)w.Put<std::uint32_t>(row.companionForm.value_or(0));if(hasCostumeEffects)w.Put(row.bodyEffect);if(hasWeaponCostumeEffects)for(auto effect:row.weaponEffects)w.Put(effect);for(auto byte:EncodeCostumeLifetime(*item.costume))w.Put(byte);}
        }

        if(hasSpecial || hasSoul || hasAmmo || hasForge || hasSpecialInventory){
            if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);
            w.Put<std::uint8_t>(item.regalia.has_value());if(item.regalia){const auto& row=std::get<ItemRegaliaRow>(catalog.Find(item.key)->category);w.Put<std::uint8_t>(static_cast<unsigned>(row.kind));w.Put(row.slot);w.Put(row.grade);for(auto b:EncodeRegaliaState(*item.regalia))w.Put(b);w.Put<std::uint8_t>(row.rules.size());for(const auto& rule:row.rules){w.Put(rule.index);w.Put(rule.variant);w.Put(rule.minimumChargeBp);w.Put<std::uint32_t>(static_cast<unsigned>(rule.bonus.bonus));w.Put<std::uint64_t>(rule.bonus.valueScaled);}}
            if(hasStoleClocks){const bool timed=(item.key.Category()==ItemCategory::Regalia || item.key.Category()==ItemCategory::Accessory) && item.costume.has_value();w.Put<std::uint8_t>(timed);if(timed){const auto clock=FindWearableClock(catalog,item.key);if(!clock)return std::unexpected(E::BadValue);w.Put<std::uint8_t>(static_cast<unsigned>(clock->clock));w.Put(clock->durationMs);for(auto b:EncodeCostumeLifetime(*item.costume))w.Put(b);if(item.key.Category()==ItemCategory::Accessory){const auto* rules=catalog.FindDefinitionRules(item.key,item.refinementLevel);w.Put<std::uint8_t>(!rules || rules->allowUnequip);}}}
            if(hasSashes){w.Put<std::uint8_t>(item.sash.has_value());if(item.sash){
                const auto* profile=catalog.FindAbsorptionSash(item.key);const auto blob=EncodeAbsorptionSashState(*item.sash);if(!profile || !blob)return std::unexpected(E::BadValue);
                w.Put(profile->grade);for(auto b:*blob)w.Put(b);
                const auto* donor=item.sash->donor ? catalog.FindSashDonor(item.sash->donor->key,item.sash->donor->refinement) : nullptr;
                const auto contributions=donor ? AbsorptionSashContributions(*donor,*item.sash) : std::vector<BonusEntry>{};
                w.Put<std::uint8_t>(contributions.size());for(auto b:contributions){w.Put<std::uint32_t>(static_cast<unsigned>(b.bonus));w.Put<std::uint64_t>(b.valueScaled);}
            }}
            w.Put<std::uint8_t>(item.bonusTool.has_value());if(item.bonusTool){const auto& row=std::get<ItemBonusToolRow>(catalog.Find(item.key)->category);w.Put<std::uint8_t>(static_cast<unsigned>(row.operation));w.Put(row.units);w.Put(row.parameter);for(auto b:EncodeBonusToolState(*item.bonusTool))w.Put(b);w.Put<std::uint8_t>(row.tiers.size());for(const auto& t:row.tiers){w.Put(t.draw);w.Put(t.tier);w.Put(t.chanceBp);}}
        }
        if(hasSoul || hasAmmo || hasForge || hasSpecialInventory){const auto* row=std::get_if<ItemSoulRow>(&catalog.Find(item.key)->category);w.Put<std::uint8_t>(row ? static_cast<unsigned>(row->kind) : 0);if(row){if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);w.Put<std::uint8_t>(row->channel ? static_cast<unsigned>(*row->channel) : 0);w.Put(row->refineChanceBp);if(item.soul){for(auto b:EncodeSoulState(*item.soul))w.Put(b);const auto& levels=catalog.FindSoulProfile(*row->profile)->levels;w.Put<std::uint16_t>(levels.size());for(const auto& l:levels){w.Put(l.level);w.Put(l.chargeCapacity);w.Put(l.cycleChargeCost);w.Put(l.strikesPerCycle);w.Put(l.initialCharge);w.Put(l.initialStrikes);w.Put(l.rechargeIntervalMs);w.Put(l.firstMultiplierBp);w.Put(l.secondMultiplierBp);w.Put(l.thirdMultiplierBp);}if(hasSoulTransitions || hasStoleClocks){const auto* rules=catalog.FindDefinitionRules(item.key,item.refinementLevel);w.Put<std::uint16_t>(item.refinementLevel+(rules ? rules->displayRefinementOffset : 0));w.Put<std::uint8_t>(row->nextCore.has_value());if(row->nextCore){w.Put<std::uint16_t>(static_cast<unsigned>(row->nextCore->Category()));w.Put(row->nextCore->LocalId());}}}}}
        if(hasAmmo || hasForge || hasSpecialInventory){const auto& row=catalog.Find(item.key)->category;const auto* bow=std::get_if<ItemWeaponRow>(&row);const auto* ammo=std::get_if<ItemAmmoRow>(&row);const auto policy=ammo?ammo->ammoPolicy:bow && bow->subtype==WeaponSubtype::Bow?bow->ammoPolicy:0;w.Put<std::uint8_t>(ammo?2:policy?1:0);if(policy){w.Put(policy);w.Put(catalog.FindAmmoPolicy(policy)->unitsPerHit);}}
        if(hasForge || hasSpecialInventory){const auto* row=std::get_if<ItemForgeAidRow>(&catalog.Find(item.key)->category);w.Put<std::uint8_t>(row!=nullptr);if(row){w.Put<std::uint8_t>(unsigned(row->kind));w.Put(row->units);w.Put(row->targetMask);w.Put(row->minimumLevel);w.Put(row->maximumLevel);w.Put<std::uint8_t>(unsigned(row->chanceMode));w.Put(row->chanceBp);w.Put<std::uint8_t>(unsigned(row->failure));w.Put(row->boosterGroup);w.Put<std::uint16_t>(row->levels.size());for(const auto& l:row->levels){w.Put(l.level);w.Put(l.chanceBp);}}}
        if(hasStatefulUtilities){
            if(!ValidatePersistentItem(catalog,item))return std::unexpected(E::BadValue);
            const auto flags=(item.recall ? 1 : 0)|(item.tool && item.tool->remainingUses ? 2 : 0);w.Put<std::uint8_t>(flags);
            if(item.recall){const auto blob=EncodeRecallItemState(*item.recall);if(blob.empty())return std::unexpected(E::BadValue);w.Put<std::uint8_t>(blob.size());for(auto b:blob)w.Put(b);}
            if(item.tool && item.tool->remainingUses)w.Put(*item.tool->remainingUses);
            if(hasEnergy){w.Put<std::uint8_t>(item.energy.has_value());if(item.energy){w.Put(item.energy->percent);const auto& row=std::get<ItemConsumableRow>(catalog.Find(item.key)->category);w.Put(catalog.FindTimedEffect(*row.timedEffectProfile)->durationMs);}}
            if(hasMoney){w.Put<std::uint8_t>(item.currencyAmount.has_value());if(item.currencyAmount)w.Put(*item.currencyAmount);}
            if(hasPrivateShops){
                w.Put<std::uint8_t>(item.shopPermitDeadline.has_value());if(item.shopPermitDeadline)w.Put(*item.shopPermitDeadline);
                w.Put<std::uint8_t>(item.privateShop.has_value());if(item.privateShop){if(!ValidPrivateShopListing(*item.privateShop) || !PrivateShopDisplayCells(catalog,item.key,item.privateShop->cell))return std::unexpected(E::BadValue);for(auto b:item.privateShop->shop)w.Put(b);w.Put(item.privateShop->price);w.Put(item.privateShop->cell);}
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
		const auto version=r.Get<std::uint8_t>();if (version<TypedInventoryVersion || version>32) return std::unexpected(E::BadVersion);
		TypedInventoryView view;
		for (auto n = r.Get<std::uint16_t>(); n; --n)
		{
			ItemStorageLayout s;
			s.storage = { receiver, static_cast<ItemStorageKind>(r.Get<std::uint8_t>()), r.Get<std::uint64_t>() };
			s.columns = r.Get<std::uint16_t>(); s.rows = r.Get<std::uint16_t>(); s.pages = r.Get<std::uint16_t>();
			if (s.storage.kind == ItemStorageKind::SpecialInventory && version < 18) throw Bad {}; // C47
			view.storages.push_back(s);
		}
		for (auto n = r.Get<std::uint8_t>(); n; --n)
		{
			ItemWallet wallet { receiver, static_cast<WalletCurrency>(r.Get<std::uint8_t>()), 0, 0 };
			wallet.balance = r.Get<std::uint64_t>();
			view.wallets.push_back(wallet);
			if(wallet.currency==WalletCurrency::Gaya && version<32)return std::unexpected(E::BadVersion);
		}
		if(version==32 && std::ranges::none_of(view.wallets,[](const auto& wallet){return wallet.currency==WalletCurrency::Gaya;}))return std::unexpected(E::BadVersion);
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
            if(flags&8){const auto remaining=r.Get<std::uint64_t>();const auto active=r.Get<std::uint8_t>();if(remaining>INT64_MAX || active>(version>=22 ? 3 : 1) || ((active&2) ? remaining!=0 : (active&1) && !remaining) || item.count!=1 || item.key.Category()!=ItemCategory::Consumable)throw Bad{};item.toggle=ToggleEffectState{remaining,(active&1)!=0};}
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
						: (companion!=CompanionKind::Pet && companion!=CompanionKind::Mount) ||
                            (state>2 && !(version>=11 && companion==CompanionKind::Mount && (state==5 || state==6))))
					|| !view.companions.emplace(item.id, std::pair{companion, state}).second) throw Bad{};
				if (!growth)
				{
					item.companion = CompanionState{};
					item.companion->recallRequested = state != 0;
					item.companion->ridingRequested = state==5 || state==6;
                    if(version>=12) {
                        const auto clock=static_cast<CompanionClock>(r.Get<std::uint8_t>());const auto started=r.Get<std::uint8_t>();
                        const auto duration=r.Get<std::uint64_t>();CompanionLifetime life;life.started=started;life.deadlineUtcMs=r.Get<std::uint64_t>();life.remainingMs=r.Get<std::uint64_t>();
                        const auto flying=r.Get<std::uint8_t>();ItemCompanionRow rule{item.key,companion,1,companion==CompanionKind::Mount ? std::optional<std::uint32_t>{1} : std::nullopt,clock,duration};rule.flyingMount=flying;
                        item.companion->lifetime=life;
                        if(started>1 || flying>1 || !ValidCompanion(rule) || !ValidCompanionState(rule,*item.companion))throw Bad{};
                        view.fixedRules.emplace(item.id,TypedInventoryView::FixedRule{clock,duration,flying!=0});
                    }
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
                    if(supply==8) {
                        const auto clock=static_cast<CostumeClock>(r.Get<std::uint8_t>());const auto duration=r.Get<std::uint64_t>();const auto form=r.Get<std::uint32_t>();
                        if(s.containedSeal || !s.createdAtUtcMs || (s.lifetime.expired && s.equippedSeal) || !form ||
                            !AdvanceItemClock({clock,duration},s.lifetime,{0,0,s.equippedSeal!=0,false}))throw Bad{};
                    }
                }
            }
            if(version>=13) {
                const auto present=r.Get<std::uint8_t>();if(present>1)throw Bad{};
                if(present) {
                    const auto costumeKind=static_cast<CostumeKind>(r.Get<std::uint8_t>());const auto weapon=r.Get<std::uint8_t>();const auto clock=static_cast<CostumeClock>(r.Get<std::uint8_t>());const auto duration=r.Get<std::uint64_t>();const auto form=version>=19 ? r.Get<std::uint32_t>() : 0;const auto effect=version>=20 ? r.Get<std::uint16_t>() : std::uint16_t{0};
                    std::array<std::uint16_t,8> weaponEffects{};if(version>=21)for(auto& id:weaponEffects)id=r.Get<std::uint16_t>();
                    const auto raw=r.Take(22);std::array<std::uint8_t,22> data;for(unsigned n=0;n<22;++n)data[n]=std::to_integer<std::uint8_t>(raw[n]);const auto life=DecodeCostumeLifetime(data);
                    ItemCostumeRow rule{item.key,costumeKind,weapon ? std::optional{static_cast<WeaponSubtype>(weapon)} : std::nullopt,clock,duration,form ? std::optional{form} : std::nullopt,effect,weaponEffects};
                    if(!life || !ValidCostumeLifetime(rule,*life,kind==2) || life->expired || item.count!=1 || (kind==2 && item.placement->anchor!=CostumeSlot(costumeKind)-1))throw Bad{};
                    item.costume=*life;view.costumeRules.emplace(item.id,rule);
                } else if(item.key.Category()==ItemCategory::Costume)throw Bad{};
            }
            if(version>=14){
                const auto regalia=r.Get<std::uint8_t>();if(regalia>1)throw Bad{};
                if(regalia){const auto type=static_cast<RegaliaKind>(r.Get<std::uint8_t>());const auto slot=r.Get<std::uint8_t>(),grade=r.Get<std::uint8_t>();const auto raw=r.Take(93);std::vector<std::uint8_t> blob;for(auto b:raw)blob.push_back(std::to_integer<std::uint8_t>(b));const auto state=DecodeRegaliaState(blob);if(!state || (version<28 && type==RegaliaKind::Stole && !state->bonuses[5]))throw Bad{};ItemRegaliaRow rule{item.key,type,slot,grade,state->capacityMs,0,{}};const auto n=r.Get<std::uint8_t>();if(n>48)throw Bad{};for(unsigned j=0;j<n;++j)rule.rules.push_back({r.Get<std::uint8_t>(),r.Get<std::uint8_t>(),r.Get<std::uint16_t>(),{static_cast<BonusId>(r.Get<std::uint32_t>()),static_cast<std::int64_t>(r.Get<std::uint64_t>())}});if(!ValidRegaliaState(rule,*state) || item.count!=1 || (state->active && kind!=2) || (kind==2 && item.placement->anchor!=RegaliaSlot(rule)-1))throw Bad{};item.regalia=*state;view.regaliaRules.emplace(item.id,rule);}
                else if(version<26 && item.key.Category()==ItemCategory::Regalia)throw Bad{};
                if(version>=24){
                    const auto timed=r.Get<std::uint8_t>();if(timed>1)throw Bad{};
                    if(timed){
                        const CostumeClockDefinition clock{static_cast<CostumeClock>(r.Get<std::uint8_t>()),r.Get<std::uint64_t>()};
                        const auto raw=r.Take(22);std::array<std::uint8_t,22> data;for(unsigned j=0;j<22;++j)data[j]=std::to_integer<std::uint8_t>(raw[j]);const auto life=DecodeCostumeLifetime(data);
                        const bool unique=version>=25 && item.key.Category()==ItemCategory::Accessory;
                        const bool stole=item.regalia && view.regaliaRules.at(item.id).kind==RegaliaKind::Stole;
                        if((!unique && !stole) || item.costume || item.count!=1 || (unique && kind==2 && !FitsEquipmentSlot(EquipmentSlot::Unique,item.placement->anchor)) || clock.clock==CostumeClock::Permanent || !life || life->expired || !ValidItemClock(clock,*life,kind==2))throw Bad{};
                        item.costume=*life;view.stoleClocks.emplace(item.id,clock);
                        if(unique){const auto allowed=r.Get<std::uint8_t>();if(allowed>1)throw Bad{};view.uniqueUnequipAllowed.emplace(item.id,allowed!=0);}
                    }
                }
                if(version>=26){const auto has=r.Get<std::uint8_t>();if(has>1)throw Bad{};if(has){
                    const auto grade=r.Get<std::uint8_t>();const auto raw=r.Take(110);std::vector<std::uint8_t> blob;for(auto b:raw)blob.push_back(std::to_integer<std::uint8_t>(b));const auto state=DecodeAbsorptionSashState(blob);
                    const std::array<unsigned,4> low{1,5,10,11},high{1,5,10,25};
                    if(!state || grade<1 || grade>4 || state->percent<low[grade-1] || state->percent>high[grade-1] || item.key.Category()!=ItemCategory::Regalia || item.regalia || item.costume || item.count!=1 || item.refinementLevel || (kind==2 && item.placement->anchor!=28))throw Bad{};
                    item.sash=*state;view.sashGrades.emplace(item.id,grade);const auto count=r.Get<std::uint8_t>();if(count>15 || (!state->donor && count))throw Bad{};
                    auto& contributions=view.sashContributions[item.id];for(unsigned j=0;j<count;++j){BonusEntry b{static_cast<BonusId>(r.Get<std::uint32_t>()),static_cast<std::int64_t>(r.Get<std::uint64_t>())};const auto* d=FindBonusDefinition(b.bonus);if(!d || b.valueScaled<=0 || b.valueScaled>d->maximum)throw Bad{};contributions.push_back(b);}
                }if(item.key.Category()==ItemCategory::Regalia && !item.sash && !item.regalia)throw Bad{};}
                const auto tool=r.Get<std::uint8_t>();if(tool>1)throw Bad{};
                if(tool){ItemBonusToolRow rule{item.key,static_cast<SpecialBonusOperation>(r.Get<std::uint8_t>()),r.Get<std::uint16_t>(),r.Get<std::uint16_t>()};const auto raw=r.Take(18);std::vector<std::uint8_t> blob;for(auto b:raw)blob.push_back(std::to_integer<std::uint8_t>(b));const auto state=DecodeBonusToolState(blob);if(!state)throw Bad{};if(version<28 && rule.operation==SpecialBonusOperation::CostumeTransfer)throw Bad{};rule.payload=state->payload;const auto n=r.Get<std::uint8_t>();if(n>48)throw Bad{};for(unsigned j=0;j<n;++j)rule.tiers.push_back({r.Get<std::uint8_t>(),r.Get<std::uint8_t>(),r.Get<std::uint16_t>()});if(!ValidBonusToolState(rule,*state) || kind!=1)throw Bad{};item.bonusTool=*state;view.bonusToolRules.emplace(item.id,rule);}
                else if(item.key.Category()==ItemCategory::BonusTool)throw Bad{};
            }
            if(version>=15){const auto type=r.Get<std::uint8_t>();if(type>2)throw Bad{};if(type){ItemSoulRow row{item.key,static_cast<SoulKind>(type)};const auto channel=r.Get<std::uint8_t>();row.refineChanceBp=r.Get<std::uint16_t>();if(item.key.Category()!=ItemCategory::Soul || kind!=1 || row.refineChanceBp>10000)throw Bad{};if(type==1){if(channel<1 || channel>2 || row.refineChanceBp || item.count!=1)throw Bad{};row.channel=static_cast<SoulChannel>(channel);const auto raw=r.Take(21);std::vector<std::uint8_t> b;for(auto v:raw)b.push_back(std::to_integer<std::uint8_t>(v));item.soul=DecodeSoulState(b);SoulProfile profile{1,"wire.soul",{}};const auto n=r.Get<std::uint16_t>();if(n<1 || n>256 || item.refinementLevel>=n)throw Bad{};for(unsigned j=0;j<n;++j)profile.levels.push_back({r.Get<std::uint16_t>(),r.Get<std::uint32_t>(),r.Get<std::uint32_t>(),r.Get<std::uint32_t>(),r.Get<std::uint32_t>(),r.Get<std::uint32_t>(),r.Get<std::uint64_t>(),r.Get<std::uint32_t>(),r.Get<std::uint32_t>(),r.Get<std::uint32_t>()});if(!item.soul || !ValidSoulProfile(profile))throw Bad{};const auto& l=profile.levels[item.refinementLevel];const auto& s=*item.soul;if(s.charge>l.chargeCapacity || s.strikes>l.strikesPerCycle || s.rechargeRemainderMs>=l.rechargeIntervalMs || (s.charge==l.chargeCapacity && s.rechargeRemainderMs) || (s.active && s.charge<l.cycleChargeCost))throw Bad{};if(version>=23){const auto grade=r.Get<std::uint16_t>();view.soulGrades.emplace(item.id,grade);const auto has=r.Get<std::uint8_t>();if(has>1)throw Bad{};if(has){const auto category=r.Get<std::uint16_t>();const auto local=r.Get<std::uint32_t>();const auto next=ItemKey::Create(category,local);if(!next || category!=14 || *next==item.key)throw Bad{};row.nextCore=*next;}}view.soulRules.emplace(item.id,std::move(profile));}else if(channel || item.refinementLevel)throw Bad{};view.souls.emplace(item.id,row);}else if(item.key.Category()==ItemCategory::Soul)throw Bad{};}
            if(version>=16){const auto type=r.Get<std::uint8_t>();if(type>2)throw Bad{};if(type){const auto policy=r.Get<std::uint32_t>();const auto units=r.Get<std::uint16_t>();if(!policy || !units || (type==1?item.key.Category()!=ItemCategory::Weapon:item.key.Category()!=ItemCategory::Ammo || item.refinementLevel || (kind==2 && item.placement->anchor!=19)))throw Bad{};view.ammunition.emplace(item.id,TypedInventoryView::AmmoRule{type,policy,units});}else if(item.key.Category()==ItemCategory::Ammo)throw Bad{};}
            else if(item.key.Category()==ItemCategory::Ammo)throw Bad{};
            if(version>=17){const auto present=r.Get<std::uint8_t>();if(present>1)throw Bad{};if(present){ItemForgeAidRow row{item.key,static_cast<ForgeAidKind>(r.Get<std::uint8_t>()),r.Get<std::uint16_t>(),r.Get<std::uint8_t>(),r.Get<std::uint16_t>(),r.Get<std::uint16_t>(),static_cast<ForgeChance>(r.Get<std::uint8_t>()),r.Get<std::uint16_t>(),static_cast<ForgeFailure>(r.Get<std::uint8_t>()),r.Get<std::uint8_t>(),{}};const auto n=r.Get<std::uint16_t>();if(n>256)throw Bad{};for(unsigned j=0;j<n;++j)row.levels.push_back({r.Get<std::uint16_t>(),r.Get<std::uint16_t>()});if(!ValidForgeAid(row) || item.refinementLevel || kind==2 || kind==10 || item.count<1)throw Bad{};view.forgeAids.emplace(item.id,std::move(row));}else if(item.key.Category()==ItemCategory::ForgeAid)throw Bad{};}else if(item.key.Category()==ItemCategory::ForgeAid)throw Bad{};
            if(version>=27){const auto auxiliary=r.Get<std::uint8_t>();if(auxiliary>3)throw Bad{};
                if(auxiliary&1){const auto n=r.Get<std::uint8_t>();if(n!=5 && n!=17)throw Bad{};const auto raw=r.Take(n);std::vector<std::uint8_t> blob;for(auto b:raw)blob.push_back(std::to_integer<std::uint8_t>(b));const auto state=DecodeRecallItemState(blob);if(!state || item.key.Category()!=ItemCategory::Quest || (state->destination && item.count!=1))throw Bad{};item.recall=*state;}
                if(auxiliary&2){const auto uses=r.Get<std::uint16_t>();if(!uses || !item.tool || item.key.Category()!=ItemCategory::Tool || item.count!=1 || item.refinementLevel || item.tool->trainingPoints || item.tool->baitPower || kind==2)throw Bad{};item.tool->remainingUses=uses;}
            }
            if(version>=29){const auto present=r.Get<std::uint8_t>();if(present>1)throw Bad{};if(present){const auto percent=r.Get<std::uint8_t>();if(!percent || percent>100 || item.key.Category()!=ItemCategory::Consumable || item.refinementLevel || kind==2 || item.attributes!=InstanceAttributes{})throw Bad{};const auto duration=r.Get<std::uint64_t>();if(!duration || duration>INT64_MAX)throw Bad{};item.energy=EnergyCrystalState{percent};view.energyDurations.emplace(item.id,duration);}}
            if(version>=30){const auto present=r.Get<std::uint8_t>();if(present>1)throw Bad{};if(present){const auto amount=r.Get<std::uint64_t>();if(!amount || amount>INT64_MAX || item.key.Category()!=ItemCategory::CurrencyToken || item.count!=1 || item.refinementLevel || kind==2 || item.attributes!=InstanceAttributes{})throw Bad{};item.currencyAmount=amount;}}
            if(version>=31){
                const auto permit=r.Get<std::uint8_t>();if(permit>1)throw Bad{};if(permit){const auto deadline=r.Get<std::uint64_t>();if(!deadline || deadline>INT64_MAX || item.key.Category()!=ItemCategory::Quest || item.count!=1 || item.refinementLevel || kind==2)throw Bad{};item.shopPermitDeadline=deadline;}
                const auto listed=r.Get<std::uint8_t>();if(listed>1)throw Bad{};if(listed){PrivateShopListing state;for(auto& b:state.shop)b=r.Get<std::uint8_t>();state.price=r.Get<std::uint64_t>();state.cell=r.Get<std::uint16_t>();if(!ValidPrivateShopListing(state) || kind!=1 || item.boundTo)throw Bad{};item.privateShop=state;}
            }
            view.items.push_back(std::move(item));
		}
        std::set<SoulChannel> activeSouls;for(const auto& i:view.items)if(i.soul && i.soul->active && !activeSouls.insert(*view.souls.at(i.id).channel).second)throw Bad{};
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
