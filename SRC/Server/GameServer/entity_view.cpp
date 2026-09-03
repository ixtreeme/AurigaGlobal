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
#include "ecs/systems/VisibilitySystem.hpp"
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


// The (peer, age) pairs UpdateSectree used to read out of m_map_view, taken
// from ViewMap + ViewAgeMap instead. Snapshotted because every caller erases
// and dispatches while walking. The LPENTITY in the pair is unavoidable here:
// the loop bodies call CEntity::ViewRemove and the LPENTITY dispatchers, which
// still need the object. UpdateSectree only runs for items, buildings and
// offline shops - characters returned early since D.6.
std::vector<std::pair<LPENTITY, uint32_t>> SnapshotView(entt::entity ownerE)
{
    std::vector<std::pair<LPENTITY, uint32_t>> out;
    if (ownerE == entt::null || !g_registry.valid(ownerE))
        return out;

    const auto* view = g_registry.try_get<ecs::ViewMap>(ownerE);
    if (!view)
        return out;

    const auto* ageMap = g_registry.try_get<ecs::ViewAgeMap>(ownerE);
    out.reserve(view->visible.size());

    for (const entt::entity otherE : view->visible)
    {
        if (otherE == entt::null || !g_registry.valid(otherE))
            continue;

        LPENTITY other = ecs::SpatialService::LPENTITYFromEntity(g_registry, otherE);
        if (!other)
            continue;

        uint32_t age = 0;
        if (ageMap)
        {
            if (const auto it = ageMap->ageByEntity.find(otherE); it != ageMap->ageByEntity.end())
                age = it->second;
        }
        out.emplace_back(other, age);
    }
    return out;
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
		return;
	}

	for (const auto& [entity, age] : SnapshotView(EntityOf(this)))
		entity->ViewRemove(this, false);

	MirrorViewClear(this);
}

void CEntity::ViewReencode()
{
	if (m_bIsObserver)
		return;

	// The character branch is ecs::VisibilitySystem::Reencode now. It used to
	// live here, walking ViewMap.visible and turning each entity back into an
	// LPENTITY purely so DispatchInsert could turn it forward again. Only
	// non-characters fall through to the legacy m_map_view walk below - they
	// are the ones that still maintain it.
	if (IsType(ENTITY_CHARACTER))
	{
		ecs::VisibilitySystem::Reencode(EntityOf(this));
		return;
	}

	DispatchRemove(this, this, "view.reencode.self");
	DispatchInsert(this, this, "view.reencode.self");

	for (const auto& [entity, age] : SnapshotView(EntityOf(this)))
	{
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

	// Phase G: the ECS ViewMap is the view, not a mirror of m_map_view. This
	// body is the old MirrorViewInsert plus the presence test m_map_view used
	// to answer.
	const entt::entity self = EntityOf(this);
	const entt::entity other = EntityOf(entity);
	if (self == entt::null || other == entt::null
		|| !g_registry.valid(self) || !g_registry.valid(other))
	{
		LOG_WARN("[VIEW_UNTRACKED] insert with no entity handle: self={} other={}",
			static_cast<const void*>(this), static_cast<const void*>(entity));
		return;
	}

	EnsureVisibilityComponents(self);
	EnsureVisibilityComponents(other);

	auto& view = g_registry.get<ecs::ViewMap>(self);
	const bool wasVisible = view.visible.find(other) != view.visible.end();

	view.visible.insert(other);
	view.generation = static_cast<uint32_t>(m_iViewAge);
	g_registry.get<ecs::ViewAgeMap>(self).ageByEntity[other] = static_cast<uint32_t>(m_iViewAge);
	g_registry.get<ecs::ViewerMap>(other).viewers.insert(self);

	if (wasVisible)
		return;  // refresh: the age moved, nothing to announce

	if (!entity->m_bIsObserver)
		DispatchInsert(entity, this, "view.insert");

	if (recursive)
		entity->ViewInsert(this, false);
}

void CEntity::ViewRemove(LPENTITY entity, bool recursive)
{
	const entt::entity self = EntityOf(this);
	const entt::entity other = EntityOf(entity);
	if (self == entt::null || other == entt::null || !g_registry.valid(self))
		return;

	// The erase doubles as the "was it there?" early-out m_map_view provided.
	auto* view = g_registry.try_get<ecs::ViewMap>(self);
	if (!view || view->visible.erase(other) == 0)
		return;

	if (auto* ageMap = g_registry.try_get<ecs::ViewAgeMap>(self))
		ageMap->ageByEntity.erase(other);

	if (g_registry.valid(other))
	{
		if (auto* reverse = g_registry.try_get<ecs::ViewerMap>(other))
			reverse->viewers.erase(self);
	}

	if (!entity->m_bIsObserver)
		DispatchRemove(entity, this, "view.remove");

	if (recursive)
		entity->ViewRemove(this, false);
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

	if (m_bObserverModeChange)
	{
		if (m_bIsObserver)
		{
			for (const auto& [ent, age] : SnapshotView(EntityOf(this)))
			{
				if (age < static_cast<uint32_t>(m_iViewAge))
				{
					DispatchRemove(ent, this, "view.update.observer.remove_stale");
					ViewRemove(ent, false);

					ent->ViewRemove(this, false);
				}
				else
				{
					DispatchRemove(this, ent, "view.update.observer.remove_self");
				}
			}
		}
		else
		{
			for (const auto& [ent, age] : SnapshotView(EntityOf(this)))
			{
				if (age < static_cast<uint32_t>(m_iViewAge))
				{
					DispatchRemove(ent, this, "view.update.observer_exit.remove_stale");
					ViewRemove(ent, false);

					ent->ViewRemove(this, false);
				}
				else
				{
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
			for (const auto& [ent, age] : SnapshotView(EntityOf(this)))
			{
				if (age < static_cast<uint32_t>(m_iViewAge))
				{
					DispatchRemove(ent, this, "view.update.remove_stale");
					ViewRemove(ent, false);

					ent->ViewRemove(this, false);
				}
			}
		}
	}

}


