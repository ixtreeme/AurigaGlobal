#include "../../SRC/Server/GameServer/ecs/systems/HerbalismRules.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/WoodcuttingRules.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace HerbalismRules;
    static_assert(IsShears(230) && IsShears(239) && !IsShears(229) && !IsShears(240));
    static_assert(!IsShears(210) && !WoodcuttingRules::IsAxe(230));
    static_assert(TrainingMarker != WoodcuttingRules::TrainingMarker);
    static_assert(HerbFor(34800)==50701 && HerbFor(34811)==50712);
    static_assert(HerbFor(34799)==0 && HerbFor(34812)==0 && HerbFor(34090)==0);
    static_assert(!CanRefine(239,UINT32_MAX) && AddPoint(239,1)==0);
    static_assert(Due(10,UINT32_MAX-10) && !Due(UINT32_MAX-10,10));
    for (uint32_t tool=230; tool<239; ++tool) {
        const auto max = MaxPoints(tool);
        assert(max && !CanRefine(tool,max-1) && CanRefine(tool,max));
        assert(AddPoint(tool,max-1)==max && AddPoint(tool,UINT32_MAX)==max);
    }
    std::cout << "HERBALISM_RULES=PASS species=12 grades=10 isolation=woodcutting/mining\n";
}
