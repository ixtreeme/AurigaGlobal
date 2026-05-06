#include "stdafx.h"
#include "char_interface.hpp"
#include "config.h"
#include "desc.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "utils.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/dirty_components.hpp"
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
	m_iViewAge = 0;
	m_pos.x = m_pos.y = m_pos.z = 0;
	m_map_view.clear();

	m_pSectree = nullptr;
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
	ViewCleanup();
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

struct FuncPacketAround
{
	const void *        m_data;
	int                 m_bytes;
	LPENTITY            m_except;

	FuncPacketAround(const void * data, int bytes, LPENTITY except = nullptr) :m_data(data), m_bytes(bytes), m_except(except)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (ent == m_except)
			return;

		if (ent->GetDesc())
			ent->GetDesc()->Packet(m_data, m_bytes);
	}
};

struct FuncPacketView : public FuncPacketAround
{
	FuncPacketView(const void * data, int bytes, LPENTITY except = nullptr) : FuncPacketAround(data, bytes, except)
	{}

	void operator() (const CEntity::ENTITY_MAP::value_type& v)
	{
		FuncPacketAround::operator() (v.first);
	}
};

void CEntity::PacketAround(const void * data, int bytes, LPENTITY except)
{

	PacketView(data, bytes, except);
}

void CEntity::PacketView(const void * data, int bytes, LPENTITY except)
{
	if (!GetSectree())
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
	FuncPacketAround f(data, bytes, except);
	std::unordered_set<LPENTITY> sent;

	if (!m_bIsObserver)
	{
		const entt::entity selfE = ecs::SpatialService::EntityFromLPENTITY(this);
		bool walkedViewerMap = false;

		if (selfE != entt::null && g_registry.valid(selfE))
		{
			if (auto* viewerMap = g_registry.try_get<ecs::ViewerMap>(selfE))
			{
				walkedViewerMap = true;
				for (const entt::entity viewerE : viewerMap->viewers)
				{
					if (viewerE == entt::null || !g_registry.valid(viewerE))
						continue;
					LPENTITY viewer = ecs::SpatialService::LPENTITYFromEntity(g_registry, viewerE);
					if (!viewer || viewer == except)
						continue;
					if (sent.insert(viewer).second)
						f(viewer);
				}
			}
		}

		// Fallback for non-character sources whose ViewerMap is incomplete
		// (the path that pre-D.6 fed them from chars' UpdateSectree polling
		// is gone; PacketView usage on non-chars is currently nil but the
		// safety net guards against silent regressions). For chars whose
		// ViewerMap was iterated above, this walk is skipped entirely.
		if (!walkedViewerMap)
		{
			const int32_t range = VIEW_RANGE + VIEW_BONUS_RANGE;
			const int32_t selfX = GetX();
			const int32_t selfY = GetY();
			struct RangeCollector {
				LPENTITY self;
				LPENTITY except;
				int32_t selfX;
				int32_t selfY;
				int32_t range;
				std::unordered_set<LPENTITY>& sent;
				FuncPacketAround& f;
				void operator()(LPENTITY ent)
				{
					if (!ent || ent == self || ent == except)
						return;
					if (!ent->IsType(ENTITY_CHARACTER))
						return;
					if (!ent->GetDesc())
						return;
					if (DISTANCE_APPROX(ent->GetX() - selfX, ent->GetY() - selfY) > range)
						return;
					if (sent.insert(ent).second)
						f(ent);
				}
			} collector { this, except, selfX, selfY, range, sent, f };

			GetSectree()->ForEachAround(collector);
		}
	}

	if (sent.insert(this).second)
		f(this);
}

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
		const auto e = AIHelpers::EcsOf(ch);
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
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "ObserverMode %d", m_bIsObserver ? 1 : 0);
	}
}


