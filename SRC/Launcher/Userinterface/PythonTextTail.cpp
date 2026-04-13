//
//
#include "stdafx.h"
#include "InstanceBase.h"
#include "resource.h"
#include "PythonTextTail.h"
#include "PythonCharacterManager.h"
#include "PythonGuild.h"
#include "Locale.h"
#include "MarkManager.h"
#ifdef ENABLE_NEW_SHOP_IN_CITIES
#include "PythonApplication.h"
#endif
#include "PythonSystem.h"


#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
namespace
{
	static inline bool __StartsWith(const std::string& s, const char* pfx)
	{
		const size_t n = strlen(pfx);
		return s.size() >= n && 0 == s.compare(0, n, pfx);
	}

	static std::string __ComposeNameWithItemOnTitle(const std::string& baseName, const std::string& itemPrefix)
	{
		if (itemPrefix.empty())
			return baseName;

		// Put the item-title after rank tags (e.g. [GM]) to avoid "[GM]" duplications
		static const char* kRankTags[] = { "[GM]", "[SA]", "[GA]", "[SGA]", "[ADMIN]" };
		for (const char* tag : kRankTags)
		{
			if (__StartsWith(baseName, tag))
				return std::string(tag) + itemPrefix + baseName.substr(strlen(tag));
		}

		return itemPrefix + baseName;
	}
}
#endif

const D3DXCOLOR c_TextTail_Player_Color = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
const D3DXCOLOR c_TextTail_Monster_Color = D3DXCOLOR(1.0f, 0.0f, 0.0f, 1.0f);
const D3DXCOLOR c_TextTail_Item_Color = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
const D3DXCOLOR c_TextTail_Chat_Color = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
const D3DXCOLOR c_TextTail_Info_Color = D3DXCOLOR(1.0f, 0.785f, 0.785f, 1.0f);
const D3DXCOLOR c_TextTail_Guild_Name_Color = 0xFFEFD3FF;
const float c_TextTail_Name_Position = -10.0f;
const float c_fxMarkPosition = 1.5f;
const float c_fyGuildNamePosition = 15.0f;
const float c_fyMarkPosition = 15.0f + 11.0f;
bool bPKTitleEnable = true;

// TEXTTAIL_LIVINGTIME_CONTROL
long gs_TextTail_LivingTime = 5000;

long TextTail_GetLivingTime()
{
	assert(gs_TextTail_LivingTime > 1000);
	return gs_TextTail_LivingTime;
}

void TextTail_SetLivingTime(long livingTime)
{
	gs_TextTail_LivingTime = livingTime;
}
// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

CGraphicText* ms_pFont = nullptr;

void CPythonTextTail::GetInfo(std::string* pstInfo)
{
	char szInfo[256];
	sprintf(szInfo, "TextTail: ChatTail %zd, ChrTail (Map %zd, List %zd), ItemTail (Map %zd, List %zd), Pool %zd",
		m_ChatTailMap.size(),
		m_CharacterTextTailMap.size(), m_CharacterTextTailList.size(),
		m_ItemTextTailMap.size(), m_ItemTextTailList.size(),
		m_TextTailPool.GetCapacity());

	pstInfo->append(szInfo);
}

void CPythonTextTail::UpdateAllTextTail()
{
	CInstanceBase* pInstance = CPythonCharacterManager::Instance().GetMainInstancePtr();
	if (pInstance)
	{
		TPixelPosition pixelPos;
		pInstance->NEW_GetPixelPosition(&pixelPos);

		TTextTailMap::iterator itorMap;

		for (itorMap = m_CharacterTextTailMap.begin(); itorMap != m_CharacterTextTailMap.end(); ++itorMap)
		{
			UpdateDistance(pixelPos, itorMap->second);
		}

		for (itorMap = m_ItemTextTailMap.begin(); itorMap != m_ItemTextTailMap.end(); ++itorMap)
		{
			UpdateDistance(pixelPos, itorMap->second);
		}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		for (itorMap = m_ShopTextTailMap.begin(); itorMap != m_ShopTextTailMap.end(); ++itorMap)
			UpdateDistance(pixelPos, itorMap->second);
#endif

		for (TChatTailMap::iterator itorChat = m_ChatTailMap.begin(); itorChat != m_ChatTailMap.end(); ++itorChat)
		{
			UpdateDistance(pixelPos, itorChat->second);

			// NOTE : Chat TextTailÀÌ ÀÖÀ¸¸é Ä³¸¯�
			if (itorChat->second->bNameFlag)
			{
				uint32_t dwVID = itorChat->first;
				ShowCharacterTextTail(dwVID);
			}
		}
	}
}




void CPythonTextTail::UpdateShowingTextTail()
{
	TTextTailList::iterator itor;

	for (itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		UpdateTextTail(*itor);
	}

	for (TChatTailMap::iterator itorChat = m_ChatTailMap.begin(); itorChat != m_ChatTailMap.end(); ++itorChat)
	{
		UpdateTextTail(itorChat->second);
	}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	for (auto itorMap = m_ShopTextTailMap.begin(); itorMap != m_ShopTextTailMap.end(); ++itorMap)
		if (itorMap->second->bRender)
			UpdateTextTail(itorMap->second);
#endif

	for (itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail* pTextTail = *itor;
		UpdateTextTail(pTextTail);

		// NOTE : Chat TextTailÀÌ ÀÖÀ» °æ¿ì À§Ä¡¸¦ ¹Ù²Û´Ù.
		TChatTailMap::iterator itor = m_ChatTailMap.find(pTextTail->dwVirtualID);
		if (m_ChatTailMap.end() != itor)
		{
			TTextTail* pChatTail = itor->second;
			if (pChatTail->bNameFlag)
			{
				pTextTail->y = pChatTail->y - 17.0f;
			}
		}
	}
}

void CPythonTextTail::UpdateTextTail(TTextTail* pTextTail)
{
	if (!pTextTail->pOwner)
		return;

	/////

	CPythonGraphic& rpyGraphic = CPythonGraphic::Instance();
	rpyGraphic.Identity();

	const D3DXVECTOR3& c_rv3Position = pTextTail->pOwner->GetPosition();
	rpyGraphic.ProjectPosition(c_rv3Position.x,
		c_rv3Position.y,
		c_rv3Position.z + pTextTail->fHeight,
		&pTextTail->x,
		&pTextTail->y,
		&pTextTail->z);

	pTextTail->x = floorf(pTextTail->x);
	pTextTail->y = floorf(pTextTail->y);

	// NOTE : 13m ¹Û¿¡ ÀÖÀ»¶§¸¸ ±íÀÌ¸¦ ³Ö½À´Ï´Ù - [levites]
	if (pTextTail->fDistanceFromPlayer < 1300.0f)
	{
		pTextTail->z = 0.0f;
	}
	else
	{
		pTextTail->z = pTextTail->z * CPythonGraphic::Instance().GetOrthoDepth() * -1.0f;
		pTextTail->z += 10.0f;
	}
}

void CPythonTextTail::ArrangeTextTail()
{
	TTextTailList::iterator itor;
	TTextTailList::iterator itorCompare;

	uint32_t dwTime = CTimer::Instance().GetCurrentMillisecond();

	for (itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		TTextTail* pInsertTextTail = *itor;

		int yTemp = 5;
		int LimitCount = 0;

		for (itorCompare = m_ItemTextTailList.begin(); itorCompare != m_ItemTextTailList.end();)
		{
			TTextTail* pCompareTextTail = *itorCompare;

			if (*itorCompare == *itor)
			{
				++itorCompare;
				continue;
			}

			if (LimitCount >= 20)
				break;

			if (isIn(pInsertTextTail, pCompareTextTail))
			{
				pInsertTextTail->y = (pCompareTextTail->y + pCompareTextTail->yEnd + yTemp);

				itorCompare = m_ItemTextTailList.begin();
				++LimitCount;
				continue;
			}

			++itorCompare;
		}


		if (pInsertTextTail->pOwnerTextInstance)
		{
			pInsertTextTail->pOwnerTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pOwnerTextInstance->Update();

			pInsertTextTail->pTextInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + 15.0f, pInsertTextTail->z);
			pInsertTextTail->pTextInstance->Update();

		}
		else
		{
			pInsertTextTail->pTextInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pTextInstance->Update();

		}
#ifdef ENABLE_FAKE_SHOP_HEADER
	#ifdef ENABLE_FAKE_SHOP_HEADER
		if (!pInsertTextTail->pMountCountInstance)
		{
			//TraceError(">>> [ArrangeTextTail] pMountCountInstance is NULL for ptr = %p\n", pInsertTextTail);

			continue; // Vagy return, vagy skip, hogy ne legyen crash
		}
	#endif
		if (pInsertTextTail->pMountCountInstance)
		{
			pInsertTextTail->pMountCountInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);

			if (pInsertTextTail->pOwnerTextInstance)
				pInsertTextTail->pMountCountInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + 15.0f, pInsertTextTail->z);
			else
				pInsertTextTail->pMountCountInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);

			pInsertTextTail->pMountCountInstance->Update();

		}
		else
		{
			//pInsertTextTail->pMountCountInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			//pInsertTextTail->pMountCountInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + 15.0f, pInsertTextTail->z);
			//pInsertTextTail->pMountCountInstance->Update();


			//TraceError(">>> [ArrangeTextTail] pMountCountInstance is NULL\n");
		}

		if (pInsertTextTail->pMountCountIconInstance)
		{
			float offsetY = 25.0f;
			pInsertTextTail->pMountCountIconInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + offsetY);
		}
#endif

	}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	for (auto itorMap = m_ShopTextTailMap.begin(); itorMap != m_ShopTextTailMap.end(); ++itorMap)
	{
		if (!itorMap->second->bRender)
			continue;

		TTextTail* pInsertTextTail = itorMap->second;

		if (pInsertTextTail->pOwnerTextInstance)
		{
			pInsertTextTail->pOwnerTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pOwnerTextInstance->Update();

			pInsertTextTail->pTextInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + 15.0f, pInsertTextTail->z);
			pInsertTextTail->pTextInstance->Update();
#ifdef ENABLE_FAKE_SHOP_HEADER//x

			pInsertTextTail->pMountCountInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pMountCountInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y + 15.0f, pInsertTextTail->z);
			pInsertTextTail->pMountCountInstance->Update();

#endif

		}
		else
		{
			pInsertTextTail->pTextInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pTextInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pTextInstance->Update();
#ifdef ENABLE_FAKE_SHOP_HEADERd//x
			pInsertTextTail->pMountCountInstance->SetColor(pInsertTextTail->Color.r, pInsertTextTail->Color.g, pInsertTextTail->Color.b);
			pInsertTextTail->pMountCountInstance->SetPosition(pInsertTextTail->x, pInsertTextTail->y, pInsertTextTail->z);
			pInsertTextTail->pMountCountInstance->Update();
#endif
		}

	}
#endif

	for (itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail* pTextTail = *itor;

		float fxAdd = 0.0f;

		// Mark À§Ä¡ ¾÷µ¥ÀÌÆ®
		CGraphicMarkInstance* pMarkInstance = pTextTail->pMarkInstance;
		CGraphicTextInstance* pGuildNameInstance = pTextTail->pGuildNameTextInstance;
		if (pMarkInstance && pGuildNameInstance)
		{
			int iWidth, iHeight;
			int iImageHalfSize = pMarkInstance->GetWidth() / 2 + c_fxMarkPosition;
			pGuildNameInstance->GetTextSize(&iWidth, &iHeight);

			pMarkInstance->SetPosition(pTextTail->x - iWidth / 2 - iImageHalfSize, pTextTail->y - c_fyMarkPosition);
			pGuildNameInstance->SetPosition(pTextTail->x + iImageHalfSize, pTextTail->y - c_fyGuildNamePosition, pTextTail->z);
			pGuildNameInstance->Update();
		}

		int iNameWidth, iNameHeight;
		pTextTail->pTextInstance->GetTextSize(&iNameWidth, &iNameHeight);

#ifdef ENABLE_MULTI_LANGUAGE
		CGraphicImageInstance* pLanguageInstance = pTextTail->pLanguageInstance;
#endif
#ifdef ENABLE_FAKE_SHOP_HEADER
		if (pTextTail->pMountCountInstance && pTextTail->pMountCountIconInstance)
		{
			int nameWidth, nameHeight;
			pTextTail->pTextInstance->GetTextSize(&nameWidth, &nameHeight);

			// Név magassága alapján menj feljebb
			float offsetY = -(float)nameHeight - 25.0f;

			pTextTail->pMountCountInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
			pTextTail->pMountCountInstance->SetPosition(pTextTail->x, pTextTail->y + offsetY, pTextTail->z);
			pTextTail->pMountCountInstance->Update();

			int countWidth, countHeight;
			pTextTail->pMountCountInstance->GetTextSize(&countWidth, &countHeight);
			float textRight = pTextTail->x + (countWidth / 2.0f);

			pTextTail->pMountCountIconInstance->SetPosition(
				textRight + 2.0f,
				pTextTail->y + offsetY - 2.0f
			);
		}
#endif

		


//#ifdef ENABLE_FAKE_SHOP_HEADER
//		if (pTextTail->pMountCountInstance)
//		{
//			pTextTail->pMountCountInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
//
//			const float offsetY = 12.0f; // 
//
//			pTextTail->pMountCountInstance->SetPosition(
//				pTextTail->x,             // középpont
//				pTextTail->y + offsetY,   // függőleges eltolás
//				pTextTail->z
//			);
//			pTextTail->pMountCountInstance->Update();
//		}
//#endif


		CGraphicTextInstance* pTitle = pTextTail->pTitleTextInstance;
		if (pTitle)
		{
			int iTitleWidth, iTitleHeight;
			pTitle->GetTextSize(&iTitleWidth, &iTitleHeight);

			fxAdd = 8.0f;

			if (LocaleService_IsEUROPE())
			{
				pTitle->SetPosition(pTextTail->x - (iNameWidth / 2), pTextTail->y, pTextTail->z);
			}
			else
			{
				pTitle->SetPosition(pTextTail->x - (iNameWidth / 2) - fxAdd, pTextTail->y, pTextTail->z);
			}
			pTitle->Update();

			CGraphicTextInstance* pLevel = pTextTail->pLevelTextInstance;
			if (pLevel)
			{
				int iLevelWidth, iLevelHeight;
				pLevel->GetTextSize(&iLevelWidth, &iLevelHeight);

				if (LocaleService_IsEUROPE()) // µ¶ÀÏ¾î´Â ¸íÄªÀÌ ±æ¾î ¿À¸¥Á¤·Ä
				{
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - iTitleWidth, pTextTail->y, pTextTail->z);
				}
				else
				{
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - fxAdd - iTitleWidth, pTextTail->y, pTextTail->z);
				}

				pLevel->Update();

#ifdef ENABLE_MULTI_LANGUAGE
				if (pLanguageInstance)
				{
					int iLevelWidth, iLevelHeight;
					pLevel->GetTextSize(&iLevelWidth, &iLevelHeight);
					pLanguageInstance->SetPosition(pTextTail->x - (iNameWidth / 2) - iTitleWidth - iLevelWidth - pLanguageInstance->GetWidth() - 12.0f, pTextTail->y - 10.0f);
				}
#endif	
			}
		}
		else
		{
			fxAdd = 4.0f;

			CGraphicTextInstance* pLevel = pTextTail->pLevelTextInstance;
			if (pLevel)
			{
				int iLevelWidth, iLevelHeight;
				pLevel->GetTextSize(&iLevelWidth, &iLevelHeight);

				if (LocaleService_IsEUROPE())
				{
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2), pTextTail->y, pTextTail->z);
				}
				else
				{
					pLevel->SetPosition(pTextTail->x - (iNameWidth / 2) - fxAdd, pTextTail->y, pTextTail->z);
				}

				pLevel->Update();

#ifdef ENABLE_MULTI_LANGUAGE
				if (pLanguageInstance)
					pLanguageInstance->SetPosition(pTextTail->x - (iNameWidth / 2) - iLevelWidth - pLanguageInstance->GetWidth() - 8.0f, pTextTail->y - 10.0f);
#endif	
			}
		}

		pTextTail->pTextInstance->SetColor(pTextTail->Color.r, pTextTail->Color.g, pTextTail->Color.b);
		pTextTail->pTextInstance->SetPosition(pTextTail->x + fxAdd, pTextTail->y, pTextTail->z);
		pTextTail->pTextInstance->Update();

	}

	for (auto itorChat = m_ChatTailMap.begin(); itorChat != m_ChatTailMap.end();)
	{
		TTextTail* pTextTail = itorChat->second;

		if (pTextTail->LivingTime < dwTime)
		{
			DeleteTextTail(pTextTail);
			itorChat = m_ChatTailMap.erase(itorChat);
			continue;
		}
		else
			++itorChat;

		pTextTail->pTextInstance->SetColor(pTextTail->Color);
		pTextTail->pTextInstance->SetPosition(pTextTail->x, pTextTail->y, pTextTail->z);
		pTextTail->pTextInstance->Update();
	}
}

void CPythonTextTail::Render()
{
	TTextTailList::iterator itor;

	for (itor = m_CharacterTextTailList.begin(); itor != m_CharacterTextTailList.end(); ++itor)
	{
		TTextTail* pTextTail = *itor;
		pTextTail->pTextInstance->Render();
		if (pTextTail->pMarkInstance && pTextTail->pGuildNameTextInstance)
		{
			pTextTail->pMarkInstance->Render();
			pTextTail->pGuildNameTextInstance->Render();
		}
		if (pTextTail->pTitleTextInstance)
		{
			pTextTail->pTitleTextInstance->Render();
		}
#ifdef ENABLE_FAKE_SHOP_HEADER
		if (pTextTail->pMountCountInstance)
		{
			pTextTail->pMountCountInstance->Render();
		}
#endif
#if defined(WJ_SHOW_MOB_INFO)
		if (pTextTail->pLevelTextInstance && (pTextTail->bIsPC == TRUE || CPythonSystem::Instance().IsShowMobLevel()))
#else
		if (pTextTail->pLevelTextInstance)
#endif
		{
			pTextTail->pLevelTextInstance->Render();
		}
#ifdef ENABLE_MULTI_LANGUAGE
		if (!CPythonSystem::Instance().GetHideMode6Status()) {
			if (pTextTail->pLanguageInstance)
				pTextTail->pLanguageInstance->Render();
		}
#ifdef ENABLE_FAKE_SHOP_HEADER
		if (pTextTail->pMountCountIconInstance)
			pTextTail->pMountCountIconInstance->Render();
#endif

#endif
	}

	for (itor = m_ItemTextTailList.begin(); itor != m_ItemTextTailList.end(); ++itor)
	{
		TTextTail* pTextTail = *itor;

		RenderTextTailBox(pTextTail);
		pTextTail->pTextInstance->Render();
		if (pTextTail->pOwnerTextInstance)
			pTextTail->pOwnerTextInstance->Render();
	}

#ifdef ENABLE_NEW_SHOP_IN_CITIES
	if (!CPythonSystem::Instance().GetHideMode5Status()) {
		for (auto itorMap = m_ShopTextTailMap.begin(); itorMap != m_ShopTextTailMap.end(); ++itorMap)
		{
			if (!itorMap->second->bRender)
				continue;

			TTextTail* pTextTail = itorMap->second;

			RenderTextTailBox(pTextTail);
			pTextTail->pTextInstance->Render();
			if (pTextTail->pOwnerTextInstance)
				pTextTail->pOwnerTextInstance->Render();
		}
	}
#endif

	for (auto itorChat = m_ChatTailMap.begin(); itorChat != m_ChatTailMap.end(); ++itorChat)
	{
		TTextTail* pTextTail = itorChat->second;
		if (pTextTail->pOwner->isShow())
			RenderTextTailName(pTextTail);
	}
}

void CPythonTextTail::RenderTextTailBox(TTextTail* pTextTail)
{
	// °ËÀº»ö �
#ifdef __ENABLE_NEW_OFFLINESHOP__
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	if (pTextTail->bIsShop)
	{
		// °ËÀº»ö �
		CPythonGraphic::Instance().SetDiffuseColor(0.0f, 0.0f, 0.0f, 1.0f);
		CPythonGraphic::Instance().RenderBox2d(pTextTail->x + pTextTail->xStart - 10.f,
			pTextTail->y + pTextTail->yStart - 10.f,
			pTextTail->x + pTextTail->xEnd + 10.f,
			pTextTail->y + pTextTail->yEnd + 10.f,
			pTextTail->z);

		
		CPythonGraphic::Instance().SetDiffuseColor(0.0f, 0.0f, 0.0f, 0.3f);
		CPythonGraphic::Instance().RenderBar2d(pTextTail->x + pTextTail->xStart - 10.f,
			pTextTail->y + pTextTail->yStart - 10.f,
			pTextTail->x + pTextTail->xEnd + 10.f,
			pTextTail->y + pTextTail->yEnd + 10.f,
			pTextTail->z);

		return;
	}
#	endif
#endif
	CPythonGraphic::Instance().SetDiffuseColor(0.0f, 0.0f, 0.0f, 1.0f);
	CPythonGraphic::Instance().RenderBox2d(pTextTail->x + pTextTail->xStart,
		pTextTail->y + pTextTail->yStart,
		pTextTail->x + pTextTail->xEnd,
		pTextTail->y + pTextTail->yEnd,
		pTextTail->z);

	// °ËÀº»ö �
	CPythonGraphic::Instance().SetDiffuseColor(0.0f, 0.0f, 0.0f, 0.3f);
	CPythonGraphic::Instance().RenderBar2d(pTextTail->x + pTextTail->xStart,
		pTextTail->y + pTextTail->yStart,
		pTextTail->x + pTextTail->xEnd,
		pTextTail->y + pTextTail->yEnd,
		pTextTail->z);
}
#ifdef ENABLE_FAKE_SHOP_HEADER
void CPythonTextTail::RenderTextTailName(TTextTail* pTextTail)
{
	if (!pTextTail)
		return;

	if (pTextTail->pTextInstance)
		pTextTail->pTextInstance->Render();

	if (pTextTail->pMountCountInstance && (uintptr_t)pTextTail->pMountCountInstance > 0x10000)
		pTextTail->pMountCountInstance->Render();
	//else
	//	TraceError(">>> [RenderTextTailName] pMountCountInstance invalid (nullptr or corrupted) for dwVID=%u\n", pTextTail->dwVirtualID);
}
#else

void CPythonTextTail::RenderTextTailName(TTextTail* pTextTail)
{
	pTextTail->pTextInstance->Render();
}
#endif
void CPythonTextTail::HideAllTextTail()
{
	// NOTE : Show AllÀ» ÇØÁØµÚ Hide AllÀ» ÇØÁÖÁö ¾ÊÀ¸¸é ¹®Á¦ ¹ß»ý °¡´É¼º ÀÖÀ½
	//        µðÀÚÀÎ ÀÚÃ¼°¡ ±×·¸°Ô ±ò²ûÇÏ°Ô µÇÁö ¾Ê¾ÒÀ½ - [levites]
	m_CharacterTextTailList.clear();
	m_ItemTextTailList.clear();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	for (auto& iter : m_ShopTextTailMap)
		iter.second->bRender = false;
#endif
}

void CPythonTextTail::UpdateDistance(const TPixelPosition& c_rCenterPosition, TTextTail* pTextTail)
{
	const D3DXVECTOR3& c_rv3Position = pTextTail->pOwner->GetPosition();
	D3DXVECTOR2 v2Distance(c_rv3Position.x - c_rCenterPosition.x, -c_rv3Position.y - c_rCenterPosition.y);
	pTextTail->fDistanceFromPlayer = D3DXVec2Length(&v2Distance);
}

void CPythonTextTail::ShowAllTextTail()
{
	TTextTailMap::iterator itor;
	for (itor = m_CharacterTextTailMap.begin(); itor != m_CharacterTextTailMap.end(); ++itor)
	{
		TTextTail* pTextTail = itor->second;
		if (pTextTail->fDistanceFromPlayer < 3500.0f) {
			ShowCharacterTextTail(itor->first);
		}
	}
	for (itor = m_ItemTextTailMap.begin(); itor != m_ItemTextTailMap.end(); ++itor)
	{
		TTextTail* pTextTail = itor->second;
		if (pTextTail->fDistanceFromPlayer < 3500.0f) {
			ShowItemTextTail(itor->first);
		}
	}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	//OFFSHOP_DEBUG("ShopTextTailMap size %u ",m_ShopTextTailMap.size());
	for (itor = m_ShopTextTailMap.begin(); itor != m_ShopTextTailMap.end(); ++itor)
	{
		TTextTail* pTextTail = itor->second;
		if (pTextTail->fDistanceFromPlayer < 3500.f) {
#ifdef OUTLINE_NAMES_TEXTLINE
			pTextTail->pTextInstance->SetOutline(CPythonSystem::Instance().GetNamesType());

#endif
			pTextTail->bRender = true;
		}
	}
#endif
}

void CPythonTextTail::ShowCharacterTextTail(uint32_t VirtualID)
{
	auto itor = m_CharacterTextTailMap.find(VirtualID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;

	//  SELF-HEAL: 
#ifdef ENABLE_FAKE_SHOP_HEADER
	if (!pTextTail->MountCountText.empty())
	{

		if (!pTextTail->pMountCountInstance || !pTextTail->pMountCountIconInstance)
		{

			//TraceError("[SelfHeal] Re-creating MountCount for VID %u", VirtualID);
			AttachMountCountWithIcon(
				VirtualID,
				pTextTail->MountCountText.c_str(),
				pTextTail->MountCountColor,
				pTextTail->MountCountIconPath.c_str());
		}
	}
#endif

	// Ha már a listában van, nem tesszük be újra
	if (std::find(m_CharacterTextTailList.begin(), m_CharacterTextTailList.end(), pTextTail) != m_CharacterTextTailList.end())
		return;

	if (!pTextTail->pOwner->isShow())
		return;

	CInstanceBase* pInstance = CPythonCharacterManager::Instance().GetInstancePtr(pTextTail->dwVirtualID);
	if (!pInstance)
		return;

	if (pInstance->IsGuildWall())
		return;

#ifdef OUTLINE_NAMES_TEXTLINE
	bool outline = CPythonSystem::Instance().GetNamesType();
	if (pTextTail->pTextInstance)
		pTextTail->pTextInstance->SetOutline(outline);
	if (pTextTail->pMountCountInstance)
		pTextTail->pMountCountInstance->SetOutline(outline);
	if (pTextTail->pLevelTextInstance)
		pTextTail->pLevelTextInstance->SetOutline(outline);
	if (pTextTail->pTitleTextInstance)
		pTextTail->pTitleTextInstance->SetOutline(outline);
	if (pTextTail->pGuildNameTextInstance)
		pTextTail->pGuildNameTextInstance->SetOutline(outline);
#endif

	if (pInstance->CanPickInstance())
	{
		m_CharacterTextTailList.push_back(pTextTail);
	}
	else
	{
#ifdef ENABLE_FAKE_SHOP_HEADER
		
		if (pTextTail->pMountCountInstance)
			m_CharacterTextTailList.push_back(pTextTail);
#endif
	}
}



void CPythonTextTail::ShowItemTextTail(uint32_t VirtualID)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(VirtualID);

	if (m_ItemTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;

	if (m_ItemTextTailList.end() != std::find(m_ItemTextTailList.begin(), m_ItemTextTailList.end(), pTextTail))
	{
		//Tracef("ÀÌ¹Ì ¸®½ºÆ®¿¡ ÀÖÀ½ : %d\n", VirtualID);
		return;
	}

#ifdef OUTLINE_NAMES_TEXTLINE
	if (pTextTail->pTextInstance) {
		pTextTail->pTextInstance->SetOutline(CPythonSystem::Instance().GetNamesType());
	}
#endif

	m_ItemTextTailList.push_back(pTextTail);
}

bool CPythonTextTail::isIn(CPythonTextTail::TTextTail* pSource, CPythonTextTail::TTextTail* pTarget)
{
	float x1Source = pSource->x + pSource->xStart;
	float y1Source = pSource->y + pSource->yStart;
	float x2Source = pSource->x + pSource->xEnd;
	float y2Source = pSource->y + pSource->yEnd;
	float x1Target = pTarget->x + pTarget->xStart;
	float y1Target = pTarget->y + pTarget->yStart;
	float x2Target = pTarget->x + pTarget->xEnd;
	float y2Target = pTarget->y + pTarget->yEnd;

	if (x1Source <= x2Target && x2Source >= x1Target &&
		y1Source <= y2Target && y2Source >= y1Target)
	{
		return true;
	}

	return false;
}

void CPythonTextTail::RegisterCharacterTextTail(uint32_t dwGuildID, uint32_t dwVirtualID, const D3DXCOLOR& c_rColor, float fAddHeight, bool IsPC)
{
	CInstanceBase* pCharacterInstance = CPythonCharacterManager::Instance().GetInstancePtr(dwVirtualID);

	if (!pCharacterInstance)
		return;

		
		std::string strBaseName = pCharacterInstance->GetNameString();
	#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
		std::string strItemPrefix;
		{
			auto itPrefix = m_ItemOnTitlePrefixMap.find(dwVirtualID);
			if (itPrefix != m_ItemOnTitlePrefixMap.end())
				strItemPrefix = itPrefix->second;
		}
		const std::string strDisplayName = __ComposeNameWithItemOnTitle(strBaseName, strItemPrefix);
	#else
		const std::string strDisplayName = strBaseName;
	#endif
TTextTail* pTextTail = RegisterTextTail(dwVirtualID,
		strDisplayName.c_str(),
		pCharacterInstance->GetGraphicThingInstancePtr(),
		pCharacterInstance->GetGraphicThingInstanceRef().GetHeight() + fAddHeight,
		c_rColor);

	CGraphicTextInstance* pTextInstance = pTextTail->pTextInstance;
	#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
		pTextTail->BaseName = strBaseName;
		pTextTail->ItemOnTitlePrefix = strItemPrefix;
	#endif
#ifndef OUTLINE_NAMES_TEXTLINE
	pTextInstance->SetOutline(true);
#endif
	pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	pTextTail->pMarkInstance = nullptr;
	pTextTail->pGuildNameTextInstance = nullptr;
	pTextTail->pTitleTextInstance = nullptr;
	pTextTail->pLevelTextInstance = nullptr;
#ifdef ENABLE_MULTI_LANGUAGE
	pTextTail->pLanguageInstance = nullptr;
#endif
//#ifdef ENABLE_FAKE_SHOP_HEADER//icon
//	pTextTail->pMountCountIconInstance = nullptr;
//
//#endif
//#ifdef ENABLE_FAKE_SHOP_HEADER
//	pTextTail->pMountCountInstance = nullptr;
//#endif
	pTextTail->bIsPC = IsPC;

	if (0 != dwGuildID)
	{
		pTextTail->pMarkInstance = CGraphicMarkInstance::New();

		uint32_t dwMarkID = CGuildMarkManager::Instance().GetMarkID(dwGuildID);

		if (dwMarkID != CGuildMarkManager::INVALID_MARK_ID)
		{
			std::string markImagePath;

			if (CGuildMarkManager::Instance().GetMarkImageFilename(dwMarkID / CGuildMarkImage::MARK_TOTAL_COUNT, markImagePath))
			{
				pTextTail->pMarkInstance->SetImageFileName(markImagePath.c_str());
				pTextTail->pMarkInstance->Load();
				pTextTail->pMarkInstance->SetIndex(dwMarkID % CGuildMarkImage::MARK_TOTAL_COUNT);
			}
		}

		std::string strGuildName;
		if (!CPythonGuild::Instance().GetGuildName(dwGuildID, &strGuildName))
			strGuildName = "Noname";
#ifdef ENABLE_GUILD_LV_SHOW_ABOVE_CHAR_RAZOR93
		int guildLevel = CPythonGuild::Instance().GetGuildLevel(dwGuildID);

		
		char szGuildLevelText[16];
		_snprintf(szGuildLevelText, sizeof(szGuildLevelText), " (Lv%d)", guildLevel);
		strGuildName += szGuildLevelText;

#endif
		CGraphicTextInstance*& prGuildNameInstance = pTextTail->pGuildNameTextInstance;
		prGuildNameInstance = CGraphicTextInstance::New();
		prGuildNameInstance->SetTextPointer(ms_pFont);
#ifndef OUTLINE_NAMES_TEXTLINE
		prGuildNameInstance->SetOutline(true);
#endif
		prGuildNameInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		prGuildNameInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
		prGuildNameInstance->SetValue(strGuildName.c_str());
		prGuildNameInstance->SetColor(c_TextTail_Guild_Name_Color.r, c_TextTail_Guild_Name_Color.g, c_TextTail_Guild_Name_Color.b);
		prGuildNameInstance->Update();

	}

#ifdef ENABLE_MULTI_LANGUAGE
	if (!pCharacterInstance->IsSA()) {
		CGraphicImageInstance*& prLanguage = pTextTail->pLanguageInstance;

		if (!prLanguage)
		{
			uint8_t bLanguage = pCharacterInstance->GetLanguage();

			if (pCharacterInstance->IsPC() && bLanguage)
			{
				std::string langName = "en";

				if (bLanguage == 1)
					langName = "en";
				else if (bLanguage == 2)
					langName = "ro";
				else if (bLanguage == 3)
					langName = "it";
				else if (bLanguage == 4)
					langName = "tr";
				else if (bLanguage == 5)
					langName = "de";
				else if (bLanguage == 6)
					langName = "pl";
				else if (bLanguage == 7)
					langName = "pt";
				else if (bLanguage == 8)
					langName = "es";
				else if (bLanguage == 9)
					langName = "cz";
				else if (bLanguage == 10)
					langName = "hu";

				char szFileName[256];
				sprintf(szFileName, "d:/ymir work/ui/game/flag/%s.tga", langName.c_str());

				if (CResourceManager::Instance().IsFileExist(szFileName))
				{
					CGraphicImage* pLanguageImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(szFileName);

					if (pLanguageImage)
					{
						prLanguage = CGraphicImageInstance::New();
						prLanguage->SetImagePointer(pLanguageImage);
					}
				}
			}
		}
	}
#endif

	m_CharacterTextTailMap.insert(TTextTailMap::value_type(dwVirtualID, pTextTail));
}

#ifdef ENABLE_FAKE_SHOP_HEADER

CPythonTextTail::TTextTail* CPythonTextTail::GetTextTail(uint32_t dwVID)
{
	auto it = m_CharacterTextTailMap.find(dwVID);
	if (it != m_CharacterTextTailMap.end())
		return it->second;
	return nullptr;
}



void CPythonTextTail::RegisterMountCountTextTail(uint32_t dwVID, const std::string& mountText)
{

	CInstanceBase* pInstance = CPythonCharacterManager::Instance().GetInstancePtr(dwVID);
	if (!pInstance)
		return;
	if (!pInstance->IsPC())
		return;

	// Keressünk rá a létező MountCount TextTail-re CSAK a listában!
	TTextTail* pTextTail = nullptr;

	for (auto it = m_CharacterTextTailList.begin(); it != m_CharacterTextTailList.end(); ++it)
	{
		if ((*it)->dwVirtualID == dwVID && (*it)->pMountCountInstance)
		{
			pTextTail = *it;
			break;
		}
	}

	if (!pTextTail)
	{
		// Új Tail csak a MountCount-hoz
		pTextTail = m_TextTailPool.Alloc();
		memset(pTextTail, 0, sizeof(TTextTail));
		pTextTail->dwVirtualID = dwVID;
		pTextTail->bIsPC = pInstance->IsPC();

		pTextTail->pMountCountInstance = CGraphicTextInstance::New();
		pTextTail->pMountCountInstance->SetTextPointer(ms_pFont);
		pTextTail->pMountCountInstance->SetValue(mountText.c_str());
		pTextTail->pMountCountInstance->SetColor(255, 204, 0);
		pTextTail->pMountCountInstance->SetOutline(true);
		pTextTail->pMountCountInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		pTextTail->pMountCountInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
		pTextTail->pMountCountInstance->Update();

		m_CharacterTextTailList.push_back(pTextTail);

		// ⚠️ NINCS insert a m_CharacterTextTailMap-be!
	}
	else
	{
		// Már létezik → csak frissítjük a szöveget
		pTextTail->pMountCountInstance->SetValue(mountText.c_str());
		pTextTail->pMountCountInstance->SetColor(255, 204, 0);
		pTextTail->pMountCountInstance->Update();
	}
}
#endif


void CPythonTextTail::RegisterItemTextTail(uint32_t VirtualID, const char* c_szText, CGraphicObjectInstance* pOwner)
{
#ifdef __DEBUG
	char szName[256];
	spritnf(szName, "%s[%d]", c_szText, VirtualID);

	TTextTail* pTextTail = RegisterTextTail(VirtualID, c_szText, pOwner, c_TextTail_Name_Position, c_TextTail_Item_Color);
	m_ItemTextTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
#else
	TTextTail* pTextTail = RegisterTextTail(VirtualID, c_szText, pOwner, c_TextTail_Name_Position, c_TextTail_Item_Color);
	m_ItemTextTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
#endif
}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
void CPythonTextTail::RegisterShopInstanceTextTail(uint32_t dwVirtualID, const char* c_szName, CGraphicObjectInstance* pOwner)
{
	TTextTail* pTextTail = RegisterShopTextTail(dwVirtualID, c_szName, pOwner);
	m_ShopTextTailMap.insert(TTextTailMap::value_type(dwVirtualID, pTextTail));
}

void CPythonTextTail::SetShopTextTailColor(uint32_t dwVirtualID, const D3DXCOLOR& c_rColor)
{
	const auto itor = m_ShopTextTailMap.find(dwVirtualID);
	if (m_ShopTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;
	if (!pTextTail || !pTextTail->pTextInstance)
		return;

	pTextTail->Color = c_rColor;
	pTextTail->pTextInstance->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	pTextTail->pTextInstance->Update();
}

#endif

void CPythonTextTail::RegisterChatTail(uint32_t VirtualID, const char* c_szChat)
{
	CInstanceBase* pCharacterInstance = CPythonCharacterManager::Instance().GetInstancePtr(VirtualID);

	if (!pCharacterInstance)
		return;

	TChatTailMap::iterator itor = m_ChatTailMap.find(VirtualID);

	if (m_ChatTailMap.end() != itor)
	{
		TTextTail* pTextTail = itor->second;

		pTextTail->pTextInstance->SetValue(c_szChat);
		pTextTail->pTextInstance->Update();
		pTextTail->Color = c_TextTail_Chat_Color;
		pTextTail->pTextInstance->SetColor(c_TextTail_Chat_Color);

		// TEXTTAIL_LIVINGTIME_CONTROL
		pTextTail->LivingTime = CTimer::Instance().GetCurrentMillisecond() + TextTail_GetLivingTime();
		// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

		pTextTail->bNameFlag = TRUE;

		return;
	}

	TTextTail* pTextTail = RegisterTextTail(VirtualID,
		c_szChat,
		pCharacterInstance->GetGraphicThingInstancePtr(),
		pCharacterInstance->GetGraphicThingInstanceRef().GetHeight() + 10.0f
#ifdef ENABLE_RACE_HEIGHT
		+ pCharacterInstance->GetBaseHeight()
#endif
		,
		c_TextTail_Chat_Color);

	// TEXTTAIL_LIVINGTIME_CONTROL
	pTextTail->LivingTime = CTimer::Instance().GetCurrentMillisecond() + TextTail_GetLivingTime();
	// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

	pTextTail->bNameFlag = TRUE;
#ifndef OUTLINE_NAMES_TEXTLINE
	pTextTail->pTextInstance->SetOutline(true);
#endif
	pTextTail->pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	m_ChatTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
}

void CPythonTextTail::RegisterInfoTail(uint32_t VirtualID, const char* c_szChat)
{
	CInstanceBase* pCharacterInstance = CPythonCharacterManager::Instance().GetInstancePtr(VirtualID);

	if (!pCharacterInstance)
		return;

	TChatTailMap::iterator itor = m_ChatTailMap.find(VirtualID);

	if (m_ChatTailMap.end() != itor)
	{
		TTextTail* pTextTail = itor->second;

		pTextTail->pTextInstance->SetValue(c_szChat);
		pTextTail->pTextInstance->Update();
		pTextTail->Color = c_TextTail_Info_Color;
		pTextTail->pTextInstance->SetColor(c_TextTail_Info_Color);

		// TEXTTAIL_LIVINGTIME_CONTROL
		pTextTail->LivingTime = CTimer::Instance().GetCurrentMillisecond() + TextTail_GetLivingTime();
		// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

		pTextTail->bNameFlag = FALSE;

		return;
	}

	TTextTail* pTextTail = RegisterTextTail(VirtualID,
		c_szChat,
		pCharacterInstance->GetGraphicThingInstancePtr(),
		pCharacterInstance->GetGraphicThingInstanceRef().GetHeight() + 10.0f,
		c_TextTail_Info_Color);

	// TEXTTAIL_LIVINGTIME_CONTROL
	pTextTail->LivingTime = CTimer::Instance().GetCurrentMillisecond() + TextTail_GetLivingTime();
	// END_OF_TEXTTAIL_LIVINGTIME_CONTROL

	pTextTail->bNameFlag = FALSE;
#ifndef OUTLINE_NAMES_TEXTLINE
	pTextTail->pTextInstance->SetOutline(true);
#endif
	pTextTail->pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	m_ChatTailMap.insert(TTextTailMap::value_type(VirtualID, pTextTail));
}

bool CPythonTextTail::GetTextTailPosition(uint32_t dwVID, float* px, float* py, float* pz)
{
	TTextTailMap::iterator itorCharacter = m_CharacterTextTailMap.find(dwVID);

	if (m_CharacterTextTailMap.end() == itorCharacter)
	{
		return false;
	}

	TTextTail* pTextTail = itorCharacter->second;
	*px = pTextTail->x;
	*py = pTextTail->y;
	*pz = pTextTail->z;

	return true;
}

bool CPythonTextTail::IsChatTextTail(uint32_t dwVID)
{
	TChatTailMap::iterator itorChat = m_ChatTailMap.find(dwVID);

	if (m_ChatTailMap.end() == itorChat)
		return false;

	return true;
}

void CPythonTextTail::SetCharacterTextTailColor(uint32_t VirtualID, const D3DXCOLOR& c_rColor)
{
	TTextTailMap::iterator itorCharacter = m_CharacterTextTailMap.find(VirtualID);

	if (m_CharacterTextTailMap.end() == itorCharacter)
		return;

	TTextTail* pTextTail = itorCharacter->second;
	pTextTail->pTextInstance->SetColor(c_rColor);
	pTextTail->Color = c_rColor;
}

void CPythonTextTail::SetItemTextTailOwner(uint32_t dwVID, const char* c_szName)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(dwVID);
	if (m_ItemTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;

	if (strlen(c_szName) > 0)
	{
		if (!pTextTail->pOwnerTextInstance)
		{
			pTextTail->pOwnerTextInstance = CGraphicTextInstance::New();
		}

		std::string strName = c_szName;
		static const std::string& strOwnership = ApplicationStringTable_GetString(IDS_POSSESSIVE_MORPHENE).empty() ? "'s" : ApplicationStringTable_GetString(IDS_POSSESSIVE_MORPHENE);
		strName += strOwnership;


		pTextTail->pOwnerTextInstance->SetTextPointer(ms_pFont);
		pTextTail->pOwnerTextInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		pTextTail->pOwnerTextInstance->SetValue(strName.c_str());
		pTextTail->pOwnerTextInstance->SetColor(1.0f, 1.0f, 0.0f);
		pTextTail->pOwnerTextInstance->Update();


		int xOwnerSize, yOwnerSize;
		pTextTail->pOwnerTextInstance->GetTextSize(&xOwnerSize, &yOwnerSize);
		pTextTail->yStart = -2.0f;
		pTextTail->yEnd += float(yOwnerSize + 4);
		pTextTail->xStart = fMIN(pTextTail->xStart, float(-xOwnerSize / 2 - 1));
		pTextTail->xEnd = fMAX(pTextTail->xEnd, float(xOwnerSize / 2 + 1));
	}
	else
	{
		if (pTextTail->pOwnerTextInstance)
		{
			CGraphicTextInstance::Delete(pTextTail->pOwnerTextInstance);
			pTextTail->pOwnerTextInstance = nullptr;
		}

		int xSize, ySize;
		pTextTail->pTextInstance->GetTextSize(&xSize, &ySize);
		pTextTail->xStart = (float)(-xSize / 2 - 2);
		pTextTail->yStart = -2.0f;
		pTextTail->xEnd = (float)(xSize / 2 + 2);
		pTextTail->yEnd = (float)ySize;
	}
}
#ifdef ENABLE_FAKE_SHOP_HEADERd
void CPythonTextTail::DeleteCharacterTextTail(uint32_t VirtualID)
{

	auto itorChat = m_ChatTailMap.find(VirtualID);
	if (m_ChatTailMap.end() != itorChat)
	{
		DeleteTextTail(itorChat->second);
		m_ChatTailMap.erase(itorChat);
	}
}
#else
void CPythonTextTail::DeleteCharacterTextTail(uint32_t VirtualID)
{

	auto itorCharacter = m_CharacterTextTailMap.find(VirtualID);
	auto itorChat = m_ChatTailMap.find(VirtualID);

	if (m_CharacterTextTailMap.end() != itorCharacter)
	{
		
		auto itList = std::find(m_CharacterTextTailList.begin(), m_CharacterTextTailList.end(), itorCharacter->second);
		if (itList != m_CharacterTextTailList.end())
			m_CharacterTextTailList.erase(itList);

		DeleteTextTail(itorCharacter->second);
		m_CharacterTextTailMap.erase(itorCharacter);
	}

	if (m_ChatTailMap.end() != itorChat)
	{
		DeleteTextTail(itorChat->second);
		m_ChatTailMap.erase(itorChat);
	}
}
//void CPythonTextTail::DeleteCharacterTextTail(uint32_t VirtualID)
//{
//	auto itorCharacter = m_CharacterTextTailMap.find(VirtualID);
//	auto itorChat = m_ChatTailMap.find(VirtualID);
//
//	if (m_CharacterTextTailMap.end() != itorCharacter)
//	{
//		DeleteTextTail(itorCharacter->second);
//		m_CharacterTextTailMap.erase(itorCharacter);
//	}
//	else
//	{
//		Tracenf("CPythonTextTail::DeleteCharacterTextTail - Find VID[%d] Error", VirtualID);
//	}
//
//	if (m_ChatTailMap.end() != itorChat)
//	{
//		DeleteTextTail(itorChat->second);
//		m_ChatTailMap.erase(itorChat);
//	}
//}


#endif
#ifdef ENABLE_FAKE_SHOP_HEADER
void CPythonTextTail::DeleteMountCountTextTail(uint32_t dwVID)
{
	auto it = m_CharacterTextTailMap.find(dwVID);
	if (it != m_CharacterTextTailMap.end())
	{
		TTextTail* pTextTail = it->second;

		auto itList = std::find(m_CharacterTextTailList.begin(), m_CharacterTextTailList.end(), pTextTail);
		if (itList != m_CharacterTextTailList.end())
			m_CharacterTextTailList.erase(itList);

		if (pTextTail->pMountCountInstance)
			CGraphicTextInstance::Delete(pTextTail->pMountCountInstance);
		pTextTail->pMountCountInstance = nullptr;
	}
}

#endif

#ifdef ENABLE_NEW_SHOP_IN_CITIES
void CPythonTextTail::DeleteShopTextTail(uint32_t VirtualID)
{
	TTextTailMap::iterator itor = m_ShopTextTailMap.find(VirtualID);

	if (m_ShopTextTailMap.end() == itor)
	{
		Tracef(" CPythonTextTail::DeleteShopTextTail - None Item Text Tail\n");
		return;
	}

	DeleteTextTail(itor->second);
	m_ShopTextTailMap.erase(itor);
}
#endif

void CPythonTextTail::DeleteItemTextTail(uint32_t VirtualID)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(VirtualID);

	if (m_ItemTextTailMap.end() == itor)
	{
		Tracef(" CPythonTextTail::DeleteItemTextTail - None Item Text Tail\n");
		return;
	}

	DeleteTextTail(itor->second);
	m_ItemTextTailMap.erase(itor);
}

CPythonTextTail::TTextTail* CPythonTextTail::RegisterTextTail(uint32_t dwVirtualID, const char* c_szText, CGraphicObjectInstance* pOwner, float fHeight, const D3DXCOLOR& c_rColor)
{
	TTextTail* pTextTail = m_TextTailPool.Alloc();

#ifdef __ENABLE_NEW_OFFLINESHOP__
#	ifdef ENABLE_NEW_SHOP_IN_CITIES
	pTextTail->bIsShop = false;
	pTextTail->bRender = false;
#	endif
#endif
	pTextTail->dwVirtualID = dwVirtualID;
	pTextTail->pOwner = pOwner;
	pTextTail->pTextInstance = CGraphicTextInstance::New();
#ifdef ENABLE_FAKE_SHOP_HEADER
	pTextTail->pMountCountInstance = CGraphicTextInstance::New();
#endif

	pTextTail->pOwnerTextInstance = nullptr;
	pTextTail->fHeight = fHeight;

	pTextTail->pTextInstance->SetTextPointer(ms_pFont);
	pTextTail->pTextInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
	pTextTail->pTextInstance->SetValue(c_szText);
	pTextTail->pTextInstance->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	pTextTail->pTextInstance->Update();

	int xSize, ySize;
	pTextTail->pTextInstance->GetTextSize(&xSize, &ySize);
	pTextTail->xStart = (float)(-xSize / 2 - 2);
	pTextTail->yStart = -2.0f;
	pTextTail->xEnd = (float)(xSize / 2 + 2);
	pTextTail->yEnd = (float)ySize;
	pTextTail->Color = c_rColor;
	pTextTail->fDistanceFromPlayer = 0.0f;
	pTextTail->x = -100.0f;
	pTextTail->y = -100.0f;
	pTextTail->z = 0.0f;
	pTextTail->pMarkInstance = nullptr;
	pTextTail->pGuildNameTextInstance = nullptr;
	pTextTail->pTitleTextInstance = nullptr;
	pTextTail->pLevelTextInstance = nullptr;

#ifdef ENABLE_MULTI_LANGUAGE
	pTextTail->pLanguageInstance = nullptr;
#endif
#ifdef ENABLE_FAKE_SHOP_HEADER//icon
	pTextTail->pMountCountIconInstance = nullptr;

#endif
	return pTextTail;
}
#ifdef ENABLE_NEW_SHOP_IN_CITIES
CPythonTextTail::TTextTail* CPythonTextTail::RegisterShopTextTail(uint32_t dwVirtualID, const char* c_szText, CGraphicObjectInstance* pOwner)
{

	const D3DXCOLOR& c_rColor = D3DXCOLOR(1.0, 1.0, 0.5, 1.0);

	TTextTail* pTextTail = m_TextTailPool.Alloc();

	pTextTail->bIsShop = true;

	pTextTail->dwVirtualID = dwVirtualID;
	pTextTail->pOwner = pOwner;
	pTextTail->pTextInstance = CGraphicTextInstance::New();
	pTextTail->pOwnerTextInstance = nullptr;
	pTextTail->fHeight = 180.f;

	pTextTail->pTextInstance->SetTextPointer(ms_pFont);
	pTextTail->pTextInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
	pTextTail->pTextInstance->SetValue(c_szText);
	pTextTail->pTextInstance->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	pTextTail->pTextInstance->Update();

	int xSize, ySize;
	pTextTail->pTextInstance->GetTextSize(&xSize, &ySize);
	pTextTail->xStart = (float)(-xSize / 2 - 2);
	pTextTail->yStart = -2.0f;
	pTextTail->xEnd = (float)(xSize / 2 + 2);
	pTextTail->yEnd = (float)ySize;
	pTextTail->Color = c_rColor;
	pTextTail->fDistanceFromPlayer = 0.0f;
	pTextTail->x = -100.0f;
	pTextTail->y = -100.0f;
	pTextTail->z = 0.0f;
	pTextTail->pMarkInstance = nullptr;
	pTextTail->pGuildNameTextInstance = nullptr;
	pTextTail->pTitleTextInstance = nullptr;
	pTextTail->pLevelTextInstance = nullptr;
#ifdef ENABLE_FAKE_SHOP_HEADER
	pTextTail->pMountCountInstance = nullptr;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	pTextTail->pLanguageInstance = nullptr;
#endif
#ifdef ENABLE_FAKE_SHOP_HEADER//icon
	pTextTail->pMountCountIconInstance = nullptr;

#endif
	return pTextTail;
}

bool CPythonTextTail::GetPickedNewShop(uint32_t* pdwVID)
{
	*pdwVID = 0;

	if (!CPythonOfflineshop::instance().GetShowNameFlag() && !CPythonSystem::instance().IsAlwaysShowName())
		return false;

	long ixMouse = 0, iyMouse = 0;

	POINT p;
	CPythonApplication::Instance().GetMousePosition(&p);

	ixMouse = p.x;
	iyMouse = p.y;

	for (auto itor = m_ShopTextTailMap.begin(); itor != m_ShopTextTailMap.end(); ++itor)
	{
		TTextTail* pTextTail = itor->second;

		if (ixMouse >= pTextTail->x + (pTextTail->xStart - 10) && ixMouse <= pTextTail->x + (pTextTail->xEnd + 10) &&
			iyMouse >= pTextTail->y + (pTextTail->yStart - 10) && iyMouse <= pTextTail->y + (pTextTail->yEnd + 10))
		{
			*pdwVID = itor->first;
			return true;
		}
	}

	return false;
}

#endif

void CPythonTextTail::DeleteTextTail(TTextTail* pTextTail)
{
	if (pTextTail->pTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pTextInstance);
		pTextTail->pTextInstance = nullptr;
	}
	if (pTextTail->pOwnerTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pOwnerTextInstance);
		pTextTail->pOwnerTextInstance = nullptr;
	}
	if (pTextTail->pMarkInstance)
	{
		CGraphicMarkInstance::Delete(pTextTail->pMarkInstance);
		pTextTail->pMarkInstance = nullptr;
	}
	if (pTextTail->pGuildNameTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pGuildNameTextInstance);
		pTextTail->pGuildNameTextInstance = nullptr;
	}
	if (pTextTail->pTitleTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pTitleTextInstance);
		pTextTail->pTitleTextInstance = nullptr;
	}
	if (pTextTail->pLevelTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pLevelTextInstance);
		pTextTail->pLevelTextInstance = nullptr;
	}
#ifdef ENABLE_FAKE_SHOP_HEADER
	if (pTextTail->pMountCountIconInstance)
	{
		CGraphicImageInstance::Delete(pTextTail->pMountCountIconInstance);
		pTextTail->pMountCountIconInstance = nullptr;
	}

	if (pTextTail->pMountCountInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pMountCountInstance);
		pTextTail->pMountCountInstance = nullptr;
	}
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	if (pTextTail->pLanguageInstance)
	{
		CGraphicImageInstance::Delete(pTextTail->pLanguageInstance);
		pTextTail->pLanguageInstance = nullptr;
	}
#endif
#ifdef ENABLE_FAKE_SHOP_HEADER//icon
	if (pTextTail->pMountCountIconInstance)
	{
		CGraphicImageInstance::Delete(pTextTail->pMountCountIconInstance);
		pTextTail->pMountCountIconInstance = nullptr;
	}
#endif
	m_TextTailPool.Free(pTextTail);
}

int CPythonTextTail::Pick(int ixMouse, int iyMouse)
{
	for (TTextTailMap::iterator itor = m_ItemTextTailMap.begin(); itor != m_ItemTextTailMap.end(); ++itor)
	{
		TTextTail* pTextTail = itor->second;

		if (ixMouse >= pTextTail->x + pTextTail->xStart && ixMouse <= pTextTail->x + pTextTail->xEnd &&
			iyMouse >= pTextTail->y + pTextTail->yStart && iyMouse <= pTextTail->y + pTextTail->yEnd)
		{
			SelectItemName(itor->first);
			return (itor->first);
		}
	}

	return -1;
}

void CPythonTextTail::SelectItemName(uint32_t dwVirtualID)
{
	TTextTailMap::iterator itor = m_ItemTextTailMap.find(dwVirtualID);

	if (m_ItemTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;
	pTextTail->pTextInstance->SetColor(0.1f, 0.9f, 0.1f);
}

void CPythonTextTail::AttachTitle(uint32_t dwVID, const char* c_szName, const D3DXCOLOR& c_rColor)
{
	//Tracef("AttachTitle: dwVID=%d, szText=%s\n", dwVID, c_szName);
	//TraceError("AttachTitle: bPKTitleEnable = %d\n", bPKTitleEnable);
	if (!bPKTitleEnable)
		return;
	

	//TraceError("AttachTitle: Searching for TextTail with dwVID = %u\n", dwVID);
	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
	{
	//	TraceError("AttachTitle: TextTail NOT FOUND for dwVID = %u\n", dwVID);
		return;
	}
	//TraceError("AttachTitle: TextTail FOUND for dwVID = %u\n", dwVID);


	TTextTail* pTextTail = itor->second;

	CGraphicTextInstance*& prTitle = pTextTail->pTitleTextInstance;
	if (!prTitle)
	{
		prTitle = CGraphicTextInstance::New();
		prTitle->SetTextPointer(ms_pFont);
#ifndef OUTLINE_NAMES_TEXTLINE
		prTitle->SetOutline(true);
#endif
		if (LocaleService_IsEUROPE())
			prTitle->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_RIGHT);
		else
			prTitle->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		prTitle->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	}

	prTitle->SetValue(c_szName);
	prTitle->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	prTitle->Update();
	//TraceError("AttachTitle: FINAL Text = %s\n", c_szName);

}
#ifdef ENABLE_FAKE_SHOP_HEADER
void CPythonTextTail::AttachMountCount(uint32_t dwVID, const char* szText, const D3DXCOLOR& color)
{
	CInstanceBase* pInstance = CPythonCharacterManager::Instance().GetInstancePtr(dwVID);
	if (!pInstance)
		return;
	if (!pInstance->IsPC())
		return;

	if (!bPKTitleEnable)
		return;

	if (!szText || strlen(szText) > 128)
		return;

	if (!ms_pFont)
		return;

	auto itor = m_CharacterTextTailMap.find(dwVID);
	if (itor == m_CharacterTextTailMap.end())
		return;

	TTextTail* pTextTail = itor->second;
	if (!pTextTail)
		return;
//#ifdef ENABLE_FAKE_SHOP_HEADER
//	if (pTextTail->pMountCountIconInstance)
//	{
//		CGraphicImageInstance::Delete(pTextTail->pMountCountIconInstance);
//		pTextTail->pMountCountIconInstance = nullptr;
//	}
//#endif
	// Létrehozás vagy újrainicializálás, ha invalid
	if (!pTextTail->pMountCountInstance || (uintptr_t)pTextTail->pMountCountInstance < 0x10000)
	{
		if (pTextTail->pMountCountInstance)
		{
			CGraphicTextInstance::Delete(pTextTail->pMountCountInstance);
			pTextTail->pMountCountInstance = nullptr;
		}

		pTextTail->pMountCountInstance = CGraphicTextInstance::New();
		if (!pTextTail->pMountCountInstance)
			return;

		pTextTail->pMountCountInstance->SetTextPointer(ms_pFont);
		pTextTail->pMountCountInstance->SetOutline(true);
		pTextTail->pMountCountInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		pTextTail->pMountCountInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	}
	else
	{
		// Már létezik → új érték, új szín
		pTextTail->pMountCountInstance->SetTextPointer(ms_pFont);
		
	}

	pTextTail->pMountCountInstance->SetValue(szText);
	pTextTail->pMountCountInstance->SetColor(color.r, color.g, color.b);
	pTextTail->pMountCountInstance->Update();
}


#endif
#ifdef ENABLE_FAKE_SHOP_HEADER
void CPythonTextTail::AttachMountCountWithIcon(uint32_t dwVID, const char* szText, const D3DXCOLOR& color, const char* szIconPath)
{
		CInstanceBase* pInstance = CPythonCharacterManager::Instance().GetInstancePtr(dwVID);
	if (!pInstance)
		return;
	if (!pInstance->IsPC())
		return;

	auto itor = m_CharacterTextTailMap.find(dwVID);
	if (itor == m_CharacterTextTailMap.end())
		return;

	TTextTail* pTextTail = itor->second;
	if (!pTextTail)
		return;

	// 🔒 Elmentjük a self-heal adatokat
	pTextTail->MountCountText = szText;
	pTextTail->MountCountColor = color;
	pTextTail->MountCountIconPath = szIconPath;

	// Szöveg
	if (!pTextTail->pMountCountInstance)
	{
		pTextTail->pMountCountInstance = CGraphicTextInstance::New();
		pTextTail->pMountCountInstance->SetTextPointer(ms_pFont);
		pTextTail->pMountCountInstance->SetOutline(true);
		pTextTail->pMountCountInstance->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
		pTextTail->pMountCountInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	}
	pTextTail->pMountCountInstance->SetValue(szText);
	pTextTail->pMountCountInstance->SetColor(color.r, color.g, color.b);
	pTextTail->pMountCountInstance->Update();

	// Ikon
	if (!pTextTail->pMountCountIconInstance)
		pTextTail->pMountCountIconInstance = CGraphicImageInstance::New();

	CGraphicImage* pImage = (CGraphicImage*)CResourceManager::Instance().GetResourcePointer(szIconPath);
	if (pImage)
	{
		pTextTail->pMountCountIconInstance->SetImagePointer(pImage);
		pTextTail->pMountCountIconInstance->SetScale(0.5f, 0.5f);
		pTextTail->pMountCountIconInstance->SetDiffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

#endif


void CPythonTextTail::DetachTitle(uint32_t dwVID)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;

	if (pTextTail->pTitleTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pTitleTextInstance);
		pTextTail->pTitleTextInstance = nullptr;
	}
}

void CPythonTextTail::EnablePKTitle(bool bFlag)
{
	bPKTitleEnable = bFlag;
}

void CPythonTextTail::AttachLevel(uint32_t dwVID, const char* c_szText, const D3DXCOLOR& c_rColor)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;

	CGraphicTextInstance*& prLevel = pTextTail->pLevelTextInstance;
	if (!prLevel)
	{
		prLevel = CGraphicTextInstance::New();
		prLevel->SetTextPointer(ms_pFont);
#ifndef OUTLINE_NAMES_TEXTLINE
		prLevel->SetOutline(true);
#endif
		prLevel->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_RIGHT);
		prLevel->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
	}

	prLevel->SetValue(c_szText);
	prLevel->SetColor(c_rColor.r, c_rColor.g, c_rColor.b);
	prLevel->Update();
}

void CPythonTextTail::DetachLevel(uint32_t dwVID)
{
	if (!bPKTitleEnable)
		return;

	TTextTailMap::iterator itor = m_CharacterTextTailMap.find(dwVID);
	if (m_CharacterTextTailMap.end() == itor)
		return;

	TTextTail* pTextTail = itor->second;

	if (pTextTail->pLevelTextInstance)
	{
		CGraphicTextInstance::Delete(pTextTail->pLevelTextInstance);
		pTextTail->pLevelTextInstance = nullptr;
	}
}


void CPythonTextTail::Initialize()
{
	// DEFAULT_FONT
	//ms_pFont = (CGraphicText *)CResourceManager::Instance().GetTypeResourcePointer(g_strDefaultFontName.c_str());

	CGraphicText* pkDefaultFont = static_cast<CGraphicText*>(DefaultFont_GetResource());
	if (!pkDefaultFont)
	{
		TraceError("CPythonTextTail::Initialize - CANNOT_FIND_DEFAULT_FONT");
		return;
	}

	ms_pFont = pkDefaultFont;
	// END_OF_DEFAULT_FONT
}

void CPythonTextTail::Destroy()
{
	m_TextTailPool.Clear();
}

void CPythonTextTail::Clear()
{
	m_CharacterTextTailMap.clear();
	m_CharacterTextTailList.clear();
	m_ItemTextTailMap.clear();
	m_ItemTextTailList.clear();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	m_ShopTextTailMap.clear();
#endif
	m_ChatTailMap.clear();

	m_TextTailPool.Clear();
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	m_ItemOnTitlePrefixMap.clear();
#endif

}

CPythonTextTail::CPythonTextTail()
{
	Clear();
}

CPythonTextTail::~CPythonTextTail()
{
	Destroy();
}


#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
void CPythonTextTail::SetItemOnTitlePrefix(uint32_t dwVID, const char* szPrefix)
{
	if (!szPrefix)
		szPrefix = "";

	m_ItemOnTitlePrefixMap[dwVID] = szPrefix;

	UpdateItemOnTitleTextTail(dwVID);
}

void CPythonTextTail::UpdateItemOnTitleTextTail(uint32_t dwVID)
{
	auto it = m_CharacterTextTailMap.find(dwVID);
	if (it == m_CharacterTextTailMap.end())
		return;

	TTextTail* pTextTail = it->second;
	if (!pTextTail || !pTextTail->pTextInstance)
		return;

	// Ensure we have a stable base name (without item prefix)
	if (pTextTail->BaseName.empty())
	{
		CInstanceBase* pInst = CPythonCharacterManager::Instance().GetInstancePtr(dwVID);
		if (pInst)
			pTextTail->BaseName = pInst->GetNameString();
	}

	std::string itemPrefix;
	auto itPfx = m_ItemOnTitlePrefixMap.find(dwVID);
	if (itPfx != m_ItemOnTitlePrefixMap.end())
		itemPrefix = itPfx->second;

	pTextTail->ItemOnTitlePrefix = itemPrefix;

	const std::string displayName = __ComposeNameWithItemOnTitle(pTextTail->BaseName, pTextTail->ItemOnTitlePrefix);
	pTextTail->pTextInstance->SetValue(displayName.c_str());
	pTextTail->pTextInstance->Update();

	int xSize, ySize;
	pTextTail->pTextInstance->GetTextSize(&xSize, &ySize);
	pTextTail->xStart = float(-xSize / 2 - 2);
	pTextTail->xEnd = float(xSize / 2 + 2);
	pTextTail->yEnd = float(ySize);

}
#endif
