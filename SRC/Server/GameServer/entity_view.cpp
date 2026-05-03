#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <Core/Logging.hpp>

#include "utils.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/Registry.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/components/visibility_components.hpp"
#include "sectree_manager.h"
#include "config.h"

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
	const entt::entity ownerE = EntityOf(owner);
	if (ownerE == entt::null || !g_registry.valid(ownerE))
		return;

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
		LOG_WARN("[VIEWMAP_DRIFT] entity={} ctx={} legacy_size={} ecs_size={} missing_in_ecs={} missing_in_legacy={}",
			static_cast<uint32_t>(ownerE),
			context ? context : "unknown",
			legacySize,
			view.visible.size(),
			missingInEcs,
			missingInLegacy);
	}
}

} // namespace

void CEntity::ViewCleanup()
{
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

	EncodeRemovePacket(this);
	EncodeInsertPacket(this);

	auto it = m_map_view.begin();

	while (it != m_map_view.end())
	{
		LPENTITY entity = it++->first;

		EncodeRemovePacket(entity);
		if (!m_bIsObserver)
			EncodeInsertPacket(entity);

		if (!entity->m_bIsObserver)
			entity->EncodeInsertPacket(this);
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
		entity->EncodeInsertPacket(this);

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
		entity->EncodeRemovePacket(this);

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

			if (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(chMe)) && !ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(chEnt)) && !chEnt->IsWarp() && !chEnt->IsGoto())
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
			LOG_ERROR("null sectree name: {} {} {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(tch)).data(), GetX(), GetY());
		}

		return;
	}

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

					ent->EncodeRemovePacket(this);
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
					EncodeRemovePacket(ent);
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

					ent->EncodeRemovePacket(this);
					m_map_view.erase(this_it);
					MirrorViewRemove(this, ent);

					ent->ViewRemove(this, false);
				}
				else
				{
					LPENTITY ent = this_it->first;
					ent->EncodeInsertPacket(this);
					EncodeInsertPacket(ent);

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

					ent->EncodeRemovePacket(this);
					m_map_view.erase(this_it);
					MirrorViewRemove(this, ent);

					ent->ViewRemove(this, false);
				}
			}
		}
	}

	ValidateViewMapMirror(this, m_map_view, "view.update_sectree");
}


