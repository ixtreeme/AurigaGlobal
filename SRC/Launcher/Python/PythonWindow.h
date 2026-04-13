#pragma once

#include <cstdint>
#include "../Base/Utils.h"
#include "../UserInterface/Locale_inc.h"
#include <cstdint>

namespace UI
{
	class CWindow
	{
		public:
			typedef std::list<CWindow *> TWindowContainer;

			static uint32_t Type();
			bool IsType(uint32_t dwType);

			enum EHorizontalAlign
			{
				HORIZONTAL_ALIGN_LEFT = 0,
				HORIZONTAL_ALIGN_CENTER = 1,
				HORIZONTAL_ALIGN_RIGHT = 2,
			};

			enum EVerticalAlign
			{
				VERTICAL_ALIGN_TOP = 0,
				VERTICAL_ALIGN_CENTER = 1,
				VERTICAL_ALIGN_BOTTOM = 2,
			};

			enum EFlags
			{
				FLAG_MOVABLE			= (1 <<  0),	// 움직일 수 있는 창
				FLAG_LIMIT				= (1 <<  1),	// 창이 화면을 벗어나지 않음
				FLAG_SNAP				= (1 <<  2),	// 스냅 될 수 있는 창
				FLAG_DRAGABLE			= (1 <<  3),
				FLAG_ATTACH				= (1 <<  4),	// 완전히 부모에 붙어 있는 창 (For Drag / ex. ScriptWindow)
				FLAG_RESTRICT_X			= (1 <<  5),	// 좌우 이동 제한
				FLAG_RESTRICT_Y			= (1 <<  6),	// 상하 이동 제한
				FLAG_NOT_CAPTURE		= (1 <<  7),
				FLAG_FLOAT				= (1 <<  8),	// 공중에 떠있어서 순서 재배치가 되는 창
				FLAG_NOT_PICK			= (1 <<  9),	// 마우스에 의해 Pick되지 않는 창
				FLAG_IGNORE_SIZE		= (1 << 10),
				FLAG_RTL				= (1 << 11),	// Right-to-left
			};

		public:
			CWindow(PyObject * ppyObject);
		//	CWindow();
			virtual ~CWindow();

			void			AddChild(CWindow * pWin);

			void			Clear();
			void			DestroyHandle();
			void			Update();
			void			Render();

			void			SetName(const char * c_szName);
			const char *	GetName()		{ return m_strName.c_str(); }
			void			SetSize(int32_t width, int32_t height);
			int32_t			GetWidth() const { return m_lWidth; }
			int32_t			GetHeight()	const	{ return m_lHeight; }

			void			SetHorizontalAlign(uint32_t dwAlign);
			void			SetVerticalAlign(uint32_t dwAlign);
			void			SetPosition(int32_t x, int32_t y);
			void			GetPosition(int32_t* plx, int32_t* ply);
			int32_t			GetPositionX() const		{ return m_x; }
			int32_t			GetPositionY() const		{ return m_y; }
			RECT &			GetRect()		{ return m_rect; }
			void			GetLocalPosition(int32_t& rlx, int32_t& rly);
			void			GetMouseLocalPosition(int32_t& rlx, int32_t& rly);
			int32_t			UpdateRect();

			RECT &			GetLimitBias()	{ return m_limitBiasRect; }
			void			SetLimitBias(int32_t l, int32_t r, int32_t t, int32_t b) { m_limitBiasRect.left = l, m_limitBiasRect.right = r, m_limitBiasRect.top = t, m_limitBiasRect.bottom = b; }

			void			Show();
			void			Hide();

			virtual	bool	IsShow();
			void			OnHideWithChilds();
			void			OnHide();

			bool			IsRendering();

			bool			HasParent()		{ return m_pParent ? true : false; }
			bool			HasChild()		{ return !(m_pChildList.empty()); }
			int				GetChildCount()	{ return (int)m_pChildList.size(); }

			CWindow *		GetRoot();
			CWindow *		GetParent();

			bool			IsChild(CWindow* pWin, bool bCheckRecursive = false);

			void			DeleteChild(CWindow * pWin);
			void			SetTop(CWindow * pWin);

			bool			IsIn(int32_t x, int32_t y);
			bool			IsIn();
			CWindow *		PickWindow(int32_t x, int32_t y);
			CWindow *		PickTopWindow(int32_t x, int32_t y);	// NOTE : Children으로 내려가지 않고 상위에서만
															//        체크 하는 특화된 함수

			void			__RemoveReserveChildren();

			void			AddFlag(uint32_t flag)		{ SET_BIT(m_dwFlag, flag);		}
			void			RemoveFlag(uint32_t flag)	{ REMOVE_BIT(m_dwFlag, flag);	}
			bool			IsFlag(uint32_t flag)		{ return (m_dwFlag & flag) ? true : false;	}
			/////////////////////////////////////
			///
			void			SetInsideRender(bool flag);
			void			GetRenderBox(RECT* box);
			void			UpdateTextLineRenderBox();
			void			UpdateRenderBox();
			void			UpdateRenderBoxRecursive();
			virtual void	OnAfterRender();
			virtual void	OnUpdateRenderBox() {}
			virtual void	OnRender();
			virtual void	OnUpdate();
			virtual void	OnChangePosition(){}

			virtual void	OnSetFocus();
			virtual void	OnKillFocus();

			virtual void	OnMouseDrag(int32_t lx, int32_t ly);
			virtual void	OnMouseOverIn();
			virtual void	OnMouseOverOut();
			virtual void	OnMouseOver();
			virtual void	OnDrop();
			virtual void	OnTop();
			virtual void	OnIMEUpdate();

			virtual void	OnMoveWindow(int32_t x, int32_t y);

			///////////////////////////////////////

			bool			RunIMETabEvent();
			bool			RunIMEReturnEvent();
			bool			RunIMEKeyDownEvent(int ikey);

			CWindow *		RunKeyDownEvent(int ikey);
			bool			RunKeyUpEvent(int ikey);
			bool			RunPressEscapeKeyEvent();
			bool			RunPressExitKeyEvent();

			virtual bool	OnIMETabEvent();
			virtual bool	OnIMEReturnEvent();
			virtual bool	OnIMEKeyDownEvent(int ikey);

			virtual bool	OnIMEChangeCodePage();
			virtual bool	OnIMEOpenCandidateListEvent();
			virtual bool	OnIMECloseCandidateListEvent();
			virtual bool	OnIMEOpenReadingWndEvent();
			virtual bool	OnIMECloseReadingWndEvent();

			virtual bool	OnMouseLeftButtonDown();
			virtual bool	OnMouseLeftButtonUp();
			virtual bool	OnMouseLeftButtonDoubleClick();
			virtual bool	OnMouseRightButtonDown();
			virtual bool	OnMouseRightButtonUp();
			virtual bool	OnMouseRightButtonDoubleClick();
			virtual bool	OnMouseMiddleButtonDown();
			virtual bool	OnMouseMiddleButtonUp();
			virtual bool	OnKeyDown(int ikey);
			virtual bool	OnKeyUp(int ikey);
			virtual bool	OnPressEscapeKey();
			virtual bool	OnPressExitKey();
			///////////////////////////////////////
			virtual bool	RunMouseWheelEvent(int32_t nLen);
			virtual bool	OnRunMouseWheelEvent(int32_t nLen);
			virtual void	SetColor(uint32_t dwColor){}
			virtual bool	OnIsType(uint32_t dwType);
			/////////////////////////////////////

			virtual bool	IsWindow() { return true; }
			/////////////////////////////////////

			virtual void	iSetRenderingRect(int iLeft, int iTop, int iRight, int iBottom);
			virtual void	SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
			virtual int		GetRenderingWidth();
			virtual int		GetRenderingHeight();
			void			ResetRenderingRect(bool bCallEvent = true);

		private:
			virtual void	OnSetRenderingRect();

		protected:
			std::string			m_strName;

			EHorizontalAlign	m_HorizontalAlign;
			EVerticalAlign		m_VerticalAlign;
			int32_t				m_x, m_y;			
			int					m_lWidth, m_lHeight;	
			RECT				m_rect;				
			RECT				m_limitBiasRect;	
			RECT				m_renderingRect;

			bool				m_bMovable;
			bool				m_bShow;

			uint32_t				m_dwFlag;

			PyObject *			m_poHandler;

			CWindow	*			m_pParent;
			TWindowContainer	m_pChildList;

			bool				m_isUpdatingChildren;
			TWindowContainer	m_pReserveChildList;

			bool				m_isInsideRender;
			RECT				m_renderBox;

#ifdef _DEBUG
		public:
			uint32_t				DEBUG_dwCounter;
#endif
	};

	class CLayer : public CWindow
	{
		public:
			CLayer(PyObject * ppyObject) : CWindow(ppyObject) {}
			virtual ~CLayer() {}

			bool IsWindow() { return FALSE; }
	};

	class CUiWikiRenderTarget : public CWindow
	{
		public:
			CUiWikiRenderTarget(PyObject* ppyObject);
			virtual ~CUiWikiRenderTarget();
		
		public:
			bool	SetWikiRenderTargetModule(int iRenderTargetModule);
			void	OnUpdateRenderBox();
		
		protected:
			void	OnRender();
		
		protected:
			int	m_dwIndex;
	};

	class CBox : public CWindow
	{
		public:
			CBox(PyObject * ppyObject);
			virtual ~CBox();

			void SetColor(uint32_t dwColor);

		protected:
			void OnRender();

		protected:
			uint32_t m_dwColor;
	};

	class CBar : public CWindow
	{
		public:
			CBar(PyObject * ppyObject);
			virtual ~CBar();

			void SetColor(uint32_t dwColor);

		protected:
			void OnRender();

		protected:
			uint32_t m_dwColor;
	};

	class CLine : public CWindow
	{
		public:
			CLine(PyObject * ppyObject);
			virtual ~CLine();

			void SetColor(uint32_t dwColor);

		protected:
			void OnRender();

		protected:
			uint32_t m_dwColor;
	};

	class CBar3D : public CWindow
	{
		public:
			static uint32_t Type();

		public:
			CBar3D(PyObject * ppyObject);
			virtual ~CBar3D();

			void SetColor(uint32_t dwLeft, uint32_t dwRight, uint32_t dwCenter);

		protected:
			void OnRender();

		protected:
			uint32_t m_dwLeftColor;
			uint32_t m_dwRightColor;
			uint32_t m_dwCenterColor;
	};

	// Text
	class CTextLine : public CWindow
	{
		public:
			static DWORD Type();
		
			CTextLine(PyObject * ppyObject);
			virtual ~CTextLine();

			void SetMax(int iMax);
			void SetHorizontalAlign(int iType);
			void SetVerticalAlign(int iType);
			void SetSecret(bool bFlag);
			void SetOutline(bool bFlag);
			void SetFeather(bool bFlag);
			void SetMultiLine(bool bFlag);
			void SetFontName(const char * c_szFontName);
			void SetFontColor(uint32_t dwColor);
			void SetLimitWidth(float fWidth);
			void SetFixedRenderPos(WORD startPos, WORD endPos) { m_TextInstance.SetFixedRenderPos(startPos, endPos); }
			void GetRenderPositions(WORD& startPos, WORD& endPos) { m_TextInstance.GetRenderPositions(startPos, endPos); }

			void ShowCursor();
			void HideCursor();
			bool IsShowCursor();
			int GetCursorPosition();

			void SetText(const char * c_szText);
			const char * GetText();

			void GetTextSize(int* pnWidth, int* pnHeight);

			bool IsShow();
			int GetRenderingWidth();
			int GetRenderingHeight();
			void OnSetRenderingRect();

		protected:
			void OnUpdate();
			void OnRender();
			void OnChangePosition();

			virtual void OnSetText(const char * c_szText);

			void OnUpdateRenderBox() {
				UpdateTextLineRenderBox();
				m_TextInstance.SetRenderBox(m_renderBox);
			}


		protected:
			CGraphicTextInstance m_TextInstance;
	};

	class CNumberLine : public CWindow
	{
		public:
			CNumberLine(PyObject * ppyObject);
			CNumberLine(CWindow * pParent);
			virtual ~CNumberLine();

			void SetPath(const char * c_szPath);
			void SetHorizontalAlign(int iType);
			void SetNumber(const char * c_szNumber);

		protected:
			void ClearNumber();
			void OnRender();
			void OnChangePosition();

		protected:
			std::string m_strPath;
			std::string m_strNumber;
			std::vector<CGraphicImageInstance *> m_ImageInstanceVector;

			int m_iHorizontalAlign;
			uint32_t m_dwWidthSummary;
	};

	// Image
	class CImageBox : public CWindow
	{
		public:
			CImageBox(PyObject * ppyObject);
			virtual ~CImageBox();

			void UnloadImage()
			{
				OnDestroyInstance();
				SetSize(GetWidth(), GetHeight());
				UpdateRect();
			}

			bool LoadImage(const char * c_szFileName);
			void SetDiffuseColor(float fr, float fg, float fb, float fa);
#ifdef NEW_PET_SYSTEM	
			void SetScale(float sx, float sy);
#endif
			int GetWidth();
			int GetHeight();

		protected:
			virtual void OnCreateInstance();
			virtual void OnDestroyInstance();

			virtual void OnUpdate();
			virtual void OnRender();
			void OnChangePosition();

		protected:
			CGraphicImageInstance * m_pImageInstance;
	};
	class CMarkBox : public CWindow
	{
		public:
			CMarkBox(PyObject * ppyObject);
			virtual ~CMarkBox();

			void LoadImage(const char * c_szFilename);
			void SetDiffuseColor(float fr, float fg, float fb, float fa);
			void SetIndex(UINT uIndex);
			void SetScale(FLOAT fScale);

		protected:
			virtual void OnCreateInstance();
			virtual void OnDestroyInstance();

			virtual void OnUpdate();
			virtual void OnRender();
			void OnChangePosition();
		protected:
			CGraphicMarkInstance * m_pMarkInstance;
	};
	class CExpandedImageBox : public CImageBox
	{
		public:
			static uint32_t Type();

		public:
			CExpandedImageBox(PyObject * ppyObject);
			virtual ~CExpandedImageBox();

			void SetScale(float fx, float fy);
			void SetOrigin(float fx, float fy);
			void SetRotation(float fRotation);
			void SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
			void SetRenderingMode(int iMode);
			int GetRenderingWidth();
			int GetRenderingHeight();
			void OnSetRenderingRect();
			void SetExpandedRenderingRect(float fLeftTop, float fLeftBottom, float fTopLeft, float fTopRight, float fRightTop, float fRightBottom, float fBottomLeft, float fBottomRight);
			void SetTextureRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
			uint32_t GetPixelColor(uint32_t x, uint32_t y);

//#ifdef ENABLE_IMAGE_CLIP_RECT
			void SetImageClipRect(float fLeft, float fTop, float fRight, float fBottom, bool bIsVertical = false);
//#endif

		protected:
			void OnCreateInstance();
			void OnDestroyInstance();
			void OnUpdateRenderBox();

			virtual void OnUpdate();
			virtual void OnRender();

			bool OnIsType(uint32_t dwType);
	};
	class CAniImageBox : public CWindow
	{
		public:
			static uint32_t Type();

		public:
			CAniImageBox(PyObject * ppyObject);
			virtual ~CAniImageBox();

			void SetDelay(int iDelay);
			void AppendImage(const char * c_szFileName, float xs = 0, float ys = 0
#ifdef ENABLE_ACCE_SYSTEM
			, float r = 1.0, float g = 1.0, float b = 1.0, float a = 1.0
#endif
			);
			void SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
			void SetRenderingMode(int iMode);
			void SetDiffuseColor(float r, float g, float b, float a);
#ifdef ENABLE_NEW_FISHING_SYSTEM
			void SetRotation(float r);
#endif
			void ResetFrame();

		protected:
			void OnUpdate();
			void OnRender();
			void OnChangePosition();
			virtual void OnEndFrame();

			bool OnIsType(uint32_t dwType);

		protected:
			uint8_t m_bycurDelay;
			uint8_t m_byDelay;
			uint8_t m_bycurIndex;
#ifdef ENABLE_NEW_FISHING_SYSTEM
			bool m_RotationProcess;
#endif
			std::vector<CGraphicExpandedImageInstance*> m_ImageVector;
	};

#ifdef ENABLE_NEW_FISHING_SYSTEM
	class CFishBox : public CWindow
	{
		public:
			static uint32_t Type();

		public:
			CFishBox(PyObject * ppyObject);
			virtual ~CFishBox();

			bool GetMove();
			void MoveStart();
			void MoveStop();
			void SetRandomPosition();
			void GetPosition(int * x, int * y);
			void RegisterAni(CAniImageBox* ani);

		protected:
			void OnUpdate();
			virtual void OnEndMove();

			bool OnIsType(uint32_t dwType);

			D3DXVECTOR2 m_v2SrcPos, m_v2DstPos, m_v2NextPos, m_v2Direction, m_v2NextDistance;
			float m_fDistance, m_fMoveSpeed;
			bool m_bIsMove;
			bool m_left, m_right;
			uint8_t m_lastRandom;
			float m_direction;
			CAniImageBox* m_pAniBox;
	};
#endif

	// Button
	class CButton : public CWindow
	{
		public:
			CButton(PyObject * ppyObject);
			virtual ~CButton();

			bool SetUpVisual(const char * c_szFileName);
			bool SetOverVisual(const char * c_szFileName);
			bool SetDownVisual(const char * c_szFileName);
			bool SetDisableVisual(const char * c_szFileName);

			const char * GetUpVisualFileName();
			const char * GetOverVisualFileName();
			const char * GetDownVisualFileName();

			void Flash();

			// #if app.ENABLE_SKILL_COLOR_SYSTEM
			void EnableFlash();
			void DisableFlash();

			void Enable();
			void Disable();

			void SetUp();
			void Up();
			void Over();
			void Down();

			bool IsDisable();
			bool IsPressed();

			void OnSetRenderingRect();

		protected:
			void OnUpdate();
			void OnRender();
			void OnChangePosition();

			bool OnMouseLeftButtonDown();
			bool OnMouseLeftButtonDoubleClick();
			bool OnMouseLeftButtonUp();
			void OnMouseOverIn();
			void OnMouseOverOut();

			bool IsEnable();

			void SetCurrentVisual(CGraphicExpandedImageInstance* pVisual);

		protected:
			bool m_bEnable;
			bool m_isPressed;
			bool m_isFlash;

			CGraphicExpandedImageInstance* m_pcurVisual;
			CGraphicExpandedImageInstance m_upVisual;
			CGraphicExpandedImageInstance m_overVisual;
			CGraphicExpandedImageInstance m_downVisual;
			CGraphicExpandedImageInstance m_disableVisual;

	};
	class CRadioButton : public CButton
	{
		public:
			CRadioButton(PyObject * ppyObject);
			virtual ~CRadioButton();

		protected:
			bool OnMouseLeftButtonDown();
			bool OnMouseLeftButtonUp();
			void OnMouseOverIn();
			void OnMouseOverOut();
	};
	class CToggleButton : public CButton
	{
		public:
			CToggleButton(PyObject * ppyObject);
			virtual ~CToggleButton();

		protected:
			bool OnMouseLeftButtonDown();
			bool OnMouseLeftButtonUp();
			void OnMouseOverIn();
			void OnMouseOverOut();
	};
	class CDragButton : public CButton
	{
		public:
			CDragButton(PyObject * ppyObject);
			virtual ~CDragButton();

			void SetRestrictMovementArea(int ix, int iy, int iwidth, int iheight);

		protected:
			void OnChangePosition();
			void OnMouseOverIn();
			void OnMouseOverOut();

		protected:
			RECT m_restrictArea;
	};
	
#ifdef ENABLE_UI_EXTRA
	class CMoveTextLine : public CTextLine
	{
	public:
		CMoveTextLine(PyObject * ppyObject);
		virtual ~CMoveTextLine();

	public:
		static uint32_t Type();

		void SetMoveSpeed(float fSpeed);
		void SetMovePosition(float fDstX, float fDstY);
		bool GetMove();
		void MoveStart();
		void MoveStop();

	protected:
		void OnUpdate();
		void OnRender();
		void OnEndMove();
		void OnChangePosition();

		bool OnIsType(uint32_t dwType);

		D3DXVECTOR2 m_v2SrcPos, m_v2DstPos, m_v2NextPos, m_v2Direction, m_v2NextDistance;
		float m_fDistance, m_fMoveSpeed;
		bool m_bIsMove;
	};
	class CMoveImageBox : public CImageBox
	{
		public:
			CMoveImageBox(PyObject * ppyObject);
			virtual ~CMoveImageBox();

			static uint32_t Type();

			void SetMoveSpeed(float fSpeed);
			void SetMovePosition(float fDstX, float fDstY);
			bool GetMove();
			void MoveStart();
			void MoveStop();

		protected:
			virtual void OnCreateInstance();
			virtual void OnDestroyInstance();

			virtual void OnUpdate();
			virtual void OnRender();
			virtual void OnEndMove();

			bool OnIsType(uint32_t dwType);

			D3DXVECTOR2 m_v2SrcPos, m_v2DstPos, m_v2NextPos, m_v2Direction, m_v2NextDistance;
			float m_fDistance, m_fMoveSpeed;
			bool m_bIsMove;
	};
	class CMoveScaleImageBox : public CMoveImageBox
	{
		public:
			CMoveScaleImageBox(PyObject * ppyObject);
			virtual ~CMoveScaleImageBox();

			static uint32_t Type();

			void SetMaxScale(float fMaxScale);
			void SetMaxScaleRate(float fMaxScaleRate);
			void SetScalePivotCenter(bool bScalePivotCenter);

		protected:
			virtual void OnCreateInstance();
			virtual void OnDestroyInstance();

			virtual void OnUpdate();

			bool OnIsType(uint32_t dwType);

			float m_fMaxScale, m_fMaxScaleRate, m_fScaleDistance, m_fAdditionalScale;
			D3DXVECTOR2 m_v2CurScale;
	};
#endif
};

extern bool g_bOutlineBoxEnable;
#ifdef ENABLE_NEW_FISHING_SYSTEM
#ifndef MAX_FISHING_WAYS
#define MAX_FISHING_WAYS 6
#endif
extern float listFishLeft[MAX_FISHING_WAYS][3];
extern float listFishRight[MAX_FISHING_WAYS][3];
#endif
