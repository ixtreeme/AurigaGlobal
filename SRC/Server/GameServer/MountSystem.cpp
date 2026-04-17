#include "stdafx.h"
#include "config.h"
#include "utils.h"
#include "vector.h"
#include "char.h"
#include "sectree_manager.h"
#include "char_manager.h"
#include "mob_manager.h"
#include "MountSystem.h"
#include <common/VnumHelper.h>
#include "packet.h"
#include "item_manager.h"
#include "item.h"
#include "ecs/EventDispatcher.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/events.hpp"

EVENTINFO(mountsystem_event_info)
{
	CMountSystem* pMountSystem;
};

EVENTFUNC(mountsystem_update_event)
{
	mountsystem_event_info* info = dynamic_cast<mountsystem_event_info*>( event->info );
	if ( info == nullptr)
	{
		sys_err( "<mountsystem_update_event> <Factor> Null pointer" );
		return 0;
	}

	CMountSystem*	pMountSystem = info->pMountSystem;

	if (nullptr == pMountSystem)
		return 0;


	pMountSystem->Update(0);
	if (auto* owner = pMountSystem->GetOwner())
	{
		const entt::entity e = CVIDRegistry::Instance().Find(owner->GetVID());
		if (e != entt::null)
			g_dispatcher.trigger(ecs::EvMountSystemUpdate { e });
	}
	return PASSES_PER_SEC(1) / 4;
}

///////////////////////////////////////////////////////////////////////////////////////
//  CMountActor
///////////////////////////////////////////////////////////////////////////////////////

CMountActor::CMountActor(LPCHARACTER owner, uint32_t vnum)
{
	m_dwVnum = vnum;
	m_dwVID = 0;
	m_dwLastActionTime = 0;

	m_pkChar = nullptr;
	m_pkOwner = owner;

	m_originalMoveSpeed = 0;

	m_dwSummonItemVID = 0;
	m_dwSummonItemVnum = 0;
}

CMountActor::~CMountActor()
{
	this->Unsummon();
	m_pkOwner = nullptr;
}

void CMountActor::SetName()
{
	std::string petName = m_pkOwner->GetName();

	if (true == IsSummoned())
	{
		petName += "'s Mount";
		
		m_pkChar->SetName(petName);
	}

	m_name = petName;
}
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93


bool CMountActor::Mount(LPITEM mountItem)
{
#ifdef DISABLE_CORE_PULSE_RAZOR93
	if (!ch->IsNextMountPulse()) {
		ch->ChatPacket(CHAT_TYPE_INFO, "You can't do this that fast, please calm down a bit...");
		return false;
	}
#endif

	if (nullptr == m_pkOwner)
		return false;

	if (!mountItem)
		return false;

#ifdef BLOCK_RIDING_INSIDE_WAR
	if (m_pkOwner->GetWarMap()) {
#ifdef TEXTS_IMPROVEMENT
		m_pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 852, "");
#endif
		Unmount();
		return false;
	}
#endif

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

	uint32_t myMountVnum = m_pkOwner->GetMountVnum();

#ifdef ENABLE_COSTUME_MOUNT
	uint32_t dwMountSkinvnum = m_pkOwner->GetMountSkinVnum();
	if (dwMountSkinvnum > 0) {
		if (dwMountSkinvnum != myMountVnum) {
			Unmount();
		}
	}
	else if (myMountVnum != m_dwVnum)
#else
	if (myMountVnum != m_dwVnum)
#endif
	{
		Unmount();
	}
	else if (m_pkOwner->IsHorseRiding()) {
		m_pkOwner->StopRiding();
		m_pkOwner->HorseSummon(false);
	}

	uint32_t dwTime = mountItem->IsUnlimitedTimeUnique() ? 86400 : mountItem->GetSocket(0) - time(nullptr);

	//  Duplikacio elleni vedelem
	if (!m_pkOwner->FindAffect(AFFECT_MOUNT_BONUS))
	{
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
		{
			if (mountItem->GetProto()->aApplies[i].bType == APPLY_NONE)
				continue;

			m_pkOwner->AddAffect(
				AFFECT_MOUNT_BONUS,
				aApplyInfo[mountItem->GetProto()->aApplies[i].bType].bPointType,
				mountItem->GetProto()->aApplies[i].lValue,
				AFF_NONE,
				dwTime,
				0,
				false
			);
		}

		m_pkOwner->AddAffect(AFFECT_MOUNT_BONUS, POINT_MOV_SPEED, 50, AFF_NONE, dwTime, 0, false);
	}
	else
	{
		m_pkOwner->ChatPacket(CHAT_TYPE_INFO, "MountActor::Mount - Mount bonus already active, skipping duplicate apply.");
		
	}

#ifdef ENABLE_COSTUME_MOUNT
	if (dwMountSkinvnum > 0)
		m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, dwMountSkinvnum, AFF_NONE, dwTime, 0, true);
	else
		m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);

	return myMountVnum == m_dwVnum;
#else
	m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);
	return m_pkOwner->GetMountVnum() == m_dwVnum;
#endif
}
#else
bool CMountActor::Mount(LPITEM mountItem)
{
#ifdef DISABLE_CORE_PULSE_RAZOR93

	if (!ch->IsNextMountPulse()) {
		ch->ChatPacket(CHAT_TYPE_INFO, "You can't do this that fast, please calm down a bit...");
		return;
	}
#endif
	if (nullptr == m_pkOwner)
		return false;
	
	if(!mountItem)
		return false;

#ifdef BLOCK_RIDING_INSIDE_WAR
	if (m_pkOwner->GetWarMap()) {
#ifdef TEXTS_IMPROVEMENT
		m_pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 852, "");
#endif
		Unmount();
		return false;
	}
#endif

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();
	
	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

	uint32_t myMountVnum = m_pkOwner->GetMountVnum();
#ifdef ENABLE_COSTUME_MOUNT
	uint32_t dwMountSkinvnum = m_pkOwner->GetMountSkinVnum();
	if (dwMountSkinvnum > 0) {
		if (dwMountSkinvnum != myMountVnum) {
			Unmount();
		}
	} else if (myMountVnum != m_dwVnum)
#else
	if (myMountVnum != m_dwVnum)
#endif
	{
		Unmount();
	} else if (m_pkOwner->IsHorseRiding()) {
		m_pkOwner->StopRiding();
		m_pkOwner->HorseSummon(false);
	}

	uint32_t dwTime = mountItem->IsUnlimitedTimeUnique() == true ? 86400 : mountItem->GetSocket(0) - time(nullptr);
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		if (mountItem->GetProto()->aApplies[i].bType == APPLY_NONE)
			continue;

		m_pkOwner->AddAffect(AFFECT_MOUNT_BONUS, aApplyInfo[mountItem->GetProto()->aApplies[i].bType].bPointType, mountItem->GetProto()->aApplies[i].lValue, AFF_NONE, dwTime, 0, false);
	}


	m_pkOwner->AddAffect(AFFECT_MOUNT_BONUS, POINT_MOV_SPEED, 50, AFF_NONE, dwTime, 0, false);


#ifdef ENABLE_COSTUME_MOUNT
	if (dwMountSkinvnum > 0)
		m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, dwMountSkinvnum, AFF_NONE, dwTime, 0, true);
	else
		m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);

	return myMountVnum == m_dwVnum;
#else
	m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);
	return m_pkOwner->GetMountVnum() == m_dwVnum;
#endif
}


#endif

void CMountActor::Unmount()
{
	if (nullptr == m_pkOwner)
		return;
	
	if (!m_pkOwner->GetMountVnum())
		return;

	m_pkOwner->RemoveAffect(AFFECT_MOUNT);
	m_pkOwner->RemoveAffect(AFFECT_MOUNT_BONUS);



	m_pkOwner->MountVnum(0);
	
	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();
	
	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);


	m_pkOwner->PointChange(POINT_ST, 0);
	m_pkOwner->PointChange(POINT_DX, 0);
	m_pkOwner->PointChange(POINT_HT, 0);
	m_pkOwner->PointChange(POINT_IQ, 0);


}

void CMountActor::Unsummon()
{
	if (true == this->IsSummoned())
	{
		this->SetSummonItem(nullptr);
		
		if (nullptr != m_pkChar)
		{
			LPCHARACTER pkMount = CHARACTER_MANAGER::instance().Find(m_pkChar->GetVID());
			if (pkMount == m_pkChar)
				M2_DESTROY_CHARACTER(m_pkChar);
		}

		m_pkChar = nullptr;
		m_dwVID = 0;
	}



		m_pkOwner->MountVnum(0);

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

}

uint32_t CMountActor::Summon(LPITEM pSummonItem, bool bSpawnFar)
{
	int32_t x = m_pkOwner->GetX();
	int32_t y = m_pkOwner->GetY();
	int32_t z = m_pkOwner->GetZ();

	if (true == bSpawnFar)
	{
		x += (number(0, 1) * 2 - 1) * number(2000, 2500);
		y += (number(0, 1) * 2 - 1) * number(2000, 2500);
	}
	else
	{
		x += number(-100, 100);
		y += number(-100, 100);
	}
	
	if (nullptr != m_pkChar)
	{
		m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y);
		m_dwVID = m_pkChar->GetVID();

		return m_dwVID;
	}
	
#ifdef ENABLE_COSTUME_PET
	uint32_t dwMountSkinvnum = m_pkOwner->GetMountSkinVnum();
	if (dwMountSkinvnum > 0)
		m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(dwMountSkinvnum, m_pkOwner->GetMapIndex(), x, y, z, false, (int)(m_pkOwner->GetRotation()+180), false);
	else
		m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(m_dwVnum, m_pkOwner->GetMapIndex(), x, y, z, false, (int)(m_pkOwner->GetRotation()+180), false);
#else
	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(m_dwVnum, m_pkOwner->GetMapIndex(), x, y, z, false, (int)(m_pkOwner->GetRotation()+180), false);
#endif

	if (nullptr == m_pkChar)
	{
		sys_err("[CMountActor::Summon] Failed to summon the mount. (vnum: %d)", m_dwVnum);
		return 0;
	}

	m_pkChar->SetMount();

	m_pkChar->SetEmpire(m_pkOwner->GetEmpire());

	m_dwVID = m_pkChar->GetVID();

	this->SetName();

	this->SetSummonItem(pSummonItem);
	
	//m_pkOwner->ComputePoints();
	
	m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y, z);
	return m_dwVID;
}

bool CMountActor::UpdateFollowAI()
{
	if (nullptr == m_pkChar->m_pkMobData)
	{
		return false;
	}

	if (0 == m_originalMoveSpeed)
	{
		const CMob* mobData = CMobManager::Instance().Get(m_dwVnum);

		if (nullptr != mobData)
			m_originalMoveSpeed = mobData->m_table.sMovingSpeed;
	}
	float	START_FOLLOW_DISTANCE = 300.0f;
	//float	START_RUN_DISTANCE = 900.0f;

	float	RESPAWN_DISTANCE = 4500.f;
	int		APPROACH = 200;

	//bool bRun = false;

	uint32_t currentTime = get_dword_time();

	int32_t ownerX = m_pkOwner->GetX();		int32_t ownerY = m_pkOwner->GetY();
	int32_t charX = m_pkChar->GetX();			int32_t charY = m_pkChar->GetY();

	float fDist = DISTANCE_APPROX(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		float fOwnerRot = m_pkOwner->GetRotation() * 3.141592f / 180.f;
		float fx = -APPROACH * cos(fOwnerRot);
		float fy = -APPROACH * sin(fOwnerRot);
		if (m_pkChar->Show(m_pkOwner->GetMapIndex(), ownerX + fx, ownerY + fy))
		{
			return true;
		}
	}

	if (fDist >= START_FOLLOW_DISTANCE)
	{
		m_pkChar->SetNowWalking(false);

		Follow(APPROACH);

		m_pkChar->SetLastAttacked(currentTime);
		m_dwLastActionTime = currentTime;
	}
	else
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

	return true;
}

bool CMountActor::Update(uint32_t deltaTime)
{
	bool bResult = true;

	if (m_pkOwner->IsDead() || (IsSummoned() && m_pkChar->IsDead())
		|| nullptr == ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())
		|| ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())->GetOwner() != this->GetOwner()
		)
	{
		this->Unsummon();
		return true;
	}

	if (this->IsSummoned())
		bResult = bResult && this->UpdateFollowAI();

	return bResult;
}

bool CMountActor::Follow(float fMinDistance)
{
	if( !m_pkOwner || !m_pkChar)
		return false;

	int32_t fOwnerX = m_pkOwner->GetX();
	int32_t fOwnerY = m_pkOwner->GetY();

	int32_t fPetX = m_pkChar->GetX();
	int32_t fPetY = m_pkChar->GetY();

	float fDist = DISTANCE_SQRT(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= fMinDistance)
		return false;

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);

	if (!m_pkChar->Goto(static_cast<int>(static_cast<float>(fPetX) + fx + 0.5f), static_cast<int>(static_cast<float>(fPetY) + fy + 0.5f)) )
		return false;

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);
	
	return true;
}

void CMountActor::SetSummonItem(LPITEM pItem)
{
	if (nullptr == pItem)
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = pItem->GetVID();
	m_dwSummonItemVnum = pItem->GetVnum();
}

///////////////////////////////////////////////////////////////////////////////////////
//  CMountSystem
///////////////////////////////////////////////////////////////////////////////////////

CMountSystem::CMountSystem(LPCHARACTER owner)
{
	m_pkOwner = owner;
	m_dwUpdatePeriod = 400;

	m_dwLastUpdateTime = 0;
}

CMountSystem::~CMountSystem()
{
	Destroy();
}

void CMountSystem::Destroy()
{
	for (auto iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second.get();

		if (nullptr != mountActor)
		{
			mountActor->Unsummon();
		}
	}
	event_cancel(&m_pkMountSystemUpdateEvent);
	m_mountActorMap.clear();
}

bool CMountSystem::Update(uint32_t deltaTime)
{
	bool bResult = true;

	uint32_t currentTime = get_dword_time();

	if (m_dwUpdatePeriod > currentTime - m_dwLastUpdateTime)
		return true;

	std::vector <CMountActor*> v_garbageActor;

	for (auto iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second.get();

		if (nullptr != mountActor && mountActor->IsSummoned())
		{
			LPCHARACTER pMount = mountActor->GetCharacter();
			if (nullptr == pMount)
			{
				v_garbageActor.push_back(mountActor);
				continue;
			}

			if (CHARACTER_MANAGER::instance().Find(pMount->GetVID()) != pMount)
			{
				v_garbageActor.push_back(mountActor);
			}
			else
			{
				bResult = bResult && mountActor->Update(deltaTime);
			}
		}
	}
	for (auto it = v_garbageActor.begin(); it != v_garbageActor.end(); ++it)
		DeleteMount(*it);

	m_dwLastUpdateTime = currentTime;

	return bResult;
}

void CMountSystem::DeleteMount(uint32_t mobVnum)
{
	TMountActorMap::iterator iter = m_mountActorMap.find(mobVnum);

	if (m_mountActorMap.end() == iter)
	{
		sys_err("[CMountSystem::DeleteMount] Can't find mount on my list (VNUM: %u)", mobVnum);
		return;
	}

	CMountActor* mountActor = iter->second.get();
	if (nullptr == mountActor)
		sys_err("[CMountSystem::DeleteMount] Null Pointer (mountActor)");

	m_mountActorMap.erase(iter);
}

void CMountSystem::DeleteMount(CMountActor* mountActor)
{
	for (auto iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		if (iter->second.get() == mountActor)
		{
			m_mountActorMap.erase(iter);

			return;
		}
	}

	sys_err("[CMountSystem::DeleteMount] Can't find mountActor(0x%x) on my list(size: %u) ", mountActor, m_mountActorMap.size());
}

void CMountSystem::Unsummon(uint32_t vnum, bool bDeleteFromList)
{
	CMountActor* actor = this->GetByVnum(vnum);

	if (nullptr == actor)
	{
		sys_err("[CMountSystem::Unsummon(%d)] Null Pointer (actor)", vnum);
		return;
	}
	actor->Unsummon();

	if (true == bDeleteFromList)
		this->DeleteMount(actor);

	bool bActive = false;
	for (auto it = m_mountActorMap.begin(); it != m_mountActorMap.end(); ++it)
	{
		bActive |= it->second->IsSummoned();
	}
	if (false == bActive)
	{
		event_cancel(&m_pkMountSystemUpdateEvent);
		m_pkMountSystemUpdateEvent = nullptr;
	}
}


void CMountSystem::Summon(uint32_t mobVnum, LPITEM pSummonItem, bool bSpawnFar)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (nullptr == mountActor)
	{
		auto newActor = std::make_unique<CMountActor>(m_pkOwner, mobVnum);
		mountActor = newActor.get();
		m_mountActorMap.insert(std::make_pair(mobVnum, std::move(newActor)));
	}

	uint32_t mountVID = mountActor->Summon(pSummonItem, bSpawnFar);

	if (!mountVID)
		sys_err("[CMountSystem::Summon(%d)] Null Pointer (mountVID)", pSummonItem->GetID());

	if (nullptr == m_pkMountSystemUpdateEvent)
	{
		mountsystem_event_info* info = AllocEventInfo<mountsystem_event_info>();

		info->pMountSystem = this;

		m_pkMountSystemUpdateEvent = event_create(mountsystem_update_event, info, PASSES_PER_SEC(1) / 4);
	}

	if (pSummonItem->GetSocket(2) == 1) {
		Mount(mobVnum, pSummonItem);
	}

	//return mountActor;
}

void CMountSystem::Mount(uint32_t mobVnum, LPITEM mountItem)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		sys_err("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}
	
	if(!mountItem)
		return;

	this->Unsummon(mobVnum, false);
	mountActor->Mount(mountItem);
	mountItem->SetSocket(2, 1);
}

void CMountSystem::Unmount(uint32_t mobVnum)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		sys_err("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}

	mountActor->Unmount();

	if(LPITEM pSummonItem = m_pkOwner->GetWear(WEAR_COSTUME_MOUNT))
	{
		pSummonItem->SetSocket(2, 0);
		this->Summon(mobVnum, pSummonItem, false);
	}
}

CMountActor* CMountSystem::GetByVID(uint32_t vid) const
{
	CMountActor* mountActor = nullptr;

	bool bFound = false;

	for (auto iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		mountActor = iter->second.get();

		if (nullptr == mountActor)
		{
			sys_err("[CMountSystem::GetByVID(%d)] Null Pointer (mountActor)", vid);
			continue;
		}

		bFound = mountActor->GetVID() == vid;

		if (true == bFound)
			break;
	}

	return bFound ? mountActor : nullptr;
}

CMountActor* CMountSystem::GetByVnum(uint32_t vnum) const
{
	CMountActor* mountActor = nullptr;

	auto iter = m_mountActorMap.find(vnum);

	if (m_mountActorMap.end() != iter)
		mountActor = iter->second.get();

	return mountActor;
}

size_t CMountSystem::CountSummoned() const
{
	size_t count = 0;

	for (auto iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second.get();

		if (nullptr != mountActor)
		{
			if (mountActor->IsSummoned())
				++count;
		}
	}

	return count;
}

#ifdef ENABLE_COSTUME_MOUNT
void CMountActor::UpdateMountSkin() {
	LPITEM pSummonItem = ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID());
	if (pSummonItem != nullptr){
		Unsummon();
		Summon(pSummonItem, false);
	}
}

void CMountSystem::UpdateMountSkin() {
	for (TMountActorMap::const_iterator iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter) {
		CMountActor* mountActor = iter->second.get();
		if (mountActor != nullptr)
		{
			if (mountActor->IsSummoned())
				mountActor->UpdateMountSkin();
		}
	}
}
#endif
