#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
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
		sys_log(0, "CArenaManager::AddArena - AddMap Error MapID: %d", mapIdx);
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
			sys_log(0, "CArenaMap::AddArena - Same Start Position set. stA(%d, %d) stB(%d, %d)", startA_X, startA_Y, startB_X, startB_Y);
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

	sys_log(0, "ARENA: ArenaMap will be destroy. mapIndex(%d)", m_dwMapIndex);

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

void CArenaManager::SendArenaMapListTo(LPCHARACTER pChar)
{
	for (auto iter = m_mapArenaMap.begin(); iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap* pArena = iter->second;
		pArena->SendArenaMapListTo(pChar, (iter->first));
	}
}

void CArenaMap::SendArenaMapListTo(LPCHARACTER pChar, uint32_t mapIdx)
{
	if (pChar == nullptr) return;

	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(pChar), CHAT_TYPE_INFO, "ArenaMapInfo Map: %d stA(%d, %d) stB(%d, %d)", mapIdx,
				(CArena*)(*iter)->GetStartPointA().x, (CArena*)(*iter)->GetStartPointA().y,
				(CArena*)(*iter)->GetStartPointB().x, (CArena*)(*iter)->GetStartPointB().y);
	}
}

bool CArenaManager::StartDuel(LPCHARACTER pCharFrom, LPCHARACTER pCharTo, int nSetPoint, int nMinute)
{
	if (pCharFrom == nullptr || pCharTo == nullptr) return false;

	for (auto iter = m_mapArenaMap.begin(); iter != m_mapArenaMap.end(); ++iter)
	{
		CArenaMap* pArenaMap = iter->second;
		if (pArenaMap->StartDuel(pCharFrom, pCharTo, nSetPoint, nMinute) == true)
		{
			return true;
		}
	}

	return false;
}

bool CArenaMap::StartDuel(LPCHARACTER pCharFrom, LPCHARACTER pCharTo, int nSetPoint, int nMinute)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;
		if (pArena->IsEmpty() == true)
		{
			return pArena->StartDuel(pCharFrom, pCharTo, nSetPoint, nMinute);
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
		sys_err( "ready_to_start_event> <Factor> Null pointer" );
		return 0;
	}

	CArena* pArena = info->pArena;

	if (pArena == nullptr)
	{
		sys_err("ARENA: Arena start event info is null.");
		return 0;
	}

	LPCHARACTER chA = pArena->GetPlayerA();
	LPCHARACTER chB = pArena->GetPlayerB();

	if (chA == nullptr || chB == nullptr)
	{
		sys_err("ARENA: Player err in event func ready_start_event");

		if (chA != nullptr)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 299, "");
#endif
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());
		}

		if (chB != nullptr)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 299, "");
#endif
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerBPID(), pArena->GetPlayerAPID());
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
				chA->SetArena(pArena);
				chB->SetArena(pArena);

				int count = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");

				if (count > 10000)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 348, "");
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 348, "");
#endif
				}
				else
				{
					chA->SetPotionLimit(count);
					chB->SetPotionLimit(count);
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 349, "%d", count);
					ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 349, "%d", count);
#endif
				}
				
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 222, "");
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 222, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 222, "");
#endif

				info->state++;
				return PASSES_PER_SEC(10);
			}
			break;

		case 1:
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 301, "");
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 301, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 301, "");
#endif
				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart) + 4;

				uint32_t dwOppList[8]; // �ִ� ��Ƽ�� 8�� �̹Ƿ�..

				dwOppList[0] = chB->GetPacketVID();
				TEMP_BUFFER buf;

				buf.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf.write(&dwOppList[0], 4);
				chA->GetDesc()->Packet(buf.read_peek(), buf.size());


				dwOppList[0] = chA->GetPacketVID();
				TEMP_BUFFER buf2;

				buf2.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf2.write(&dwOppList[0], 4);
				chB->GetDesc()->Packet(buf2.read_peek(), buf2.size());

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
				chA->Show(chA->GetMapIndex(), pArena->GetStartPointA().x * 100, pArena->GetStartPointA().y * 100);
				chB->Show(chB->GetMapIndex(), pArena->GetStartPointB().x * 100, pArena->GetStartPointB().y * 100);

				chA->GetDesc()->SetPhase(PHASE_GAME);
				chA->StartRecoveryEvent();
				chA->SetPosition(POS_STANDING);
				ecs::PointSystem::Change(AIHelpers::EcsOf(chA), POINT_HP, chA->GetMaxHP() - chA->GetHP());
				ecs::PointSystem::Change(AIHelpers::EcsOf(chA), POINT_SP, chA->GetMaxSP() - chA->GetSP());
				chA->ViewReencode();

				chB->GetDesc()->SetPhase(PHASE_GAME);
				chB->StartRecoveryEvent();
				chB->SetPosition(POS_STANDING);
				ecs::PointSystem::Change(AIHelpers::EcsOf(chB), POINT_HP, chB->GetMaxHP() - chB->GetHP());
				ecs::PointSystem::Change(AIHelpers::EcsOf(chB), POINT_SP, chB->GetMaxSP() - chB->GetSP());
				chB->ViewReencode();

				TEMP_BUFFER buf;
				TEMP_BUFFER buf2;
				uint32_t dwOppList[8]; // �ִ� ��Ƽ�� 8�� �̹Ƿ�..
				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart) + 4;

				dwOppList[0] = chB->GetPacketVID();
				buf.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf.write(&dwOppList[0], 4);
				chA->GetDesc()->Packet(buf.read_peek(), buf.size());

				dwOppList[0] = chA->GetPacketVID();
				buf2.write(&duelStart, sizeof(TPacketGCDuelStart));
				buf2.write(&dwOppList[0], 4);
				chB->GetDesc()->Packet(buf2.read_peek(), buf2.size());

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 301, "");
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 301, "");
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
				sys_log(0, "ARENA: Something wrong in event func. info->state(%d)", info->state);

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
		sys_err( "duel_time_out> <Factor> Null pointer" );
		return 0;
	}

	CArena* pArena = info->pArena;

	if (pArena == nullptr)
	{
		sys_err("ARENA: Time out event error");
		return 0;
	}

	LPCHARACTER chA = pArena->GetPlayerA();
	LPCHARACTER chB = pArena->GetPlayerB();

	if (chA == nullptr || chB == nullptr)
	{
		if (chA != nullptr)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 299, "");
#endif
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());
		}

		if (chB != nullptr)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 299, "");
#endif
			sys_log(0, "ARENA: Oppernent is disappered. MyPID(%d) OppPID(%d)", pArena->GetPlayerBPID(), pArena->GetPlayerAPID());
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
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 224, "");
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 224, "");
				pArena->SendChatPacketToObserver(CHAT_TYPE_INFO, 224, "");
#endif

				TPacketGCDuelStart duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.wSize = sizeof(TPacketGCDuelStart);

				chA->GetDesc()->Packet(&duelStart, sizeof(TPacketGCDuelStart));
				chA->GetDesc()->Packet(&duelStart, sizeof(TPacketGCDuelStart));

				info->state++;

				sys_log(0, "ARENA: Because of time over, duel is end. PIDA(%d) vs PIDB(%d)", pArena->GetPlayerAPID(), pArena->GetPlayerBPID());

				return PASSES_PER_SEC(10);
				break;

			case 1:
				pArena->EndDuel();
				break;
		}
	}

	return 0;
}

bool CArena::StartDuel(LPCHARACTER pCharFrom, LPCHARACTER pCharTo, int nSetPoint, int nMinute)
{
	this->m_dwPIDA = pCharFrom->GetPlayerID();
	this->m_dwPIDB = pCharTo->GetPlayerID();
	this->m_dwSetCount = nSetPoint;

	pCharFrom->WarpSet(GetStartPointA().x * 100, GetStartPointA().y * 100);
	pCharTo->WarpSet(GetStartPointB().x * 100, GetStartPointB().y * 100);

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

	ecs::PointSystem::Change(AIHelpers::EcsOf(pCharFrom), POINT_HP, pCharFrom->GetMaxHP() - pCharFrom->GetHP());
	ecs::PointSystem::Change(AIHelpers::EcsOf(pCharFrom), POINT_SP, pCharFrom->GetMaxSP() - pCharFrom->GetSP());

	ecs::PointSystem::Change(AIHelpers::EcsOf(pCharTo), POINT_HP, pCharTo->GetMaxHP() - pCharTo->GetHP());
	ecs::PointSystem::Change(AIHelpers::EcsOf(pCharTo), POINT_SP, pCharTo->GetMaxSP() - pCharTo->GetSP());

	sys_log(0, "ARENA: Start Duel with PID_A(%d) vs PID_B(%d)", GetPlayerAPID(), GetPlayerBPID());
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

	LPCHARACTER playerA = GetPlayerA();
	LPCHARACTER playerB = GetPlayerB();

	if (playerA != nullptr)
	{
		playerA->SetPKMode(PK_MODE_PEACE);
		playerA->StartRecoveryEvent();
		playerA->SetPosition(POS_STANDING);
		ecs::PointSystem::Change(AIHelpers::EcsOf(playerA), POINT_HP, playerA->GetMaxHP() - playerA->GetHP());
		ecs::PointSystem::Change(AIHelpers::EcsOf(playerA), POINT_SP, playerA->GetMaxSP() - playerA->GetSP());

		playerA->SetArena(nullptr);

		playerA->WarpSet(ARENA_RETURN_POINT_X(playerA->GetEmpire()), ARENA_RETURN_POINT_Y(playerA->GetEmpire()));
	}

	if (playerB != nullptr)
	{
		playerB->SetPKMode(PK_MODE_PEACE);
		playerB->StartRecoveryEvent();
		playerB->SetPosition(POS_STANDING);
		ecs::PointSystem::Change(AIHelpers::EcsOf(playerB), POINT_HP, playerB->GetMaxHP() - playerB->GetHP());
		ecs::PointSystem::Change(AIHelpers::EcsOf(playerB), POINT_SP, playerB->GetMaxSP() - playerB->GetSP());

		playerB->SetArena(nullptr);

		playerB->WarpSet(ARENA_RETURN_POINT_X(playerB->GetEmpire()), ARENA_RETURN_POINT_Y(playerB->GetEmpire()));
	}

	for (auto iter = m_mapObserver.begin(); iter != m_mapObserver.end(); ++iter)
	{
		LPCHARACTER pChar = CHARACTER_MANAGER::instance().FindByPID(iter->first);
		if (pChar != nullptr)
		{
			pChar->WarpSet(ARENA_RETURN_POINT_X(pChar->GetEmpire()), ARENA_RETURN_POINT_Y(pChar->GetEmpire()));
		}
	}

	m_mapObserver.clear();

	sys_log(0, "ARENA: End Duel PID_A(%d) vs PID_B(%d)", GetPlayerAPID(), GetPlayerBPID());

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
			LPCHARACTER chA = pArena->GetPlayerA();
			LPCHARACTER chB = pArena->GetPlayerB();

			if (chA != nullptr && chB != nullptr)
			{
				lua_newtable(L);

				lua_pushstring(L, chA->GetName());
				lua_rawseti(L, -2, 1);

				lua_pushstring(L, chB->GetName());
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

bool CArenaManager::CanAttack(LPCHARACTER pCharAttacker, LPCHARACTER pCharVictim)
{
	if (pCharAttacker == nullptr || pCharVictim == nullptr) return false;

	if (pCharAttacker == pCharVictim) return false;

	int32_t mapIndex = pCharAttacker->GetMapIndex();
	if (mapIndex != pCharVictim->GetMapIndex()) return false;

	auto iter = m_mapArenaMap.find(mapIndex);

	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->CanAttack(pCharAttacker, pCharVictim);
}

bool CArenaMap::CanAttack(LPCHARACTER pCharAttacker, LPCHARACTER pCharVictim)
{
	if (pCharAttacker == nullptr || pCharVictim == nullptr) return false;

	uint32_t dwPIDA = pCharAttacker->GetPlayerID();
	uint32_t dwPIDB = pCharVictim->GetPlayerID();

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

bool CArenaManager::OnDead(LPCHARACTER pCharKiller, LPCHARACTER pCharVictim)
{
	if (pCharKiller == nullptr || pCharVictim == nullptr) return false;

	int32_t mapIndex = pCharKiller->GetMapIndex();
	if (mapIndex != pCharVictim->GetMapIndex()) return false;

	const auto iter = m_mapArenaMap.find(mapIndex);
	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->OnDead(pCharKiller,  pCharVictim);
}

bool CArenaMap::OnDead(LPCHARACTER pCharKiller, LPCHARACTER pCharVictim)
{
	uint32_t dwPIDA = pCharKiller->GetPlayerID();
	uint32_t dwPIDB = pCharVictim->GetPlayerID();

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

	LPCHARACTER pCharA = GetPlayerA();
	LPCHARACTER pCharB = GetPlayerB();

	if (pCharA == nullptr && pCharB == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, 707, "");
#endif
		restart = false;
	}
	else if (pCharA == nullptr && pCharB != nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, 708, "");
#endif
		restart = false;
	}
	else if (pCharA != nullptr && pCharB == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		SendChatPacketToObserver(CHAT_TYPE_NOTICE, 708, "");
#endif
		restart = false;
	}
	else if (pCharA != nullptr && pCharB != nullptr)
	{
		if (m_dwPIDA == dwPIDA)
		{
			m_dwSetPointOfA++;

			if (m_dwSetPointOfA >= m_dwSetCount)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 109, "%s", pCharA->GetName());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharB), CHAT_TYPE_INFO, 109, "%s", pCharA->GetName());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 109, "%s", pCharA->GetName());
#endif
				sys_log(0, "ARENA: Duel is end. Winner %s(%d) Loser %s(%d)",
						pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
			}
			else
			{
				restart = true;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 110, "%s", pCharA->GetName());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharB), CHAT_TYPE_INFO, 110, "%s", pCharA->GetName());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 110, "%s", pCharA->GetName());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 709, "%s#%d#%s#%d", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
#endif
				sys_log(0, "ARENA: %s(%d) won a round vs %s(%d)", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
			}
		}
		else if (m_dwPIDB == dwPIDA)
		{
			m_dwSetPointOfB++;
			if (m_dwSetPointOfB >= m_dwSetCount)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 109, "%s", pCharB->GetName());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharB), CHAT_TYPE_INFO, 109, "%s", pCharB->GetName());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 109, "%s", pCharB->GetName());
#endif
				sys_log(0, "ARENA: Duel is end. Winner(%d) Loser(%d)", GetPlayerBPID(), GetPlayerAPID());
			}
			else
			{
				restart = true;
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 110, "%s", pCharB->GetName());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharB), CHAT_TYPE_INFO, 110, "%s", pCharB->GetName());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 110, "%s", pCharB->GetName());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 709, "%s#%d#%s#%d", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
				SendChatPacketToObserver(CHAT_TYPE_NOTICE, 709, "%s#%d#%s#%d", pCharA->GetName(), GetPlayerAPID(), pCharB->GetName(), GetPlayerBPID());
#endif
				sys_log(0, "ARENA : PID(%d) won a round. Opp(%d)", GetPlayerBPID(), GetPlayerAPID());
			}
		}
		else
		{
			// wtf
			sys_log(0, "ARENA : OnDead Error (%d, %d) (%d, %d)", m_dwPIDA, m_dwPIDB, dwPIDA, dwPIDB);
		}

		int potion = quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count");
		pCharA->SetPotionLimit(potion);
		pCharB->SetPotionLimit(potion);
	}
	else
	{
		// ���� �ȵȴ� ?!
	}

	if (restart == false)
	{
#ifdef TEXTS_IMPROVEMENT
		if (pCharA != nullptr) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 223, "");
		}
		if (pCharB != nullptr) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharB), CHAT_TYPE_INFO, 223, "");
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
		if (pCharA != nullptr) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharA), CHAT_TYPE_INFO, 221, "");
		}
		
		if (pCharB != nullptr) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pCharB), CHAT_TYPE_INFO, 221, "");
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

bool CArenaManager::AddObserver(LPCHARACTER pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	const auto iter = m_mapArenaMap.find(mapIdx);

	if (iter == m_mapArenaMap.end()) return false;

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->AddObserver(pChar, ObserverX, ObserverY);
}

bool CArenaMap::AddObserver(LPCHARACTER pChar, uint16_t ObserverX, uint16_t ObserverY)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsMyObserver(ObserverX, ObserverY) == true)
		{
			pChar->SetArena(pArena);
			return pArena->AddObserver(pChar);
		}
	}

	return false;
}

bool CArena::IsMyObserver(uint16_t ObserverX, uint16_t ObserverY)
{
	return ((std::cmp_equal(ObserverX, m_ObserverPoint.x)) && (std::cmp_equal(ObserverY, m_ObserverPoint.y)));
}

bool CArena::AddObserver(LPCHARACTER pChar)
{
	uint32_t pid = ((pChar)->GetPlayerID());

	m_mapObserver.insert(std::make_pair(pid, (LPCHARACTER)nullptr));

	pChar->SaveExitLocation();
	pChar->WarpSet(m_ObserverPoint.x * 100, m_ObserverPoint.y * 100);

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
		if (GetPlayerB() != nullptr) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(GetPlayerB()), CHAT_TYPE_INFO, 232, "");
		}
#endif
		sys_log(0, "ARENA : Duel is end because of Opp(%d) is disconnect. MyPID(%d)", GetPlayerAPID(), GetPlayerBPID());
		EndDuel();
	}
	else if (m_dwPIDB == pid)
	{
#ifdef TEXTS_IMPROVEMENT
		if (GetPlayerA() != nullptr) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(GetPlayerA()), CHAT_TYPE_INFO, 232, "");
		}
#endif
		sys_log(0, "ARENA : Duel is end because of Opp(%d) is disconnect. MyPID(%d)", GetPlayerBPID(), GetPlayerAPID());
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
		LPCHARACTER pChar = iter->second;
		if (pChar != nullptr) {
			if (pChar->GetDesc() != nullptr) {
				pChar->GetDesc()->Packet(c_pvData, iSize);
			}
		}
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
		LPCHARACTER pChar = iter->second;
		if (pChar) {
			if (pChar->GetDesc() != nullptr) {
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pChar), type, idx, chatbuf);
			}
		}
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

bool CArenaManager::RegisterObserverPtr(LPCHARACTER pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	if (pChar == nullptr) return false;

	const auto iter = m_mapArenaMap.find(mapIdx);

	if (iter == m_mapArenaMap.end())
	{
		sys_log(0, "ARENA : Cannot find ArenaMap. %d %d %d", mapIdx, ObserverX, ObserverY);
		return false;
	}

	CArenaMap* pArenaMap = iter->second;
	return pArenaMap->RegisterObserverPtr(pChar, mapIdx, ObserverX, ObserverY);
}

bool CArenaMap::RegisterObserverPtr(LPCHARACTER pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	for (auto iter = m_listArena.begin(); iter != m_listArena.end(); ++iter)
	{
		CArena* pArena = *iter;

		if (pArena->IsMyObserver(ObserverX, ObserverY) == true)
		{
			return pArena->RegisterObserverPtr(pChar);
		}
	}

	return false;
}

bool CArena::RegisterObserverPtr(LPCHARACTER pChar)
{
	uint32_t pid = ((pChar)->GetPlayerID());

	if (const auto iter = m_mapObserver.find(pid); iter == m_mapObserver.end())
	{
		sys_log(0, "ARENA : not in ob list");
		return false;
	}

	m_mapObserver[pid] = pChar;
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




