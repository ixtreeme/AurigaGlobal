#include "StdAfx.h"
#include "NetworkActorManager.h"
#include "PythonCharacterManager.h"
#include "PythonItem.h"

#include "AbstractPlayer.h"

void SNetworkActorData::UpdatePosition()
{
	uint32_t dwClientCurTime=ELTimer_GetMSec();
	uint32_t dwElapsedTime=dwClientCurTime-m_dwClientSrcTime;

	if (dwElapsedTime<m_dwDuration)
	{
		float fRate=float(dwElapsedTime)/float(m_dwDuration);
		m_lCurX= int32_t((m_lDstX-m_lSrcX)*fRate+m_lSrcX);
		m_lCurY= int32_t((m_lDstY-m_lSrcY)*fRate+m_lSrcY);
	}
	else
	{
		m_lCurX=m_lDstX;
		m_lCurY=m_lDstY;
	}
}

void SNetworkActorData::SetDstPosition(uint32_t dwServerTime, int32_t lDstX, int32_t lDstY, uint32_t dwDuration)
{
	m_lSrcX=m_lCurX;
	m_lSrcY=m_lCurY;
	m_lDstX=lDstX;
	m_lDstY=lDstY;

	m_dwDuration=dwDuration;
	m_dwServerSrcTime=dwServerTime;
	m_dwClientSrcTime=ELTimer_GetMSec();
}

void SNetworkActorData::SetPosition(int32_t lPosX, int32_t lPosY)
{
	m_lDstX=m_lSrcX=m_lCurX=lPosX;
	m_lDstY=m_lSrcY=m_lCurY=lPosY;
}

// NETWORK_ACTOR_DATA_COPY
SNetworkActorData::SNetworkActorData(const SNetworkActorData& src)
{
	__copy__(src);
}

void SNetworkActorData::operator=(const SNetworkActorData& src)
{
	__copy__(src);
}

void SNetworkActorData::__copy__(const SNetworkActorData& src)
{
	m_bType = src.m_bType;
	m_dwVID = src.m_dwVID;
	m_dwStateFlags = src.m_dwStateFlags;
	m_dwEmpireID = src.m_dwEmpireID;
	m_dwRace = src.m_dwRace;
	m_dwMovSpd = src.m_dwMovSpd;
	m_dwAtkSpd = src.m_dwAtkSpd;
	m_fRot = src.m_fRot;
	m_lCurX = src.m_lCurX;
	m_lCurY = src.m_lCurY;
	m_lSrcX = src.m_lSrcX;
	m_lSrcY = src.m_lSrcY;
	m_lDstX = src.m_lDstX;
	m_lDstY = src.m_lDstY;
	m_kAffectFlags.CopyInstance(src.m_kAffectFlags);

	m_dwServerSrcTime = src.m_dwServerSrcTime;
	m_dwClientSrcTime = src.m_dwClientSrcTime;
	m_dwDuration = src.m_dwDuration;
#ifdef ENABLE_RUNE_SYSTEM
	m_dwRune = src.m_dwRune;
#endif
	m_dwArmor = src.m_dwArmor;
	m_dwWeapon = src.m_dwWeapon;
	m_dwHair = src.m_dwHair;

#ifdef ENABLE_ACCE_SYSTEM
	m_dwAcce = src.m_dwAcce;
#endif
#ifdef ENABLE_COSTUME_EFFECT
	m_dwEffectBody = src.m_dwEffectBody;
	m_dwEffectWeapon = src.m_dwEffectWeapon;
#endif
#ifdef ENABLE_COSTUME_EFFECT
	m_dwEffectBody = src.m_dwEffectBody;
	m_dwEffectWeapon = src.m_dwEffectWeapon;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	m_bLanguage = src.m_bLanguage;
#endif
	m_dwOwnerVID = src.m_dwOwnerVID;

	m_sAlignment = src.m_sAlignment;
	m_byPKMode = src.m_byPKMode;
	m_dwMountVnum = src.m_dwMountVnum;

	m_dwGuildID = src.m_dwGuildID;
	m_dwLevel = src.m_dwLevel;
	m_stName = src.m_stName;

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	memcpy(m_dwSkillColor, src.m_dwSkillColor, sizeof(m_dwSkillColor));
#endif
}
// END_OF_NETWORK_ACTOR_DATA_COPY

SNetworkActorData::SNetworkActorData()
{
	SetPosition(0, 0);

	m_bType=0;
	m_dwVID=0;
	m_dwStateFlags=0;
	m_dwRace=0;
	m_dwMovSpd=0;
	m_dwAtkSpd=0;
	m_fRot=0.0f;
#ifdef ENABLE_RUNE_SYSTEM
	m_dwRune = 0;
#endif
	m_dwArmor=0;
	m_dwWeapon=0;
	m_dwHair=0;
	
#ifdef ENABLE_ACCE_SYSTEM
	m_dwAcce = 0;
#endif
#ifdef ENABLE_COSTUME_EFFECT
	m_dwEffectBody = 0;
	m_dwEffectWeapon = 0;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	m_bLanguage = 0;
#endif
	m_dwEmpireID=0;
	m_dwOwnerVID=0;

	m_dwDuration=0;
	m_dwClientSrcTime=0;
	m_dwServerSrcTime=0;

	m_sAlignment=0;
	m_byPKMode=0;
	m_dwMountVnum=0;
	m_stName="";

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	memset(m_dwSkillColor, 0, sizeof(m_dwSkillColor));
#endif
	m_kAffectFlags.Clear();
}

////////////////////////////////////////////////////////////////////////////////

CNetworkActorManager::CNetworkActorManager() : m_lMainPosX(0), m_lMainPosY(0)
{
	m_dwMainVID=0;
}

CNetworkActorManager::~CNetworkActorManager()
{
}

void CNetworkActorManager::Destroy()
{
	m_kNetActorDict.clear();

	m_dwMainVID=0;
	m_lMainPosX=0;
	m_lMainPosY=0;
}

void CNetworkActorManager::SetMainActorVID(uint32_t dwVID)
{
	m_dwMainVID=dwVID;
	m_lMainPosX=0;
	m_lMainPosY=0;

	m_kNetActorDict.clear();
}

void CNetworkActorManager::Update()
{
	__OLD_Update();
}

void CNetworkActorManager::__OLD_Update()
{
	__UpdateMainActor();

	CPythonCharacterManager& rkChrMgr=__GetCharacterManager();

	std::map<uint32_t, SNetworkActorData>::iterator i;
	for (i=m_kNetActorDict.begin(); i!=m_kNetActorDict.end(); ++i)
	{
		SNetworkActorData& rkNetActorData=i->second;
		rkNetActorData.UpdatePosition();

		CInstanceBase * pkInstFind = rkChrMgr.GetInstancePtr(rkNetActorData.m_dwVID);
		if (!pkInstFind)
		{
			if (__IsVisibleActor(rkNetActorData))
				__AppendCharacterManagerActor(rkNetActorData);
		}
	}
}

CInstanceBase* CNetworkActorManager::__FindActor(SNetworkActorData& rkNetActorData, int32_t lDstX, int32_t lDstY)
{
	CPythonCharacterManager& rkChrMgr=__GetCharacterManager();
	CInstanceBase * pkInstFind = rkChrMgr.GetInstancePtr(rkNetActorData.m_dwVID);
	if (!pkInstFind)
	{
		if (__IsVisiblePos(lDstX, lDstY))
			return __AppendCharacterManagerActor(rkNetActorData);

		return nullptr;
	}

	return pkInstFind;
}

CInstanceBase* CNetworkActorManager::__FindActor(SNetworkActorData& rkNetActorData)
{
	CPythonCharacterManager& rkChrMgr=__GetCharacterManager();
	CInstanceBase * pkInstFind = rkChrMgr.GetInstancePtr(rkNetActorData.m_dwVID);
	if (!pkInstFind)
	{
		if (__IsVisibleActor(rkNetActorData))
			return __AppendCharacterManagerActor(rkNetActorData);

		return nullptr;
	}

	return pkInstFind;
}

void CNetworkActorManager::__RemoveAllGroundItems()
{
	CPythonItem& rkItemMgr=CPythonItem::Instance();
	rkItemMgr.DeleteAllItems();
}

void CNetworkActorManager::__RemoveAllActors()
{
	m_kNetActorDict.clear();

	CPythonCharacterManager & rkChrMgr = CPythonCharacterManager::Instance();
	rkChrMgr.DeleteAllInstances();
}

void CNetworkActorManager::__RemoveDynamicActors()
{
	//copy it
	std::vector<uint32_t> dwCharacterVIDList;

	CPythonCharacterManager& rkChrMgr=CPythonCharacterManager::Instance();
	for(CPythonCharacterManager::CharacterIterator i = rkChrMgr.CharacterInstanceBegin(); i!=rkChrMgr.CharacterInstanceEnd(); ++i)
	{
		dwCharacterVIDList.push_back( (*i)->GetVirtualID() );
	}

	for( int i = 0; i < dwCharacterVIDList.size(); ++i )
	{
		CInstanceBase*  pkInstEach = rkChrMgr.GetInstancePtr( dwCharacterVIDList[i] );
		if( !pkInstEach )
			continue;

		CActorInstance* rkActorEach=pkInstEach->GetGraphicThingInstancePtr();
#ifdef ENABLE_BUGFIXES
		if( rkActorEach->IsPC() || rkActorEach->IsNPC() || rkActorEach->IsEnemy() || rkActorEach->IsStone())
#else
		if( rkActorEach->IsPC() || rkActorEach->IsNPC() || rkActorEach->IsEnemy())
#endif
		{
			rkChrMgr.DeleteInstance(dwCharacterVIDList[i]);
			std::map<uint32_t, SNetworkActorData>::iterator it =m_kNetActorDict.find(dwCharacterVIDList[i]);
			if (it != m_kNetActorDict.end())
			{
				m_kNetActorDict.erase(it);
			}
		}
	}

	rkChrMgr.DestroyDeadInstanceList();
}

void CNetworkActorManager::__UpdateMainActor()
{
	CPythonCharacterManager & rkChrMgr = CPythonCharacterManager::Instance();
	CInstanceBase* pkInstMain=rkChrMgr.GetMainInstancePtr();
	if (!pkInstMain)
		return;

	TPixelPosition kPPosMain;
	pkInstMain->NEW_GetPixelPosition(&kPPosMain);

	m_lMainPosX= int32_t(kPPosMain.x);
	m_lMainPosY= int32_t(kPPosMain.y);
}

extern bool IsWall(unsigned race);

bool CNetworkActorManager::__IsVisibleActor(const SNetworkActorData& c_rkNetActorData)
{
	if (__IsMainActorVID(c_rkNetActorData.m_dwVID))
		return true;

	// 2004.08.02.myevan.항상 보이기 플래그
	if (c_rkNetActorData.m_kAffectFlags.IsSet(CInstanceBase::AFFECT_SHOW_ALWAYS))
		return true;

	if (__IsVisiblePos(c_rkNetActorData.m_lCurX, c_rkNetActorData.m_lCurY))
		return true;

	if (IsWall(c_rkNetActorData.m_dwRace))
		return true;

 	return false;
}

bool CNetworkActorManager::__IsVisiblePos(int32_t lPosX, int32_t lPosY)
{
	int32_t dx = lPosX-m_lMainPosX;
	int32_t dy = lPosY-m_lMainPosY;
	int32_t len = (int32_t)sqrt(double(dx * dx + dy * dy));

	extern int CHAR_STAGE_VIEW_BOUND;
	if (len < CHAR_STAGE_VIEW_BOUND)
		return true;

	return false;
}

bool CNetworkActorManager::__IsMainActorVID(uint32_t dwVID)
{
	if (dwVID == m_dwMainVID)
		return true;

	return false;
}

CPythonCharacterManager& CNetworkActorManager::__GetCharacterManager()
{
	return CPythonCharacterManager::Instance();
}

void CNetworkActorManager::__RemoveCharacterManagerActor(SNetworkActorData& rkNetActorData)
{
	CPythonCharacterManager & rkChrMgr = CPythonCharacterManager::Instance();

	if (__IsMainActorVID(rkNetActorData.m_dwVID))
	{
		rkChrMgr.DeleteInstance(rkNetActorData.m_dwVID);
	}
	else
	{
		rkChrMgr.DeleteInstanceByFade(rkNetActorData.m_dwVID);
	}
}

CInstanceBase* CNetworkActorManager::__AppendCharacterManagerActor(SNetworkActorData& rkNetActorData)
{
	Lognf(0, "CNetworkActorManager::__AppendCharacterManagerActor(%d, %s)", rkNetActorData.m_dwVID, rkNetActorData.m_stName.c_str());

	uint32_t dwVID=rkNetActorData.m_dwVID;

	CPythonCharacterManager & rkChrMgr = CPythonCharacterManager::Instance();


	CInstanceBase::SCreateData kCreateData;
	kCreateData.m_bType=rkNetActorData.m_bType;
	kCreateData.m_dwLevel=rkNetActorData.m_dwLevel;
	kCreateData.m_dwGuildID=rkNetActorData.m_dwGuildID;
	kCreateData.m_dwEmpireID=rkNetActorData.m_dwEmpireID;
	kCreateData.m_dwVID=rkNetActorData.m_dwVID;
	kCreateData.m_dwMountVnum=rkNetActorData.m_dwMountVnum;
	kCreateData.m_dwRace=rkNetActorData.m_dwRace;
	kCreateData.m_fRot=rkNetActorData.m_fRot;
 	kCreateData.m_lPosX=rkNetActorData.m_lCurX;
	kCreateData.m_lPosY=rkNetActorData.m_lCurY;
	kCreateData.m_stName=rkNetActorData.m_stName;
	kCreateData.m_dwStateFlags=rkNetActorData.m_dwStateFlags;
	kCreateData.m_dwMovSpd=rkNetActorData.m_dwMovSpd;
	kCreateData.m_dwAtkSpd=rkNetActorData.m_dwAtkSpd;
	kCreateData.m_sAlignment=rkNetActorData.m_sAlignment;
	kCreateData.m_byPKMode=rkNetActorData.m_byPKMode;
	kCreateData.m_kAffectFlags=rkNetActorData.m_kAffectFlags;
#ifdef ENABLE_RUNE_SYSTEM
	kCreateData.m_dwRune = rkNetActorData.m_dwRune;
#endif
	kCreateData.m_dwArmor=rkNetActorData.m_dwArmor;
	kCreateData.m_dwWeapon=rkNetActorData.m_dwWeapon;
	kCreateData.m_dwHair=rkNetActorData.m_dwHair;
#ifdef ENABLE_ACCE_SYSTEM
	kCreateData.m_dwAcce = rkNetActorData.m_dwAcce;
#endif
#ifdef ENABLE_COSTUME_EFFECT
	kCreateData.m_dwEffectBody = rkNetActorData.m_dwEffectBody;
	kCreateData.m_dwEffectWeapon = rkNetActorData.m_dwEffectWeapon;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	kCreateData.m_bLanguage = rkNetActorData.m_bLanguage;
#endif
	kCreateData.m_isMain=__IsMainActorVID(dwVID);

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	memcpy(kCreateData.m_dwSkillColor, rkNetActorData.m_dwSkillColor, sizeof(kCreateData.m_dwSkillColor));
#endif

	CInstanceBase* pOldInstance = rkChrMgr.GetInstancePtr(dwVID);
	if (pOldInstance)
	{
		TPixelPosition oldPos;
		pOldInstance->GetGraphicThingInstancePtr()->GetPixelPosition( &oldPos );

		bool bIsMountingHorse = pOldInstance->IsMountingHorse();
		if( (bIsMountingHorse && kCreateData.m_dwMountVnum == 0) ||
			(!bIsMountingHorse && kCreateData.m_dwMountVnum != 0) )
		{
			//Override Pos
			kCreateData.m_lPosX = oldPos.x;
			kCreateData.m_lPosY = -oldPos.y;
		}

		rkChrMgr.DeleteInstance(dwVID);
	}


	CInstanceBase* pNewInstance = rkChrMgr.CreateInstance(kCreateData);
	if (!pNewInstance)
		return nullptr;

	if (kCreateData.m_isMain)
	{
		IAbstractPlayer & rkPlayer = IAbstractPlayer::GetSingleton();
		rkPlayer.SetMainCharacterIndex(dwVID);


		m_lMainPosX=rkNetActorData.m_lCurX;
		m_lMainPosY=rkNetActorData.m_lCurY;
	}

	uint32_t dwClientCurTime=ELTimer_GetMSec();
	uint32_t dwElapsedTime=dwClientCurTime-rkNetActorData.m_dwClientSrcTime;

	if (dwElapsedTime<rkNetActorData.m_dwDuration)
	{
		TPixelPosition kPPosDst;
		kPPosDst.x=float(rkNetActorData.m_lDstX);
		kPPosDst.y=float(rkNetActorData.m_lDstY);
		kPPosDst.z=0;
		pNewInstance->PushTCPState(rkNetActorData.m_dwServerSrcTime+dwElapsedTime, kPPosDst, rkNetActorData.m_fRot, CInstanceBase::FUNC_MOVE, 0);
	}
	return pNewInstance;
}

void CNetworkActorManager::AppendActor(const SNetworkActorData& c_rkNetActorData)
{
	if (__IsMainActorVID(c_rkNetActorData.m_dwVID))
	{
		bool bChangeMountStatus = false;

		CInstanceBase* pMainInstance = CPythonCharacterManager::Instance().GetInstancePtr(c_rkNetActorData.m_dwVID);
		if( pMainInstance )
		{
			bool bIsMountingHorse = pMainInstance->IsMountingHorse();
			bChangeMountStatus = (bIsMountingHorse && c_rkNetActorData.m_dwMountVnum == 0) || (!bIsMountingHorse && c_rkNetActorData.m_dwMountVnum != 0);

#ifdef ENABLE_SKILL_COLOR_SYSTEM
			pMainInstance->ChangeSkillColor(*c_rkNetActorData.m_dwSkillColor);
#endif

		}

		if( !bChangeMountStatus )
		{
			//__RemoveAllActors();
			__RemoveDynamicActors();
			__RemoveAllGroundItems();
		}

	}

	SNetworkActorData& rkNetActorData=m_kNetActorDict[c_rkNetActorData.m_dwVID];
	rkNetActorData=c_rkNetActorData;

	if (__IsVisibleActor(rkNetActorData))
	{
		if (!__AppendCharacterManagerActor(rkNetActorData))
			m_kNetActorDict.erase(c_rkNetActorData.m_dwVID);
	}
}

void CNetworkActorManager::RemoveActor(uint32_t dwVID)
{
	std::map<uint32_t, SNetworkActorData>::iterator f=m_kNetActorDict.find(dwVID);
	if (m_kNetActorDict.end()==f)
	{
#ifdef _DEBUG
		TraceError("CNetworkActorManager::RemoveActor(dwVID=%d) - NOT EXIST VID", dwVID);
#endif
		return;
	}

	SNetworkActorData& rkNetActorData=f->second;
	__RemoveCharacterManagerActor(rkNetActorData);

	m_kNetActorDict.erase(f);
}

void CNetworkActorManager::UpdateActor(const SNetworkUpdateActorData& c_rkNetUpdateActorData)
{
	std::map<uint32_t, SNetworkActorData>::iterator f=m_kNetActorDict.find(c_rkNetUpdateActorData.m_dwVID);
	if (m_kNetActorDict.end()==f)
	{
#ifdef _DEBUG
		TraceError("CNetworkActorManager::UpdateActor(dwVID=%d) - NOT EXIST VID", c_rkNetUpdateActorData.m_dwVID);
#endif
		return;
	}

	SNetworkActorData& rkNetActorData=f->second;

	CInstanceBase* pkInstFind=__FindActor(rkNetActorData);
	if (pkInstFind)
	{
#ifdef ENABLE_RUNE_SYSTEM
		pkInstFind->SetRune(c_rkNetUpdateActorData.m_dwRune);
#endif
		pkInstFind->ChangeArmor(c_rkNetUpdateActorData.m_dwArmor);
		pkInstFind->ChangeWeapon(c_rkNetUpdateActorData.m_dwWeapon);
		pkInstFind->ChangeHair(c_rkNetUpdateActorData.m_dwHair);
#ifdef ENABLE_ACCE_SYSTEM
		pkInstFind->ChangeAcce(c_rkNetUpdateActorData.m_dwAcce);
#endif
#ifdef ENABLE_COSTUME_EFFECT
		pkInstFind->ChangeCostumeEffect(true, c_rkNetUpdateActorData.m_dwEffectBody);
		pkInstFind->ChangeCostumeEffect(false, c_rkNetUpdateActorData.m_dwEffectWeapon);
#endif
#ifdef ENABLE_MULTI_LANGUAGE
		pkInstFind->SetLanguage(c_rkNetUpdateActorData.m_bLanguage);
#endif
		pkInstFind->ChangeGuild(c_rkNetUpdateActorData.m_dwGuildID);
		pkInstFind->SetAffectFlagContainer(c_rkNetUpdateActorData.m_kAffectFlags);
		pkInstFind->SetMoveSpeed(c_rkNetUpdateActorData.m_dwMovSpd);
		pkInstFind->SetAttackSpeed(c_rkNetUpdateActorData.m_dwAtkSpd);
		pkInstFind->SetAlignment(c_rkNetUpdateActorData.m_sAlignment);
		pkInstFind->SetPKMode(c_rkNetUpdateActorData.m_byPKMode);
		pkInstFind->SetStateFlags(c_rkNetUpdateActorData.m_dwStateFlags);

#ifdef ENABLE_SKILL_COLOR_SYSTEM
		pkInstFind->ChangeSkillColor(*c_rkNetUpdateActorData.m_dwSkillColor);
#endif
		//if( c_rkNetUpdateActorData.m_dwMountVnum != 0 )
		//{
		//	pkInstFind->MountHorse(c_rkNetUpdateActorData.m_dwMountVnum);
		//}
		//else
		//{
		//	if( pkInstFind->IsMountingHorse() )
		//	{
		//		pkInstFind->DismountHorse();
		//	}
		//}
	}

	rkNetActorData.m_kAffectFlags=c_rkNetUpdateActorData.m_kAffectFlags;
	rkNetActorData.m_dwGuildID=c_rkNetUpdateActorData.m_dwGuildID;
	rkNetActorData.m_dwMovSpd=c_rkNetUpdateActorData.m_dwMovSpd;
	rkNetActorData.m_dwAtkSpd=c_rkNetUpdateActorData.m_dwAtkSpd;
#ifdef ENABLE_RUNE_SYSTEM
	rkNetActorData.m_dwRune = c_rkNetUpdateActorData.m_dwRune;
#endif
	rkNetActorData.m_dwArmor=c_rkNetUpdateActorData.m_dwArmor;
	rkNetActorData.m_dwWeapon=c_rkNetUpdateActorData.m_dwWeapon;
	rkNetActorData.m_dwHair=c_rkNetUpdateActorData.m_dwHair;
#ifdef ENABLE_ACCE_SYSTEM
	rkNetActorData.m_dwAcce = c_rkNetUpdateActorData.m_dwAcce;
#endif
#ifdef ENABLE_COSTUME_EFFECT
	rkNetActorData.m_dwEffectBody = c_rkNetUpdateActorData.m_dwEffectBody;
	rkNetActorData.m_dwEffectWeapon = c_rkNetUpdateActorData.m_dwEffectWeapon;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	rkNetActorData.m_bLanguage = c_rkNetUpdateActorData.m_bLanguage;
#endif
	rkNetActorData.m_sAlignment=c_rkNetUpdateActorData.m_sAlignment;
	rkNetActorData.m_byPKMode=c_rkNetUpdateActorData.m_byPKMode;

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	memcpy(rkNetActorData.m_dwSkillColor, c_rkNetUpdateActorData.m_dwSkillColor, sizeof(rkNetActorData.m_dwSkillColor));
#endif
}

void CNetworkActorManager::MoveActor(const SNetworkMoveActorData& c_rkNetMoveActorData)
{
	auto f=m_kNetActorDict.find(c_rkNetMoveActorData.m_dwVID);
	if (m_kNetActorDict.end()==f)
	{
#ifdef _DEBUG
		TraceError("CNetworkActorManager::MoveActor(dwVID=%d) - NOT EXIST VID", c_rkNetMoveActorData.m_dwVID);
#endif
		return;
	}

	SNetworkActorData& rkNetActorData=f->second;

	CInstanceBase* pkInstFind=__FindActor(rkNetActorData, c_rkNetMoveActorData.m_lPosX, c_rkNetMoveActorData.m_lPosY);
	if (pkInstFind)
	{
		TPixelPosition kPPosDst;
		kPPosDst.x=float(c_rkNetMoveActorData.m_lPosX);
		kPPosDst.y=float(c_rkNetMoveActorData.m_lPosY);
		kPPosDst.z=0.0f;

		pkInstFind->PushTCPState(c_rkNetMoveActorData.m_dwTime, kPPosDst,
			c_rkNetMoveActorData.m_fRot, c_rkNetMoveActorData.m_dwFunc, c_rkNetMoveActorData.m_dwArg);
	}

	rkNetActorData.SetDstPosition(c_rkNetMoveActorData.m_dwTime,
		c_rkNetMoveActorData.m_lPosX, c_rkNetMoveActorData.m_lPosY, c_rkNetMoveActorData.m_dwDuration);
	rkNetActorData.m_fRot=c_rkNetMoveActorData.m_fRot;
}

void CNetworkActorManager::SyncActor(uint32_t dwVID, int32_t lPosX, int32_t lPosY)
{
	std::map<uint32_t, SNetworkActorData>::iterator f=m_kNetActorDict.find(dwVID);
	if (m_kNetActorDict.end()==f)
	{
#ifdef _DEBUG
		TraceError("CNetworkActorManager::SyncActor(dwVID=%d) - NOT EXIST VID", dwVID);
#endif
		return;
	}

	SNetworkActorData& rkNetActorData=f->second;

	CInstanceBase* pkInstFind=__FindActor(rkNetActorData);
	if (pkInstFind)
	{
		pkInstFind->NEW_SyncPixelPosition(lPosX, lPosY);
	}

	rkNetActorData.SetPosition(lPosX, lPosY);
}

void CNetworkActorManager::SetActorOwner(uint32_t dwOwnerVID, uint32_t dwVictimVID)
{
	std::map<uint32_t, SNetworkActorData>::iterator f=m_kNetActorDict.find(dwVictimVID);
	if (m_kNetActorDict.end()==f)
	{
#ifdef _DEBUG
		TraceError("CNetworkActorManager::SetActorOwner(dwOwnerVID=%d, dwVictimVID=%d) - NOT EXIST VID", dwOwnerVID, dwVictimVID);
#endif
		return;
	}

	SNetworkActorData& rkNetActorData=f->second;
	rkNetActorData.m_dwOwnerVID=dwOwnerVID;

	CInstanceBase* pkInstFind=__FindActor(rkNetActorData);
	if (pkInstFind)
	{
		pkInstFind->NEW_SetOwner(rkNetActorData.m_dwOwnerVID);
	}


}
