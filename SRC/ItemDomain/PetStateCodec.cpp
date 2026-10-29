#include <item/PetLifecycle.hpp>
#include <item/ItemCatalog.hpp>
#include <stdexcept>
#include <type_traits>
namespace auriga::item {
namespace {
struct Writer {
    std::vector<std::uint8_t> bytes;
    template<class T> void Put(T value) {
        static_assert(std::is_unsigned_v<T>);
        for(unsigned i=0;i<sizeof(T);++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));
    }
};
struct Reader {
    std::span<const std::uint8_t> bytes;
    std::size_t at=0;
    template<class T> T Get() {
        static_assert(std::is_unsigned_v<T>);
        if(bytes.size()-at<sizeof(T))throw std::out_of_range("pet payload");
        T value=0;for(unsigned i=0;i<sizeof(T);++i)value|=T(bytes[at++])<<(i*8);return value;
    }
    bool Bool(){const auto value=Get<std::uint8_t>();if(value>1)throw std::out_of_range("pet flag");return value!=0;}
};
}
std::expected<std::vector<std::uint8_t>,PetLifecycleError> EncodeGrowthPetState(const ItemCatalog& c,ItemKey key,const GrowthPetState& s) {
    if(auto valid=ValidateGrowthPetState(c,key,s);!valid)return std::unexpected(valid.error());
    Writer w;w.Put<std::uint32_t>(0x54504741);w.Put<std::uint16_t>(1);
    w.Put<std::uint16_t>(static_cast<std::uint16_t>(key.Category()));w.Put<std::uint32_t>(key.LocalId());
    w.Put<std::uint8_t>(static_cast<std::uint8_t>(s.name.size()));w.bytes.insert(w.bytes.end(),s.name.begin(),s.name.end());
    w.Put(s.bornAtUtcMs);w.Put(s.ageMs);w.Put(s.progress.level);w.Put(s.progress.stage);
    w.Put(s.progress.mobExperience);w.Put(s.progress.itemExperience);
    w.Put<std::uint8_t>(static_cast<std::uint8_t>(s.life.clock));w.Put(s.life.capacityMs);w.Put(s.life.remainingMs);w.Put<std::uint8_t>(s.life.permanentGrant);
    w.Put(s.traits.maximumHpPercentTenths);w.Put(s.traits.monsterDamagePercentTenths);w.Put(s.traits.pveNormalDamagePercentTenths);
    for(const auto& slot:s.skills){w.Put<std::uint8_t>(slot.unlocked);w.Put(slot.skill);w.Put(slot.rank);}
    w.Put(s.carrierInstanceId);w.Put<std::uint8_t>(s.everSummoned);w.Put<std::uint8_t>(s.recallRequested);return std::move(w.bytes);
}
std::expected<GrowthPetState,PetLifecycleError> DecodeGrowthPetState(const ItemCatalog& c,ItemKey key,std::span<const std::uint8_t> bytes) {
    using E=PetLifecycleError;
    if(bytes.size()>512)return std::unexpected(E::InvalidCodec);
    try {
        Reader r{bytes};if(r.Get<std::uint32_t>()!=0x54504741 || r.Get<std::uint16_t>()!=1)return std::unexpected(E::InvalidCodec);
        const auto category=r.Get<std::uint16_t>();const auto local=r.Get<std::uint32_t>();
        if(category!=static_cast<std::uint16_t>(key.Category()) || local!=key.LocalId())return std::unexpected(E::InvalidCodec);
        const auto length=r.Get<std::uint8_t>();if(length>64 || length>bytes.size()-r.at)return std::unexpected(E::InvalidCodec);
        GrowthPetState s;s.name.assign(reinterpret_cast<const char*>(bytes.data()+r.at),length);r.at+=length;
        s.bornAtUtcMs=r.Get<std::uint64_t>();s.ageMs=r.Get<std::uint64_t>();
        s.progress.level=r.Get<std::uint16_t>();s.progress.stage=r.Get<std::uint8_t>();
        s.progress.mobExperience=r.Get<std::uint64_t>();s.progress.itemExperience=r.Get<std::uint64_t>();
        s.life.clock=static_cast<CompanionClock>(r.Get<std::uint8_t>());s.life.capacityMs=r.Get<std::uint64_t>();s.life.remainingMs=r.Get<std::uint64_t>();s.life.permanentGrant=r.Bool();
        s.traits.maximumHpPercentTenths=r.Get<std::uint16_t>();s.traits.monsterDamagePercentTenths=r.Get<std::uint16_t>();s.traits.pveNormalDamagePercentTenths=r.Get<std::uint16_t>();
        for(auto& slot:s.skills){slot.unlocked=r.Bool();slot.skill=r.Get<std::uint32_t>();slot.rank=r.Get<std::uint16_t>();}
        s.carrierInstanceId=r.Get<std::uint64_t>();s.everSummoned=r.Bool();s.recallRequested=r.Bool();if(r.at!=bytes.size())return std::unexpected(E::InvalidCodec);
        if(auto valid=ValidateGrowthPetState(c,key,s);!valid)return std::unexpected(valid.error());return s;
    }catch(const std::out_of_range&){return std::unexpected(E::InvalidCodec);}
}
}
