#pragma once

#include <entt/entt.hpp>
#include <common/length.h>

bool IsInputInventoryPosition(TItemPos position);
bool IsInputItemAt(entt::entity owner, entt::entity item, TItemPos position);
bool IsDetachedInputItem(entt::entity item);
bool RestoreInputItem(entt::entity owner, entt::entity item, TItemPos position);
