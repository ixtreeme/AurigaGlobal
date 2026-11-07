#include <item/PersistentItems.hpp>
#include <algorithm>

namespace auriga::item {
bool ValidatePetSupplyState(const ItemCatalog& c,ItemKey key,const PetSupplyState& s,bool destroyed) {
    const auto* entry=c.Find(key);const auto* row=entry ? std::get_if<ItemPetSupplyRow>(&entry->category) : nullptr;
    const auto* profile=row ? c.FindPetSupplyProfile(row->profile) : nullptr;
    if(!profile || profile->kind!=PetSupplyKind::Carrier)return false;
    return !s.equippedSeal && !s.createdAtUtcMs && s.lifetime==CostumeLifetimeState{};
}
std::expected<std::vector<std::uint8_t>,ItemPersistenceError> EncodePetSupplyState(const ItemCatalog& c,ItemKey key,const PetSupplyState& s) {
    if(!ValidatePetSupplyState(c,key,s))return std::unexpected(ItemPersistenceError::InvalidItem);
    std::vector<std::uint8_t> bytes={'P','S','I',1};
    const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)bytes.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    put(static_cast<unsigned>(key.Category()),2);put(key.LocalId(),4);put(s.containedSeal,8);put(s.equippedSeal,8);put(s.createdAtUtcMs,8);
    put((s.lifetime.started ? 1 : 0)|(s.lifetime.expired ? 2 : 0),1);put(s.lifetime.deadlineUtcMs,8);put(s.lifetime.remainingMs,8);
    return bytes;
}
std::expected<PetSupplyState,ItemPersistenceError> DecodePetSupplyState(const ItemCatalog& c,ItemKey key,std::span<const std::uint8_t> bytes) {
    using E=ItemPersistenceError;
    if(bytes.size()!=51 || bytes[0]!='P' || bytes[1]!='S' || bytes[2]!='I' || bytes[3]!=1)return std::unexpected(E::InvalidStoredData);
    std::size_t at=4;const auto get=[&](unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(bytes[at++])<<(8*i);return v;};
    const auto category=get(2),local=get(4);if(category!=static_cast<unsigned>(key.Category()) || local!=key.LocalId())return std::unexpected(E::InvalidStoredData);
    PetSupplyState s;s.containedSeal=get(8);s.equippedSeal=get(8);s.createdAtUtcMs=get(8);const auto flags=get(1);
    if(flags>3)return std::unexpected(E::InvalidStoredData);
    s.lifetime.started=flags&1;s.lifetime.expired=flags&2;s.lifetime.deadlineUtcMs=get(8);s.lifetime.remainingMs=get(8);
    if(!ValidatePetSupplyState(c,key,s))return std::unexpected(E::InvalidStoredData);return s;
}
bool ValidPetCarrierLinks(std::span<const PersistentItem> items) {
    for(const auto& item:items)if(!item.destroyed) {
        if(item.growthPet && item.growthPet->carrierInstanceId) {
            const auto carrier=std::ranges::find_if(items,[&](const auto& p){return p.id.Value()==item.growthPet->carrierInstanceId && !p.destroyed;});
            if(carrier==items.end() || carrier->owner!=item.owner || !carrier->petSupply || carrier->petSupply->containedSeal!=item.id.Value())return false;
        }
        if(item.petSupply && item.petSupply->containedSeal) {
            const auto seal=std::ranges::find_if(items,[&](const auto& p){return p.id.Value()==item.petSupply->containedSeal && !p.destroyed;});
            if(seal==items.end() || seal->owner!=item.owner || !seal->growthPet || seal->growthPet->carrierInstanceId!=item.id.Value())return false;
        }
    }
    return true;
}
}
