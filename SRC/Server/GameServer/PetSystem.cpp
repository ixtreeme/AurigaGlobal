#include "stdafx.h"
#include "config.h"
#include "utils.h"
#include "vector.h"
#include "char_interface.hpp"
#include "sectree_manager.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "mob_manager.h"
#include "PetSystem.h"
#include <common/VnumHelper.h>
#include "packet.h"
#include "item_manager.h"
#include "item.h"
#include "desc.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/movement_components.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/components/pet_mount_components.hpp"

namespace
{
entt::entity FindSummonItemByVID(uint32_t vid)
{
	return ItemSystem::FindItemByVID(vid);
}

bool IsSummonItemOwnedBy(uint32_t vid, LPCHARACTER owner)
{
	const entt::entity item = FindSummonItemByVID(vid);
	return item != entt::null && ItemSystem::GetItemOwner(item) == AIHelpers::EcsOf(owner);
}

LPITEM LegacyItemFromEntity(entt::entity item)
{
	const uint32_t id = ItemSystem::GetItemID(item);
	return id != 0 ? ITEM_MANAGER::instance().Find(id) : nullptr;
}

bool SnapFollowerToOwner(LPCHARACTER follower, LPCHARACTER owner, int32_t x, int32_t y, int32_t z = 0)
{
	if (!follower || !owner)
		return false;

	if (follower->Sync(x, y))
	{
		follower->Stop();
		follower->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);
		return true;
	}

	return follower->Show(owner->GetMapIndex(), x, y, z);
}
}


EVENTINFO(petsystem_event_info)
{
	CPetSystem* pPetSystem;
};

// PetSystem을 update 해주는 event.
// PetSystem은 CHRACTER_MANAGER에서 기존 FSM으로 update 해주는 기존 chracters와 달리,
// Owner의 STATE를 update 할 때 _UpdateFollowAI 함수로 update 해준다.
// 그런데 owner의 state를 update를 CHRACTER_MANAGER에서 해주기 때문에,
// petsystem을 update하다가 pet을 unsummon하는 부분에서 문제가 생겼다.
// (CHRACTER_MANAGER에서 update 하면 chracter destroy가 pending되어, CPetSystem에서는 dangling 포인터를 가지고 있게 된다.)
// 따라서 PetSystem만 업데이트 해주는 event를 발생시킴.
EVENTFUNC(petsystem_update_event)
{
	petsystem_event_info* info = dynamic_cast<petsystem_event_info*>( event->info );
	if ( info == nullptr)
	{
		sys_err( "check_speedhack_event> <Factor> Null pointer" );
		return 0;
	}

	CPetSystem*	pPetSystem = info->pPetSystem;

	if (nullptr == pPetSystem)
		return 0;


	pPetSystem->Update(0);
	// 0.25초마다 갱신.
	return PASSES_PER_SEC(1) / 4;
}

/// NOTE: 1캐릭터가 몇개의 펫을 가질 수 있는지 제한... 캐릭터마다 개수를 다르게 할거라면 변수로 넣등가... 음..
/// 가질 수 있는 개수와 동시에 소환할 수 있는 개수가 틀릴 수 있는데 이런건 기획 없으니 일단 무시
const float PET_COUNT_LIMIT = 3;

///////////////////////////////////////////////////////////////////////////////////////
//  CPetActor
///////////////////////////////////////////////////////////////////////////////////////

CPetActor::CPetActor(LPCHARACTER owner, uint32_t vnum, uint32_t options)
{
#ifdef ENABLE_COSTUME_PET
	m_dwBaseVnum = vnum;
#endif
	m_dwVnum = vnum;
	m_dwVID = 0;
	m_dwOptions = options;
	m_dwLastActionTime = 0;

	m_pkChar = nullptr;
	m_pkOwner = owner;

	m_originalMoveSpeed = 0;

	m_dwSummonItemVID = 0;
	m_dwSummonItemVnum = 0;
}

CPetActor::~CPetActor()
{
	this->Unsummon();

	m_pkOwner = nullptr;
}

void CPetActor::SetName(const char* name)
{
	std::string petName = m_pkOwner->GetName();
	petName += "'s Pet";

	if (true == IsSummoned())
		m_pkChar->SetName(petName);

	m_name = petName;
}

bool CPetActor::Mount()
{
	if (nullptr == m_pkOwner)
		return false;

	if (true == HasOption(EPetOption_Mountable))
		m_pkOwner->MountVnum(m_dwVnum);

	return m_pkOwner->GetMountVnum() == m_dwVnum;;
}

void CPetActor::Unmount()
{
	if (nullptr == m_pkOwner)
		return;

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();
}

#ifdef ENABLE_COSTUME_PET
void CPetActor::UpdatePetSkin() {
	LPITEM pSummonItem = LegacyItemFromEntity(FindSummonItemByVID(this->GetSummonItemVID()));
	if (pSummonItem != nullptr){
		Unsummon();
		Summon("Noname", pSummonItem, false);

	}
}

void CPetSystem::UpdatePetSkin() {
	for (TPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter) {
		CPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned())
				petActor->UpdatePetSkin();
		}
	}
}
#endif

void CPetActor::Unsummon()
{
	if (true == this->IsSummoned())
	{
		LPITEM pSummonItem = LegacyItemFromEntity(FindSummonItemByVID(this->GetSummonItemVID()));
		if (pSummonItem) {
			ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, pSummonItem), 2, false);
			ItemSystem::UnlockItem(EntityFactory::CreateItemEntity(g_registry, pSummonItem));
		}
		
		// 버프 삭제
		this->ClearBuff();
		this->SetSummonItem(nullptr);
		if (nullptr != m_pkOwner)
			m_pkOwner->ComputePoints();

		if (nullptr != m_pkChar)
			M2_DESTROY_CHARACTER(m_pkChar);

		m_pkChar = nullptr;
		m_dwVID = 0;
	}
}

uint32_t CPetActor::Summon(const char* petName, LPITEM pSummonItem, bool bSpawnFar)
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
		SnapFollowerToOwner(m_pkChar, m_pkOwner, x, y, z);
		m_dwVID = m_pkChar->GetLegacyVID();

		return m_dwVID;
	}
#ifdef ENABLE_COSTUME_PET
	uint32_t vnumToSpawn = m_dwBaseVnum ? m_dwBaseVnum : m_dwVnum;
	if (m_pkOwner) {
		uint32_t skin = m_pkOwner->GetPetSkinVnum();
		if (skin != 0)
			vnumToSpawn = skin;
	}
#else
	uint32_t vnumToSpawn = m_dwBaseVnum ? m_dwBaseVnum : m_dwVnum;
#endif
#ifdef ENABLE_COSTUME_PET
	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(
		vnumToSpawn,
		m_pkOwner->GetMapIndex(),
		x, y, z,
		false, (int)(m_pkOwner->GetRotation() + 180), false);
#else
	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(
				m_dwVnum,
				m_pkOwner->GetMapIndex(),
				x, y, z,
				false, (int)(m_pkOwner->GetRotation()+180), false);
#endif
	if (nullptr == m_pkChar)
	{
		sys_err("[CPetSystem::Summon] Failed to summon the pet. (vnum: %d)", m_dwVnum);
		return 0;
	}
//	#ifdef ENABLE_COSTUME_PET
//	uint32_t dwPetSkinvnum = m_pkOwner->GetPetSkinVnum();
//	m_dwVnum = dwPetSkinvnum != 0 ? dwPetSkinvnum : m_dwVnum;
//#endif
	m_pkChar->SetPet();

//	m_pkOwner->DetailLog();
//	m_pkChar->DetailLog();

	//펫의 국가를 주인의 국가로 설정함.
	m_pkChar->SetEmpire(m_pkOwner->GetEmpire());

	m_dwVID = m_pkChar->GetLegacyVID();

	this->SetName("");

	// SetSummonItem(pSummonItem)를 부른 후에 ComputePoints를 부르면 버프 적용됨.
	this->SetSummonItem(pSummonItem);
	m_pkOwner->ComputePoints();
	m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y, z);
	ItemSystem::SetItemSocket(EntityFactory::CreateItemEntity(g_registry, pSummonItem), 2, true);
	ItemSystem::LockItem(EntityFactory::CreateItemEntity(g_registry, pSummonItem));
#ifdef ENABLE_RECALL
	const CAffect* pAffect = m_pkOwner->FindAffect(AFFECT_RECALL1);
	if (pAffect) {
		m_pkOwner->RemoveAffect(const_cast<CAffect*>(pAffect));
	}
	
	m_pkOwner->AddAffect(AFFECT_RECALL1, APPLY_NONE, 0, ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, pSummonItem)), INFINITE_AFFECT_DURATION, 0, true, false);
#endif
#ifdef ENABLE_COSTUME_PET
	uint32_t dwPetSkinvnum = m_pkOwner->GetPetSkinVnum();
	m_dwVnum = dwPetSkinvnum != 0 ? dwPetSkinvnum : m_dwVnum;
#endif
	return m_dwVID;
}

bool CPetActor::_UpdatAloneActionAI(float fMinDist, float fMaxDist)
{
	float fDist = number(fMinDist, fMaxDist);
	float r = (float)number (0, 359);
	float dest_x = GetOwner()->GetX() + fDist * cos(r);
	float dest_y = GetOwner()->GetY() + fDist * sin(r);

	//m_pkChar->SetRotation(number(0, 359));        // 방향은 랜덤으로 설정

	//GetDeltaByDegree(m_pkChar->GetRotation(), fDist, &fx, &fy);

	// 느슨한 못감 속성 체크; 최종 위치와 중간 위치가 갈수없다면 가지 않는다.
	//if (!(SECTREE_MANAGER::instance().IsMovablePosition(m_pkChar->GetMapIndex(), m_pkChar->GetX() + (int) fx, m_pkChar->GetY() + (int) fy)
	//			&& SECTREE_MANAGER::instance().IsMovablePosition(m_pkChar->GetMapIndex(), m_pkChar->GetX() + (int) fx/2, m_pkChar->GetY() + (int) fy/2)))
	//	return true;

	m_pkChar->SetNowWalking(true);

	//if (m_pkChar->Goto(m_pkChar->GetX() + (int) fx, m_pkChar->GetY() + (int) fy))
	//	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	const entt::entity petEntity = AIHelpers::EcsOf(m_pkChar);
	const bool isMoving = petEntity != entt::null
		&& g_registry.valid(petEntity)
		&& g_registry.all_of<ecs::MovementDestination>(petEntity);
	if (!isMoving && m_pkChar->Goto(dest_x, dest_y))
	{
		if (petEntity != entt::null && g_registry.valid(petEntity))
			g_registry.emplace_or_replace<ecs::MovementDestination>(petEntity, static_cast<int32_t>(dest_x), static_cast<int32_t>(dest_y));
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	}

	m_dwLastActionTime = get_dword_time();

	return true;
}

// StateHorse함수 그냥 C&P -_-;
bool CPetActor::_UpdateFollowAI()
{
	if (nullptr == m_pkChar->GetMobData())
	{
		//sys_err("[CPetActor::_UpdateFollowAI] m_pkChar->m_pkMobData is NULL");
		return false;
	}

	// NOTE: 캐릭터(펫)의 원래 이동 속도를 알아야 하는데, 해당 값(m_pkChar->m_pkMobData->m_table.sMovingSpeed)을 직접적으로 접근해서 알아낼 수도 있지만
	// m_pkChar->m_pkMobData 값이 invalid한 경우가 자주 발생함. 현재 시간관계상 원인은 다음에 파악하고 일단은 m_pkChar->m_pkMobData 값을 아예 사용하지 않도록 함.
	// 여기서 매번 검사하는 이유는 최초 초기화 할 때 정상 값을 제대로 못얻어오는 경우도 있음.. -_-;; ㅠㅠㅠㅠㅠㅠㅠㅠㅠ
	if (0 == m_originalMoveSpeed)
	{
		const CMob* mobData = CMobManager::Instance().Get(m_dwVnum);

		if (nullptr != mobData)
			m_originalMoveSpeed = mobData->m_table.sMovingSpeed;
	}
	float	START_FOLLOW_DISTANCE = 300.0f;		// 이 거리 이상 떨어지면 쫓아가기 시작함
	float	START_RUN_DISTANCE = 900.0f;		// 이 거리 이상 떨어지면 뛰어서 쫓아감.

	float	RESPAWN_DISTANCE = 4500.f;			// 이 거리 이상 멀어지면 주인 옆으로 소환함.
	int		APPROACH = 200;						// 접근 거리

	//bool bDoMoveAlone = true;					// 캐릭터와 가까이 있을 때 혼자 여기저기 움직일건지 여부 -_-;
	bool bRun = false;							// 뛰어야 하나?

	uint32_t currentTime = get_dword_time();

	int32_t ownerX = m_pkOwner->GetX();		int32_t ownerY = m_pkOwner->GetY();
	int32_t charX = m_pkChar->GetX();			int32_t charY = m_pkChar->GetY();

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
		if( fDist >= START_RUN_DISTANCE)
		{
			bRun = true;
		}

		m_pkChar->SetNowWalking(!bRun);		// NOTE: 함수 이름보고 멈추는건줄 알았는데 SetNowWalking(false) 하면 뛰는거임.. -_-;

		Follow(APPROACH);

		m_pkChar->SetLastAttacked(currentTime);
		m_dwLastActionTime = currentTime;
	}
	//else
	//{
	//	if (fabs(m_pkChar->GetRotation() - GetDegreeFromPositionXY(charX, charY, ownerX, ownerX)) > 10.f || fabs(m_pkChar->GetRotation() - GetDegreeFromPositionXY(charX, charY, ownerX, ownerX)) < 350.f)
	//	{
	//		m_pkChar->Follow(m_pkOwner, APPROACH);
	//		m_pkChar->SetLastAttacked(currentTime);
	//		m_dwLastActionTime = currentTime;
	//	}
	//}
	// Follow 중이지만 주인과 일정 거리 이내로 가까워졌다면 멈춤
	else
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	//else if (currentTime - m_dwLastActionTime > number(5000, 12000))
	//{
	//	this->_UpdatAloneActionAI(START_FOLLOW_DISTANCE / 2, START_FOLLOW_DISTANCE);
	//}

	return true;
}

bool CPetActor::Update(uint32_t deltaTime)
{
	bool bResult = true;

	// 펫 주인이 죽었거나, 소환된 펫의 상태가 이상하다면 펫을 없앰. (NOTE: 가끔가다 이런 저런 이유로 소환된 펫이 DEAD 상태에 빠지는 경우가 있음-_-;)
	// 펫을 소환한 아이템이 없거나, 내가 가진 상태가 아니라면 펫을 없앰.
	//if (m_pkOwner->IsDead() || (IsSummoned() && m_pkChar->IsDead())
	if ((IsSummoned() && m_pkChar->IsDead())
		|| !IsSummonItemOwnedBy(this->GetSummonItemVID(), this->GetOwner())
		)
	{
		this->Unsummon();
		return true;
	}

	if (this->IsSummoned() && HasOption(EPetOption_Followable))
		bResult = bResult && this->_UpdateFollowAI();

	return bResult;
}

//NOTE : 주의!!! MinDistance를 크게 잡으면 그 변위만큼의 변화동안은 follow하지 않는다,
bool CPetActor::Follow(float fMinDistance)
{
	// 가려는 위치를 바라봐야 한다.
	if( !m_pkOwner || !m_pkChar)
		return false;

	float fOwnerX = m_pkOwner->GetX();
	float fOwnerY = m_pkOwner->GetY();

	float fPetX = m_pkChar->GetX();
	float fPetY = m_pkChar->GetY();

	float fDist = DISTANCE_SQRT(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= fMinDistance)
		return false;

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);

	if (!m_pkChar->Goto((int)(fPetX+fx+0.5f), (int)(fPetY+fy+0.5f)) )
		return false;

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void CPetActor::SetSummonItem (LPITEM pItem)
{
	if (nullptr == pItem)
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = pItem->GetVID();
	m_dwSummonItemVnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pItem));

	const entt::entity owner = AIHelpers::EcsOf(m_pkOwner);
	if (owner != entt::null && g_registry.valid(owner)) {
		auto& pet = g_registry.emplace_or_replace<ecs::PetComponent>(owner);
		pet.owner = owner;
		pet.itemID = ItemSystem::GetItemID(EntityFactory::CreateItemEntity(g_registry, pItem));
		pet.itemVID = pItem->GetVID();
		pet.itemVnum = ItemSystem::GetItemVnum(EntityFactory::CreateItemEntity(g_registry, pItem));
		pet.level = 0;
		pet.state = IsSummoned() ? 1u : 0u;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			pet.sockets[i] = static_cast<int32_t>(ItemSystem::GetItemSocket(EntityFactory::CreateItemEntity(g_registry, pItem), i));
	}
}

bool __PetCheckBuff(const CPetActor* pPetActor)
{
	bool bMustHaveBuff = true;
	switch (pPetActor->GetVnum())
	{
		case 34004:
		case 34009:
			if (nullptr == pPetActor->GetOwner()->GetDungeon())
				bMustHaveBuff = false;
		default:
			break;
	}
	return bMustHaveBuff;
}

void CPetActor::GiveBuff()
{
	// 파황 펫 버프는 던전에서만 발생함.
	if (!__PetCheckBuff(this))
		return;
	LPITEM item = LegacyItemFromEntity(FindSummonItemByVID(m_dwSummonItemVID));
	if (nullptr != item)
		item->ModifyPoints(true);
	return ;
}

void CPetActor::ClearBuff()
{
	if (nullptr == m_pkOwner)
		return ;
	TItemTable* item_proto = ITEM_MANAGER::instance().GetTable(m_dwSummonItemVnum);
	if (nullptr == item_proto)
		return;
	if (!__PetCheckBuff(this)) // @fixme129
		return;
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++)
	{
		if (item_proto->aApplies[i].bType == APPLY_NONE)
			continue;
		m_pkOwner->ApplyPoint(item_proto->aApplies[i].bType, -item_proto->aApplies[i].lValue);
	}

	return ;
}

///////////////////////////////////////////////////////////////////////////////////////
//  CPetSystem
///////////////////////////////////////////////////////////////////////////////////////

CPetSystem::CPetSystem(LPCHARACTER owner)
{
//	assert(0 != owner && "[CPetSystem::CPetSystem] Invalid owner");

	m_pkOwner = owner;
	m_dwUpdatePeriod = 400;

	m_dwLastUpdateTime = 0;
}

CPetSystem::~CPetSystem()
{
	Destroy();
}

void CPetSystem::Destroy()
{
	for (auto iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			delete petActor;
		}
	}
	event_cancel(&m_pkPetSystemUpdateEvent);
	m_petActorMap.clear();
}

/// 펫 시스템 업데이트. 등록된 펫들의 AI 처리 등을 함.
bool CPetSystem::Update(uint32_t deltaTime)
{
	bool bResult = true;

	uint32_t currentTime = get_dword_time();

	// CHARACTER_MANAGER에서 캐릭터류 Update할 때 매개변수로 주는 (Pulse라고 되어있는)값이 이전 프레임과의 시간차이인줄 알았는데
	// 전혀 다른 값이라서-_-; 여기에 입력으로 들어오는 deltaTime은 의미가 없음ㅠㅠ

	if (m_dwUpdatePeriod > currentTime - m_dwLastUpdateTime)
		return true;

	std::vector <CPetActor*> v_garbageActor;

	for (auto iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor && petActor->IsSummoned())
		{
			LPCHARACTER pPet = petActor->GetCharacter();

			if (nullptr == CHARACTER_MANAGER::instance().Find(pPet->GetLegacyVID()))
			{
				v_garbageActor.push_back(petActor);
			}
			else
			{
				bResult = bResult && petActor->Update(deltaTime);
			}
		}
	}
	for (auto it = v_garbageActor.begin(); it != v_garbageActor.end(); ++it)
		DeletePet(*it);

	m_dwLastUpdateTime = currentTime;

	return bResult;
}

/// 관리 목록에서 펫을 지움
void CPetSystem::DeletePet(uint32_t mobVnum)
{
	const auto iter = m_petActorMap.find(mobVnum);

	if (m_petActorMap.end() == iter)
	{
		sys_err("[CPetSystem::DeletePet] Can't find pet on my list (VNUM: %d)", mobVnum);
		return;
	}

	CPetActor* petActor = iter->second;

	if (nullptr == petActor)
		sys_err("[CPetSystem::DeletePet] Null Pointer (petActor)");
	else
		delete petActor;

	m_petActorMap.erase(iter);
}

/// 관리 목록에서 펫을 지움
void CPetSystem::DeletePet(CPetActor* petActor)
{
	for (auto iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		if (iter->second == petActor)
		{
			delete petActor;
			m_petActorMap.erase(iter);

			return;
		}
	}

	sys_err("[CPetSystem::DeletePet] Can't find petActor(0x%x) on my list(size: %d) ", petActor, m_petActorMap.size());
}

void CPetSystem::Unsummon(uint32_t vnum, bool bDeleteFromList)
{
	CPetActor* actor = this->GetByVnum(vnum);

	if (nullptr == actor)
	{
		sys_err("[CPetSystem::GetByVnum(%d)] Null Pointer (petActor)", vnum);
		return;
	}
	actor->Unsummon();

	if (true == bDeleteFromList)
		this->DeletePet(actor);

	bool bActive = false;
	for (auto it = m_petActorMap.begin(); it != m_petActorMap.end(); ++it)
	{
		bActive |= it->second->IsSummoned();
	}
	if (false == bActive)
	{
		event_cancel(&m_pkPetSystemUpdateEvent);
		m_pkPetSystemUpdateEvent = nullptr;
	}
}

void CPetSystem::UnsummonAll()
{
	for (auto iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;
		if (petActor != nullptr)
		{
			if (petActor->IsSummoned()) {
				petActor->Unsummon();
				break;
			}
		}
	}
	
	if (m_pkPetSystemUpdateEvent) {
		event_cancel(&m_pkPetSystemUpdateEvent);
		m_pkPetSystemUpdateEvent = nullptr;
	}
}

CPetActor* CPetSystem::Summon(uint32_t mobVnum, LPITEM pSummonItem, const char* petName, bool bSpawnFar, uint32_t options)
{
	CPetActor* petActor = this->GetByVnum(mobVnum);

	// 등록된 펫이 아니라면 새로 생성 후 관리 목록에 등록함.
	if (nullptr == petActor)
	{
		petActor = M2_NEW CPetActor(m_pkOwner, mobVnum, options);
		m_petActorMap.insert(std::make_pair(mobVnum, petActor));
	}

#ifdef ENABLE_NEWSTUFF
	uint32_t petVID = petActor->Summon(petName, pSummonItem, bSpawnFar);
	if (!petVID)
		sys_err("[CPetSystem::Summon(%d)] Null Pointer (petVID)", pSummonItem);
#endif

	if (nullptr == m_pkPetSystemUpdateEvent)
	{
		petsystem_event_info* info = AllocEventInfo<petsystem_event_info>();

		info->pPetSystem = this;

		m_pkPetSystemUpdateEvent = event_create(petsystem_update_event, info, PASSES_PER_SEC(1) / 4);	// 0.25초
	}

	return petActor;
}


CPetActor* CPetSystem::GetByVID(uint32_t vid) const
{
	CPetActor* petActor = nullptr;

	bool bFound = false;

	for (auto iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		petActor = iter->second;

		if (nullptr == petActor)
		{
			sys_err("[CPetSystem::GetByVID(%d)] Null Pointer (petActor)", vid);
			continue;
		}

		bFound = petActor->GetVID() == vid;

		if (true == bFound)
			break;
	}

	return bFound ? petActor : nullptr;
}

/// 등록 된 펫 중에서 주어진 몹 VNUM을 가진 액터를 반환하는 함수.
CPetActor* CPetSystem::GetByVnum(uint32_t vnum) const
{
	CPetActor* petActor = nullptr;

	auto iter = m_petActorMap.find(vnum);

	if (m_petActorMap.end() != iter)
		petActor = iter->second;

	return petActor;
}

size_t CPetSystem::CountSummoned() const
{
	size_t count = 0;

	for (TPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->IsSummoned())
				++count;
		}
	}

	return count;
}

void CPetSystem::RefreshBuff()
{
	for (TPetActorMap::const_iterator iter = m_petActorMap.begin(); iter != m_petActorMap.end(); ++iter)
	{
		CPetActor* petActor = iter->second;

		if (nullptr != petActor)
		{
			if (petActor->IsSummoned())
			{
				petActor->GiveBuff();
			}
		}
	}
}

