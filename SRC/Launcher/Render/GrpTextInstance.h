#pragma once

#include "Pool.h"
#include "GrpText.h"
#ifdef __ENABLE_EMOJI_SYSTEM__
#include "GrpImageInstance.h"
#endif

class CGraphicTextInstance
{
	public:
		typedef CDynamicPool<CGraphicTextInstance> TPool;

	public:
		enum EHorizontalAlign
		{
			HORIZONTAL_ALIGN_LEFT		= 0x01,
			HORIZONTAL_ALIGN_CENTER		= 0x02,
			HORIZONTAL_ALIGN_RIGHT		= 0x03,
		};
		enum EVerticalAlign
		{
			VERTICAL_ALIGN_TOP		= 0x10,
			VERTICAL_ALIGN_CENTER	= 0x20,
			VERTICAL_ALIGN_BOTTOM	= 0x30
		};

	public:
		static void Hyperlink_UpdateMousePos(int x, int y);
		static int  Hyperlink_GetText(char* buf, int len);

	public:
		CGraphicTextInstance();
		virtual ~CGraphicTextInstance();

		void Destroy();

		void Update();
		void Render(RECT * pClipRect = nullptr);

		void SetFixedRenderPos(WORD startPos, WORD endPos) { m_startPos = startPos; m_endPos = endPos; m_isFixedRenderPos = true; }
		void GetRenderPositions(WORD& startPos, WORD& endPos) { startPos = m_startPos; endPos = m_endPos; }

		void ShowCursor();
		void HideCursor();
		bool IsShowCursor();
		void ShowOutLine();
		void HideOutLine();

		void SetColor(uint32_t color);
		void SetColor(float r, float g, float b, float a = 1.0f);
		uint32_t GetColor() const;

		void SetOutLineColor(uint32_t color);
		void SetOutLineColor(float r, float g, float b, float a = 1.0f);

		void SetHorizonalAlign(int hAlign);
		void SetVerticalAlign(int vAlign);
		void SetMax(int iMax);
		void SetTextPointer(CGraphicText* pText);
		void SetValueString(const std::string& c_stValue);
		void SetValue(const char* c_szValue, uint64_t len = -1

		);
		void SetPosition(float fx, float fy, float fz = 0.0f);
		float GetPositionX() const { return m_v3Position.x; }
		float GetPositionY() const { return m_v3Position.y; }

		void SetSecret(bool Value);
		void SetOutline(bool Value);
		void SetFeather(bool Value);
		void SetMultiLine(bool Value);
		void SetLimitWidth(float fWidth);

		void GetTextSize(int* pRetWidth, int* pRetHeight);
		const std::string& GetValueStringReference();
		WORD GetTextLineCount();

		int PixelPositionToCharacterPosition(int iPixelPosition);
		int GetHorizontalAlign();
		void SetRenderingRect(float fLeft, float fTop, float fRight, float fBottom);
		void iSetRenderingRect(int iLeft, int iTop, int iRight, int iBottom);
		
		void SetRenderBox(RECT& renderBox);


	protected:
		void __Initialize();
		int  __DrawCharacter(CGraphicFontTexture * pFontTexture, WORD codePage, wchar_t text, uint32_t dwColor);
		void __GetTextPos(uint32_t index, float* x, float* y);
		int __GetTextTag(const wchar_t * src, int maxLen, int & tagLen, std::wstring & extraInfo);

	protected:
		struct SHyperlink
		{
			short sx;
			short ex;
			std::wstring text;

			SHyperlink() : sx(0), ex(0) { }
		};
#ifdef __ENABLE_EMOJI_SYSTEM__
		struct SEmoji
		{
			short x;
			CGraphicImageInstance * pInstance;

			SEmoji() : x(0)
			{
				pInstance = nullptr;
			}
		};
#endif

	protected:
		uint32_t m_dwTextColor;
		uint32_t m_dwOutLineColor;

		WORD m_textWidth;
		WORD m_textHeight;

		uint8_t m_hAlign;
		uint8_t m_vAlign;
		WORD m_startPos, m_endPos;
		bool m_isFixedRenderPos;
		RECT m_renderBox;

		WORD m_iMax;
		float m_fLimitWidth;

		bool m_isCursor;
		bool m_isSecret;
		bool m_isMultiLine;

		bool m_isOutline;
		float m_fFontFeather;

		/////

		std::string m_stText;
		D3DXVECTOR3 m_v3Position;

	private:
		bool m_isUpdate;
		bool m_isUpdateFontTexture;

		CGraphicText::TRef m_roText;
		CGraphicFontTexture::TPCharacterInfomationVector m_pCharInfoVector;
		std::vector<uint32_t> m_dwColorInfoVector;
		std::vector<SHyperlink> m_hyperlinkVector;
#ifdef __ENABLE_EMOJI_SYSTEM__
		std::vector<SEmoji> m_emojiVector;
#endif

		bool m_bUseRenderingRect;
		RECT m_RenderingRect;


	public:
		static void CreateSystem(UINT uCapacity);
		static void DestroySystem();

		static CGraphicTextInstance* New();
		static void Delete(CGraphicTextInstance* pkInst);

		static CDynamicPool<CGraphicTextInstance>		ms_kPool;
};

extern const char* FindToken(const char* begin, const char* end);
extern int ReadToken(const char* token);
