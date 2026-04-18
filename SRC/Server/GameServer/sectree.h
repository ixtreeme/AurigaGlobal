#ifndef __INC_SECTREE_H__
#define __INC_SECTREE_H__

#include "entity.h"

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

struct FCollectEntity {
	void operator()(LPENTITY entity) {
		// Consider removing sanity check after debug pass
		/*
		if (entity->IsType(ENTITY_CHARACTER)) {
			LPCHARACTER character = (LPCHARACTER)entity;
			uint32_t vid = character->GetLegacyVID();
			LPCHARACTER found = CHARACTER_MANGAER::instance().Find(vid);
			if (found == NULL || vid != found->GetLegacyVID()) {
				sys_err("<Factor> Invalid character %p", get_pointer(character));
				return;
			}
		} else if (entity->IsType(ENTITY_ITEM)) {
			LPITEM item = (LPITEM)entity;
			uint32_t vid = item->GetVID();
			LPITEM found = ITEM_MANGAER::instance().FindByVID(vid);
			if (found == NULL || vid != found->GetLegacyVID()) {
				sys_err("<Factor> Invalid item %p", get_pointer(item));
				return;
			}
		} else if (entity->IsType(ENTITY_OBJECT)) {
			LPOBJECT object = (LPOBJECT)entity;
			uint32_t vid = object->GetVID();
			LPOBJECT found = CManager::instance().FindObjectByVID(vid);
			if (found == NULL || vid != found->GetLegacyVID()) {
				sys_err("<Factor> Invalid object %p", get_pointer(object));
				return;
			}
		} else {
			sys_err("<Factor> Invalid entity type %p", get_pointer(entity));
			return;
		}
		*/
		result.push_back(entity);
	}
	template<typename F>
	void ForEach(F& f) {
		std::vector<LPENTITY>::iterator it = result.begin();
		for ( ; it != result.end(); ++it) {
			LPENTITY entity = *it;
			f(entity);
		}
	}
	typedef std::vector<LPENTITY> ListType;
	ListType result; // list collected
};

class CAttribute;

class SECTREE
{
	public:
		friend class SECTREE_MANAGER;
		friend class SECTREE_MAP;

		template <class _Func> LPENTITY	find_if (_Func & func) const
		{
			auto it_tree = m_neighbor_list.begin();

			while (it_tree != m_neighbor_list.end())
			{
				ENTITY_SET::iterator it_entity = (*it_tree)->m_set_entity.begin();

				while (it_entity != (*it_tree)->m_set_entity.end())
				{
					if (func(*it_entity))
						return (*it_entity);

					++it_entity;
				}

				++it_tree;
			}

			return nullptr;
		}

		template <class _Func> void ForEachAround(_Func & func)
		{
			// <Factor> Using snapshot copy to avoid side-effects
			FCollectEntity collector;
			for (auto it = m_neighbor_list.begin(); it != m_neighbor_list.end(); ++it)
			{
				const LPSECTREE sectree = *it;
				sectree->for_each_entity(collector);
			}
			collector.ForEach(func);
			/*
			LPSECTREE_LIST::iterator it_tree = m_neighbor_list.begin();
			for ( ; it_tree != m_neighbor_list.end(); ++it_tree) {
				(*it_tree)->for_each_entity(func);
			}
			*/
		}
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93



		
		template <class _Func>
		void ForEachAroundPlus(_Func& func, int rings)
		{
			
			FCollectEntity collector;

			std::set<LPSECTREE> visited;
			std::vector<LPSECTREE> cur, next;

			visited.insert(this);
			cur.push_back(this);

			for (int r = 0; r <= rings; ++r)
			{
				next.clear();

				for (size_t i = 0; i < cur.size(); ++i)
				{
					LPSECTREE st = cur[i];
					st->for_each_entity(collector);

					LPSECTREE_LIST::iterator it = st->m_neighbor_list.begin();
					for (; it != st->m_neighbor_list.end(); ++it)
					{
						LPSECTREE nb = *it;
						if (visited.insert(nb).second)
							next.push_back(nb);
					}
				}

				cur.swap(next);
			}

			collector.ForEach(func);
		}

		
		template <class _Func>
		void ForEachAroundPlus(_Func& func)
		{
			ForEachAroundPlus(func, 1);
		}

#endif
		template <class _Func> void for_each_for_find_victim(_Func & func)
		{
			LPSECTREE_LIST::iterator it_tree = m_neighbor_list.begin();

			while (it_tree != m_neighbor_list.end())
			{
				//첫번째를 찾으면 바로 리턴
				if ( (*(it_tree++))->for_each_entity_for_find_victim(func) )
					return;
			}
		}
		template <class _Func> bool for_each_entity_for_find_victim(_Func & func)
		{
			auto it = m_set_entity.begin();

			while (it != m_set_entity.end())
			{
				//정상적으로 찾으면 바로 리턴
				if ( func(*it++) )
					return true;
			}
			return false;
		}


	public:
		SECTREE();
		~SECTREE();

		void				Initialize();
		void				Destroy();

		SECTREEID			GetID();

		bool				InsertEntity(LPENTITY ent);
		void				RemoveEntity(LPENTITY ent);

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
		template <class _Func> void for_each_entity(_Func& func)
		{
			auto it = m_set_entity.begin();

			while (it != m_set_entity.end())
			{
				LPENTITY entity = *it;

				if (entity->GetSectree() != this)
				{
					sys_err("<Factor> SECTREE-ENTITY relationship mismatch ent=%p tree=%p enttree=%p",
						get_pointer(entity), this, entity ? entity->GetSectree() : nullptr);
					it = m_set_entity.erase(it);
					continue;
				}

				++it;
				func(entity);
			}
		}

		SECTREEID			m_id;
		ENTITY_SET			m_set_entity;
		LPSECTREE_LIST			m_neighbor_list;
		int				m_iPCCount;
		bool				isClone;

		CAttribute *			m_pkAttribute;
};

#endif
