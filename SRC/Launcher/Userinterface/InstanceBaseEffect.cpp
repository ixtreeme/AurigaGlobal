#include "StdAfx.h"
#include "InstanceBase.h"
#include "PythonTextTail.h"
#include "AbstractApplication.h"
#include "AbstractPlayer.h"
#include "PythonPlayer.h"
#include "PythonSystem.h"

#include "../Effect/EffectManager.h"
#include "../Effect/ParticleSystemData.h"
#include "../Render/Camera.h"

float CInstanceBase::ms_fDustGap;
float CInstanceBase::ms_fHorseDustGap;
uint32_t CInstanceBase::ms_adwCRCAffectEffect[CInstanceBase::EFFECT_NUM];
std::string CInstanceBase::ms_astAffectEffectAttachBone[EFFECT_NUM];

#define BYTE_COLOR_TO_D3DX_COLOR(r, g, b) D3DXCOLOR(float(r)/255.0f, float(g)/255.0f, float(b)/255.0f, 1.0f)

/*
D3DXCOLOR CInstanceBase::ms_kD3DXClrPC(0xFFFFD84D);//1.0f, 0.8470f, 0.3f, 1.0f
D3DXCOLOR CInstanceBase::ms_kD3DXClrNPC(0xFF7BE85E);//0.4823f, 0.9098f, 0.3686f, 1.0f
D3DXCOLOR CInstanceBase::ms_kD3DXClrMOB(0xFFEC170a);//0.9254f, 0.0901f, 0.0392f, 1.0f
D3DXCOLOR CInstanceBase::ms_kD3DXClrPVP(0xFF8532D9);
D3DXCOLOR CInstanceBase::ms_kD3DXClrPVPSelf(0xFFEE36DF);
D3DXCOLOR CInstanceBase::ms_kD3DXClrKiller = BYTE_COLOR_TO_D3DX_COLOR(180, 100, 0);
D3DXCOLOR CInstanceBase::ms_kD3DXClrTitle[CInstanceBase::TITLE_MAX_NUM] =
{
	BYTE_COLOR_TO_D3DX_COLOR(  0, 204, 255),
	BYTE_COLOR_TO_D3DX_COLOR(  0, 144, 255),
	BYTE_COLOR_TO_D3DX_COLOR( 92, 110, 255),
	BYTE_COLOR_TO_D3DX_COLOR(155, 155, 255),
	0xFFFFFFFF, // None
	BYTE_COLOR_TO_D3DX_COLOR(207, 117,   0),
	BYTE_COLOR_TO_D3DX_COLOR(235,  83,   0),
	BYTE_COLOR_TO_D3DX_COLOR(227,   0,   0),
	BYTE_COLOR_TO_D3DX_COLOR(255,   0,   0),
};
*/

D3DXCOLOR g_akD3DXClrTitle[CInstanceBase::TITLE_NUM];
D3DXCOLOR g_akD3DXClrName[CInstanceBase::NAMECOLOR_NUM];

#ifdef ENABLE_GENDER_ALIGNMENT
std::map<uint8_t, std::vector<std::string>> g_TitleNameMap;
#else
std::map<int, std::string> g_TitleNameMap;
#endif
std::set<uint32_t> g_kSet_dwPVPReadyKey;
std::set<uint32_t> g_kSet_dwPVPKey;
std::set<uint32_t> g_kSet_dwGVGKey;
std::set<uint32_t> g_kSet_dwDUELKey;

bool g_isEmpireNameMode=false;

void  CInstanceBase::SetEmpireNameMode(bool isEnable)
{
	g_isEmpireNameMode=isEnable;

	if (isEnable)
	{
		g_akD3DXClrName[NAMECOLOR_MOB]=g_akD3DXClrName[NAMECOLOR_EMPIRE_MOB];
		g_akD3DXClrName[NAMECOLOR_NPC]=g_akD3DXClrName[NAMECOLOR_EMPIRE_NPC];
		g_akD3DXClrName[NAMECOLOR_PC]=g_akD3DXClrName[NAMECOLOR_NORMAL_PC];

		for (UINT uEmpire=1; uEmpire<EMPIRE_NUM; ++uEmpire)
			g_akD3DXClrName[NAMECOLOR_PC+uEmpire]=g_akD3DXClrName[NAMECOLOR_EMPIRE_PC+uEmpire];

	}
	else
	{
		g_akD3DXClrName[NAMECOLOR_MOB]=g_akD3DXClrName[NAMECOLOR_NORMAL_MOB];
		g_akD3DXClrName[NAMECOLOR_NPC]=g_akD3DXClrName[NAMECOLOR_NORMAL_NPC];

		for (UINT uEmpire=0; uEmpire<EMPIRE_NUM; ++uEmpire)
			g_akD3DXClrName[NAMECOLOR_PC+uEmpire]=g_akD3DXClrName[NAMECOLOR_NORMAL_PC];
	}
}

const D3DXCOLOR& CInstanceBase::GetIndexedNameColor(UINT eNameColor)
{
	if (eNameColor>=NAMECOLOR_NUM)
	{
		static D3DXCOLOR s_kD3DXClrNameDefault(0xffffffff);
		return s_kD3DXClrNameDefault;
	}

	return g_akD3DXClrName[eNameColor];
}

void CInstanceBase::AddDamageEffect(uint32_t damage, uint8_t flag, bool bSelf, bool bTarget)
{
	if (!CPythonSystem::Instance().IsShowDamage())
		return;

	SEffectDamage sDamage;
	sDamage.bSelf = bSelf;
	sDamage.bTarget = bTarget;
	sDamage.damage = damage;
	sDamage.flag = flag;

	m_DamageQueue.emplace_back(sDamage);
}

bool CInstanceBase::__ProcessSingleDamage(const SEffectDamage& sDamage)
{
	CCamera* pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return false;

	const uint32_t damage = sDamage.damage;
	const uint8_t flag = sDamage.flag;
	const bool bSelf = sDamage.bSelf;
	const bool bTarget = sDamage.bTarget;

	const float cameraAngle = GetDegreeFromPosition2(
		pCamera->GetTarget().x, pCamera->GetTarget().y,
		pCamera->GetEye().x, pCamera->GetEye().y);

	static constexpr float FONT_WIDTH = 35.0f;

	CEffectManager& rkEftMgr = CEffectManager::Instance();

	D3DXVECTOR3 v3Pos = m_GraphicThingInstance.GetPosition();
	v3Pos.z += static_cast<float>(m_GraphicThingInstance.GetHeight());

	const D3DXVECTOR3 v3Rot(0.0f, 0.0f, cameraAngle);

	// Dodge / Block -> Miss
	if ((flag & DAMAGE_DODGE) || (flag & DAMAGE_BLOCK))
	{
		rkEftMgr.CreateEffect(
			ms_adwCRCAffectEffect[bSelf ? EFFECT_DAMAGE_MISS : EFFECT_DAMAGE_TARGETMISS],
			v3Pos,
			v3Rot);
		return true;
	}

#ifndef DISABLE_CRITICAL_EFFECT
	if (flag & DAMAGE_CRITICAL)
	{
		rkEftMgr.CreateEffect(
			ms_adwCRCAffectEffect[EFFECT_DAMAGE_CRITICAL],
			v3Pos,
			v3Rot);
	}
#endif

	std::string strDamageType;
	uint32_t rdwCRCEft = 0;

	if (bSelf)
	{
		strDamageType = "damage_";
		rdwCRCEft = (m_bDamageEffectType == 0) ? EFFECT_DAMAGE_SELFDAMAGE : EFFECT_DAMAGE_SELFDAMAGE2;
		m_bDamageEffectType = !m_bDamageEffectType;
	}
	else if (!bTarget)
	{
		strDamageType = "nontarget_";
		rdwCRCEft = EFFECT_DAMAGE_NOT_TARGET;

		// Meghagyom az eredeti viselkedést:
		// nontarget esetén ne rajzoljon számot.
		return true;
	}
	else
	{
		if (flag & DAMAGE_CRITICAL)
			strDamageType = "crit_target_";
		else if (flag & DAMAGE_PENETRATE)
			strDamageType = "traf_target_";
		else
			strDamageType = "target_";

		rdwCRCEft = EFFECT_DAMAGE_TARGET;
	}

	struct DamageSegment
	{
		uint32_t value;
		const char* suffix; // "m", "k", vagy ""
	};

	std::vector<DamageSegment> segments;
	segments.reserve(3);

	uint32_t remaining = damage;

	if (remaining >= 1000000)
	{
		segments.push_back({ remaining / 1000000, "m" });
		remaining %= 1000000;
	}

	if (remaining >= 1000)
	{
		segments.push_back({ remaining / 1000, "k" });
		remaining %= 1000;
	}

	if (remaining > 0 || segments.empty())
	{
		segments.push_back({ remaining, "" });
	}

	const std::string basePath = "d:/ymir work/effect/affect/damagevalue/" + strDamageType;

	auto CreateDigitEffectAtIndex = [&](const std::string& texturePath, uint32_t index)
		{
			std::vector<std::string> textures;
			textures.reserve(1);
			textures.emplace_back(texturePath);

			rkEftMgr.SetEffectTextures(ms_adwCRCAffectEffect[rdwCRCEft], textures);

			D3DXMATRIX matrix;
			D3DXMATRIX matTrans;

			D3DXMatrixTranslation(&matrix, v3Pos.x, v3Pos.y, v3Pos.z);
			D3DXMatrixMultiply(&matrix, &pCamera->GetInverseViewMatrix(), &matrix);

			D3DXMatrixTranslation(&matTrans, -(FONT_WIDTH * index), 0.0f, 0.0f);
			matrix = matTrans * matrix;

			D3DXMatrixMultiply(&matrix, &pCamera->GetViewMatrix(), &matrix);

			auto iEffectInstance = rkEftMgr.CreateEffect(
				ms_adwCRCAffectEffect[rdwCRCEft],
				D3DXVECTOR3(matrix._41, matrix._42, matrix._43),
				v3Rot);
		
			/*if (iEffectInstance >= 0)
			{
				CEffectInstance* pEff = nullptr; 
				pEff->SetTimeScale(0.80f);
			}*/

		};

	uint32_t index = 0;

	// Jobbról balra renderelünk:
	// maradék -> k -> m
	for (auto it = segments.rbegin(); it != segments.rend(); ++it)
	{
		const DamageSegment& seg = *it;

		// pl. 2,000,000 esetén ne jelenjen meg 0k
		if (seg.value == 0 && seg.suffix[0] != '\0')
			continue;

		// suffix (k / m)
		if (seg.suffix[0] != '\0')
		{
			if (index >= 20)
			{
				TraceError("CInstanceBase::__ProcessSingleDamage - too many damage glyphs");
				break;
			}

			CreateDigitEffectAtIndex(basePath + seg.suffix + std::string(".dds"), index);
			++index;
		}

		uint32_t val = seg.value;

		do
		{
			if (index >= 20)
			{
				TraceError("CInstanceBase::__ProcessSingleDamage - too many damage glyphs");
				break;
			}

			const uint32_t num = val % 10;
			val /= 10;

			char numBuf[16];
			snprintf(numBuf, sizeof(numBuf), "%u.dds", num);

			CreateDigitEffectAtIndex(basePath + numBuf, index);
			++index;
		} while (val > 0);
	}

	return true;
}

bool CInstanceBase::ProcessDamage()
{
	if (m_DamageQueue.empty())
		return false;

	bool bProcessed = false;

	// Régi hiba: csak 1 elemet dolgozott fel / hívás.
	// Új: batch-ben több elemet kezelünk ugyanabban a frame-ben.
	static constexpr size_t kMaxDamageProcessPerFrame = 20;

	size_t processedCount = 0;

	while (!m_DamageQueue.empty() && processedCount < kMaxDamageProcessPerFrame)
	{
		const SEffectDamage sDamage = m_DamageQueue.front();
		m_DamageQueue.pop_front();

		if (__ProcessSingleDamage(sDamage))
			bProcessed = true;

		++processedCount;
	}

	return bProcessed;
}

void CInstanceBase::AttachSpecialEffect(uint32_t effect)
{
	__AttachEffect(effect);
}

void CInstanceBase::LevelUp()
{
	__AttachEffect(EFFECT_LEVELUP);
}

void CInstanceBase::SkillUp()
{
	__AttachEffect(EFFECT_SKILLUP);
}

void CInstanceBase::CreateSpecialEffect(uint32_t iEffectIndex)
{
	const D3DXMATRIX & c_rmatGlobal = m_GraphicThingInstance.GetTransform();

	uint32_t dwEffectIndex = CEffectManager::Instance().GetEmptyIndex();
	uint32_t dwEffectCRC = ms_adwCRCAffectEffect[iEffectIndex];
	CEffectManager::Instance().CreateEffectInstance(dwEffectIndex, dwEffectCRC);
	CEffectManager::Instance().SelectEffectInstance(dwEffectIndex);
	CEffectManager::Instance().SetEffectInstanceGlobalMatrix(c_rmatGlobal);
}

void CInstanceBase::__EffectContainer_Destroy()
{
	SEffectContainer::Dict& rkDctEftID=__EffectContainer_GetDict();

	SEffectContainer::Dict::iterator i;
	for (i=rkDctEftID.begin(); i!=rkDctEftID.end(); ++i)
		__DetachEffect(i->second);

	rkDctEftID.clear();
}

void CInstanceBase::__EffectContainer_Initialize()
{
	SEffectContainer::Dict& rkDctEftID=__EffectContainer_GetDict();
	rkDctEftID.clear();
}

CInstanceBase::SEffectContainer::Dict& CInstanceBase::__EffectContainer_GetDict()
{
	return m_kEffectContainer.m_kDct_dwEftID;
}

// Return value ¸¦ boolean żˇĽ­ ID ·Î ąŮ˛ß´Ď´Ů
uint32_t CInstanceBase::__EffectContainer_AttachEffect(uint32_t dwEftKey)
{
	SEffectContainer::Dict& rkDctEftID=__EffectContainer_GetDict();
	SEffectContainer::Dict::iterator f=rkDctEftID.find(dwEftKey);
	if (rkDctEftID.end()!=f)
		return 0;

	uint32_t dwEftID=__AttachEffect(dwEftKey);
	rkDctEftID.insert(SEffectContainer::Dict::value_type(dwEftKey, dwEftID));
	return dwEftID;
}


void CInstanceBase::__EffectContainer_DetachEffect(uint32_t dwEftKey)
{
	SEffectContainer::Dict& rkDctEftID=__EffectContainer_GetDict();
	SEffectContainer::Dict::iterator f=rkDctEftID.find(dwEftKey);
	if (rkDctEftID.end()==f)
		return;

	__DetachEffect(f->second);

	rkDctEftID.erase(f);
}

#ifdef ENABLE_NO_COLLISION
bool CInstanceBase::__EffectContainer_IsEffect(uint32_t dwEftKey) {
	SEffectContainer::Dict & rkDctEftID = __EffectContainer_GetDict();
	return rkDctEftID.end() == rkDctEftID.find(dwEftKey) ? false : true;
}
#endif

void CInstanceBase::__AttachEmpireEffect(uint32_t eEmpire)
{
	if (!__IsExistMainInstance())
		return;

	CInstanceBase* pkInstMain=__GetMainInstancePtr();

	if (IsWarp())
		return;
	if (IsObject())
		return;
	if (IsFlag())
		return;
	if (IsResource())
		return;

#ifdef ENABLE_EMPIRE_EFFECT_LIMITED
	if (!IsPC() && IsNPC()) {
		return;
	}
#endif

	if (pkInstMain->IsGameMaster())
	{
	}
	else
	{
		if (pkInstMain->IsSameEmpire(*this))
			return;

		// HIDE_OTHER_EMPIRE_EUNHYEONG_ASSASSIN
		if (IsAffect(AFFECT_EUNHYEONG))
			return;
		// END_OF_HIDE_OTHER_EMPIRE_EUNHYEONG_ASSASSIN
	}

	if (IsGameMaster())
		return;

	__EffectContainer_AttachEffect(EFFECT_EMPIRE+eEmpire);
}

void CInstanceBase::__AttachSelectEffect()
{
	__EffectContainer_AttachEffect(EFFECT_SELECT);
}

void CInstanceBase::__DetachSelectEffect()
{
	__EffectContainer_DetachEffect(EFFECT_SELECT);
}

void CInstanceBase::__AttachTargetEffect()
{
	__EffectContainer_AttachEffect(EFFECT_TARGET);
}

void CInstanceBase::__DetachTargetEffect()
{
	__EffectContainer_DetachEffect(EFFECT_TARGET);
}


void CInstanceBase::__StoneSmoke_Inialize()
{
	m_kStoneSmoke.m_dwEftID=0;
}

void CInstanceBase::__StoneSmoke_Destroy()
{
	if (!m_kStoneSmoke.m_dwEftID)
		return;

	__DetachEffect(m_kStoneSmoke.m_dwEftID);
	m_kStoneSmoke.m_dwEftID=0;
}

void CInstanceBase::__StoneSmoke_Create(uint32_t eSmoke)
{
	m_kStoneSmoke.m_dwEftID=m_GraphicThingInstance.AttachSmokeEffect(eSmoke);
}

void CInstanceBase::SetAlpha(float fAlpha)
{
	__SetBlendRenderingMode();
	__SetAlphaValue(fAlpha);
}

bool CInstanceBase::UpdateDeleting()
{
	Update();
	Transform();

	IAbstractApplication& rApp=IAbstractApplication::GetSingleton();

	float fAlpha = __GetAlphaValue() - (rApp.GetGlobalElapsedTime() * 1.5f);
	__SetAlphaValue(fAlpha);

	if (fAlpha < 0.0f)
		return false;

	return true;
}

void CInstanceBase::DeleteBlendOut()
{
	__SetBlendRenderingMode();
	__SetAlphaValue(1.0f);
	DetachTextTail();

	IAbstractPlayer& rkPlayer=IAbstractPlayer::GetSingleton();
	rkPlayer.NotifyDeletingCharacterInstance(GetVirtualID());
}

void CInstanceBase::ClearPVPKeySystem()
{
	g_kSet_dwPVPReadyKey.clear();
	g_kSet_dwPVPKey.clear();
	g_kSet_dwGVGKey.clear();
	g_kSet_dwDUELKey.clear();
}

void CInstanceBase::InsertPVPKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwPVPKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	g_kSet_dwPVPKey.insert(dwPVPKey);
}

void CInstanceBase::InsertPVPReadyKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwPVPReadyKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	g_kSet_dwPVPKey.insert(dwPVPReadyKey);
}

void CInstanceBase::RemovePVPKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwPVPKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	g_kSet_dwPVPKey.erase(dwPVPKey);
}

void CInstanceBase::InsertGVGKey(uint32_t dwSrcGuildVID, uint32_t dwDstGuildVID)
{
	uint32_t dwGVGKey = __GetPVPKey(dwSrcGuildVID, dwDstGuildVID);
	g_kSet_dwGVGKey.insert(dwGVGKey);
}

void CInstanceBase::RemoveGVGKey(uint32_t dwSrcGuildVID, uint32_t dwDstGuildVID)
{
	uint32_t dwGVGKey = __GetPVPKey(dwSrcGuildVID, dwDstGuildVID);
	g_kSet_dwGVGKey.erase(dwGVGKey);
}

void CInstanceBase::InsertDUELKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwPVPKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	g_kSet_dwDUELKey.insert(dwPVPKey);
}

uint32_t CInstanceBase::__GetPVPKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	if (dwVIDSrc>dwVIDDst)
		std::swap(dwVIDSrc, dwVIDDst);

	uint32_t awSrc[2];
	awSrc[0]=dwVIDSrc;
	awSrc[1]=dwVIDDst;

    const uint8_t* s = (const uint8_t*) awSrc;
    const uint8_t* end = s + sizeof(awSrc);
    unsigned long h = 0;

    while (s < end)
    {
        h *= 16777619;
        h ^= (uint8_t) *(uint8_t*) (s++);
    }

    return h;
}

bool CInstanceBase::__FindPVPKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwPVPKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	if (g_kSet_dwPVPKey.end()==g_kSet_dwPVPKey.find(dwPVPKey))
		return false;

	return true;
}

bool CInstanceBase::__FindPVPReadyKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwPVPKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	if (g_kSet_dwPVPReadyKey.end()==g_kSet_dwPVPReadyKey.find(dwPVPKey))
		return false;

	return true;
}
//±ćµĺŔü˝Ă »ó´ë ±ćµĺŔÎÁö Č®ŔÎÇŇ¶§.
bool CInstanceBase::__FindGVGKey(uint32_t dwSrcGuildID, uint32_t dwDstGuildID)
{
	uint32_t dwGVGKey=__GetPVPKey(dwSrcGuildID, dwDstGuildID);

	if (g_kSet_dwGVGKey.end()==g_kSet_dwGVGKey.find(dwGVGKey))
		return false;

	return true;
}
//´ë·Ă ¸đµĺżˇĽ­´Â ´ë·Ă »ó´ë¸¸ °ř°ÝÇŇ Ľö ŔÖ´Ů.
bool CInstanceBase::__FindDUELKey(uint32_t dwVIDSrc, uint32_t dwVIDDst)
{
	uint32_t dwDUELKey=__GetPVPKey(dwVIDSrc, dwVIDDst);

	if (g_kSet_dwDUELKey.end()==g_kSet_dwDUELKey.find(dwDUELKey))
		return false;

	return true;
}

bool CInstanceBase::IsPVPInstance(CInstanceBase& rkInstSel)
{
	uint32_t dwVIDSrc=GetVirtualID();
	uint32_t dwVIDDst=rkInstSel.GetVirtualID();

	uint32_t dwGuildIDSrc=GetGuildID();
	uint32_t dwGuildIDDst=rkInstSel.GetGuildID();

	if (GetDuelMode())	//´ë·Ă ¸đµĺŔĎ¶§´Â ~_~
		return true;

	return __FindPVPKey(dwVIDSrc, dwVIDDst) || __FindGVGKey(dwGuildIDSrc, dwGuildIDDst);
											//__FindDUELKey(dwVIDSrc, dwVIDDst);
}

const D3DXCOLOR& CInstanceBase::GetNameColor()
{
	return GetIndexedNameColor(GetNameColorIndex());
}

UINT CInstanceBase::GetNameColorIndex()
{
	if (IsPC())
	{
		if (m_isKiller)
		{
			return NAMECOLOR_PK;
		}

		if (__IsExistMainInstance() && !__IsMainInstance())
		{
			CInstanceBase* pkInstMain=__GetMainInstancePtr();
			if (!pkInstMain)
			{
				TraceError("CInstanceBase::GetNameColorIndex - MainInstance is NULL");
				return NAMECOLOR_PC;
			}
			uint32_t dwVIDMain=pkInstMain->GetVirtualID();
			uint32_t dwVIDSelf=GetVirtualID();

			if (pkInstMain->GetDuelMode())
			{
				switch(pkInstMain->GetDuelMode())
				{
				case DUEL_CANNOTATTACK:
					return NAMECOLOR_PC + GetEmpireID();
				case DUEL_START:
					if(__FindDUELKey(dwVIDMain, dwVIDSelf))
						return NAMECOLOR_PVP;
					else
						return NAMECOLOR_PC + GetEmpireID();
				}
			}

			if (pkInstMain->IsSameEmpire(*this))
			{
				if (__FindPVPKey(dwVIDMain, dwVIDSelf))
				{
					return NAMECOLOR_PVP;
				}

				uint32_t dwGuildIDMain=pkInstMain->GetGuildID();
				uint32_t dwGuildIDSelf=GetGuildID();
				if (__FindGVGKey(dwGuildIDMain, dwGuildIDSelf))
				{
					return NAMECOLOR_PVP;
				}
				/*
				if (__FindDUELKey(dwVIDMain, dwVIDSelf))
				{
					return NAMECOLOR_PVP;
				}
				*/
			}
			else
			{
				return NAMECOLOR_PVP;
			}
		}

		IAbstractPlayer& rPlayer=IAbstractPlayer::GetSingleton();
		if (rPlayer.IsPartyMemberByVID(GetVirtualID()))
			return NAMECOLOR_PARTY;

		return NAMECOLOR_PC + GetEmpireID();

	}
	else if (IsNPC())
	{
		return NAMECOLOR_NPC;
	}
	else if (IsEnemy())
	{
		return NAMECOLOR_MOB;
	}
	else if (IsPoly())
	{
		return NAMECOLOR_MOB;
	}


	return D3DXCOLOR(0xffffffff);
}

const D3DXCOLOR& CInstanceBase::GetTitleColor()
{
	uint32_t uGrade = GetAlignmentGrade();
	if ( uGrade >= TITLE_NUM)
	{
		static D3DXCOLOR s_kD3DXClrTitleDefault(0xffffffff);
		return s_kD3DXClrTitleDefault;
	}

	return g_akD3DXClrTitle[uGrade];
}

void CInstanceBase::AttachTextTail()
{
	Tracef("AttachTextTail called for VID: %d", GetVirtualID());

	if (m_isTextTail)
	{
		TraceError("CInstanceBase::AttachTextTail - VID [%d] ALREADY EXIST", GetVirtualID());
		return;
	}

	m_isTextTail = true;

	uint32_t dwVID=GetVirtualID();
#ifdef ENABLE_RACE_HEIGHT
	float fTextTailHeight = GetBaseHeight() + 10.0f;
#else
	float fTextTailHeight=IsMountingHorse() ? 110.0f : 10.0f;
#endif
	static D3DXCOLOR s_kD3DXClrTextTail=D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	CPythonTextTail::Instance().RegisterCharacterTextTail(m_dwGuildID, dwVID, s_kD3DXClrTextTail, fTextTailHeight, IsPC());

	// CHARACTER_LEVEL
	if (m_dwLevel)
	{
		UpdateTextTailLevel(m_dwLevel);
	}

}

void CInstanceBase::DetachTextTail()
{
	if (!m_isTextTail)
		return;

	m_isTextTail=false;
	CPythonTextTail::Instance().DeleteCharacterTextTail(GetVirtualID());
}

void CInstanceBase::UpdateTextTailLevel(uint32_t level)
{
	//static D3DXCOLOR s_kLevelColor = D3DXCOLOR(119.0f/255.0f, 246.0f/255.0f, 168.0f/255.0f, 1.0f);
	static D3DXCOLOR s_kLevelColor = D3DXCOLOR(152.0f/255.0f, 255.0f/255.0f, 51.0f/255.0f, 1.0f);

	char szText[256];
#ifdef NEW_PET_SYSTEM
	static D3DXCOLOR s_kPetLevelColor = D3DCOLOR_XRGB(255, 255, 0);
	if (IsNewPet())
	{
		sprintf(szText, "[Lv.%d] ", level);
		CPythonTextTail::Instance().AttachLevel(GetVirtualID(), szText, s_kPetLevelColor);
	}
	else
	{
		sprintf(szText, "[Lv.%d] ", level);
		CPythonTextTail::Instance().AttachLevel(GetVirtualID(), szText, s_kLevelColor);		
	}
#else
	sprintf(szText, "[Lv.%d] ", level);
	CPythonTextTail::Instance().AttachLevel(GetVirtualID(), szText, s_kLevelColor);
#endif
}


void CInstanceBase::RefreshTextTail()
{
	CPythonTextTail::Instance().SetCharacterTextTailColor(GetVirtualID(), GetNameColor());

	uint32_t iAlignmentGrade = GetAlignmentGrade();
	
	{
#ifdef ENABLE_GENDER_ALIGNMENT
		const std::map<uint8_t, std::vector<std::string>>::const_iterator itor = g_TitleNameMap.find(iAlignmentGrade);
#else
		std::map<int, std::string>::iterator itor = g_TitleNameMap.find(iAlignmentGrade);
#endif
		if (g_TitleNameMap.end() != itor)
		{
#ifdef ENABLE_GENDER_ALIGNMENT
			const uint8_t bSex = RaceToSex(GetRace());
			const std::string & c_rstrTitleName = itor->second.at(bSex);
#else
			const std::string & c_rstrTitleName = itor->second;
#endif
			CPythonTextTail::Instance().AttachTitle(GetVirtualID(), c_rstrTitleName.c_str(), GetTitleColor());
		}
	}
}

void CInstanceBase::RefreshTextTailTitle()
{
	RefreshTextTail();
}

// 2004.07.25.myevan.ŔĚĆĺĆ® ľČ şŮ´Â ą®Á¦ ÇŘ°á
/////////////////////////////////////////////////
void CInstanceBase::__ClearAffectFlagContainer()
{
	m_kAffectFlagContainer.Clear();
}

void CInstanceBase::__ClearAffects()
{
#ifdef ENABLE_MELEY_LAIR_DUNGEON
	if ((IsStone()) && (GetVirtualNumber() != MELEY_LAIR_DUNGEON_STATUE))
#else
	if (IsStone())
#endif
	{
		__StoneSmoke_Destroy();
	}
	else
	{
		if (m_kWarrior.m_dwGeomgyeongEffect)
		{
			__DetachEffect(m_kWarrior.m_dwGeomgyeongEffect);
			m_kWarrior.m_dwGeomgyeongEffect = 0;
		}
		
		for (int iAffect=0; iAffect<AFFECT_NUM; ++iAffect)
		{
			__DetachEffect(m_adwCRCAffectEffect[iAffect]);
			m_adwCRCAffectEffect[iAffect]=0;
		}
		
		__ClearAffectFlagContainer();
	}

	m_GraphicThingInstance.__OnClearAffects();
}

/////////////////////////////////////////////////

void CInstanceBase::__SetNormalAffectFlagContainer(const CAffectFlagContainer& c_rkAffectFlagContainer)
{
	for (uint32_t i=0; i<CAffectFlagContainer::BIT_SIZE; ++i)
	{
		bool isOldSet=m_kAffectFlagContainer.IsSet(i);
		bool isNewSet=c_rkAffectFlagContainer.IsSet(i);

		if (isOldSet != isNewSet)
		{
			__SetAffect(i, isNewSet);

			if (isNewSet)
				m_GraphicThingInstance.__OnSetAffect(i);
			else
				m_GraphicThingInstance.__OnResetAffect(i);
		}
	}

	m_kAffectFlagContainer.CopyInstance(c_rkAffectFlagContainer);
}

void CInstanceBase::__SetStoneSmokeFlagContainer(const CAffectFlagContainer& c_rkAffectFlagContainer)
{
	m_kAffectFlagContainer.CopyInstance(c_rkAffectFlagContainer);

	uint32_t eSmoke;
	if (m_kAffectFlagContainer.IsSet(STONE_SMOKE8))
		eSmoke = 3;
	else if (m_kAffectFlagContainer.IsSet(STONE_SMOKE5) || m_kAffectFlagContainer.IsSet(STONE_SMOKE6) ||
		m_kAffectFlagContainer.IsSet(STONE_SMOKE7))
		eSmoke = 2;
	else if (m_kAffectFlagContainer.IsSet(STONE_SMOKE2) || m_kAffectFlagContainer.IsSet(STONE_SMOKE3) ||
		m_kAffectFlagContainer.IsSet(STONE_SMOKE4))
		eSmoke = 1;
	else
		eSmoke = 0;

	__StoneSmoke_Destroy();
	__StoneSmoke_Create(eSmoke);
}

void CInstanceBase::SetAffectFlagContainer(const CAffectFlagContainer& c_rkAffectFlagContainer)
{
	if (IsBuilding())
	{
		return;
	}
#ifdef ENABLE_MELEY_LAIR_DUNGEON
	else if ((IsStone()) && (GetVirtualNumber() != MELEY_LAIR_DUNGEON_STATUE))
#else
	else if (IsStone())
#endif
	{
		__SetStoneSmokeFlagContainer(c_rkAffectFlagContainer);
	}
	else
	{
		__SetNormalAffectFlagContainer(c_rkAffectFlagContainer);
	}
}


void CInstanceBase::SCRIPT_SetAffect(UINT eAffect, bool isVisible)
{
	__SetAffect(eAffect, isVisible);
}

void CInstanceBase::__SetReviveInvisibilityAffect(bool isVisible)
{
	if (isVisible)
	{
		// NOTE : Dress ¸¦ ŔÔ°í ŔÖŔ¸¸é Alpha ¸¦ łÖÁö ľĘ´Â´Ů.
		if (IsWearingDress())
			return;

		m_GraphicThingInstance.BlendAlphaValue(0.5f, 1.0f);
	}
	else
	{
		m_GraphicThingInstance.BlendAlphaValue(1.0f, 1.0f);
	}
}

void CInstanceBase::__Assassin_SetEunhyeongAffect(bool isVisible)
{
	if (isVisible)
	{
		// NOTE : Dress ¸¦ ŔÔ°í ŔÖŔ¸¸é Alpha ¸¦ łÖÁö ľĘ´Â´Ů.
		if (IsWearingDress())
			return;

		if (__IsMainInstance() || __MainCanSeeHiddenThing())
		{
			m_GraphicThingInstance.BlendAlphaValue(0.5f, 1.0f);
		}
		else
		{
			// 2004.10.16.myevan.ŔşÇüąý żĎŔü Ĺő¸í
			m_GraphicThingInstance.BlendAlphaValue(0.0f, 1.0f);
			m_GraphicThingInstance.HideAllAttachingEffect();
		}
	}
	else
	{
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
		if (IsAffect(AFFECT_INVISIBILITY) && __MainCanSeeHiddenThing())
			return;
#endif
		m_GraphicThingInstance.BlendAlphaValue(1.0f, 1.0f);
		m_GraphicThingInstance.ShowAllAttachingEffect();
	}
}

void CInstanceBase::__Shaman_SetParalysis(bool isParalysis)
{
	m_GraphicThingInstance.SetParalysis(isParalysis);
}



void CInstanceBase::__Warrior_SetGeomgyeongAffect(bool isVisible)
{
	if (isVisible)
	{
		if (IsWearingDress())
			return;

		if (m_kWarrior.m_dwGeomgyeongEffect)
			__DetachEffect(m_kWarrior.m_dwGeomgyeongEffect);

		m_GraphicThingInstance.SetReachScale(1.5f);
		if (m_GraphicThingInstance.IsTwoHandMode())
			m_kWarrior.m_dwGeomgyeongEffect=__AttachEffect(EFFECT_WEAPON+WEAPON_TWOHAND);
		else
			m_kWarrior.m_dwGeomgyeongEffect=__AttachEffect(EFFECT_WEAPON+WEAPON_ONEHAND);
	}
	else
	{
		m_GraphicThingInstance.SetReachScale(1.0f);

		__DetachEffect(m_kWarrior.m_dwGeomgyeongEffect);
		m_kWarrior.m_dwGeomgyeongEffect=0;
	}
}

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
void CInstanceBase::__Assassin_SetGyeongGongAffect(bool isVisible)
{
	if (isVisible)
	{
		if (IsWearingDress())
			return;

		if (m_kAssassin.m_dwGyeongGongEffect)
			__DetachEffect(m_kAssassin.m_dwGyeongGongEffect);

		m_GraphicThingInstance.SetReachScale(1.5f);
		m_kAssassin.m_dwGyeongGongEffect = __AttachEffect(EFFECT_GYEONGGONG_BOOM);
	}
	else
	{
		m_GraphicThingInstance.SetReachScale(1.0f);

		__DetachEffect(m_kAssassin.m_dwGyeongGongEffect);
		m_kAssassin.m_dwGyeongGongEffect = 0;
	}
}
#endif

void CInstanceBase::__SetAffect(UINT eAffect, bool isVisible)
{
	switch (eAffect)
	{
		case AFFECT_YMIR:
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
			if (IsAffect(AFFECT_INVISIBILITY) && !__MainCanSeeHiddenThing())
				return;
#else
			if (IsAffect(AFFECT_INVISIBILITY))
				return;
#endif
			break;
/*
		case AFFECT_GWIGEOM: // Ŕü±â ĽÓĽş °ř°ÝŔ¸·Î ąŮ˛đ żąÁ¤
			if (isVisible)
			{
				m_GraphicThingInstance.SetBattleHitEffect(ms_adwCRCAffectEffect[EFFECT_ELECTRIC_HIT]);
				m_GraphicThingInstance.SetBattleAttachEffect(ms_adwCRCAffectEffect[EFFECT_ELECTRIC_ATTACH]);
			}
			else
			{
				m_GraphicThingInstance.SetBattleHitEffect(ms_adwCRCAffectEffect[EFFECT_HIT]);
				m_GraphicThingInstance.SetBattleAttachEffect(0);
			}
			return;
			break;
		case AFFECT_HWAYEOM: // Č­ż° ĽÓĽş °ř°ÝŔ¸·Î ąŮ˛đ żąÁ¤
			if (isVisible)
			{
				m_GraphicThingInstance.SetBattleHitEffect(ms_adwCRCAffectEffect[EFFECT_FLAME_HIT]);
				m_GraphicThingInstance.SetBattleAttachEffect(ms_adwCRCAffectEffect[EFFECT_FLAME_ATTACH]);
			}
			else
			{
				m_GraphicThingInstance.SetBattleHitEffect(ms_adwCRCAffectEffect[EFFECT_HIT]);
				m_GraphicThingInstance.SetBattleAttachEffect(0);
			}
			// Č­ż°ÂüŔş °ř°ÝÇŇ ¶§¸¸ ŔĎ˝ĂŔűŔ¸·Î Visible ÇŐ´Ď´Ů.
			return;
			break;
*/
		case AFFECT_CHEONGEUN:
			m_GraphicThingInstance.SetResistFallen(isVisible);
			break;
		case AFFECT_GEOMGYEONG:
			__Warrior_SetGeomgyeongAffect(isVisible);
			return;
			break;
		case AFFECT_REVIVE_INVISIBILITY:
			__Assassin_SetEunhyeongAffect(isVisible);
			break;
		case AFFECT_EUNHYEONG:
			__Assassin_SetEunhyeongAffect(isVisible);
			break;
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
		case AFFECT_GYEONGGONG:
			if (isVisible)
			{
				__Assassin_SetGyeongGongAffect(isVisible);
				if (!IsWalking())
					return;
			}
			break;
		case AFFECT_KWAESOK:
			// °ć°řĽú, ÄčĽÓŔş ¶Ű¶§¸¸ Attaching ˝ĂĹµ´Ď´Ů. - [levites]
			if (isVisible)
				if (!IsWalking())
					return;
			break;
#else
		case AFFECT_GYEONGGONG:
		case AFFECT_KWAESOK:
			// °ć°řĽú, ÄčĽÓŔş ¶Ű¶§¸¸ Attaching ˝ĂĹµ´Ď´Ů. - [levites]
			if (isVisible)
				if (!IsWalking())
					return;
			break;
#endif
		case AFFECT_INVISIBILITY:
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
			if (__MainCanSeeHiddenThing())
			{
				if (isVisible)
					m_GraphicThingInstance.BlendAlphaValue(0.5f, 1.0f);
				else
					m_GraphicThingInstance.BlendAlphaValue(1.0f, 1.0f);
				break;
			}
#endif
			// 2004.07.17.levites.isShow¸¦ ViewFrustumCheck·Î şŻ°ć
			if (isVisible)
			{
#ifdef ENABLE_NEW_BUGFIXES
				m_GraphicThingInstance.HideAllAttachingEffect();
#else
				m_GraphicThingInstance.ClearAttachingEffect();
#endif
				__EffectContainer_Destroy();
				DetachTextTail();
			}
			else
			{
#ifdef ENABLE_NEW_BUGFIXES
				m_GraphicThingInstance.ShowAllAttachingEffect();
#endif
				m_GraphicThingInstance.BlendAlphaValue(1.0f, 1.0f);
				AttachTextTail();
				RefreshTextTail();
			}
			return;
			break;
//		case AFFECT_FAINT:
//			m_GraphicThingInstance.SetFaint(isVisible);
//			break;
//		case AFFECT_SLEEP:
//			m_GraphicThingInstance.SetSleep(isVisible);
//			break;
		case AFFECT_STUN:
			m_GraphicThingInstance.SetSleep(isVisible);
			break;
	}

	if (eAffect>=AFFECT_NUM)
	{
		TraceError("CInstanceBase[VID:%d]::SetAffect(eAffect:%d<AFFECT_NUM:%d, isVisible=%d)", GetVirtualID(), eAffect, isVisible);
		return;
	}

	if (isVisible)
	{
		if (!m_adwCRCAffectEffect[eAffect])
		{
			m_adwCRCAffectEffect[eAffect]=__AttachEffect(EFFECT_AFFECT+eAffect);
		}
	}
	else
	{
		if (m_adwCRCAffectEffect[eAffect])
		{
			__DetachEffect(m_adwCRCAffectEffect[eAffect]);
			m_adwCRCAffectEffect[eAffect]=0;
		}
	}
}

bool CInstanceBase::IsPossibleEmoticon()
{
	CEffectManager& rkEftMgr=CEffectManager::Instance();
	for(uint32_t eEmoticon = 0; eEmoticon < EMOTICON_NUM; eEmoticon++)
	{
		uint32_t effectID = ms_adwCRCAffectEffect[EFFECT_EMOTICON+eEmoticon];
		if( effectID &&	rkEftMgr.IsAliveEffect(effectID) )
			return false;
	}
	
	if(ELTimer_GetMSec() - m_dwEmoticonTime < 1000)
	{
		//TraceError("ELTimer_GetMSec() - m_dwEmoticonTime");
		return false;
	}
	
	return true;
}

void CInstanceBase::SetFishEmoticon()
{
	SetEmoticon(EMOTICON_FISH);
}

void CInstanceBase::SetEmoticon(UINT eEmoticon)
{
	if (eEmoticon>=EMOTICON_NUM)
	{
		TraceError("CInstanceBase[VID:%d]::SetEmoticon(eEmoticon:%d<EMOTICON_NUM:%d, isVisible=%d)",
			GetVirtualID(), eEmoticon);
		return;
	}
	if (IsPossibleEmoticon())
	{
		D3DXVECTOR3 v3Pos = m_GraphicThingInstance.GetPosition();
#ifdef ENABLE_RACE_HEIGHT
		v3Pos.z += float(GetBaseHeight() + m_GraphicThingInstance.GetHeight());
#else
		v3Pos.z += float(m_GraphicThingInstance.GetHeight());
#endif
		//CEffectManager& rkEftMgr=CEffectManager::Instance();
		CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();

		D3DXVECTOR3 v3Dir = (pCamera->GetEye()-v3Pos)*9/10;
		v3Pos = pCamera->GetEye()-v3Dir;

		v3Pos = D3DXVECTOR3(0,0,0);
#ifdef ENABLE_RACE_HEIGHT
		v3Pos.z += float(GetBaseHeight() + m_GraphicThingInstance.GetHeight());
#else
		v3Pos.z += float(m_GraphicThingInstance.GetHeight());
#endif

		//rkEftMgr.CreateEffect(ms_adwCRCAffectEffect[EFFECT_EMOTICON+eEmoticon],v3Pos,D3DXVECTOR3(0,0,0));
		m_GraphicThingInstance.AttachEffectByID(0, nullptr, ms_adwCRCAffectEffect[EFFECT_EMOTICON+eEmoticon],&v3Pos);
		m_dwEmoticonTime = ELTimer_GetMSec();
	}
}

void CInstanceBase::SetDustGap(float fDustGap)
{
	ms_fDustGap=fDustGap;
}

void CInstanceBase::SetHorseDustGap(float fDustGap)
{
	ms_fHorseDustGap=fDustGap;
}

void CInstanceBase::__DetachEffect(uint32_t dwEID)
{
	m_GraphicThingInstance.DettachEffect(dwEID);
}

uint32_t CInstanceBase::__AttachEffect(UINT eEftType)
{

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	switch (eEftType)
	{
		case EFFECT_AFFECT + AFFECT_GONGPO:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/sura/effect/fear_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_JUMAGAP:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/sura/effect/jumagap_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_HOSIN:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/shaman/effect/3hosin_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_BOHO:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/shaman/effect/boho_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_KWAESOK:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/shaman/effect/10kwaesok_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_HEUKSIN:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/sura/effect/heuksin_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_MUYEONG:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/sura/effect/muyeong_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_FIRE:
			RegisterEffect(eEftType, "Bip01", "d:/ymir work/effect/hit/blow_flame/flame_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_GICHEON:
			RegisterEffect(eEftType, "Bip01 R Hand", "d:/ymir work/pc/shaman/effect/6gicheon_hand.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_JEUNGRYEOK:
			RegisterEffect(eEftType, "Bip01 L Hand", "d:/ymir work/pc/shaman/effect/jeungryeok_hand.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_PABEOP:
			RegisterEffect(eEftType, "Bip01 Head", "d:/ymir work/pc/sura/effect/pabeop_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_CHEONGEUN:
		case EFFECT_AFFECT + AFFECT_FALLEN_CHEONGEUN:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/warrior/effect/gyeokgongjang_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_GWIGEOM:
			RegisterEffect(eEftType, "Bip01 R Finger2", "d:/ymir work/pc/sura/effect/gwigeom_loop.mse", false, GetNameString());
			break;
		case EFFECT_AFFECT + AFFECT_GYEONGGONG:
			RegisterEffect(eEftType, "", "d:/ymir work/pc/assassin/effect/gyeonggong_loop.mse", false, GetNameString());
			break;
		case EFFECT_WEAPON + WEAPON_ONEHAND:
			RegisterEffect(eEftType, "equip_right_hand", "d:/ymir work/pc/warrior/effect/geom_sword_loop.mse", false, GetNameString());
			break;
		case EFFECT_WEAPON + WEAPON_TWOHAND:
			RegisterEffect(eEftType, "equip_right_hand", "d:/ymir work/pc/warrior/effect/geom_spear_loop.mse", false, GetNameString());
			break;
		default:
			break;
	}
#endif

	// 2004.07.17.levites.isShow¸¦ ViewFrustumCheck·Î şŻ°ć
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
	if (IsAffect(AFFECT_INVISIBILITY) && !__MainCanSeeHiddenThing())
		return 0;
#else
	if (IsAffect(AFFECT_INVISIBILITY))
		return 0;
#endif

	if (eEftType>=EFFECT_NUM)
		return 0;

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	uint32_t * dwSkillColor = 
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	eEftType == EFFECT_GYEONGGONG_BOOM ? m_GraphicThingInstance.GetSkillColorByEffectID(EFFECT_AFFECT + AFFECT_GYEONGGONG) : m_GraphicThingInstance.GetSkillColorByEffectID(eEftType);
#else
	m_GraphicThingInstance.GetSkillColorByEffectID(eEftType)
#endif
	;
#endif

	if (ms_astAffectEffectAttachBone[eEftType].empty())
	{
//#ifdef ENABLE_RACE_HEIGHT
//		D3DXVECTOR3 v3Pos = m_GraphicThingInstance.GetPosition();
//		v3Pos.z += float(m_GraphicThingInstance.GetHeight());
//
//		CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
//		D3DXVECTOR3 v3Dir = (pCamera->GetEye()-v3Pos)*9/10;
//		v3Pos = pCamera->GetEye()-v3Dir;
//		v3Pos = D3DXVECTOR3(0,0,0);
//		v3Pos.z += float(GetBaseHeight());
//#endif
#ifdef ENABLE_SKILL_COLOR_SYSTEM
		//return m_GraphicThingInstance.AttachEffectByID(0, NULL, ms_adwCRCAffectEffect[eEftType], &v3Pos, dwSkillColor);
		return m_GraphicThingInstance.AttachEffectByID(0, nullptr, ms_adwCRCAffectEffect[eEftType], nullptr, dwSkillColor);
#else
		return m_GraphicThingInstance.AttachEffectByID(0, NULL, ms_adwCRCAffectEffect[eEftType]);
#endif
	}
	else
	{
		std::string & rstrBoneName = ms_astAffectEffectAttachBone[eEftType];
		const char * c_szBoneName;
		// ľçĽŐżˇ şŮŔĎ ¶§ »çżëÇŃ´Ů.
		// ŔĚ·± ˝ÄŔÇ żążÜ Ăł¸®¸¦ ÇŘłőŔş °ÍŔş Äł¸ŻĹÍ ¸¶´Ů Equip ŔÇ Bone Name ŔĚ ´Ů¸Ł±â ¶§ą®.
		if (0 == rstrBoneName.compare("PART_WEAPON"))
		{
			if (m_GraphicThingInstance.GetAttachingBoneName(CRaceData::PART_WEAPON, &c_szBoneName))
			{
#ifdef ENABLE_SKILL_COLOR_SYSTEM
				return m_GraphicThingInstance.AttachEffectByID(0, c_szBoneName, ms_adwCRCAffectEffect[eEftType], nullptr, dwSkillColor);
#else
				return m_GraphicThingInstance.AttachEffectByID(0, c_szBoneName, ms_adwCRCAffectEffect[eEftType]);
#endif
			}
		}
		else if (0 == rstrBoneName.compare("PART_WEAPON_LEFT"))
		{
			if (m_GraphicThingInstance.GetAttachingBoneName(CRaceData::PART_WEAPON_LEFT, &c_szBoneName))
			{
#ifdef ENABLE_SKILL_COLOR_SYSTEM
				return m_GraphicThingInstance.AttachEffectByID(0, c_szBoneName, ms_adwCRCAffectEffect[eEftType], nullptr, dwSkillColor);
#else
				return m_GraphicThingInstance.AttachEffectByID(0, c_szBoneName, ms_adwCRCAffectEffect[eEftType]);
#endif
			}
		}
		else
		{
#ifdef ENABLE_SKILL_COLOR_SYSTEM
			return m_GraphicThingInstance.AttachEffectByID(0, rstrBoneName.c_str(), ms_adwCRCAffectEffect[eEftType], nullptr, dwSkillColor);
#else
			return m_GraphicThingInstance.AttachEffectByID(0, rstrBoneName.c_str(), ms_adwCRCAffectEffect[eEftType]);
#endif
		}
	}

	return 0;
}

void CInstanceBase::__ComboProcess()
{
	/*
	uint32_t dwcurComboIndex = m_GraphicThingInstance.GetComboIndex();

	if (0 != dwcurComboIndex)
	{
		if (m_dwLastComboIndex != m_GraphicThingInstance.GetComboIndex())
		{
			if (!m_GraphicThingInstance.IsHandMode() & IsAffect(AFFECT_HWAYEOM))
			{
				__AttachEffect(EFFECT_FLAME_ATTACK);
			}
		}
	}

	m_dwLastComboIndex = dwcurComboIndex;
	*/
}

#ifdef ENABLE_SKILL_COLOR_SYSTEM
bool CInstanceBase::RegisterEffect(UINT eEftType, const char* c_szEftAttachBone, const char* c_szEftName, bool isCache, const char * name)
#else
bool CInstanceBase::RegisterEffect(UINT eEftType, const char* c_szEftAttachBone, const char* c_szEftName, bool isCache)
#endif
{
	if (eEftType>=EFFECT_NUM)
		return false;

	ms_astAffectEffectAttachBone[eEftType]=c_szEftAttachBone;

	uint32_t& rdwCRCEft=ms_adwCRCAffectEffect[eEftType];
#ifdef ENABLE_SKILL_COLOR_SYSTEM
	if (!CEffectManager::Instance().RegisterEffect2(c_szEftName, &rdwCRCEft, isCache, name))
#else
	if (!CEffectManager::Instance().RegisterEffect2(c_szEftName, &rdwCRCEft, isCache))
#endif
	{
		TraceError("CInstanceBase::RegisterEffect(eEftType=%d, c_szEftAttachBone=%s, c_szEftName=%s, isCache=%d) - Error", eEftType, c_szEftAttachBone, c_szEftName, isCache);
		rdwCRCEft=0;
		return false;
	}

	return true;
}

#ifdef ENABLE_GENDER_ALIGNMENT
void CInstanceBase::RegisterTitleName(const int iIndex, const char * c_szTitleNameF, const char * c_szTitleNameM)
{
	std::vector<std::string> vecTitleName{c_szTitleNameF, c_szTitleNameM};
	g_TitleNameMap.insert(std::make_pair(iIndex, vecTitleName));
}
#else
void CInstanceBase::RegisterTitleName(int iIndex, const char * c_szTitleName)
{
	g_TitleNameMap.insert(std::make_pair(iIndex, c_szTitleName));
}
#endif

D3DXCOLOR __RGBToD3DXColoru(UINT r, UINT g, UINT b)
{
	uint32_t dwColor=0xff;dwColor<<=8;
	dwColor|=r;dwColor<<=8;
	dwColor|=g;dwColor<<=8;
	dwColor|=b;

	return D3DXCOLOR(dwColor);
}

bool CInstanceBase::RegisterNameColor(UINT uIndex, UINT r, UINT g, UINT b)
{
	if (uIndex>=NAMECOLOR_NUM)
		return false;

	g_akD3DXClrName[uIndex]=__RGBToD3DXColoru(r, g, b);
	return true;
}

bool CInstanceBase::RegisterTitleColor(UINT uIndex, UINT r, UINT g, UINT b)
{
	if (uIndex>=TITLE_NUM)
		return false;

	g_akD3DXClrTitle[uIndex]=__RGBToD3DXColoru(r, g, b);
	return true;
}
#ifdef __AUTO_QUQUE_ATTACK__
void CInstanceBase::SetAutoFarmAffect(const bool bStatus)
{
	if (bStatus)
		__EffectContainer_AttachEffect(EFFECT_ACCE_SUCCEDED);
	else
		__EffectContainer_DetachEffect(EFFECT_ACCE_SUCCEDED);
}
#endif
