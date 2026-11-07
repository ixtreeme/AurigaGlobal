#include <item/SpecialBonuses.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <openssl/sha.h>

namespace auriga::item {
namespace {
bool Bonus(const BonusEntry& b){const auto* d=FindBonusDefinition(b.bonus);return d && AllowsBonusSource(*d,BonusSource::Instance) && b.valueScaled && b.valueScaled>=d->minimum && b.valueScaled<=d->maximum;}
void Put(std::vector<std::uint8_t>& b,std::uint64_t value,unsigned n){for(unsigned j=0;j<n;++j)b.push_back(static_cast<std::uint8_t>(value>>(8*j)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned at,unsigned n){std::uint64_t v=0;for(unsigned j=0;j<n;++j)v|=std::uint64_t{b[at+j]}<<(8*j);return v;}
void Entry(std::vector<std::uint8_t>& b,const std::optional<BonusEntry>& e){Put(b,e ? static_cast<unsigned>(e->bonus) : 0,4);Put(b,e ? static_cast<std::uint64_t>(e->valueScaled) : 0,8);}
bool Entry(std::span<const std::uint8_t> b,unsigned at,std::optional<BonusEntry>& e){const auto id=Get(b,at,4),v=Get(b,at+4,8);if(!id)return !v;e=BonusEntry{static_cast<BonusId>(id),static_cast<std::int64_t>(v)};return Bonus(*e);}
}
std::uint32_t RegaliaSlot(const ItemRegaliaRow& r){return r.kind==RegaliaKind::Stole ? 12 : 12+r.slot;}
bool ValidRegalia(const ItemRegaliaRow& r) {
    if(r.key.Category()!=ItemCategory::Regalia || r.rules.empty() || r.rules.size()>48)return false;
    if(r.kind==RegaliaKind::Stole ? (r.slot || r.grade<1 || r.grade>4 || r.capacityMs || r.initialMs) :
        (r.kind!=RegaliaKind::Rune || r.slot<1 || r.slot>7 || r.grade || (r.slot==7 ? r.capacityMs || r.initialMs : r.capacityMs<100 || r.capacityMs>INT64_MAX || r.initialMs>r.capacityMs)))return false;
    std::map<unsigned,std::set<unsigned>> indices;std::map<unsigned,BonusId> types;
    for(const auto& v:r.rules) {
        if(!Bonus(v.bonus) || (r.kind==RegaliaKind::Stole ? v.index>=6 || v.variant>=4 || v.minimumChargeBp : v.index>=2 || v.variant || v.minimumChargeBp>10000))return false;
        if(types.contains(v.index) && types[v.index]!=v.bonus.bonus)return false;types[v.index]=v.bonus.bonus;
        if(!indices[v.index].insert(r.kind==RegaliaKind::Stole ? v.variant : v.minimumChargeBp).second)return false;
    }
    if(types.size()!=(r.kind==RegaliaKind::Stole ? 6 : 2))return false;
    std::set<BonusId> distinct;for(auto [index,type]:types)if(!distinct.insert(type).second)return false;
    for(auto& [index,keys]:indices)if(r.kind==RegaliaKind::Stole ? keys!=std::set<unsigned>{0,1,2,3} : !keys.contains(0))return false;
    return true;
}
bool ValidBonusTool(const ItemBonusToolRow& r) {
    if(r.key.Category()!=ItemCategory::BonusTool || r.operation<SpecialBonusOperation::StoleReroll || r.operation>SpecialBonusOperation::PairedAddon || !r.units)return false;
    if((r.operation==SpecialBonusOperation::CostumeGrant)!=r.payload.has_value() || (r.payload && !Bonus(*r.payload)))return false;
    if(r.operation==SpecialBonusOperation::RuneCharge ? (!r.parameter || r.parameter>10000 || r.units!=1) : r.parameter)return false;
    if(r.operation!=SpecialBonusOperation::CostumeReset)return r.tiers.empty();
    std::map<unsigned,unsigned> sums;std::set<std::pair<unsigned,unsigned>> keys;
    for(const auto& t:r.tiers)if(t.draw>=3 || !t.tier || t.tier>16 || !t.chanceBp || t.chanceBp>10000 || !keys.emplace(t.draw,t.tier).second || (sums[t.draw]+=t.chanceBp)>10000)return false;
    return sums==std::map<unsigned,unsigned>{{0,10000},{1,10000},{2,10000}};
}
bool ValidRegaliaState(const ItemRegaliaRow& r,const RegaliaState& s) {
    if(!ValidRegalia(r))return false;
    if(r.kind==RegaliaKind::Rune)return std::ranges::none_of(s.bonuses,[](const auto& b){return b.has_value();}) &&
        (r.slot==7 ? !s.capacityMs && !s.remainingMs && !s.active : s.capacityMs>=100 && s.capacityMs<=INT64_MAX && s.remainingMs<=s.capacityMs && (!s.active || s.remainingMs));
    if(s.capacityMs || s.remainingMs || s.active)return false;
    for(unsigned j=0;j<6;++j)if(!s.bonuses[j] || !Bonus(*s.bonuses[j]) || s.bonuses[j]->bonus!=std::ranges::find(r.rules,j,&RegaliaRule::index)->bonus.bonus)return false;
    return true; // Values already issued survive balance edits.
}
bool ValidBonusToolState(const ItemBonusToolRow& r,const BonusToolState& s) {
    return ValidBonusTool(r) && (r.operation==SpecialBonusOperation::CostumeGrant ? s.payload && Bonus(*s.payload) && s.payload->bonus==r.payload->bonus && !s.remainingBp :
        r.operation==SpecialBonusOperation::RuneCharge ? !s.payload && s.remainingBp>0 && s.remainingBp<=10000 : !s.payload && !s.remainingBp);
}
RegaliaState InitialRegalia(const ItemRegaliaRow& r,std::span<const std::uint8_t,16> entropy,std::uint64_t id) {
    RegaliaState s;if(r.kind==RegaliaKind::Rune){s.capacityMs=r.capacityMs;s.remainingMs=r.initialMs;return s;}
    std::array<unsigned char,24> bytes{};std::copy(entropy.begin(),entropy.end(),bytes.begin());for(unsigned j=0;j<8;++j)bytes[16+j]=static_cast<unsigned char>(id>>(8*j));
    std::array<unsigned char,SHA256_DIGEST_LENGTH> digest{};SHA256(bytes.data(),bytes.size(),digest.data());
    for(unsigned j=0;j<6;++j){const auto v=digest[j]&3;s.bonuses[j]=std::ranges::find_if(r.rules,[&](const auto& rule){return rule.index==j && rule.variant==v;})->bonus;}return s;
}
BonusToolState InitialBonusTool(const ItemBonusToolRow& r){return {r.payload,r.operation==SpecialBonusOperation::RuneCharge ? r.parameter : std::uint16_t{0}};}
RegaliaState AdvanceRegalia(const ItemRegaliaRow& r,RegaliaState s,std::uint64_t elapsed) {
    if(r.kind==RegaliaKind::Rune && r.slot<7){s.remainingMs-=std::min(s.remainingMs,elapsed);if(!s.remainingMs)s.active=false;}return s;
}
std::vector<BonusEntry> RegaliaContributions(const ItemRegaliaRow& r,const RegaliaState& s) {
    std::vector<BonusEntry> out;if(r.kind==RegaliaKind::Stole){for(const auto& b:s.bonuses)if(b)out.push_back(*b);return out;}
    if(r.slot<7 && (!s.active || !s.remainingMs))return out;
    // Compare without multiplying a potentially INT64_MAX-sized clock.
    for(unsigned j=0;j<2;++j){const RegaliaRule* best=nullptr;for(const auto& v:r.rules)if(v.index==j) {
        const auto required=(s.capacityMs/10000)*v.minimumChargeBp+((s.capacityMs%10000)*v.minimumChargeBp+9999)/10000;
        if((r.slot==7 || s.remainingMs>=required) && (!best || v.minimumChargeBp>best->minimumChargeBp))best=&v;
    }if(best)out.push_back(best->bonus);}return out;
}
std::vector<std::uint8_t> EncodeRegaliaState(const RegaliaState& s){std::vector<std::uint8_t> b{'R','G','L',1};Put(b,s.active,1);Put(b,s.capacityMs,8);Put(b,s.remainingMs,8);for(const auto& e:s.bonuses)Entry(b,e);return b;}
std::optional<RegaliaState> DecodeRegaliaState(std::span<const std::uint8_t> b){if(b.size()!=93 || b[0]!='R' || b[1]!='G' || b[2]!='L' || b[3]!=1 || b[4]>1)return {};RegaliaState s;s.active=b[4];s.capacityMs=Get(b,5,8);s.remainingMs=Get(b,13,8);if(s.capacityMs>INT64_MAX || s.remainingMs>s.capacityMs)return {};for(unsigned j=0;j<6;++j)if(!Entry(b,21+j*12,s.bonuses[j]))return {};return s;}
std::vector<std::uint8_t> EncodeBonusToolState(const BonusToolState& s){std::vector<std::uint8_t> b{'B','T','L',1};Put(b,s.remainingBp,2);Entry(b,s.payload);return b;}
std::optional<BonusToolState> DecodeBonusToolState(std::span<const std::uint8_t> b){if(b.size()!=18 || b[0]!='B' || b[1]!='T' || b[2]!='L' || b[3]!=1)return {};BonusToolState s;s.remainingBp=static_cast<std::uint16_t>(Get(b,4,2));if(s.remainingBp>10000 || !Entry(b,6,s.payload))return {};return s;}
}
