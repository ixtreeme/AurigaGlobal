#include "../../SRC/Server/GameServer/ecs/systems/WoodcuttingRules.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace WoodcuttingRules;
    static_assert(IsAxe(210) && IsAxe(219));
    static_assert(!IsAxe(209) && !IsAxe(220) && !IsAxe(29101));
    static_assert(!CanRefine(210, 99) && CanRefine(210, 100));
    static_assert(!CanRefine(219, 999999));
    static_assert(AddPoint(210, 99) == 100 && AddPoint(210, 100) == 100);
    static_assert(AddPoint(219, 999999) == 0);
    static_assert(Due(100, 100) && !Due(99, 100));
    static_assert(Due(10, UINT32_MAX - 10) && !Due(UINT32_MAX - 10, 10));
    static_assert(StayedInPlace(100, 200, 1, 100, 200, 1));
    static_assert(!StayedInPlace(100, 200, 1, 101, 200, 1));
    static_assert(!StayedInPlace(100, 200, 1, 100, 201, 1));
    static_assert(!StayedInPlace(100, 200, 1, 100, 200, 2));
    for (uint32_t axe = 210; axe < 219; ++axe) {
        const auto maximum = MaxPoints(axe);
        assert(maximum && !CanRefine(axe, maximum - 1) && CanRefine(axe, maximum));
        assert(AddPoint(axe, maximum - 1) == maximum && AddPoint(axe, maximum) == maximum);
    }
    std::cout << "WOODCUT_RULES=PASS grades=10 boundaries=all clock=wrap-safe mining=unchanged\n";
}
