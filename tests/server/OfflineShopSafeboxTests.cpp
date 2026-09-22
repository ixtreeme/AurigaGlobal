#include "../../SRC/Server/GameServer/core/stdafx.h"
#include "../../SRC/Server/GameServer/ecs/Registry.hpp"
#include "../../SRC/Server/GameServer/ecs/components/social_components.hpp"
#include "../../SRC/Server/GameServer/ecs/systems/OfflineShopSystem.hpp"
#include "../../SRC/Server/GameServer/social/new_offlineshop.h"

#include <iostream>
#include <stdexcept>

entt::registry g_registry;

static int fixturePulse = 0;
int thecore_pulse() { return fixturePulse; }

// The safebox type is exercised through its owner relations only; the item and
// value containers are empty in this fixture, so the stubs stay trivial.
namespace offlineshop {
CShopItem::CShopItem(uint32_t) {}
CShopItem::CShopItem(const CShopItem&) {}
CShopItem::~CShopItem() {}

CShopSafebox::CShopSafebox(entt::entity owner) : m_pkOwner(owner) {}
CShopSafebox::CShopSafebox() : m_pkOwner(entt::null) {}
CShopSafebox::CShopSafebox(const CShopSafebox& rCopy)
    : m_vecItems(rCopy.m_vecItems), m_pkOwner(rCopy.m_pkOwner), m_valutes(rCopy.m_valutes) {}
CShopSafebox::~CShopSafebox() {}
void CShopSafebox::SetOwner(entt::entity owner) { m_pkOwner = owner; }
entt::entity CShopSafebox::GetOwner() { return m_pkOwner; }
}

namespace {

int checks = 0;
void Check(bool condition, const char* why) { ++checks; if (!condition) throw std::runtime_error(why); }

// F12: the back pointer must move on every handover, not only on the null
// transitions, and F2: clearing the component must reach the live safebox
// before a session cache erase could destroy it.
void HandoverAndClear() {
    g_registry.clear();
    const auto e = g_registry.create();
    const auto other = g_registry.create();
    offlineshop::CShopSafebox first, second;

    ecs::OfflineShopSystem::SetShopSafebox(e, &first);
    Check(ecs::OfflineShopSystem::GetShopSafebox(e) == &first && first.GetOwner() == e,
        "first safebox was not attached to its owner");

    ecs::OfflineShopSystem::SetShopSafebox(e, &second);
    Check(first.GetOwner() == entt::null, "old safebox kept a stale owner back pointer");
    Check(second.GetOwner() == e && ecs::OfflineShopSystem::GetShopSafebox(e) == &second,
        "new safebox did not take over the owner relation");

    ecs::OfflineShopSystem::SetShopSafebox(e, &second);
    Check(second.GetOwner() == e, "idempotent handover dropped the owner");

    ecs::OfflineShopSystem::SetShopSafebox(e, nullptr);
    Check(second.GetOwner() == entt::null && ecs::OfflineShopSystem::GetShopSafebox(e) == nullptr,
        "clearing the component left the safebox owner set");

    ecs::OfflineShopSystem::SetShopSafebox(e, &first);
    ecs::OfflineShopSystem::SetShopSafebox(other, &first);
    Check(first.GetOwner() == other, "safebox reassignment did not move the owner");

    ecs::OfflineShopSystem::SetShopSafebox(entt::null, &second);
    Check(second.GetOwner() == entt::null, "null entity accepted a safebox");
    g_registry.destroy(other);
    Check(!g_registry.valid(other), "fixture failed to destroy the owner");
    g_registry.clear();
}

} // namespace

int main() {
    try {
        HandoverAndClear();
        std::cout << "Offline shop safebox checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
