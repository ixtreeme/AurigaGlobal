#include "../../SRC/Server/GameServer/item/LegacyItemBoundary.hpp"
#include "../../SRC/Server/GameServer/item/ItemBackendState.hpp"
#include "../../SRC/Server/common/item/LegacyItemProtocol.hpp"
#include <iostream>
#include <stdexcept>

entt::registry g_registry;
static unsigned checks;
static void Check(bool value, const char* why)
{
    ++checks;
    if (!value) throw std::runtime_error(why);
}
int main()
{
    try
    {
        using Mode = ecs::TypedItemLogin::Mode;
        const auto owner = g_registry.create();
        Check(BlocksLegacyItems(owner), "missing native state must never enable retired items");
        auto& login = g_registry.emplace<ecs::TypedItemLogin>(owner);
        Check(BlocksLegacyItems(owner), "early legacy mutation cannot race pending owner load");
        login.mode = Mode::Native;
        Check(BlocksLegacyItems(owner), "native owner never enables retired items");
        login.mode = Mode::Unavailable;
        Check(BlocksLegacyItems(owner), "failed SQL load must never enable retired items");
        g_registry.destroy(owner);
        Check(BlocksLegacyItems(owner), "logged-out entity must never enable retired items");
        for (const auto h : {11,12,13,20,21,22,23,27,50,55,57,60,69,70,71,77,83,96,125,126,127,146,171,211,215})
            Check(auriga::item::IsLegacyItemMutation(h), "retired inventory, shop, trade or storage packet blocked");
        for (const auto h : {3,7,15,16,17,18,19,42,49,66,67,80,201,202,212,213,214,221,225,226,227,228,229,230,231,232,233,234,235,236,242,243,244,247,249,250,254})
            Check(!auriga::item::IsLegacyItemMutation(h), "typed operations, ground pickup and unrelated gameplay remain available");
        std::cout << "Legacy item boundary: " << checks << " checks passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
