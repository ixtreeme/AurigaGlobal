#pragma once


#include "../Base/FileLoader.h"

#include <map>
#include <vector>

#include <directxsdk/d3dx9.h>

template<typename T>
class CTransitor
{
	public:
		CTransitor() {}
		~CTransitor() {}

		void SetActive(bool bActive = true)
		{
			m_bActivated = bActive;
		}

		bool isActive()
		{
			return m_bActivated;
		}

		bool isActiveTime(float fcurTime)
		{
			if (fcurTime >= m_fEndTime)
				return FALSE;

			return TRUE;
		}

		uint32_t GetID()
		{
			return m_dwID;
		}

		void SetID(uint32_t dwID)
		{
			m_dwID = dwID;
		}

		void SetSourceValue(const T & c_rSourceValue)
		{
			m_SourceValue = c_rSourceValue;
		}

		void SetTransition(const T & c_rSourceValue, const T & c_rTargetValue, float fStartTime, float fBlendTime)
		{
			m_SourceValue = c_rSourceValue;
			m_TargetValue = c_rTargetValue;
			m_fStartTime = fStartTime;
			m_fEndTime = fStartTime + fBlendTime;
		}

		bool GetValue(float fcurTime, T * pValue)
		{
			if (fcurTime <= m_fStartTime)
				return FALSE;

			float fPercentage = (fcurTime - m_fStartTime) / (m_fEndTime - m_fStartTime);
			*pValue = m_SourceValue + (m_TargetValue - m_SourceValue) * fPercentage;
			return TRUE;
		}

	protected:
		uint32_t	m_dwID;			// Public Transitor ID

		bool	m_bActivated;	// Have been started to blend?
		float	m_fStartTime;
		float	m_fEndTime;

		T		m_SourceValue;
		T		m_TargetValue;
};

typedef CTransitor<float>			TTransitorFloat;
typedef CTransitor<D3DXVECTOR3>		TTransitorVector3;
typedef CTransitor<D3DXCOLOR>		TTransitorColor;
#ifdef LEADERBOARD_RAZOR93
bool ReplaceTextureGlobalByFilenameLoose(const char* base, LPDIRECT3DTEXTURE9 newTex);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

void PrintfTabs(FILE * File, int iTabCount, const char * c_szString, ...);


//typedef CTokenVector TTokenVector;

extern bool	LoadTextData(const char * c_szFileName, CTokenMap & rstTokenMap);
extern bool	LoadMultipleTextData(const char * c_szFileName, CTokenVectorMap & rstTokenVectorMap);

extern D3DXVECTOR3 TokenToVector(CTokenVector & rVector);
extern D3DXCOLOR TokenToColor(CTokenVector & rVector);

#define GOTO_CHILD_NODE(TextFileLoader, Index) CTextFileLoader::CGotoChild Child(TextFileLoader, Index);

///////////////////////////////////////////////////////////////////////////////////////////////////

extern int CALLBACK EnumFontFamExProc(CONST LOGFONT* plogFont, CONST TEXTMETRIC* textMetric, uint32_t dwWord, LPARAM lParam);
//extern int GetCharsetFromCodePage(WORD codePage);
//extern const char* GetFontFaceFromCodePageNT(WORD codePage);
//extern const char* GetFontFaceFromCodePage9x(WORD codePage);
extern uint32_t GetDefaultCodePage();
extern const char * GetDefaultFontFace();
extern const char*	GetFontFaceFromCodePage(WORD codePage);
extern void SetDefaultFontFace(const char* fontFace);
extern bool SetDefaultCodePage(uint32_t codePage);
extern void base64_decode(const char * str,char * resultStr);

extern uint32_t GetMaxTextureWidth();
extern uint32_t GetMaxTextureHeight();