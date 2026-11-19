#include <item/AbsorptionSashWire.hpp>
namespace auriga::item {
std::vector<std::uint8_t> EncodeAbsorptionSashReply(const ItemCatalog& c,std::uint8_t status,std::uint64_t nonce,
    std::uint64_t source,std::uint64_t target,std::uint8_t selection,const AbsorptionSashQuote* q,std::uint8_t outcome,std::uint64_t resultInstance){
    std::vector<std::uint8_t> b;const auto put=[&](std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    put(220,1);put(0,4);put(2,1);put(status,1);put(nonce,8);put(source,8);put(target,8);put(3,1);put(selection,1);put(0,2);put(q!=nullptr,1);
    if(q){
        put(q->grade,1);put(q->currentPercent,1);put(q->minimumPercent,1);put(q->maximumPercent,1);put(q->successBp,2);put(q->goldCost,8);
        put(q->resultKey ? static_cast<unsigned>(q->resultKey->Category()) : 0,2);put(q->resultKey ? q->resultKey->LocalId() : 0,4);put(outcome,1);put(resultInstance,8);
        for(const auto* preview:{&q->previewMinimum,&q->previewMaximum}){const auto* d=preview->donor ? c.FindSashDonor(preview->donor->key,preview->donor->refinement) : nullptr;const auto bonuses=d ? AbsorptionSashContributions(*d,*preview) : std::vector<BonusEntry>{};put(bonuses.size(),1);for(auto bonus:bonuses){put(static_cast<unsigned>(bonus.bonus),4);put(bonus.valueScaled,8);}}
    }
    for(unsigned i=0;i<4;++i)b[1+i]=static_cast<std::uint8_t>(b.size()>>(8*i));return b;
}
}
