#ifndef __INC_SECTREE_H__
#define __INC_SECTREE_H__

#include "entity.h"
#include <Core/Logging.hpp>
#include "ecs/Registry.hpp"
#include <unordered_map>
#include <type_traits>

enum ESectree
{
	SECTREE_SIZE	= 6400,
	SECTREE_HALF_SIZE	= 3200,
	CELL_SIZE		= 50
};

typedef struct sectree_coord
{
	int32_t            x : 16;
	int32_t            y : 16;
} SECTREE_COORD;

typedef union sectreeid
{
	uint32_t		package;
	SECTREE_COORD	coord;
} SECTREEID;

enum
{
	ATTR_BLOCK = (1 << 0),
	ATTR_WATER = (1 << 1),
	ATTR_BANPK = (1 << 2),
	ATTR_OBJECT = (1 << 7),
};

// Compatibility is confined to callbacks which have not migrated yet. The
// sector and every snapshot store versioned handles, never CEntity pointers.
LPENTITY SectreeLegacyEntity(entt::entity entity);
bool SectreeMember(entt::entity entity, const SECTREE* tree);

struct FCollectEntity {
    struct Entry { entt::entity entity; const SECTREE* tree; };
    std::vector<Entry> result;
    void Add(entt::entity entity, const SECTREE* tree) { result.push_back({entity, tree}); }
    template<typename F> void ForEach(F& f) {
        for (const auto& entry : result) {
            if (!SectreeMember(entry.entity, entry.tree)) continue;
            if constexpr (std::is_invocable_v<F&, entt::entity>)
                f(entry.entity);
            else if (auto* legacy = SectreeLegacyEntity(entry.entity))
                f(legacy);
        }
    }
};

class CAttribute;

class SECTREE
{
	public:
		friend class SECTREE_MANAGER;
		friend class SECTREE_MAP;


        // Native snapshot enumeration. Captured members are revalidated before
        // each callback, including when a prior callback moved/destroyed them.
        FCollectEntity SnapshotAround(int rings = 1) const;
        void Collect(FCollectEntity& out) const;
        template <class F> void ForEachAround(F& func) { auto list = SnapshotAround(); list.ForEach(func); }
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
        template <class F> void ForEachAroundPlus(F& func, int rings = 1) {
            auto list = SnapshotAround(rings); list.ForEach(func);
        }
#endif

	public:
		SECTREE();
		~SECTREE();

		void				Initialize();
		void				Destroy();

		SECTREEID			GetID();

        bool InsertEntity(entt::entity entity);
        void RemoveEntity(entt::entity entity);
        bool Contains(entt::entity entity) const;
        bool IsDestroying() const { return m_destroying || m_closed; }
        // Remaining character/building callers enter once at this boundary.
        bool InsertEntity(LPENTITY entity);
        void RemoveEntity(LPENTITY entity);

		void				SetRegenEvent(LPEVENT event);
		bool				Regen();

		void				IncreasePC();
		void				DecreasePC();

		void				BindAttribute(CAttribute * pkAttribute);

		CAttribute *			GetAttributePtr() { return m_pkAttribute; }

		uint32_t				GetAttribute(int32_t x, int32_t y);
		bool				IsAttr(int32_t x, int32_t y, uint32_t dwFlag);

		void				CloneAttribute(LPSECTREE tree); // private map 처리시 사용

		int				GetEventAttribute(int32_t x, int32_t y); // 20050313 현재는 사용하지 않음

		void				SetAttribute(uint32_t x, uint32_t y, uint32_t dwAttr);
		void				RemoveAttribute(uint32_t x, uint32_t y, uint32_t dwAttr);

	private:
		static void OnPlacementDestroyed(entt::registry& registry, entt::entity entity);
		SECTREEID			m_id;
		std::unordered_map<entt::entity, bool> m_entities; // value: counted as PC
        bool m_destroying = false;
        bool m_closed = false; // Whole-map teardown closes every sector first.
		LPSECTREE_LIST			m_neighbor_list;
		int				m_iPCCount;
		bool				isClone;

		CAttribute *			m_pkAttribute;
};

#endif
