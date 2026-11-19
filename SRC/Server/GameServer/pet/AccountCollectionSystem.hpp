#pragma once

#include "../item/ItemStore.hpp"
#include <item/StorageRuntime.hpp>
#include <entt/entity/entity.hpp>

namespace AccountCollectionSystem
{
void Request(entt::entity owner);
void Apply(entt::entity owner, const auriga::item::persistence::LoadedItemOwner& account);
void Notify(std::uint32_t account);
void Invalidate(std::uint32_t account);
std::optional<std::uint16_t> MountCount(entt::entity owner);
std::uint64_t Revision(entt::entity owner);
std::span<const auriga::item::BonusEntry> Bonuses(entt::entity owner);
}
