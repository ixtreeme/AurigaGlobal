#pragma once

#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include <algorithm>
#include <chrono>

inline bool HasTypedLanguageRing(entt::entity character)
{
    namespace item = auriga::item;
    const auto* owned = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
    if (!owned) return false;

    // The typed identities of the normal ring and its sample.
    static const auto ring = item::ItemKey::Create(3, 705).value();
    static const auto sample = item::ItemKey::Create(3, 709).value();
    return std::ranges::any_of(owned->state.items, [&](const item::PersistentItem& equipped) {
        if (equipped.destroyed || !equipped.placement ||
            equipped.placement->kind != item::ItemStorageKind::Equipment ||
            (equipped.key != ring && equipped.key != sample))
            return false;
        if (!equipped.costume) return true;
        if (equipped.costume->expired) return false;
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return equipped.costume->deadlineUtcMs == 0 || equipped.costume->deadlineUtcMs > static_cast<std::uint64_t>(now);
    });
}
