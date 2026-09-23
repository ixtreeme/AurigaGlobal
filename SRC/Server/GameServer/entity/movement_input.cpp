#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "building.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "desc.h"
#include "log.h"
#include "map_location.h"
#include "motion.h"
#include "packet.h"
#include "protocol.h"
#include "questmanager.h"
#include "regen.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "start_position.h"
#include "utils.h"
#include "../ecs/components/movement_components.hpp"
#include "../ecs/services/SpatialService.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/ViewSystem.hpp"
#include "OXEvent.h"

void CInputMain::OnClick(entt::entity character, const char* data)
{
    if (!data || !ecs::PlayerRuntime::IsPC(character)) return;
    command_on_click request {};
    memcpy(&request, data, sizeof(request));
    const entt::entity target = CHARACTER_MANAGER::instance().FindEntity(request.vid);
    if (ecs::PlayerRuntime::IsValid(target))
        ecs::PlayerRuntime::OnClick(target, character);
    else if (test_server)
        LOG_ERROR("OnClick {}: missing VID {}", ecs::PlayerRuntime::GetName(character), request.vid);
}

void CInputMain::Position(entt::entity character, const char * data)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate Position handler ECS
// DUAL-PATH: legacy only during migration window
	struct command_position * pinfo = (struct command_position *) data;

	switch (pinfo->position)
	{
		case POSITION_GENERAL:
			ecs::MovementSystem::Standup(character);
			break;

		case POSITION_SITTING_CHAIR:
			ecs::MovementSystem::Sitdown(character, 0);
			break;

		case POSITION_SITTING_GROUND:
			ecs::MovementSystem::Sitdown(character, 1);
			break;
	}
}

void CInputMain::Move(entt::entity character, const char * data)
{
	if (!ecs::IsCharacter(character))
		return;

	struct command_move * pinfo = (struct command_move *) data;
	if (!ecs::MovementSystem::CanMove(character))
		return;

	if (pinfo->bFunc >= FUNC_MAX_NUM && !(pinfo->bFunc & 0x80))
	{
		LOG_ERROR("invalid move type: {}", ecs::PlayerRuntime::GetName(character).data());
		return;
	}

	//enum EMoveFuncType
	//{
	//	FUNC_WAIT,
	//	FUNC_MOVE,
	//	FUNC_ATTACK,
	//	FUNC_COMBO,
	//	FUNC_MOB_SKILL,
	//	_FUNC_SKILL,
	//	FUNC_MAX_NUM,
	//	FUNC_SKILL = 0x80,
	//};


//	if (!test_server)
	{
		const float fDistFromCurrent = DISTANCE_SQRT((ecs::PlayerRuntime::GetX(character) - pinfo->lX) / 100, (ecs::PlayerRuntime::GetY(character) - pinfo->lY) / 100);
		float fDist = fDistFromCurrent;

		// When movement is already in-flight, compare the next client target against the
		// pending server destination as well. Without this, legitimate follow-up move
		// packets get treated as teleports and the server rubberbands the player.
		if (pinfo->bFunc == FUNC_MOVE &&
			ecs::MovementSystem::GetCurrentMoveDuration(character) > 0 &&
			(ecs::MovementSystem::GetCurrentDestX(character) != ecs::PlayerRuntime::GetX(character) || ecs::MovementSystem::GetCurrentDestY(character) != ecs::PlayerRuntime::GetY(character)))
		{
			const float fDistFromDest = DISTANCE_SQRT((ecs::MovementSystem::GetCurrentDestX(character) - pinfo->lX) / 100, (ecs::MovementSystem::GetCurrentDestY(character) - pinfo->lY) / 100);
			fDist = std::min(fDistFromCurrent, fDistFromDest);
		}
		if (((false == MountSystem::IsRiding(character) && fDist > 30) || fDist > 60) && OXEVENT_MAP_INDEX != ecs::PlayerRuntime::GetMapIndex(character))
		{
			LOG_INFO("MOVE: {} trying to move too far (dist: {:.1f}m current: {:.1f}m) Riding({})", ecs::PlayerRuntime::GetName(character).data(), fDist, fDistFromCurrent, MountSystem::IsRiding(character));

			ecs::MovementSystem::Show(character, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetZ(character));
			ecs::MovementSystem::Stop(character);
// Phase 15E-final.LPENTITY.4-architect H fixup-4:
			// Anti-cheat backport early-returns BEFORE the line ~2385
			// PacketAround(GC_MOVE) broadcast, so peers never receive
			// a movement packet for the rejected client move.
			// Pre-Phase D the m_map_view polling at the receiving
			// peers next tick self-corrected via UpdateSectree.
			// After D.6 stubbed that polling, the peer is left
			// rendering whatever the last packet said - typically
			// frozen at the post-stop position.
			//
			// Fix: emit SendMovePacket(FUNC_WAIT). The FUNC_WAIT
			// branch pulls (x, y, duration) from GetCurrentDestX/Y
			// and MoveDuration; after Stop() above they evaluate
			// to the servers current position with duration 0 -
			// halt at this position.
			ecs::MovementSystem::SendMovePacket(character, FUNC_WAIT, 0, 0, 0, 0, 0, -1.0f);

						return;
		}
#ifdef ENALBE_MOUNT_SECTREE_UPDATE_RAZOR93
		if (true == MountSystem::IsRiding(character))
		{
			ecs::SpatialService::UpdateSectree(g_registry, character);
		}
#endif
#ifdef ENABLE_CHECK_GHOSTMODE
		if (ecs::PlayerRuntime::IsPC(character) && CombatSystem::IsDead(character))
		{
			LOG_INFO("MOVE: {} trying to move as dead", ecs::PlayerRuntime::GetName(character).data());

			ecs::MovementSystem::Show(character, ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetZ(character));
			ecs::MovementSystem::Stop(character);
			return;
		}
#endif

		uint32_t dwCurTime = get_dword_time();
		if (ecs::PlayerRuntime::GetDesc(character)) {
			bool CheckSpeedHack = (false == ecs::PlayerRuntime::GetDesc(character)->IsHandshaking() && dwCurTime - ecs::PlayerRuntime::GetDesc(character)->GetClientTime() > 7000);
			if (CheckSpeedHack)
			{
				int iDelta = (int)(dwCurTime - pinfo->dwTime);
				int iServerDelta = (int)(dwCurTime - ecs::PlayerRuntime::GetDesc(character)->GetClientTime());
				if (iDelta >= 30000) {
					LOG_INFO("SPEEDHACK: slow timer name {} delta {}", ecs::PlayerRuntime::GetName(character).data(), iDelta);
					ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(3);
				} else if (iDelta < -(iServerDelta / 50)) {
					LOG_INFO("SPEEDHACK: DETECTED! {} (delta {} {})", ecs::PlayerRuntime::GetName(character).data(), iDelta, iServerDelta);
					ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(3);
				}
			}

			//if (pinfo->bFunc == FUNC_COMBO && g_bCheckMultiHack)
			//{
			//}
		}
	}

	// migrated from CHARACTER::Move
	entt::entity e = (ecs::IsCharacter(character) && ecs::PlayerRuntime::GetDesc(character)) ? ecs::PlayerRuntime::GetDesc(character)->GetEntity() : entt::null;
	if (e != entt::null && g_registry.valid(e))
	{
		g_registry.emplace_or_replace<ecs::MovementDestination>(e, static_cast<int32_t>(pinfo->lX), static_cast<int32_t>(pinfo->lY));
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}
	// DUAL-PATH: ECS + legacy call
	if (pinfo->bFunc == FUNC_MOVE)
	{
		if (ecs::PointSystem::GetLimitPoint(character, POINT_MOV_SPEED) == 0)
			return;

		ecs::MovementSystem::SetRotation(character, pinfo->bRot * 5.0f);
		ecs::MovementSystem::ResetStopTime(character);

		ecs::MovementSystem::Goto(character, pinfo->lX, pinfo->lY);
	}
	else
	{
		if (pinfo->bFunc == FUNC_ATTACK || pinfo->bFunc == FUNC_COMBO)
		{
			ecs::MovementSystem::OnMove(character, true);
		}
		else if (pinfo->bFunc & FUNC_SKILL)
		{
			const int MASK_SKILL_MOTION = 0x7F;
			unsigned int motion = pinfo->bFunc & MASK_SKILL_MOTION;

			if (!SkillSystem::IsUsableSkillMotion(character, motion))
			{
				ecs::PlayerRuntime::GetDesc(character)->DelayedDisconnect(number(150, 500));
			}

			ecs::MovementSystem::OnMove(character);
		}

		ecs::MovementSystem::SetRotation(character, pinfo->bRot * 5.0f);
		ecs::MovementSystem::ResetStopTime(character);

		ecs::MovementSystem::Move(character, pinfo->lX, pinfo->lY);
		ecs::MovementSystem::Stop(character);
	}

	TPacketGCMove pack;

	pack.bHeader      = HEADER_GC_MOVE;
	pack.bFunc        = pinfo->bFunc;
	pack.bArg         = pinfo->bArg;
	pack.bRot         = pinfo->bRot;
	pack.dwVID        = ecs::PlayerRuntime::GetPacketVID(character);
	pack.lX           = pinfo->lX;
	pack.lY           = pinfo->lY;
	pack.dwTime       = pinfo->dwTime;
	pack.dwDuration   = (pinfo->bFunc == FUNC_MOVE) ? ecs::MovementSystem::GetCurrentMoveDuration(character) : 0;

	ecs::ViewSystem::PacketView(character, &pack, sizeof(TPacketGCMove), character);
	/*
	LOG_INFO(
			"MOVE: {} Func:{} Arg:{} Pos:{}x{} Time:{} Dist:{:.1f}",
			ecs::PlayerRuntime::GetName(character).data(),
			pinfo->bFunc,
			pinfo->bArg,
			pinfo->lX / 100,
			pinfo->lY / 100,
			pinfo->dwTime,
			fDist);
	*/
}

int CInputMain::SyncPosition(entt::entity character, const char * c_pcData, uint64_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate SyncPosition handler ECS
// DUAL-PATH: legacy only during migration window
	const TPacketCGSyncPosition* pinfo = reinterpret_cast<const TPacketCGSyncPosition*>( c_pcData );

	if (uiBytes < pinfo->wSize)
		return -1;

	int iExtraLen = pinfo->wSize - sizeof(TPacketCGSyncPosition);

	if (iExtraLen < 0)
	{
		LOG_ERROR("invalid packet length (len {} size {} buffer {})", iExtraLen, pinfo->wSize, uiBytes);
		ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);
		return -1;
	}

	if (0 != (iExtraLen % sizeof(TPacketCGSyncPositionElement)))
	{
		LOG_ERROR("invalid packet length {} (name: {})", pinfo->wSize, ecs::PlayerRuntime::GetName(character).data());
		return iExtraLen;
	}

	int iCount = iExtraLen / sizeof(TPacketCGSyncPositionElement);

	if (iCount <= 0)
		return iExtraLen;

	static const int nCountLimit = 60;

	if( iCount > nCountLimit )
	{
		//LogManager::instance().HackLog( "SYNC_POSITION_HACK", ch );
		LOG_ERROR("Too many SyncPosition Count({}) from Name({})", iCount, ecs::PlayerRuntime::GetName(character).data());
		iCount = nCountLimit;
	}

	TEMP_BUFFER tbuf;
	LPBUFFER lpBuf = tbuf.getptr();

	TPacketGCSyncPosition * pHeader = (TPacketGCSyncPosition *) buffer_write_peek(lpBuf);
	buffer_write_proceed(lpBuf, sizeof(TPacketGCSyncPosition));

	const TPacketCGSyncPositionElement* e =
		reinterpret_cast<const TPacketCGSyncPositionElement*>(c_pcData + sizeof(TPacketCGSyncPosition));

	timeval tvCurTime;
	gettimeofday(&tvCurTime, nullptr);

	for (int i = 0; i < iCount; ++i, ++e)
	{
		const entt::entity victimEntity = CHARACTER_MANAGER::instance().FindEntity(e->dwVID);


		if (!ecs::IsCharacter(victimEntity))
			continue;

		switch (ecs::PlayerRuntime::GetCharType(victimEntity))
		{
			case CHAR_TYPE_NPC:
			case CHAR_TYPE_WARP:
			case CHAR_TYPE_GOTO:
				continue;
		}

		if (!NetworkSyncSystem::SetSyncOwner(victimEntity, character))
			continue;

		const float fDistWithSyncOwner = DISTANCE_SQRT( (ecs::PlayerRuntime::GetX(victimEntity) - ecs::PlayerRuntime::GetX(character)) / 100, (ecs::PlayerRuntime::GetY(victimEntity) - ecs::PlayerRuntime::GetY(character)) / 100 );
		static constexpr float fLimitDistWithSyncOwner = 2500.f + 1000.f;

		if (fDistWithSyncOwner > fLimitDistWithSyncOwner)
		{
			if (ecs::PlayerRuntime::GetSyncHackCount(character) < 60){
				ecs::PlayerRuntime::SetSyncHackCount(character, ecs::PlayerRuntime::GetSyncHackCount(character) + 1);
				continue;
			} else{
				LogManager::instance().HackLog( "SYNC_POSITION_HACK", character );

				LOG_ERROR("Too far SyncPosition DistanceWithSyncOwner({})({}) from Name({}) CH({},{}) VICTIM({},{}) SYNC({},{})", fDistWithSyncOwner, ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), e->lX, e->lY);

				ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);

				return -1;
			}
		}

		const float fDist = DISTANCE_SQRT( (ecs::PlayerRuntime::GetX(victimEntity) - e->lX) / 100, (ecs::PlayerRuntime::GetY(victimEntity) - e->lY) / 100 );


		static constexpr int32_t g_lValidSyncInterval = 50 * 1000;
		const timeval& tvLastSyncTime = ecs::PlayerRuntime::GetLastSyncTime(victimEntity);
		timeval* tvDiff = timediff(&tvCurTime, &tvLastSyncTime);

		if (tvDiff->tv_sec == 0 && tvDiff->tv_usec < g_lValidSyncInterval)
		{
			if (ecs::PlayerRuntime::GetSyncHackCount(character) < 60)
			{
				ecs::PlayerRuntime::SetSyncHackCount(character, ecs::PlayerRuntime::GetSyncHackCount(character) + 1);
				continue;
			}
			else
			{
				LogManager::instance().HackLog("SYNC_POSITION_HACK", character);

				LOG_ERROR("Too often SyncPosition Interval({}ms)({}) from Name({}) VICTIM({},{}) SYNC({},{})", tvDiff->tv_sec * 1000 + tvDiff->tv_usec / 1000, ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), e->lX, e->lY);

				ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);

				return -1;
			}
		}
		else if( fDist > 40.0f ){

			LogManager::instance().HackLog( "SYNC_POSITION_HACK", character );

			LOG_ERROR("Too far SyncPosition Distance({})({}) from Name({}) CH({},{}) VICTIM({},{}) SYNC({},{})", fDist, ecs::PlayerRuntime::GetName(victimEntity).data(), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetX(victimEntity), ecs::PlayerRuntime::GetY(victimEntity), e->lX, e->lY);

			ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_CLOSE);

			return -1;
		} else{
			ecs::PlayerRuntime::SetLastSyncTime(victimEntity, tvCurTime);
			ecs::MovementSystem::Sync(victimEntity, e->lX, e->lY);
			buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
		}
	}

	if (buffer_size(lpBuf) != sizeof(TPacketGCSyncPosition))
	{
		pHeader->bHeader = HEADER_GC_SYNC_POSITION;
		pHeader->wSize = buffer_size(lpBuf);

		ecs::ViewSystem::PacketView(character, buffer_read_peek(lpBuf), buffer_size(lpBuf), character);
	}

	return iExtraLen;
}

void CInputMain::FlyTarget(entt::entity character, const char * pcData, uint8_t bHeader)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate FlyTarget handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGFlyTargeting * p = (TPacketCGFlyTargeting *) pcData;
	CombatSystem::FlyTarget(character, p->dwTargetVID, p->x, p->y, bHeader);
}

void CInputMain::Target(entt::entity character, const char * pcData)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;
	TPacketCGTarget * p = (TPacketCGTarget *) pcData;

	const entt::entity buildingEntity = building::CManager::instance().FindObjectByVID(p->dwVID);

	if (buildingEntity != entt::null)
	{
		TPacketGCTarget pckTarget;
		pckTarget.header = HEADER_GC_TARGET;
		pckTarget.dwVID = p->dwVID;
		ecs::PlayerRuntime::GetDesc(character)->Packet(&pckTarget, sizeof(TPacketGCTarget));
	}
	else
		CombatSystem::SetTarget(character, CHARACTER_MANAGER::instance().FindEntity(p->dwVID));
}
