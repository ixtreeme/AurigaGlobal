#pragma once

#include "PythonWindow.h"
#include "../UserInterface/Locale_inc.h"

namespace UI
{
	enum
	{
		ITEM_WIDTH = 32,
		ITEM_HEIGHT = 32,

		SLOT_NUMBER_NONE = 0xffffffff,
	};

	enum ESlotStyle
	{
		SLOT_STYLE_NONE,
		SLOT_STYLE_PICK_UP,
		SLOT_STYLE_SELECT,
	};

	enum ESlotState
	{
		SLOT_STATE_LOCK		= (1 << 0),
		SLOT_STATE_CANT_USE	= (1 << 1),
		SLOT_STATE_DISABLE	= (1 << 2),
		SLOT_STATE_ALWAYS_RENDER_COVER = (1 << 3),			// 현재 Cover 버튼은 슬롯에 무언가 들어와 있을 때에만 렌더링 하는데, 이 flag가 있으면 빈 슬롯이어도 커버 렌더링
#ifdef WJ_ENABLE_TRADABLE_ICON
		SLOT_STATE_CANT_MOUSE_EVENT		= (1 << 4),
		SLOT_STATE_UNUSABLE				= (1 << 5),
#endif
	};

	class CSlotWindow : public CWindow
	{
		public:
			static DWORD Type();

		public:
			class CSlotButton;
			class CCoverButton;
			class CCoolTimeFinishEffect;

			friend class CSlotButton;
			friend class CCoverButton;

			typedef struct SSlot
			{
				uint32_t	dwState;
				uint32_t	dwSlotNumber;
				uint32_t	dwCenterSlotNumber;		// NOTE : 사이즈가 큰 아이템의 경우 아이템의 실제 위치 번호
				uint32_t	dwItemIndex;			// NOTE : 여기서 사용되는 Item이라는 단어는 좁은 개념의 것이 아닌,
				bool	isItem;					//        "슬롯의 내용물"이라는 포괄적인 개념어. 더 좋은 것이 있을까? - [levites]

				// CoolTime
				float	fCoolTime;
				float	fStartCoolTime;

				// Toggle
				bool	bActive;

				int		ixPosition;
				int		iyPosition;

				int		ixCellSize;
				int		iyCellSize;

				uint8_t	byxPlacedItemSize;
				uint8_t	byyPlacedItemSize;

				CGraphicImageInstance * pInstance;
				CNumberLine * pNumberLine;

				bool	bRenderBaseSlotImage;
				CCoverButton * pCoverButton;
				CSlotButton * pSlotButton;
				CImageBox * pSignImage;
				CAniImageBox * pFinishCoolTimeEffect;
				CAniImageBox * pHilightEffect[3];
			} TSlot;
			typedef std::list<TSlot> TSlotList;
			typedef TSlotList::iterator TSlotListIterator;

		public:
			CSlotWindow(PyObject * ppyObject);
			virtual ~CSlotWindow();

			void Destroy();

			// Manage Slot
			void SetSlotType(uint32_t dwType);
			void SetSlotStyle(uint32_t dwStyle);

			void AppendSlot(uint32_t dwIndex, int ixPosition, int iyPosition, int ixCellSize, int iyCellSize);
			void SetCoverButton(uint32_t dwIndex, const char * c_szUpImageName, const char * c_szOverImageName, const char * c_szDownImageName, const char * c_szDisableImageName, bool bLeftButtonEnable, bool bRightButtonEnable);
			void SetSlotBaseImage(const char * c_szFileName, float fr, float fg, float fb, float fa);
#ifdef NEW_PET_SYSTEM
			void SetSlotBaseImageScale(const char * c_szFileName, float fr, float fg, float fb, float fa, float sx, float sy);
#endif	
			void AppendSlotButton(const char * c_szUpImageName, const char * c_szOverImageName, const char * c_szDownImageName);
			void AppendRequirementSignImage(const char * c_szImageName);

			void EnableCoverButton(uint32_t dwIndex);
			void DisableCoverButton(uint32_t dwIndex);
			void SetAlwaysRenderCoverButton(uint32_t dwIndex, bool bAlwaysRender = false);

			void ShowSlotBaseImage(uint32_t dwIndex);
			void HideSlotBaseImage(uint32_t dwIndex);
			bool IsDisableCoverButton(uint32_t dwIndex);
			bool HasSlot(uint32_t dwIndex);

			void ClearAllSlot();
			void ClearSlot(uint32_t dwIndex);
			void SetSlot(uint32_t dwIndex, uint32_t dwVirtualNumber, uint8_t byWidth, uint8_t byHeight, CGraphicImage * pImage, D3DXCOLOR& diffuseColor);
#ifdef NEW_PET_SYSTEM
			void SetSlotScale(uint32_t dwIndex, uint32_t dwVirtualNumber, uint8_t byWidth, uint8_t byHeight, CGraphicImage * pImage, float sx, float sy, D3DXCOLOR& diffuseColor);
#endif
			void SetSlotCount(uint32_t dwIndex, uint32_t dwCount);
			void SetSlotCountNew(uint32_t dwIndex, uint32_t dwGrade, uint32_t dwCount);
			void SetSlotCoolTime(uint32_t dwIndex, float fCoolTime, float fElapsedTime = 0.0f);
			void ActivateSlot(uint32_t dwIndex);
			void DeactivateSlot(uint32_t dwIndex);
#ifdef ENABLE_HIGHLIGHT_SYSTEM
			bool IsActiveSlot(uint32_t dwIndex);
#endif
			void RefreshSlot();
			void SetSlotDiffuseColor(uint32_t index, D3DXCOLOR& diffusecolor);

			uint32_t GetSlotCount();

			void LockSlot(uint32_t dwIndex);
			void UnlockSlot(uint32_t dwIndex);
		//	bool IsLockSlot(uint32_t dwIndex);
			void SetCantUseSlot(uint32_t dwIndex);
			void SetUseSlot(uint32_t dwIndex);
		//	bool IsCantUseSlot(uint32_t dwIndex);
			void EnableSlot(uint32_t dwIndex);
			void DisableSlot(uint32_t dwIndex);
		//	bool IsEnableSlot(uint32_t dwIndex);
#ifdef WJ_ENABLE_TRADABLE_ICON
			void SetCanMouseEventSlot(uint32_t dwIndex);
			void SetCantMouseEventSlot(uint32_t dwIndex);
			void SetUsableSlotOnTopWnd(uint32_t dwIndex);
			void SetUnusableSlotOnTopWnd(uint32_t dwIndex);
#endif
			// Select
			void ClearSelected();
			void SelectSlot(uint32_t dwSelectingIndex);
			bool isSelectedSlot(uint32_t dwIndex);
			uint32_t GetSelectedSlotCount();
			uint32_t GetSelectedSlotNumber(uint32_t dwIndex);

			// Slot Button
			void ShowSlotButton(uint32_t dwSlotNumber);
			void HideAllSlotButton();
			void OnPressedSlotButton(uint32_t dwType, uint32_t dwSlotNumber, bool isLeft = TRUE);

			// Requirement Sign
			void ShowRequirementSign(uint32_t dwSlotNumber);
			void HideRequirementSign(uint32_t dwSlotNumber);

			// ToolTip
			bool OnOverInItem(uint32_t dwSlotNumber);
			bool OnOverInEmptySlot(uint32_t dwSlotNumber);
			void OnOverOutItem();
			void OnOverOutEmptySlot();

			// For Usable Item
			void SetUseMode(bool bFlag);
			void SetUsableItem(bool bFlag);

			// CallBack
			void ReserveDestroyCoolTimeFinishEffect(uint32_t dwSlotIndex);

		protected:
			void __Initialize();
			void __CreateToggleSlotImage();
			void __CreateFinishCoolTimeEffect(TSlot * pSlot);
			void __CreateBaseImage(const char * c_szFileName, float fr, float fg, float fb, float fa);

#ifdef NEW_PET_SYSTEM
			void __CreateBaseImageScale(const char * c_szFileName, float fr, float fg, float fb, float fa, float sx, float sy);
#endif
			void __DestroyToggleSlotImage();
			void __DestroyFinishCoolTimeEffect(TSlot * pSlot);
			void __DestroyBaseImage();

			// Event
			void OnUpdate();
			void OnRender();
			bool OnMouseLeftButtonDown();
			bool OnMouseLeftButtonUp();
			bool OnMouseRightButtonDown();
			bool OnMouseLeftButtonDoubleClick();
			void OnMouseOverOut();
			void OnMouseOver();
			void RenderSlotBaseImage();
			void RenderLockedSlot();
			virtual void OnRenderPickingSlot();
			virtual void OnRenderSelectedSlot();

			// Select
			void OnSelectEmptySlot(int iSlotNumber);
			void OnSelectItemSlot(int iSlotNumber);
			void OnUnselectEmptySlot(int iSlotNumber);
			void OnUnselectItemSlot(int iSlotNumber);
			void OnUseSlot();

			// Manage Slot
			bool GetSlotPointer(uint32_t dwIndex, TSlot ** ppSlot);
			bool GetSelectedSlotPointer(TSlot ** ppSlot);
			virtual bool GetPickedSlotPointer(TSlot ** ppSlot);
			void ClearSlot(TSlot * pSlot);
			virtual void OnRefreshSlot();

			// ETC
			bool OnIsType(uint32_t dwType);

		protected:
			uint32_t m_dwSlotType;
			uint32_t m_dwSlotStyle;
			std::list<uint32_t> m_dwSelectedSlotIndexList;
			TSlotList m_SlotList;
			uint32_t m_dwToolTipSlotNumber;

			bool m_isUseMode;
			bool m_isUsableItem;

			CGraphicImageInstance * m_pBaseImageInstance;
			CImageBox * m_pToggleSlotImage;
			std::deque<uint32_t> m_ReserveDestroyEffectDeque;
	};
};