#ifndef __INC_ITEM_MANAGER__
#define __INC_ITEM_MANAGER__

#include <entt/entity/entity.hpp>

#include <common/CommonDefines.h>

class ITEM;

class ITEM_MANAGER : public singleton<ITEM_MANAGER>
{
	public:
		ITEM_MANAGER();
		virtual ~ITEM_MANAGER();
		bool                    Initialize(TItemTable * table, int size);
#ifdef ENABLE_ITEM_EXTRA_PROTO
		bool					InitializeExtraProto(TItemExtraProto* table, uint32_t count);
		TItemExtraProto*		GetExtraProto(uint32_t vnum);
#endif

		void			Destroy();

		uint32_t			GetNewID();
		bool			SetMaxItemID(TItemIDRangeTable range);
		bool			SetMaxSpareItemID(TItemIDRangeTable range);

		entt::entity            CreateItem(uint32_t vnum, uint32_t count = 1, uint32_t dwID = 0, bool bTryMagic = false, int iRarePct = -1, bool bSkipSave = false);
		void DestroyItem(entt::entity item);
		void			RemoveItem(entt::entity item, const char * c_pszReason = nullptr);

		TItemTable *            GetTable(uint32_t vnum);
		bool			GetVnum(const char * c_pszName, uint32_t & r_dwVnum);

		bool			GetDropPct(entt::entity victim, entt::entity killer, OUT double& iDeltaPercent, OUT int& iRandRange);


		const std::vector<TItemTable> & GetTable() { return m_vec_prototype; }
        uint64_t BonusCatalogRevision() const;


	protected:
		int                     RealNumber( uint32_t vnum) const;

	protected:
		typedef std::map<uint32_t, entt::entity> ITEM_VID_MAP;

#ifdef ENABLE_ITEM_EXTRA_PROTO
		std::map<uint32_t, TItemExtraProto> m_map_ExtraProto;
#endif
		std::vector<TItemTable>		m_vec_prototype;
		std::vector<TItemTable*> m_vec_item_vnum_range_info;
		int				m_iTopOfTable;
		ITEM_VID_MAP			m_VIDMap;
		uint32_t				m_dwCurrentID;
		TItemIDRangeTable	m_ItemIDRange;
		TItemIDRangeTable	m_ItemIDSpareRange;

		// Versioned identities, retained across cleanup callbacks and entity destruction.
		std::unordered_set<entt::entity> m_itemsBeingDestroyed;

	private:
		void ForgetItem(entt::entity item, uint32_t id, uint32_t vid);
		void DestroyItemNow(entt::entity item);
	protected:
		std::map<uint32_t, entt::entity>		m_map_pkItemByID;


	public:
		std::map<uint32_t, TItemTable>  m_map_vid;
		std::map<uint32_t, TItemTable>&  GetVIDMap() { return m_map_vid; }
		std::vector<TItemTable>& GetVecProto() { return m_vec_prototype; }

#ifndef ENABLE_SWITCHBOT
		const static int MAX_NORM_ATTR_NUM = ITEM_ATTRIBUTE_NORM_NUM;
		const static int MAX_RARE_ATTR_NUM = ITEM_ATTRIBUTE_RARE_NUM;
#endif
#ifdef ENABLE_EXTRA_INVENTORY
		bool IsExtraItem(uint32_t vnum);
#endif
	private:

};

#endif
