#include <item/AbsorptionSashes.hpp>
#include <algorithm>
#include <climits>
#include <openssl/sha.h>

namespace auriga::item {
namespace {
bool Positive(const BonusEntry& b) {
    const auto* d=FindBonusDefinition(b.bonus);
    return d && b.valueScaled>0 && b.valueScaled>=d->minimum && b.valueScaled<=d->maximum;
}
bool Payload(const AbsorptionSashState& s) {
    return s.percent>=1 && s.percent<=25 && (!s.donor ||
        ((s.donor->key.Category()==ItemCategory::Weapon || s.donor->key.Category()==ItemCategory::Armor) &&
        s.donor->refinement<=255 && std::ranges::all_of(s.donor->attributes,[](const auto& b){return !b || Positive(*b);})));
}
std::uint64_t Unit(BonusId id) {
    const auto* d=FindBonusDefinition(id);
    return !d ? 0 : d->unit==BonusUnit::Raw ? 1000 : d->unit==BonusUnit::BasisPoints ? 100 : 1;
}
std::int64_t Rounded(std::int64_t value,std::uint8_t percent,std::uint64_t unit) {
    if(value<=0 || !unit || !percent || percent>25)return 0;
    const auto divisor=unit*100,raw=static_cast<std::uint64_t>(value);
    return static_cast<std::int64_t>(((raw/divisor)*percent+((raw%divisor)*percent+divisor/2)/divisor)*unit);
}
void Put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned at,unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(b[at+i])<<(8*i);return v;}
}
bool ValidAbsorptionSashProfile(const AbsorptionSashProfile& p) {
    if(p.key.Category()!=ItemCategory::Regalia || p.next.Category()!=ItemCategory::Regalia || p.grade<1 || p.grade>4 || p.maximumPercent!=25 || !p.successBp || p.successBp>10000 || !p.goldCost || p.goldCost>INT64_MAX)return false;
    const std::array<unsigned,4> initialLow{1,5,10,11},initialHigh{1,5,10,19},nextLow{5,10,12,1},nextHigh{5,10,19,5};
    const auto n=p.grade-1;
    return p.initialMinimum==initialLow[n] && p.initialMaximum==initialHigh[n] && p.combineMinimum==nextLow[n] && p.combineMaximum==nextHigh[n] && (p.grade!=4 || p.next==p.key);
}
bool ValidSashDonorProfile(const SashDonorProfile& p) {
    return (p.key.Category()==ItemCategory::Weapon || p.key.Category()==ItemCategory::Armor) && p.refinement<=255 &&
        p.physicalMaximumRaw>=0 && p.magicalMaximumRaw>=0 && p.physicalMaximumRaw<=1000000000000000LL && p.magicalMaximumRaw<=1000000000000000LL &&
        (p.key.Category()==ItemCategory::Weapon || (!p.physicalMaximumRaw && !p.magicalMaximumRaw && !p.physicalBasePositive && !p.magicalBasePositive)) &&
        (!p.physicalBasePositive || p.physicalMaximumRaw>0) && (!p.magicalBasePositive || p.magicalMaximumRaw>0) &&
        p.components.size()<=5 && std::ranges::all_of(p.components,Positive);
}
bool ValidAbsorptionSashState(const AbsorptionSashProfile& p,const AbsorptionSashState& s) {
    return ValidAbsorptionSashProfile(p) && Payload(s) && s.percent>=p.initialMinimum && s.percent<=(p.grade==4 ? p.maximumPercent : p.initialMaximum);
}
AbsorptionSashState InitialAbsorptionSash(const AbsorptionSashProfile& p,std::span<const std::uint8_t,16> entropy,std::uint64_t instance) {
    AbsorptionSashState s;s.percent=p.initialMinimum;
    if(!ValidAbsorptionSashProfile(p) || p.initialMinimum==p.initialMaximum)return s;
    std::array<unsigned char,64> input{};constexpr std::string_view domain="auriga.item.sash.v1";
    std::copy(domain.begin(),domain.end(),input.begin());std::copy(entropy.begin(),entropy.end(),input.begin()+24);
    for(unsigned j=0;j<8;++j)input[40+j]=static_cast<unsigned char>(instance>>(8*j));
    const unsigned count=p.initialMaximum-p.initialMinimum+1,limit=256-256%count;
    std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};
    for(std::uint64_t counter=0;;++counter){for(unsigned j=0;j<8;++j)input[48+j]=static_cast<unsigned char>(counter>>(8*j));SHA256(input.data(),input.size(),digest.data());for(auto value:digest)if(value<limit){s.percent=static_cast<std::uint8_t>(p.initialMinimum+value%count);return s;}}
}
std::int64_t AbsorbSashComponent(BonusEntry b,std::uint8_t percent) {
    if(!Positive(b) || !percent || percent>25)return 0;
    const auto unit=Unit(b.bonus);return std::max<std::int64_t>(static_cast<std::int64_t>(unit),Rounded(b.valueScaled,percent,unit));
}
std::vector<BonusEntry> AbsorptionSashContributions(const SashDonorProfile& p,const AbsorptionSashState& s) {
    std::vector<BonusEntry> out;if(!ValidSashDonorProfile(p) || !Payload(s) || !s.donor || s.donor->key!=p.key || s.donor->refinement!=p.refinement)return out;
    const auto weapon=[&](BonusId id,std::int64_t raw,bool positive){const auto value=Rounded(raw,s.percent,1000)+(positive ? 1000 : 0);if(value)out.push_back({id,value});};
    if(p.key.Category()==ItemCategory::Weapon){weapon(BonusId::PhysicalAttackGradeBonus,p.physicalMaximumRaw,p.physicalBasePositive);weapon(BonusId::MagicalAttackGradeBonus,p.magicalMaximumRaw,p.magicalBasePositive);}
    for(const auto& b:p.components)if(const auto value=AbsorbSashComponent(b,s.percent))out.push_back({b.bonus,value});
    for(const auto& b:s.donor->attributes)if(b)if(const auto value=AbsorbSashComponent(*b,s.percent))out.push_back({b->bonus,value});
    return out;
}
std::optional<std::vector<std::uint8_t>> EncodeAbsorptionSashState(const AbsorptionSashState& s) {
    if(!Payload(s))return {};
    std::vector<std::uint8_t> b{'A','B','S',1};Put(b,s.percent,1);Put(b,s.donor.has_value(),1);
    Put(b,s.donor ? static_cast<unsigned>(s.donor->key.Category()) : 0,2);Put(b,s.donor ? s.donor->key.LocalId() : 0,4);Put(b,s.donor ? s.donor->refinement : 0,2);
    for(unsigned i=0;i<8;++i){const auto entry=s.donor ? s.donor->attributes[i] : std::optional<BonusEntry>{};Put(b,entry ? static_cast<unsigned>(entry->bonus) : 0,4);Put(b,entry ? static_cast<std::uint64_t>(entry->valueScaled) : 0,8);}return b;
}
std::optional<AbsorptionSashState> DecodeAbsorptionSashState(std::span<const std::uint8_t> b) {
    if(b.size()!=110 || b[0]!='A' || b[1]!='B' || b[2]!='S' || b[3]!=1 || b[5]>1)return {};
    AbsorptionSashState s;s.percent=b[4];const auto category=Get(b,6,2),local=Get(b,8,4),level=Get(b,12,2);
    if(b[5]){const auto key=ItemKey::Create(static_cast<std::uint16_t>(category),static_cast<std::uint32_t>(local));if(!key)return {};s.donor=AbsorbedSashDonor{*key,static_cast<std::uint16_t>(level)};}
    else if(category || local || level)return {};
    for(unsigned i=0;i<8;++i){const auto id=Get(b,14+i*12,4),value=Get(b,18+i*12,8);if(!id){if(value)return {};continue;}if(!s.donor || value>INT64_MAX)return {};s.donor->attributes[i]=BonusEntry{static_cast<BonusId>(id),static_cast<std::int64_t>(value)};}
    if(!Payload(s))return {};return s;
}
}
