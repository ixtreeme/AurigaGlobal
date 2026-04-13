#pragma once

#include "ItemData.h"

class CItemManager : public CSingleton<CItemManager>
{
	public:
		enum EItemDescCol
		{
			ITEMDESC_COL_VNUM,
			ITEMDESC_COL_NAME,
			ITEMDESC_COL_DESC,
			ITEMDESC_COL_SUMM,
			ITEMDESC_COL_NUM,
		};



#ifdef ENABLE_ACCE_SYSTEM
		enum EItemScaleColumn
		{
			ITEMSCALE_VNUM,
			ITEMSCALE_JOB,
			ITEMSCALE_SEX,
			ITEMSCALE_SCALE_X,
			ITEMSCALE_SCALE_Y,
			ITEMSCALE_SCALE_Z,
			ITEMSCALE_POSITION_X,
			ITEMSCALE_POSITION_Y,
			ITEMSCALE_POSITION_Z,
			ITEMSCALE_NUM,
		};
#endif




	public:
		typedef std::map<uint32_t, CItemData*> TItemMap;
		typedef std::map<std::string, CItemData*> TItemNameMap;
		typedef std::vector<CItemData*> TItemVec;
		typedef std::vector<uint32_t> TItemNumVec;
	
	public:
		void WikiAddVnumToBlacklist(uint32_t vnum)
		{
			auto it = m_ItemMap.find(vnum);
			if (it != m_ItemMap.end())
				it->second->SetBlacklisted(true);
		};
		
		TItemNumVec* WikiGetLastItems()
		{
			return &m_tempItemVec;
		}
		
		bool								CanLoadWikiItem(uint32_t dwVnum);
		uint32_t							GetWikiItemStartRefineVnum(uint32_t dwVnum);
		std::string							GetWikiItemBaseRefineName(uint32_t dwVnum);
		size_t								WikiLoadClassItems(uint8_t classType, uint32_t raceFilter);
		std::tuple<const char*, int>	SelectByNamePart(const char * namePart);
	
	protected:
		TItemNumVec m_tempItemVec;
	
	private:
		bool IsFilteredAntiflag(CItemData* itemData, uint32_t raceFilter);

	public:
		CItemManager();
		virtual ~CItemManager();

		void			Destroy();

		bool			SelectItemData(uint32_t dwIndex);
		CItemData *		GetSelectedItemDataPointer();

#ifdef ENABLE_ITEM_EXTRA_PROTO
		CItemData::TItemExtraProto* GetSelectedExtraProto();
#endif

		bool			GetItemDataPointer(uint32_t dwItemID, CItemData ** ppItemData);

		/////
		bool			LoadItemDesc(const char* c_szFileName);
		bool			LoadItemList(const char* c_szFileName);
		bool			LoadItemTable(const char* c_szFileName);
#ifdef ENABLE_SHINING_SYSTEM
		bool			LoadShiningTable(const char* c_szFileName);
#endif
#ifdef ENABLE_ITEM_EXTRA_PROTO
		bool			LoadItemExtraProto(std::string filename);
		CItemData::TItemExtraProto* GetExtraProto(uint32_t vnum);
#endif
		CItemData *		MakeItemData(uint32_t dwIndex);
#ifdef __ENABLE_NEW_OFFLINESHOP__
		void			GetItemsNameMap(std::map<uint32_t, std::string>& inMap);
#endif

#ifdef ENABLE_ACCE_SYSTEM
		bool			LoadItemScale(const char* c_szFileName);
#endif

		bool			CanIncrRefineLevel();

	protected:
		TItemMap m_ItemMap;
		std::vector<CItemData*>  m_vec_ItemRange;
		CItemData * m_pSelectedItemData;
#ifdef ENABLE_ITEM_EXTRA_PROTO
		std::map<uint32_t, CItemData::TItemExtraProto> m_map_extraProto;
		CItemData::TItemExtraProto* m_pSelectedExtraProto;
#endif
};
