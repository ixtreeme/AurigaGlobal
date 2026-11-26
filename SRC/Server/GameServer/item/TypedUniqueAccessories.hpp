#pragma once

#include "ItemStore.hpp"
#include "../ecs/Registry.hpp"
#include <algorithm>
#include <chrono>
#include <initializer_list>

namespace auriga::item::runtime
{
enum class UniqueAccessory : std::uint32_t
{
    LeadershipBook = 702, DiligenceMedal = 703, ExperienceRing = 704, LanguageRing = 705,
    GoddessTear = 706, ThiefGloves = 708, SampleLanguageRing = 709,
    EmotionMask = 725, LeadershipBookSample = 726,
    EmotionMaskShop = 762, EmotionMaskBound = 763, EmotionMaskTimed = 764,
    LeadershipBookShop = 765, LeadershipBookBound = 766, LeadershipBookGift = 775
};

inline bool HasEquippedAccessory(entt::entity character, std::initializer_list<UniqueAccessory> choices)
{
    const auto* owned = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
    if (!owned) return false;

    return std::ranges::any_of(owned->state.items, [&](const PersistentItem& item) {
        if (item.destroyed || item.owner != owned->state.fence.owner || !item.placement ||
            item.placement->kind != ItemStorageKind::Equipment ||
            !FitsEquipmentSlot(EquipmentSlot::Unique, item.placement->anchor) ||
            item.key.Category() != ItemCategory::Accessory ||
            std::ranges::find(choices, static_cast<UniqueAccessory>(item.key.LocalId())) == choices.end())
            return false;
        if (!item.costume) return true;
        if (item.costume->expired) return false;
        if (!item.costume->deadlineUtcMs) return true;
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return item.costume->deadlineUtcMs > static_cast<std::uint64_t>(now);
    });
}

inline bool HasTypedLanguageRing(entt::entity character)
{
    return HasEquippedAccessory(character, {UniqueAccessory::LanguageRing, UniqueAccessory::SampleLanguageRing});
}
}

namespace auriga::item::runtime {
inline bool HasTypedEmotionMask(entt::entity e)
{
    return HasEquippedAccessory(e, {UniqueAccessory::EmotionMask, UniqueAccessory::EmotionMaskShop,
        UniqueAccessory::EmotionMaskBound, UniqueAccessory::EmotionMaskTimed});
}
inline bool HasTypedLeadershipBook(entt::entity e)
{
    return HasEquippedAccessory(e, {UniqueAccessory::LeadershipBook, UniqueAccessory::LeadershipBookSample,
        UniqueAccessory::LeadershipBookShop, UniqueAccessory::LeadershipBookBound, UniqueAccessory::LeadershipBookGift});
}
}
