#include "../../stdafx.h"

#include "MovementSystem.hpp"

#include <cmath>

#include "../../char.h"
#include "../../char_manager.h"
#include "../../desc_client.h"
#include "../../packet.h"
#include "../../motion.h"
#include "../../vector.h"
#include "../../sectree_manager.h"
#include "../../regen.h"
#include "../../start_position.h"
#include "../../config.h"
#include "../../unique_item.h"
#include "../../utils.h"
#include "../../questmanager.h"
#include "../../mount_inventory_helper.h"
#include "../../party.h"
#include "../AIHelpers.hpp"
#include "../components/dirty_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/combat_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"

void EncodeMovePacket(TPacketGCMove& pack, uint32_t dwVID, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float bRot);
EVENTFUNC(recovery_event);

namespace
{
    inline entt::entity EcsEntityOf(const CHARACTER* ch)
    {
        if (!ch)
            return entt::null;

        return CVIDRegistry::Instance().Find(ch->GetVID());
    }

    inline bool HasCombatState(const CHARACTER* ch)
    {
        const entt::entity e = EcsEntityOf(ch);
        return e != entt::null && g_registry.valid(e) &&
            g_registry.all_of<ecs::CombatActiveTag>(e);
    }

    inline bool HasMoveState(const CHARACTER* ch)
    {
        const entt::entity e = EcsEntityOf(ch);
        return e != entt::null && g_registry.valid(e) &&
            g_registry.all_of<ecs::MovementDestination>(e);
    }

    inline bool HasIdleState(const CHARACTER* ch)
    {
        const entt::entity e = EcsEntityOf(ch);
        if (e == entt::null || !g_registry.valid(e))
            return true;

        return !g_registry.all_of<ecs::CombatActiveTag>(e) &&
            !g_registry.all_of<ecs::MovementDestination>(e);
    }

    inline void EnterIdleState(CHARACTER* ch)
    {
        const entt::entity e = EcsEntityOf(ch);
        if (e == entt::null || !g_registry.valid(e))
            return;

        g_registry.remove<ecs::CombatActiveTag>(e);
        g_registry.remove<ecs::CombatTarget>(e);
        g_registry.remove<ecs::MovementDestination>(e);
    }

    inline void EnterBattleState(CHARACTER* ch)
    {
        const entt::entity e = EcsEntityOf(ch);
        if (e == entt::null || !g_registry.valid(e))
            return;

        g_registry.emplace_or_replace<ecs::CombatActiveTag>(e);
    }
}

void MovementSystem_Update(entt::registry& reg, uint32_t tick)
{
    // migrated from CHARACTER::Goto
    auto view = reg.view<ecs::Position, ecs::MovementDestination, ecs::MovementState>();

    view.each([&](const entt::entity entity, ecs::Position& position, ecs::MovementDestination& destination, ecs::MovementState& movementState) {
        const int32_t dx = destination.x - position.x;
        const int32_t dy = destination.y - position.y;

        if (dx == 0 && dy == 0) {
            movementState.lastMoveTime = tick;
            movementState.stopTime = tick;
            reg.remove<ecs::MovementDestination>(entity);
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
            return;
        }

        int32_t step = 200;
        if (const auto* movementSpeed = reg.try_get<ecs::MovementSpeed>(entity)) {
            step = std::max<int32_t>(1, movementSpeed->run);
        }

        const double distance = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
        if (distance <= step) {
            position.x = destination.x;
            position.y = destination.y;
            movementState.lastMoveTime = tick;
            movementState.stopTime = tick;
            movementState.moveDuration = 0;
            reg.remove<ecs::MovementDestination>(entity);
        } else {
            const double ratio = static_cast<double>(step) / distance;
            position.x += static_cast<int32_t>(std::round(dx * ratio));
            position.y += static_cast<int32_t>(std::round(dy * ratio));
            movementState.moveStartTime = tick;
            movementState.moveDuration = 1;
            movementState.lastMoveTime = tick;
            movementState.isWalking = true;
            movementState.isNowWalking = true;
        }

        reg.emplace_or_replace<ecs::DirtyTag>(entity);
        g_dispatcher.trigger(ecs::EvEntityMoved { entity, position.x, position.y });
    });
}

void CHARACTER::StartRecoveryEvent()
{
	if (m_pkRecoveryEvent)
		return;

	if (IsDead() || IsStun())
		return;

	if (IsNPC() && GetHP() >= GetMaxHP()) // ¸ó1oAÍ´Â A1·ÂAI ´U Â÷AÖA¸¸é 1AAU 3EÇN´U.
		return;


#ifdef ENABLE_MELEY_LAIR
	int32_t racenum = GetRaceNum();
	if (racenum == 6193 || racenum == 6118)
	{
		return;
	}
#endif

	char_event_info* info = AllocEventInfo<char_event_info>();

	info->ch = this;

	int iSec = IsPC() ? 3 : (std::max((uint8_t)1, GetMobTable().bRegenCycle));
	m_pkRecoveryEvent = event_create(recovery_event, info, PASSES_PER_SEC(iSec));
}

void CHARACTER::Standup()
{
	struct packet_position pack_position;

	if (!IsPosition(POS_SITTING))
		return;

	SetPosition(POS_STANDING);

	sys_log(1, "STANDUP: %s", GetName());

	pack_position.header = HEADER_GC_CHARACTER_POSITION;
	pack_position.vid = GetVID();
	pack_position.position = POSITION_GENERAL;

	PacketAround(&pack_position, sizeof(pack_position));
}

void CHARACTER::Sitdown(int is_ground)
{
	struct packet_position pack_position;

	if (IsPosition(POS_SITTING))
		return;

	SetPosition(POS_SITTING);
	sys_log(1, "SITDOWN: %s", GetName());

	pack_position.header = HEADER_GC_CHARACTER_POSITION;
	pack_position.vid = GetVID();
	pack_position.position = POSITION_SITTING_GROUND;
	PacketAround(&pack_position, sizeof(pack_position));
}

#ifdef ENABLE_ANCIENT_PYRAMID
void CHARACTER::SetRotation(float fRot, bool bForce)
#else
void CHARACTER::SetRotation(float fRot)
#endif
{
	if (!IsPC())
	{
		int32_t vnum = GetRaceNum();
#ifdef ENABLE_ANCIENT_PYRAMID
		if (vnum == PYRAMID_BOSSVNUM && (!bForce))
		{
			return;
		}
#endif

#ifdef __DEFENSE_WAVE__
		if (vnum >= 3960 && vnum <= 3962 && (!bForce))
		{
			return;
		}
#endif
	}

	m_pointsInstant.fRot = fRot;
}

// x, y 1aÇâA¸·Î o¸°í 1±´U.
void CHARACTER::SetRotationToXY(int32_t x, int32_t y)
{
	SetRotation(GetDegreeFromPositionXY(GetX(), GetY(), x, y));
}

bool CHARACTER::CannotMoveByAffect() const
{
	return (IsAffectFlag(AFF_STUN));
}

bool CHARACTER::CanMove() const
{
	if (CannotMoveByAffect())
		return false;

	if (GetMyShop())	// »óÁ! ?¬ »óAÂ?!1­´Â ?oÁ÷AI 1ö 3oA1
		return false;

	// 0.2AE AüAI¶ó¸é ?oÁ÷AI 1ö 3o´U.
	/*
	   if (get_float_time() - m_fSyncTime < 0.2f)
	   return false;
	 */
	return true;
}

// 1«Á¶°Ç x, y A§Ä!·Î AIµ? 1AA2´U.
bool CHARACTER::Sync(int32_t x, int32_t y)
{
	if (!GetSectree())
		return false;

	LPSECTREE new_tree = SECTREE_MANAGER::instance().Get(GetMapIndex(), x, y);

	if (!new_tree)
	{
		if (GetDesc())
		{
			sys_err("cannot find tree at %d %d (name: %s)", x, y, GetName());
			GetDesc()->SetPhase(PHASE_CLOSE);
		}
		else
		{
			sys_err("no tree: %s %d %d %d", GetName(), x, y, GetMapIndex());
			Dead();
		}

		return false;
	}

	SetRotationToXY(x, y);
	SetXYZ(x, y, 0);

	if (GetDungeon())
	{
		// ´oÁ—?ë AIoYA® 1Ó1o o—E­
		int iLastEventAttr = m_iEventAttr;
		m_iEventAttr = new_tree->GetEventAttribute(x, y);

		if (m_iEventAttr != iLastEventAttr)
		{
			if (GetParty())
			{
				quest::CQuestManager::instance().AttrOut(GetParty()->GetLeaderPID(), this, iLastEventAttr);
				quest::CQuestManager::instance().AttrIn(GetParty()->GetLeaderPID(), this, m_iEventAttr);
			}
			else
			{
				quest::CQuestManager::instance().AttrOut(GetPlayerID(), this, iLastEventAttr);
				quest::CQuestManager::instance().AttrIn(GetPlayerID(), this, m_iEventAttr);
			}
		}
	}

	if (GetSectree() != new_tree)
	{
		if (!IsNPC())
		{
			SECTREEID id = new_tree->GetID();
			SECTREEID old_id = GetSectree()->GetID();

			const float fDist = DISTANCE_SQRT(id.coord.x - old_id.coord.x, id.coord.y - old_id.coord.y);
			sys_log(0, "SECTREE DIFFER: %s %dx%d was %dx%d dist %.1fm",
				GetName(),
				id.coord.x,
				id.coord.y,
				old_id.coord.x,
				old_id.coord.y,
				fDist);
		}

		new_tree->InsertEntity(this);
	}

	return true;
}

void CHARACTER::Stop()
{
	if (!HasIdleState(this))
		MonsterLog("[IDLE] Á¤Áö");

	EnterIdleState(this);

	m_posDest.x = m_posStart.x = GetX();
	m_posDest.y = m_posStart.y = GetY();
}

bool CHARACTER::Goto(int32_t x, int32_t y)
{
	// TODO °A¸®A1A© ÇE?ä
	// °°Ao A§Ä!¸é AIµ?ÇO ÇE?ä 3oA1 (AÚµ? 1o°o)
	if (GetX() == x && GetY() == y)
		return false;

	if (!IsPC())
	{
		int32_t vnum = GetRaceNum();
#ifdef ENABLE_MELEY_LAIR
		if (vnum == 6193)
		{
			return false;
		}
#endif

#ifdef ENABLE_ANCIENT_PYRAMID
		if (vnum == PYRAMID_BOSSVNUM)
		{
			return false;
		}
#endif

#ifdef __DEFENSE_WAVE__
		if (vnum >= 3960 && vnum <= 3962)
		{
			return false;
		}
#endif
	}

	if (m_posDest.x == x && m_posDest.y == y)
	{
		if (!HasMoveState(this))
		{
			m_dwStateDuration = 4;
			const entt::entity e = EcsEntityOf(this);
			if (e != entt::null && g_registry.valid(e))
				g_registry.emplace_or_replace<ecs::MovementDestination>(e, static_cast<int32_t>(x), static_cast<int32_t>(y));
		}
		return false;
	}

	m_posDest.x = x;
	m_posDest.y = y;

	CalculateMoveDuration();

	m_dwStateDuration = 4;


	if (!HasMoveState(this)) {
		MonsterLog("[MOVE] %s", GetVictim() ? "´ë»óAßAu" : "±×3ÉAIµ?");
	}

	const entt::entity e = EcsEntityOf(this);
	if (e != entt::null && g_registry.valid(e))
		g_registry.emplace_or_replace<ecs::MovementDestination>(e, static_cast<int32_t>(x), static_cast<int32_t>(y));

	return true;
}


uint32_t CHARACTER::GetMotionMode() const
{
	uint32_t dwMode = MOTION_MODE_GENERAL;

	if (IsPolymorphed())
		return dwMode;

	LPITEM pkItem;

	if ((pkItem = GetWear(WEAR_WEAPON)))
	{
		switch (pkItem->GetProto()->bSubType)
		{
		case WEAPON_SWORD:
			dwMode = MOTION_MODE_ONEHAND_SWORD;
			break;

		case WEAPON_TWO_HANDED:
			dwMode = MOTION_MODE_TWOHAND_SWORD;
			break;

		case WEAPON_DAGGER:
			dwMode = MOTION_MODE_DUALHAND_SWORD;
			break;

		case WEAPON_BOW:
			dwMode = MOTION_MODE_BOW;
			break;

		case WEAPON_BELL:
			dwMode = MOTION_MODE_BELL;
			break;

		case WEAPON_FAN:
			dwMode = MOTION_MODE_FAN;
			break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case WEAPON_CLAW:
			dwMode = MOTION_MODE_CLAW;
			break;
#endif
		}
	}
	return dwMode;
}

float CHARACTER::GetMoveMotionSpeed() const
{
	uint32_t dwMode = GetMotionMode();
	if (!IsPC())
	{
		if (dwMode == 0)
		{
			int32_t vnum = GetRaceNum();
#ifdef ENABLE_MELEY_LAIR
			if (vnum == 6193)
			{
				return 100.0f;
			}
#endif

#ifdef ENABLE_ANCIENT_PYRAMID
			if (vnum == PYRAMID_BOSSVNUM)
			{
				return 100.0f;
			}
#endif

#ifdef __DEFENSE_WAVE__
			if (vnum >= 3960 && vnum <= 3962)
			{
				return 100.0f;
			}
#endif
		}
	}

	const CMotion* pkMotion = nullptr;

	if (!GetMountVnum())
		pkMotion = CMotionManager::instance().GetMotion(GetRaceNum(), MAKE_MOTION_KEY(dwMode, (IsWalking() && IsPC()) ? MOTION_WALK : MOTION_RUN));
	else
	{
		pkMotion = CMotionManager::instance().GetMotion(GetMountVnum(), MAKE_MOTION_KEY(MOTION_MODE_GENERAL, (IsWalking() && IsPC()) ? MOTION_WALK : MOTION_RUN));

		if (!pkMotion)
			pkMotion = CMotionManager::instance().GetMotion(GetRaceNum(), MAKE_MOTION_KEY(MOTION_MODE_HORSE, (IsWalking() && IsPC()) ? MOTION_WALK : MOTION_RUN));
	}

	if (pkMotion)
		return -pkMotion->GetAccumVector().y / pkMotion->GetDuration();
	else
	{
		if (test_server) {
			sys_err("cannot find motion (name %s race %d mode %d)", GetName(), GetRaceNum(), dwMode);
		}

		return 300.0f;
	}
}

float CHARACTER::GetMoveSpeed() const
{
	return GetMoveMotionSpeed() * 10000 / CalculateDuration(GetLimitPoint(POINT_MOV_SPEED), 10000);
}

void CHARACTER::CalculateMoveDuration()
{
	m_posStart.x = GetX();
	m_posStart.y = GetY();

	float fDist = DISTANCE_SQRT(m_posStart.x - m_posDest.x, m_posStart.y - m_posDest.y);

	float motionSpeed = GetMoveMotionSpeed();

	m_dwMoveDuration = CalculateDuration(GetLimitPoint(POINT_MOV_SPEED),
		(int)((fDist / motionSpeed) * 1000.0f));

	if (IsNPC())
		sys_log(1, "%s: GOTO: distance %f, spd %u, duration %u, motion speed %f pos %d %d -> %d %d",
			GetName(), fDist, GetLimitPoint(POINT_MOV_SPEED), m_dwMoveDuration, motionSpeed,
			m_posStart.x, m_posStart.y, m_posDest.x, m_posDest.y);

	m_dwMoveStartTime = get_dword_time();
}

// x y A§Ä!·Î AIµ? ÇN´U. (AIµ?ÇO 1ö AÖ´Â °! 3o´Â °!¸¦ E®AÎ ÇI°í Sync ¸?1Oµa·Î 1ÇÁ¦ AIµ? ÇN´U)
// 1­1ö´Â charAÇ x, y °aA» 1U·Î 1U2UÁö¸¸,
// A¬¶ó?!1­´Â AIAü A§Ä!?!1­ 1U2U x, y±îÁö interpolationÇN´U.
// °E°A3a ¶U´Â °ÍAo charAÇ m_bNowWalking?! ´?·ÁAÖ´U.
// Warp¸¦ AÇµµÇN °ÍAI¶ó¸é Show¸¦ »ç?ëÇO °Í.
bool CHARACTER::Move(int32_t x, int32_t y)
{
	// °°Ao A§Ä!¸é AIµ?ÇO ÇE?ä 3oA1 (AÚµ? 1o°o)
	if (GetX() == x && GetY() == y)
		return true;

	if (test_server)
		if (m_bDetailLog)
			sys_log(0, "%s position %u %u", GetName(), x, y);

	OnMove();
	return Sync(x, y);
}

void CHARACTER::SendMovePacket(uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float iRot)
{
	TPacketGCMove pack;

	if (bFunc == FUNC_WAIT)
	{
		x = m_posDest.x;
		y = m_posDest.y;
		dwDuration = m_dwMoveDuration;
	}

	if (iRot == -1.0f)
		EncodeMovePacket(pack, GetVID(), bFunc, bArg, x, y, dwDuration, dwTime, GetRotation() / 5.0f);
	else
		EncodeMovePacket(pack, GetVID(), bFunc, bArg, x, y, dwDuration, dwTime, iRot);
	PacketView(&pack, sizeof(TPacketGCMove), this);
}

void CHARACTER::MotionPacketEncode(uint8_t motion, LPCHARACTER victim, struct packet_motion* packet)
{
	packet->header = HEADER_GC_MOTION;
	packet->vid = m_vid;
	packet->motion = motion;

	if (victim)
		packet->victim_vid = victim->GetVID();
	else
		packet->victim_vid = 0;
}

void CHARACTER::Motion(uint8_t motion, LPCHARACTER victim)
{
	struct packet_motion pack_motion;
	MotionPacketEncode(motion, victim, &pack_motion);
	PacketAround(&pack_motion, sizeof(struct packet_motion));
}

EVENTFUNC(save_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		sys_err("save_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	sys_log(1, "SAVE_EVENT: %s", ch->GetName());
	ch->Save();
	ch->FlushDelayedSaveItem();
	return (save_event_second_cycle);
}


void CHARACTER::SetNowWalking(bool bWalkFlag)
{
    if (m_bNowWalking != bWalkFlag)
    {
        if (bWalkFlag)
        {
            m_bNowWalking = true;
            m_dwWalkStartTime = get_dword_time();
        }
        else
        {
            m_bNowWalking = false;
        }

        {
            TPacketGCWalkMode p;
            p.vid = GetVID();
            p.header = HEADER_GC_WALK_MODE;
            p.mode = m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;

            PacketView(&p, sizeof(p));
        }

        if (IsNPC())
        {
            if (m_bNowWalking)
                MonsterLog("°E´Â´U");
            else
                MonsterLog("¶Ú´U");
        }
    }
}

void CHARACTER::StartStaminaConsume()
{
    if (m_bStaminaConsume)
        return;
    PointChange(POINT_STAMINA, 0);
    m_bStaminaConsume = true;
    if (IsStaminaHalfConsume())
        ChatPacket(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec / 2, GetStamina());
    else
        ChatPacket(CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec, GetStamina());
}

void CHARACTER::StopStaminaConsume()
{
    if (!m_bStaminaConsume)
        return;
    PointChange(POINT_STAMINA, 0);
    m_bStaminaConsume = false;
    ChatPacket(CHAT_TYPE_COMMAND, "StopStaminaConsume %d", GetStamina());
}

bool CHARACTER::IsStaminaConsume() const
{
    return m_bStaminaConsume;
}

bool CHARACTER::IsStaminaHalfConsume() const
{
    return IsEquipUniqueItem(UNIQUE_ITEM_HALF_STAMINA);
}

void CHARACTER::ResetStopTime()
{
    m_dwStopTime = get_dword_time();
}

uint32_t CHARACTER::GetStopTime() const
{
    return m_dwStopTime;
}

void CHARACTER::GoHome()
{
    WarpSet(EMPIRE_START_X(GetEmpire()), EMPIRE_START_Y(GetEmpire()));
}
