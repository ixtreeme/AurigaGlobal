#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Base/attribute.h>
#include "sectree_manager.h"
#include "char_manager.h"
#include "ecs/SpatialHelpers.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/components/item_components.hpp"
#include "ecs/components/transform_components.hpp"
#include "ecs/components/visibility_components.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/VisibilitySystem.hpp"
#include "desc_manager.h"

namespace {
std::unordered_set<entt::entity> relocating;
struct Relocation {
    entt::entity entity;
    bool entered;
    explicit Relocation(entt::entity e) : entity(e), entered(relocating.insert(e).second) {}
    ~Relocation() { if (entered) relocating.erase(entity); }
};
template<class T> bool Prepare(entt::entity e) {
    if (!g_registry.valid(e)) return false;
    if (!g_registry.all_of<T>(e)) g_registry.insert<T>(&e, &e + 1);
    return g_registry.valid(e) && g_registry.all_of<T>(e);
}
void Wake(entt::entity e) {
    if (!g_registry.valid(e) || g_registry.all_of<ecs::TagPC>(e)) return;
    const auto* type = g_registry.try_get<ecs::CharacterType>(e);
    if (type && type->value != CHAR_TYPE_WARP && type->value != CHAR_TYPE_GOTO)
        CHARACTER_MANAGER::instance().AddToStateList(e);
}
}

LPENTITY SectreeLegacyEntity(entt::entity e) {
    return ecs::SpatialService::LPENTITYFromEntity(g_registry, e);
}
bool SectreeMember(entt::entity e, const SECTREE* tree) {
    return g_registry.valid(e) && ecs::SectorOf(g_registry, e) == tree && tree && tree->Contains(e);
}

SECTREE::SECTREE() {
    static bool connected = false;
    if (!connected) {
        g_registry.on_destroy<ecs::SectorPlacement>().connect<&SECTREE::OnPlacementDestroyed>();
        connected = true;
    }
    Initialize();
}
void SECTREE::OnPlacementDestroyed(entt::registry& reg, entt::entity e) {
    // Registry destruction must also retire the native sector index. The
    // component still exists during this signal; do not recursively remove it.
    auto* tree = ecs::SectorOf(reg, e);
    if (!tree) return;
    auto it = tree->m_entities.find(e);
    if (it == tree->m_entities.end()) return;
    const bool pc = it->second;
    tree->m_entities.erase(it);
    if (pc) tree->DecreasePC();
}
SECTREE::~SECTREE() { Destroy(); }
void SECTREE::Initialize() {
    m_id.package = 0; m_pkAttribute = nullptr; m_iPCCount = 0; isClone = false;
    m_destroying = false;
}
bool SECTREE::Contains(entt::entity e) const { return m_entities.contains(e); }
void SECTREE::Collect(FCollectEntity& out) const {
    for (const auto& [e, pc] : m_entities)
        if (g_registry.valid(e)) out.Add(e, this);
}
FCollectEntity SECTREE::SnapshotAround(int rings) const {
    FCollectEntity result;
    std::unordered_set<const SECTREE*> visited {this};
    std::vector<const SECTREE*> current {this}, next;
    for (int ring = 0; ring <= rings && !current.empty(); ++ring) {
        next.clear();
        for (const auto* tree : current) {
            tree->Collect(result);
            for (const auto* neighbor : tree->m_neighbor_list)
                if (neighbor && visited.insert(neighbor).second) next.push_back(neighbor);
        }
        current.swap(next);
    }
    return result;
}
void SECTREE::Destroy() {
    if (m_destroying) return;
    m_destroying = true;
    const auto members = m_entities;
    for (const auto& [e, pc] : members) {
        if (!SectreeMember(e, this)) { m_entities.erase(e); continue; }
        const auto* kind = g_registry.try_get<ecs::SpatialKindTag>(e);
        const auto type = kind ? kind->kind : ecs::SpatialKind::Character;
        // Detach before callbacks. This region cannot accept new members.
        ecs::SpatialService::RemoveEntity(g_registry, e);
        if (!g_registry.valid(e) || ecs::SectorOf(g_registry, e)) continue;
        if (type == ecs::SpatialKind::Item)
            ItemSystem::DestroyItemEntityEcs(e, "SECTREE_DESTROY_ITEM");
        else if (type == ecs::SpatialKind::Character) {
            if (auto* desc = ecs::PlayerRuntime::GetDesc(e))
                DESC_MANAGER::instance().DestroyDesc(desc);
            else M2_DESTROY_CHARACTER(e);
        }
    }
    m_entities.clear();
    if (!isClone && m_pkAttribute) { M2_DELETE(m_pkAttribute); m_pkAttribute = nullptr; }
}
SECTREEID SECTREE::GetID() { return m_id; }
void SECTREE::IncreasePC() {
    // Build() normally includes self; standalone sectors must work as well.
    std::unordered_set<SECTREE*> neighbors(m_neighbor_list.begin(), m_neighbor_list.end());
    neighbors.insert(this);
    for (auto* tree : neighbors) {
        const bool wake = tree->m_iPCCount++ == 0;
        if (wake) {
            FCollectEntity list; tree->Collect(list);
            auto activate = [](entt::entity e) { Wake(e); };
            list.ForEach(activate);
        }
    }
}
void SECTREE::DecreasePC() {
    std::unordered_set<SECTREE*> neighbors(m_neighbor_list.begin(), m_neighbor_list.end());
    neighbors.insert(this);
    for (auto* tree : neighbors) {
        if (tree->m_iPCCount > 0) --tree->m_iPCCount;
        if (tree->m_iPCCount == 0) {
            FCollectEntity list; tree->Collect(list);
            auto stop = [](entt::entity e) {
                if (g_registry.all_of<ecs::CharacterType>(e) && !g_registry.all_of<ecs::TagPC>(e))
                    CHARACTER_MANAGER::instance().RemoveFromStateList(e);
            };
            list.ForEach(stop);
        }
    }
}
bool SECTREE::InsertEntity(LPENTITY legacy) {
    return InsertEntity(ecs::SpatialService::EntityFromLPENTITY(legacy));
}
void SECTREE::RemoveEntity(LPENTITY legacy) {
    RemoveEntity(ecs::SpatialService::EntityFromLPENTITY(legacy));
}
bool SECTREE::InsertEntity(entt::entity e) {
    if (IsDestroying() || !g_registry.valid(e) || ecs::VisibilitySystem::IsRemoving(g_registry, e)) return false;
    Relocation action(e);
    if (!action.entered) return false;
    auto* previous = ecs::SectorOf(g_registry, e);
    if (previous == this && Contains(e)) return false;
    const auto eligible = [&] {
        if (IsDestroying() || !g_registry.valid(e)) return false;
        if (g_registry.all_of<ecs::ItemIdentity>(e)) {
            const auto* owner = g_registry.try_get<ecs::ItemOwner>(e);
            const auto* location = g_registry.try_get<ecs::ItemLocation>(e);
            if ((owner && (owner->owner != entt::null || owner->ownerPID != 0)) ||
                !location || location->window != GROUND) return false;
        }
        const auto* pos = g_registry.try_get<ecs::Position>(e);
        const auto* map = g_registry.try_get<ecs::MapIndex>(e);
        return pos && map && ecs::SectorAt(map->value, pos->x, pos->y) == this;
    };
    const bool hadPlacement = g_registry.all_of<ecs::SectorPlacement>(e);
    if (!eligible() || !Prepare<ecs::SpatialRevision>(e) || !Prepare<ecs::SpatialEntity>(e) ||
        !Prepare<ecs::ViewActiveTag>(e) || !Prepare<ecs::VisibilityDirty>(e) || !Prepare<ecs::SectorPlacement>(e) ||
        !eligible() || !g_registry.all_of<ecs::SectorPlacement, ecs::SpatialRevision, ecs::SpatialEntity>(e)) {
        // A failed on_construct must not leave a phantom inventory-blocking
        // placement. Never remove an existing or callback-committed placement.
        if (!hadPlacement && g_registry.valid(e) && !ecs::SectorOf(g_registry, e))
            g_registry.remove<ecs::SectorPlacement>(e);
        return false;
    }
    // on_construct can remove a previous placement; it cannot recursively enter
    // another membership operation for the same handle.
    if (previous && !previous->Contains(e)) previous = nullptr;
    const bool pc = g_registry.all_of<ecs::TagPC>(e);
    const auto position = g_registry.get<ecs::Position>(e);
    const auto map = g_registry.get<ecs::MapIndex>(e).value;
    m_entities.emplace(e, pc); // Allocate before modifying previous membership.
    bool previousPC = false;
    if (previous) {
        if (auto it = previous->m_entities.find(e); it != previous->m_entities.end()) {
            previousPC = it->second;
            previous->m_entities.erase(it);
        }
    }
    g_registry.get<ecs::SectorPlacement>(e) = {map, uint32_t(position.x), uint32_t(position.y)};
    ++g_registry.get<ecs::SpatialRevision>(e).value;
    if (pc) IncreasePC();
    if (previousPC) previous->DecreasePC();
    if (g_registry.valid(e) && Contains(e) && !pc && m_iPCCount > 0) Wake(e);
    return true;
}
void SECTREE::RemoveEntity(entt::entity e) {
    Relocation action(e);
    if (!action.entered) return;
    const auto it = m_entities.find(e);
    if (it == m_entities.end()) return;
    const bool pc = it->second;
    m_entities.erase(it);
    if (g_registry.valid(e) && ecs::SectorOf(g_registry, e) == this) {
        if (auto* revision = g_registry.try_get<ecs::SpatialRevision>(e)) ++revision->value;
        g_registry.remove<ecs::SectorPlacement>(e);
    }
    if (pc) DecreasePC();
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


