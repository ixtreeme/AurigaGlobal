#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <Core/Logging.hpp>

#include "utils.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/Registry.hpp"
#include "ecs/services/EntityNetworkDispatch.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/components/visibility_components.hpp"
#include "sectree_manager.h"
#include "config.h"

#include <unordered_map>
#include <utility>

namespace
{

entt::entity EntityOf(LPENTITY entity)
{
	return ecs::SpatialService::EntityFromLPENTITY(entity);
}

void EnsureVisibilityComponents(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	(void)g_registry.get_or_emplace<ecs::ViewMap>(e);
	(void)g_registry.get_or_emplace<ecs::ViewerMap>(e);
	(void)g_registry.get_or_emplace<ecs::ViewAgeMap>(e);
}

void MirrorViewInsert(LPENTITY owner, LPENTITY other, uint32_t generation)
{
	const entt::entity ownerE = EntityOf(owner);
	const entt::entity otherE = EntityOf(other);
	if (ownerE == entt::null || otherE == entt::null || !g_registry.valid(ownerE) || !g_registry.valid(otherE))
		return;

	EnsureVisibilityComponents(ownerE);
	EnsureVisibilityComponents(otherE);

	auto& view = g_registry.get<ecs::ViewMap>(ownerE);
	view.visible.insert(otherE);
	view.generation = generation;

	auto& ageMap = g_registry.get<ecs::ViewAgeMap>(ownerE);
	ageMap.ageByEntity[otherE] = generation;

	auto& reverse = g_registry.get<ecs::ViewerMap>(otherE);
	reverse.viewers.insert(ownerE);
}

void MirrorViewRemove(LPENTITY owner, LPENTITY other)
{
	const entt::entity ownerE = EntityOf(owner);
	const entt::entity otherE = EntityOf(other);
	if (ownerE == entt::null || otherE == entt::null || !g_registry.valid(ownerE))
		return;

	if (auto* view = g_registry.try_get<ecs::ViewMap>(ownerE))
		view->visible.erase(otherE);
	if (auto* ageMap = g_registry.try_get<ecs::ViewAgeMap>(ownerE))
		ageMap->ageByEntity.erase(otherE);

	if (g_registry.valid(otherE)) {
		if (auto* reverse = g_registry.try_get<ecs::ViewerMap>(otherE))
			reverse->viewers.erase(ownerE);
	}
}

void MirrorViewClear(LPENTITY owner)
{
	const entt::entity ownerE = EntityOf(owner);
	if (ownerE == entt::null || !g_registry.valid(ownerE))
		return;

	if (auto* view = g_registry.try_get<ecs::ViewMap>(ownerE)) {
		for (const entt::entity otherE : view->visible) {
			if (otherE != entt::null && g_registry.valid(otherE)) {
				if (auto* reverse = g_registry.try_get<ecs::ViewerMap>(otherE))
					reverse->viewers.erase(ownerE);
			}
		}
		view->visible.clear();
	}

	if (auto* ageMap = g_registry.try_get<ecs::ViewAgeMap>(ownerE))
		ageMap->ageByEntity.clear();

	if (auto* reverse = g_registry.try_get<ecs::ViewerMap>(ownerE)) {
		for (const entt::entity viewerE : reverse->viewers) {
			if (viewerE != entt::null && g_registry.valid(viewerE)) {
				if (auto* view = g_registry.try_get<ecs::ViewMap>(viewerE))
					view->visible.erase(ownerE);
				if (auto* ageMap = g_registry.try_get<ecs::ViewAgeMap>(viewerE))
					ageMap->ageByEntity.erase(ownerE);
			}
		}
		reverse->viewers.clear();
	}
}

void ValidateViewMapMirror(LPENTITY owner, const CEntity::ENTITY_MAP& legacyView, const char* context)
{
#ifdef AURIGA_LPENTITY_FIXUP_AUDIT
	const entt::entity ownerE = EntityOf(owner);
	if (ownerE == entt::null || !g_registry.valid(ownerE))
		return;

	// Phase 15E-final.LPENTITY.4-architect.D.6:
	// For characters, m_map_view is no longer maintained - the legacy
	// CFuncViewInsert polling in UpdateSectree was disabled in D.6, and
	// ViewCleanup / ViewReencode now read ECS ViewMap/ViewerMap directly.
	// The dual-mirror invariant only holds for non-character entities;
	// silencing the drift log here for characters avoids 30K-warning
	// log floods that would otherwise stall the game thread (the same
	// crash mode 261e74d guarded against).
	if (owner && owner->IsType(ENTITY_CHARACTER))
		return;

	// LPENTITY.4-fixup-B + audit: O(N^2) validation gated and rate-limited
	// per entity. Without throttling this fires once per ViewInsert refresh
	// (hundreds per second per entity) - 35K log warnings in 30 seconds
	// drowned the async log queue and slowed the game thread enough that
	// regen / spawn events stalled and the client connection timed out.
	//
	// The drift information is still useful: log once per entity per
	// 30 seconds. If drift state stays unchanged the log is suppressed;
	// state changes still surface because we record on first observation.
	static thread_local std::unordered_map<uint64_t, uint32_t> s_lastLogTick;
	const uint32_t nowMs = static_cast<uint32_t>(get_dword_time());
	const uint64_t key = static_cast<uint64_t>(ownerE);
	if (auto it = s_lastLogTick.find(key); it != s_lastLogTick.end()) {
		if (nowMs - it->second < 30000u)
			return;
	}

	EnsureVisibilityComponents(ownerE);

	const auto& view = g_registry.get<ecs::ViewMap>(ownerE);
	size_t legacySize = 0;
	size_t missingInEcs = 0;

	for (const auto& [legacyOther, age] : legacyView) {
		const entt::entity otherE = EntityOf(legacyOther);
		if (otherE == entt::null || !g_registry.valid(otherE))
			continue;

		++legacySize;
		if (view.visible.find(otherE) == view.visible.end())
			++missingInEcs;
	}

	size_t missingInLegacy = 0;
	for (const entt::entity visibleE : view.visible) {
		bool found = false;
		for (const auto& [legacyOther, age] : legacyView) {
			if (EntityOf(legacyOther) == visibleE) {
				found = true;
				break;
			}
		}
		if (!found)
			++missingInLegacy;
	}

	if (missingInEcs != 0 || missingInLegacy != 0 || legacySize != view.visible.size()) {
		s_lastLogTick[key] = nowMs;
		LOG_WARN("[VIEWMAP_DRIFT] entity={} ctx={} legacy_size={} ecs_size={} missing_in_ecs={} missing_in_legacy={}",
			static_cast<uint32_t>(ownerE),
			context ? context : "unknown",
			legacySize,
			view.visible.size(),
			missingInEcs,
			missingInLegacy);
	}
#else
	(void)owner;
	(void)legacyView;
	(void)context;
#endif
}

void DispatchInsert(LPENTITY source, LPENTITY viewer, const char* context)
{
	const entt::entity sourceE = EntityOf(source);
	const entt::entity viewerE = EntityOf(viewer);
	if (sourceE != entt::null && viewerE != entt::null && g_registry.valid(sourceE) && g_registry.valid(viewerE)) {
		ecs::EntityNetworkDispatch::SendInsert(g_registry, sourceE, viewerE);
		return;
	}

	LOG_WARN("[DISPATCH_FALLBACK] ctx={} op=insert source={} viewer={}",
		context ? context : "unknown",
		static_cast<const void*>(source),
		static_cast<const void*>(viewer));
	source->EncodeInsertPacket(viewer);
}

void DispatchRemove(LPENTITY source, LPENTITY viewer, const char* context)
{
	const entt::entity sourceE = EntityOf(source);
	const entt::entity viewerE = EntityOf(viewer);
	if (sourceE != entt::null && viewerE != entt::null && g_registry.valid(sourceE) && g_registry.valid(viewerE)) {
		ecs::EntityNetworkDispatch::SendRemove(g_registry, sourceE, viewerE);
		return;
	}

	LOG_WARN("[DISPATCH_FALLBACK] ctx={} op=remove source={} viewer={}",
		context ? context : "unknown",
		static_cast<const void*>(source),
		static_cast<const void*>(viewer));
	source->EncodeRemovePacket(viewer);
}

} // namespace

void CEntity::ViewCleanup()
{
	// Phase 15E-final.LPENTITY.4-architect.D.6 + fixup-7:
	// For characters, m_map_view is no longer maintained (the legacy
	// CFuncViewInsert poll in UpdateSectree was disabled in D.6). Walk
	// the ECS ViewerMap.viewers instead - that is the event-driven set
	// of "who currently sees this character", kept current by the
	// VisibilitySystem D.4 handler.
	//
	// fixup-7: emit SendRemove DIRECTLY via EntityNetworkDispatch rather
	// than going through the legacy CEntity::ViewRemove(this, false).
	// CEntity::ViewRemove(entity) does:
	//
	//   const auto it = m_map_view.find(entity);
	//   if (it == m_map_view.end()) return;   // <-- early-out
	//   m_map_view.erase(it);
	//   MirrorViewRemove(this, entity);
	//   if (!entity->m_bIsObserver)
	//       DispatchRemove(entity, this, "view.remove");   // <-- the SendRemove
	//
	// The early-out triggers whenever the viewer's legacy m_map_view does
	// NOT contain `this`. After D.6 stubbed UpdateSectree polling for
	// characters, every char viewer's m_map_view is frozen at whatever
	// the spawn-time CFuncViewInsert filled - so any `self` that came
	// into existence after the viewer's spawn is missing from
	// viewer.m_map_view, the early-out fires, and DispatchRemove is
	// silently skipped. The user-visible symptom: Metin stones spawned
	// after a player logged in stay rendered on the player's client when
	// killed (HEADER_GC_DEAD reaches the client and starts the shatter
	// animation, but HEADER_GC_CHARACTER_DELETE never arrives).
	//
	// Direct SendRemove sidesteps the stale-mirror check entirely. The
	// MirrorViewClear call below handles the ECS-side ViewMap/ViewerMap
	// cleanup (both for self and for every reverse pair).
	if (IsType(ENTITY_CHARACTER))
	{
		const entt::entity selfE = EntityOf(this);
		if (selfE != entt::null && g_registry.valid(selfE))
		{
			if (auto* viewerMap = g_registry.try_get<ecs::ViewerMap>(selfE))
			{
				// Snapshot - the loop body mutates ViewerMap via
				// MirrorViewClear at the end.
				const auto viewers = viewerMap->viewers;
				for (const entt::entity viewerE : viewers)
				{
					if (viewerE == entt::null || !g_registry.valid(viewerE))
						continue;
					ecs::EntityNetworkDispatch::SendRemove(g_registry, selfE, viewerE);
				}
			}
		}
		MirrorViewClear(this);
		// m_map_view may still hold stale entries from before D.6 - clear
		// it so the legacy structure does not leak entries into post-Phase-D
		// reads (PacketView f76a3f1 hybrid, ViewReencode, etc.).
		m_map_view.clear();
		return;
	}

	auto it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		LPENTITY entity = it->first;
		++it;

		entity->ViewRemove(this, false);
	}

	m_map_view.clear();
	MirrorViewClear(this);
	ValidateViewMapMirror(this, m_map_view, "view.cleanup");
}

void CEntity::ViewReencode()
{
	if (m_bIsObserver)
		return;

	DispatchRemove(this, this, "view.reencode.self");
	DispatchInsert(this, this, "view.reencode.self");

	// Phase 15E-final.LPENTITY.4-architect.D.6:
	// For characters, walk the ECS ViewMap.visible (what this character
	// currently sees) rather than the now-unmaintained m_map_view.
	if (IsType(ENTITY_CHARACTER))
	{
		const entt::entity selfE = EntityOf(this);
		if (selfE != entt::null && g_registry.valid(selfE))
		{
			if (auto* viewMap = g_registry.try_get<ecs::ViewMap>(selfE))
			{
				const auto visible = viewMap->visible; // snapshot
				for (const entt::entity otherE : visible)
				{
					if (otherE == entt::null || !g_registry.valid(otherE))
						continue;
					LPENTITY other = ecs::SpatialService::LPENTITYFromEntity(g_registry, otherE);
					if (!other)
						continue;

					// Phase 15E-final.LPENTITY.4-architect H fixup-5:
					// Peer-direction respawn burst removed. The pre-fixup-5 body
					// emitted DispatchRemove + DispatchInsert(this, other) which
					// forced every peer client to despawn-then-respawn the moving
					// character, resetting any in-progress movement animation
					// render-side. Pre-Phase D the per-tick polling re-emitted
					// GC_MOVE which papered over the reset; after D.6 stubbed
					// the polling, peers got stuck rendering the post-reset
					// idle pose. The DispatchInsert(other, this) reverse below
					// stays - it refreshes the SELF client's render of the peer,
					// which is the legitimate purpose of ViewReencode for the
					// re-encoding character.

					if (!other->m_bIsObserver)
						DispatchInsert(other, this, "view.reencode.reverse");
				}
			}
		}
		return;
	}

	auto it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		LPENTITY entity = it++->first;

		DispatchRemove(this, entity, "view.reencode.visible");
		if (!m_bIsObserver)
			DispatchInsert(this, entity, "view.reencode.visible");

		if (!entity->m_bIsObserver)
			DispatchInsert(entity, this, "view.reencode.reverse");
	}

}

void CEntity::ViewInsert(LPENTITY entity, bool recursive)
{
	if (this == entity)
		return;

	if (auto it = m_map_view.find(entity); m_map_view.end() != it)
	{
		it->second = m_iViewAge;
		MirrorViewInsert(this, entity, static_cast<uint32_t>(m_iViewAge));
		ValidateViewMapMirror(this, m_map_view, "view.insert.refresh");
		return;
	}

	m_map_view.insert(ENTITY_MAP::value_type(entity, m_iViewAge));
	MirrorViewInsert(this, entity, static_cast<uint32_t>(m_iViewAge));

	if (!entity->m_bIsObserver)
		DispatchInsert(entity, this, "view.insert");

	if (recursive)
		entity->ViewInsert(this, false);

	ValidateViewMapMirror(this, m_map_view, "view.insert");
}

void CEntity::ViewRemove(LPENTITY entity, bool recursive)
{
	const auto it = m_map_view.find(entity);

	if (it == m_map_view.end())
		return;

	m_map_view.erase(it);
	MirrorViewRemove(this, entity);

	if (!entity->m_bIsObserver)
		DispatchRemove(entity, this, "view.remove");

	if (recursive)
		entity->ViewRemove(this, false);

	ValidateViewMapMirror(this, m_map_view, "view.remove");
}

class CFuncViewInsert
{
private:
	int dwViewRange;

public:
	LPENTITY m_me;

	CFuncViewInsert(LPENTITY ent) :
		dwViewRange(VIEW_RANGE + VIEW_BONUS_RANGE),
		m_me(ent)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_OBJECT))
			if (DISTANCE_APPROX(ent->GetX() - m_me->GetX(), ent->GetY() - m_me->GetY()) > dwViewRange)
				return;

		m_me->ViewInsert(ent);

		if (ent->IsType(ENTITY_CHARACTER) && m_me->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER chMe = (LPCHARACTER)m_me;
			LPCHARACTER chEnt = (LPCHARACTER)ent;

			if (ecs::PlayerRuntime::IsPC(((chMe) ? (chMe)->GetEntityHandle() : entt::null)) && !ecs::PlayerRuntime::IsPC(((chEnt) ? (chEnt)->GetEntityHandle() : entt::null)) && !chEnt->IsWarp() && !chEnt->IsGoto())
				chEnt->StartStateMachine();
		}
	}
};

void CEntity::UpdateSectree()
{
	if (!GetSectree())
	{
		if (IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER tch = (LPCHARACTER)this;
			LOG_ERROR("null sectree name: {} {} {}", ecs::PlayerRuntime::GetName(((tch) ? (tch)->GetEntityHandle() : entt::null)).data(), GetX(), GetY());
		}

		return;
	}

	// Phase 15E-final.LPENTITY.4-architect.D.6:
	// Character visibility maintenance is now event-driven via the
	// PositionChangedEvent + VisibilitySystem handler (D.4). The legacy
	// polling loop here (++m_iViewAge; CFuncViewInsert; m_map_view stale
	// cleanup) is the original f76a3f1 root cause: idle characters do
	// not re-poll, so their m_map_view goes stale during their stillness
	// window. Skipping the polling for characters delegates entirely to
	// the event handler. m_map_view, m_iViewAge, and the ValidateViewMapMirror
	// audit are all unmaintained for characters until Phase G deletes
	// them outright.
	//
	// Items / buildings / offline shops still go through the legacy
	// polling (their visibility-maintenance triggers are the spawn-time
	// SpatialService::UpdateSectree calls in InventorySystem, building.cpp,
	// new_offlineshop_manager.cpp - which produce the per-call full
	// neighbour walk that those entity types need).
	if (IsType(ENTITY_CHARACTER))
		return;

	++m_iViewAge;

	CFuncViewInsert f(this);
	GetSectree()->ForEachAround(f);

	ENTITY_MAP::iterator it, this_it;

	if (m_bObserverModeChange)
	{
		if (m_bIsObserver)
		{
			it = m_map_view.begin();

			while (it != m_map_view.end())
			{
				this_it = it++;
				if (this_it->second < m_iViewAge)
				{
					LPENTITY ent = this_it->first;

					DispatchRemove(ent, this, "view.update.observer.remove_stale");
					m_map_view.erase(this_it);
					MirrorViewRemove(this, ent);

					ent->ViewRemove(this, false);
				}
				else
				{

					LPENTITY ent = this_it->first;
					//ent->EncodeRemovePacket(this);
					//m_map_view.erase(this_it);

					//ent->ViewRemove(this, false);
					DispatchRemove(this, ent, "view.update.observer.remove_self");
				}
			}
		}
		else
		{
			it = m_map_view.begin();

			while (it != m_map_view.end())
			{
				this_it = it++;

				if (this_it->second < m_iViewAge)
				{
					LPENTITY ent = this_it->first;

					DispatchRemove(ent, this, "view.update.observer_exit.remove_stale");
					m_map_view.erase(this_it);
					MirrorViewRemove(this, ent);

					ent->ViewRemove(this, false);
				}
				else
				{
					LPENTITY ent = this_it->first;
					DispatchInsert(ent, this, "view.update.observer_exit.insert_reverse");
					DispatchInsert(this, ent, "view.update.observer_exit.insert_self");

					ent->ViewInsert(this, true);
				}
			}
		}

		m_bObserverModeChange = false;
	}
	else
	{
		if (!m_bIsObserver)
		{
			it = m_map_view.begin();

			while (it != m_map_view.end())
			{
				this_it = it++;

				if (this_it->second < m_iViewAge)
				{
					LPENTITY ent = this_it->first;

					DispatchRemove(ent, this, "view.update.remove_stale");
					m_map_view.erase(this_it);
					MirrorViewRemove(this, ent);

					ent->ViewRemove(this, false);
				}
			}
		}
	}

	ValidateViewMapMirror(this, m_map_view, "view.update_sectree");
}


