#include <item/EnergyCrystals.hpp>
#include <item/PersistentItems.hpp>
#include <item/EquipmentStats.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <map>
#include <set>
namespace auriga::item {
bool IsEnergyCrystal(const ItemCatalog& c,ItemKey key){
    const auto* e=c.Find(key);const auto* r=e ? std::get_if<ItemConsumableRow>(&e->category) : nullptr;
    const auto* p=r && r->timedEffectProfile ? c.FindTimedEffect(*r->timedEffectProfile) : nullptr;
    return p && p->utility==TimedEffectUtility::Energy;
}
bool ValidEnergyCrystalPercent(const ItemCatalog& c,ItemKey key,unsigned percent){
    const auto tiers=c.EnergyTiers(key);return percent && std::ranges::any_of(tiers,[&](const auto& t){return t.percent==percent;});
}
std::optional<EnergyCrystalState> RollEnergyCrystal(const ItemCatalog& c,ItemKey key,unsigned roll){
    if(!roll || roll>10000)return {};for(const auto& tier:c.EnergyTiers(key)){if(roll<=tier.chanceBp)return EnergyCrystalState{tier.percent};roll-=tier.chanceBp;}return {};
}
std::optional<EnergyCrystalState> InitialEnergyCrystal(const ItemCatalog& c,ItemKey key,std::span<const std::uint8_t,16> op,std::uint64_t instance){
    if(!instance || !IsEnergyCrystal(c,key))return {};
    std::array<unsigned char,64> input{},digest{};constexpr std::string_view domain="auriga.item.energy.v1";
    std::copy(domain.begin(),domain.end(),input.begin());std::copy(op.begin(),op.end(),input.begin()+24);
    for(unsigned i=0;i<8;++i)input[40+i]=static_cast<unsigned char>(instance>>(8*i));
    constexpr auto limit=(std::uint64_t{1}<<32)/10000*10000;
    for(std::uint64_t counter=0;;++counter){
        for(unsigned i=0;i<8;++i)input[48+i]=static_cast<unsigned char>(counter>>(8*i));unsigned n=0;
        if(EVP_Digest(input.data(),input.size(),digest.data(),&n,EVP_sha256(),nullptr)!=1 || n!=32)return {};
        std::uint32_t raw=0;for(unsigned i=0;i<4;++i)raw|=std::uint32_t{digest[i]}<<(8*i);
        if(raw<limit)return RollEnergyCrystal(c,key,raw%10000+1);
    }
}
std::optional<std::vector<BonusEntry>> EnergyEquipmentBonuses(const ItemCatalog& c,std::span<const PersistentItem> items,unsigned percent){
    if(!percent || percent>100)return {};std::map<BonusId,std::int64_t> sums;std::set<unsigned> slots;
    for(const auto& item:items){
        if(item.destroyed || !item.placement || item.placement->kind!=ItemStorageKind::Equipment || item.placement->anchor>7)continue;
        if(!ValidatePersistentItem(c,item) || !slots.insert(item.placement->anchor).second)return {};
        const auto category=item.key.Category();if(category!=ItemCategory::Weapon && category!=ItemCategory::Armor && category!=ItemCategory::Accessory)continue;
        for(unsigned i=0;i<item.attributes.slots.size();++i)if(item.attributes.lockedSlot!=i)if(const auto& b=item.attributes.slots[i]){
            const auto value=AddEquipmentValue(sums[b->bonus],b->valueScaled);if(!value)return {};sums[b->bonus]=*value;
        }
    }
    std::vector<BonusEntry> out;
    for(auto [id,value]:sums){
        const auto* definition=FindBonusDefinition(id);if(!definition)return {};
        const auto quantum=definition->unit==BonusUnit::Raw ? 1000 : definition->unit==BonusUnit::BasisPoints ? 100 : 1;
        const auto scaled=ScaleEquipmentValue(value/quantum,percent,100,true);if(!scaled)return {};
        const auto native=ScaleEquipmentValue(*scaled,quantum,1,true);if(!native)return {};if(*native)out.push_back({id,*native});
    }
    return out;
}
}
