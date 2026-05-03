#pragma once

#include <entt/entt.hpp>

#ifdef AURIGA_LPENTITY_FIXUP_AUDIT
#include "../../packet.h"
#endif

namespace ecs::EntityNetworkDispatch {

void SendInsert(entt::registry& reg, entt::entity source, entt::entity viewer);
void SendRemove(entt::registry& reg, entt::entity source, entt::entity viewer);
void Reencode(entt::registry& reg, entt::entity viewer);

#ifdef AURIGA_LPENTITY_FIXUP_AUDIT
// LPENTITY.4-fixup.4: non-sending build of the native character insert
// packet, used by EntityNetworkDispatchAudit::CheckCharacterInsertParity.
// Returns true if the packet was filled, false if the entity lacks the
// minimum components (VID, Position).
bool BuildCharacterInsertForAudit(entt::registry& reg, entt::entity source, TPacketGCCharacterAdd& packet);
#endif

} // namespace ecs::EntityNetworkDispatch
