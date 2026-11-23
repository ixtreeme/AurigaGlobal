#include "../../SRC/Server/GameServer/ecs/systems/WoodcuttingRules.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace WoodcuttingRules;
    static_assert(Due(100, 100) && !Due(99, 100));
    static_assert(Due(10, UINT32_MAX - 10) && !Due(UINT32_MAX - 10, 10));
    static_assert(StayedInPlace(100, 200, 1, 100, 200, 1));
    static_assert(!StayedInPlace(100, 200, 1, 101, 200, 1));
    static_assert(!StayedInPlace(100, 200, 1, 100, 201, 1));
    static_assert(!StayedInPlace(100, 200, 1, 100, 200, 2));
    std::cout << "WOODCUT_RULES=PASS clock=wrap-safe\n";
}
