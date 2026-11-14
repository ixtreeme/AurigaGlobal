#include "../../SRC/Server/GameServer/pet/CollectionBonusRules.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using CollectionBonuses::Totals;
void Require(bool ok) { if(!ok) throw std::runtime_error("collection bonus regression"); }
Totals Make(uint8_t window,unsigned count) { Totals total(window);for(unsigned i=1;i<=count;++i)total.AddItem(i);return total; }
int main() {
    for(unsigned count:{0u,9u,10u,19u,20u,192u}) {
        auto total=Make(5,count);Require(total.Tiers()==count/10);
        Require(total.bonuses[1].Total(total.Tiers())==500*(count/10));
        for(uint8_t type:{116,117,119,63})Require(total.bonuses[type].Total(total.Tiers())==2*(count/10));
    }
    for(unsigned count:{0u,4u,5u,9u,10u,14u,15u,79u,192u}) {
        auto total=Make(14,count);Require(total.Tiers()==count/5);
        for(uint8_t type:{103,74,73})Require(std::abs(total.bonuses[type].Total(total.Tiers())-.2*(count/5))<1e-10);
    }
    auto ten=Make(5,10);ten.AddApply(1,1.125);Require(ten.bonuses[1].Total(ten.Tiers())==501.125);
    Require(!ten.AddItem(1)&&ten.Count()==10); // repeated seal/stack never increases the threshold
    auto nine=Make(5,9);nine.AddApply(1,1.125);Require(nine.bonuses[1].Total(nine.Tiers())==1.125);
    auto pets=Make(14,5);pets.AddApply(103,2.375);Require(std::abs(pets.bonuses[103].Total(pets.Tiers())-2.575)<1e-10);
    auto four=Make(14,4);four.AddApply(103,2.375);Require(four.bonuses[103].Total(four.Tiers())==2.375);
    four.AddApply(103,NAN);Require(four.bonuses[103].items==2.375);
    std::cout<<"COLLECTION_BONUS_RULES=SUCCESS thresholds fractions removal duplicate_seals\n";
}
