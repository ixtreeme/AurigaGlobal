#pragma once
#include "TypedDungeonItems.hpp"
#include <functional>
#include <memory>
struct TypedDungeonAction {std::uint64_t stamp;std::function<void()> publish;};
using TypedDungeonPlanner=std::function<std::optional<TypedDungeonAction>(entt::entity,entt::entity)>;
struct TypedDungeonActionLease;
bool TypedDungeonActionPending(entt::entity);
std::shared_ptr<TypedDungeonActionLease> AcquireTypedDungeonAction(entt::entity dungeon);
bool UseTypedDungeonAction(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&,TypedDungeonPlanner,bool targetRequired=true);
