#include "stdafx.h"
#include "ecs/systems/ViewSystem.hpp"
#include "ecs/systems/VisibilitySystem.hpp"
#include "ecs/services/VisibilityService.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "char_interface.hpp"
#include "config.h"
#include "desc.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "utils.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#include "ecs/SpatialHelpers.hpp"
#include "ecs/components/dirty_components.hpp"
#include "ecs/components/spatial_components.hpp"
#include "ecs/components/status_components.hpp"
#include "ecs/components/transform_components.hpp"
#include "ecs/components/visibility_components.hpp"
#include "ecs/services/SpatialService.hpp"

#include <unordered_set>

CEntity::CEntity()
{
	Initialize();
}

CEntity::~CEntity()
{
	if (!m_bIsDestroyed)
		assert(!"You must call CEntity::destroy() method in your derived class destructor");
}

void CEntity::Initialize(int type)
{
	m_bIsDestroyed = false;

	m_iType = type;
	m_pos.x = m_pos.y = m_pos.z = 0;

	// Phase 15E-final.LPENTITY.4-architect.H.3:
	// m_pSectree was deleted - the ECS SectorPlacement component owned
	// by SECTREE::InsertEntity / RemoveEntity (H.2) is the sole source.
	m_lpDesc = nullptr;
	m_lMapIndex = 0;
	m_bIsObserver = false;
	m_bObserverModeChange = false;
}

void CEntity::Destroy()
{
	if (m_bIsDestroyed) {
		return;
	}
	m_bIsDestroyed = true;
	ecs::ViewSystem::ViewCleanup(ecs::SpatialService::EntityFromLPENTITY(this));
}

namespace {
inline const ecs::Position* TryGetPositionFor(const CEntity* self)
{
	// Phase 15E-final.LPENTITY.4-architect.B.1.1:
	// Resolve `self` to its ECS entity via SpatialService (the legitimate
	// LPENTITY -> entt::entity boundary), then read the Position component.
	// Returns nullptr when the entity has not yet been registered with ECS
	// (bootstrap window between CEntity ctor and the subclass factory).
	// In that window the legacy m_pos is also at its zero-init state,
	// so callers that hit the nullptr branch see (0, 0, 0) - matching
	// legacy behaviour bit-exactly.
	if (!self)
		return nullptr;
	const entt::entity e = ecs::SpatialService::EntityFromLPENTITY(
		const_cast<LPENTITY>(static_cast<const CEntity*>(self)));
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	return g_registry.try_get<ecs::Position>(e);
}
}

int32_t CEntity::GetX() const
{
	if (const auto* pos = TryGetPositionFor(this))
		return pos->x;
	return 0;
}

int32_t CEntity::GetY() const
{
	if (const auto* pos = TryGetPositionFor(this))
		return pos->y;
	return 0;
}

int32_t CEntity::GetZ() const
{
	if (const auto* pos = TryGetPositionFor(this))
		return pos->z;
	return 0;
}

PIXEL_POSITION CEntity::GetXYZ() const
{
	PIXEL_POSITION result;
	if (const auto* pos = TryGetPositionFor(this))
	{
		result.x = pos->x;
		result.y = pos->y;
		result.z = pos->z;
	}
	else
	{
		result.x = 0;
		result.y = 0;
		result.z = 0;
	}
	return result;
}

LPSECTREE CEntity::GetSectree() const
{
	// Phase 15E-final.LPENTITY.4-architect.H.3:
	// Pure ECS resolution. m_pSectree has been deleted. SectorPlacement
	// is maintained by SECTREE::InsertEntity/RemoveEntity (H.2) plus the
	// SpatialService / MovementSystem write paths. Returns nullptr in
	// the bootstrap window (CEntity ctor before EntityFactory) and after
	// despawn - both observable behaviours match the pre-H.3 m_pSectree
	// field, which was nullptr in the same situations.
	const entt::entity e = ecs::SpatialService::EntityFromLPENTITY(
		const_cast<LPENTITY>(static_cast<const CEntity*>(this)));
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;

	return ecs::SectorOf(g_registry, e);
}

void CEntity::SetType(int type)
{
	m_iType = type;
}

int CEntity::GetType() const
{
	return m_iType;
}

bool CEntity::IsType(int type) const
{
	return (m_iType == type ? true : false);
}



namespace ecs::ViewSystem {

void PacketView(entt::entity self, const void* data, int bytes, entt::entity except)
{
    if (!g_registry.valid(self) || !ecs::SectorOf(g_registry, self)) return;
    const auto* revision = g_registry.try_get<ecs::SpatialRevision>(self);
    const uint64_t version = revision ? revision->value : 0;
    const auto recipients = ecs::VisibilityService::GetViewersOf(g_registry, self);
    for (auto target : recipients) {
        if (!g_registry.valid(self)) return;
        const auto* current = g_registry.try_get<ecs::SpatialRevision>(self);
        if ((current ? current->value : 0) != version) return;
        if (target == except || !g_registry.valid(target)) continue;
        if (auto* desc = ecs::PlayerRuntime::GetDesc(target)) desc->Packet(data, bytes);
    }
}

} // namespace ecs::ViewSystem


void CEntity::SetObserverMode(bool bFlag)
{
    if (m_bIsObserver == bFlag) return;
    const auto entity = ecs::SpatialService::EntityFromLPENTITY(this);
    const bool character = IsType(ENTITY_CHARACTER);
    m_bIsObserver = bFlag;
    m_bObserverModeChange = false;
    // Commit the ECS observer state before publishing visibility changes.
    // Do not dereference this after any component/network callback.
    if (!g_registry.valid(entity)) return;
    if (character) {
        if (auto* status = g_registry.try_get<ecs::StatusFlags>(entity))
            status->isObserverMode = bFlag;
        if (bFlag) {
            if (!g_registry.all_of<ecs::ObserverModeTag>(entity))
                g_registry.insert<ecs::ObserverModeTag>(&entity, &entity + 1);
        }
        else g_registry.remove<ecs::ObserverModeTag>(entity);
        if (!g_registry.valid(entity)) return;
        if (!g_registry.all_of<ecs::DirtyTag>(entity))
            g_registry.insert<ecs::DirtyTag>(&entity, &entity + 1);
    }
    ecs::VisibilitySystem::Refresh(g_registry, entity);
    if (character && g_registry.valid(entity))
        ecs::ChatSystem::Send(entity, CHAT_TYPE_COMMAND, "ObserverMode %d", bFlag ? 1 : 0);
}

void CEntity::UpdateSectree()
{
    const auto entity = ecs::SpatialService::EntityFromLPENTITY(this);
    m_bObserverModeChange = false;
    ecs::VisibilitySystem::Refresh(g_registry, entity);
}
