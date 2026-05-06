#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <Base/attribute.h>
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/SpatialHelpers.hpp"
#include "ecs/components/spatial_components.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "item.h"
#include "item_manager.h"
#include "desc_manager.h"
#include "packet.h"

SECTREE::SECTREE()
{
	Initialize();
}

SECTREE::~SECTREE()
{
	Destroy();
}

void SECTREE::Initialize()
{
	m_id.package = 0;
	m_pkAttribute = nullptr;
	m_iPCCount = 0;
	isClone = false;
}

void SECTREE::Destroy()
{
	if (!m_set_entity.empty())
	{
		LOG_ERROR("Sectree: entity set not empty!!");

		ENTITY_SET::iterator it = m_set_entity.begin();

		while (it != m_set_entity.end())
		{
			LPENTITY ent = *(it++);

			if (!ent)
				continue;

			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER)ent;

				LOG_ERROR("Sectree: destroying character: {} is_pc {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) ? 1 : 0);

				if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
					DESC_MANAGER::instance().DestroyDesc(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)));
				else
					M2_DESTROY_CHARACTER(ch);
			}
			else if (ent->IsType(ENTITY_ITEM))
			{
				LPITEM item = (LPITEM)ent;

				LOG_ERROR("Sectree: destroying item: {}", item->GetName());
				ItemSystem::DestroyItemEntityEcs(
					EntityFactory::CreateItemEntity(g_registry, item),
					"SECTREE_DESTROY_ITEM");
			}
			else
			{
				LOG_ERROR("Sectree: unknown type: {}", ent->GetType());
			}
		}
	}

	m_set_entity.clear();

	if (!isClone && m_pkAttribute)
	{
		M2_DELETE(m_pkAttribute);
		m_pkAttribute = nullptr;
	}
}

SECTREEID SECTREE::GetID()
{
	return m_id;
}

void SECTREE::IncreasePC()
{
	LPSECTREE_LIST::iterator it_tree = m_neighbor_list.begin();

	while (it_tree != m_neighbor_list.end())
	{
		++(*it_tree)->m_iPCCount;
		++it_tree;
	}
}

void SECTREE::DecreasePC()
{
	LPSECTREE_LIST::iterator it_tree = m_neighbor_list.begin();

	while (it_tree != m_neighbor_list.end())
	{
		LPSECTREE tree = *it_tree++;

		if (--tree->m_iPCCount <= 0)
		{
			if (tree->m_iPCCount < 0)
			{
				const auto coordX = tree->m_id.coord.x;
				const auto coordY = tree->m_id.coord.y;
				LOG_ERROR("tree pc count lower than zero (value {} coord {} {})", tree->m_iPCCount, coordX, coordY);
				tree->m_iPCCount = 0;
			}

			ENTITY_SET::iterator it_entity = tree->m_set_entity.begin();

			while (it_entity != tree->m_set_entity.end())
			{
				LPENTITY pkEnt = *(it_entity++);

				if (pkEnt->IsType(ENTITY_CHARACTER))
				{
					LPCHARACTER ch = (LPCHARACTER) pkEnt;
					ch->StopStateMachine();
				}
			}
		}
	}
}

bool SECTREE::InsertEntity(LPENTITY pkEnt)
{
	LPSECTREE pkCurTree;

	if ((pkCurTree = pkEnt->GetSectree()) == this)
		return false;

	if (m_set_entity.find(pkEnt) != m_set_entity.end()) {
		LOG_ERROR("entity {} already exist in this sectree!", static_cast<const void*>(get_pointer(pkEnt)));
		return false;
	}

	if (pkCurTree)
		pkCurTree->m_set_entity.erase(pkEnt);

	// Phase 15E-final.LPENTITY.4-architect.H.3:
	// pkEnt->SetSectree(this) deleted - the legacy m_pSectree field is
	// gone; the ECS SectorPlacement update below is the sole sectree
	// reference for the entity.
	//pkEnt->UpdateSectree();

	// Phase 15E-final.LPENTITY.4-architect.H.2:
	// Mirror the legacy m_pSectree write into ECS SectorPlacement so the
	// H.1 GetSectree ECS-resolution path always sees the right sector
	// without relying on the dual-store maintenance running through
	// SpatialService::InsertEntity. The seam matters because several
	// callers reach SECTREE::InsertEntity directly (e.g. CHARACTER::Sync
	// at MovementSystem.cpp:634, CHARACTER::Show at SessionSystem.cpp:1005)
	// without an explicit SyncSectorPlacement call right after.
	//
	// Use pkEnt->GetMapIndex() rather than this->GetID() because SECTREEID
	// only carries (sectorX, sectorY) - the mapIndex is owned by the
	// SECTREE_MANAGER side, not stored on the SECTREE itself. The entity's
	// legacy m_lMapIndex is set by every InsertEntity caller before this
	// point (e.g. CHARACTER::Show line 934 SetMapIndex(lMapIndex)).
	{
		const entt::entity e = ecs::SpatialService::EntityFromLPENTITY(pkEnt);
		if (e != entt::null && g_registry.valid(e))
		{
			ecs::SyncSectorPlacement(
				g_registry, e, pkEnt->GetMapIndex(), pkEnt->GetX(), pkEnt->GetY());
		}
	}

	m_set_entity.insert(pkEnt);

	if (pkEnt->IsType(ENTITY_CHARACTER))
	{
		LPCHARACTER pkChr = (LPCHARACTER) pkEnt;

		if ((ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pkChr))))
		{
			IncreasePC();

			if (pkCurTree)
				pkCurTree->DecreasePC();
		}
		else if (m_iPCCount > 0 && !pkChr->IsWarp() && !pkChr->IsGoto()) // PC�� �ƴϰ� �� ���� PC�� �ִٸ� Idle event�� ���� ��Ų��.
		{
			pkChr->StartStateMachine();
		}
	}

	return true;
}

void SECTREE::RemoveEntity(LPENTITY pkEnt)
{
	ENTITY_SET::iterator it = m_set_entity.find(pkEnt);

	if (it == m_set_entity.end()) {
		return;
	}
	m_set_entity.erase(it);

	// Phase 15E-final.LPENTITY.4-architect.H.3:
	// pkEnt->SetSectree(nullptr) deleted. The ECS SectorPlacement remove
	// below is the sole "no longer in any sector" signal.
	//
	// reg.remove is idempotent - if the component is already gone (e.g.
	// SpatialService::RemoveEntity removed SectorPlacement explicitly at
	// line 269) the second remove is a no-op.
	{
		const entt::entity e = ecs::SpatialService::EntityFromLPENTITY(pkEnt);
		if (e != entt::null && g_registry.valid(e))
			g_registry.remove<ecs::SectorPlacement>(e);
	}

	if (pkEnt->IsType(ENTITY_CHARACTER))
	{
	if (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf((LPCHARACTER) pkEnt)))
			DecreasePC();
	}
}

void SECTREE::BindAttribute(CAttribute * pkAttribute)
{
	m_pkAttribute = pkAttribute;
}

void SECTREE::CloneAttribute(LPSECTREE tree)
{
	m_pkAttribute = tree->m_pkAttribute;
	isClone = true;
}

void SECTREE::SetAttribute(uint32_t x, uint32_t y, uint32_t dwAttr)
{
	assert(m_pkAttribute != NULL);
	m_pkAttribute->Set(x, y, dwAttr);
}

void SECTREE::RemoveAttribute(uint32_t x, uint32_t y, uint32_t dwAttr)
{
	assert(m_pkAttribute != NULL);
	m_pkAttribute->Remove(x, y, dwAttr);
}

uint32_t SECTREE::GetAttribute(int32_t x, int32_t y)
{
	assert(m_pkAttribute != NULL);
	return m_pkAttribute->Get((x % SECTREE_SIZE) / CELL_SIZE, (y % SECTREE_SIZE) / CELL_SIZE);
}

bool SECTREE::IsAttr(int32_t x, int32_t y, uint32_t dwFlag)
{
	if (IS_SET(GetAttribute(x, y), dwFlag))
		return true;

	return false;
}

int SECTREE::GetEventAttribute(int32_t x, int32_t y)
{
	return GetAttribute(x, y) >> 8;
}


