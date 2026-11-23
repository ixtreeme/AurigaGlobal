#include "../../SRC/Server/GameServer/ecs/systems/HerbalismRules.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace HerbalismRules;
    static_assert(IsPlantRace(34800) && IsPlantRace(34811));
    static_assert(!IsPlantRace(34799) && !IsPlantRace(34812) && !IsPlantRace(34090));
    static_assert(Due(10, UINT32_MAX - 10) && !Due(UINT32_MAX - 10, 10));
    std::cout << "HERBALISM_RULES=PASS species=12 clock=wrap-safe\n";
}
