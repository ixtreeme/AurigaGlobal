#include "StdAfx.h"
#include "InstanceBase.h"
#include "PythonBackground.h"
#include "PythonNonPlayer.h"
#include "PythonPlayer.h"
#include "PythonCharacterManager.h"
#include "AbstractPlayer.h"
#include "AbstractApplication.h"
#include "packet.h"
#include "../Render/StateManager.h"
#include "../Game/ItemManager.h"
#include "../Game/GameLibDefines.h"
#ifdef WJ_SHOW_MOB_INFO
#include "PythonSystem.h"
#include "PythonTextTail.h"
#endif
#ifdef ENABLE_RACE_HEIGHT
#include "../Game/RaceManager.h"
#endif

bool HAIR_COLOR_ENABLE=false;
bool USE_ARMOR_SPECULAR=false;
bool RIDE_HORSE_ENABLE=true;
constexpr float c_fDefaultRotationSpeed = NORMAL_ROTATION_SPEED;
constexpr float c_fDefaultHorseRotationSpeed = MOUNT_ROTATION_SPEED;
#define ENABLE_NO_MOUNT_CHECK

typedef struct
{
	uint32_t	dwCodNPC;
	float		dwDimensiune;
} TDimensiuneNpc;

static const std::vector<TDimensiuneNpc> vectorDimensiuneNpc =
{
	//Mounturi - Start
	{ 14596 , 50.0f },
	{ 20206 , 20.0f },
	{ 20210 , 400.0f },
	{ 20203 , 50.0f },
	{ 20207 , 50.0f },
	{ 20211 , 50.0f },
	{ 20204 , 50.0f },
	{ 20208 , 50.0f },
	{ 20212 , 50.0f },
	{ 20226 , 50.0f },
	{ 20221 , 50.0f },
	{ 20110 , 50.0f },
	{ 20112 , 50.0f },
	{ 20120 , 50.0f },
	{ 20121 , 50.0f },
	{ 20123 , 50.0f },
	{ 20124 , 50.0f },
	{ 20125 , 50.0f },
	{ 20219 , 50.0f },
	{ 20220 , 50.0f },
	{ 20233 , 50.0f },
	{ 20234 , 50.0f },
	{ 20235 , 50.0f },
	{ 20236 , 50.0f },
	{ 20237 , 50.0f },
	{ 20239 , 50.0f },
	{ 20240 , 50.0f },
	{ 20241 , 50.0f },
	{ 20258 , 50.0f },
	{ 20259 , 50.0f },
	{ 20260 , 50.0f },
	{ 20263 , 50.0f },
	{ 20264 , 50.0f },
	{ 20267 , 50.0f },
	{ 22302 , 50.0f },
	{ 22303 , 50.0f },
	{ 22304 , 50.0f },
	//Mounturi - End

	//Npc-uri - Start
	{ 20084 , 100.0f }
	//Npc-uri - End
};

inline float DimeniuneNPC(const uint32_t dwRaceIndex)
{
	for (const auto & it : vectorDimensiuneNpc)
	{
		if (it.dwCodNPC == dwRaceIndex)
			return it.dwDimensiune;
	}

	return 0.0f;
}

bool IsWall(unsigned race)
{
	switch (race)
	{
		case 14201:
		case 14202:
		case 14203:
		case 14204:
			return true;
			break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////

CInstanceBase::SHORSE::SHORSE()
{
	__Initialize();
}

CInstanceBase::SHORSE::~SHORSE()
{
	assert(m_pkActor==NULL);
}

void CInstanceBase::SHORSE::__Initialize()
{
	m_isMounting=false;
	m_pkActor= nullptr;
}

void CInstanceBase::SHORSE::SetAttackSpeed(UINT uAtkSpd)
{
	if (!IsMounting())
		return;

	CActorInstance& rkActor=GetActorRef();
	rkActor.SetAttackSpeed(uAtkSpd/100.0f);
}

void CInstanceBase::SHORSE::SetMoveSpeed(UINT uMovSpd)
{
	if (!IsMounting())
		return;

	CActorInstance& rkActor=GetActorRef();
	rkActor.SetMoveSpeed(uMovSpd/100.0f);
}

void CInstanceBase::SHORSE::Create(const TPixelPosition& c_rkPPos, UINT eRace, UINT eHitEffect)
{
	assert(NULL==m_pkActor && "CInstanceBase::SHORSE::Create - ALREADY MOUNT");

	m_pkActor=new CActorInstance;

	CActorInstance& rkActor=GetActorRef();
	rkActor.SetEventHandler(CActorInstance::IEventHandler::GetEmptyPtr());
	if (!rkActor.SetRace(eRace))
	{
		delete m_pkActor;
		m_pkActor= nullptr;
		return;
	}

	rkActor.SetShape(0);
	rkActor.SetBattleHitEffect(eHitEffect);
	rkActor.SetAlphaValue(0.0f);
	rkActor.BlendAlphaValue(1.0f, 0.5f);
	rkActor.SetMoveSpeed(1.0f);
	rkActor.SetAttackSpeed(1.0f);
	rkActor.SetMotionMode(CRaceMotionData::MODE_GENERAL);
	rkActor.Stop();
	rkActor.RefreshActorInstance();
	float fScale = DimeniuneNPC(eRace);
	if (fScale)
	{
		fScale /= 100.0f;
		rkActor.SetScaleNew(fScale, fScale, fScale);
	}
	
	rkActor.SetCurPixelPosition(c_rkPPos);
	m_isMounting=true;
}

void CInstanceBase::SHORSE::Destroy()
{
	if (m_pkActor)
	{
		m_pkActor->Destroy();
		delete m_pkActor;
	}

	__Initialize();
}

CActorInstance& CInstanceBase::SHORSE::GetActorRef()
{
	assert(NULL!=m_pkActor && "CInstanceBase::SHORSE::GetActorRef");
	return *m_pkActor;
}

CActorInstance* CInstanceBase::SHORSE::GetActorPtr()
{
	return m_pkActor;
}

enum eMountType {MOUNT_TYPE_NONE=0, MOUNT_TYPE_NORMAL=1, MOUNT_TYPE_COMBAT=2, MOUNT_TYPE_MILITARY=3};
eMountType GetMountLevelByVnum(uint32_t dwMountVnum, bool IsNew)
{
	if (!dwMountVnum)
		return MOUNT_TYPE_NONE;

	switch (dwMountVnum)
	{
		// ### YES SKILL
		// @fixme116 begin
		case 20107: // normal military horse (no guild)
		case 20108: // normal military horse (guild member)
		case 20109: // normal military horse (guild master)
			if (IsNew)
				return MOUNT_TYPE_NONE;
		// @fixme116 end
		// Classic
		case 20110: // Classic Boar
		case 20111: // Classic Wolf
		case 20112: // Classic Tiger
		case 20113: // Classic Lion
		case 20114: // White Lion
		// Special Lv2
		case 20115: // Wild Battle Boar
		case 20116: // Fight Wolf
		case 20117: // Storm Tiger
		case 20118: // Battle Lion (bugged)
		case 20205: // Wild Battle Boar (alternative)
		case 20206: // Fight Wolf (alternative)
		case 20207: // Storm Tiger (alternative)
		case 20208: // Battle Lion (bugged) (alternative)
		// Royal Tigers
		case 20120: // blue
		case 20121: // dark red
		case 20122: // gold
		case 20123: // green
		case 20124: // pied
		case 20125: // white
		// Royal mounts (Special Lv3)
		case 20209: // Royal Boar
		case 20210: // Royal Wolf
		case 20211: // Royal Tiger
		case 20212: // Royal Lion
		//
		case 20215: // Rudolph m Lv3 (yes skill, yes atk)
		case 20218: // Rudolph f Lv3 (yes skill, yes atk)
		case 20225: // Dyno Lv3 (yes skill, yes atk)
		case 20230: // Turkey Lv3 (yes skill, yes atk)
		case 60008:
			return MOUNT_TYPE_MILITARY;
			break;
		// ### NO SKILL YES ATK
		// @fixme116 begin
		case 20104: // normal combat horse (no guild)
		case 20105: // normal combat horse (guild member)
		case 20106: // normal combat horse (guild master)
			if (IsNew)
				return MOUNT_TYPE_NONE;
		// @fixme116 end
		case 20119: // Black Horse (no skill, yes atk)
		case 20214: // Rudolph m Lv2 (no skill, yes atk)
		case 20217: // Rudolph f Lv2 (no skill, yes atk)
		case 20219: // Equus Porphyreus (no skill, yes atk)
		case 20220: // Comet (no skill, yes atk)
		case 20221: // Polar Predator (no skill, yes atk)
		case 20222: // Armoured Panda (no skill, yes atk)
		case 20224: // Dyno Lv2 (no skill, yes atk)
		case 20226: // Nightmare (no skill, yes atk)
		case 20227: // Unicorn (no skill, yes atk)
		case 20229: // Turkey Lv2 (no skill, yes atk)
		case 20231: // Leopard (no skill, yes atk)
		case 20232: // Black Panther (no skill, yes atk)
			return MOUNT_TYPE_COMBAT;
			break;
		// ### NO SKILL NO ATK
		// @fixme116 begin
		case 20101: // normal beginner horse (no guild)
		case 20102: // normal beginner horse (guild member)
		case 20103: // normal beginner horse (guild master)
			if (IsNew)
				return MOUNT_TYPE_NONE;
		// @fixme116 end
		case 20213: // Rudolph m Lv1 (no skill, no atk)
		case 20216: // Rudolph f Lv1 (no skill, no atk)
		// Special Lv1
		case 20201: // Boar Lv1 (no skill, no atk)
		case 20202: // Wolf Lv1 (no skill, no atk)
		case 20203: // Tiger Lv1 (no skill, no atk)
		case 20204: // Lion Lv1 (no skill, no atk)
		//
		case 20223: // Dyno Lv1 (no skill, no atk)
		case 20228: // Turkey Lv1 (no skill, no atk)
			return MOUNT_TYPE_NORMAL;
			break;
		default:
			return MOUNT_TYPE_NONE;
			break;
	}
}

UINT CInstanceBase::SHORSE::GetLevel()
{
	if (m_pkActor)
	{
#ifndef ENABLE_NO_MOUNT_CHECK
		return static_cast<UINT>(GetMountLevelByVnum(m_pkActor->GetRace(), false));
#else
		return (m_pkActor->GetRace()) ? MOUNT_TYPE_MILITARY : MOUNT_TYPE_NONE;
#endif
	}
	return 0;
}

bool CInstanceBase::SHORSE::IsNewMount()
{
#ifndef ENABLE_NO_MOUNT_CHECK
	if (m_pkActor)
	{
		uint32_t dwMountVnum = m_pkActor->GetRace();
		eMountType mountType = GetMountLevelByVnum(dwMountVnum, true);
		return (mountType != MOUNT_TYPE_NONE) && (mountType != MOUNT_TYPE_NORMAL);
	}
#endif
	return false;
}
bool CInstanceBase::SHORSE::CanUseSkill()
{
	// ¸¶»ó½ºÅ³Àº ¸»ÀÇ ·¹º§ÀÌ 3 ÀÌ»óÀÌ¾î¾ß¸¸ ÇÔ.
	if (IsMounting())
		return 2 < GetLevel();

	return true;
}

bool CInstanceBase::SHORSE::CanAttack()
{
	if (IsMounting())
		if (GetLevel()<=1)
			return false;

	return true;
}

bool CInstanceBase::SHORSE::IsMounting()
{
	return m_isMounting;
}

void CInstanceBase::SHORSE::Deform()
{
	if (!IsMounting())
		return;

	CActorInstance& rkActor=GetActorRef();
	rkActor.INSTANCEBASE_Deform();
}

void CInstanceBase::SHORSE::Render()
{
	if (!IsMounting())
		return;

	CActorInstance& rkActor=GetActorRef();
	rkActor.Render();
}

void CInstanceBase::__AttachHorseSaddle()
{
	if (!IsMountingHorse())
		return;
	
	m_kHorse.m_pkActor->AttachModelInstance(CRaceData::PART_MAIN, "saddle", m_GraphicThingInstance, CRaceData::PART_MAIN);
}

void CInstanceBase::__DetachHorseSaddle()
{
	if (!IsMountingHorse())
		return;
	m_kHorse.m_pkActor->DetachModelInstance(CRaceData::PART_MAIN, m_GraphicThingInstance, CRaceData::PART_MAIN);
}

//////////////////////////////////////////////////////////////////////////////////////

void CInstanceBase::BlockMovement()
{
	m_GraphicThingInstance.BlockMovement();
}

bool CInstanceBase::IsBlockObject(const CGraphicObjectInstance& c_rkBGObj)
{
	return m_GraphicThingInstance.IsBlockObject(c_rkBGObj);
}

bool CInstanceBase::AvoidObject(const CGraphicObjectInstance& c_rkBGObj)
{
	return m_GraphicThingInstance.AvoidObject(c_rkBGObj);
}

///////////////////////////////////////////////////////////////////////////////////

bool __ArmorVnumToShape(int iVnum, uint32_t * pdwShape)
{
	*pdwShape = iVnum;

	/////////////////////////////////////////

	if (0 == iVnum || 1 == iVnum)
		return false;

	if (!USE_ARMOR_SPECULAR)
		return false;

	CItemData * pItemData;
	if (!CItemManager::Instance().GetItemDataPointer(iVnum, &pItemData))
		return false;

	enum
	{
		SHAPE_VALUE_SLOT_INDEX = 3,
	};

	*pdwShape = pItemData->GetValue(SHAPE_VALUE_SLOT_INDEX);

	return true;
}

// 2004.07.05.myevan.±Ã½ÅÅº¿µ ³¢ÀÌ´Â ¹®Á¦
class CActorInstanceBackground : public IBackground
{
	public:
		CActorInstanceBackground() {}
		virtual ~CActorInstanceBackground() {}
		bool IsBlock(int x, int y)
		{
			CPythonBackground& rkBG=CPythonBackground::Instance();
			return rkBG.isAttrOn(x, y, CTerrainImpl::ATTRIBUTE_BLOCK);
		}
};

static CActorInstanceBackground gs_kActorInstBG;

bool CInstanceBase::LessRenderOrder(CInstanceBase* pkInst)
{
	int nMainAlpha=(__GetAlphaValue() < 1.0f) ? 1 : 0;
	int nTestAlpha=(pkInst->__GetAlphaValue() < 1.0f) ? 1 : 0;
	if (nMainAlpha < nTestAlpha)
		return true;
	if (nMainAlpha > nTestAlpha)
		return false;

	if (GetRace()<pkInst->GetRace())
		return true;
	if (GetRace()>pkInst->GetRace())
		return false;

	if (GetShape()<pkInst->GetShape())
		return true;

	if (GetShape()>pkInst->GetShape())
		return false;

	UINT uLeftLODLevel=__LessRenderOrder_GetLODLevel();
	UINT uRightLODLevel=pkInst->__LessRenderOrder_GetLODLevel();
	if (uLeftLODLevel<uRightLODLevel)
		return true;
	if (uLeftLODLevel>uRightLODLevel)
		return false;

	if (m_awPart[CRaceData::PART_WEAPON]<pkInst->m_awPart[CRaceData::PART_WEAPON])
		return true;

	return false;
}

UINT CInstanceBase::__LessRenderOrder_GetLODLevel()
{
	CGrannyLODController* pLODCtrl=m_GraphicThingInstance.GetLODControllerPointer(0);
	if (!pLODCtrl)
		return 0;

	return pLODCtrl->GetLODLevel();
}

bool CInstanceBase::__Background_GetWaterHeight(const TPixelPosition& c_rkPPos, float* pfHeight)
{
	int32_t lHeight;
	if (!CPythonBackground::Instance().GetWaterHeight(int(c_rkPPos.x), int(c_rkPPos.y), &lHeight))
		return false;

	*pfHeight = float(lHeight);

	return true;
}

bool CInstanceBase::__Background_IsWaterPixelPosition(const TPixelPosition& c_rkPPos)
{
	return CPythonBackground::Instance().isAttrOn(c_rkPPos.x, c_rkPPos.y, CTerrainImpl::ATTRIBUTE_WATER);
}

const float PC_DUST_RANGE = 2000.0f;
const float NPC_DUST_RANGE = 1000.0f;

uint32_t CInstanceBase::ms_dwUpdateCounter=0;
uint32_t CInstanceBase::ms_dwRenderCounter=0;
uint32_t CInstanceBase::ms_dwDeformCounter=0;

CDynamicPool<CInstanceBase> CInstanceBase::ms_kPool;

bool CInstanceBase::__IsInDustRange()
{
	if (!__IsExistMainInstance())
		return false;

	CInstanceBase* pkInstMain=__GetMainInstancePtr();

	float fDistance=NEW_GetDistanceFromDestInstance(*pkInstMain);

	if (IsPC())
	{
		if (fDistance<=PC_DUST_RANGE)
			return true;
	}

	if (fDistance<=NPC_DUST_RANGE)
		return true;

	return false;
}

void CInstanceBase::__EnableSkipCollision()
{
	if (__IsMainInstance()) {
		//TraceError("CInstanceBase::__EnableSkipCollision - You should not skip your own collisions!!");
		return;
	}

	m_GraphicThingInstance.EnableSkipCollision();
}

void CInstanceBase::__DisableSkipCollision()
{
	m_GraphicThingInstance.DisableSkipCollision();
}

uint32_t CInstanceBase::__GetShadowMapColor(float x, float y)
{
	CPythonBackground& rkBG=CPythonBackground::Instance();
	return rkBG.GetShadowMapColor(x, y);
}

float CInstanceBase::__GetBackgroundHeight(float x, float y)
{
	CPythonBackground& rkBG=CPythonBackground::Instance();
	return rkBG.GetHeight(x, y);
}

#ifdef __MOVIE_MODE__

bool CInstanceBase::IsMovieMode()
{
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
	if (IsAffect(AFFECT_INVISIBILITY) && !__MainCanSeeHiddenThing())
		return true;
#else
	if (IsAffect(AFFECT_INVISIBILITY))
		return true;
#endif

	return false;
}

#endif

bool CInstanceBase::IsInvisibility()
{
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
	if (IsAffect(AFFECT_INVISIBILITY) && !__MainCanSeeHiddenThing())
		return true;
#else
	if (IsAffect(AFFECT_INVISIBILITY))
		return true;
#endif

	return false;
}

bool CInstanceBase::IsParalysis()
{
	return m_GraphicThingInstance.IsParalysis();
}
//IsGameMaster
bool CInstanceBase::IsGameMaster()
{
	if (m_kAffectFlagContainer.IsSet(AFFECT_YMIR))
		return true;
	else
		return false;
}

bool CInstanceBase::IsSA()
{
	if (IsGameMaster()) {
		std::string name = GetNameString();
		if (name.find("[SA]") != std::string::npos)
			return true;
	}
	
	return false;
}

bool CInstanceBase::IsSameEmpire(CInstanceBase& rkInstDst)
{
	if (0 == rkInstDst.m_dwEmpireID)
		return true;

	if (IsGameMaster())
		return true;

	if (rkInstDst.IsGameMaster())
		return true;

	if (rkInstDst.m_dwEmpireID==m_dwEmpireID)
		return true;

	return true;
}

uint32_t CInstanceBase::GetEmpireID()
{
	return m_dwEmpireID;
}

uint32_t CInstanceBase::GetGuildID()
{
	return m_dwGuildID;
}

#ifdef ENABLE_SKILL_COLOR_SYSTEM
uint32_t* CInstanceBase::GetSkillColor(uint32_t dwSkillIndex)
{
	uint32_t dwSkillSlot = dwSkillIndex + 1;
	CPythonSkill::SSkillData* c_pSkillData;
	if (!CPythonSkill::Instance().GetSkillData(dwSkillSlot, &c_pSkillData))
		return nullptr;

	WORD dwEffectID = c_pSkillData->GradeData[CPythonSkill::SKILL_GRADE_COUNT].wMotionIndex - CRaceMotionData::NAME_SKILL - (1 * 25);

	return m_GraphicThingInstance.GetSkillColorByMotionID(dwEffectID);
}
#endif

uint32_t CInstanceBase::GetAlignment()
{
	return m_sAlignment;
}

uint32_t CInstanceBase::GetAlignmentGrade()
{
	if (!IsPC())
		return -1;

	if (m_sAlignment <= 4999)		return 0;
	if (m_sAlignment <= 14999)		return 1;
	if (m_sAlignment <= 19999)		return 2;
	if (m_sAlignment <= 29999)		return 3;
	if (m_sAlignment <= 49999)		return 4;
	if (m_sAlignment <= 74999)		return 5;
	if (m_sAlignment <= 99999)		return 6;
	if (m_sAlignment <= 124999)		return 7;
	if (m_sAlignment <= 174999)		return 8;
	if (m_sAlignment <= 249999)		return 9;
	if (m_sAlignment <= 499999)		return 10;
	if (m_sAlignment <= 749999)		return 11;
	if (m_sAlignment <= 999999)		return 12;
	if (m_sAlignment <= 1499999)	return 13;
	if (m_sAlignment <= 2499999)	return 14;
	if (m_sAlignment <= 2999999)	return 15;
	if (m_sAlignment <= 3499999)	return 16;
	if (m_sAlignment <= 3999999)	return 17;
	if (m_sAlignment <= 4499999)	return 18;
	if (m_sAlignment <= 4999999)	return 19;
	if (m_sAlignment <= 5000000)	return 20;
	return 20;
}

//int CInstanceBase::GetAlignmentType()
//{
//	switch (GetAlignmentGrade())
//	{
//		case 0:
//		case 1:
//		case 2:
//		case 3:
//		{
//			return ALIGNMENT_TYPE_WHITE;
//			break;
//		}
//
//		case 5:
//		case 6:
//		case 7:
//		case 8:
//		{
//			return ALIGNMENT_TYPE_DARK;
//			break;
//		}
//	}
//
//	return ALIGNMENT_TYPE_NORMAL;
//}

uint8_t CInstanceBase::GetPKMode()
{
	return m_byPKMode;
}

bool CInstanceBase::IsKiller()
{
	return m_isKiller;
}

bool CInstanceBase::IsPartyMember()
{
	return m_isPartyMember;
}

bool CInstanceBase::IsInSafe()
{
	const TPixelPosition& c_rkPPosCur=m_GraphicThingInstance.NEW_GetCurPixelPositionRef();
	if (CPythonBackground::Instance().isAttrOn(c_rkPPosCur.x, c_rkPPosCur.y, CTerrainImpl::ATTRIBUTE_BANPK))
		return true;

	return false;
}

float CInstanceBase::CalculateDistanceSq3d(const TPixelPosition& c_rkPPosDst)
{
	const TPixelPosition& c_rkPPosSrc=m_GraphicThingInstance.NEW_GetCurPixelPositionRef();
	return SPixelPosition_CalculateDistanceSq3d(c_rkPPosSrc, c_rkPPosDst);
}

void CInstanceBase::OnSelected()
{
#ifdef __MOVIE_MODE__
	if (!__IsExistMainInstance())
		return;
#endif

	if (IsStoneDoor())
		return;

	if (IsDead())
		return;

	__AttachSelectEffect();
}

void CInstanceBase::OnUnselected()
{
	__DetachSelectEffect();
}

void CInstanceBase::OnTargeted()
{
#ifdef __MOVIE_MODE__
	if (!__IsExistMainInstance())
		return;
#endif

	if (IsStoneDoor())
		return;

	if (IsDead())
		return;

	__AttachTargetEffect();
}

void CInstanceBase::OnUntargeted()
{
	__DetachTargetEffect();
}

void CInstanceBase::DestroySystem()
{
	ms_kPool.Clear();
}

void CInstanceBase::CreateSystem(UINT uCapacity)
{
	ms_kPool.Create(uCapacity);

	memset(ms_adwCRCAffectEffect, 0, sizeof(ms_adwCRCAffectEffect));

	ms_fDustGap=250.0f;
	ms_fHorseDustGap=500.0f;
}

CInstanceBase* CInstanceBase::New()
{
	return ms_kPool.Alloc();
}

void CInstanceBase::Delete(CInstanceBase* pkInst)
{
	pkInst->Destroy();
	ms_kPool.Free(pkInst);
}

void CInstanceBase::SetMainInstance()
{
	CPythonCharacterManager& rkChrMgr=CPythonCharacterManager::Instance();

	uint32_t dwVID=GetVirtualID();
	rkChrMgr.SetMainInstance(dwVID);

	m_GraphicThingInstance.SetMainInstance();
}

CInstanceBase* CInstanceBase::__GetMainInstancePtr()
{
	CPythonCharacterManager& rkChrMgr=CPythonCharacterManager::Instance();
	return rkChrMgr.GetMainInstancePtr();
}

void CInstanceBase::__ClearMainInstance()
{
	CPythonCharacterManager& rkChrMgr=CPythonCharacterManager::Instance();
	rkChrMgr.ClearMainInstance();
}

/* ½ÇÁ¦ ÇÃ·¹ÀÌ¾î Ä³¸¯ÅÍÀÎÁö Á¶»ç.*/
bool CInstanceBase::__IsMainInstance()
{
	if (this==__GetMainInstancePtr())
		return true;

	return false;
}

bool CInstanceBase::__IsExistMainInstance()
{
	if(__GetMainInstancePtr())
		return true;
	else
		return false;
}

bool CInstanceBase::__MainCanSeeHiddenThing()
{
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
	CInstanceBase * pInstance = __GetMainInstancePtr();
	return (pInstance) ? TRUE == pInstance->IsGameMaster() : false;
#else
	return false;
#endif
}

float CInstanceBase::__GetBowRange()
{
	float fRange = 2500.0f - 100.0f;

	if (__IsMainInstance())
	{
		IAbstractPlayer& rPlayer=IAbstractPlayer::GetSingleton();
		fRange += float(rPlayer.GetStatus(POINT_BOW_DISTANCE));
	}

	return fRange;
}

CInstanceBase* CInstanceBase::__FindInstancePtr(uint32_t dwVID)
{
	CPythonCharacterManager& rkChrMgr=CPythonCharacterManager::Instance();
	return rkChrMgr.GetInstancePtr(dwVID);
}

bool CInstanceBase::__FindRaceType(uint32_t dwRace, uint8_t* pbType)
{
	CPythonNonPlayer& rkNonPlayer=CPythonNonPlayer::Instance();
	return rkNonPlayer.GetInstanceType(dwRace, pbType);
}

bool CInstanceBase::Create(const SCreateData& c_rkCreateData, bool wikiPreview)
{
	IAbstractApplication::GetSingleton().SkipRenderBuffering(100);

	SetInstanceType(c_rkCreateData.m_bType);

	if (!SetRace(c_rkCreateData.m_dwRace)) {
		return false;
	}

	SetVirtualID(c_rkCreateData.m_dwVID);

	if (c_rkCreateData.m_isMain)
		SetMainInstance();

	if (IsGuildWall())
	{
		unsigned center_x;
		unsigned center_y;

		c_rkCreateData.m_kAffectFlags.ConvertToPosition(&center_x, &center_y);

		float center_z = __GetBackgroundHeight(center_x, center_y);
		NEW_SetPixelPosition(TPixelPosition(float(c_rkCreateData.m_lPosX), float(c_rkCreateData.m_lPosY), center_z));
	}
	else
	{
		SCRIPT_SetPixelPosition(float(c_rkCreateData.m_lPosX), float(c_rkCreateData.m_lPosY));
	}

	if (0 != c_rkCreateData.m_dwMountVnum)
		MountHorse(c_rkCreateData.m_dwMountVnum);

	SetArmor(c_rkCreateData.m_dwArmor);

	if (IsPC())
	{
		SetHair(c_rkCreateData.m_dwHair);
		SetWeapon(c_rkCreateData.m_dwWeapon);
#ifdef ENABLE_RUNE_SYSTEM
		SetRune(c_rkCreateData.m_dwRune);
#endif
#ifdef ENABLE_ACCE_SYSTEM
		SetAcce(c_rkCreateData.m_dwAcce);
#endif
#ifdef ENABLE_COSTUME_EFFECT
		ChangeCostumeEffect(true, c_rkCreateData.m_dwEffectBody);
		ChangeCostumeEffect(false, c_rkCreateData.m_dwEffectWeapon);
#endif
#ifdef ENABLE_MULTI_LANGUAGE
		SetLanguage(c_rkCreateData.m_bLanguage);
#endif	
	}

	__Create_SetName(c_rkCreateData);

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	ChangeSkillColor(*c_rkCreateData.m_dwSkillColor);
	for (const auto& i : c_rkCreateData.m_dwSkillColor)
	{
		memcpy(m_dwSkillColor, i, MAX_EFFECT_COUNT * sizeof(uint32_t));
	}
	//memcpy(m_dwSkillColor, c_rkCreateData.m_dwSkillColor,sizeof(m_dwSkillColor));
#endif

#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
	if (IsEnemy() && CPythonSystem::Instance().IsShowMobLevel())
		m_dwLevel = CPythonNonPlayer::Instance().GetMonsterLevel(GetRace());
	else
		m_dwLevel = c_rkCreateData.m_dwLevel;

	
#else
	m_dwLevel = c_rkCreateData.m_dwLevel;
#endif

	m_dwGuildID = c_rkCreateData.m_dwGuildID;
	m_dwEmpireID = c_rkCreateData.m_dwEmpireID;

	SetVirtualNumber(c_rkCreateData.m_dwRace);
	SetRotation(c_rkCreateData.m_fRot);

	SetAlignment(c_rkCreateData.m_sAlignment);
	SetPKMode(c_rkCreateData.m_byPKMode);

	SetMoveSpeed(c_rkCreateData.m_dwMovSpd);
	SetAttackSpeed(c_rkCreateData.m_dwAtkSpd);

#ifdef NEW_PET_SYSTEM
	if ((m_dwRace >= 34041) && (m_dwRace <= 34111)){
		float scale = c_rkCreateData.m_dwLevel * 0.005f + 0.75f;
		m_GraphicThingInstance.SetScaleNew(scale, scale, scale);
	}
	else
		m_GraphicThingInstance.SetScaleNew(1.0f,1.0f,1.0f);
#endif

	float fScale = DimeniuneNPC(GetRace());
	if (fScale)
	{
		fScale /= 100.0f;
		m_GraphicThingInstance.SetScaleNew(fScale, fScale, fScale);
	}

	// NOTE : Dress ¸¦ ÀÔ°í ÀÖÀ¸¸é Alpha ¸¦ ³ÖÁö ¾Ê´Â´Ù.
	if (!IsWearingDress())
	{
		// NOTE : ¹Ýµå½Ã Affect ¼ÂÆÃ À­ÂÊ¿¡ ÀÖ¾î¾ß ÇÔ
		m_GraphicThingInstance.SetAlphaValue(0.0f);
		m_GraphicThingInstance.BlendAlphaValue(1.0f, 0.5f);
	}

	if (!IsGuildWall())
	{
		SetAffectFlagContainer(c_rkCreateData.m_kAffectFlags);
	}

	// NOTE : ¹Ýµå½Ã Affect ¼ÂÆÃ ÈÄ¿¡ ÇØ¾ß ÇÔ
	AttachTextTail();
	RefreshTextTail();

	if (c_rkCreateData.m_dwStateFlags & ADD_CHARACTER_STATE_SPAWN)
	{
		if (IsAffect(AFFECT_SPAWN))
			__AttachEffect(EFFECT_SPAWN_APPEAR);

		if (IsPC())
		{
			Refresh(CRaceMotionData::NAME_WAIT, true);
		}
		else
		{
			Refresh(CRaceMotionData::NAME_SPAWN, false);
		}
	}
	else
	{
		Refresh(CRaceMotionData::NAME_WAIT, true);
	}

	__AttachEmpireEffect(c_rkCreateData.m_dwEmpireID);

	RegisterBoundingSphere();

	if (c_rkCreateData.m_dwStateFlags & ADD_CHARACTER_STATE_DEAD)
		m_GraphicThingInstance.DieEnd();

	SetStateFlags(c_rkCreateData.m_dwStateFlags);

	m_GraphicThingInstance.SetBattleHitEffect(ms_adwCRCAffectEffect[EFFECT_HIT]);

	if (!IsPC() || wikiPreview)
	{
		uint32_t dwBodyColor = CPythonNonPlayer::Instance().GetMonsterColor(c_rkCreateData.m_dwRace);
		if (0 != dwBodyColor)
		{
			SetModulateRenderMode();
			SetAddColor(dwBodyColor);
		}
	}

	__AttachHorseSaddle();

	// ±æµå ½Éº¼À» À§ÇÑ ÀÓ½Ã ÄÚµå, ÀûÁ¤ À§Ä¡¸¦ Ã£´Â Áß
	const int c_iGuildSymbolRace = 14200;
	if (c_iGuildSymbolRace == GetRace())
	{
		std::string strFileName = GetGuildSymbolFileName(m_dwGuildID);
		if (IsFile(strFileName.c_str()))
			m_GraphicThingInstance.ChangeMaterial(strFileName.c_str());
	}

#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
	if (IsAffect(AFFECT_INVISIBILITY) && __MainCanSeeHiddenThing())
		m_GraphicThingInstance.BlendAlphaValue(0.5f, 0.5f);
#endif


#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
	CPythonCharacterManager::Instance().ApplyPendingMountCountIfExists(this);
#endif

	return true;
}
#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93

void CInstanceBase::SetMountCountOverhead(const char* szText)
{
	TraceError("SetMountCountOverhead: %s\n", szText);

	D3DXCOLOR color = D3DXCOLOR(0.9f, 0.9f, 0.1f, 1.0f); // sárga
	CPythonTextTail::Instance().AttachMountCount(GetVirtualID(), szText, color);
}
#endif


#ifdef ENABLE_SKILL_COLOR_SYSTEM
void CInstanceBase::ChangeSkillColor(const uint32_t *dwSkillColor)
{
	uint32_t tmpdwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
	memcpy(tmpdwSkillColor, dwSkillColor, sizeof(tmpdwSkillColor));

	uint32_t skill[CRaceMotionData::SKILL_NUM][ESkillColorLength::MAX_EFFECT_COUNT] = {};

	for (int i = 0; i < 8; ++i)
	{
		for (int t = 0; t < ESkillColorLength::MAX_SKILL_COUNT; ++t)
		{
			for (int x = 0; x < ESkillColorLength::MAX_EFFECT_COUNT; ++x)
			{
				skill[i * 10 + i*(ESkillColorLength::MAX_SKILL_COUNT - 1) + t + 1][x] = *(dwSkillColor++);
			}
		}
		dwSkillColor -= static_cast<uint32_t>(ESkillColorLength::MAX_SKILL_COUNT * ESkillColorLength::MAX_EFFECT_COUNT);
	}

	for (int i = BUFF_BEGIN; i < MAX_SKILL_COUNT + MAX_BUFF_COUNT; i++)
	{
		uint8_t id = 0;
		switch (i)
		{
			case BUFF_BEGIN+0:
				id = 94;
				break;
			case BUFF_BEGIN + 1:
				id = 95;
				break;
			case BUFF_BEGIN + 2:
				id = 96;
				break;
			case BUFF_BEGIN + 3:
				id = 110;
				break;
			case BUFF_BEGIN + 4:
				id = 111;
				break;
			default:
				break;
		}

		if (id == 0)
			continue;

		for (int x = 0; x < ESkillColorLength::MAX_EFFECT_COUNT; ++x)
			skill[id][x] = tmpdwSkillColor[i][x];
	}

	m_GraphicThingInstance.ChangeSkillColor(*skill);
}
#endif

void CInstanceBase::__Create_SetName(const SCreateData& c_rkCreateData)
{
	if (IsGoto())
	{
		SetNameString("", 0);
		return;
	}
	if (IsWarp())
	{
		__Create_SetWarpName(c_rkCreateData);
		return;
	}
	
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
	if (IsEnemy() && CPythonSystem::Instance().IsShowMobAIFlag() && CPythonNonPlayer::Instance().IsAggressive(GetRace()))
	{
		std::string strName = c_rkCreateData.m_stName;
		//strName += "*";
		SetNameString(strName.c_str(), strName.length());
	}
	else {
		SetNameString(c_rkCreateData.m_stName.c_str(), c_rkCreateData.m_stName.length());
	}

#else
	SetNameString(c_rkCreateData.m_stName.c_str(), c_rkCreateData.m_stName.length());
#endif
}

void CInstanceBase::__Create_SetWarpName(const SCreateData& c_rkCreateData)
{
	const char * c_szName;
	if (CPythonNonPlayer::Instance().GetName(c_rkCreateData.m_dwRace, &c_szName))
	{
		std::string strName = c_szName;
		int iFindingPos = strName.find_first_of(" ", 0);
		if (iFindingPos > 0)
		{
			strName.resize(iFindingPos);
		}
		SetNameString(strName.c_str(), strName.length());
	}
	else
	{
		SetNameString(c_rkCreateData.m_stName.c_str(), c_rkCreateData.m_stName.length());
	}
}

void CInstanceBase::SetNameString(const char* c_szName, int len)
{
	m_stName.assign(c_szName, len);
}


bool CInstanceBase::SetRace(uint32_t eRace)
{
	m_dwRace = eRace;

	if (!m_GraphicThingInstance.SetRace(eRace))
		return false;

	if (!__FindRaceType(m_dwRace, &m_eRaceType))
		m_eRaceType=CActorInstance::TYPE_PC;

	return true;
}

bool CInstanceBase::__IsChangableWeapon(uint32_t iWeaponID)
{
	// µå·¹½º ÀÔ°í ÀÖÀ»¶§´Â ºÎÄÉ¿ÜÀÇ Àåºñ´Â ³ª¿ÀÁö ¾Ê°Ô..
	if (IsWearingDress())
	{
		const int c_iBouquets[] =
		{
			50201,	// Bouquet for Assassin
			50202,	// Bouquet for Shaman
			50203,
			50204,
			0, // #0000545: [M2CN] ¿þµù µå·¹½º¿Í Àåºñ Âø¿ë ¹®Á¦
		};

		for (int i = 0; c_iBouquets[i] != 0; ++i)
			if (iWeaponID == c_iBouquets[i])
				return true;

		return false;
	}
	else
		return true;
}

bool CInstanceBase::IsWearingDress()
{
	const int c_iWeddingDressShape = 201;
	return c_iWeddingDressShape == m_eShape;
}

bool CInstanceBase::IsHoldingPickAxe()
{
	const int c_iPickAxeStart = 29101;
	const int c_iPickAxeEnd = 29110;
	return m_awPart[CRaceData::PART_WEAPON] >= c_iPickAxeStart && m_awPart[CRaceData::PART_WEAPON] <= c_iPickAxeEnd;
}

bool CInstanceBase::IsNewMount()
{
	return m_kHorse.IsNewMount();
}

bool CInstanceBase::IsMountingHorse()
{
	return m_kHorse.IsMounting();
}

void CInstanceBase::MountHorse(UINT eRace)
{
	m_kHorse.Destroy();
	m_kHorse.Create(m_GraphicThingInstance.NEW_GetCurPixelPositionRef(), eRace, ms_adwCRCAffectEffect[EFFECT_HIT]);
	
	SetMotionMode(CRaceMotionData::MODE_HORSE);
	SetRotationSpeed(c_fDefaultHorseRotationSpeed);

	m_GraphicThingInstance.MountHorse(m_kHorse.GetActorPtr());
	m_GraphicThingInstance.Stop();
	m_GraphicThingInstance.RefreshActorInstance();
}

void CInstanceBase::DismountHorse()
{
	m_kHorse.Destroy();
}

void CInstanceBase::GetInfo(std::string* pstInfo)
{
	char szInfo[256];
	sprintf(szInfo, "Inst - UC %d, RC %d Pool - %zd ",
		ms_dwUpdateCounter,
		ms_dwRenderCounter,
		ms_kPool.GetCapacity()
	);

	pstInfo->append(szInfo);
}

void CInstanceBase::ResetPerformanceCounter()
{
	ms_dwUpdateCounter=0;
	ms_dwRenderCounter=0;
	ms_dwDeformCounter=0;
}

bool CInstanceBase::NEW_IsLastPixelPosition()
{
	return m_GraphicThingInstance.IsPushing();
}

const TPixelPosition& CInstanceBase::NEW_GetLastPixelPositionRef()
{
	return m_GraphicThingInstance.NEW_GetLastPixelPositionRef();
}

void CInstanceBase::NEW_SetDstPixelPositionZ(FLOAT z)
{
	m_GraphicThingInstance.NEW_SetDstPixelPositionZ(z);
}

void CInstanceBase::NEW_SetDstPixelPosition(const TPixelPosition& c_rkPPosDst)
{
	m_GraphicThingInstance.NEW_SetDstPixelPosition(c_rkPPosDst);
}

void CInstanceBase::NEW_SetSrcPixelPosition(const TPixelPosition& c_rkPPosSrc)
{
	m_GraphicThingInstance.NEW_SetSrcPixelPosition(c_rkPPosSrc);
}

const TPixelPosition& CInstanceBase::NEW_GetCurPixelPositionRef()
{
	return m_GraphicThingInstance.NEW_GetCurPixelPositionRef();
}

const TPixelPosition& CInstanceBase::NEW_GetDstPixelPositionRef()
{
	return m_GraphicThingInstance.NEW_GetDstPixelPositionRef();
}

const TPixelPosition& CInstanceBase::NEW_GetSrcPixelPositionRef()
{
	return m_GraphicThingInstance.NEW_GetSrcPixelPositionRef();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void CInstanceBase::OnSyncing()
{
	m_GraphicThingInstance.__OnSyncing();
}

void CInstanceBase::OnWaiting()
{
	m_GraphicThingInstance.__OnWaiting();
}

void CInstanceBase::OnMoving()
{
	m_GraphicThingInstance.__OnMoving();
}

void CInstanceBase::ChangeGuild(uint32_t dwGuildID)
{
	m_dwGuildID=dwGuildID;

	
	DetachTextTail();
	AttachTextTail();
	RefreshTextTail();
}

uint32_t CInstanceBase::GetPart(CRaceData::EParts part)
{
	assert(part >= 0 && part < CRaceData::PART_MAX_NUM);
	return m_awPart[part];
}

uint32_t CInstanceBase::GetShape()
{
	return m_eShape;
}

bool CInstanceBase::CanAct()
{
	return m_GraphicThingInstance.CanAct();
}

bool CInstanceBase::CanMove()
{
	return m_GraphicThingInstance.CanMove();
}

bool CInstanceBase::CanUseSkill()
{
	if (IsPoly())
		return false;

	if (IsWearingDress())
		return false;

	if (IsHoldingPickAxe())
		return false;

	if (!m_kHorse.CanUseSkill())
		return false;

	if (!m_GraphicThingInstance.CanUseSkill())
		return false;

	return true;
}

bool CInstanceBase::CanAttack()
{
	if (!m_kHorse.CanAttack())
		return false;

	if (IsWearingDress())
		return false;

	if (IsHoldingPickAxe())
		return false;

	return m_GraphicThingInstance.CanAttack();
}



bool CInstanceBase::CanFishing()
{
	return m_GraphicThingInstance.CanFishing();
}


bool CInstanceBase::IsBowMode()
{
	return m_GraphicThingInstance.IsBowMode();
}

bool CInstanceBase::IsHandMode()
{
	return m_GraphicThingInstance.IsHandMode();
}

bool CInstanceBase::IsFishingMode()
{
	if (CRaceMotionData::MODE_FISHING == m_GraphicThingInstance.GetMotionMode())
		return true;

	return false;
}

bool CInstanceBase::IsFishing()
{
	return m_GraphicThingInstance.IsFishing();
}

bool CInstanceBase::IsDead()
{
	return m_GraphicThingInstance.IsDead();
}

bool CInstanceBase::IsStun()
{
	return m_GraphicThingInstance.IsStun();
}

bool CInstanceBase::IsSleep()
{
	return m_GraphicThingInstance.IsSleep();
}


bool CInstanceBase::__IsSyncing()
{
	return m_GraphicThingInstance.__IsSyncing();
}



void CInstanceBase::NEW_SetOwner(uint32_t dwVIDOwner)
{
	m_GraphicThingInstance.SetOwner(dwVIDOwner);
}

float CInstanceBase::GetLocalTime()
{
	return m_GraphicThingInstance.GetLocalTime();
}


void CInstanceBase::PushUDPState(uint32_t dwCmdTime, const TPixelPosition& c_rkPPosDst, float fDstRot, UINT eFunc, UINT uArg)
{
}

uint32_t	ELTimer_GetServerFrameMSec();

void CInstanceBase::PushTCPStateExpanded(uint32_t dwCmdTime, const TPixelPosition& c_rkPPosDst, float fDstRot, UINT eFunc, UINT uArg, UINT uTargetVID)
{
	SCommand kCmdNew;
	kCmdNew.m_kPPosDst = c_rkPPosDst;
	kCmdNew.m_dwChkTime = dwCmdTime+100;
	kCmdNew.m_dwCmdTime = dwCmdTime;
	kCmdNew.m_fDstRot = fDstRot;
	kCmdNew.m_eFunc = eFunc;
	kCmdNew.m_uArg = uArg;
	kCmdNew.m_uTargetVID = uTargetVID;
	m_kQue_kCmdNew.push_back(kCmdNew);
}

void CInstanceBase::PushTCPState(uint32_t dwCmdTime, const TPixelPosition& c_rkPPosDst, float fDstRot, UINT eFunc, UINT uArg)
{
	if (__IsMainInstance())
	{
		//assert(!"CInstanceBase::PushTCPState ÇÃ·¹ÀÌ¾î ÀÚ½Å¿¡°Ô ÀÌµ¿ÆÐÅ¶Àº ¿À¸é ¾ÈµÈ´Ù!");
		//TraceError("CInstanceBase::PushTCPState You can't send move packets to yourself!");
		return;
	}

	int nNetworkGap=ELTimer_GetServerFrameMSec()-dwCmdTime;

	m_nAverageNetworkGap=(m_nAverageNetworkGap*70+nNetworkGap*30)/100;

	/*
	if (m_dwBaseCmdTime == 0)
	{
		m_dwBaseChkTime = ELTimer_GetFrameMSec()-nNetworkGap;
		m_dwBaseCmdTime = dwCmdTime;

		Tracenf("VID[%d] ³×Æ®¿÷°¸ [%d]", GetVirtualID(), nNetworkGap);
	}
	*/

	//m_dwBaseChkTime-m_dwBaseCmdTime+ELTimer_GetServerMSec();

	SCommand kCmdNew;
	kCmdNew.m_kPPosDst = c_rkPPosDst;
	kCmdNew.m_dwChkTime = dwCmdTime+m_nAverageNetworkGap;//m_dwBaseChkTime + (dwCmdTime - m_dwBaseCmdTime);// + nNetworkGap;
	kCmdNew.m_dwCmdTime = dwCmdTime;
	kCmdNew.m_fDstRot = fDstRot;
	kCmdNew.m_eFunc = eFunc;
	kCmdNew.m_uArg = uArg;
	m_kQue_kCmdNew.push_back(kCmdNew);

	//int nApplyGap=kCmdNew.m_dwChkTime-ELTimer_GetServerFrameMSec();

	//if (nApplyGap<-500 || nApplyGap>500)
	//	Tracenf("VID[%d] NAME[%s] ³×Æ®¿÷°¸ [cur:%d ave:%d] ÀÛµ¿½Ã°£ (%d)", GetVirtualID(), GetNameString(), nNetworkGap, m_nAverageNetworkGap, nApplyGap);
}

/*
CInstanceBase::TStateQueue::iterator CInstanceBase::FindSameState(TStateQueue& rkQuekStt, uint32_t dwCmdTime, UINT eFunc, UINT uArg)
{
	TStateQueue::iterator i=rkQuekStt.begin();
	while (rkQuekStt.end()!=i)
	{
		SState& rkSttEach=*i;
		if (rkSttEach.m_dwCmdTime==dwCmdTime)
			if (rkSttEach.m_eFunc==eFunc)
				if (rkSttEach.m_uArg==uArg)
					break;
		++i;
	}

	return i;
}
*/

bool CInstanceBase::__CanProcessNetworkStatePacket()
{
	if (m_GraphicThingInstance.IsDead())
		return false;
	if (m_GraphicThingInstance.IsKnockDown())
		return false;
	if (m_GraphicThingInstance.IsUsingSkill())
		if (!m_GraphicThingInstance.CanCancelSkill())
			return false;

	return true;
}

bool CInstanceBase::__IsEnableTCPProcess(UINT eCurFunc)
{
	if (m_GraphicThingInstance.IsActEmotion())
	{
		return false;
	}

	if (!m_bEnableTCPState)
	{
		if (FUNC_EMOTION != eCurFunc)
		{
			return false;
		}
	}

	return true;
}

void CInstanceBase::StateProcess()
{
	while (true)
	{
		if (m_kQue_kCmdNew.empty())
			return;

		uint32_t dwDstChkTime = m_kQue_kCmdNew.front().m_dwChkTime;
		uint32_t dwCurChkTime = ELTimer_GetServerFrameMSec();

		if (dwCurChkTime < dwDstChkTime)
			return;

		SCommand kCmdTop = m_kQue_kCmdNew.front();
		m_kQue_kCmdNew.pop_front();

		TPixelPosition kPPosDst = kCmdTop.m_kPPosDst;
		//uint32_t dwCmdTime = kCmdTop.m_dwCmdTime;
		FLOAT fRotDst = kCmdTop.m_fDstRot;
		UINT eFunc = kCmdTop.m_eFunc;
		UINT uArg = kCmdTop.m_uArg;
		UINT uVID = GetVirtualID();
		UINT uTargetVID = kCmdTop.m_uTargetVID;

		TPixelPosition kPPosCur;
		NEW_GetPixelPosition(&kPPosCur);

		/*
		if (IsPC())
			Tracenf("%d cmd: vid=%d[%s] func=%d arg=%d  curPos=(%f, %f) dstPos=(%f, %f) rot=%f (time %d)",
			ELTimer_GetMSec(),
			uVID, m_stName.c_str(), eFunc, uArg,
			kPPosCur.x, kPPosCur.y,
			kPPosDst.x, kPPosDst.y, fRotDst, dwCmdTime-m_dwBaseCmdTime);
		*/

		TPixelPosition kPPosDir = kPPosDst - kPPosCur;
		float fDirLen = sqrt(kPPosDir.x * kPPosDir.x + kPPosDir.y * kPPosDir.y);

		//Tracenf("°Å¸® %f", fDirLen);

		if (!__CanProcessNetworkStatePacket())
		{
			Lognf(0, "vid=%d Skip State as unable to process IsDead=%d, IsKnockDown=%d", uVID, m_GraphicThingInstance.IsDead(), m_GraphicThingInstance.IsKnockDown());
			return;
		}

		if (!__IsEnableTCPProcess(eFunc))
		{
			return;
		}

		switch (eFunc)
		{
			case FUNC_WAIT:
			{
				//Tracenf("%s (%f, %f) -> (%f, %f) ³²Àº°Å¸® %f", GetNameString(), kPPosCur.x, kPPosCur.y, kPPosDst.x, kPPosDst.y, fDirLen);
				if (fDirLen > 1.0f)
				{
					//NEW_GetSrcPixelPositionRef() = kPPosCur;
					//NEW_GetDstPixelPositionRef() = kPPosDst;
					NEW_SetSrcPixelPosition(kPPosCur);
					NEW_SetDstPixelPosition(kPPosDst);

					__EnableSkipCollision();

					m_fDstRot = fRotDst;
					m_isGoing = true;

					m_kMovAfterFunc.eFunc = FUNC_WAIT;

					if (!IsWalking())
						StartWalking();

					//Tracen("¸ñÇ¥Á¤Áö");
				}
				else
				{
					//Tracen("ÇöÀç Á¤Áö");

					m_isGoing = false;

					if (!IsWaiting())
						EndWalking();

					SCRIPT_SetPixelPosition(kPPosDst.x, kPPosDst.y);
					SetAdvancingRotation(fRotDst);
					SetRotation(fRotDst);
				}
				break;
			}

			case FUNC_MOVE:
			{
				//NEW_GetSrcPixelPositionRef() = kPPosCur;
				//NEW_GetDstPixelPositionRef() = kPPosDst;
				NEW_SetSrcPixelPosition(kPPosCur);
				NEW_SetDstPixelPosition(kPPosDst);
				m_fDstRot = fRotDst;
				m_isGoing = true;
				__EnableSkipCollision();
				//m_isSyncMov = TRUE;

				m_kMovAfterFunc.eFunc = FUNC_MOVE;

				if (!IsWalking())
				{
					//Tracen("°È°í ÀÖÁö ¾Ê¾Æ °È±â ½ÃÀÛ");
					StartWalking();
				}
				else
				{
					//Tracen("ÀÌ¹Ì °È´ÂÁß ");
				}
				break;
			}

			case FUNC_COMBO:
			{
				if (fDirLen >= 50.0f)
				{
					NEW_SetSrcPixelPosition(kPPosCur);
					NEW_SetDstPixelPosition(kPPosDst);
					m_fDstRot=fRotDst;
					m_isGoing = true;
					__EnableSkipCollision();

					m_kMovAfterFunc.eFunc = FUNC_COMBO;
					m_kMovAfterFunc.uArg = uArg;

					if (!IsWalking())
						StartWalking();
				}
				else
				{
					//Tracen("´ë±â °ø°Ý Á¤Áö");

					m_isGoing = false;

					if (IsWalking())
						EndWalking();

					SCRIPT_SetPixelPosition(kPPosDst.x, kPPosDst.y);
					RunComboAttack(fRotDst, uArg);
				}
				break;
			}

			case FUNC_ATTACK:
			{
				if (fDirLen>=50.0f)
				{
					//NEW_GetSrcPixelPositionRef() = kPPosCur;
					//NEW_GetDstPixelPositionRef() = kPPosDst;
					NEW_SetSrcPixelPosition(kPPosCur);
					NEW_SetDstPixelPosition(kPPosDst);
					m_fDstRot = fRotDst;
					m_isGoing = true;
					__EnableSkipCollision();
					//m_isSyncMov = TRUE;

					m_kMovAfterFunc.eFunc = FUNC_ATTACK;

					if (!IsWalking())
						StartWalking();

					//Tracen("³Ê¹« ¸Ö¾î¼­ ÀÌµ¿ ÈÄ °ø°Ý");
				}
				else
				{
					//Tracen("³ë¸» °ø°Ý Á¤Áö");

					m_isGoing = false;

					if (IsWalking())
						EndWalking();

					SCRIPT_SetPixelPosition(kPPosDst.x, kPPosDst.y);
					BlendRotation(fRotDst);

					RunNormalAttack(fRotDst);

					//Tracen("°¡±õ±â ¶§¹®¿¡ ¿öÇÁ °ø°Ý");
				}
				break;
			}

			case FUNC_MOB_SKILL:
			{
				if (fDirLen >= 50.0f)
				{
					NEW_SetSrcPixelPosition(kPPosCur);
					NEW_SetDstPixelPosition(kPPosDst);
					m_fDstRot = fRotDst;
					m_isGoing = true;
					__EnableSkipCollision();

					m_kMovAfterFunc.eFunc = FUNC_MOB_SKILL;
					m_kMovAfterFunc.uArg = uArg;

					if (!IsWalking())
						StartWalking();
				}
				else
				{
					m_isGoing = false;

					if (IsWalking())
						EndWalking();

					SCRIPT_SetPixelPosition(kPPosDst.x, kPPosDst.y);
					BlendRotation(fRotDst);

					m_GraphicThingInstance.InterceptOnceMotion(CRaceMotionData::NAME_SPECIAL_1 + uArg);
				}
				break;
			}

			case FUNC_EMOTION:
			{
				if (fDirLen>100.0f)
				{
					NEW_SetSrcPixelPosition(kPPosCur);
					NEW_SetDstPixelPosition(kPPosDst);
					m_fDstRot = fRotDst;
					m_isGoing = true;

					if (__IsMainInstance())
						__EnableSkipCollision();

					m_kMovAfterFunc.eFunc = FUNC_EMOTION;
					m_kMovAfterFunc.uArg = uArg;
					m_kMovAfterFunc.uArgExpanded = uTargetVID;
					m_kMovAfterFunc.kPosDst = kPPosDst;

					if (!IsWalking())
						StartWalking();
				}
				else
				{
					__ProcessFunctionEmotion(uArg, uTargetVID, kPPosDst);
				}
				break;
			}

			default:
			{
				if (eFunc & FUNC_SKILL)
				{
					if (fDirLen >= 50.0f)
					{
						//NEW_GetSrcPixelPositionRef() = kPPosCur;
						//NEW_GetDstPixelPositionRef() = kPPosDst;
						NEW_SetSrcPixelPosition(kPPosCur);
						NEW_SetDstPixelPosition(kPPosDst);
						m_fDstRot = fRotDst;
						m_isGoing = true;
						//m_isSyncMov = TRUE;
						__EnableSkipCollision();

						m_kMovAfterFunc.eFunc = eFunc;
						m_kMovAfterFunc.uArg = uArg;

						if (!IsWalking())
							StartWalking();

						//Tracen("³Ê¹« ¸Ö¾î¼­ ÀÌµ¿ ÈÄ °ø°Ý");
					}
					else
					{
						//Tracen("½ºÅ³ Á¤Áö");

						m_isGoing = false;

						if (IsWalking())
							EndWalking();

						SCRIPT_SetPixelPosition(kPPosDst.x, kPPosDst.y);
						SetAdvancingRotation(fRotDst);
						SetRotation(fRotDst);

						NEW_UseSkill(0, eFunc & 0x7f, uArg&0x0f, (uArg>>4) ? true : false);
						//Tracen("°¡±õ±â ¶§¹®¿¡ ¿öÇÁ °ø°Ý");
					}
				}
				break;
			}
		}
	}
}


void CInstanceBase::MovementProcess()
{
	TPixelPosition kPPosCur;
	NEW_GetPixelPosition(&kPPosCur);

	// ·»´õ¸µ ÁÂÇ¥°èÀÌ¹Ç·Î y¸¦ -È­ÇØ¼­ ´õÇÑ´Ù.

	TPixelPosition kPPosNext;
	{
		const D3DXVECTOR3 & c_rkV3Mov = m_GraphicThingInstance.GetMovementVectorRef();

		kPPosNext.x = kPPosCur.x + (+c_rkV3Mov.x);
		kPPosNext.y = kPPosCur.y + (-c_rkV3Mov.y);
		kPPosNext.z = kPPosCur.z + (+c_rkV3Mov.z);
	}

	TPixelPosition kPPosDeltaSC = kPPosCur - NEW_GetSrcPixelPositionRef();
	TPixelPosition kPPosDeltaSN = kPPosNext - NEW_GetSrcPixelPositionRef();
	TPixelPosition kPPosDeltaSD = NEW_GetDstPixelPositionRef() - NEW_GetSrcPixelPositionRef();

	float fCurLen = sqrtf(kPPosDeltaSC.x * kPPosDeltaSC.x + kPPosDeltaSC.y * kPPosDeltaSC.y);
	float fNextLen = sqrtf(kPPosDeltaSN.x * kPPosDeltaSN.x + kPPosDeltaSN.y * kPPosDeltaSN.y);
	float fTotalLen = sqrtf(kPPosDeltaSD.x * kPPosDeltaSD.x + kPPosDeltaSD.y * kPPosDeltaSD.y);
	float fRestLen = fTotalLen - fCurLen;

	if (__IsMainInstance())
	{
		if (m_isGoing && IsWalking())
		{
			float fDstRot = NEW_GetAdvancingRotationFromPixelPosition(NEW_GetSrcPixelPositionRef(), NEW_GetDstPixelPositionRef());

			SetAdvancingRotation(fDstRot);

			if (fRestLen<=0.0)
			{
				if (IsWalking())
					EndWalking();

				//Tracen("¸ñÇ¥ µµ´Þ Á¤Áö");

				m_isGoing = false;

				BlockMovement();

				if (FUNC_EMOTION == m_kMovAfterFunc.eFunc)
				{
					uint32_t dwMotionNumber = m_kMovAfterFunc.uArg;
					uint32_t dwTargetVID = m_kMovAfterFunc.uArgExpanded;
					__ProcessFunctionEmotion(dwMotionNumber, dwTargetVID, m_kMovAfterFunc.kPosDst);
					m_kMovAfterFunc.eFunc = FUNC_WAIT;
					return;
				}
			}
		}
	}
	else
	{
		if (m_isGoing && IsWalking())
		{
			float fDstRot = NEW_GetAdvancingRotationFromPixelPosition(NEW_GetSrcPixelPositionRef(), NEW_GetDstPixelPositionRef());

			SetAdvancingRotation(fDstRot);

			// ¸¸¾à ·»ÅÏ½Ã°¡ ´Ê¾î ³Ê¹« ¸¹ÀÌ ÀÌµ¿Çß´Ù¸é..
			if (fRestLen < -100.0f)
			{
				NEW_SetSrcPixelPosition(kPPosCur);

				float fDstRot = NEW_GetAdvancingRotationFromPixelPosition(kPPosCur, NEW_GetDstPixelPositionRef());
				SetAdvancingRotation(fDstRot);
				//Tracenf("VID %d ¿À¹ö ¹æÇâ¼³Á¤ (%f, %f) %f rest %f", GetVirtualID(), kPPosCur.x, kPPosCur.y, fDstRot, fRestLen);

				// ÀÌµ¿ÁßÀÌ¶ó¸é ´ÙÀ½¹ø¿¡ ¸ØÃß°Ô ÇÑ´Ù
				if (FUNC_MOVE == m_kMovAfterFunc.eFunc)
				{
					m_kMovAfterFunc.eFunc = FUNC_WAIT;
				}
			}
			// µµÂøÇß´Ù¸é...
			else if (fCurLen <= fTotalLen && fTotalLen <= fNextLen)
			{
				if (m_GraphicThingInstance.IsDead() || m_GraphicThingInstance.IsKnockDown())
				{
					__DisableSkipCollision();

					//Tracen("»ç¸Á »óÅÂ¶ó µ¿ÀÛ ½ºÅµ");

					m_isGoing = false;

					//Tracen("Çàµ¿ ºÒ´É »óÅÂ¶ó ÀÌÈÄ µ¿ÀÛ ½ºÅµ");
				}
				else
				{
					switch (m_kMovAfterFunc.eFunc)
					{
						case FUNC_ATTACK:
						{
							if (IsWalking())
								EndWalking();

							__DisableSkipCollision();
							m_isGoing = false;

							BlockMovement();
							SCRIPT_SetPixelPosition(NEW_GetDstPixelPositionRef().x, NEW_GetDstPixelPositionRef().y);
							SetAdvancingRotation(m_fDstRot);
							SetRotation(m_fDstRot);

							RunNormalAttack(m_fDstRot);
							break;
						}

						case FUNC_COMBO:
						{
							if (IsWalking())
								EndWalking();

							__DisableSkipCollision();
							m_isGoing = false;

							BlockMovement();
							SCRIPT_SetPixelPosition(NEW_GetDstPixelPositionRef().x, NEW_GetDstPixelPositionRef().y);
							RunComboAttack(m_fDstRot, m_kMovAfterFunc.uArg);
							break;
						}

						case FUNC_EMOTION:
						{
							m_isGoing = false;
							m_kMovAfterFunc.eFunc = FUNC_WAIT;
							__DisableSkipCollision();
							BlockMovement();

							uint32_t dwMotionNumber = m_kMovAfterFunc.uArg;
							uint32_t dwTargetVID = m_kMovAfterFunc.uArgExpanded;
							__ProcessFunctionEmotion(dwMotionNumber, dwTargetVID, m_kMovAfterFunc.kPosDst);
							break;
						}

						case FUNC_MOVE:
						{
							break;
						}

						case FUNC_MOB_SKILL:
						{
							if (IsWalking())
								EndWalking();

							__DisableSkipCollision();
							m_isGoing = false;

							BlockMovement();
							SCRIPT_SetPixelPosition(NEW_GetDstPixelPositionRef().x, NEW_GetDstPixelPositionRef().y);
							SetAdvancingRotation(m_fDstRot);
							SetRotation(m_fDstRot);

							m_GraphicThingInstance.InterceptOnceMotion(CRaceMotionData::NAME_SPECIAL_1 + m_kMovAfterFunc.uArg);
							break;
						}

						default:
						{
							if (m_kMovAfterFunc.eFunc & FUNC_SKILL)
							{
								SetAdvancingRotation(m_fDstRot);
								BlendRotation(m_fDstRot);
								NEW_UseSkill(0, m_kMovAfterFunc.eFunc & 0x7f, m_kMovAfterFunc.uArg&0x0f, (m_kMovAfterFunc.uArg>>4) ? true : false);
							}
							else
							{
								//Tracenf("VID %d ½ºÅ³ °ø°Ý (%f, %f) rot %f", GetVirtualID(), NEW_GetDstPixelPositionRef().x, NEW_GetDstPixelPositionRef().y, m_fDstRot);

								__DisableSkipCollision();
								m_isGoing = FALSE;

								BlockMovement();
								SCRIPT_SetPixelPosition(NEW_GetDstPixelPositionRef().x, NEW_GetDstPixelPositionRef().y);
								SetAdvancingRotation(m_fDstRot);
								BlendRotation(m_fDstRot);
								if (!IsWaiting())
								{
									EndWalking();
								}

								//Tracenf("VID %d Á¤Áö (%f, %f) rot %f IsWalking %d", GetVirtualID(), NEW_GetDstPixelPositionRef().x, NEW_GetDstPixelPositionRef().y, m_fDstRot, IsWalking());
							}
							break;
						}
					}

				}
			}

		}
	}

	if (IsWalking() || m_GraphicThingInstance.IsUsingMovingSkill())
	{
		float fRotation = m_GraphicThingInstance.GetRotation();
		float fAdvancingRotation = m_GraphicThingInstance.GetAdvancingRotation();
		int iDirection = GetRotatingDirection(fRotation, fAdvancingRotation);

		if (DEGREE_DIRECTION_SAME != m_iRotatingDirection)
		{
			if (DEGREE_DIRECTION_LEFT == iDirection)
			{
				fRotation = fmodf(fRotation + m_fRotSpd*m_GraphicThingInstance.GetSecondElapsed(), 360.0f);
			}
			else if (DEGREE_DIRECTION_RIGHT == iDirection)
			{
				fRotation = fmodf(fRotation - m_fRotSpd*m_GraphicThingInstance.GetSecondElapsed() + 360.0f, 360.0f);
			}

			if (m_iRotatingDirection != GetRotatingDirection(fRotation, fAdvancingRotation))
			{
				m_iRotatingDirection = DEGREE_DIRECTION_SAME;
				fRotation = fAdvancingRotation;
			}

			m_GraphicThingInstance.SetRotation(fRotation);
		}

		if (__IsInDustRange())
		{
			float fDustDistance = NEW_GetDistanceFromDestPixelPosition(m_kPPosDust);
			if (IsMountingHorse())
			{
				if (fDustDistance > ms_fHorseDustGap)
				{
					NEW_GetPixelPosition(&m_kPPosDust);
					__AttachEffect(EFFECT_HORSE_DUST);
				}
			}
			else
			{
				if (fDustDistance > ms_fDustGap)
				{
					NEW_GetPixelPosition(&m_kPPosDust);
					__AttachEffect(EFFECT_DUST);
				}
			}
		}
	}
}

void CInstanceBase::__ProcessFunctionEmotion(uint32_t dwMotionNumber, uint32_t dwTargetVID, const TPixelPosition & c_rkPosDst)
{
	if (IsWalking())
		EndWalkingWithoutBlending();

	__EnableChangingTCPState();
	SCRIPT_SetPixelPosition(c_rkPosDst.x, c_rkPosDst.y);

	CInstanceBase * pTargetInstance = CPythonCharacterManager::Instance().GetInstancePtr(dwTargetVID);
	if (pTargetInstance)
	{
		pTargetInstance->__EnableChangingTCPState();

		if (pTargetInstance->IsWalking())
			pTargetInstance->EndWalkingWithoutBlending();

		WORD wMotionNumber1 = HIWORD(dwMotionNumber);
		WORD wMotionNumber2 = LOWORD(dwMotionNumber);

		int src_job = RaceToJob(GetRace());
		int dst_job = RaceToJob(pTargetInstance->GetRace());

		NEW_LookAtDestInstance(*pTargetInstance);
		m_GraphicThingInstance.InterceptOnceMotion(wMotionNumber1 + dst_job);
		m_GraphicThingInstance.SetRotation(m_GraphicThingInstance.GetTargetRotation());
		m_GraphicThingInstance.SetAdvancingRotation(m_GraphicThingInstance.GetTargetRotation());

		pTargetInstance->NEW_LookAtDestInstance(*this);
		pTargetInstance->m_GraphicThingInstance.InterceptOnceMotion(wMotionNumber2 + src_job);
		pTargetInstance->m_GraphicThingInstance.SetRotation(pTargetInstance->m_GraphicThingInstance.GetTargetRotation());
		pTargetInstance->m_GraphicThingInstance.SetAdvancingRotation(pTargetInstance->m_GraphicThingInstance.GetTargetRotation());

		if (pTargetInstance->__IsMainInstance())
		{
			IAbstractPlayer & rPlayer=IAbstractPlayer::GetSingleton();
			rPlayer.EndEmotionProcess();
		}
	}

	if (__IsMainInstance())
	{
		IAbstractPlayer & rPlayer=IAbstractPlayer::GetSingleton();
		rPlayer.EndEmotionProcess();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Update & Deform & Render

int g_iAccumulationTime = 0;

void CInstanceBase::Update()
{
	++ms_dwUpdateCounter;

	StateProcess();
	m_GraphicThingInstance.PhysicsProcess();
	m_GraphicThingInstance.RotationProcess();
	m_GraphicThingInstance.ComboProcess();
	m_GraphicThingInstance.AccumulationMovement();

	if (m_GraphicThingInstance.IsMovement())
	{
		TPixelPosition kPPosCur;
		NEW_GetPixelPosition(&kPPosCur);

		uint32_t dwCurTime=ELTimer_GetFrameMSec();
		//if (m_dwNextUpdateHeightTime<dwCurTime)
		{
			m_dwNextUpdateHeightTime=dwCurTime;
			kPPosCur.z = __GetBackgroundHeight(kPPosCur.x, kPPosCur.y);
			NEW_SetPixelPosition(kPPosCur);
		}

		// SetMaterialColor
		{
			uint32_t dwMtrlColor=__GetShadowMapColor(kPPosCur.x, kPPosCur.y);
			m_GraphicThingInstance.SetMaterialColor(dwMtrlColor);
		}
	}

	m_GraphicThingInstance.UpdateAdvancingPointInstance();

	if (m_GraphicThingInstance.IsMount()) {
		if (CPythonSystem::instance().GetHideMode1Status()) {
			if (!IsAffect(AFFECT_INVISIBILITY)) {
				__SetAffect(AFFECT_INVISIBILITY, true);
				m_kAffectFlagContainer.Set(AFFECT_INVISIBILITY, true);
			}
		} else {
			if (IsAffect(AFFECT_INVISIBILITY)) {
				__SetAffect(AFFECT_INVISIBILITY, false);
				m_kAffectFlagContainer.Set(AFFECT_INVISIBILITY, false);
			}
		}
	}

	if (m_GraphicThingInstance.IsPet()) {
		if (CPythonSystem::instance().GetHideMode2Status()) {
			if (!IsAffect(AFFECT_INVISIBILITY)) {
				__SetAffect(AFFECT_INVISIBILITY, true);
				m_kAffectFlagContainer.Set(AFFECT_INVISIBILITY, true);
			}
		} else {
			if (IsAffect(AFFECT_INVISIBILITY)) {
				__SetAffect(AFFECT_INVISIBILITY, false);
				m_kAffectFlagContainer.Set(AFFECT_INVISIBILITY, false);
			}
		}
	}

	if (GetRace() >= 30000 && GetRace() <= 30007) {
		if (CPythonSystem::instance().GetHideMode4Status()) {
			if (!IsAffect(AFFECT_INVISIBILITY)) {
				__SetAffect(AFFECT_INVISIBILITY, true);
				m_kAffectFlagContainer.Set(AFFECT_INVISIBILITY, true);
			}
		} else {
			if (IsAffect(AFFECT_INVISIBILITY)) {
				__SetAffect(AFFECT_INVISIBILITY, false);
				m_kAffectFlagContainer.Set(AFFECT_INVISIBILITY, false);
			}
		}
	}

	AttackProcess();
	MovementProcess();

	m_GraphicThingInstance.MotionProcess(IsPC());
	if (IsMountingHorse())
	{
		m_kHorse.m_pkActor->HORSE_MotionProcess(FALSE);
	}

	__ComboProcess();
	while (ProcessDamage());
}

void CInstanceBase::Transform()
{
	if (__IsSyncing())
	{
		//OnSyncing();
	}
	else
	{
		if (IsWalking() || m_GraphicThingInstance.IsUsingMovingSkill())
		{
			const D3DXVECTOR3& c_rv3Movment=m_GraphicThingInstance.GetMovementVectorRef();

			float len=(c_rv3Movment.x*c_rv3Movment.x)+(c_rv3Movment.y*c_rv3Movment.y);
			if (len>1.0f)
				OnMoving();
			else
				OnWaiting();
		}
	}
	
	m_GraphicThingInstance.INSTANCEBASE_Transform();
}


void CInstanceBase::Deform()
{
	// 2004.07.17.levites.isShow¸¦ ViewFrustumCheck·Î º¯°æ
	if (!__CanRender())
		return;

	++ms_dwDeformCounter;

	m_GraphicThingInstance.INSTANCEBASE_Deform();

	m_kHorse.Deform();
}

void CInstanceBase::RenderTrace()
{
	if (!__CanRender())
		return;

	m_GraphicThingInstance.RenderTrace();
}




void CInstanceBase::Render()
{
	// 2004.07.17.levites.isShow¸¦ ViewFrustumCheck·Î º¯°æ
	if (!__CanRender())
		return;

	++ms_dwRenderCounter;

	m_kHorse.Render();
	m_GraphicThingInstance.Render();

	if (CActorInstance::IsDirLine())
	{
		if (NEW_GetDstPixelPositionRef().x != 0.0f)
		{
			static CScreen s_kScreen;

			STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG1,	D3DTA_DIFFUSE);
			STATEMANAGER.SetTextureStageState(0, D3DTSS_COLOROP,	D3DTOP_SELECTARG1);
			STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAOP,	D3DTOP_DISABLE);
			STATEMANAGER.SaveRenderState(D3DRS_ZENABLE, FALSE);
			STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, FALSE);
			STATEMANAGER.SetRenderState(D3DRS_LIGHTING, FALSE);

			TPixelPosition px;
			m_GraphicThingInstance.GetPixelPosition(&px);
			D3DXVECTOR3 kD3DVt3Cur(px.x, px.y, px.z);
			//D3DXVECTOR3 kD3DVt3Cur(NEW_GetSrcPixelPositionRef().x, -NEW_GetSrcPixelPositionRef().y, NEW_GetSrcPixelPositionRef().z);
			D3DXVECTOR3 kD3DVt3Dest(NEW_GetDstPixelPositionRef().x, -NEW_GetDstPixelPositionRef().y, NEW_GetDstPixelPositionRef().z);

			//printf("%s %f\n", GetNameString(), kD3DVt3Cur.y - kD3DVt3Dest.y);
			//float fdx = NEW_GetDstPixelPositionRef().x - NEW_GetSrcPixelPositionRef().x;
			//float fdy = NEW_GetDstPixelPositionRef().y - NEW_GetSrcPixelPositionRef().y;

			s_kScreen.SetDiffuseColor(0.0f, 0.0f, 1.0f);
			s_kScreen.RenderLine3d(kD3DVt3Cur.x, kD3DVt3Cur.y, px.z, kD3DVt3Dest.x, kD3DVt3Dest.y, px.z);
			STATEMANAGER.RestoreRenderState(D3DRS_ZENABLE);
			STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, TRUE);
			STATEMANAGER.SetRenderState(D3DRS_LIGHTING, TRUE);
		}
	}
}

void CInstanceBase::RenderToShadowMap()
{
	if (IsDoor())
		return;

	if (IsBuilding())
		return;

	if (!__CanRender())
		return;

	if (!__IsExistMainInstance())
		return;

	CInstanceBase* pkInstMain=__GetMainInstancePtr();

	const float SHADOW_APPLY_DISTANCE = 2500.0f;

	float fDistance=NEW_GetDistanceFromDestInstance(*pkInstMain);
	if (fDistance>=SHADOW_APPLY_DISTANCE)
		return;

	m_GraphicThingInstance.RenderToShadowMap();
}

void CInstanceBase::RenderCollision()
{
	m_GraphicThingInstance.RenderCollisionData();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Setting & Getting Data

void CInstanceBase::SetVirtualID(uint32_t dwVirtualID)
{
	m_GraphicThingInstance.SetVirtualID(dwVirtualID);
}

void CInstanceBase::SetVirtualNumber(uint32_t dwVirtualNumber)
{
	m_dwVirtualNumber = dwVirtualNumber;
}

void CInstanceBase::SetInstanceType(int iInstanceType)
{
	m_GraphicThingInstance.SetActorType(iInstanceType);
}

void CInstanceBase::SetAlignment(uint32_t sAlignment)
{
	m_sAlignment = sAlignment;
	RefreshTextTailTitle();
}

#ifdef NEW_PET_SYSTEM
void CInstanceBase::SetLevelText(int mLevel)
{		
	m_dwLevel = mLevel;
	UpdateTextTailLevel(m_dwLevel);
}
#endif

void CInstanceBase::SetPKMode(uint8_t byPKMode)
{
	if (m_byPKMode == byPKMode)
		return;

	m_byPKMode = byPKMode;

	if (__IsMainInstance())
	{
		IAbstractPlayer& rPlayer=IAbstractPlayer::GetSingleton();
		rPlayer.NotifyChangePKMode();
	}
}

void CInstanceBase::SetKiller(bool bFlag)
{
	if (m_isKiller == bFlag)
		return;

	m_isKiller = bFlag;
	RefreshTextTail();
}

void CInstanceBase::SetPartyMemberFlag(bool bFlag)
{
	m_isPartyMember = bFlag;
}

void CInstanceBase::SetStateFlags(uint32_t dwStateFlags)
{
	if (dwStateFlags & ADD_CHARACTER_STATE_KILLER)
		SetKiller(TRUE);
	else
		SetKiller(FALSE);

	if (dwStateFlags & ADD_CHARACTER_STATE_PARTY)
		SetPartyMemberFlag(TRUE);
	else
		SetPartyMemberFlag(FALSE);
}

void CInstanceBase::SetComboType(UINT uComboType)
{
	m_GraphicThingInstance.SetComboType(uComboType);
}

const char * CInstanceBase::GetNameString()
{
	return m_stName.c_str();
}

#ifdef ENABLE_LEVEL_IN_TRADE
uint32_t CInstanceBase::GetLevel()
{
	return m_dwLevel;
}
#endif

#ifdef ENABLE_TEXT_LEVEL_REFRESH
void CInstanceBase::SetLevel(uint32_t dwLevel)
{
	m_dwLevel = dwLevel;
}
#endif

#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
void CInstanceBase::MobInfoAiFlagRefresh()
{
	// set
	std::string strName = CPythonNonPlayer::Instance().GetMonsterName(GetRace());
	//if (CPythonSystem::Instance().IsShowMobAIFlag() && CPythonNonPlayer::Instance().IsAggressive(GetRace()))
	//	strName += "*";
	SetNameString(strName.c_str(), strName.length());
	// refresh
	DetachTextTail();
	AttachTextTail();
	RefreshTextTail();
}
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
void CInstanceBase::MobInfoLevelRefresh()
{
	// set
	if (CPythonSystem::Instance().IsShowMobLevel())
		m_dwLevel = CPythonNonPlayer::Instance().GetMonsterLevel(GetRace());
	else
		m_dwLevel = 0;
	// refresh
	if (m_dwLevel)
		UpdateTextTailLevel(m_dwLevel);
	else
		CPythonTextTail::Instance().DetachLevel(GetVirtualID());
}
#endif

uint32_t CInstanceBase::GetRace()
{
	return m_dwRace;
}

uint32_t CInstanceBase::GetRank()
{
	if (!IsEnemy())
		return 0;
	
	return CPythonNonPlayer::Instance().GetMonsterRank(GetRace());
}

//bool CInstanceBase::IsConflictAlignmentInstance(CInstanceBase& rkInstVictim)
//{
//	if (PK_MODE_PROTECT == rkInstVictim.GetPKMode())
//		return false;
//
//	switch (GetAlignmentType())
//	{
//		case ALIGNMENT_TYPE_NORMAL:
//		case ALIGNMENT_TYPE_WHITE:
//			if (ALIGNMENT_TYPE_DARK == rkInstVictim.GetAlignmentType())
//				return true;
//			break;
//		case ALIGNMENT_TYPE_DARK:
//			if (GetAlignmentType() != rkInstVictim.GetAlignmentType())
//				return true;
//			break;
//	}
//
//	return false;
//}

void CInstanceBase::SetDuelMode(uint32_t type)
{
	m_dwDuelMode = type;
}

uint32_t CInstanceBase::GetDuelMode()
{
	return m_dwDuelMode;
}

bool CInstanceBase::IsAttackableInstance(CInstanceBase& rkInstVictim)
{
	if (__IsMainInstance())
	{
		CPythonPlayer& rkPlayer=CPythonPlayer::Instance();
		if(rkPlayer.IsObserverMode())
			return false;
	}

	if (GetVirtualID() == rkInstVictim.GetVirtualID())
		return false;

	if (IsStone())
	{
		if (rkInstVictim.IsPC())
			return true;
	}
	else if (IsPC())
	{
		if (rkInstVictim.IsStone())
			return true;

		if (rkInstVictim.IsPC())
		{
			if (GetDuelMode())
			{
				switch(GetDuelMode())
				{
				case DUEL_CANNOTATTACK:
					return false;
				case DUEL_START:
					if(__FindDUELKey(GetVirtualID(),rkInstVictim.GetVirtualID()))
						return true;
					else
						return false;
				}
			}
			if (PK_MODE_GUILD == GetPKMode())
				if (GetGuildID() == rkInstVictim.GetGuildID())
					return false;

			if (rkInstVictim.IsKiller())
				if (!IAbstractPlayer::GetSingleton().IsSamePartyMember(GetVirtualID(), rkInstVictim.GetVirtualID()))
					return true;

			if (PK_MODE_PROTECT != GetPKMode())
			{
				if (PK_MODE_FREE == GetPKMode())
				{
					if (PK_MODE_PROTECT != rkInstVictim.GetPKMode())
						if (!IAbstractPlayer::GetSingleton().IsSamePartyMember(GetVirtualID(), rkInstVictim.GetVirtualID()))
							return true;
				}
				if (PK_MODE_GUILD == GetPKMode())
				{
					if (PK_MODE_PROTECT != rkInstVictim.GetPKMode())
						if (!IAbstractPlayer::GetSingleton().IsSamePartyMember(GetVirtualID(), rkInstVictim.GetVirtualID()))
							if (GetGuildID() != rkInstVictim.GetGuildID())
								return true;
				}
			}

			if (IsSameEmpire(rkInstVictim))
			{
				if (IsPVPInstance(rkInstVictim))
					return true;

				if (PK_MODE_REVENGE == GetPKMode())
					if (!IAbstractPlayer::GetSingleton().IsSamePartyMember(GetVirtualID(), rkInstVictim.GetVirtualID()))
						//if (IsConflictAlignmentInstance(rkInstVictim))
							return true;
			}
			else
			{
				return true;
			}
		}

		if (rkInstVictim.IsEnemy())
			return true;

		if (rkInstVictim.IsWoodenDoor())
			return true;
	}
	else if (IsEnemy())
	{
		if (rkInstVictim.IsPC())
			return true;

		if (rkInstVictim.IsBuilding())
			return true;

	}
	else if (IsPoly())
	{
		if (rkInstVictim.IsPC())
			return true;

		if (rkInstVictim.IsEnemy())
			return true;
	}
	return false;
}

bool CInstanceBase::IsTargetableInstance(CInstanceBase& rkInstVictim)
{
	return rkInstVictim.CanPickInstance();
}

// 2004. 07. 07. [levites] - ½ºÅ³ »ç¿ëÁß Å¸°ÙÀÌ ¹Ù²î´Â ¹®Á¦ ÇØ°áÀ» À§ÇÑ ÄÚµå
bool CInstanceBase::CanChangeTarget()
{
	return m_GraphicThingInstance.CanChangeTarget();
}

// 2004.07.17.levites.isShow¸¦ ViewFrustumCheck·Î º¯°æ
bool CInstanceBase::CanPickInstance()
{
	if (!__IsInViewFrustum())
		return false;

	if (IsDoor())
	{
		if (IsDead())
			return false;
	}

	if (IsPC())
	{
		if (IsAffect(AFFECT_EUNHYEONG))
		{
			if (!__MainCanSeeHiddenThing())
				return false;
		}
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
		if (IsAffect(AFFECT_REVIVE_INVISIBILITY) && !__MainCanSeeHiddenThing())
			return false;
#else
		if (IsAffect(AFFECT_REVIVE_INVISIBILITY))
			return false;
#endif
#ifdef ENABLE_CANSEEHIDDENTHING_FOR_GM
		if (IsAffect(AFFECT_INVISIBILITY) && !__MainCanSeeHiddenThing())
			return false;
#else
		if (IsAffect(AFFECT_INVISIBILITY))
			return false;
#endif

	}

	if (IsDead())
		return false;

	return true;
}

bool CInstanceBase::CanViewTargetHP(CInstanceBase& rkInstVictim)
{
	if (rkInstVictim.IsStone())
		return true;
	if (rkInstVictim.IsWoodenDoor())
		return true;
	if (rkInstVictim.IsEnemy())
		return true;
#ifdef ENABLE_VIEW_TARGET_PLAYER_HP
	if (rkInstVictim.IsPC())
		return true;
#endif

	return false;
}

bool CInstanceBase::IsPoly()
{
	return m_GraphicThingInstance.IsPoly();
}

bool CInstanceBase::IsPC()
{
	return m_GraphicThingInstance.IsPC();
}

bool CInstanceBase::IsNPC()
{
	return m_GraphicThingInstance.IsNPC();
}

#ifdef NEW_PET_SYSTEM
bool CInstanceBase::IsNewPet()
{
	return m_GraphicThingInstance.IsNewPet();
}
#endif

bool CInstanceBase::IsEnemy()
{
	return m_GraphicThingInstance.IsEnemy();
}

bool CInstanceBase::IsStone()
{
	return m_GraphicThingInstance.IsStone();
}


bool CInstanceBase::IsGuildWall()	//IsBuilding ±æµå°Ç¹°ÀüÃ¼ IsGuildWallÀº ´ãÀåº®¸¸(¹®Àº Á¦¿Ü)
{
	return IsWall(m_dwRace);
}


bool CInstanceBase::IsResource()
{
	switch (m_dwVirtualNumber)
	{
		case 20047:
		case 20048:
		case 20049:
		case 20050:
		case 20051:
		case 20052:
		case 20053:
		case 20054:
		case 20055:
		case 20056:
		case 20057:
		case 20058:
		case 20059:
		case 30301:
		case 30302:
		case 30303:
		case 30304:
		case 30305:
		case 30306:
			return true;
	}

	return false;
}

bool CInstanceBase::IsWarp()
{
	return m_GraphicThingInstance.IsWarp();
}

bool CInstanceBase::IsGoto()
{
	return m_GraphicThingInstance.IsGoto();
}

bool CInstanceBase::IsObject()
{
	return m_GraphicThingInstance.IsObject();
}

bool CInstanceBase::IsBuilding()
{
	return m_GraphicThingInstance.IsBuilding();
}

bool CInstanceBase::IsDoor()
{
	return m_GraphicThingInstance.IsDoor();
}

bool CInstanceBase::IsWoodenDoor()
{
	if (m_GraphicThingInstance.IsDoor())
	{
		int vnum = GetVirtualNumber();
		if (vnum == 13000) // ³ª¹«¹®
			return true;
		else if (vnum >= 30111 && vnum <= 30119) // »ç±Í¹®
			return true;
		else
			return false;
	}
	else
	{
		return false;
	}
}

bool CInstanceBase::IsStoneDoor()
{
	return m_GraphicThingInstance.IsDoor() && 13001 == GetVirtualNumber();
}

bool CInstanceBase::IsFlag()
{
	if (GetRace() == 20035)
		return true;
	if (GetRace() == 20036)
		return true;
	if (GetRace() == 20037)
		return true;

	return false;
}

bool CInstanceBase::IsForceVisible()
{
	if (IsAffect(AFFECT_SHOW_ALWAYS))
		return true;

	if (IsObject() || IsBuilding() || IsDoor() )
		return true;

	return false;
}

int	CInstanceBase::GetInstanceType()
{
	return m_GraphicThingInstance.GetActorType();
}

uint32_t CInstanceBase::GetVirtualID()
{
	return m_GraphicThingInstance.GetVirtualID();
}

uint32_t CInstanceBase::GetVirtualNumber()
{
	return m_dwVirtualNumber;
}

// 2004.07.17.levites.isShow¸¦ ViewFrustumCheck·Î º¯°æ
bool CInstanceBase::__IsInViewFrustum()
{
	return m_GraphicThingInstance.isShow();
}

bool CInstanceBase::__CanRender()
{
	if (IsAlwaysRender())
	{
		return true;
	}
	
	if (!__IsInViewFrustum())
		return false;
	if (IsAffect(AFFECT_INVISIBILITY))
		return false;

	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Graphic Control

bool CInstanceBase::IntersectBoundingBox()
{
	float u, v, t;
	return m_GraphicThingInstance.Intersect(&u, &v, &t);
}

bool CInstanceBase::IntersectDefendingSphere()
{
	return m_GraphicThingInstance.IntersectDefendingSphere();
}

float CInstanceBase::GetDistance(CInstanceBase * pkTargetInst)
{
	TPixelPosition TargetPixelPosition;
	pkTargetInst->m_GraphicThingInstance.GetPixelPosition(&TargetPixelPosition);
	return GetDistance(TargetPixelPosition);
}

float CInstanceBase::GetDistance(const TPixelPosition & c_rPixelPosition)
{
	TPixelPosition PixelPosition;
	m_GraphicThingInstance.GetPixelPosition(&PixelPosition);

	float fdx = PixelPosition.x - c_rPixelPosition.x;
	float fdy = PixelPosition.y - c_rPixelPosition.y;

	return sqrtf((fdx*fdx) + (fdy*fdy));
}

#ifdef ENABLE_RACE_HEIGHT
float CInstanceBase::GetBaseHeight()
{
	CActorInstance* pkHorse = m_kHorse.GetActorPtr();
	if ((!pkHorse) || (!m_kHorse.IsMounting()))
		return 0.0f;
	
	uint32_t dwMountVnum = m_kHorse.m_pkActor->GetRace();
	return CRaceManager::instance().GetRidingRaceHeight(dwMountVnum);
}
#endif

CActorInstance& CInstanceBase::GetGraphicThingInstanceRef()
{
	return m_GraphicThingInstance;
}

CActorInstance* CInstanceBase::GetGraphicThingInstancePtr()
{
	return &m_GraphicThingInstance;
}

void CInstanceBase::RefreshActorInstance()
{
	m_GraphicThingInstance.RefreshActorInstance();
}

void CInstanceBase::Refresh(uint32_t dwMotIndex, bool isLoop)
{
	RefreshState(dwMotIndex, isLoop);
}

void CInstanceBase::RestoreRenderMode()
{
	m_GraphicThingInstance.RestoreRenderMode();
}

void CInstanceBase::SetAddRenderMode()
{
	m_GraphicThingInstance.SetAddRenderMode();
}

void CInstanceBase::SetModulateRenderMode()
{
	m_GraphicThingInstance.SetModulateRenderMode();
}

void CInstanceBase::SetRenderMode(int iRenderMode)
{
	m_GraphicThingInstance.SetRenderMode(iRenderMode);
}

void CInstanceBase::SetAddColor(const D3DXCOLOR & c_rColor)
{
	m_GraphicThingInstance.SetAddColor(c_rColor);
}

void CInstanceBase::__SetBlendRenderingMode()
{
	m_GraphicThingInstance.SetBlendRenderMode();
}

void CInstanceBase::__SetAlphaValue(float fAlpha)
{
	m_GraphicThingInstance.SetAlphaValue(fAlpha);
}

float CInstanceBase::__GetAlphaValue()
{
	return m_GraphicThingInstance.GetAlphaValue();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Part

void CInstanceBase::SetHair(uint32_t eHair)
{
	if (!HAIR_COLOR_ENABLE)
		return;

	if (IsPC()==false)
		return;
	m_awPart[CRaceData::PART_HAIR] = eHair;
	m_GraphicThingInstance.SetHair(eHair);
}

void CInstanceBase::ChangeHair(uint32_t eHair)
{
	if (!HAIR_COLOR_ENABLE)
		return;

	if (IsPC()==false)
		return;

	if (GetPart(CRaceData::PART_HAIR)==eHair)
		return;

	SetHair(eHair);

	//int type = m_GraphicThingInstance.GetMotionMode();

	RefreshState(CRaceMotionData::NAME_WAIT, true);
	//RefreshState(type, true);
}

void CInstanceBase::SetArmor(uint32_t dwArmor)
{
	uint32_t dwShape;
	if (__ArmorVnumToShape(dwArmor, &dwShape))
	{
		CItemData * pItemData;
		if (CItemManager::Instance().GetItemDataPointer(dwArmor, &pItemData))
		{
			float fSpecularPower=pItemData->GetSpecularPowerf();
			SetShape(dwShape, fSpecularPower);
			__GetRefinedEffect(pItemData);
#ifdef ENABLE_SHINING_SYSTEM
			__GetShiningEffect(pItemData);
#endif
			return;
		}
		else
			__ClearArmorRefineEffect();
#ifdef ENABLE_SHINING_SYSTEM
		__ClearArmorShiningEffect();
#endif
	}

	SetShape(dwArmor);
}



#ifdef ENABLE_ACCE_SYSTEM
void CInstanceBase::SetAcce(uint32_t dwAcce, bool wikiPreview)
{
	if (!IsPC())
		return;

	if (IsPoly())
		return;

	if (dwAcce == 0)
	{
		m_awPart[CRaceData::PART_ACCE] = 0;
		m_GraphicThingInstance.AttachAcce(nullptr, 0.0f);
		ClearAcceEffect();
		return;
	}
	else {
		ClearAcceEffect();
	}

	float fSpecular = 65.0f;
	dwAcce += 85000;
	if (dwAcce > 86000)
	{
		dwAcce -= 1000;
#ifdef ENABLE_STOLE_COSTUME
		switch (dwAcce) {
			case 85019:
			case 85020:
			case 85021:
			case 85022:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_GREEN;
				break;
			case 85023:
			case 85024:
			case 85025:
			case 85026:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_BLACK;
				break;
			case 85027:
			case 85028:
			case 85029:
			case 85030:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_ORANGE;
				break;
			case 85031:
			case 85032:
			case 85033:
			case 85034:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_PURPLE;
				break;
			case 85035:
			case 85036:
			case 85037:
			case 85038:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_RED;
				break;
			case 85039:
			case 85040:
			case 85041:
			case 85042:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_WHITE;
				break;
			case 85043:
			case 85044:
			case 85045:
			case 85046:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_YELLOW;
				break;
			case 85047:
			case 85048:
			case 85049:
			case 85050:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_1;
				break;
			case 85051:
			case 85052:
			case 85053:
			case 85054:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_2;
				break;
			case 85055:
			case 85056:
			case 85057:
			case 85058:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_3;
				break;
			case 85059:
			case 85060:
			case 85061:
			case 85062:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_4;
				break;
			case 85063:
			case 85064:
			case 85065:
			case 85066:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_5;
				break;
			case 85067:
			case 85068:
			case 85069:
			case 85070:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_6;
				break;
			case 85083:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_7;
				break;
			case 85088:
			case 85092:
			case 85096:
			case 85100:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_WING;
				break;
			case 85104:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_WING;
				break;
			case 85108:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_WING;
				break;
			case 85112:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_RED;
				break;
			case 85116:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_WING;
				break;
			case 85120:
				{
					m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_WHITE;
				}
				break;
			case 85124:
			case 85128:
				{
					m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_RED;
				}
				break;
			case 85132:
			case 85136:
				{
					m_dwAcceEffect = EFFECT_REFINED + EFFECT_STOLA_YELLOW;
				}
				break;
			case 85140:
			case 85144:
				{
					m_dwAcceEffect = EFFECT_REFINED + EFFECT_WING_COSTUME15;
				}
				break;
			case 85148:
			case 85152:
				{
					m_dwAcceEffect = EFFECT_REFINED + EFFECT_WING_COSTUME16;
				}
				break;
			default:
				m_dwAcceEffect = EFFECT_REFINED + EFFECT_ACCE;
				break;
		}
#endif
		
		fSpecular += 35;
		if (!wikiPreview)
			__EffectContainer_AttachEffect(m_dwAcceEffect);
	}
	
	fSpecular /= 100.0f;
	m_awPart[CRaceData::PART_ACCE] = dwAcce;

	CItemData * pItemData;
	if (!CItemManager::Instance().GetItemDataPointer(dwAcce, &pItemData))
		return;
	
	m_GraphicThingInstance.AttachAcce(pItemData, fSpecular);
#ifdef ENABLE_OBJ_SCALLING
	uint32_t dwRace = GetRace(), dwPos = RaceToJob(dwRace), dwSex = RaceToSex(dwRace);
	dwPos += 1;
	if (dwSex == 0)
		dwPos += 5;

	float fScaleX, fScaleY, fScaleZ, fPositionX, fPositionY, fPositionZ;
	if (pItemData && pItemData->GetItemScale(dwPos, fScaleX, fScaleY, fScaleZ, fPositionX, fPositionY, fPositionZ))
	{
		m_GraphicThingInstance.SetScale(fScaleX, fScaleY, fScaleZ, true);
		if (m_kHorse.IsMounting())
			fPositionZ += 10.0f;
		
		m_GraphicThingInstance.SetScalePositionOld(fPositionX, fPositionY, fPositionZ);
	}
#endif
}

void CInstanceBase::ChangeAcce(uint32_t dwAcce)
{
	if (!IsPC())
		return;

	SetAcce(dwAcce);
}

void CInstanceBase::ClearAcceEffect()
{
	if (!m_dwAcceEffect)
		return;

	__EffectContainer_DetachEffect(m_dwAcceEffect);
	m_dwAcceEffect = 0;
}
#endif

#ifdef ENABLE_COSTUME_EFFECT
void CInstanceBase::ClearCostumeEffect(bool bIsBody)
{
	if (bIsBody) {
		if (m_dwCostumeBodyEffect)
		{
			__DetachEffect(m_dwCostumeBodyEffect);
			m_dwCostumeBodyEffect = 0;
		}
	}
	else {
		if (m_dwCostumeWeaponRightEffect)
		{
			__DetachEffect(m_dwCostumeWeaponRightEffect);
			m_dwCostumeWeaponRightEffect = 0;
		}
		
		if (m_dwCostumeWeaponLeftEffect)
		{
			__DetachEffect(m_dwCostumeWeaponLeftEffect);
			m_dwCostumeWeaponLeftEffect = 0;
		}
	}
}

void CInstanceBase::ChangeCostumeEffect(bool bIsBody, uint32_t dwEffectID, bool effect)
{
	if (!IsPC())
		return;
	
	if (effect || dwEffectID < 1) {
		ClearCostumeEffect(bIsBody);
	}
	
	if (dwEffectID > 0) {
		if (bIsBody) {
			if (GetPart(CRaceData::PART_EFFECT_BODY) != dwEffectID) {
				ClearCostumeEffect(bIsBody);
			}

			if (!m_dwCostumeBodyEffect) {
				m_dwCostumeBodyEffect = __AttachEffect(EFFECT_REFINED + dwEffectID);
				m_awPart[CRaceData::PART_EFFECT_BODY] = dwEffectID;
			}
		}
		else {
			if (GetPart(CRaceData::PART_EFFECT_WEAPON) != dwEffectID) {
				ClearCostumeEffect(bIsBody);
			}

			if (!m_dwCostumeWeaponRightEffect) {
				m_dwCostumeWeaponRightEffect = __AttachEffect(EFFECT_REFINED + dwEffectID);
				m_awPart[CRaceData::PART_EFFECT_WEAPON] = dwEffectID;
			}

			if (!m_dwCostumeWeaponLeftEffect) {
				if (dwEffectID == EFFECT_WEAPON_COSTUME1_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME1_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME2_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME2_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME3_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME3_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME4_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME4_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME5_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME5_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME6_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME6_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME7_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME7_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME8_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME8_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME9_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME9_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME10_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME10_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME11_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME11_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME12_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME12_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME1_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME1_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME2_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME2_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME3_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME3_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME4_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME4_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME5_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME5_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME6_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME6_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME7_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME7_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME8_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME8_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME9_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME9_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME10_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME10_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME11_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME11_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME12_DAGGER_SPECIAL) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME12_DAGGER_SPECIAL_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME13_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME13_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME17_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME17_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME18_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME18_DAGGER_LEFT);
				}
				else if (dwEffectID == EFFECT_WEAPON_COSTUME19_DAGGER) {
					m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME19_DAGGER_LEFT);
				}
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				else if (IsMountingHorse()) {
					if (dwEffectID == EFFECT_WEAPON_COSTUME1_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME1_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME2_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME2_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME3_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME3_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME4_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME4_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME5_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME5_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME6_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME6_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME7_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME7_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME8_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME8_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME9_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME9_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME10_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME10_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME11_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME11_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME12_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME12_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME1_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME1_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME2_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME2_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME3_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME3_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME4_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME4_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME5_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME5_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME6_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME6_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME7_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME7_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME8_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME8_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME9_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME9_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME10_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME10_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME11_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME11_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME12_FAN_SPECIAL) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME12_FAN_SPECIAL_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME13_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME13_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME17_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME17_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME18_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME18_FAN_LEFT);
					}
					else if (dwEffectID == EFFECT_WEAPON_COSTUME19_FAN) {
						m_dwCostumeWeaponLeftEffect = __AttachEffect(EFFECT_REFINED + EFFECT_WEAPON_COSTUME19_FAN_LEFT);
					}
				}
#endif
			}
		}
	}
}
#endif

void CInstanceBase::SetShape(uint32_t eShape, float fSpecular)
{
	if (IsPoly())
	{
		m_GraphicThingInstance.SetShape(0);
	}
	else
	{
		m_GraphicThingInstance.SetShape(eShape, fSpecular);
	}

	m_eShape = eShape;
}

uint32_t CInstanceBase::GetWeaponType()
{
	uint32_t dwWeapon = GetPart(CRaceData::PART_WEAPON);
	CItemData * pItemData;
	if (!CItemManager::Instance().GetItemDataPointer(dwWeapon, &pItemData))
		return CItemData::WEAPON_NONE;

	return pItemData->GetWeaponType();
}





/*
void CInstanceBase::SetParts(const WORD * c_pParts)
{
	if (IsPoly())
		return;

	if (__IsShapeAnimalWear())
		return;

	UINT eWeapon=c_pParts[CRaceData::PART_WEAPON];

	if (__IsChangableWeapon(eWeapon) == false)
			eWeapon = 0;

	if (eWeapon != m_GraphicThingInstance.GetPartItemID(CRaceData::PART_WEAPON))
	{
		m_GraphicThingInstance.AttachPart(CRaceData::PART_MAIN, CRaceData::PART_WEAPON, eWeapon);
		m_awPart[CRaceData::PART_WEAPON] = eWeapon;
	}

	__AttachHorseSaddle();
}
*/

void CInstanceBase::__ClearWeaponRefineEffect()
{
	if (m_swordRefineEffectRight)
	{
		__DetachEffect(m_swordRefineEffectRight);
		m_swordRefineEffectRight = 0;
	}
	if (m_swordRefineEffectLeft)
	{
		__DetachEffect(m_swordRefineEffectLeft);
		m_swordRefineEffectLeft = 0;
	}
}

void CInstanceBase::__ClearArmorRefineEffect()
{
	if (m_armorRefineEffect)
	{
		__DetachEffect(m_armorRefineEffect);
		m_armorRefineEffect = 0;
	}
}

//#define ENABLE_SIMPLE_REFINED_EFFECT_CHECK
//#define USE_WEAPON_COSTUME_WITH_EFFECT
//#define USE_BODY_COSTUME_WITH_EFFECT
UINT CInstanceBase::__GetRefinedEffect(CItemData* pItem)
{
#ifdef ENABLE_SIMPLE_REFINED_EFFECT_CHECK
	uint32_t refine = pItem->GetRefine();
#else
	uint32_t refine = max(pItem->GetRefine() + pItem->GetSocketCount(),CItemData::ITEM_SOCKET_MAX_NUM) - CItemData::ITEM_SOCKET_MAX_NUM;
#endif
	switch (pItem->GetType())
	{
	case CItemData::ITEM_TYPE_WEAPON:
		__ClearWeaponRefineEffect();
		

		if (refine < 7)	//ÇöÀç Á¦·Ãµµ 7 ÀÌ»ó¸¸ ÀÌÆåÆ®°¡ ÀÖ½À´Ï´Ù.
			return 0;


		switch(pItem->GetSubType())
		{
		case CItemData::WEAPON_DAGGER:
			m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_SMALLSWORD_REFINED7+refine-7;
			m_swordRefineEffectLeft = EFFECT_REFINED+EFFECT_SMALLSWORD_REFINED7_LEFT+refine-7;
			break;
		case CItemData::WEAPON_FAN: {
				m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_FANBELL_REFINED7+refine-7;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED+EFFECT_FANBELL_LEFT_REFINED7+refine-7;
#endif
			}
			break;
		case CItemData::WEAPON_ARROW:
		case CItemData::WEAPON_BELL:
			m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_SMALLSWORD_REFINED7+refine-7;
			break;
		case CItemData::WEAPON_BOW:
			m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_BOW_REFINED7+refine-7;
			break;
		default:
			m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_SWORD_REFINED7+refine-7;
		}



#ifdef __ENABLE_NEW_EFFECT_CIANITE_WEAPON__
		if (pItem->GetSubType() == CItemData::WEAPON_SWORD)
		{
			uint32_t vnum = pItem->GetIndex();
			if (vnum == 529 || vnum == 569)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_CIANITE_SPECIAL1;
			}
			if (vnum == 539 || vnum == 579)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_CIANITE_SPECIAL2;
			}
			if (vnum == 549 || vnum == 589)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_CIANITE_SPECIAL3;
			}
			if (vnum == 559 || vnum == 599)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_CIANITE_SPECIAL4;
			}
		}
		if (pItem->GetSubType() == CItemData::WEAPON_TWO_HANDED)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 3519)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_2HAND_CIANITE_SPECIAL1;
			}
			if (vnum == 3529)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_2HAND_CIANITE_SPECIAL2;
			}
			if (vnum == 3539)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_2HAND_CIANITE_SPECIAL3;
			}
			if (vnum == 3549)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_2HAND_CIANITE_SPECIAL4;
			}
		}

		if (pItem->GetSubType() == CItemData::WEAPON_BELL)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 5519)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_CIANITE_SPECIAL1;
			}
			if (vnum == 5529)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_CIANITE_SPECIAL2;
			}
			if (vnum == 5539)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_CIANITE_SPECIAL3;
			}
			if (vnum == 5549)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_CIANITE_SPECIAL4;
			}
		}

		if (pItem->GetSubType() == CItemData::WEAPON_FAN)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 7519)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_CIANITE_SPECIAL1;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_CIANITE_LEFT_SPECIAL1;
#endif
			}
			if (vnum == 7529)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_CIANITE_SPECIAL2;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_CIANITE_LEFT_SPECIAL2;
#endif
			}
			if (vnum == 7539)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_CIANITE_SPECIAL3;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_CIANITE_LEFT_SPECIAL3;
#endif
			}
			if (vnum == 7549)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_CIANITE_SPECIAL4;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_CIANITE_LEFT_SPECIAL4;
#endif
			}
		}

		if (pItem->GetSubType() == CItemData::WEAPON_BOW)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 2519)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_CIANITE_SPECIAL1;
			}
			if (vnum == 2529)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_CIANITE_SPECIAL2;
			}
			if (vnum == 2539)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_CIANITE_SPECIAL3;
			}
			if (vnum == 2549)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_CIANITE_SPECIAL4;
			}
		}

		if (pItem->GetSubType() == CItemData::WEAPON_DAGGER)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 1519)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL2;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL1_LEFT;
			}
			if (vnum == 1529)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL2;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL2_LEFT;
			}
			if (vnum == 1539)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL3;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL3_LEFT;
			}
			if (vnum == 1549)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL4;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_DAGGER_CIANITE_SPECIAL4_LEFT;
			}
		}

#endif

#ifdef __ENABLE_NEW_EFFECT_ZODIACO_WEAPON__
		if (pItem->GetSubType() == CItemData::WEAPON_SWORD)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 329 || vnum == 369)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_ZODIACO_SPECIAL1;
			}
			if (vnum == 339 || vnum == 379)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_ZODIACO_SPECIAL2;
			}
			if (vnum == 349 || vnum == 389)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_ZODIACO_SPECIAL3;
			}
			if (vnum == 359 || vnum == 399)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SWORD_ZODIACO_SPECIAL4;
			}
		}
		if (pItem->GetSubType() == CItemData::WEAPON_TWO_HANDED)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 3239)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SPEAR_ZODIACO_SPECIAL1;
			}
			if (vnum == 3249)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SPEAR_ZODIACO_SPECIAL2;
			}
			if (vnum == 3259)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SPEAR_ZODIACO_SPECIAL3;
			}
			if (vnum == 3269)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_SPEAR_ZODIACO_SPECIAL4;
			}

		}
		if (pItem->GetSubType() == CItemData::WEAPON_BELL)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 5179)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_ZODIACO_SPECIAL1;
			}
			if (vnum == 5189)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_ZODIACO_SPECIAL2;
			}
			if (vnum == 5199)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_ZODIACO_SPECIAL3;
			}
			if (vnum == 5209)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BELL_ZODIACO_SPECIAL4;
			}

		}

		if (pItem->GetSubType() == CItemData::WEAPON_FAN)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 7319)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_ZODIACO_SPECIAL1;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_ZODIACO_LEFT_SPECIAL1;
#endif
			}
			if (vnum == 7329)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_ZODIACO_SPECIAL2;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_ZODIACO_LEFT_SPECIAL2;
#endif
			}
			if (vnum == 7339)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_ZODIACO_SPECIAL3;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_ZODIACO_LEFT_SPECIAL3;
#endif
			}
			if (vnum == 7349)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_FAN_ZODIACO_SPECIAL4;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
				if (IsMountingHorse())
					m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_ZODIACO_LEFT_SPECIAL4;
#endif
			}

		}

		if (pItem->GetSubType() == CItemData::WEAPON_BOW)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 2219)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_ZODIACO_SPECIAL1;
			}
			if (vnum == 2229)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_ZODIACO_SPECIAL2;
			}
			if (vnum == 2239)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_ZODIACO_SPECIAL3;
			}
			if (vnum == 2249)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_BOW_ZODIACO_SPECIAL4;
			}

		}

		if (pItem->GetSubType() == CItemData::WEAPON_DAGGER)
		{
			uint32_t vnum = pItem->GetIndex();

			if (vnum == 1199)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL1;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL1_LEFT;
			}
			if (vnum == 1209)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL2;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL2_LEFT;
			}
			if (vnum == 1219)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL3;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL3_LEFT;
			}
			if (vnum == 1229)
			{
				__ClearWeaponRefineEffect();
				m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL4;
				m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_KNIFE_ZODIACO_SPECIAL4_LEFT;
			}

		}

#endif


		if (m_swordRefineEffectRight)
			m_swordRefineEffectRight = __AttachEffect(m_swordRefineEffectRight);
		if (m_swordRefineEffectLeft)
			m_swordRefineEffectLeft = __AttachEffect(m_swordRefineEffectLeft);
		break;
	case CItemData::ITEM_TYPE_ARMOR:
		__ClearArmorRefineEffect();

		// °©¿Ê Æ¯È­ ÀÌÆåÆ®
		if (pItem->GetSubType() == CItemData::ARMOR_BODY)
		{
			uint32_t vnum = pItem->GetIndex();

			if (12010 <= vnum && vnum <= 12049)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_SPECIAL);
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_SPECIAL2);
			}
#ifdef ENABLE_LVL115_ARMOR_EFFECT
			else if (20760 <= vnum && vnum <= 20959)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_SPECIAL3);
			}
#endif //ENABLE_LVL115_ARMOR_EFFECT

#ifdef __ENABLE_NEW_EFFECT_CIANITE__
			else if (vnum == 12100 || vnum == 12104 || vnum == 12108 || vnum == 12112)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_CIANITE1);
			}
			else if (vnum == 12101 || vnum == 12105 || vnum == 12109 || vnum == 12113)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_CIANITE2);
			}
			else if (vnum == 12102 || vnum == 12106 || vnum == 12110 || vnum == 12114)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_CIANITE3);
			}
			else if (vnum == 12103 || vnum == 12107 || vnum == 12111 || vnum == 12115)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_CIANITE4);
			}
#endif

#ifdef __ENABLE_NEW_EFFECT_ZODIACO__
			else if (vnum == 19309 || vnum == 19509 || vnum == 19709 || vnum == 19909)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_ZODIACO);
			}
			else if (vnum == 19310 || vnum == 19510 || vnum == 19710 || vnum == 19910)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_ZODIACO2);
			}
			else if (vnum == 19311 || vnum == 19511 || vnum == 19711 || vnum == 19911)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_ZODIACO3);
			}
			else if (vnum == 19312 || vnum == 19512 || vnum == 19712 || vnum == 19912)
			{
				__AttachEffect(EFFECT_REFINED+EFFECT_BODYARMOR_ZODIACO4);
			}
#endif


		}

		if (refine < 7)	//ÇöÀç Á¦·Ãµµ 7 ÀÌ»ó¸¸ ÀÌÆåÆ®°¡ ÀÖ½À´Ï´Ù.
			return 0;

		if (pItem->GetSubType() == CItemData::ARMOR_BODY)
		{
			m_armorRefineEffect = EFFECT_REFINED+EFFECT_BODYARMOR_REFINED7+refine-7;
			__AttachEffect(m_armorRefineEffect);
		}
		break;
	case CItemData::ITEM_TYPE_COSTUME: {
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			if (pItem->GetSubType() == CItemData::COSTUME_WEAPON)
			{
				__ClearWeaponRefineEffect();
				uint32_t vnum = pItem->GetIndex();
				switch (pItem->GetIndex()) {
#ifdef __ENABLE_NEW_EFFECT_ZODIACO_WEAPON1__
					case 40100:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_SWORD_ZODIACO_WEAPON1;
						break;
					case 40097:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_SPEAR_ZODIACO_WEAPON1;
						break;
					case 40096:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_BELL_ZODIACO_WEAPON1;
						break;
					case 40095: {
							m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_FAN_ZODIACO_WEAPON1;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_FAN_ZODIACO_LEFT_WEAPON1;
#endif
						}
						break;
					case 40098:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_BOW_ZODIACO_WEAPON1;
						break;
					case 40099:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_KNIFE_ZODIACO_WEAPON1;
						m_swordRefineEffectLeft = EFFECT_REFINED+EFFECT_KNIFE_ZODIACO_WEAPON1_LEFT;
						break;
#endif
					case 48601:
					case 48611:
					case 48621:
					case 48631:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_SWORD;
						break;
					case 48602:
					case 48612:
					case 48622:
					case 48632:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_DAGGER_RIGHT;
						m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_DAGGER_LEFT;
						break;
					case 48603:
					case 48613:
					case 48623:
					case 48633:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_BOW;
						break;
					case 48604:
					case 48614:
					case 48624:
					case 48634:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_TWOHAND;
						break;
					case 48605:
					case 48615:
					case 48625:
					case 48635:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_BELL;
						break;
					case 48606:
					case 48616:
					case 48626:
					case 48636:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_FAN;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_HALLOWEEN_2021_FAN_LEFT;
#endif
						}
						break;
					case 49001:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_SWORD;
						break;
					case 49002:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_DAGGER_RIGHT;
						m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_DAGGER_LEFT;
						break;
					case 49003:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_BOW;
						break;
					case 49004:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_TWOHAND;
						break;
					case 49005:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_BELL;
						break;
					case 49006:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME1_FAN;
						break;
					case 49011:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_SWORD;
						break;
					case 49012:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_DAGGER_RIGHT;
						m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_DAGGER_LEFT;
						break;
					case 49013:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_BOW;
						break;
					case 49014:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_TWOHAND;
						break;
					case 49015:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_BELL;
						break;
					case 49016:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME2_FAN;
						break;
					case 49021:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_SWORD;
						break;
					case 49024:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_TWO_HANDED;
						break;
					case 49025:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_BELL;
						break;
					case 49026:
						{
							m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_FAN;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_WEAPON_COSTUME10_FAN_LEFT;
#endif
						}
						break;
					case 49023:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_BOW;
						break;
					case 49022:
						m_swordRefineEffectRight = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_DAGGER;
						m_swordRefineEffectLeft = EFFECT_REFINED+EFFECT_WEAPON_COSTUME10_DAGGER_LEFT;
						break;
					case 49031:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_SWORD;
						break;
					case 49032:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_DAGGER_RIGHT;
						m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_DAGGER_LEFT;
						break;
					case 49033:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_BOW;
						break;
					case 49034:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_TWOHAND;
						break;
					case 49035:
						m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_BELL;
						break;
					case 49036:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_FAN;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_PATCH3_COSTUME3_FAN_LEFT;
#endif
						}
						break;
					case 49041:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH4_SWORD;
						}
						break;
					case 49042:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH4_DAGGER_RIGHT;
							m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_PATCH4_DAGGER_LEFT;
						}
						break;
					case 49043:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH4_BOW;
						}
						break;
					case 49044:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH4_TWOHAND;
						}
						break;
					case 49045:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH4_BELL;
						}
						break;
					case 49046:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_PATCH4_FAN;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_PATCH4_FAN_LEFT;
#endif
						}
						break;
					case 49051:
					case 49061:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_SWORD;
						}
						break;
					case 49052:
					case 49062:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_DAGGER;
							m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_DAGGER_LEFT;
						}
						break;
					case 49053:
					case 49063:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_BOW;
						}
						break;
					case 49054:
					case 49064:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_TWO_HANDED;
						}
						break;
					case 49055:
					case 49065:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_BELL;
						}
						break;
					case 49056:
					case 49066:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_FAN;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_WEAPON_COSTUME14_FAN_LEFT;
#endif
						}
						break;
					case 49111:
					case 49121:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_SWORD;
						}
						break;
					case 49112:
					case 49122:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_DAGGER;
							m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_DAGGER_LEFT;
						}
						break;
					case 49113:
					case 49123:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_BOW;
						}
						break;
					case 49114:
					case 49124:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_TWO_HANDED;
						}
						break;
					case 49115:
					case 49125:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_BELL;
						}
						break;
					case 49116:
					case 49126:
						{
							m_swordRefineEffectRight = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_FAN;
#ifdef ENABLE_BUGFIX_EFFECT_FAN
							if (IsMountingHorse())
								m_swordRefineEffectLeft = EFFECT_REFINED + EFFECT_WEAPON_COSTUME16_FAN_LEFT;
#endif
						}
						break;
					default:
						break;
				}

				if (m_swordRefineEffectRight)
				{
					m_swordRefineEffectRight = __AttachEffect(m_swordRefineEffectRight);
				}

				if (m_swordRefineEffectLeft)
				{
					m_swordRefineEffectLeft = __AttachEffect(m_swordRefineEffectLeft);
				}
			}
#endif
			
			if (pItem->GetSubType() == CItemData::COSTUME_BODY)
			{
				__ClearArmorRefineEffect();
				
				uint32_t vnum = pItem->GetIndex();
				switch (pItem->GetIndex()) {
#ifdef __ENABLE_NEW_EFFECT_ZODIACO1__
					case 40192:
					case 40193:
					case 40194:
					case 40195:
					case 40196:
					case 40197:
					case 40198:
					case 40199:
						m_armorRefineEffect = EFFECT_REFINED+EFFECT_BODYARMOR_ZODIACO1;
						break;
#endif
					case 48501:
					case 48502:
					case 48511:
					case 48512:
					case 48521:
					case 48522:
					case 48531:
					case 48532:
						m_armorRefineEffect = EFFECT_REFINED+EFFECT_HALLOWEEN_2021_COSTUME;
						break;
					case 49007:
					case 49008:
						m_armorRefineEffect = EFFECT_REFINED+EFFECT_PATCH3_COSTUME1_COSTUME;
						break;
					case 49017:
					case 49018:
						m_armorRefineEffect = EFFECT_REFINED+EFFECT_PATCH3_COSTUME2_COSTUME;
						break;
					case 49027:
					case 49028:
						m_armorRefineEffect = EFFECT_REFINED+EFFECT_BODY_COSTUME10;
						break;
					case 49037:
					case 49038:
						m_armorRefineEffect = EFFECT_REFINED+EFFECT_PATCH3_COSTUME3_COSTUME;
						break;
					case 49047:
						{
							m_armorRefineEffect = EFFECT_REFINED+EFFECT_PATCH4_COSTUME_M;
							__AttachEffect(EFFECT_REFINED+EFFECT_PATCH4_COSTUME_F_1);
							__AttachEffect(EFFECT_REFINED+EFFECT_PATCH4_COSTUME_F_2);
						}
						break;
					case 49048:
						{
							m_armorRefineEffect = EFFECT_REFINED+EFFECT_PATCH4_COSTUME_M;
							__AttachEffect(EFFECT_REFINED+EFFECT_PATCH4_COSTUME_M_1);
							__AttachEffect(EFFECT_REFINED+EFFECT_PATCH4_COSTUME_M_2);
						}
						break;
					case 49057:
					case 49058:
					case 49067:
					case 49068:
						{
							m_armorRefineEffect = EFFECT_REFINED + EFFECT_BODY_COSTUME14;
						}
						break;
					case 49117:
					case 49118:
					case 49127:
					case 49128:
						{
							m_armorRefineEffect = EFFECT_REFINED + EFFECT_BODY_COSTUME16;
						}
						break;
					default:
						break;
				}
				
				if (m_armorRefineEffect)
					m_armorRefineEffect = __AttachEffect(m_armorRefineEffect);
			}
			break;
		}
	}
	
	return 0;
}

bool CInstanceBase::SetWeapon(uint32_t eWeapon)
{
	if (IsPoly())
		return false;

	if (__IsShapeAnimalWear())
		return false;

	if (!__IsChangableWeapon(eWeapon))
		eWeapon = 0;

	m_GraphicThingInstance.AttachWeapon(eWeapon);
	m_awPart[CRaceData::PART_WEAPON] = eWeapon;

	// Weapon Effect
	CItemData* pItemData = nullptr;

	if (eWeapon && CItemManager::Instance().GetItemDataPointer(eWeapon, &pItemData))
	{
		__GetRefinedEffect(pItemData);

#ifdef ENABLE_SHINING_SYSTEM
		__GetShiningEffect(pItemData);
#endif
	}
	else
	{
		__ClearWeaponRefineEffect();

#ifdef ENABLE_SHINING_SYSTEM
		__ClearWeaponShiningEffect();
#endif
	}

	return true;
}


void CInstanceBase::ChangeWeapon(uint32_t eWeapon)
{
	if (eWeapon == m_GraphicThingInstance.GetPartItemID(CRaceData::PART_WEAPON))
		return;

	if (SetWeapon(eWeapon))
		RefreshState(CRaceMotionData::NAME_WAIT, true);
}

bool CInstanceBase::ChangeArmor(uint32_t dwArmor)
{
	uint32_t eShape;
	__ArmorVnumToShape(dwArmor, &eShape);
	
	if (GetShape()==eShape)
		return false;
	
	CAffectFlagContainer kAffectFlagContainer;
	kAffectFlagContainer.CopyInstance(m_kAffectFlagContainer);

	uint32_t dwVID = GetVirtualID();
	uint32_t dwRace = GetRace();
	uint32_t eHair = GetPart(CRaceData::PART_HAIR);
#ifdef ENABLE_ACCE_SYSTEM
	uint32_t dwAcce = GetPart(CRaceData::PART_ACCE);
#endif
	uint32_t eWeapon = GetPart(CRaceData::PART_WEAPON);
	float fRot = GetRotation();
	float fAdvRot = GetAdvancingRotation();

	if (IsWalking())
		EndWalking();

	// 2004.07.25.myevan.ÀÌÆåÆ® ¾È ºÙ´Â ¹®Á¦
	//////////////////////////////////////////////////////
	__ClearAffects();
	//////////////////////////////////////////////////////

	if (!SetRace(dwRace))
	{
		TraceError("CPythonCharacterManager::ChangeArmor - SetRace VID[%d] Race[%d] ERROR", dwVID, dwRace);
		return false;
	}

	SetArmor(dwArmor);
	SetHair(eHair);
	SetWeapon(eWeapon);
#ifdef ENABLE_RUNE_SYSTEM
	SetRune(GetPart(CRaceData::PART_RUNE), true);
#endif
#ifdef ENABLE_ACCE_SYSTEM
	SetAcce(dwAcce);
#endif
#ifdef ENABLE_COSTUME_EFFECT
	ChangeCostumeEffect(true, GetPart(CRaceData::PART_EFFECT_BODY), true);
	ChangeCostumeEffect(false, GetPart(CRaceData::PART_EFFECT_WEAPON), true);
#endif
	SetRotation(fRot);
	SetAdvancingRotation(fAdvRot);

	__AttachHorseSaddle();

	RefreshState(CRaceMotionData::NAME_WAIT, TRUE);

	// 2004.07.25.myevan.ÀÌÆåÆ® ¾È ºÙ´Â ¹®Á¦
	/////////////////////////////////////////////////
	SetAffectFlagContainer(kAffectFlagContainer);
	/////////////////////////////////////////////////

	CActorInstance::IEventHandler& rkEventHandler=GetEventHandlerRef();
	rkEventHandler.OnChangeShape();

	return true;
}

bool CInstanceBase::__IsShapeAnimalWear()
{
	if (100 == GetShape() ||
		101 == GetShape() ||
		102 == GetShape() ||
		103 == GetShape())
		return true;

	return false;
}

uint32_t CInstanceBase::__GetRaceType()
{
	return m_eRaceType;
}


void CInstanceBase::RefreshState(uint32_t dwMotIndex, bool isLoop)
{
	uint32_t dwPartItemID = m_GraphicThingInstance.GetPartItemID(CRaceData::PART_WEAPON);

	uint8_t byItemType = 0xff;
	uint8_t bySubType = 0xff;

	CItemManager & rkItemMgr = CItemManager::Instance();
	CItemData * pItemData;

	if (rkItemMgr.GetItemDataPointer(dwPartItemID, &pItemData))
	{
		byItemType = pItemData->GetType();
		bySubType = pItemData->GetWeaponType();
	}

	if (IsPoly())
	{
		SetMotionMode(CRaceMotionData::MODE_GENERAL);
	}
	else if (IsWearingDress())
	{
		SetMotionMode(CRaceMotionData::MODE_WEDDING_DRESS);
	}
	else if (IsHoldingPickAxe())
	{
		if (m_kHorse.IsMounting())
		{
			SetMotionMode(CRaceMotionData::MODE_HORSE);
		}
		else
		{
			SetMotionMode(CRaceMotionData::MODE_GENERAL);
		}
	}
	else if (CItemData::ITEM_TYPE_ROD == byItemType)
	{
		if (m_kHorse.IsMounting())
		{
			SetMotionMode(CRaceMotionData::MODE_HORSE);
		}
		else
		{
			SetMotionMode(CRaceMotionData::MODE_FISHING);
		}
	}
	else if (m_kHorse.IsMounting())
	{
		const uint32_t dwJob = RaceToJob(GetRace());
		const bool bIsAssassin = (dwJob == NRaceData::JOB_ASSASSIN);

		switch (bySubType)
		{
			case CItemData::WEAPON_SWORD:
				SetMotionMode(CRaceMotionData::MODE_HORSE_ONEHAND_SWORD);
				break;

			case CItemData::WEAPON_TWO_HANDED:
				SetMotionMode(CRaceMotionData::MODE_HORSE_TWOHAND_SWORD); // Only Warrior
				break;

			case CItemData::WEAPON_DAGGER:
				SetMotionMode(bIsAssassin ? CRaceMotionData::MODE_HORSE_ONEHAND_SWORD : CRaceMotionData::MODE_HORSE_DUALHAND_SWORD); // Only Assassin
				break;

			case CItemData::WEAPON_FAN:
				SetMotionMode(CRaceMotionData::MODE_HORSE_FAN); // Only Shaman
				break;

			case CItemData::WEAPON_BELL:
				SetMotionMode(CRaceMotionData::MODE_HORSE_BELL); // Only Shaman
				break;

			case CItemData::WEAPON_BOW:
				SetMotionMode(CRaceMotionData::MODE_HORSE_BOW); // Only Shaman
				break;
			default:
				SetMotionMode(CRaceMotionData::MODE_HORSE);
				break;
		}
	}
	else
	{
		switch (bySubType)
		{
			case CItemData::WEAPON_SWORD:
				SetMotionMode(CRaceMotionData::MODE_ONEHAND_SWORD);
				break;

			case CItemData::WEAPON_TWO_HANDED:
				SetMotionMode(CRaceMotionData::MODE_TWOHAND_SWORD); // Only Warrior
				break;

			case CItemData::WEAPON_DAGGER:
				SetMotionMode(CRaceMotionData::MODE_DUALHAND_SWORD); // Only Assassin
				break;

			case CItemData::WEAPON_BOW:
				SetMotionMode(CRaceMotionData::MODE_BOW); // Only Assassin
				break;

			case CItemData::WEAPON_FAN:
				SetMotionMode(CRaceMotionData::MODE_FAN); // Only Shaman
				break;

			case CItemData::WEAPON_BELL:
				SetMotionMode(CRaceMotionData::MODE_BELL); // Only Shaman
				break;
			case CItemData::WEAPON_ARROW:
			default:
				SetMotionMode(CRaceMotionData::MODE_GENERAL);
				break;
		}
	}

	if (isLoop)
		m_GraphicThingInstance.InterceptLoopMotion(dwMotIndex);
	else
		m_GraphicThingInstance.InterceptOnceMotion(dwMotIndex);

	RefreshActorInstance();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Device

void CInstanceBase::RegisterBoundingSphere()
{
	if (!IsStone())
	{
		m_GraphicThingInstance.DeformNoSkin();
	}

	m_GraphicThingInstance.RegisterBoundingSphere();
}

bool CInstanceBase::CreateDeviceObjects()
{
	return m_GraphicThingInstance.CreateDeviceObjects();
}

void CInstanceBase::DestroyDeviceObjects()
{
	m_GraphicThingInstance.DestroyDeviceObjects();
}
#ifdef MEMORY_LEAK_FIX_RAZRO93

void CInstanceBase::Destroy()
{
	DetachTextTail();
	DismountHorse();

	if (m_pCylinderCollisionInstance)
	{
		delete m_pCylinderCollisionInstance;
		m_pCylinderCollisionInstance = nullptr;
	}

	m_kQue_kCmdNew.clear();

	__EffectContainer_Destroy();
	__StoneSmoke_Destroy();

	if (__IsMainInstance())
		__ClearMainInstance();

	m_GraphicThingInstance.Destroy();

	__Initialize();
}


#else

void CInstanceBase::Destroy()
{
	DetachTextTail();

	DismountHorse();

	m_kQue_kCmdNew.clear();

	__EffectContainer_Destroy();
	__StoneSmoke_Destroy();

	if (__IsMainInstance())
		__ClearMainInstance();

	m_GraphicThingInstance.Destroy();

	__Initialize();
}
#endif
void CInstanceBase::__InitializeRotationSpeed()
{
	SetRotationSpeed(c_fDefaultRotationSpeed);
}

void CInstanceBase::__Warrior_Initialize()
{
	m_kWarrior.m_dwGeomgyeongEffect=0;
}

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
void CInstanceBase::__Assassin_Initialize()
{
	m_kAssassin.m_dwGyeongGongEffect = 0;
}
#endif

#ifdef ENABLE_RUNE_SYSTEM
void CInstanceBase::SetRune(uint32_t dwRune, bool effect)
{
	if (GetPart(CRaceData::PART_RUNE) != dwRune)
		m_awPart[CRaceData::PART_RUNE] = dwRune;
	
	SetRuneEffect(effect);
}

void CInstanceBase::SetRuneEffect(bool effect) {
	if (effect) {
		ClearRuneEffect();

		if (GetPart(CRaceData::PART_RUNE) == 1) {
			m_runeEffect = __AttachEffect(EFFECT_RUNE);
		}
	}
	else {
		if (GetPart(CRaceData::PART_RUNE) == 1) {
			if (!m_runeEffect) {
				m_runeEffect = __AttachEffect(EFFECT_RUNE);
			}
		} else {
			ClearRuneEffect();
		}
	}
}

void CInstanceBase::ClearRuneEffect()
{
	if (m_runeEffect)
	{
		__DetachEffect(m_runeEffect);
		m_runeEffect = 0;
	}
}
#endif


void CInstanceBase::__Initialize()
{
	__Warrior_Initialize();
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	__Assassin_Initialize();
#endif
	__StoneSmoke_Inialize();
	__EffectContainer_Initialize();
	__InitializeRotationSpeed();

	SetEventHandler(CActorInstance::IEventHandler::GetEmptyPtr());

	m_kAffectFlagContainer.Clear();

	m_dwLevel = 0;
	m_dwGuildID = 0;
	m_dwEmpireID = 0;

	m_eType = 0;
	m_eRaceType = 0;
	m_eShape = 0;
	m_dwRace = 0;
	m_dwVirtualNumber = 0;

	m_dwBaseCmdTime=0;
	m_dwBaseChkTime=0;
	m_dwSkipTime=0;

	m_GraphicThingInstance.Initialize();

	m_dwAdvActorVID=0;
	m_dwLastDmgActorVID=0;

	m_nAverageNetworkGap=0;
	m_dwNextUpdateHeightTime=0;

	// Moving by keyboard
	m_iRotatingDirection = DEGREE_DIRECTION_SAME;

	// Moving by mouse
	m_isTextTail = FALSE;
	m_isGoing = FALSE;
	NEW_SetSrcPixelPosition(TPixelPosition(0, 0, 0));
	NEW_SetDstPixelPosition(TPixelPosition(0, 0, 0));

	m_kPPosDust = TPixelPosition(0, 0, 0);


	m_kQue_kCmdNew.clear();

	m_dwLastComboIndex = 0;

	m_swordRefineEffectRight = 0;
	m_swordRefineEffectLeft = 0;
	m_armorRefineEffect = 0;
#ifdef ENABLE_SHINING_SYSTEM
	__ClearWeaponShiningEffect(false);
#endif
#ifdef ENABLE_ACCE_SYSTEM
	m_dwAcceEffect = 0;
#endif
#ifdef ENABLE_COSTUME_EFFECT
	m_dwCostumeBodyEffect = 0;
	m_dwCostumeWeaponRightEffect = 0;
	m_dwCostumeWeaponLeftEffect = 0;
#endif
	m_sAlignment = 0;
	m_byPKMode = 0;
	m_isKiller = false;
	m_isPartyMember = false;

	m_bEnableTCPState = TRUE;

	m_stName = "";

	memset(m_awPart, 0, sizeof(m_awPart));
	memset(m_adwCRCAffectEffect, 0, sizeof(m_adwCRCAffectEffect));
	//memset(m_adwCRCEmoticonEffect, 0, sizeof(m_adwCRCEmoticonEffect));
	memset(&m_kMovAfterFunc, 0, sizeof(m_kMovAfterFunc));
#ifdef MEMORY_LEAK_FIX_RAZRO93
	m_pCylinderCollisionInstance = nullptr;
#endif
	m_bDamageEffectType = false;
	m_dwDuelMode = DUEL_NONE;
	m_dwEmoticonTime = 0;
	m_IsAlwaysRender = false;
#ifdef ENABLE_RUNE_SYSTEM
	m_runeEffect = 0;
#endif
}

CInstanceBase::CInstanceBase()
{
	__Initialize();
}

CInstanceBase::~CInstanceBase()
{
	Destroy();
}

bool CInstanceBase::IsAlwaysRender()
{
	return m_IsAlwaysRender;
}

void CInstanceBase::SetAlwaysRender(bool val)
{
	m_IsAlwaysRender = val;
}

void CInstanceBase::GetBoundBox(D3DXVECTOR3 * vtMin, D3DXVECTOR3 * vtMax)
{
	m_GraphicThingInstance.GetBoundBox(vtMin, vtMax);
}

#ifdef ENABLE_SHINING_SYSTEM
//This method checks if the item has shinings and will attach them.
//Weapons are heaving special checks for two sided weapons like daggers or fans while mounting.
//Every itemtype is supported. If no explicit logic is implemented it will be attached to "Bip01".
void CInstanceBase::__GetShiningEffect(CItemData* pItem)
{
	//Set this to true if you want to hide the refine effect if there's a shining for this item.
	bool removeRefineEffect = false;
	CItemData::TItemShiningTable shiningTable = pItem->GetItemShiningTable();
#if defined(ENABLE_COSTUME_SYSTEM)
	if (pItem->GetType() == CItemData::ITEM_TYPE_WEAPON || (pItem->GetType() == CItemData::ITEM_TYPE_COSTUME && pItem->GetSubType() == CItemData::COSTUME_WEAPON))
	{
		uint8_t bSubType = pItem->GetType() == CItemData::ITEM_TYPE_COSTUME && pItem->GetSubType() == CItemData::COSTUME_WEAPON ? pItem->GetValue(3) : pItem->GetSubType();
#else
	if (pItem->GetType() == CItemData::ITEM_TYPE_WEAPON)
	{
		BYTE bSubType = pItem->GetSubType();
#endif
		__ClearWeaponShiningEffect();
		if (shiningTable.Any() && removeRefineEffect)
		{
			__ClearWeaponRefineEffect();
		}
		for (int i = 0; i < CItemData::ITEM_SHINING_MAX_COUNT; i++)
		{
			if (strcmp(shiningTable.szShinings[i], ""))
			{
				if (bSubType == CItemData::WEAPON_BOW)
				{
					__AttachWeaponShiningEffect(i, shiningTable.szShinings[i], "PART_WEAPON_LEFT");
				}
				else
				{
					//Check for double sided weapons or fan which is attached on both sides when mounted.
#ifdef ENABLE_WOLFMAN
					bool twoSidedWeapon = bSubType == CItemData::WEAPON_DAGGER || (IsMountingHorse() && bSubType == CItemData::WEAPON_FAN) || bSubType == CItemData::WEAPON_CLAW;
#else
					bool twoSidedWeapon = bSubType == CItemData::WEAPON_DAGGER || (IsMountingHorse() && bSubType == CItemData::WEAPON_FAN);
#endif
					if (twoSidedWeapon)
					{
						__AttachWeaponShiningEffect(i, shiningTable.szShinings[i], "PART_WEAPON_LEFT");
					}
					__AttachWeaponShiningEffect(i, shiningTable.szShinings[i], "PART_WEAPON");
				}
			}
		}
	}
#if defined(ENABLE_COSTUME_SYSTEM)
	if ((pItem->GetType() == CItemData::ITEM_TYPE_ARMOR && pItem->GetSubType() == CItemData::ARMOR_BODY) || (pItem->GetType() == CItemData::ITEM_TYPE_COSTUME && pItem->GetSubType() == CItemData::COSTUME_BODY))
#else
	if (pItem->GetType() == CItemData::ITEM_TYPE_ARMOR && pItem->GetSubType() == CItemData::ARMOR_BODY)
#endif
	{
		__ClearArmorShiningEffect();
		if (shiningTable.Any() && removeRefineEffect)
		{
			__ClearArmorRefineEffect();
		}
		for (int i = 0; i < CItemData::ITEM_SHINING_MAX_COUNT; i++)
		{
			if (strcmp(shiningTable.szShinings[i], ""))
			{
				__AttachArmorShiningEffect(i, shiningTable.szShinings[i]);
			}
		}
	}
	}

//Attaching the shining effect. 
//BoneName can be "PART_WEAPON" or "PART_WEAPON_LEFT" to get the attaching bone name dynamically.
//If boneName is not given "Bip01" is used as boneName.
void CInstanceBase::__AttachWeaponShiningEffect(int effectIndex, const char* effectFileName, const char* boneName)
{
	if (IsAffect(AFFECT_INVISIBILITY))
	{
		return;
	}

	if (effectIndex >= CItemData::ITEM_SHINING_MAX_COUNT)
	{
		return;
	}

	CEffectManager::Instance().RegisterEffect(effectFileName, false, false);

	if (!strcmp(boneName, "PART_WEAPON"))
	{
		const char* c_szRightBoneName;
		m_GraphicThingInstance.GetAttachingBoneName(CRaceData::PART_WEAPON, &c_szRightBoneName);

		if (strcmp(c_szRightBoneName, ""))
		{
			m_weaponShiningEffects[0][effectIndex] = m_GraphicThingInstance.AttachEffectByName(0, c_szRightBoneName, effectFileName);
		}
	}
	else if (!strcmp(boneName, "PART_WEAPON_LEFT"))
	{
		const char* c_szLeftBoneName;
		m_GraphicThingInstance.GetAttachingBoneName(CRaceData::PART_WEAPON_LEFT, &c_szLeftBoneName);

		if (strcmp(c_szLeftBoneName, ""))
		{
			m_weaponShiningEffects[1][effectIndex] = m_GraphicThingInstance.AttachEffectByName(0, c_szLeftBoneName, effectFileName);
		}
	}
	else
	{
		Tracef("Invalid partname for getting attaching bone name. %s - %s", effectFileName, boneName);
	}
}

//Attaching the armor shining effect. 
//If boneName is not given "Bip01" is used as boneName.
void CInstanceBase::__AttachArmorShiningEffect(int effectIndex, const char* effectFileName, const char* boneName)
{
	if (IsAffect(AFFECT_INVISIBILITY))
	{
		return;
	}

	if (effectIndex >= CItemData::ITEM_SHINING_MAX_COUNT)
	{
		return;
	}

	if (!strcmp(boneName, ""))
	{
		Tracef("Empty bone name for attaching armor shining. Effect Index: %i, EffectFileName: %s", effectIndex, effectFileName);
		return;
	}

	CEffectManager::Instance().RegisterEffect(effectFileName, false, false);
	m_armorShiningEffects[effectIndex] = m_GraphicThingInstance.AttachEffectByName(0, boneName, effectFileName);
}

//Clears all weapon shining effects. Left & Right if set.
void CInstanceBase::__ClearWeaponShiningEffect(bool detaching)
{
	if (detaching)
	{
		for (int i = 0; i < CItemData::ITEM_SHINING_MAX_COUNT; i++)
		{
			if (m_weaponShiningEffects[0][i])
			{
				__DetachEffect(m_weaponShiningEffects[0][i]);
			}
			if (m_weaponShiningEffects[1][i])
			{
				__DetachEffect(m_weaponShiningEffects[1][i]);
			}
		}
	}
	memset(&m_weaponShiningEffects, 0, sizeof(m_weaponShiningEffects));
}
//Clears all armor shining effects.
void CInstanceBase::__ClearArmorShiningEffect(bool detaching)
{
	if (detaching)
	{
		for (int i = 0; i < CItemData::ITEM_SHINING_MAX_COUNT; i++)
		{
			if (m_armorShiningEffects[i])
			{
				__DetachEffect(m_armorShiningEffects[i]);
			}
		}
	}
	memset(&m_armorShiningEffects, 0, sizeof(m_armorShiningEffects));
}
#endif