#pragma once

#include "../Base/Singleton.h"
#include "../Render/GrpTextInstance.h"
#include "../Render/GrpMarkInstance.h"
#include "../Granny/ThingInstance.h"
#include "../Game/MapType.h"

class CPythonTextTail : public CSingleton<CPythonTextTail>
{
	public:
		typedef struct STextTail
		{
			CGraphicTextInstance* pTextInstance = nullptr;
			CGraphicTextInstance* pOwnerTextInstance = nullptr;

			CGraphicMarkInstance* pMarkInstance = nullptr;
			CGraphicTextInstance* pGuildNameTextInstance = nullptr;

			CGraphicTextInstance* pTitleTextInstance = nullptr;
			CGraphicTextInstance* pLevelTextInstance = nullptr;

#ifdef ENABLE_FAKE_SHOP_HEADER
			CGraphicTextInstance* pMountCountInstance = nullptr;
			CGraphicImageInstance* pMountCountIconInstance = nullptr;

			std::string MountCountText;
			D3DXCOLOR MountCountColor = D3DXCOLOR(0, 0, 0, 0);
			std::string MountCountIconPath;
#endif

#ifdef ENABLE_MULTI_LANGUAGE
			CGraphicImageInstance* pLanguageInstance = nullptr;
#endif

			CGraphicObjectInstance* pOwner = nullptr;

			uint32_t dwVirtualID = 0;

			float x = 0.0f, y = 0.0f, z = 0.0f;
			float fDistanceFromPlayer = 0.0f;
			D3DXCOLOR Color = D3DXCOLOR(0, 0, 0, 0);
			bool bNameFlag = FALSE;

			float xStart = 0.0f, yStart = 0.0f, xEnd = 0.0f, yEnd = 0.0f;

			uint32_t LivingTime = 0;

			float fHeight = 0.0f;

#ifdef __ENABLE_NEW_OFFLINESHOP__
#   ifdef ENABLE_NEW_SHOP_IN_CITIES
			bool bIsShop = false;
			bool bRender = false;
#   endif
#endif

#if defined(WJ_SHOW_MOB_INFO)
			bool bIsPC = FALSE;
#endif


			
			#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
						std::string BaseName;
						std::string ItemOnTitlePrefix;
			#endif

						STextTail() = default;
			virtual ~STextTail() = default;

		} TTextTail;


		typedef std::map<uint32_t, TTextTail*>		TTextTailMap;
		typedef std::list<TTextTail*>			TTextTailList;
		typedef TTextTailMap					TChatTailMap;

	public:
		CPythonTextTail(void);
		virtual ~CPythonTextTail(void);

		void GetInfo(std::string* pstInfo);

		void Initialize();
		void Destroy();
		void Clear();

		void UpdateAllTextTail();
		void UpdateShowingTextTail();
		void Render();

		void ArrangeTextTail();
		void HideAllTextTail();
		void ShowAllTextTail();
		void ShowCharacterTextTail(uint32_t VirtualID);
		void ShowItemTextTail(uint32_t VirtualID);
#ifdef ENABLE_FAKE_SHOP_HEADER//icon
		void RegisterMountCountTextTail(uint32_t dwVID, const std::string& mountText);
		void DeleteMountCountTextTail(uint32_t dwVID);
		void AttachMountCountWithIcon(uint32_t dwVID, const char* szText, const D3DXCOLOR& color, const char* szIconPath);
#endif

		void RegisterCharacterTextTail(uint32_t dwGuildID, uint32_t dwVirtualID, const D3DXCOLOR & c_rColor, float fAddHeight=10.0f, bool IsPc = false);
		void RegisterItemTextTail(uint32_t VirtualID, const char * c_szText, CGraphicObjectInstance * pOwner);
		void RegisterChatTail(uint32_t VirtualID, const char * c_szChat);
		void RegisterInfoTail(uint32_t VirtualID, const char * c_szChat);
		void SetCharacterTextTailColor(uint32_t VirtualID, const D3DXCOLOR & c_rColor);
		void SetItemTextTailOwner(uint32_t dwVID, const char * c_szName);
		void DeleteCharacterTextTail(uint32_t VirtualID);
		#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
			void SetItemOnTitlePrefix(uint32_t dwVID, const char* szPrefix);
			void UpdateItemOnTitleTextTail(uint32_t dwVID);
		#endif
		void DeleteItemTextTail(uint32_t VirtualID);
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		void RegisterShopInstanceTextTail(uint32_t dwVirtualID, const char* c_szName, CGraphicObjectInstance* pOwner);
		void DeleteShopTextTail(uint32_t VirtualID);
		TTextTail * RegisterShopTextTail(uint32_t dwVirtualID, const char * c_szText, CGraphicObjectInstance * pOwner);
		void SetShopTextTailColor(uint32_t dwVirtualID, const D3DXCOLOR& c_rColor);
		bool GetPickedNewShop(uint32_t* pdwVID);
#endif

		int Pick(int ixMouse, int iyMouse);
		void SelectItemName(uint32_t dwVirtualID);

		bool GetTextTailPosition(uint32_t dwVID, float* px, float* py, float* pz);
		bool IsChatTextTail(uint32_t dwVID);

		void EnablePKTitle(bool bFlag);
		void AttachTitle(uint32_t dwVID, const char * c_szName, const D3DXCOLOR& c_rColor);
#ifdef ENABLE_FAKE_SHOP_HEADER
		void AttachMountCount(uint32_t dwVID, const char* szText, const D3DXCOLOR& c_rColor);
#endif
		void DetachTitle(uint32_t dwVID);

		void AttachLevel(uint32_t dwVID, const char* c_szText, const D3DXCOLOR& c_rColor);
		void DetachLevel(uint32_t dwVID);


	protected:
		TTextTail * RegisterTextTail(uint32_t dwVirtualID, const char * c_szText, CGraphicObjectInstance * pOwner, float fHeight, const D3DXCOLOR & c_rColor);
		void DeleteTextTail(TTextTail * pTextTail);

		void UpdateTextTail(TTextTail * pTextTail);
		void RenderTextTailBox(TTextTail * pTextTail);
		void RenderTextTailName(TTextTail * pTextTail);
		void UpdateDistance(const TPixelPosition & c_rCenterPosition, TTextTail * pTextTail);

		bool isIn(TTextTail * pSource, TTextTail * pTarget);

	protected:
		TTextTailMap				m_CharacterTextTailMap;
		TTextTailMap				m_ItemTextTailMap;
		TChatTailMap				m_ChatTailMap;
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		TTextTailMap				m_ShopTextTailMap;
#endif

		TTextTailList				m_CharacterTextTailList;
		TTextTailList				m_ItemTextTailList;

	private:
	
	#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
		std::unordered_map<uint32_t, std::string> m_ItemOnTitlePrefixMap;
	#endif

		CDynamicPool<STextTail>		m_TextTailPool;
#ifdef ENABLE_FAKE_SHOP_HEADER
		TTextTail* GetTextTail(uint32_t dwVID);
#endif
};
