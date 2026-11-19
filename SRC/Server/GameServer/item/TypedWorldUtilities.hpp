#pragma once
#include <entt/entity/fwd.hpp>
#include <item/PersistentItems.hpp>
bool TryUseTypedWorldUtility(entt::entity,const auriga::item::PersistentItem&,const auriga::item::ItemCatalog&);
// Returns true while a durable recall is being completed or acknowledged.
bool ResumeTypedWorldWarp(entt::entity);
