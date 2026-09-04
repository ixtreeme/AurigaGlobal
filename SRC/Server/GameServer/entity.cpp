#include "stdafx.h"
#include "ecs/systems/ViewSystem.hpp"
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
	ecs::ViewSystem::ViewCleanup(ecs::SpatialService::EntityFromLPENTITY(this));
	m_bIsDestroyed = true;
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
	if (self == entt::null || !g_registry.valid(self))
		return;

	if (!ecs::SpatialService::GetSectree(g_registry, self))
		return;

	// Phase 15E-final.LPENTITY.4-architect.D.8:
	//
	// The hybrid f76a3f1 broadcast (m_map_view loop + sectree neighbour
	// walk + dedup) was a band-aid for the original two-client desync
	// root cause: idle characters never re-poll, so m_map_view goes stale
	// during their stillness window. After Phase D.4-D.6 the ECS ViewerMap
	// is event-driven and current at every tick - so the band-aid retires
	// here. PacketView body is now a straight walk of ViewerMap.viewers
	// plus self, exactly matching the architect doc Phase D.8 spec.
	//
	// For non-character source entities, ViewerMap may be incomplete
	// (the legacy CFuncViewInsert path that fed it from chars' polling
	// was disabled in D.6). PacketView is called only from char paths
	// (FuncPacketView via PacketAround, MovementSystem broadcasts), so
	// the practical recipient set is unaffected. If a future caller
	// invokes PacketView on a non-char source and finds the ViewerMap
	// empty, the fallback below catches it via a one-time sectree walk.
	std::unordered_set<entt::entity> sent;

	const auto send = [&](entt::entity target) {
		if (target == except)
			return;

		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(target))
			desc->Packet(data, bytes);
	};

	const auto isCharacter = [](entt::entity e) {
		const auto* kind = g_registry.try_get<ecs::SpatialKindTag>(e);
		return kind && kind->kind == ecs::SpatialKind::Character;
	};

	if (!ecs::PlayerRuntime::IsObserverMode(self))
	{
		bool walkedViewerMap = false;

		if (isCharacter(self))
		{
		// Phase 15E-final.LPENTITY.4-architect H fixup-2:
		// Self-heal ViewerMap before the broadcast walk. The D.4 event
		// handler is supposed to keep self.ViewerMap.viewers in sync with
		// the sectree truth, but live WinTest after Phase H.1-H.3 + H
		// fixup-1 still showed peers losing visibility under fast-mount
		// movement ("ghost" symptom: char rendered on the moving client
		// keeps moving, peer client shows it frozen). The diff handler
		// must be missing some path - rather than chasing the exact gap,
		// guarantee correctness here at the broadcast site.
		//
		// Walk the sectree neighbour grid at self's current position and
		// add any character in range that is missing from the ViewerMap.
		// This is "additive only" - no entity is removed - so it cannot
		// race with a legitimate D.4 leaving transition. The cost is one
		// sectree query per PacketView call on character sources (~9
		// neighbour cells, range filter); cheap relative to packet
		// construction and network send.
			const int32_t range = VIEW_RANGE + VIEW_BONUS_RANGE;
			ecs::SpatialService::ForEachAround(g_registry, self, range,
				[&](entt::entity other) {
					if (other == self || !isCharacter(other))
						return;

					g_registry.get_or_emplace<ecs::ViewerMap>(self).viewers.insert(other);
					if (auto* otherView = g_registry.try_get<ecs::ViewMap>(other))
						otherView->visible.insert(self);
				});
		}

		if (auto* viewerMap = g_registry.try_get<ecs::ViewerMap>(self))
		{
			walkedViewerMap = true;
			for (const entt::entity viewer : viewerMap->viewers)
			{
				if (viewer == entt::null || !g_registry.valid(viewer))
					continue;

				if (sent.insert(viewer).second)
					send(viewer);
			}
		}

		if (!walkedViewerMap)
		{
			const int32_t range = VIEW_RANGE + VIEW_BONUS_RANGE;
			ecs::SpatialService::ForEachAround(g_registry, self, range,
				[&](entt::entity other) {
					if (other == self || other == except || !isCharacter(other))
						return;

					if (!ecs::PlayerRuntime::GetDesc(other))
						return;

					if (sent.insert(other).second)
						send(other);
				});
		}
	}

	if (sent.insert(self).second)
		send(self);
}

} // namespace ecs::ViewSystem


void CEntity::SetObserverMode(bool bFlag)
{
	if (m_bIsObserver == bFlag)
		return;

	m_bIsObserver = bFlag;
	m_bObserverModeChange = true;
	UpdateSectree();

	if (IsType(ENTITY_CHARACTER))
	{
		LPCHARACTER ch = (LPCHARACTER) this;
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;

		const auto e = chEntity;
		if (e != entt::null && g_registry.valid(e))
		{
			if (bFlag)
				g_registry.emplace_or_replace<ecs::ObserverModeTag>(e);
			else if (g_registry.all_of<ecs::ObserverModeTag>(e))
				g_registry.remove<ecs::ObserverModeTag>(e);

			if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
				status->isObserverMode = bFlag;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "ObserverMode %d", m_bIsObserver ? 1 : 0);
	}
}


