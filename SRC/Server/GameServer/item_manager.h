#ifndef __INC_ITEM_MANAGER__
#define __INC_ITEM_MANAGER__

#include <entt/entity/entity.hpp>

#ifdef __INGAME_WIKI__
#include <memory>
#include <common/in_game_wiki.h>
#endif
#include <common/CommonDefines.h>

#include "fifo_allocator.h"

struct TargetInfoItem
{
	uint32_t vnum = 0;
	uint32_t count = 0;
};

class CSpecialAttrGroup
{
public:
	CSpecialAttrGroup(uint32_t vnum)
		: m_dwVnum(vnum)
	{}
	struct CSpecialAttrInfo
	{
		CSpecialAttrInfo (uint32_t _apply_type, uint32_t _apply_value)
			: apply_type(_apply_type), apply_value(_apply_value)
		{}
		uint32_t apply_type;
		uint32_t apply_value;

	};
	uint32_t m_dwVnum;
	std::string	m_stEffectFileName;
	std::vector<CSpecialAttrInfo> m_vecAttrs;
};

class CSpecialItemGroup
{
	public:
		enum EGiveType
		{
			NONE,
			GOLD,
			EXP,
			MOB,
			SLOW,
			DRAIN_HP,
			POISON,
			MOB_GROUP,
		};

		enum ESIGType { NORMAL, PCT, QUEST, SPECIAL };

		struct CSpecialItemInfo
		{
			uint32_t vnum;
			int count;
			int rare;

			CSpecialItemInfo(uint32_t _vnum, int _count, int _rare)
				: vnum(_vnum), count(_count), rare(_rare)
				{}
		};

		CSpecialItemGroup(uint32_t vnum, uint8_t type=0)
			: m_dwVnum(vnum), m_bType(type)
			{}

		void AddItem(uint32_t vnum, int count, int prob, int rare)
		{
			if (!prob)
				return;
			if (!m_vecProbs.empty())
				prob += m_vecProbs.back();
			m_vecProbs.push_back(prob);
			m_vecItems.push_back(CSpecialItemInfo(vnum, count, rare));
		}

		bool IsEmpty() const
		{
			return m_vecProbs.empty();
		}

		// by rtsummit
		int GetMultiIndex(std::vector <int> &idx_vec) const
		{
			idx_vec.clear();
			if (m_bType == PCT)
			{
				int count = 0;
				if (number(1,100) <= m_vecProbs[0])
				{
					idx_vec.push_back(0);
					count++;
				}
				for (uint32_t i = 1; i < m_vecProbs.size(); i++)
				{
					if (number(1,100) <= m_vecProbs[i] - m_vecProbs[i-1])
					{
						idx_vec.push_back(i);
						count++;
					}
				}
				return count;
			}
			else
			{
				idx_vec.push_back(GetOneIndex());
				return 1;
			}
		}

		int GetOneIndex() const
		{
			int n = number(1, m_vecProbs.back());
			auto it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
			return std::distance(m_vecProbs.begin(), it);
		}

		int GetVnum(int idx) const
		{
			return m_vecItems[idx].vnum;
		}

		int GetCount(int idx) const
		{
			return m_vecItems[idx].count;
		}

		int GetRarePct(int idx) const
		{
			return m_vecItems[idx].rare;
		}

		bool Contains(uint32_t dwVnum) const
		{
			for (uint32_t i = 0; i < m_vecItems.size(); i++)
			{
				if (m_vecItems[i].vnum == dwVnum)
					return true;
			}
			return false;
		}

		uint32_t GetAttrVnum(uint32_t dwVnum) const
		{
			if (CSpecialItemGroup::SPECIAL != m_bType)
				return 0;
			for (auto it = m_vecItems.begin(); it != m_vecItems.end(); ++it)
			{
				if (it->vnum == dwVnum)
				{
					return it->count;
				}
			}
			return 0;
		}

		int GetGroupSize() const
		{
			return m_vecProbs.size();
		}

		uint32_t m_dwVnum;
		uint8_t	m_bType;
		std::vector<int> m_vecProbs;
		std::vector<CSpecialItemInfo> m_vecItems; // vnum, count
};

class CMobItemGroup
{
	public:
		struct SMobItemGroupInfo
		{
			uint32_t dwItemVnum;
			int iCount;
			int iRarePct;

			SMobItemGroupInfo(uint32_t dwItemVnum, int iCount, int iRarePct)
				: dwItemVnum(dwItemVnum),
			iCount(iCount),
			iRarePct(iRarePct)
			{
			}
		};

		CMobItemGroup(uint32_t dwMobVnum, int iKillDrop, const std::string& r_stName)
			:
			m_dwMobVnum(dwMobVnum),
		m_iKillDrop(iKillDrop),
		m_stName(r_stName)
		{
		}

		int GetKillPerDrop() const
		{
			return m_iKillDrop;
		}

		void AddItem(uint32_t dwItemVnum, int iCount, int iPartPct, int iRarePct)
		{
			if (!m_vecProbs.empty())
				iPartPct += m_vecProbs.back();
			m_vecProbs.push_back(iPartPct);
			m_vecItems.push_back(SMobItemGroupInfo(dwItemVnum, iCount, iRarePct));
		}

		// MOB_DROP_ITEM_BUG_FIX
		bool IsEmpty() const
		{
			return m_vecProbs.empty();
		}

		int GetOneIndex() const
		{
			int n = number(1, m_vecProbs.back());
			auto it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
			return std::distance(m_vecProbs.begin(), it);
		}
		// END_OF_MOB_DROP_ITEM_BUG_FIX

		const SMobItemGroupInfo& GetOne() const
		{
			return m_vecItems[GetOneIndex()];
		}

	private:
		uint32_t m_dwMobVnum;
		int m_iKillDrop;
		std::string m_stName;
		std::vector<int> m_vecProbs;
		std::vector<SMobItemGroupInfo> m_vecItems;
};

class CDropItemGroup
{
	struct SDropItemGroupInfo
	{
		uint32_t	dwVnum;
		uint32_t	dwPct;
		int	iCount;

		SDropItemGroupInfo(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: dwVnum(dwVnum), dwPct(dwPct), iCount(iCount)
			{}
	};

	public:
	CDropItemGroup(uint32_t dwVnum, uint32_t dwMobVnum, const std::string& r_stName)
		:
		m_dwVnum(dwVnum),
	m_dwMobVnum(dwMobVnum),
	m_stName(r_stName)
	{
	}

	const std::vector<SDropItemGroupInfo> & GetVector()
	{
		return m_vec_items;
	}

	void AddItem(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
	{
		m_vec_items.push_back(SDropItemGroupInfo(dwItemVnum, dwPct, iCount));
	}

	private:
	uint32_t m_dwVnum;
	uint32_t m_dwMobVnum;
	std::string m_stName;
	std::vector<SDropItemGroupInfo> m_vec_items;
};

class CLevelItemGroup
{
	struct SLevelItemGroupInfo
	{
		uint32_t dwVNum;
		uint32_t dwPct;
		int iCount;

		SLevelItemGroupInfo(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: dwVNum(dwVnum), dwPct(dwPct), iCount(iCount)
		{ }
	};

	private :
		uint32_t m_dwLevelLimit;
		std::string m_stName;
		std::vector<SLevelItemGroupInfo> m_vec_items;

	public :
		CLevelItemGroup(uint32_t dwLevelLimit)
			: m_dwLevelLimit(dwLevelLimit)
		{}

		uint32_t GetLevelLimit() { return m_dwLevelLimit; }

		void AddItem(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
		{
			m_vec_items.push_back(SLevelItemGroupInfo(dwItemVnum, dwPct, iCount));
		}

		const std::vector<SLevelItemGroupInfo> & GetVector()
		{
			return m_vec_items;
		}
};

class CBuyerThiefGlovesItemGroup
{
	struct SThiefGroupInfo
	{
		uint32_t	dwVnum;
		uint32_t	dwPct;
		int	iCount;

		SThiefGroupInfo(uint32_t dwVnum, uint32_t dwPct, int iCount)
			: dwVnum(dwVnum), dwPct(dwPct), iCount(iCount)
			{}
	};

	public:
	CBuyerThiefGlovesItemGroup(uint32_t dwVnum, uint32_t dwMobVnum, const std::string& r_stName)
		:
		m_dwVnum(dwVnum),
	m_dwMobVnum(dwMobVnum),
	m_stName(r_stName)
	{
	}

	const std::vector<SThiefGroupInfo> & GetVector()
	{
		return m_vec_items;
	}

	void AddItem(uint32_t dwItemVnum, uint32_t dwPct, int iCount)
	{
		m_vec_items.push_back(SThiefGroupInfo(dwItemVnum, dwPct, iCount));
	}

	private:
	uint32_t m_dwVnum;
	uint32_t m_dwMobVnum;
	std::string m_stName;
	std::vector<SThiefGroupInfo> m_vec_items;
};

class ITEM;

class ITEM_MANAGER : public singleton<ITEM_MANAGER>
{
	public:
		ITEM_MANAGER();
		virtual ~ITEM_MANAGER();
#ifdef ENABLE_METINSTONE_DROP_BUGFIX_RAZOR9d
		bool IsRegisteredDropMob(uint32_t dwMobVnum) const;
#endif

		bool                    Initialize(TItemTable * table, int size);
#ifdef ENABLE_ITEM_EXTRA_PROTO
		bool					InitializeExtraProto(TItemExtraProto* table, uint32_t count);
		TItemExtraProto*		GetExtraProto(uint32_t vnum);
#endif

		void			Destroy();
		void			Update();
		void			GracefulShutdown();
#ifdef __INGAME_WIKI__
		uint32_t											GetWikiItemStartRefineVnum(uint32_t dwVnum);
		std::string											GetWikiItemBaseRefineName(uint32_t dwVnum);
		int												GetWikiMaxRefineLevel(uint32_t dwVnum);
		
		CommonWikiData::TWikiInfoTable*						GetItemWikiInfo(uint32_t vnum);
		std::vector<CommonWikiData::TWikiRefineInfo>		GetWikiRefineInfo(uint32_t vnum);
		std::vector<CSpecialItemGroup::CSpecialItemInfo>	GetWikiChestInfo(uint32_t vnum);
		std::vector<CommonWikiData::TWikiItemOriginInfo>&	GetItemOrigin(uint32_t vnum) { return m_itemOriginMap[vnum]; }
#endif
		uint32_t			GetNewID();
		bool			SetMaxItemID(TItemIDRangeTable range);
		bool			SetMaxSpareItemID(TItemIDRangeTable range);

		void			DelayedSave(entt::entity item);
		void			FlushDelayedSave(entt::entity item);
		void FlushDelayedSaveByOwner(entt::entity owner);
		// True when sent or already retired; false keeps a delayed save pending.
		bool			SaveSingleItem(entt::entity item);

		entt::entity            CreateItem(uint32_t vnum, uint32_t count = 1, uint32_t dwID = 0, bool bTryMagic = false, int iRarePct = -1, bool bSkipSave = false);
#ifndef DEBUG_ALLOC
		void DestroyItem(entt::entity item);
#else
		void DestroyItem(entt::entity item, const char* file, size_t line);
#endif
		void			RemoveItem(entt::entity item, const char * c_pszReason = nullptr);

		TItemTable *            GetTable(uint32_t vnum);
		bool			GetVnum(const char * c_pszName, uint32_t & r_dwVnum);
		bool			GetVnumByOriginalName(const char * c_pszName, uint32_t & r_dwVnum);

		bool			GetDropPct(entt::entity victim, entt::entity killer, OUT int& iDeltaPercent, OUT int& iRandRange);
		bool			CreateDropItem(entt::entity victim, entt::entity killer, std::vector<entt::entity>& vec_item);
#ifdef __SEND_TARGET_INFO__
		bool			CreateDropItemVector(entt::entity victim, entt::entity killer, std::vector<TargetInfoItem>& items);
#endif
		bool			ReadCommonDropItemFile(const char * c_pszFileName);
		bool			ReadEtcDropItemFile(const char * c_pszFileName);
		bool			ReadDropItemGroup(const char * c_pszFileName);
		bool			ReadMonsterDropItemGroup(const char * c_pszFileName);
		bool			ReadSpecialDropItemFile(const char * c_pszFileName);

		// convert name -> vnum special_item_group.txt
		bool			ConvSpecialDropItemFile();
		// convert name -> vnum special_item_group.txt

		uint32_t			GetRefineFromVnum(uint32_t dwVnum);


		const CSpecialItemGroup* GetSpecialItemGroup(uint32_t dwVnum);
		const CSpecialAttrGroup* GetSpecialAttrGroup(uint32_t dwVnum);

		const std::vector<TItemTable> & GetTable() { return m_vec_prototype; }

		// CHECK_UNIQUE_GROUP
		int			GetSpecialGroupFromItem(uint32_t dwVnum) const { const auto it = m_ItemToSpecialGroup.find(dwVnum); return (it == m_ItemToSpecialGroup.end()) ? 0 : it->second; }
		// END_OF_CHECK_UNIQUE_GROUP

	protected:
		int                     RealNumber( uint32_t vnum) const;

	protected:
		typedef std::map<uint32_t, entt::entity> ITEM_VID_MAP;

#ifdef ENABLE_ITEM_EXTRA_PROTO
		std::map<uint32_t, TItemExtraProto> m_map_ExtraProto;
#endif
		std::vector<TItemTable>		m_vec_prototype;
		std::vector<TItemTable*> m_vec_item_vnum_range_info;
		std::map<uint32_t, uint32_t>		m_map_ItemRefineFrom;
		int				m_iTopOfTable;
#ifdef __INGAME_WIKI__
		std::map<uint32_t, std::unique_ptr<CommonWikiData::TWikiInfoTable>> m_wikiInfoMap;
		std::map<uint32_t, std::vector<CommonWikiData::TWikiItemOriginInfo>> m_itemOriginMap;
#endif
		ITEM_VID_MAP			m_VIDMap;
		uint32_t				m_dwVIDCount;
		uint32_t				m_dwCurrentID;
		TItemIDRangeTable	m_ItemIDRange;
		TItemIDRangeTable	m_ItemIDSpareRange;

		std::unordered_set<entt::entity> m_set_pkItemForDelayedSave;
		// Versioned identities, retained across cleanup callbacks and entity destruction.
		std::unordered_set<entt::entity> m_itemsBeingDestroyed;

	private:
		void ForgetItem(entt::entity item, uint32_t id, uint32_t vid);
		void DestroyItemNow(entt::entity item, const char* file, size_t line);
	protected:
		std::map<uint32_t, entt::entity>		m_map_pkItemByID;
		std::map<uint32_t, uint32_t>		m_map_dwEtcItemDropProb;
		std::map<uint32_t, CDropItemGroup*> m_map_pkDropItemGroup;
		std::map<uint32_t, CSpecialItemGroup*> m_map_pkSpecialItemGroup;
		std::map<uint32_t, CSpecialItemGroup*> m_map_pkQuestItemGroup;
		std::map<uint32_t, CSpecialAttrGroup*> m_map_pkSpecialAttrGroup;
		std::map<uint32_t, CMobItemGroup*> m_map_pkMobItemGroup;
		std::map<uint32_t, CLevelItemGroup*> m_map_pkLevelItemGroup;
		std::map<uint32_t, CBuyerThiefGlovesItemGroup*> m_map_pkGloveItemGroup;


		// CHECK_UNIQUE_GROUP
		std::map<uint32_t, int>		m_ItemToSpecialGroup;
		// END_OF_CHECK_UNIQUE_GROUP

	private:
		typedef std::map <uint32_t, uint32_t> TMapDW2DW;
		TMapDW2DW	m_map_new_to_ori;

	public:
		uint32_t	GetMaskVnum(uint32_t dwVnum);
		std::map<uint32_t, TItemTable>  m_map_vid;
		std::map<uint32_t, TItemTable>&  GetVIDMap() { return m_map_vid; }
		std::vector<TItemTable>& GetVecProto() { return m_vec_prototype; }

#ifndef ENABLE_SWITCHBOT
		const static int MAX_NORM_ATTR_NUM = ITEM_ATTRIBUTE_NORM_NUM;
		const static int MAX_RARE_ATTR_NUM = ITEM_ATTRIBUTE_RARE_NUM;
#endif
		bool ReadItemVnumMaskTable(const char * c_pszFileName);
#ifdef ENABLE_EXTRA_INVENTORY
		bool IsExtraItem(uint32_t vnum);
#endif
	private:

};

#ifndef DEBUG_ALLOC
#define M2_DESTROY_ITEM(ptr) ITEM_MANAGER::instance().DestroyItem(ptr)
#else
#define M2_DESTROY_ITEM(ptr) ITEM_MANAGER::instance().DestroyItem(ptr, __FILE__, __LINE__)
#endif

#endif
