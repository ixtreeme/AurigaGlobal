#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/VisibilitySystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "constants.h"
#include "config.h"
#include "packet.h"
#include "desc.h"
#include "buffer_manager.h"
#include "start_position.h"
#include "questmanager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "arena.h"

#include <utility>

CArena::CArena(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	m_StartPointA.x = startA_X;
	m_StartPointA.y = startA_Y;
	m_StartPointA.z = 0;

	m_StartPointB.x = startB_X;
	m_StartPointB.y = startB_Y;
	m_StartPointB.z = 0;

	m_ObserverPoint.x = (startA_X + startB_X) / 2;
	m_ObserverPoint.y = (startA_Y + startB_Y) / 2;
	m_ObserverPoint.z = 0;

	m_pEvent = nullptr;
	m_pTimeOutEvent = nullptr;

	Clear();
}

void CArena::Clear()
{
	m_dwPIDA = 0;
	m_dwPIDB = 0;

	if (m_pEvent != nullptr)
	{
		event_cancel(&m_pEvent);
	}

	if (m_pTimeOutEvent != nullptr)
	{
		event_cancel(&m_pTimeOutEvent);
	}

	m_dwSetCount = 0;
	m_dwSetPointOfA = 0;
	m_dwSetPointOfB = 0;
}

bool CArenaManager::AddArena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	CArenaMap *pArenaMap = nullptr;

	if (const auto iter = m_mapArenaMap.find(mapIdx); iter == m_mapArenaMap.end())
	{
		pArenaMap = M2_NEW CArenaMap;
		m_mapArenaMap.insert(std::make_pair(mapIdx, pArenaMap));
	}
	else
	{
		pArenaMap = iter->second;
	}

	if (pArenaMap->AddArena(mapIdx, startA_X, startA_Y, startB_X, startB_Y) == false)
	{
		LOG_INFO("CArenaManager::AddArena - AddMap Error MapID: {}", mapIdx);
		return false;
	}

	return true;
}

bool CArenaMap::AddArena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		if ((CArena*)(*iter)->CheckArea(startA_X, startA_Y, startB_X, startB_Y) == nullptr)
		{
			LOG_INFO("CArenaMap::AddArena - Same Start Position set. stA({}, {}) stB({}, {})", startA_X, startA_Y, startB_X, startB_Y);
			return false;
		}
	}

	m_dwMapIndex = mapIdx;

	CArena *pArena = M2_NEW CArena(startA_X, startA_Y, startB_X, startB_Y);
	m_listArena.push_back(pArena);

	return true;
}

void CArenaManager::Destroy()
{
	auto iter = m_mapArenaMap.begin();

	for (; iter != m_mapArenaMap.end(); iter++)
	{
		CArenaMap* pArenaMap = iter->second;
		pArenaMap->Destroy();

		M2_DELETE(pArenaMap);
	}
	m_mapArenaMap.clear();
}

void CArenaMap::Destroy()
{
	auto iter = m_listArena.begin();

	LOG_INFO("ARENA: ArenaMap will be destroy. mapIndex({})", m_dwMapIndex);

	for (; iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;
		pArena->EndDuel();

		M2_DELETE(pArena);
	}
	m_listArena.clear();
}

bool CArena::CheckArea(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	if (std::cmp_equal(m_StartPointA.x, startA_X) && std::cmp_equal(m_StartPointA.y, startA_Y) &&
		std::cmp_equal(m_StartPointB.x, startB_X) && std::cmp_equal(m_StartPointB.y, startB_Y))
		return false;
	return true;
}

void CArenaManager::SendArenaMapListTo(entt::entity character)
{
	for (auto iter = m_mapArenaMap.begin(); iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap* pArena = iter->second;
		pArena->SendArenaMapListTo(character, (iter->first));
	}
}

void CArenaMap::SendArenaMapListTo(entt::entity character, uint32_t mapIdx)
{
	if (character == entt::null) return;

	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "ArenaMapInfo Map: %d stA(%d, %d) stB(%d, %d)", mapIdx,
				(CArena*)(*iter)->GetStartPointA().x, (CArena*)(*iter)->GetStartPointA().y,
				(CArena*)(*iter)->GetStartPointB().x, (CArena*)(*iter)->GetStartPointB().y);
	}
}

bool CArenaManager::StartDuel(entt::entity charFrom, entt::entity charTo, int nSetPoint, int nMinute)
{
	if (!ecs::PlayerRuntime::IsPC(charFrom) || !ecs::PlayerRuntime::IsPC(charTo)) return false;

	for (auto iter = m_mapArenaMap.begin(); iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap->StartDuel(charFrom, charTo, nSetPoint, nMinute) == true)
		{
			return true;
		}
	}

	return false;
}

bool CArenaMap::StartDuel(entt::entity charFrom, entt::entity charTo, int nSetPoint, int nMinute)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;
		if (pArena->IsEmpty() == true)
		{
			return pArena->StartDuel(charFrom, charTo, nSetPoint, nMinute);
		}
	}

	return false;
}

EVENTINFO(TArenaEventInfo)
{
	CArena *pArena;
	uint8_t state;

	TArenaEventInfo()
	: pArena(nullptr)
	, state(0)
	{
	}
};

EVENTFUNC(ready_to_start_event)
{
	if (event == nullptr)
		return 0;

	if (event->info == nullptr)
		return 0;

	auto info = dynamic_cast<TArenaEventInfo*>(event->info);

	if ( info == nullptr)
	{
		LOG_ERROR("ready_to_start_event> <Factor> Null pointer");
		return 0;
	}

	CArena* pArena = info->pArena;

	if (pArena == nullptr)
	{
		LOG_ERROR("ARENA: Arena start event info is null.");
		return 0;
	}

	const entt::entity chAEntity = pArena->GetPlayerA();

	const entt::entity chBEntity = pArena->GetPlayerB();


	if (chAEntity == entt::null || chBEntity == entt::null)
	{
		LOG_ERROR("ARENA: Player err in event func ready_start_event");

		if (chAEntity != entt::null)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 299, "");
#endif
			LOG_INFO("ARENA: Oppernent is disappered. MyPID({}) OppPID({})", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());
		}

		if (chBEntity != entt::null)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 299, "");
#endif
			LOG_INFO("ARENA: Oppernent is disappered. MyPID({}) OppPID({})", pArena->GetPlayerBPID(), pArena->GetPlayerAPID());
		}

#ifdef TEXTS_IMPROVEMENT
		pArena->SendChatPacketToObserver(CHAT_TYPE_NOTICE, 706, "");
#endif
		pArena->EndDuel();
		return 0;
	}

	switch (info->state)
	{
		case 0:
			{
				ecs::PlayerRuntime::SetArena(chAEntity, pArena);
				ecs::PlayerRuntime::SetArena(chBEntity, pArena);

				int count = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");

				if (count > 10000)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 348, "");
					ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 348, "");
#endif
				}
				else
				{
					ecs::PlayerRuntime::SetPotionLimit(chAEntity, count);
					ecs::PlayerRuntime::SetPotionLimit(chBEntity, count);
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 349, "%d", count);
					ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 349, "%d", count);
#endif
				}

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 222, "");
				ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 222, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 222, "");
#endif

				info->state++;
				return PASSES_PER_SEC(10);
			}
			break;

		case 1:
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 301, "");
				ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 301, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 301, "");
#endif
				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart) + 4;

				uint32_t dwOppList[8]; // �ִ� ��Ƽ�� 8�� �̹Ƿ�..

				dwOppList[0] = ecs::PlayerRuntime::GetPacketVID(chBEntity);
				TEMP_BUFFER buf;

				buf.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf.write(&dwOppList[0], 4);
				ecs::PlayerRuntime::GetDesc(chAEntity)->Packet(buf.read_peek(), buf.size());


				dwOppList[0] = ecs::PlayerRuntime::GetPacketVID(chAEntity);
				TEMP_BUFFER buf2;

				buf2.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf2.write(&dwOppList[0], 4);
				ecs::PlayerRuntime::GetDesc(chBEntity)->Packet(buf2.read_peek(), buf2.size());

				return 0;
			}
			break;

		case 2:
			{
				pArena->EndDuel();
				return 0;
			}
			break;

		case 3:
			{
				ecs::MovementSystem::Show(chAEntity, ecs::PlayerRuntime::GetMapIndex(chAEntity), pArena->GetStartPointA().x * 100, pArena->GetStartPointA().y * 100);
				ecs::MovementSystem::Show(chBEntity, ecs::PlayerRuntime::GetMapIndex(chBEntity), pArena->GetStartPointB().x * 100, pArena->GetStartPointB().y * 100);

				ecs::PlayerRuntime::GetDesc(chAEntity)->SetPhase(PHASE_GAME);
				ecs::PlayerRuntime::StartRecoveryEvent(chAEntity);
				ecs::PlayerRuntime::SetPosition(chAEntity, POS_STANDING);
				ecs::PointSystem::Change(chAEntity, POINT_HP, ecs::PointSystem::GetMaxHP(chAEntity) - ecs::PointSystem::Get(chAEntity, POINT_HP));
				ecs::PointSystem::Change(chAEntity, POINT_SP, ecs::PointSystem::GetMaxSP(chAEntity) - ecs::PointSystem::Get(chAEntity, POINT_SP));
				ecs::VisibilitySystem::Reencode(chAEntity);

				ecs::PlayerRuntime::GetDesc(chBEntity)->SetPhase(PHASE_GAME);
				ecs::PlayerRuntime::StartRecoveryEvent(chBEntity);
				ecs::PlayerRuntime::SetPosition(chBEntity, POS_STANDING);
				ecs::PointSystem::Change(chBEntity, POINT_HP, ecs::PointSystem::GetMaxHP(chBEntity) - ecs::PointSystem::Get(chBEntity, POINT_HP));
				ecs::PointSystem::Change(chBEntity, POINT_SP, ecs::PointSystem::GetMaxSP(chBEntity) - ecs::PointSystem::Get(chBEntity, POINT_SP));
				ecs::VisibilitySystem::Reencode(chBEntity);

				TEMP_BUFFER buf;
				TEMP_BUFFER buf2;
				uint32_t dwOppList[8]; // �ִ� ��Ƽ�� 8�� �̹Ƿ�..
				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart) + 4;

				dwOppList[0] = ecs::PlayerRuntime::GetPacketVID(chBEntity);
				buf.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf.write(&dwOppList[0], 4);
				ecs::PlayerRuntime::GetDesc(chAEntity)->Packet(buf.read_peek(), buf.size());

				dwOppList[0] = ecs::PlayerRuntime::GetPacketVID(chAEntity);
				buf2.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf2.write(&dwOppList[0], 4);
				ecs::PlayerRuntime::GetDesc(chBEntity)->Packet(buf2.read_peek(), buf2.size());

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 301, "");
				ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 301, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 301, "");
#endif
				pArena->ClearEvent();

				return 0;
			}
			break;

		default:
			{
#ifdef TEXTS_IMPROVEMENT
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 707, "");
#endif
				LOG_INFO("ARENA: Something wrong in event func. info->state({})", info->state);

				pArena->EndDuel();

				return 0;
			}
	}
}

EVENTFUNC(duel_time_out)
{
	if (event == nullptr) return 0;
	if (event->info == nullptr) return 0;

	auto info = dynamic_cast<TArenaEventInfo*>(event->info);

	if ( info == nullptr)
	{
		LOG_ERROR("duel_time_out> <Factor> Null pointer");
		return 0;
	}

	CArena* pArena = info->pArena;

	if (pArena == nullptr)
	{
		LOG_ERROR("ARENA: Time out event error");
		return 0;
	}

	const entt::entity chAEntity = pArena->GetPlayerA();

	const entt::entity chBEntity = pArena->GetPlayerB();


	if (chAEntity == entt::null || chBEntity == entt::null)
	{
		if (chAEntity != entt::null)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 299, "");
#endif
			LOG_INFO("ARENA: Oppernent is disappered. MyPID({}) OppPID({})", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());
		}

		if (chBEntity != entt::null)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 299, "");
#endif
			LOG_INFO("ARENA: Oppernent is disappered. MyPID({}) OppPID({})", pArena->GetPlayerBPID(), pArena->GetPlayerAPID());
		}

#ifdef TEXTS_IMPROVEMENT
		pArena->SendChatPacketToObserver(CHAT_TYPE_NOTICE, 706, "");
#endif
		pArena->EndDuel();
		return 0;
	}
	else
	{
		switch (info->state)
		{
			case 0:
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(chAEntity, CHAT_TYPE_INFO, 224, "");
				ecs::ChatSystem::SendNew(chBEntity, CHAT_TYPE_INFO, 224, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 224, "");
#endif

				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart);

				ecs::PlayerRuntime::GetDesc(chAEntity)->Packet(&duelStart, sizeof(TPacketGCDuelStart));
				ecs::PlayerRuntime::GetDesc(chAEntity)->Packet(&duelStart, sizeof(TPacketGCDuelStart));

				info->state++;

				LOG_INFO("ARENA: Because of time over, duel is end. PIDA({}) vs PIDB({})", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());

				return PASSES_PER_SEC(10);
				break;

			case 1:
				pArena->EndDuel();
				break;
		}
	}

	return 0;
}

bool CArena::StartDuel(entt::entity charFrom, entt::entity charTo, int nSetPoint, int nMinute)
{
	this->m_dwPIDA = ecs::PlayerRuntime::GetPlayerID(charFrom);
	this->m_dwPIDB = ecs::PlayerRuntime::GetPlayerID(charTo);
	this->m_dwSetCount = nSetPoint;

	ecs::MovementSystem::WarpSet(charFrom, GetStartPointA().x * 100, GetStartPointA().y * 100);
	ecs::MovementSystem::WarpSet(charTo, GetStartPointB().x * 100, GetStartPointB().y * 100);

	if (m_pEvent != nullptr) {
		event_cancel(&m_pEvent);
	}

	TArenaEventInfo* info = AllocEventInfo<TArenaEventInfo>();

	info->pArena = this;
	info->state = 0;

	m_pEvent = event_create(ready_to_start_event, info, PASSES_PER_SEC(10));

	if (m_pTimeOutEvent != nullptr) {
		event_cancel(&m_pTimeOutEvent);
	}

	info = AllocEventInfo<TArenaEventInfo>();

	info->pArena = this;
	info->state = 0;

	m_pTimeOutEvent = event_create(duel_time_out, info, PASSES_PER_SEC(nMinute*60));

	ecs::PointSystem::Change(charFrom, POINT_HP, ecs::PointSystem::GetMaxHP(charFrom) - ecs::PointSystem::Get(charFrom, POINT_HP));
	ecs::PointSystem::Change(charFrom, POINT_SP, ecs::PointSystem::GetMaxSP(charFrom) - ecs::PointSystem::Get(charFrom, POINT_SP));

	ecs::PointSystem::Change(charTo, POINT_HP, ecs::PointSystem::GetMaxHP(charTo) - ecs::PointSystem::Get(charTo, POINT_HP));
	ecs::PointSystem::Change(charTo, POINT_SP, ecs::PointSystem::GetMaxSP(charTo) - ecs::PointSystem::Get(charTo, POINT_SP));

	LOG_INFO("ARENA: Start Duel with PID_A({}) vs PID_B({})", GetPlayerAPID(), GetPlayerBPID());
	return true;
}

void CArenaManager::EndAllDuel()
{
	for (auto iter = m_mapArenaMap.begin(); iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap *pArenaMap = iter->second;
		if (pArenaMap != nullptr)
			pArenaMap->EndAllDuel();
	}

	return;
}

void CArenaMap::EndAllDuel()
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena *pArena = *iter;
		if (pArena != nullptr)
			pArena->EndDuel();
	}
}

void CArena::EndDuel()
{
	if (m_pEvent != nullptr) {
		event_cancel(&m_pEvent);
	}
	if (m_pTimeOutEvent != nullptr) {
		event_cancel(&m_pTimeOutEvent);
	}

	const entt::entity playerAEntity = GetPlayerA();

	const entt::entity playerBEntity = GetPlayerB();


	if (playerAEntity != entt::null)
	{
		CombatSystem::SetPKMode(playerAEntity, PK_MODE_PEACE);
		ecs::PlayerRuntime::StartRecoveryEvent(playerAEntity);
		ecs::PlayerRuntime::SetPosition(playerAEntity, POS_STANDING);
		ecs::PointSystem::Change(playerAEntity, POINT_HP, ecs::PointSystem::GetMaxHP(playerAEntity) - ecs::PointSystem::Get(playerAEntity, POINT_HP));
		ecs::PointSystem::Change(playerAEntity, POINT_SP, ecs::PointSystem::GetMaxSP(playerAEntity) - ecs::PointSystem::Get(playerAEntity, POINT_SP));

		ecs::PlayerRuntime::SetArena(playerAEntity, nullptr);

		ecs::MovementSystem::WarpSet(playerAEntity, ARENA_RETURN_POINT_X(ecs::PlayerRuntime::GetEmpire(playerAEntity)), ARENA_RETURN_POINT_Y(ecs::PlayerRuntime::GetEmpire(playerAEntity)));
	}

	if (playerBEntity != entt::null)
	{
		CombatSystem::SetPKMode(playerBEntity, PK_MODE_PEACE);
		ecs::PlayerRuntime::StartRecoveryEvent(playerBEntity);
		ecs::PlayerRuntime::SetPosition(playerBEntity, POS_STANDING);
		ecs::PointSystem::Change(playerBEntity, POINT_HP, ecs::PointSystem::GetMaxHP(playerBEntity) - ecs::PointSystem::Get(playerBEntity, POINT_HP));
		ecs::PointSystem::Change(playerBEntity, POINT_SP, ecs::PointSystem::GetMaxSP(playerBEntity) - ecs::PointSystem::Get(playerBEntity, POINT_SP));

		ecs::PlayerRuntime::SetArena(playerBEntity, nullptr);

		ecs::MovementSystem::WarpSet(playerBEntity, ARENA_RETURN_POINT_X(ecs::PlayerRuntime::GetEmpire(playerBEntity)), ARENA_RETURN_POINT_Y(ecs::PlayerRuntime::GetEmpire(playerBEntity)));
	}

	for (auto iter = m_mapObserver.begin(); iter != m_mapObserver.end(); ++iter)
	{
		const entt::entity observer = ecs::PlayerRuntime::FindByPlayerID(iter->first);
		if (observer != entt::null)
		{
			ecs::MovementSystem::WarpSet(observer,
				ARENA_RETURN_POINT_X(ecs::PlayerRuntime::GetEmpire(observer)),
				ARENA_RETURN_POINT_Y(ecs::PlayerRuntime::GetEmpire(observer)));
		}
	}

	m_mapObserver.clear();

	LOG_INFO("ARENA: End Duel PID_A({}) vs PID_B({})", GetPlayerAPID(), GetPlayerBPID());

	Clear();
}

void CArenaManager::GetDuelList(lua_State* L)
{
	auto iter = m_mapArenaMap.begin();

	int index = 1;
	lua_newtable(L);

	for (; iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap != nullptr)
			index = pArenaMap->GetDuelList(L, index);
	}
}

int CArenaMap::GetDuelList(lua_State* L, int index)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena == nullptr) continue;

		if (pArena->IsEmpty() == false)
		{
			const entt::entity chAEntity = pArena->GetPlayerA();
			const entt::entity chBEntity = pArena->GetPlayerB();

			if (chAEntity != entt::null && chBEntity != entt::null)
			{
				lua_newtable(L);

				lua_pushstring(L, ecs::PlayerRuntime::GetName(chAEntity).data());
				lua_rawseti(L, -2, 1);

				lua_pushstring(L, ecs::PlayerRuntime::GetName(chBEntity).data());
				lua_rawseti(L, -2, 2);

				lua_pushnumber(L, m_dwMapIndex);
				lua_rawseti(L, -2, 3);

				lua_pushnumber(L, pArena->GetObserverPoint().x);
				lua_rawseti(L, -2, 4);

				lua_pushnumber(L, pArena->GetObserverPoint().y);
				lua_rawseti(L, -2, 5);

				lua_rawseti(L, -2, index++);
			}
		}
	}

	return index;
}

bool CArenaManager::CanAttack(entt::entity attacker, entt::entity victim)
{
	if (attacker == entt::null || victim == entt::null) return false;

	if (attacker == victim) return false;

	int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(attacker);
	if (mapIndex != ecs::PlayerRuntime::GetMapIndex(victim)) return false;

	auto iter = m_mapArenaMap.find(mapIndex);

	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->CanAttack(attacker, victim);
}

bool CArenaMap::CanAttack(entt::entity attacker, entt::entity victim)
{
	if (attacker == entt::null || victim == entt::null) return false;

	uint32_t dwPIDA = ecs::PlayerRuntime::GetPlayerID(attacker);
	uint32_t dwPIDB = ecs::PlayerRuntime::GetPlayerID(victim);

	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;
		if (pArena->CanAttack(dwPIDA, dwPIDB) == true)
		{
			return true;
		}
	}
	return false;
}

bool CArena::CanAttack(uint32_t dwPIDA, uint32_t dwPIDB)
{
	// 1:1 ���� �ٴ�� �� ��� ���� �ʿ�
	if (m_dwPIDA == dwPIDA && m_dwPIDB == dwPIDB) return true;
	if (m_dwPIDA == dwPIDB && m_dwPIDB == dwPIDA) return true;

	return false;
}

bool CArenaManager::OnDead(entt::entity killer, entt::entity victim)
{
	if (killer == entt::null || victim == entt::null) return false;

	int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(killer);
	if (mapIndex != ecs::PlayerRuntime::GetMapIndex(victim)) return false;

	const auto iter = m_mapArenaMap.find(mapIndex);
	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->OnDead(killer, victim);
}

bool CArenaMap::OnDead(entt::entity killer, entt::entity victim)
{
	uint32_t dwPIDA = ecs::PlayerRuntime::GetPlayerID(killer);
	uint32_t dwPIDB = ecs::PlayerRuntime::GetPlayerID(victim);

	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsMember(dwPIDA) == true && pArena->IsMember(dwPIDB) == true)
		{
			pArena->OnDead(dwPIDA, dwPIDB);
			return true;
		}
	}
	return false;
}

bool CArena::OnDead(uint32_t dwPIDA, uint32_t dwPIDB)
{
	bool restart = false;

	const entt::entity charA = GetPlayerA();

	const entt::entity charB = GetPlayerB();


	if (charA == entt::null && charB == entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, 707, "");
#endif
		restart = false;
	}
	else if (charA == entt::null && charB != entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, 708, "");
#endif
		restart = false;
	}
	else if (charA != entt::null && charB == entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, 708, "");
#endif
		restart = false;
	}
	else if (charA != entt::null && charB != entt::null)
	{
		if (m_dwPIDA == dwPIDA)
		{
			m_dwSetPointOfA++;

			if (m_dwSetPointOfA >= m_dwSetCount)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 109, "%s", ecs::PlayerRuntime::GetName(charA).data());
				ecs::ChatSystem::SendNew(charB, CHAT_TYPE_INFO, 109, "%s", ecs::PlayerRuntime::GetName(charA).data());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 109, "%s", ecs::PlayerRuntime::GetName(charA).data());
#endif
				LOG_INFO("ARENA: Duel is end. Winner {}({}) Loser {}({})", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
			}
			else
			{
				restart = true;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 110, "%s", ecs::PlayerRuntime::GetName(charA).data());
				ecs::ChatSystem::SendNew(charB, CHAT_TYPE_INFO, 110, "%s", ecs::PlayerRuntime::GetName(charA).data());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 110, "%s", ecs::PlayerRuntime::GetName(charA).data());
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 709, "%s#%d#%s#%d", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
#endif
				LOG_INFO("ARENA: {}({}) won a round vs {}({})", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
			}
		}
		else if (m_dwPIDB == dwPIDA)
		{
			m_dwSetPointOfB++;
			if (m_dwSetPointOfB >= m_dwSetCount)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 109, "%s", ecs::PlayerRuntime::GetName(charB).data());
				ecs::ChatSystem::SendNew(charB, CHAT_TYPE_INFO, 109, "%s", ecs::PlayerRuntime::GetName(charB).data());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 109, "%s", ecs::PlayerRuntime::GetName(charB).data());
#endif
				LOG_INFO("ARENA: Duel is end. Winner({}) Loser({})", GetPlayerBPID(), GetPlayerAPID());
			}
			else
			{
				restart = true;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 110, "%s", ecs::PlayerRuntime::GetName(charB).data());
				ecs::ChatSystem::SendNew(charB, CHAT_TYPE_INFO, 110, "%s", ecs::PlayerRuntime::GetName(charB).data());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 110, "%s", ecs::PlayerRuntime::GetName(charB).data());
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
				ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 709, "%s#%d#%s#%d", ecs::PlayerRuntime::GetName(charA).data(), GetPlayerAPID(), ecs::PlayerRuntime::GetName(charB).data(), GetPlayerBPID());
#endif
				LOG_INFO("ARENA : PID({}) won a round. Opp({})", GetPlayerBPID(), GetPlayerAPID());
			}
		}
		else
		{
			// wtf
			LOG_INFO("ARENA : OnDead Error ({}, {}) ({}, {})", m_dwPIDA, m_dwPIDB, dwPIDA, dwPIDB);
		}

		int potion = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");
		ecs::PlayerRuntime::SetPotionLimit(charA, potion);
		ecs::PlayerRuntime::SetPotionLimit(charB, potion);
	}
	else
	{
		// ���� �ȵȴ� ?!
	}

	if (restart == false)
	{
#ifdef TEXTS_IMPROVEMENT
		if (charA != entt::null) {
			ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 223, "");
		}
		if (charB != entt::null) {
			ecs::ChatSystem::SendNew(charB, CHAT_TYPE_INFO, 223, "");
		}

		SendChatPacketToObserver(CHAT_TYPE_INFO, 223, "");
#endif
		if (m_pEvent != nullptr) {
			event_cancel(&m_pEvent);
		}

		TArenaEventInfo* info = AllocEventInfo<TArenaEventInfo>();

		info->pArena = this;
		info->state = 2;

		m_pEvent = event_create(ready_to_start_event, info, PASSES_PER_SEC(10));
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		if (charA != entt::null) {
			ecs::ChatSystem::SendNew(charA, CHAT_TYPE_INFO, 221, "");
		}

		if (charB != entt::null) {
			ecs::ChatSystem::SendNew(charB, CHAT_TYPE_INFO, 221, "");
		}

		SendChatPacketToObserver(CHAT_TYPE_INFO, 221, "");
#endif
		if (m_pEvent != nullptr) {
			event_cancel(&m_pEvent);
		}

		TArenaEventInfo* info = AllocEventInfo<TArenaEventInfo>();

		info->pArena = this;
		info->state = 3;

		m_pEvent = event_create(ready_to_start_event, info, PASSES_PER_SEC(10));
	}

	return true;
}

bool CArenaManager::AddObserver(entt::entity character, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	if (!ecs::PlayerRuntime::IsPC(character)) return false;

	const auto iter = m_mapArenaMap.find(mapIdx);

	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->AddObserver(character, ObserverX, ObserverY);
}

bool CArenaMap::AddObserver(entt::entity character, uint16_t ObserverX, uint16_t ObserverY)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsMyObserver(ObserverX, ObserverY) == true)
		{
			ecs::PlayerRuntime::SetArena(character, pArena);
			return pArena->AddObserver(character);
		}
	}

	return false;
}

bool CArena::IsMyObserver(uint16_t ObserverX, uint16_t ObserverY)
{
	return ((std::cmp_equal(ObserverX, m_ObserverPoint.x)) && (std::cmp_equal(ObserverY, m_ObserverPoint.y)));
}

bool CArena::AddObserver(entt::entity character)
{
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	if (pid == 0)
		return false;

	m_mapObserver.insert(std::make_pair(pid, entt::null));

	ecs::MovementSystem::SaveExitLocation(character);
	ecs::MovementSystem::WarpSet(character, m_ObserverPoint.x * 100, m_ObserverPoint.y * 100);

	return true;
}

bool CArenaManager::IsArenaMap(uint32_t dwMapIndex)
{
	return m_mapArenaMap.contains(dwMapIndex);
}

MEMBER_IDENTITY CArenaManager::IsMember(uint32_t dwMapIndex, uint32_t PID)
{
	if (const auto iter = m_mapArenaMap.find(dwMapIndex); iter != m_mapArenaMap.end())
	{
		CArenaMap* pArenaMap = iter->second;
		return pArenaMap->IsMember(PID);
	}

	return MEMBER_NO;
}

MEMBER_IDENTITY CArenaMap::IsMember(uint32_t PID)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsObserver(PID) == true) return MEMBER_OBSERVER;
		if (pArena->IsMember(PID) == true) return MEMBER_DUELIST;
	}
	return MEMBER_NO;
}

bool CArena::IsObserver(uint32_t PID)
{
	const auto iter = m_mapObserver.find(PID);

	return iter != m_mapObserver.end();
}

void CArena::OnDisconnect(uint32_t pid)
{
	if (m_dwPIDA == pid)
	{
#ifdef TEXTS_IMPROVEMENT
		if (GetPlayerB() != entt::null) {
			ecs::ChatSystem::SendNew(GetPlayerB(), CHAT_TYPE_INFO, 232, "");
		}
#endif
		LOG_INFO("ARENA : Duel is end because of Opp({}) is disconnect. MyPID({})", GetPlayerAPID(), GetPlayerBPID());
		EndDuel();
	}
	else if (m_dwPIDB == pid)
	{
#ifdef TEXTS_IMPROVEMENT
		if (GetPlayerA() != entt::null) {
			ecs::ChatSystem::SendNew(GetPlayerA(), CHAT_TYPE_INFO, 232, "");
		}
#endif
		LOG_INFO("ARENA : Duel is end because of Opp({}) is disconnect. MyPID({})", GetPlayerBPID(), GetPlayerAPID());
		EndDuel();
	}
}

void CArena::RemoveObserver(uint32_t pid)
{
	if (const auto iter = m_mapObserver.find(pid); iter != m_mapObserver.end())
	{
		m_mapObserver.erase(iter);
	}
}

void CArena::SendPacketToObserver(const void * c_pvData, int iSize)
{
	for (auto iter = m_mapObserver.begin(); iter != m_mapObserver.end(); ++iter)
	{
		const entt::entity observer = iter->second;
		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(observer))
			desc->Packet(c_pvData, iSize);
	}
}

#ifdef TEXTS_IMPROVEMENT
void CArena::SendChatPacketToObserver(uint8_t type, uint32_t idx, const char * format, ...)
{
	char chatbuf[256];
	va_list args;
	va_start(args, format);
	vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	for (auto iter = m_mapObserver.begin(); iter != m_mapObserver.end(); ++iter) {
		const entt::entity observer = iter->second;
		if (ecs::PlayerRuntime::GetDesc(observer))
			ecs::ChatSystem::SendNew(observer, type, idx, chatbuf);
	}
}
#endif

bool CArenaManager::EndDuel(uint32_t pid)
{
	for (auto iter = m_mapArenaMap.begin(); iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap->EndDuel(pid) == true) return true;
	}
	return false;
}

bool CArenaMap::EndDuel(uint32_t pid)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;
		if (pArena->IsMember(pid) == true)
		{
			pArena->EndDuel();
			return true;
		}
	}
	return false;
}

bool CArenaManager::RegisterObserverPtr(entt::entity character, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	if (!ecs::PlayerRuntime::IsPC(character)) return false;

	const auto iter = m_mapArenaMap.find(mapIdx);

	if (iter == m_mapArenaMap.end())
	{
		LOG_INFO("ARENA : Cannot find ArenaMap. {} {} {}", mapIdx, ObserverX, ObserverY);
		return false;
	}

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->RegisterObserverPtr(character, mapIdx, ObserverX, ObserverY);
}

bool CArenaMap::RegisterObserverPtr(entt::entity character, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsMyObserver(ObserverX, ObserverY) == true)
		{
			return pArena->RegisterObserverPtr(character);
		}
	}

	return false;
}

bool CArena::RegisterObserverPtr(entt::entity character)
{
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);

	if (const auto iter = m_mapObserver.find(pid); iter == m_mapObserver.end())
	{
		LOG_INFO("ARENA : not in ob list");
		return false;
	}

	m_mapObserver[pid] = character;
	return true;
}

// #ifdef ENABLE_NEWSTUFF
bool IsAllowedPotionOnPVP(uint32_t dwVnum)
{
	switch (dwVnum)
	{
		// blue potions
		case 27004:
		case 27005:
		case 27006:
		// auto blue potions
		case 39040:
		case 39041:
		case 39042:
		case 72727:
		case 72728:
		case 72729:
		case 72730:
			return true;
	}
	return false;
}

bool IsLimitedPotionOnPVP(uint32_t dwVnum)
{
	return IsLimitedPotion(dwVnum) && !IsAllowedPotionOnPVP(dwVnum);
}

bool IsLimitedPotion(uint32_t dwVnum)
{
	// @fixme122
	if ((50801 <= dwVnum) && (dwVnum <= 50826))
		return true;

	// @warme005
	switch (dwVnum)
	{
		case 50020:
		case 50021:
		case 50022:
		case 50801:
		case 50802:
		case 50813:
		case 50814:
		case 50817:
		case 50818:
		case 50819:
		case 50820:
		case 50821:
		case 50822:
		case 50823:
		case 50824:
		case 50825:
		case 50826:
		case 71044:
		case 71055:
			return true;
	}
	return false;
}
// #endif

bool CArenaManager::IsLimitedItem(int32_t lMapIndex, uint32_t dwVnum)
{
	if (IsArenaMap(lMapIndex) && IsLimitedPotion(dwVnum))
		return true;

	return false;
}




