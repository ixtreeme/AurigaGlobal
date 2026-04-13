#include "StdAfx.h"
#include "ActorInstance.h"

#include <cmath>

static inline bool _is_finitef(float v)
{
	return std::isfinite(v) != 0;
}
#include "AreaTerrain.h"
#include "RaceData.h"
#include "../SpeedTree/SpeedTreeForestDirectX8.h"
#include "../SpeedTree/SpeedTreeWrapper.h"
#include "GameLibDefines.h"
#include "../UserInterface/Locale_inc.h"
#ifdef ENABLE_RACE_HEIGHT
#include "RaceManager.h"
#endif
enum
{
	MAIN_RACE_MAX_NUM = 8,
};

void CActorInstance::INSTANCEBASE_Deform()
{
	Deform();
	TraceProcess();
}

void CActorInstance::INSTANCEBASE_Transform()
{
	if (m_pkHorse)
	{
		m_pkHorse->INSTANCEBASE_Transform();

		m_x = m_pkHorse->NEW_GetCurPixelPositionRef().x;
		m_y = -m_pkHorse->NEW_GetCurPixelPositionRef().y;
		m_z = m_pkHorse->NEW_GetCurPixelPositionRef().z;
		m_bNeedUpdateCollision = TRUE;
	}

	//uint32_t t2=ELTimer_GetMSec();
	Update();
	//uint32_t t3=ELTimer_GetMSec();
	TransformProcess();
	//uint32_t t4=ELTimer_GetMSec();
	Transform();
	//uint32_t t5=ELTimer_GetMSec();
	UpdatePointInstance();
	//uint32_t t6=ELTimer_GetMSec();
	ShakeProcess();
	//uint32_t t7=ELTimer_GetMSec();
	UpdateBoundingSphere();
	//uint32_t t8=ELTimer_GetMSec();
	UpdateAttribute();
}

/*
void CActorInstance::TEMP_Update()
{
	//uint32_t t1=ELTimer_GetMSec();
	OnUpdate();
	//uint32_t t2=ELTimer_GetMSec();
	UpdateBoundingSphere();
	//uint32_t t3=ELTimer_GetMSec();

#ifdef __PERFORMANCE_CHECKER__
	{
		static FILE* fp=fopen("perf_actor_update.txt", "w");

		if (t3-t1>3)
		{
			fprintf(fp, "AIU.Total %d (Time %f)\n",
				t3-t1, ELTimer_GetMSec()/1000.0f);
			fprintf(fp, "AIU.UP %d\n", t2-t1);
			fprintf(fp, "AIU.UBS %d\n", t3-t2);
			fprintf(fp, "-------------------------------- \n");
			fflush(fp);
		}
		fflush(fp);
	}
#endif
}
*/

void CActorInstance::OnUpdate()
{
#ifdef __PERFORMANCE_CHECKER__
	uint32_t t1=ELTimer_GetMSec();
#endif
	if (!IsParalysis())
		CGraphicThingInstance::OnUpdate();
#ifdef __PERFORMANCE_CHECKER__
	uint32_t t2=ELTimer_GetMSec();
#endif

	UpdateAttachingInstances();

#ifdef __PERFORMANCE_CHECKER__
	uint32_t t3=ELTimer_GetMSec();
#endif

	__BlendAlpha_Update();

#ifdef __PERFORMANCE_CHECKER__
	uint32_t t4=ELTimer_GetMSec();
	{
		static FILE* fp=fopen("perf_actor_update2.txt", "w");

		if (t4-t1>3)
		{
			fprintf(fp, "AIU2.Total %d (Time %f)\n",
				t4-t1, ELTimer_GetMSec()/1000.0f);
			fprintf(fp, "AIU2.GU %d\n", t2-t1);
			fprintf(fp, "AIU2.UAI %d\n", t3-t2);
			fprintf(fp, "AIU2.BAU %d\n", t4-t3);
			fprintf(fp, "-------------------------------- \n");
			fflush(fp);
		}
		fflush(fp);
	}
#endif
}


// 2004.07.05.myevan. 궁신탄영 맵에 끼이는 문제해결
IBackground& CActorInstance::GetBackground()
{
	return IBackground::Instance();
}


void CActorInstance::SetMainInstance()
{
	m_isMain=true;
}

void CActorInstance::SetParalysis(bool isParalysis)
{
	m_isParalysis=isParalysis;
}

void CActorInstance::SetFaint(bool isFaint)
{
	m_isFaint=isFaint;
}

void CActorInstance::SetSleep(bool isSleep)
{
	m_isSleep=isSleep;

	Stop();
}

void CActorInstance::SetResistFallen(bool isResistFallen)
{
	m_isResistFallen=isResistFallen;
}

void CActorInstance::SetReachScale(float fScale)
{
	m_fReachScale=fScale;
}

float CActorInstance::__GetReachScale()
{
	return m_fReachScale;
}

float CActorInstance::__GetAttackSpeed()
{
	return m_fAtkSpd;
}

WORD CActorInstance::__GetCurrentComboType()
{
	if (IsBowMode())
		return 0;
	if (IsHandMode())
		return 0;
	if (__IsMountingHorse())
		return 0;

	return m_wcurComboType;
}

void CActorInstance::SetComboType(WORD wComboType)
{
	m_wcurComboType = wComboType;
}

void CActorInstance::SetAttackSpeed(float fAtkSpd)
{
	m_fAtkSpd=fAtkSpd;
}

void CActorInstance::SetMoveSpeed(float fMovSpd)
{
	if (m_fMovSpd==fMovSpd)
		return;

	m_fMovSpd=fMovSpd;

	if (__IsMoveMotion())
	{
		Stop();
		Move();
	}
}

void CActorInstance::SetFishingPosition(D3DXVECTOR3 & rv3Position)
{
	m_v3FishingPosition = rv3Position;
}

// ActorInstanceMotion.cpp 에 넣도록 하자
void  CActorInstance::Move()
{
	if (m_isWalking)
	{
		SetLoopMotion(CRaceMotionData::NAME_WALK, 0.15f, m_fMovSpd);
	}
	else
	{
		SetLoopMotion(CRaceMotionData::NAME_RUN, 0.15f, m_fMovSpd);
	}
}

void  CActorInstance::Stop(float fBlendingTime)
{
	__ClearMotion();
	SetLoopMotion(CRaceMotionData::NAME_WAIT, fBlendingTime);
}

void CActorInstance::SetOwner(uint32_t dwOwnerVID)
{
	m_fOwnerBaseTime=GetLocalTime();
	m_dwOwnerVID=dwOwnerVID;
}

void CActorInstance::SetActorType(UINT eType)
{
	m_eActorType=eType;
}

UINT CActorInstance::GetActorType() const
{
	return m_eActorType;
}

bool CActorInstance::IsHandMode()
{
	if (CRaceMotionData::MODE_GENERAL==GetMotionMode())
		return true;

	if (CRaceMotionData::MODE_HORSE==GetMotionMode())
		return true;

	return false;
}

bool CActorInstance::IsTwoHandMode()
{
	if (CRaceMotionData::MODE_TWOHAND_SWORD==GetMotionMode())
		return true;

	return false;
}

bool CActorInstance::IsBowMode()
{
	if (CRaceMotionData::MODE_BOW==GetMotionMode())
		return true;

	if (CRaceMotionData::MODE_HORSE_BOW==GetMotionMode())
		return true;

	return false;
}

bool CActorInstance::IsPoly()
{
	if (TYPE_POLY==m_eActorType)
		return true;

	if (TYPE_PC==m_eActorType)
		if (m_eRace >= MAIN_RACE_MAX_NUM)
			return TRUE;

	return false;
}

bool CActorInstance::IsPC()
{
	if (TYPE_PC==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsNPC()
{
	if (TYPE_NPC==m_eActorType)
		return true;

	return false;
}

#ifdef NEW_PET_SYSTEM
bool CActorInstance::IsNewPet()
{
	if ((m_eRace >= 34041) && (m_eRace <= 34111))
		return true;

	return false;
}
#endif

bool CActorInstance::IsMount()
{
	if (TYPE_NPC != m_eActorType) {
		return false;
	}

	switch (m_eRace) {
		case 20101:
		case 20102:
		case 20103:
		case 20104:
		case 20105:
		case 20106:
		case 20107:
		case 20108:
		case 20109:
		case 20113:
		case 20114:
		case 20115:
		case 20111:
		case 20117:
		case 20122:
		case 20222:
		case 20227:
		case 20231:
		case 20232:
		case 60007:
		case 60005:
		case 60006:
		case 60008:
		case 20249:
		case 20250:
		case 20251:
		case 20252:
		case 20261:
		case 20262:
		case 20265:
		case 20266:
		case 20248:
		case 20269:
		case 20270:
		case 20244:
		case 20245:
		case 20246:
		case 20247:
			return true;
		default:
			return false;
	}
}

bool CActorInstance::IsPet()
{
	if (TYPE_NPC != m_eActorType) {
		return false;
	}

#ifdef NEW_PET_SYSTEM
	if ((m_eRace >= 34041) && (m_eRace <= 34111))
		return true;
#endif

	switch (m_eRace) {
		case 34006:
		case 34009:
		case 34010:
		case 34011:
		case 34012:
		case 34008:
		case 34007:
		case 34005:
		case 34013:
		case 34014:
		case 34015:
		case 34016:
		case 34020:
		case 34019:
		case 34017:
		case 34018:
		case 34021:
		case 34022:
		case 34023:
		case 34024:
		case 34026:
		case 34027:
		case 34028:
		case 34029:
		case 34030:
		case 34031:
		case 34033:
		case 34032:
		case 34034:
		case 34035:
		case 34039:
		case 34055:
		case 34056:
		case 34057:
		case 34060:
		case 34061:
		case 34058:
		case 34059:
		case 34063:
		case 34062:
		case 34084:
		case 34085:
		case 34066:
		case 34093:
		case 34094:
		case 34114:
		case 34115:
		case 34116:

		case 34201:
		case 34202:
		case 34203:
		case 34204:
		case 34205:
		case 34206:
		case 34207:
			return true;
		default:
			return false;
	}
}

bool CActorInstance::IsEnemy()
{
	if (TYPE_ENEMY==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsStone()
{
	if (TYPE_STONE==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsWarp()
{
	if (TYPE_WARP==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsGoto()
{
	if (TYPE_GOTO==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsBuilding()
{
	if (TYPE_BUILDING==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsDoor()
{
	if (TYPE_DOOR==m_eActorType)
		return true;

	return false;
}

bool CActorInstance::IsObject()
{
	if (TYPE_OBJECT==m_eActorType)
		return true;

	return false;
}

#ifdef ENABLE_SKILL_COLOR_SYSTEM
void CActorInstance::ChangeSkillColor(const uint32_t * dwSkillColor)
{
	memcpy(m_dwSkillColor, dwSkillColor, sizeof(m_dwSkillColor));
}
#endif

void CActorInstance::DestroySystem()
{
}

void CActorInstance::DieEnd()
{
	Die();

	CGraphicThingInstance::SetMotionAtEnd();
}

void CActorInstance::Die()
{
	if (m_isRealDead)
		return;

	if (__IsMoveMotion())
		Stop();

	SetAdvancingRotation(GetRotation());

	if (IsStone())
	{
		InterceptOnceMotion(CRaceMotionData::NAME_DEAD);
	}
	else
	{
		if (!__IsDieMotion())
		{
			InterceptOnceMotion(CRaceMotionData::NAME_DEAD);
		}
	}

	m_isRealDead = TRUE;
}

bool CActorInstance::IsSleep()
{
	return m_isSleep;
}

bool CActorInstance::IsParalysis()
{
	return m_isParalysis;
}

bool CActorInstance::IsFaint()
{
	return m_isFaint;
}

bool CActorInstance::IsResistFallen()
{
	return m_isResistFallen;
}

bool CActorInstance::IsMoving()
{
	return __IsMoveMotion();
}

bool CActorInstance::IsWaiting()
{
	return __IsWaitMotion();
}

bool CActorInstance::IsDead()
{
	return m_isRealDead;
}

bool CActorInstance::IsKnockDown()
{
	return __IsKnockDownMotion();
}

bool CActorInstance::IsDamage()
{
	return __IsDamageMotion();
}

bool CActorInstance::IsAttacked()
{
	if (IsPushing())
		return true;

	if (__IsDamageMotion())
		return true;

	if (__IsKnockDownMotion())
		return true;

	if (__IsDieMotion())
		return true;

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Process
void CActorInstance::PhysicsProcess()
{
	m_PhysicsObject.Update(m_fSecondElapsed);
	AddMovement(m_PhysicsObject.GetXMovement(), m_PhysicsObject.GetYMovement(), 0.0f);
}

static bool s_nan_rootmotion_logged = false;
static bool s_nan_movement_logged = false;
static bool s_nan_position_logged = false;

#include <cmath>
static inline bool isfinitef(float v) { return std::isfinite(v) != 0; }

void CActorInstance::__AccumulationMovement(float fRot)
{
	if (CRaceMotionData::NAME_WAIT == __GetCurrentMotionIndex())
		return;

	 
	if (!isfinitef(fRot))
	{
		TraceError("ActorInstance ERROR: NaN fRot before root-motion (vid=%u race=%u motion=%u) -> forced 0",
			GetVirtualID(), GetRace(), __GetCurrentMotionIndex());
		fRot = 0.0f;
	}

	D3DXMATRIX s_matRotationZ;
	D3DXMatrixRotationZ(&s_matRotationZ, XMConvertToRadians(fRot));

	const float avg_dt = GetAverageSecondElapsed();
	UpdateTransform(&s_matRotationZ, avg_dt);

	 
	if (!isfinitef(s_matRotationZ._41) || !isfinitef(s_matRotationZ._42) || !isfinitef(s_matRotationZ._43))
	{
		TraceError("ActorInstance ERROR: NaN root-motion XYZ (vid=%u race=%u motion=%u dt=%.6f) -> clamped 0",
			GetVirtualID(), GetRace(), __GetCurrentMotionIndex(), avg_dt);
		s_matRotationZ._41 = 0.0f;
		s_matRotationZ._42 = 0.0f;
		s_matRotationZ._43 = 0.0f;
	}

	AddMovement(s_matRotationZ._41, s_matRotationZ._42, s_matRotationZ._43);
}


void CActorInstance::AccumulationMovement()
{
	if (m_pkTree)
		return;

	if (m_pkHorse)
	{
		m_pkHorse->__AccumulationMovement(m_fcurRotation);
		return;
	}

	__AccumulationMovement(m_fAdvancingRotation);
}

void CActorInstance::TransformProcess()
{
	const float ox = m_x, oy = m_y, oz = m_z;

	if (!IsParalysis())
	{
		m_x += m_v3Movement.x;
		m_y += m_v3Movement.y;
		m_z += m_v3Movement.z;
	}

	if (!std::isfinite(m_x) || !std::isfinite(m_y) || !std::isfinite(m_z))
	{
		TraceError("ActorInstance ERROR: Position became NaN (vid=%u race=%u) -> rollback",
			GetVirtualID(), GetRace());
		m_x = ox; m_y = oy; m_z = oz;
		__InitializeMovement();
	}
	else
	{
		__InitializeMovement();
	}

	SetPosition(m_x, m_y, m_z);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Process

void CActorInstance::OnUpdateCollisionData(const CStaticCollisionDataVector * pscdVector)
{
	assert(pscdVector);
	CStaticCollisionDataVector::const_iterator it;
	for(it = pscdVector->begin();it!=pscdVector->end();++it)
	{
		const CStaticCollisionData & c_rColliData = *it;
		const D3DXMATRIX & c_rMatrix = GetTransform();
		AddCollision(&c_rColliData, &c_rMatrix);
	}
}

void CActorInstance::OnUpdateHeighInstance(CAttributeInstance * pAttributeInstance)
{
	assert(pAttributeInstance);
	SetHeightInstance(pAttributeInstance);
}

bool CActorInstance::OnGetObjectHeight(float fX, float fY, float * pfHeight)
{
	if (!m_pHeightAttributeInstance)
		return false;

	if (TYPE_BUILDING != GetType())
		return false;

	return m_pHeightAttributeInstance->GetHeight(fX, fY, pfHeight) == 1 ? true : false;
}

//////////////////////////////////////////////////////////////////
// Battle
void CActorInstance::Revive()
{
	m_isSleep = false;
	m_isParalysis = false;
	m_isFaint = false;
	m_isRealDead = false;
	m_isStun = false;
	m_isWalking = false;
	m_isMain = false;
	m_isResistFallen = false;

	__InitializeCollisionData();
}

bool CActorInstance::IsStun()
{
	return m_isStun;
}

void CActorInstance::Stun()
{
	m_isStun = true;
}

void CActorInstance::SetWalkMode()
{
	m_isWalking = true;
	if (CRaceMotionData::NAME_RUN == GET_MOTION_INDEX(m_kCurMotNode.dwMotionKey))
		SetLoopMotion(CRaceMotionData::NAME_WALK, 0.15f, m_fMovSpd);
}

void CActorInstance::SetRunMode()
{
	m_isWalking = false;
	if (CRaceMotionData::NAME_WALK == GET_MOTION_INDEX(m_kCurMotNode.dwMotionKey))
		SetLoopMotion(CRaceMotionData::NAME_RUN, 0.15f, m_fMovSpd);
}

MOTION_KEY CActorInstance::GetNormalAttackIndex()
{
	WORD wMotionIndex;
	m_pkCurRaceData->GetNormalAttackIndex(GetMotionMode(), &wMotionIndex);

	return MAKE_MOTION_KEY(GetMotionMode(), wMotionIndex);
}

//////////////////////////////////////////////////////////////////
// Movement
void CActorInstance::__InitializeMovement()
{
	m_v3Movement.x = 0.0f;
	m_v3Movement.y = 0.0f;
	m_v3Movement.z = 0.0f;
}

void CActorInstance::AddMovement(float fx, float fy, float fz)
{
	m_v3Movement.x += fx;
	m_v3Movement.y += fy;
	m_v3Movement.z += fz;
}

const float gc_fActorSlideMoveSpeed = 5.0f;

void CActorInstance::AdjustDynamicCollisionMovement(const CActorInstance * c_pActorInstance)
{
	if (m_pkHorse)
	{
		m_pkHorse->AdjustDynamicCollisionMovement(c_pActorInstance);
		return;
	}

	// NOTE : 기존의 Sphere Overlap됬을경�?처리가 비비기를 하면은 Penetration될 위험이 많아서 ( 실제로도 나왔고 --)
	// Sphere간 Collision이 생겼을 경우 이전위치로 RollBack하는 방식으로 바꿨다.
	// 단 BGObject에 대해서만.

	if (isAttacking() )
		return;

	UINT uActorType = c_pActorInstance->GetActorType();
	if( (uActorType == TYPE_BUILDING) || (uActorType == TYPE_OBJECT) || (uActorType == TYPE_DOOR) || (uActorType == TYPE_STONE))
	{
		BlockMovement();

		//Movement초기화
	/*	m_v3Movement = D3DXVECTOR3(0.f,0.f,0.f);

		TCollisionPointInstanceListIterator itMain = m_BodyPointInstanceList.begin();
		for (; itMain != m_BodyPointInstanceList.end(); ++itMain)
		{
			CDynamicSphereInstanceVector & c_rMainSphereVector = (*itMain).SphereInstanceVector;
			for (uint32_t i = 0; i < c_rMainSphereVector.size(); ++i)
			{
				CDynamicSphereInstance & c_rMainSphere = c_rMainSphereVector[i];
				c_rMainSphere.v3Position =c_rMainSphere.v3LastPosition;
			}
		}*/
	}
	else
	{

		float move_length = D3DXVec3Length(&m_v3Movement);
		if (move_length>gc_fActorSlideMoveSpeed)
			m_v3Movement*=gc_fActorSlideMoveSpeed/move_length;

		TCollisionPointInstanceListIterator itMain = m_BodyPointInstanceList.begin();
		for (; itMain != m_BodyPointInstanceList.end(); ++itMain)
		{
			CDynamicSphereInstanceVector & c_rMainSphereVector = (*itMain).SphereInstanceVector;
			for (uint32_t i = 0; i < c_rMainSphereVector.size(); ++i)
			{
				CDynamicSphereInstance & c_rMainSphere = c_rMainSphereVector[i];

				TCollisionPointInstanceList::const_iterator itOpp = c_pActorInstance->m_BodyPointInstanceList.begin();
				for(;itOpp != c_pActorInstance->m_BodyPointInstanceList.end();++itOpp)
				{
					CSphereCollisionInstance s;
					s.GetAttribute().fRadius=itOpp->SphereInstanceVector[0].fRadius;
					s.GetAttribute().v3Position=itOpp->SphereInstanceVector[0].v3Position;
					D3DXVECTOR3 v3Delta = s.GetCollisionMovementAdjust(c_rMainSphere);
					m_v3Movement+=v3Delta;
					c_rMainSphere.v3Position+=v3Delta;

					if (v3Delta.x !=0.0f || v3Delta.y !=0.0f || v3Delta.z !=0.0f )
					{
						move_length = D3DXVec3Length(&m_v3Movement);
						if (move_length>gc_fActorSlideMoveSpeed)
						{
							m_v3Movement*=gc_fActorSlideMoveSpeed/move_length;
							c_rMainSphere.v3Position = c_rMainSphere.v3LastPosition;
							c_rMainSphere.v3Position+=m_v3Movement;
						}
					}
				}
			}
		}
	}
}


void CActorInstance::__AdjustCollisionMovement(const CGraphicObjectInstance * c_pGraphicObjectInstance)
{
	if (m_pkHorse)
	{
		m_pkHorse->__AdjustCollisionMovement(c_pGraphicObjectInstance);
		return;
	}

	// Body는 하나임을 가정합니다.

	if (m_v3Movement.x == 0.0f && m_v3Movement.y == 0.0f && m_v3Movement.z == 0.0f)
		return;

	float move_length = D3DXVec3Length(&m_v3Movement);
	if (move_length>gc_fActorSlideMoveSpeed)
		m_v3Movement*=gc_fActorSlideMoveSpeed/move_length;

	TCollisionPointInstanceListIterator itMain = m_BodyPointInstanceList.begin();
	for (; itMain != m_BodyPointInstanceList.end(); ++itMain)
	{
		CDynamicSphereInstanceVector & c_rMainSphereVector = (*itMain).SphereInstanceVector;
		for (uint32_t i = 0; i < c_rMainSphereVector.size(); ++i)
		{
			CDynamicSphereInstance & c_rMainSphere = c_rMainSphereVector[i];

			D3DXVECTOR3 v3Delta = c_pGraphicObjectInstance->GetCollisionMovementAdjust(c_rMainSphere);
			m_v3Movement+=v3Delta;
			c_rMainSphere.v3Position+=v3Delta;

			if (v3Delta.x !=0.0f || v3Delta.y !=0.0f || v3Delta.z !=0.0f )
			{
				move_length = D3DXVec3Length(&m_v3Movement);
				if (move_length>gc_fActorSlideMoveSpeed)
				{
					m_v3Movement*=gc_fActorSlideMoveSpeed/move_length;
					c_rMainSphere.v3Position = c_rMainSphere.v3LastPosition;
					c_rMainSphere.v3Position+=m_v3Movement;
				}
			}

			/*if (c_pObjectInstance->CollisionDynamicSphere(c_rMainSphere))
			{
				const D3DXVECTOR3 & c_rv3Position = c_pObjectInstance->GetPosition();
				//if (GetVector3Distance(c_rMainSphere.v3Position, c_rv3Position) <
				//	GetVector3Distance(c_rMainSphere.v3LastPosition, c_rv3Position))
				{
					return TRUE;
				}

				return FALSE;
			}*/
		}
	}
}

bool CActorInstance::IsMovement()
{
	if (m_pkHorse)
		if (m_pkHorse->IsMovement())
			return true;

	if (0.0f != m_v3Movement.x)
		return true;
	if (0.0f != m_v3Movement.y)
		return true;
	if (0.0f != m_v3Movement.z)
		return true;

	return false;
}

float CActorInstance::GetHeight()
{
#ifdef ENABLE_RACE_HEIGHT
	uint32_t dwRace = GetRace();
	float fRaceHeight = CRaceManager::instance().GetRaceHeight(dwRace);
	if (fRaceHeight == 0.0f)
	{
		fRaceHeight = CGraphicThingInstance::GetHeight();
		CRaceManager::instance().SetRaceHeight(dwRace, fRaceHeight, 100.0f);
	}
	
	return fRaceHeight;
#else
	return CGraphicThingInstance::GetHeight();
#endif
}

void CActorInstance::RenderAllAttachingEffect()
{
	for (const auto& it : m_AttachingEffectList)
		CEffectManager::Instance().RenderOne(it.dwEffectIndex);
}

bool CActorInstance::IntersectDefendingSphere()
{
	for (TCollisionPointInstanceList::iterator it = m_DefendingPointInstanceList.begin(); it != m_DefendingPointInstanceList.end(); ++it)
	{
		CDynamicSphereInstanceVector & rSphereInstanceVector = (*it).SphereInstanceVector;

		CDynamicSphereInstanceVector::iterator it2 = rSphereInstanceVector.begin();
		for (; it2 != rSphereInstanceVector.end(); ++it2)
		{
			CDynamicSphereInstance & rInstance = *it2;
			D3DXVECTOR3 v3SpherePosition = rInstance.v3Position;
			float fRadius = rInstance.fRadius;

			D3DXVECTOR3 v3Orig;
			D3DXVECTOR3 v3Dir;
			float fRange;
			ms_Ray.GetStartPoint(&v3Orig);
			ms_Ray.GetDirection(&v3Dir, &fRange);

			D3DXVECTOR3 v3Distance = v3Orig - v3SpherePosition;
			float b = D3DXVec3Dot(&v3Dir, &v3Distance);
			float c = D3DXVec3Dot(&v3Distance, &v3Distance) - fRadius*fRadius;

			if (b*b - c >= 0)
				return true;
		}
	}
	return false;
}

bool CActorInstance::__IsMountingHorse()
{
	return nullptr != m_pkHorse;
}

void CActorInstance::MountHorse(CActorInstance * pkHorse)
{
	m_pkHorse = pkHorse;

	if (m_pkHorse)
	{
		m_pkHorse->SetCurPixelPosition(NEW_GetCurPixelPositionRef());
		m_pkHorse->SetRotation(GetRotation());
		m_pkHorse->SetAdvancingRotation(GetRotation());
	}
}

void CActorInstance::__CreateTree(const char * c_szFileName)
{
	__DestroyTree();

	CSpeedTreeForestDirectX8& rkForest=CSpeedTreeForestDirectX8::Instance();
	m_pkTree=rkForest.CreateInstance(m_x, m_y, m_z, GetCaseCRC32(c_szFileName, strlen(c_szFileName)), c_szFileName);
	m_pkTree->SetPosition(m_x, m_y, m_z);
	m_pkTree->UpdateBoundingSphere();
	m_pkTree->UpdateCollisionData();
}

void CActorInstance::__DestroyTree()
{
	if (!m_pkTree)
		return;

	CSpeedTreeForestDirectX8::Instance().DeleteInstance(m_pkTree);
}

void CActorInstance::__SetTreePosition(float fx, float fy, float fz)
{
	if (!m_pkTree)
		return;
	if (m_x == fx && m_y == fy && m_z == fz)
		return;

	m_pkTree->SetPosition(fx, fy, fz);
	m_pkTree->UpdateBoundingSphere();
	m_pkTree->UpdateCollisionData();
}

void CActorInstance::ClearAttachingEffect()
{
	__ClearAttachingEffect();
}

void CActorInstance::Destroy()
{
	ClearFlyTargeter();

	m_HitDataMap.clear();
	m_MotionDeque.clear();

	if (m_pAttributeInstance)
	{
		m_pAttributeInstance->Clear();
		CAttributeInstance::Delete(m_pAttributeInstance);
		m_pAttributeInstance = nullptr;
	}

	__ClearAttachingEffect();

	Clear();

	__DestroyWeaponTrace();
	__DestroyTree();
	//m_PhysicsObject.SetActorInstance(NULL);


	__Initialize();
}

void CActorInstance::__InitializeRotationData()
{
	m_fAtkDirRot = 0.0f;
	m_fcurRotation = 0.0f;
	m_rotBegin = 0.0f;
	m_rotEnd = 0.0f;
	m_rotEndTime = 0.0f;
	m_rotBeginTime = 0.0f;
	m_rotBlendTime = 0.0f;
	m_fAdvancingRotation = 0.0f;
	m_rotX = 0.0f;
	m_rotY = 0.0f;
}


void CActorInstance::__InitializeStateData()
{
	m_bEffectInitialized = false;

	m_isPreInput = FALSE;
	m_isNextPreInput = FALSE;

	m_isSleep = FALSE;
	m_isParalysis = FALSE;
	m_isFaint = FALSE;
	m_isRealDead = FALSE;
	m_isWalking = FALSE;
	m_isMain = FALSE;
	m_isStun = FALSE;
	m_isHiding = FALSE;
	m_isResistFallen = FALSE;

	m_iRenderMode = RENDER_MODE_NORMAL;
	m_fAlphaValue = 0.0f;
	m_AddColor = D3DXCOLOR(0.0f, 0.0f, 0.0f, 1.0f);

	m_dwMtrlColor=0xffffffff;
	m_dwMtrlAlpha=0xff000000;

	m_dwBattleHitEffectID = 0;
	m_dwBattleAttachEffectID = 0;
}

void CActorInstance::__InitializeMotionData()
{
	m_wcurMotionMode = CRaceMotionData::MODE_GENERAL;
	m_wcurComboType = 0;

	m_fReachScale=1.0f;
	m_fMovSpd=1.0f;
	m_fAtkSpd=1.0f;

	m_fInvisibleTime = 0.0f;

	m_kSplashArea.isEnableHitProcess=TRUE;
	m_kSplashArea.uSkill=0;
	m_kSplashArea.MotionKey=0;
	m_kSplashArea.fDisappearingTime = 0.0f;
	m_kSplashArea.SphereInstanceVector.clear();
	m_kSplashArea.HittedInstanceMap.clear();

	memset(&m_kCurMotNode, 0, sizeof(m_kCurMotNode));

	__ClearCombo();
}

void CActorInstance::__Initialize()
{
	m_pkCurRaceMotionData= nullptr;
	m_pkCurRaceData= nullptr;
	m_pkHorse= nullptr;
	m_pkTree= nullptr;

	m_fOwnerBaseTime=0.0f;

	m_eActorType = TYPE_PC;
	m_eRace = 0;

	m_eShape = 0;
	m_eHair = 0;

	m_dwSelfVID = 0;
	m_dwOwnerVID = 0;

	m_pkEventHandler = nullptr;

	m_PhysicsObject.Initialize();

	m_pAttributeInstance = nullptr;

	m_pFlyEventHandler = nullptr;

	m_v3FishingPosition = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	m_iFishingEffectID = -1;

	m_pkHorse = nullptr;

	__InitializePositionData();
	__InitializeRotationData();
	__InitializeMotionData();
	__InitializeStateData();
	__InitializeCollisionData();

	__BlendAlpha_Initialize();

	ClearFlyTargeter();

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	memset(m_dwSkillColor, 0, sizeof(m_dwSkillColor));
#endif

}

CActorInstance::CActorInstance()
{
	__Initialize();
	m_PhysicsObject.SetActorInstance(this);
}

CActorInstance::~CActorInstance()
{
	Destroy();
}
