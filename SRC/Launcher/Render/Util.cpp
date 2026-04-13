#include "StdAfx.h"
#include "../Pack/EterPackManager.h"

#include "TextFileLoader.h"
#ifdef LEADERBOARD_RAZOR93
#include <algorithm>
#include <vector>
#include "ResourceManager.h"
#include "GrpImage.h"
#include "GrpTexture.h"

static bool __TryReplace(const char* name, LPDIRECT3DTEXTURE9 newTex)
{
	//TraceError("[LB] Replace: try '%s'", name ? name : "(null)");
	if (!name || !newTex) return false;

	CResource* res = CResourceManager::Instance().GetResourcePointer(name);
//	TraceError("[LB]   lookup -> %p", res);
	if (!res) return false;

	CGraphicImage* img = static_cast<CGraphicImage*>(res);
	CGraphicTexture* tex = img->GetTexturePointer();
	//TraceError("[LB]   texptr -> %p", tex);
	if (!tex) return false;

	tex->AttachExternalTexture(newTex);
//	TraceError("[LB]   attached OK");
	return true;
}

// „Loose” keresés: több névváltozattal próbálkozik
bool ReplaceTextureGlobalByFilenameLoose(const char* base, LPDIRECT3DTEXTURE9 newTex)
{
	//TraceError("[LB] ReplaceLoose enter base='%s' tex=%p", base ? base : "(null)", newTex);
	if (!base || !newTex) { TraceError("[LB] ReplaceLoose: bad args"); return false; }

	std::vector<std::string> cand;

	// els?dlegesen a paraméter
	cand.emplace_back(base);

	// ha relatív, akkor próbáljuk a tipikus gyökerekkel
	cand.emplace_back("ymir work/razor93/" + std::string(base));
	cand.emplace_back("ymir work\\razor93\\" + std::string(base));
	cand.emplace_back("d:/ymir work/razor93/" + std::string(base));
	cand.emplace_back("d:\\ymir work\\razor93\\" + std::string(base));

	for (auto& n : cand) {
		if (__TryReplace(n.c_str(), newTex))
			return true;
	}

	TraceError("[LB] ReplaceLoose: ALL CANDIDATES FAILED for base='%s'", base);
	return false;
}

#endif

void PrintfTabs(FILE * File, int iTabCount, const char * c_szString, ...)
{
	va_list args;
	va_start(args, c_szString);

	static char szBuf[1024];
	_vsnprintf(szBuf, sizeof(szBuf), c_szString, args);
	va_end(args);

	for (int i = 0; i < iTabCount; ++i)
		fprintf(File, "    ");

	fprintf(File, szBuf);
}

bool LoadTextData(const char * c_szFileName, CTokenMap & rstTokenMap)
{
	const void* pMotionData;
	CMappedFile File;

	if (!CEterPackManager::Instance().Get(File, c_szFileName, &pMotionData))
		return false;

	CMemoryTextFileLoader textFileLoader;
	CTokenVector stTokenVector;

	textFileLoader.Bind(File.Size(), pMotionData);

	for (uint32_t i = 0; i < textFileLoader.GetLineCount(); ++i)
	{
		if (!textFileLoader.SplitLine(i, &stTokenVector))
			continue;

		if (2 != stTokenVector.size())
			return false;

		stl_lowers(stTokenVector[0]);
		stl_lowers(stTokenVector[1]);

		rstTokenMap[stTokenVector[0]] = stTokenVector[1];
	}

	return true;
}

bool LoadMultipleTextData(const char * c_szFileName, CTokenVectorMap & rstTokenVectorMap)
{
	const void* pModelData;
	CMappedFile File;

	if (!CEterPackManager::Instance().Get(File, c_szFileName, &pModelData))
		return false;

	uint32_t i;

	CMemoryTextFileLoader textFileLoader;
	CTokenVector stTokenVector;

	textFileLoader.Bind(File.Size(), pModelData);

	for (i = 0; i < textFileLoader.GetLineCount(); ++i)
	{
		if (!textFileLoader.SplitLine(i, &stTokenVector))
			continue;

		stl_lowers(stTokenVector[0]);

		// Start or End
		if (0 == stTokenVector[0].compare("start"))
		{
			CTokenVector stSubTokenVector;

			stl_lowers(stTokenVector[1]);
			std::string key = stTokenVector[1];
			stTokenVector.clear();

			for (i=i+1; i < textFileLoader.GetLineCount(); ++i)
			{
				if (!textFileLoader.SplitLine(i, &stSubTokenVector))
					continue;

				stl_lowers(stSubTokenVector[0]);

				if (0 == stSubTokenVector[0].compare("end"))
				{
					break;
				}

				for (uint32_t j = 0; j < stSubTokenVector.size(); ++j)
				{
					stTokenVector.push_back(stSubTokenVector[j]);
				}
			}

			rstTokenVectorMap.insert(CTokenVectorMap::value_type(key, stTokenVector));
		}
		else
		{
			std::string key = stTokenVector[0];
			stTokenVector.erase(stTokenVector.begin());
			rstTokenVectorMap.insert(CTokenVectorMap::value_type(key, stTokenVector));
		}
	}

	return true;
}

D3DXVECTOR3 TokenToVector(CTokenVector & rVector)
{
	if (3 != rVector.size())
	{
		assert(!"Size of token vector which will be converted to vector is not 3");
		return D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	}

	return D3DXVECTOR3(std::stof(rVector[0]),
						std::stof(rVector[1]),
						std::stof(rVector[2]));
}

D3DXCOLOR TokenToColor(CTokenVector & rVector)
{
	if (4 != rVector.size())
	{
		assert(!"Size of token vector which will be converted to color is not 4");
		return D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	}

	return D3DXCOLOR(std::stof(rVector[0]),
						std::stof(rVector[1]),
						std::stof(rVector[2]),
						std::stof(rVector[3]));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static std::string	gs_fontFace="";
static uint32_t		gs_codePage=0;

int CALLBACK EnumFontFamExProc(CONST LOGFONT* plogFont, CONST TEXTMETRIC* /*textMetric*/, uint32_t /*dwWord*/, LPARAM lParam)
{
	return stricmp((const char*)lParam, plogFont->lfFaceName);
}

uint32_t GetDefaultCodePage()
{
	return gs_codePage;
}

const char * GetDefaultFontFace()
{
	return gs_fontFace.c_str();
}

const char*	GetFontFaceFromCodePage(WORD codePage)
{
	LOGFONT logFont = {};

	logFont.lfCharSet = CP_UTF8;

	const char* fontFace = "Arial";

	HDC hDC=GetDC(nullptr);

	if(EnumFontFamiliesEx(hDC, &logFont, (FONTENUMPROC)EnumFontFamExProc, (LPARAM)fontFace, 0) == 0)
	{
		ReleaseDC(nullptr, hDC);
		return fontFace;
	}

	//fontFace = GetFontFaceFromCodePageNT(codePage);

	if(EnumFontFamiliesEx(hDC, &logFont, (FONTENUMPROC)EnumFontFamExProc, (LPARAM)fontFace, 0) == 0)
	{
		ReleaseDC(nullptr, hDC);
		return fontFace;
	}

	ReleaseDC(nullptr, hDC);

	return GetDefaultFontFace();
}

void SetDefaultFontFace(const char* fontFace)
{
	gs_fontFace=fontFace;
}

bool SetDefaultCodePage(uint32_t codePage)
{
	gs_codePage=codePage;

	std::string fontFace=GetFontFaceFromCodePage(codePage);
	if (fontFace.empty())
		return false;

	SetDefaultFontFace(fontFace.c_str());

	return true;
}


int __base64_get( const int c )
{
	if( 'A' <= c && c <= 'Z' )
		return c-'A';
	if( 'a' <= c && c <= 'z' )
		return c - 'a' + 26;
	if( '0' <= c && c <= '9' )
		return c - '0' + 52;
	if( c == '+' )
		return 62;
	if( c == '/' )
		return 63;
	if( c == '=' )	// end of line
		return -1;
	return -2;	// non value;
}

void __strcat1(char * str,int i)
{
	char result[2];
	result[0] = i;
	result[1] = NULL;
	strcat(str,result);
}

void base64_decode(const char * str,char * resultStr)
{
	int nCount=0, i=0, r, result;
	int length = strlen(str);
	char szDest[5]="";

	strcpy(resultStr,"");
	while(nCount < length)
	{
		i=0;
		strcpy(szDest, "");
		while(nCount<length && i<4)	// 4°³ÀÇ ¹ÙÀÌÆ®¸¦ ¾ò´Â´Ù.
		{
			r = str[nCount++];
			result = __base64_get(r);
			if(result!=-2)
			{
				if(result!=-1)
					szDest[i++] = result;
				else szDest[i++] = '@';	// It's end  (64¹øÀº µðÄÚµù½Ã »ç¿ëµÇÁö ¾Ê±â ¶§¹®)
			}
		}

		if(i==4)	// 4°³ÀÇ ¼Ò½º¸¦ ¸ðµÎ ¾ò¾î³Â´Ù. µðÄÚµå ½ÃÀÛ
		{
			if( nCount+3 >= length )	// µ¥ÀÌÅÍÀÇ ³¡¿¡ µµ´ÞÇß´Ù.
			{
				if( szDest[1] == '@' )
				{
					__strcat1(resultStr,(szDest[0]<<2));
					break;
				}// exit while loop
				else
					__strcat1(resultStr,(szDest[0]<<2 | szDest[1]>>4));	// 1 Byte
				if( szDest[2] == '@' )
				{
					__strcat1(resultStr,(szDest[1]<<4));
					break;
				}
				else
					__strcat1(resultStr,(szDest[1]<<4 | szDest[2]>>2));	// 2 Byte
				if( szDest[3] == '@' )
				{
					__strcat1(resultStr,(szDest[2]<<6));
					break;
				}
				else
					__strcat1(resultStr,(szDest[2]<<6 | szDest[3]));	// 3 Byte
			}
			else
			{
				__strcat1(resultStr,(szDest[0]<<2 | szDest[1]>>4));	// 1 Byte
				__strcat1(resultStr,(szDest[1]<<4 | szDest[2]>>2));	// 2 Byte
				__strcat1(resultStr,(szDest[2]<<6 | szDest[3]));	// 3 Byte
			}
		}

	}// end of while

	for (i = 0; i < strlen(resultStr); i++)
	{
		char c = resultStr[i];
		int xx = i + 5;
		resultStr[i] = char(c ^ xx);
	}
	// E
}
