#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "config.h"
#include "utils.h"
#include "vector.h"
#include "char_interface.hpp"
#include "sectree_manager.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "mob_manager.h"
#include "MountSystem.h"
#include <common/VnumHelper.h>
#include "packet.h"
#include "item_manager.h"
#include "item.h"
#include "ecs/EventDispatcher.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/components/pet_mount_components.hpp"
#include "ecs/events.hpp"

namespace
{
entt::entity FindSummonItemByVID(uint32_t vid)
{
	return ItemSystem::FindItemByVID(vid);
}

bool IsSummonItemOwnedBy(uint32_t vid, LPCHARACTER owner)
{
	const entt::entity item = FindSummonItemByVID(vid);
	return item != entt::null && ItemSystem::GetItemOwner(item) == ((owner) ? (owner)->GetEntityHandle() : entt::null);
}

LPITEM LegacyItemFromEntity(entt::entity item)
{
	if (item == entt::null)
		return nullptr;

	const uint32_t id = ItemSystem::GetItemID(item);
	return id != 0 ? ITEM_MANAGER::instance().Find(id) : nullptr;
}

bool SnapFollowerToOwner(LPCHARACTER follower, LPCHARACTER owner, int32_t x, int32_t y, int32_t z = 0)
{
	const entt::entity followerEntity = follower ? follower->GetEntityHandle() : entt::null;
	if (!follower || !owner)
		return false;

	if (follower->Sync(x, y))
	{
		ecs::MovementSystem::Stop(followerEntity);
		follower->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);
		return true;
	}

	return ecs::MovementSystem::Show(followerEntity, ecs::PlayerRuntime::GetMapIndex(((owner) ? (owner)->GetEntityHandle() : entt::null)), x, y, z);
}
}

EVENTINFO(mountsystem_event_info)
{
	CMountSystem* pMountSystem;
};

EVENTFUNC(mountsystem_update_event)
{
	mountsystem_event_info* info = dynamic_cast<mountsystem_event_info*>( event->info );
	if ( info == nullptr)
	{
		LOG_ERROR("<mountsystem_update_event> <Factor> Null pointer");
		return 0;
	}

	CMountSystem*	pMountSystem = info->pMountSystem;

	if (nullptr == pMountSystem)
		return 0;


	pMountSystem->Update(0);
	if (auto* owner = pMountSystem->GetOwner())
	{
		const entt::entity e = ((owner) ? (owner)->GetEntityHandle() : entt::null);
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
	std::string petName = ecs::PlayerRuntime::GetName(((m_pkOwner) ? (m_pkOwner)->GetEntityHandle() : entt::null)).data();

	if (true == IsSummoned())
	{
		petName += "'s Mount";

		m_pkChar->SetName(petName);
	}

	m_name = petName;
}
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93


bool CMountActor::Mount(entt::entity mountItemEntity)
{
	const entt::entity owner = m_pkOwner ? m_pkOwner->GetEntityHandle() : entt::null;
#ifdef DISABLE_CORE_PULSE_RAZOR93
	if (!MountSystem::GetMountStateRef(ch->GetEntityHandle())) {
		ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "You can't do this that fast, please calm down a bit...");
		return false;
	}
#endif

	if (nullptr == m_pkOwner)
		return false;

	LPITEM mountItem = LegacyItemFromEntity(mountItemEntity);
	if (!mountItem)
		return false;

#ifdef BLOCK_RIDING_INSIDE_WAR
	if (m_pkOwner->GetWarMap()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 852, "");
#endif
		Unmount();
		return false;
	}
#endif

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

	uint32_t myMountVnum = MountSystem::GetMountVnum(owner);

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

	uint32_t dwTime = ItemSystem::IsUnlimitedTimeUnique(mountItem->GetEntityHandle()) ? 86400 : ItemSystem::GetItemSocket((mountItem ? mountItem->GetEntityHandle() : entt::null), 0) - time(nullptr);
	const TItemTable* mountProto = ItemSystem::GetItemProto((mountItem ? mountItem->GetEntityHandle() : entt::null));
	if (!mountProto)
		return false;

	//  Duplikacio elleni vedelem
	if (!AffectSystem::FindAffect(owner, AFFECT_MOUNT_BONUS))
	{
		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
		{
			if (mountProto->aApplies[i].bType == APPLY_NONE)
				continue;

			AffectSystem::AddAffect(owner,
				AFFECT_MOUNT_BONUS,
				aApplyInfo[mountProto->aApplies[i].bType].bPointType,
				mountProto->aApplies[i].lValue,
				AFF_NONE,
				dwTime,
				0,
				false
			);
		}

		AffectSystem::AddAffect(owner, AFFECT_MOUNT_BONUS, POINT_MOV_SPEED, 50, AFF_NONE, dwTime, 0, false);
	}
	else
	{
		ecs::ChatSystem::Send(owner, CHAT_TYPE_INFO, "MountActor::Mount - Mount bonus already active, skipping duplicate apply.");

	}

#ifdef ENABLE_COSTUME_MOUNT
	if (dwMountSkinvnum > 0)
		AffectSystem::AddAffect(owner, AFFECT_MOUNT, POINT_MOUNT, dwMountSkinvnum, AFF_NONE, dwTime, 0, true);
	else
		AffectSystem::AddAffect(owner, AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);

	return myMountVnum == m_dwVnum;
#else
	AffectSystem::AddAffect(owner, AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);
	return MountSystem::GetMountVnum(owner) == m_dwVnum;
#endif
}
#else
bool CMountActor::Mount(entt::entity mountItemEntity)
{
	const entt::entity owner = m_pkOwner ? m_pkOwner->GetEntityHandle() : entt::null;
#ifdef DISABLE_CORE_PULSE_RAZOR93

	if (!MountSystem::GetMountStateRef(ch->GetEntityHandle())) {
		ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "You can't do this that fast, please calm down a bit...");
		return;
	}
#endif
	if (nullptr == m_pkOwner)
		return false;

	LPITEM mountItem = LegacyItemFromEntity(mountItemEntity);
	if(!mountItem)
		return false;

#ifdef BLOCK_RIDING_INSIDE_WAR
	if (m_pkOwner->GetWarMap()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 852, "");
#endif
		Unmount();
		return false;
	}
#endif

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

	uint32_t myMountVnum = MountSystem::GetMountVnum(owner);
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

	uint32_t dwTime = ItemSystem::IsUnlimitedTimeUnique(mountItem->GetEntityHandle()) == true ? 86400 : ItemSystem::GetItemSocket((mountItem ? mountItem->GetEntityHandle() : entt::null), 0) - time(nullptr);
	const TItemTable* mountProto = ItemSystem::GetItemProto((mountItem ? mountItem->GetEntityHandle() : entt::null));
	if (!mountProto)
		return false;

	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		if (mountProto->aApplies[i].bType == APPLY_NONE)
			continue;

		AffectSystem::AddAffect(owner, AFFECT_MOUNT_BONUS, aApplyInfo[mountProto->aApplies[i].bType].bPointType, mountProto->aApplies[i].lValue, AFF_NONE, dwTime, 0, false);
	}


	AffectSystem::AddAffect(owner, AFFECT_MOUNT_BONUS, POINT_MOV_SPEED, 50, AFF_NONE, dwTime, 0, false);


#ifdef ENABLE_COSTUME_MOUNT
	if (dwMountSkinvnum > 0)
		AffectSystem::AddAffect(owner, AFFECT_MOUNT, POINT_MOUNT, dwMountSkinvnum, AFF_NONE, dwTime, 0, true);
	else
		AffectSystem::AddAffect(owner, AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);

	return myMountVnum == m_dwVnum;
#else
	AffectSystem::AddAffect(owner, AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, dwTime, 0, true);
	return MountSystem::GetMountVnum(owner) == m_dwVnum;
#endif
}


#endif

void CMountActor::Unmount()
{
	const entt::entity owner = m_pkOwner ? m_pkOwner->GetEntityHandle() : entt::null;
	if (nullptr == m_pkOwner)
		return;

	AffectSystem::RemoveAffect(owner, AFFECT_MOUNT);
	AffectSystem::RemoveAffect(owner, AFFECT_MOUNT_BONUS);



	MountSystem::SetMountVnum(owner, 0);

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);


	ecs::PointSystem::Change(owner, POINT_ST, 0);
	ecs::PointSystem::Change(owner, POINT_DX, 0);
	ecs::PointSystem::Change(owner, POINT_HT, 0);
	ecs::PointSystem::Change(owner, POINT_IQ, 0);


}

void CMountActor::Unsummon()
{
	if (true == this->IsSummoned())
	{
		this->SetSummonItem(entt::null);

		if (nullptr != m_pkChar)
		{
			const entt::entity mountEntity = ((m_pkChar) ? (m_pkChar)->GetEntityHandle() : entt::null);
			if (mountEntity != entt::null && g_registry.valid(mountEntity))
				M2_DESTROY_CHARACTER(m_pkChar);
		}

		m_pkChar = nullptr;
		m_dwVID = 0;
	}



		MountSystem::SetMountVnum(((m_pkOwner) ? (m_pkOwner)->GetEntityHandle() : entt::null), 0);

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

}

uint32_t CMountActor::Summon(entt::entity pSummonItem, bool bSpawnFar)
{
	const entt::entity owner = m_pkOwner ? m_pkOwner->GetEntityHandle() : entt::null;
	int32_t x = ecs::PlayerRuntime::GetX(owner);
	int32_t y = ecs::PlayerRuntime::GetY(owner);
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
		SnapFollowerToOwner(m_pkChar, m_pkOwner, x, y, z);
		m_dwVID = ecs::PlayerRuntime::GetPacketVID(((m_pkChar) ? (m_pkChar)->GetEntityHandle() : entt::null));

		return m_dwVID;
	}

#ifdef ENABLE_COSTUME_PET
	uint32_t dwMountSkinvnum = m_pkOwner->GetMountSkinVnum();
	if (dwMountSkinvnum > 0)
		m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(dwMountSkinvnum, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z, false, (int)(m_pkOwner->GetRotation()+180), false);
	else
		m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(m_dwVnum, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z, false, (int)(m_pkOwner->GetRotation()+180), false);
#else
	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(m_dwVnum, ecs::PlayerRuntime::GetMapIndex(owner), x, y, z, false, (int)(m_pkOwner->GetRotation()+180), false);
#endif

	if (nullptr == m_pkChar)
	{
		LOG_ERROR("[CMountActor::Summon] Failed to summon the mount. (vnum: {})", m_dwVnum);
		return 0;
	}

	m_pkChar->SetMount();

	m_pkChar->SetEmpire(ecs::PlayerRuntime::GetEmpire(owner));

	m_dwVID = ecs::PlayerRuntime::GetPacketVID(((m_pkChar) ? (m_pkChar)->GetEntityHandle() : entt::null));

	this->SetName();

	this->SetSummonItem(pSummonItem);

	//m_pkOwner->ComputePoints();

	ecs::MovementSystem::Show(((m_pkChar) ? (m_pkChar)->GetEntityHandle() : entt::null), ecs::PlayerRuntime::GetMapIndex(owner), x, y, z);
	return m_dwVID;
}

bool CMountActor::UpdateFollowAI()
{
	const entt::entity owner = m_pkOwner ? m_pkOwner->GetEntityHandle() : entt::null;
	const entt::entity charEntity = m_pkChar ? m_pkChar->GetEntityHandle() : entt::null;
	if (nullptr == m_pkChar->GetMobData())
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

	int32_t ownerX = ecs::PlayerRuntime::GetX(owner);		int32_t ownerY = ecs::PlayerRuntime::GetY(owner);
	int32_t charX = ecs::PlayerRuntime::GetX(charEntity);			int32_t charY = ecs::PlayerRuntime::GetY(charEntity);

	float fDist = DISTANCE_APPROX(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		float fOwnerRot = m_pkOwner->GetRotation() * 3.141592f / 180.f;
		float fx = -APPROACH * cos(fOwnerRot);
		float fy = -APPROACH * sin(fOwnerRot);
		if (SnapFollowerToOwner(m_pkChar, m_pkOwner, ownerX + fx, ownerY + fy, m_pkOwner->GetZ()))
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

	if (CombatSystem::IsDead(((m_pkOwner) ? (m_pkOwner)->GetEntityHandle() : entt::null)) || (IsSummoned() && CombatSystem::IsDead(((m_pkChar) ? (m_pkChar)->GetEntityHandle() : entt::null)))
		|| !IsSummonItemOwnedBy(this->GetSummonItemVID(), this->GetOwner())
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
	const entt::entity owner = m_pkOwner ? m_pkOwner->GetEntityHandle() : entt::null;
	const entt::entity charEntity = m_pkChar ? m_pkChar->GetEntityHandle() : entt::null;
	if( !m_pkOwner || !m_pkChar)
		return false;

	int32_t fOwnerX = ecs::PlayerRuntime::GetX(owner);
	int32_t fOwnerY = ecs::PlayerRuntime::GetY(owner);

	int32_t fPetX = ecs::PlayerRuntime::GetX(charEntity);
	int32_t fPetY = ecs::PlayerRuntime::GetY(charEntity);

	float fDist = DISTANCE_SQRT(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= fMinDistance)
		return false;

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);

	if (!ecs::MovementSystem::Goto(charEntity, static_cast<int>(static_cast<float>(fPetX) + fx + 0.5f), static_cast<int>(static_cast<float>(fPetY) + fy + 0.5f)) )
		return false;

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void CMountActor::SetSummonItem(entt::entity pItemEntity)
{
	LPITEM pItem = LegacyItemFromEntity(pItemEntity);
	if (nullptr == pItem)
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = pItem->GetVID();
	m_dwSummonItemVnum = ItemSystem::GetItemVnum((pItem ? pItem->GetEntityHandle() : entt::null));

	const entt::entity owner = ((m_pkOwner) ? (m_pkOwner)->GetEntityHandle() : entt::null);
	if (owner != entt::null && g_registry.valid(owner)) {
		auto& mount = g_registry.emplace_or_replace<ecs::MountComponent>(owner);
		mount.owner = owner;
		mount.itemID = ItemSystem::GetItemID((pItem ? pItem->GetEntityHandle() : entt::null));
		mount.itemVID = pItem->GetVID();
		mount.itemVnum = ItemSystem::GetItemVnum((pItem ? pItem->GetEntityHandle() : entt::null));
		mount.level = 0;
		mount.state = IsSummoned() ? 1u : 0u;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			mount.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket((pItem ? pItem->GetEntityHandle() : entt::null), i));
	}
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

			const entt::entity mountEntity = ((pMount) ? (pMount)->GetEntityHandle() : entt::null);
			if (mountEntity == entt::null || !g_registry.valid(mountEntity))
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
		LOG_ERROR("[CMountSystem::DeleteMount] Can't find mount on my list (VNUM: {})", mobVnum);
		return;
	}

	CMountActor* mountActor = iter->second.get();
	if (nullptr == mountActor)
		LOG_ERROR("[CMountSystem::DeleteMount] Null Pointer (mountActor)");

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

	LOG_ERROR("[CMountSystem::DeleteMount] Can't find mountActor({}) on my list(size: {}) ", static_cast<const void*>(mountActor), m_mountActorMap.size());
}

void CMountSystem::Unsummon(uint32_t vnum, bool bDeleteFromList)
{
	CMountActor* actor = this->GetByVnum(vnum);

	if (nullptr == actor)
	{
		LOG_ERROR("[CMountSystem::Unsummon({})] Null Pointer (actor)", vnum);
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


void CMountSystem::Summon(uint32_t mobVnum, entt::entity pSummonItem, bool bSpawnFar)
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
		LOG_ERROR("[CMountSystem::Summon({})] Null Pointer (mountVID)", ItemSystem::GetItemID(pSummonItem));

	if (nullptr == m_pkMountSystemUpdateEvent)
	{
		mountsystem_event_info* info = AllocEventInfo<mountsystem_event_info>();

		info->pMountSystem = this;

		m_pkMountSystemUpdateEvent = event_create(mountsystem_update_event, info, PASSES_PER_SEC(1) / 4);
	}

	if (ItemSystem::GetItemSocket(pSummonItem, 2) == 1) {
		Mount(mobVnum, pSummonItem);
	}

	//return mountActor;
}

void CMountSystem::Mount(uint32_t mobVnum, entt::entity mountItem)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		LOG_ERROR("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}

	if(mountItem == entt::null)
		return;

	this->Unsummon(mobVnum, false);
	mountActor->Mount(mountItem);
	ItemSystem::SetItemSocket(mountItem, 2, 1);
}

void CMountSystem::Unmount(uint32_t mobVnum)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		LOG_ERROR("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}

	mountActor->Unmount();

	const entt::entity pSummonItem = ItemSystem::GetWearItem(
		((m_pkOwner) ? (m_pkOwner)->GetEntityHandle() : entt::null), WEAR_COSTUME_MOUNT);
	if (ItemSystem::IsValidItem(pSummonItem))
	{
		ItemSystem::SetItemSocket(pSummonItem, 2, 0);
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
			LOG_ERROR("[CMountSystem::GetByVID({})] Null Pointer (mountActor)", vid);
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
	LPITEM pSummonItem = LegacyItemFromEntity(FindSummonItemByVID(this->GetSummonItemVID()));
	if (pSummonItem != nullptr){
		Unsummon();
		Summon((pSummonItem ? pSummonItem->GetEntityHandle() : entt::null), false);
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
