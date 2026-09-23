#include "stdafx.h"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char_interface.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "char_manager.h"
#include "../ecs/PIDRegistry.hpp"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/components/social_components.hpp"
#include "sectree_manager.h"
#include "desc_client.h"
#include "p2p.h"
#include "wedding.h"
#include "config.h"
#include "utils.h"
#include "questmanager.h"
#ifdef ENABLE_NEW_USE_POTION
#include "item.h"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "unique_item.h"
#endif

extern bool g_bShutdown;

namespace marriage
{
	const int MAX_LOVE_GRADE = 4;
	const int MAX_MARRIAGE_UNIQUE_ITEM = 6;

	struct TMarriageItemBonusByGrade
	{
		uint32_t dwVnum;
		int value[MAX_LOVE_GRADE];
	} g_ItemBonus[MAX_MARRIAGE_UNIQUE_ITEM] = {
		{ 71069,	{ 4,	5,	6,	8,  } },
		{ 71070,	{ 10,	12,	15,	20, } },
		{ 71071,	{ 4,	5,	6,	8,  } },
		{ 71072,	{ -4,	-5,	-6,	-8, } },
		{ 71073,	{ 20,	25,	30,	40, } },
		{ 71074,	{ 12,	16,	20,	30, } },

	};

	const int MARRIAGE_POINT_PER_DAY = 1;
	const int MARRIAGE_POINT_PER_DAY_FAST = 2;
	using namespace std;

	void SendLoverInfo(entt::entity ch, const string& lover_name, int love_point)
	{
		TPacketGCLoverInfo p;

		p.header = HEADER_GC_LOVER_INFO;
		strlcpy(p.name, lover_name.c_str(), sizeof(p.name));
		p.love_point = love_point;
		ecs::PlayerRuntime::GetDesc(ch)->Packet(&p, sizeof(p));
	}

	namespace
	{
		ecs::CoupleState* Couple(entt::entity couple)
		{
			return couple != entt::null && g_registry.valid(couple)
				? g_registry.try_get<ecs::CoupleState>(couple) : nullptr;
		}
	}

	namespace MarriageSystem
	{
		const ecs::CoupleState* State(entt::entity couple)
		{
			return Couple(couple);
		}

		uint32_t GetOther(entt::entity couple, uint32_t PID)
		{
			const auto* state = Couple(couple);
			if (!state)
				return 0;

			if (state->pid1 == PID)
				return state->pid2;

			if (state->pid2 == PID)
				return state->pid1;

			return 0;
		}

		// The pair is online while both partners are still characters.
		bool IsOnline(entt::entity couple)
		{
			const auto* state = Couple(couple);
			return state && ecs::IsCharacter(state->character1) && ecs::IsCharacter(state->character2);
		}

		int GetMarriageGrade(entt::entity couple)
		{
			int point = MINMAX(50, GetMarriagePoint(couple), 100);
			if (point < 65)
				return 0;
			else if (point < 80)
				return 1;
			else if (point < 100)
				return 2;
			return 3;
		}

		int GetMarriagePoint(entt::entity couple)
		{
			const auto* state = Couple(couple);
			if (!state)
				return 0;

			if (test_server)
			{
				int value = quest::CQuestManager::instance().GetEventFlag("lovepoint");
				if (value)
					return MINMAX(0, value, 100);
			}

			int point_per_day = MARRIAGE_POINT_PER_DAY;
			int max_limit = 30;
			const auto first = CPIDRegistry::Instance().Find(state->pid1);
			const auto second = CPIDRegistry::Instance().Find(state->pid2);
			if (ecs::PlayerRuntime::IsPC(first) && ecs::PlayerRuntime::IsPC(second))
			{
				if (ecs::PlayerRuntime::GetPremiumRemainSeconds(first, PREMIUM_MARRIAGE_FAST) > 0 ||
					ecs::PlayerRuntime::GetPremiumRemainSeconds(second, PREMIUM_MARRIAGE_FAST) > 0)
				{
					point_per_day = MARRIAGE_POINT_PER_DAY_FAST;
					max_limit = 40;
				}
			}

			int days = (get_global_time() - state->marryTime);
			if (test_server)
				days /= 60;
			else
				days /= 86400;

			return MIN(50 + MIN(days * point_per_day, max_limit) + MIN(state->lovePoint / 1000000, max_limit), 100);
		}

		bool IsNear(entt::entity couple)
		{
			const auto* state = Couple(couple);
			if (!state || !state->married)
				return false;
			if (!IsOnline(couple))
				return false;

			return ecs::PlayerRuntime::GetMapIndex(state->character1) == ecs::PlayerRuntime::GetMapIndex(state->character2);
		}

		int GetBonus(entt::entity couple, uint32_t dwItemVnum, bool bShare, entt::entity me)
		{
			const auto* state = Couple(couple);
			if (!state)
				return 0;

			const entt::entity ch1Entity = CPIDRegistry::Instance().Find(state->pid1);
			const entt::entity ch2Entity = CPIDRegistry::Instance().Find(state->pid2);
			if (!state->married)
				return 0;


			int iFindedBonusIndex=0;
			{
				for (iFindedBonusIndex = 0; iFindedBonusIndex < MAX_MARRIAGE_UNIQUE_ITEM; ++iFindedBonusIndex)
				{
					if (g_ItemBonus[iFindedBonusIndex].dwVnum == dwItemVnum)
						break;
				}

				if (iFindedBonusIndex == MAX_MARRIAGE_UNIQUE_ITEM)
					return 0;
			}

#ifdef ENABLE_NEW_USE_POTION
			uint32_t affetIdx;
			switch (dwItemVnum) {
				case UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS:
					affetIdx = AFFECT_NEW_POTION15;
					break;
				case UNIQUE_ITEM_MARRIAGE_EXP_BONUS:
					affetIdx = AFFECT_NEW_POTION16;
					break;
				case UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS:
					affetIdx = AFFECT_NEW_POTION17;
					break;
				case UNIQUE_ITEM_MARRIAGE_TRANSFER_DAMAGE:
					affetIdx = AFFECT_NEW_POTION18;
					break;
				case UNIQUE_ITEM_MARRIAGE_ATTACK_BONUS:
					affetIdx = AFFECT_NEW_POTION19;
					break;
				case UNIQUE_ITEM_MARRIAGE_DEFENSE_BONUS:
					affetIdx = AFFECT_NEW_POTION20;
					break;
				default:
					affetIdx = 0;
					break;
			}
#endif

			if (bShare)
			{
				int count = 0;
				if (ecs::PlayerRuntime::IsPC(ch1Entity) &&
#ifdef ENABLE_NEW_USE_POTION
				affetIdx != 0 && AffectSystem::FindAffect(ch1Entity, affetIdx) != nullptr
#else
				ItemSystem::IsEquipUniqueItem(ch1Entity, dwItemVnum)
#endif
				)
					count ++;
				if (ecs::PlayerRuntime::IsPC(ch2Entity) &&
#ifdef ENABLE_NEW_USE_POTION
				affetIdx != 0 && AffectSystem::FindAffect(ch2Entity, affetIdx) != nullptr
#else
				ItemSystem::IsEquipUniqueItem(ch2Entity, dwItemVnum)
#endif
				)
					count ++;

				const TMarriageItemBonusByGrade& rkBonus = g_ItemBonus[iFindedBonusIndex];

				if (count>=1)
					return rkBonus.value[GetMarriageGrade(couple)];
				return 0;
			}
			else
			{
				int count = 0;
				if (me != ch1Entity && ecs::PlayerRuntime::IsPC(ch1Entity) &&
#ifdef ENABLE_NEW_USE_POTION
				affetIdx != 0 && AffectSystem::FindAffect(ch1Entity, affetIdx) != nullptr
#else
				ItemSystem::IsEquipUniqueItem(ch1Entity, dwItemVnum)
#endif
				)
					count ++;
				if (me != ch2Entity && ecs::PlayerRuntime::IsPC(ch2Entity) &&
#ifdef ENABLE_NEW_USE_POTION
				affetIdx != 0 && AffectSystem::FindAffect(ch2Entity, affetIdx) != nullptr
#else
				ItemSystem::IsEquipUniqueItem(ch2Entity, dwItemVnum)
#endif
				)
					count ++;

				const TMarriageItemBonusByGrade& rkBonus = g_ItemBonus[iFindedBonusIndex];

				if (count>=1)
					return rkBonus.value[GetMarriageGrade(couple)];
				return 0;
			}
		}

		void Save(entt::entity couple)
		{
			auto* state = Couple(couple);
			if (!state)
				return;

			LOG_INFO("MarriageSystem::Save() - RequestUpdate.needsSave={}", state->needsSave);
			if (state->needsSave)
			{
				CManager::instance().RequestUpdate(state->pid1, state->pid2, state->lovePoint, state->married);
				state->needsSave = false;
			}
		}

		void SetMarried(entt::entity couple)
		{
			auto* state = Couple(couple);
			if (!state)
				return;

			state->married = true;
			state->needsSave = true;
			Save(couple);

			if (IsOnline(couple))
			{
				const entt::entity character1 = state->character1;
				const entt::entity character2 = state->character2;
				const std::string name1 = state->name1;
				const std::string name2 = state->name2;

				SendLoverInfo(character1, name2, GetMarriagePoint(couple));
				SendLoverInfo(character2, name1, GetMarriagePoint(couple));

				ecs::ChatSystem::Send(character1, CHAT_TYPE_COMMAND, "lover_login");
				ecs::ChatSystem::Send(character2, CHAT_TYPE_COMMAND, "lover_login");
			}
		}

		void Update(entt::entity couple, uint32_t point)
		{
			if (!IsOnline(couple))
				return;

			auto* state = Couple(couple);
			if (point > 0 && state->married)
			{
				state->needsSave = true;
				// @fixme126
				uint64_t llActualPoints = static_cast<uint64_t>(state->lovePoint) + point;
				state->lovePoint = MIN( llActualPoints, 2000000000 );

				if (test_server)
				{
					const int lovePoint = state->lovePoint;
					const uint32_t pid1 = state->pid1;
					const uint32_t pid2 = state->pid2;

					entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(pid1);
					if (ecs::IsCharacter(ch))
						ecs::ChatSystem::Send(ch, CHAT_TYPE_PARTY, "lovepoint bykill %.3g total %d", lovePoint / 1000000., GetMarriagePoint(couple));
					ch = CHARACTER_MANAGER::instance().FindEntityByPID(pid2);
					if (ecs::IsCharacter(ch))
						ecs::ChatSystem::Send(ch, CHAT_TYPE_PARTY, "lovepoint bykill %.3g total %d", lovePoint / 1000000., GetMarriagePoint(couple));
				}
			}
		}

		void WarpToWeddingMap(entt::entity couple, uint32_t dwPID)
		{
			const auto* state = Couple(couple);
			if (!state || !state->weddingMapIndex)
				return;

			const uint32_t mapIndex = *state->weddingMapIndex;
			const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);
			if (ecs::IsCharacter(ch))
			{
				PIXEL_POSITION pos;
				if (!SECTREE_MANAGER::instance().GetRecallPositionByEmpire(mapIndex/10000, 0, pos))
				{
					LOG_ERROR("cannot get warp position");
					return;
				}
				ecs::MovementSystem::SaveExitLocation(ch);
				ecs::MovementSystem::WarpSet(ch, pos.x, pos.y, mapIndex);
			}
		}

		void RequestEndWedding(entt::entity couple)
		{
			const auto* state = Couple(couple);
			if (!state || !state->weddingMapIndex)
				return;
			CManager::instance().RequestEndWedding(state->pid1, state->pid2);
		}
	}

	namespace
	{
		void StopNearCheckEvent(entt::entity couple)
		{
			auto* state = Couple(couple);
			if (!state)
				return;

			state->lastLovePoint = 0;
			state->lastNear = false;
			event_cancel(&state->nearCheckEvent);
		}

		void NearCheck(entt::entity couple)
		{
			auto* state = Couple(couple);
			if (!state || !state->married)
				return;

			if (!MarriageSystem::IsOnline(couple))
			{
				StopNearCheckEvent(couple);
				return;
			}
			LOG_TRACE("NearCheck {} {} {} {} {} {}", state->pid1, state->pid2, MarriageSystem::IsNear(couple), state->lastNear, state->lastLovePoint, MarriageSystem::GetMarriagePoint(couple));

			if (MarriageSystem::IsNear(couple) && !state->lastNear)
			{
				state->lastNear = true;
				ecs::ChatSystem::Send(state->character1, CHAT_TYPE_COMMAND, "lover_near");
				ecs::ChatSystem::Send(state->character2, CHAT_TYPE_COMMAND, "lover_near");
			}
			else if (!MarriageSystem::IsNear(couple) && state->lastNear)
			{
				state->lastNear = false;
				ecs::ChatSystem::Send(state->character1, CHAT_TYPE_COMMAND, "lover_far");
				ecs::ChatSystem::Send(state->character2, CHAT_TYPE_COMMAND, "lover_far");
			}

			if (state->lastLovePoint != MarriageSystem::GetMarriagePoint(couple))
			{
				state->lastLovePoint = MarriageSystem::GetMarriagePoint(couple);
				TPacketGCLovePointUpdate p;
				p.header = HEADER_GC_LOVE_POINT_UPDATE;
				p.love_point = state->lastLovePoint;

				ecs::PlayerRuntime::GetDesc(state->character1)->Packet(&p, sizeof(p));
				ecs::PlayerRuntime::GetDesc(state->character2)->Packet(&p, sizeof(p));
			}
		}

		// The timer holds the couple entity, not a pointer, so a couple removed
		// without its timer being cancelled reads as gone and ends the timer.
		EVENTINFO(near_check_event_info)
		{
			entt::entity couple;

			near_check_event_info()
			: couple( entt::null )
			{
			}
		};

		EVENTFUNC(near_check_event)
		{
			near_check_event_info* info = dynamic_cast<near_check_event_info*>( event->info );

			if ( info == nullptr)
			{
				LOG_ERROR("near_check_event> <Factor> Null pointer");
				return 0;
			}

			if (!Couple(info->couple))
				return 0;

			NearCheck(info->couple);
			return PASSES_PER_SEC(5);
		}

		void StartNearCheckEvent(entt::entity couple)
		{
			StopNearCheckEvent(couple);

			auto* state = Couple(couple);
			if (!state)
				return;

			near_check_event_info* info = AllocEventInfo<near_check_event_info>();
			info->couple = couple;
			state->nearCheckEvent = event_create(near_check_event, info, 1);
		}

		entt::entity CreateCouple(uint32_t pid1, uint32_t pid2, int lovePoint, time_t marryTime, const char* name1, const char* name2)
		{
			const entt::entity couple = g_registry.create();
			auto& state = g_registry.emplace<ecs::CoupleState>(couple);
			state.pid1 = pid1;
			state.pid2 = pid2;
			state.lovePoint = lovePoint;
			state.marryTime = marryTime;
			state.name1 = name1;
			state.name2 = name2;
			return couple;
		}

		// What deleting the TMarriage did: the timer goes, the couple hears
		// about the divorce while both are online, then the state goes.
		void DestroyCouple(entt::entity couple)
		{
			StopNearCheckEvent(couple);
			if (MarriageSystem::IsOnline(couple))
			{
				const auto* state = Couple(couple);
				ecs::ChatSystem::Send(state->character1, CHAT_TYPE_COMMAND, "lover_divorce");
				ecs::ChatSystem::Send(state->character2, CHAT_TYPE_COMMAND, "lover_divorce");
			}

			if (couple != entt::null && g_registry.valid(couple))
				g_registry.destroy(couple);
		}

		void LoginPartner(entt::entity couple, entt::entity ch)
		{
			auto* state = Couple(couple);
			if (!state)
				return;

			if ((ecs::PlayerRuntime::GetPlayerID(ch)) == state->pid1)
			{
				state->character1 = ch;
				if (state->married)
					SendLoverInfo(state->character1, state->name2, MarriageSystem::GetMarriagePoint(couple));
			}
			else if ((ecs::PlayerRuntime::GetPlayerID(ch)) == state->pid2)
			{
				state->character2 = ch;
				if (state->married)
					SendLoverInfo(state->character2, state->name1, MarriageSystem::GetMarriagePoint(couple));
			}

			if (MarriageSystem::IsOnline(couple))
			{
				ecs::SocialSystem::SetMarryPartner(state->character1, state->character2);
				ecs::SocialSystem::SetMarryPartner(state->character2, state->character1);

				StartNearCheckEvent(couple);
			}

			if (state->married)
			{
				LPDESC d1, d2;
				CCI * pkCCI;

				d1 = ecs::PlayerRuntime::GetDesc(state->character1);

				if (!d1)
				{
					pkCCI = P2P_MANAGER::instance().FindByPID(state->pid1);

					if (pkCCI)
					{
						d1 = pkCCI->pkDesc;
						d1->SetRelay(pkCCI->szName);
					}
				}

				d2 = ecs::PlayerRuntime::GetDesc(state->character2);

				if (!d2)
				{
					pkCCI = P2P_MANAGER::instance().FindByPID(state->pid2);

					if (pkCCI)
					{
						d2 = pkCCI->pkDesc;
						d2->SetRelay(pkCCI->szName);
					}
				}

				if (d1 && d2)
				{
					d1->ChatPacket(CHAT_TYPE_COMMAND, "lover_login");
					d2->ChatPacket(CHAT_TYPE_COMMAND, "lover_login");
					LOG_INFO("lover_login {} {}", state->pid1, state->pid2);
				}
			}
		}

		void LogoutPartner(entt::entity couple, uint32_t pid)
		{
			auto* state = Couple(couple);
			if (!state)
				return;

			if (pid == state->pid1)
				state->character1 = entt::null;
			else if (pid == state->pid2)
				state->character2 = entt::null;

			if (ecs::IsCharacter(state->character1) || ecs::IsCharacter(state->character2))
			{
				MarriageSystem::Save(couple);

				if (ecs::IsCharacter(state->character1))
					ecs::SocialSystem::SetMarryPartner(state->character1, entt::null);

				if (ecs::IsCharacter(state->character2))
					ecs::SocialSystem::SetMarryPartner(state->character2, entt::null);

				StopNearCheckEvent(couple);
			}

			if (state->married)
			{
				LPDESC d1, d2;
				CCI * pkCCI;

				d1 = ecs::PlayerRuntime::GetDesc(state->character1);

				if (!d1)
				{
					pkCCI = P2P_MANAGER::instance().FindByPID(state->pid1);

					if (pkCCI)
					{
						d1 = pkCCI->pkDesc;
						d1->SetRelay(pkCCI->szName);
					}
				}

				if (d1 && !g_bShutdown) {
					d1->ChatPacket(CHAT_TYPE_COMMAND, "lover_logout");
				}

				d2 = ecs::PlayerRuntime::GetDesc(state->character2);

				if (!d2)
				{
					pkCCI = P2P_MANAGER::instance().FindByPID(state->pid2);

					if (pkCCI)
					{
						d2 = pkCCI->pkDesc;
						d2->SetRelay(pkCCI->szName);
					}
				}

				if (d2 && !g_bShutdown) {
					d2->ChatPacket(CHAT_TYPE_COMMAND, "lover_logout");
				}
			}
		}
	}

	CManager::CManager()
	{
	}

	CManager::~CManager()
	{
	}

	bool CManager::IsMarriageUniqueItem(uint32_t dwItemVnum)
	{
		for (int i = 0; i < MAX_MARRIAGE_UNIQUE_ITEM; i++)
		{
			if (g_ItemBonus[i].dwVnum == dwItemVnum)
				return true;
		}
		return false;
	}

	bool CManager::IsMarried(uint32_t dwPlayerID)
	{
		const auto* state = Couple(Get(dwPlayerID));
		return state && state->married;
	}

	bool CManager::IsEngaged(uint32_t dwPlayerID)
	{
		const auto* state = Couple(Get(dwPlayerID));
		return state && !state->married;
	}

	bool CManager::IsEngagedOrMarried(uint32_t dwPlayerID)
	{
		return Get(dwPlayerID) != entt::null;
	}

	bool CManager::Initialize()
	{
		return true;
	}

	void CManager::Destroy()
	{
	}

	void Align(uint32_t& dwPID1, uint32_t& dwPID2)
	{
		if (dwPID1 > dwPID2)
			std::swap(dwPID1, dwPID2);
	}

	entt::entity CManager::Get(uint32_t dwPlayerID)
	{
		const auto it = m_MarriageByPID.find(dwPlayerID);
		if (it == m_MarriageByPID.end())
			return entt::null;

		// An entry whose couple is gone (a registry reset) reads as no couple.
		if (!Couple(it->second))
		{
			m_MarriageByPID.erase(it);
			return entt::null;
		}

		return it->second;
	}

	void CManager::RequestAdd(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2)
	{
		if (dwPID1 > dwPID2)
		{
			std::swap(dwPID1, dwPID2);
			std::swap(szName1, szName2);
		}

		TPacketMarriageAdd p;

		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		strlcpy(p.szName1, szName1, sizeof(p.szName1));
		strlcpy(p.szName2, szName2, sizeof(p.szName2));
		db_clientdesc->DBPacket(HEADER_GD_MARRIAGE_ADD, 0, &p, sizeof(p));
	}

	void CManager::Add(uint32_t dwPID1, uint32_t dwPID2, time_t tMarryTime, const char* szName1, const char* szName2)
	{
		if (IsEngagedOrMarried(dwPID1) || IsEngagedOrMarried(dwPID2))
		{
			LOG_ERROR("cannot marry already married character. {} - {}", dwPID1, dwPID2);
			return;
		}

		if (dwPID1 > dwPID2)
		{
			std::swap(dwPID1, dwPID2);
			std::swap(szName1, szName2);
		}

		const entt::entity couple = CreateCouple(dwPID1, dwPID2, 0, tMarryTime, szName1, szName2);
		m_MarriageByPID.insert_or_assign(dwPID1, couple);
		m_MarriageByPID.insert_or_assign(dwPID2, couple);
		{
			const entt::entity A = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID1);
			const entt::entity B = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID2);

			if (ecs::IsCharacter(A) && ecs::IsCharacter(B))
			{
				TPacketWeddingRequest p;
				p.dwPID1 = dwPID1;
				p.dwPID2 = dwPID2;
				db_clientdesc->DBPacket(HEADER_GD_WEDDING_REQUEST, 0, &p, sizeof(p));
			}
		}
	}

	void CManager::RequestUpdate(uint32_t dwPID1, uint32_t dwPID2, int iUpdatePoint, uint8_t byMarried)
	{
		Align(dwPID1, dwPID2);

		TPacketMarriageUpdate p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		p.iLovePoint = iUpdatePoint;
		p.byMarried = byMarried;
		db_clientdesc->DBPacket(HEADER_GD_MARRIAGE_UPDATE, 0, &p, sizeof(p));
	}

	void CManager::Update(uint32_t dwPID1, uint32_t dwPID2, int32_t lTotalPoint, uint8_t byMarried)
	{
		const entt::entity couple = Get(dwPID1);

		if (couple == entt::null || MarriageSystem::GetOther(couple, dwPID1) != dwPID2)
		{
			LOG_ERROR("not under marriage : {} {}", dwPID1, dwPID2);
			return;
		}

		auto* state = Couple(couple);
		state->lovePoint = lTotalPoint;
		state->married = byMarried;
	}

	void CManager::RequestRemove(uint32_t dwPID1, uint32_t dwPID2)
	{
		Align(dwPID1, dwPID2);

		TPacketMarriageRemove p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		db_clientdesc->DBPacket(HEADER_GD_MARRIAGE_REMOVE, 0, &p, sizeof(p));
	}

	void CManager::Remove(uint32_t dwPID1, uint32_t dwPID2)
	{
		const entt::entity couple = Get(dwPID1);
		if (couple == entt::null || MarriageSystem::GetOther(couple, dwPID1) != dwPID2)
		{
			LOG_ERROR("not under marriage : {} {}", dwPID1, dwPID2);
			return;
		}

#ifdef ENABLE_NEW_USE_POTION
		uint32_t dwAffect = 0;
		const entt::entity p1Entity = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID1);

		CAffect* pAffect = nullptr;
		entt::entity pkItem = entt::null;
		if (ecs::IsCharacter(p1Entity)) {
			for (int i = 0; i < 6; i++) {
				dwAffect = AFFECT_NEW_POTION24 + i;
				pAffect = AffectSystem::FindAffect(p1Entity, dwAffect);
				if (pAffect != nullptr) {
					pkItem = ItemSystem::FindItemByID(p1Entity, pAffect->dwFlag);
					if (ItemSystem::IsValidItem(pkItem)) {
						ItemSystem::UnlockItem(pkItem);
						ItemSystem::SetItemSocket(pkItem, 1, 0);
					}

					AffectSystem::RemoveAffect(p1Entity, dwAffect);
				}
			}
		}

		const entt::entity p2Entity = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID2);

		if (ecs::IsCharacter(p2Entity)) {
			for (int i = 0; i < 6; i++) {
				dwAffect = AFFECT_NEW_POTION24 + i;
				pAffect = AffectSystem::FindAffect(p2Entity, dwAffect);
				if (pAffect != nullptr) {
					pkItem = ItemSystem::FindItemByID(p2Entity, pAffect->dwFlag);
					if (ItemSystem::IsValidItem(pkItem)) {
						ItemSystem::UnlockItem(pkItem);
						ItemSystem::SetItemSocket(pkItem, 1, 0);
					}

					AffectSystem::RemoveAffect(p2Entity, dwAffect);
				}
			}
		}
#endif

		m_MarriageByPID.erase(dwPID1);
		m_MarriageByPID.erase(dwPID2);

		DestroyCouple(couple);
	}

	void CManager::Login(entt::entity ch)
	{
		uint32_t pid = ecs::PlayerRuntime::GetPlayerID(ch);

		const entt::entity couple = Get(pid);
		if (couple == entt::null)
			return;

		LoginPartner(couple, ch);
	}

	void CManager::Logout(uint32_t pid)
	{
		const entt::entity couple = Get(pid);

		if (couple == entt::null)
			return;

		LogoutPartner(couple, pid);
	}

	void CManager::Logout(entt::entity ch)
	{
		Logout((ecs::PlayerRuntime::GetPlayerID(ch)));
	}

	void CManager::WeddingReady(uint32_t dwPID1, uint32_t dwPID2, uint32_t dwMapIndex)
	{
		const entt::entity couple = Get(dwPID1);
		if (couple == entt::null || MarriageSystem::GetOther(couple, dwPID1) != dwPID2)
		{
			LOG_ERROR("wrong marriage {}, {}", dwPID1, dwPID2);
			return;
		}

		Couple(couple)->weddingMapIndex = dwMapIndex;
	}

	void CManager::WeddingStart(uint32_t dwPID1, uint32_t dwPID2)
	{
		const entt::entity couple = Get(dwPID1);
		if (couple == entt::null || MarriageSystem::GetOther(couple, dwPID1) != dwPID2)
		{
			LOG_ERROR("wrong marriage {}, {}", dwPID1, dwPID2);
			return;
		}

		if (!Couple(couple)->weddingMapIndex)
			return;

		MarriageSystem::WarpToWeddingMap(couple, dwPID1);
		MarriageSystem::WarpToWeddingMap(couple, dwPID2);

		m_setWedding.insert(make_pair(dwPID1, dwPID2));
	}

	void CManager::WeddingEnd(uint32_t dwPID1, uint32_t dwPID2)
	{
		const entt::entity couple = Get(dwPID1);
		if (couple == entt::null || MarriageSystem::GetOther(couple, dwPID1) != dwPID2)
		{
			LOG_ERROR("wrong marriage {}, {}", dwPID1, dwPID2);
			return;
		}

		const auto* state = Couple(couple);
		if (!state->weddingMapIndex)
		{
			LOG_ERROR("not under wedding {}, {}", dwPID1, dwPID2);
			return;
		}

		const uint32_t mapIndex = *state->weddingMapIndex;
		if (map_allow_find(WEDDING_MAP_INDEX))
			if (!WeddingManager::instance().End(mapIndex))
			{
				LOG_ERROR("wedding map error: map_index={}", mapIndex);
				return;
			}

		// Ending the map runs its own teardown; read the couple again after it.
		if (auto* ended = Couple(couple))
			ended->weddingMapIndex.reset();

		m_setWedding.erase(make_pair(dwPID1, dwPID2));
	}

	void CManager::RequestEndWedding(uint32_t dwPID1, uint32_t dwPID2)
	{
		TPacketWeddingEnd p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;

		db_clientdesc->DBPacket(HEADER_GD_WEDDING_END, 0, &p, sizeof(p));
	}
}
