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

	// LPENTITY.4 sync drift fix: hybrid broadcast.
	//
	// Original logic iterated only m_map_view (legacy snapshot updated by
	// the periodic UpdateSectree). Two-client desync repro (chars at same
	// server pos but visually apart on one client) traced back to viewers
	// missing MOVE / SYNC_POSITION / effect packets because the broadcast
	// source's m_map_view did not include them at the moment of broadcast.
	// Causes: sectree boundary, asymmetric age-out, post-warp gap before
	// the next UpdateSectree pass populated m_map_view.
	//
	// New logic:
	//   1. Send to every entity in m_map_view (legacy behaviour preserved).
	//   2. Additionally walk the sectree neighbour list and send to any
	//      character within VIEW_RANGE + VIEW_BONUS_RANGE that wasn't
	//      already in m_map_view. Dedup via a small set so no recipient
	//      gets the same packet twice.
	//   3. Send to self last (matches the original f(make_pair(this, 0))
	//      semantics).
	//
	// Recipients with no descriptor are skipped by FuncPacketAround. The
	// extra sectree walk costs ~one sectree neighbour iteration per
	// broadcast; cheap relative to packet construction and send.
	std::unordered_set<LPENTITY> sent;
	sent.reserve(m_map_view.size() + 16);

	FuncPacketAround f(data, bytes, except);

	if (!m_bIsObserver)
	{
		for (const auto& entry : m_map_view)
		{
			LPENTITY ent = entry.first;
			if (!ent || ent == except)
				continue;
			if (sent.insert(ent).second)
				f(ent);
		}

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


