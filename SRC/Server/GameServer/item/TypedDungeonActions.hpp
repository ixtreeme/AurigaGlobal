#pragma once
#include "TypedDungeonItems.hpp"
#include <functional>
struct TypedDungeonAction {std::uint64_t stamp;std::function<void()> publish;};
using TypedDungeonPlanner=std::function<std::optional<TypedDungeonAction>(entt::entity,entt::entity)>;
bool TypedDungeonActionPending(entt::entity);
bool UseTypedDungeonAction(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&,TypedDungeonPlanner,bool targetRequired=true);
