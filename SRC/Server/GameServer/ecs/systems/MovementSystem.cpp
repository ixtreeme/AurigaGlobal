#include "../../stdafx.h"
#include "AffectSystem.hpp"

#include "PlayerRuntimeSystem.hpp"
#include "MovementSystem.hpp"

#include "PlayerRuntimeSystem.hpp"
#include <cmath>
#include <algorithm>

#include "../../char.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../char_manager.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../desc_client.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../dungeon.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../packet.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../motion.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../vector.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../sectree_manager.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../regen.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../start_position.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../config.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../unique_item.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../utils.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../questmanager.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../mount_inventory_helper.h"
#include "PlayerRuntimeSystem.hpp"
#include "../../party.h"
#include "PlayerRuntimeSystem.hpp"
#include "../CharacterAccessors.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../EntityFactory.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../Registry.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../AIHelpers.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "ItemSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "PointSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../SpatialHelpers.hpp"
#include "../PositionSync.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/dirty_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/identity_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/movement_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/status_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/transform_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/combat_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../components/character_runtime_components.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../events.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "../EventDispatcher.hpp"
#include "PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>

void EncodeMovePacket(TPacketGCMove& pack, uint32_t dwVID, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float bRot);
EVENTFUNC(recovery_event);

namespace
{
    inline entt::entity EcsEntityOf(const CHARACTER* ch)
    {
        if (!ch)
            return entt::null;

        return AIHelpers::EcsOf(const_cast<CHARACTER*>(ch));
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
    inline void TransitionAfterMovementStop(const ecs::VIDComponent& vid)
    {
        LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid.value);
        if (!ch || ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
            return;
        if (ch->GetVictim() && !ch->IsCoward())
            ch->SetPosition(POS_FIGHTING);
        else
            ch->SetPosition(POS_STANDING);
    }

    inline void MirrorLegacyMovement(entt::registry& reg, entt::entity entity, const ecs::Position& position)
    {
        const auto* legacy = reg.try_get<ecs::LegacyCharPtr>(entity);
        if (!legacy || !legacy->ptr)
            return;

        LPCHARACTER ch = legacy->ptr;

        // Phase 15E-final.LPENTITY.4-architect.B.1.1 atomic:
        // The previous early-return guard compared ch->GetX/GetY against the
        // freshly-advanced ECS `position`. Pre-flip GetX read legacy m_pos
        // which lagged the registry by one mutation; the guard avoided
        // redundant SetXYZ + UpdateSectree when both stores already agreed.
        // After the read flip GetX reads the SAME ECS Position component the
        // guard checks against, so the comparison would always evaluate true
        // and skip UpdateSectree. UpdateSectree refreshes m_map_view and
        // dispatches viewer transitions; skipping it caused the regression
        // user reported as "ket karakter nem latja a masik mozgasat" during
        // the first B.1.1 attempt (commits 9d27603 + fefce11, since reverted).
        //
        // Body now runs unconditionally on every ECS movement tick.
        // SetXYZ writes legacy m_pos (Phase C will remove that),
        // SyncPositionComponents re-emplaces the same Position values
        // already in the component (no-op in practice),
        // UpdateSectree is the work we actually need.
        ch->SetXYZ(position.x, position.y, ch->GetZ());
        ecs::SyncPositionComponents(reg, entity, ch->GetMapIndex(), position.x, position.y, ch->GetZ());
        ch->UpdateSectree();

        ecs::SyncSectorPlacement(reg, entity, ch->GetMapIndex(), ch->GetX(), ch->GetY());
    }
}

namespace ecs::MovementSystem {

namespace {

LPCHARACTER CharacterOf(entt::entity e)
{
    return ecs::LegacyCharOf(e);
}

bool IsValid(entt::entity e)
{
    return e != entt::null && g_registry.valid(e);
}

void MarkDirty(entt::entity e)
{
    if (IsValid(e))
        g_registry.emplace_or_replace<ecs::DirtyTag>(e);
}

} // namespace

bool Show(entt::entity e, int32_t mapIndex, int32_t x, int32_t y, int32_t z, bool showSpawnMotion)
{
    if (!IsValid(e))
        return false;

    auto* ch = CharacterOf(e);
    if (!ch)
        return false;

    const int32_t resolvedZ = (z == LONG_MAX) ? ch->GetZ() : z;
    ecs::SyncPositionComponents(g_registry, e, mapIndex, x, y, resolvedZ);

    return ch->Show(mapIndex, x, y, z, showSpawnMotion);
}

bool WarpSet(entt::entity e, int32_t x, int32_t y, int32_t privateMapIndex)
{
    if (!IsValid(e))
        return false;

    auto* ch = CharacterOf(e);
    if (!ch)
        return false;

    auto& warp = g_registry.get_or_emplace<ecs::WarpPosition>(e);
    warp.x = x;
    warp.y = y;
    warp.mapIndex = privateMapIndex ? privateMapIndex : ecs::PlayerRuntime::GetMapIndex(e);
    MarkDirty(e);

    return ch->WarpSet(x, y, privateMapIndex);
}

void ExitToSavedLocation(entt::entity e)
{
    if (!IsValid(e))
        return;

    auto* ch = CharacterOf(e);
    if (!ch)
        return;

    if (const auto* exit = g_registry.try_get<ecs::ExitPosition>(e)) {
        auto& warp = g_registry.get_or_emplace<ecs::WarpPosition>(e);
        warp.x = exit->x;
        warp.y = exit->y;
        warp.mapIndex = exit->mapIndex;
        MarkDirty(e);
    }

    ch->ExitToSavedLocation();

    if (auto* exit = g_registry.try_get<ecs::ExitPosition>(e)) {
        exit->x = 0;
        exit->y = 0;
        exit->mapIndex = 0;
        MarkDirty(e);
    }
}

bool Move(entt::entity e, int32_t x, int32_t y)
{
    if (!IsValid(e))
        return false;

    auto* ch = CharacterOf(e);
    if (!ch)
        return false;

    return ch->Move(x, y);
}

void OnMove(entt::entity e, bool isAttack)
{
    if (!IsValid(e))
        return;

    auto* ch = CharacterOf(e);
    if (!ch)
        return;

    ch->OnMove(isAttack);
}

bool Goto(entt::entity e, int32_t x, int32_t y)
{
    if (!IsValid(e))
        return false;

    auto* ch = CharacterOf(e);
    if (!ch)
        return false;

    g_registry.emplace_or_replace<ecs::MovementDestination>(e, x, y);
    MarkDirty(e);

    return ch->Goto(x, y);
}

void Stop(entt::entity e)
{
    if (!IsValid(e))
        return;

    auto* ch = CharacterOf(e);
    if (!ch)
        return;

    if (g_registry.all_of<ecs::MovementDestination>(e))
        g_registry.remove<ecs::MovementDestination>(e);
    MarkDirty(e);

    ch->Stop();
}

// LPENTITY.4-fixup helpers: mirror legacy CHARACTER movement-field writes
// into the parallel ECS components. Each helper is safe to call with a
// null/invalid entity. They patch existing components when available and
// emplace_or_replace when not, except SyncDestinationClear which removes
// MovementDestination outright (its absence is the canonical "no active
// movement" state). See docs/ecs_migration/phase15e_final_lpentity_4_fixup_audit.txt.

void SyncDestinationWrite(entt::entity e, int32_t x, int32_t y)
{
    if (!IsValid(e))
        return;

    g_registry.emplace_or_replace<ecs::MovementDestination>(e, x, y);
}

void SyncDestinationClear(entt::entity e)
{
    if (!IsValid(e))
        return;

    if (g_registry.all_of<ecs::MovementDestination>(e))
        g_registry.remove<ecs::MovementDestination>(e);

    if (auto* state = g_registry.try_get<ecs::MovementState>(e))
    {
        state->moveStartTime = 0;
        state->moveDuration = 0;
    }
}

void SyncTimingWrite(entt::entity e, uint32_t startTime, uint32_t duration)
{
    if (!IsValid(e))
        return;

    if (auto* state = g_registry.try_get<ecs::MovementState>(e))
    {
        state->moveStartTime = startTime;
        state->moveDuration = duration;
    }
}

void SyncWalkingWrite(entt::entity e, bool isNowWalking)
{
    if (!IsValid(e))
        return;

    if (auto* state = g_registry.try_get<ecs::MovementState>(e))
        state->isNowWalking = isNowWalking;
}

} // namespace ecs::MovementSystem

void MovementSystem_Update(entt::registry& reg, uint32_t tick)
{
    // During the migration window, only process entities with an active movement destination.
    auto view = reg.view<ecs::MovementDestination, ecs::VIDComponent, ecs::Position, ecs::MovementState>();

    view.each([&](const entt::entity entity,
                  ecs::MovementDestination& destination,
                  const ecs::VIDComponent& vid,
                  ecs::Position& position,
                  ecs::MovementState& movementState) {
        (void)vid;
        const int32_t dx = destination.x - position.x;
        const int32_t dy = destination.y - position.y;

        if (dx == 0 && dy == 0) {
            movementState.lastMoveTime = tick;
            movementState.stopTime = tick;
            movementState.moveDuration = 0;
            movementState.isWalking = false;
            movementState.isNowWalking = false;
            reg.remove<ecs::MovementDestination>(entity);
            MirrorLegacyMovement(reg, entity, position);
            reg.emplace_or_replace<ecs::DirtyTag>(entity);
            TransitionAfterMovementStop(vid);
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
            movementState.isWalking = false;
            movementState.isNowWalking = false;
            reg.remove<ecs::MovementDestination>(entity);
            MirrorLegacyMovement(reg, entity, position);
            TransitionAfterMovementStop(vid);
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

        MirrorLegacyMovement(reg, entity, position);
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

	if (IsNPC() && GetHP() >= GetMaxHP()) // ��1oAʹ?A1��AI �U ��A?��?1AAU 3E?�U.
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

	LOG_INFO("STANDUP: {}", GetName());

	pack_position.header = HEADER_GC_CHARACTER_POSITION;
	pack_position.vid = GetPacketVID();
	pack_position.position = POSITION_GENERAL;

	PacketAround(&pack_position, sizeof(pack_position));
}

void CHARACTER::Sitdown(int is_ground)
{
	struct packet_position pack_position;

	if (IsPosition(POS_SITTING))
		return;

	SetPosition(POS_SITTING);
	LOG_INFO("SITDOWN: {}", GetName());

	pack_position.header = HEADER_GC_CHARACTER_POSITION;
	pack_position.vid = GetPacketVID();
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

		if (auto* runtime = ecs::TryGetRuntimeFlags(EcsEntityOf(this)))
		runtime->rotation = fRot;

	if (const entt::entity e = EcsEntityOf(this); e != entt::null && g_registry.valid(e))
	{
		if (auto* rotation = g_registry.try_get<ecs::RotationComponent>(e))
			rotation->yaw = fRot;
	}
}

// x, y 1a��A��?o��?1��U.
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

	if (GetMyShop())	// ��? ??��A?!1???o��AI 1?3oA1
		return false;

	// 0.2AE A?I��� ?o��AI 1?3o�U.
	/*
	   if (get_float_time() - m_fSyncTime < 0.2f)
	   return false;
	 */
	return true;
}

// 1����?x, y A��!�� AI? 1AA2�U.
bool CHARACTER::Sync(int32_t x, int32_t y)
{
	if (!GetSectree())
		return false;

	LPSECTREE new_tree = ecs::SectorAt(GetMapIndex(), x, y);

	if (!new_tree)
	{
		if (GetDesc())
		{
			LOG_ERROR("cannot find tree at {} {} (name: {})", x, y, GetName());
			GetDesc()->SetPhase(PHASE_CLOSE);
		}
		else
		{
			LOG_ERROR("no tree: {} {} {} {}", GetName(), x, y, GetMapIndex());
			Dead();
		}

		return false;
	}

	SetRotationToXY(x, y);
	SetXYZ(x, y, 0);
	ecs::SyncPositionComponents(g_registry, EcsEntityOf(this), GetMapIndex(), x, y, GetZ());

	// LPENTITY.4 sync drift fix: peer-sync overrides whatever destination the
	// previous Goto/Move had recorded. Without this, EncodeInsertPacket reads
	// the stale m_posDest, sees (m_posDest != current pos) with iDur <= 0 (the
	// recorded move expired), and snaps pack.x/y BACK to the stale m_posDest.
	// New viewers entering range then render the character at the prior
	// destination instead of the synced position. The two-client desync the
	// user reported (chars adjacent on one client, far apart on another)
	// reproduces from this exact divergence: client A saw the move complete,
	// sync tracked the actual position; client B never received an updated
	// MOVE packet beyond the original Goto, then a reencode/insert produced
	// pack at m_posDest = old destination.
	m_posDest.x = x;
	m_posDest.y = y;
	m_posStart.x = x;
	m_posStart.y = y;
	ecs::MovementSystem::SyncDestinationClear(EcsEntityOf(this));

	if (GetDungeon())
	{
		// Sync quest event attr transitions when entering a new dungeon sector.
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
			const auto newX = id.coord.x;
			const auto newY = id.coord.y;
			const auto oldX = old_id.coord.x;
			const auto oldY = old_id.coord.y;
			LOG_INFO("SECTREE DIFFER: {} {}x{} was {}x{} dist {:.1f}m", GetName(), newX, newY, oldX, oldY, fDist);
		}

		new_tree->InsertEntity(this);

		const entt::entity e = EcsEntityOf(this);
		ecs::SyncSectorPlacement(g_registry, e, GetMapIndex(), GetX(), GetY());
		if (e != entt::null && g_registry.valid(e))
			g_registry.emplace_or_replace<ecs::ViewActiveTag>(e);
	}

	return true;
}

void CHARACTER::Stop()
{
	if (!HasIdleState(this))
		MonsterLog("[IDLE] stop");
	EnterIdleState(this);
	if (!IsPC())
		GotoState(m_stateIdle);

	m_posDest.x = m_posStart.x = GetX();
	m_posDest.y = m_posStart.y = GetY();

	// LPENTITY.4-fixup.2.b: stop clears active movement; remove ECS
	// MovementDestination and zero MovementState timing so native dispatch
	// does not encode a phantom move on subsequent inserts.
	ecs::MovementSystem::SyncDestinationClear(EcsEntityOf(this));
}

bool CHARACTER::Goto(int32_t x, int32_t y)
{
	// TODO �A��A1A????	// ��Ao A��!�� AI?? ???3oA1 (Aڵ? 1o�o)
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

	// B.1.4: read via getter (ECS MovementDestination, fallback to GetX/Y).
	if (GetCurrentDestX() == x && GetCurrentDestY() == y)
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
		MonsterLog("[MOVE] %s", GetVictim() ? "���A?u" : "��3?I?");
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
		const TItemTable* itemProto = ItemSystem::GetItemProto(EntityFactory::CreateItemEntity(g_registry, pkItem));
		if (!itemProto)
			return dwMode;

		switch (itemProto->bSubType)
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
			LOG_ERROR("cannot find motion (name {} race {} mode {})", GetName(), GetRaceNum(), dwMode);
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
		LOG_TRACE("{}: GOTO: distance {:f}, spd {}, duration {}, motion speed {:f} pos {} {} -> {} {}", GetName(), fDist, GetLimitPoint(POINT_MOV_SPEED), GetCurrentMoveDuration(), motionSpeed, m_posStart.x, m_posStart.y, m_posDest.x, m_posDest.y);

	m_dwMoveStartTime = get_dword_time();

	// LPENTITY.4-fixup.2.a: mirror legacy timing into ECS MovementState so
	// EntityNetworkDispatch::SendCharacterInsert observes consistent state.
	ecs::MovementSystem::SyncTimingWrite(EcsEntityOf(this), m_dwMoveStartTime, m_dwMoveDuration);
}

// x y A��!�� AI? ?�U. (AI?? 1?Aִ?? 3o�� ?�� E�A??�� Sync ?1O�a�� 1��?AI? ?�U)
// 1?��?charA?x, y �aA?1U�� 1U2U����,
// A��?!1??AIA?A��!?!1?1U2U x, y���� interpolation?�U.
// �E�A3a �U�� ��Ao charA?m_bNowWalking?! ?��AִU.
// Warp�� Aǵ��N ��AI��� Show�� ��?��O ��.
bool CHARACTER::Move(int32_t x, int32_t y)
{
	// ��Ao A��!�� AI?? ???3oA1 (Aڵ? 1o�o)
	if (GetX() == x && GetY() == y)
		return true;

	if (test_server)
		if (m_bDetailLog)
			LOG_TRACE("{} position {} {}", GetName(), x, y);

	OnMove();
	return Sync(x, y);
}

void CHARACTER::SendMovePacket(uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float iRot)
{
	TPacketGCMove pack;

	if (bFunc == FUNC_WAIT)
	{
		// B.1.4 + B.1.2: read via getters so the source is ECS.
		x = GetCurrentDestX();
		y = GetCurrentDestY();
		dwDuration = GetCurrentMoveDuration();
	}

	if (iRot == -1.0f)
		EncodeMovePacket(pack, GetPacketVID(), bFunc, bArg, x, y, dwDuration, dwTime, GetRotation() / 5.0f);
	else
		EncodeMovePacket(pack, GetPacketVID(), bFunc, bArg, x, y, dwDuration, dwTime, iRot);
	PacketView(&pack, sizeof(TPacketGCMove), this);
}

void CHARACTER::MotionPacketEncode(uint8_t motion, LPCHARACTER victim, struct packet_motion* packet)
{
	packet->header = HEADER_GC_MOTION;
	packet->vid = GetPacketVID();
	packet->motion = motion;

	if (victim)
		packet->victim_vid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(victim));
	else
		packet->victim_vid = 0;
}

void CHARACTER::Motion(uint8_t motion, LPCHARACTER victim)
{
	struct packet_motion pack_motion;
	MotionPacketEncode(motion, victim, &pack_motion);
	PacketAround(&pack_motion, sizeof(struct packet_motion));
}

// Phase 15E-final.LPENTITY.4-architect.B.1.2:
// CHARACTER::GetCurrentMoveDuration / GetCurrentMoveStartTime now read the
// ECS MovementState component as the authoritative source.
//
// Bootstrap (entity not yet ECS-registered, or MovementState absent):
// returns 0. Matches legacy bootstrap value from CHARACTER::Initialize
// (m_dwMoveStartTime / m_dwMoveDuration zero-init).
//
// Once the entity has been wired to ECS via AttachLegacyCharacter and the
// MovementState component emplaced by EntityFactory, the getters return
// the ECS values. Dual-write contract via the existing SyncTimingWrite
// helper at MovementSystem::CalculateMoveDuration keeps ECS in lockstep
// with legacy m_dwMoveStartTime / m_dwMoveDuration writes (Phase 4-fixup.2.a).
//
// Phase C will redirect writes; Phase G removes the legacy fields and the
// audit-only GetMoveStartTimeForAudit accessor.
uint32_t CHARACTER::GetCurrentMoveDuration() const
{
	const entt::entity e = AIHelpers::EcsOf(const_cast<CHARACTER*>(this));
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	if (const auto* state = g_registry.try_get<ecs::MovementState>(e))
		return state->moveDuration;
	return 0;
}

uint32_t CHARACTER::GetCurrentMoveStartTime() const
{
	const entt::entity e = AIHelpers::EcsOf(const_cast<CHARACTER*>(this));
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	if (const auto* state = g_registry.try_get<ecs::MovementState>(e))
		return state->moveStartTime;
	return 0;
}

// Phase 15E-final.LPENTITY.4-architect.B.1.3:
// Walk-mode read flip. IsNowWalking returns the pure ECS
// MovementState.isNowWalking flag; IsWalking adds the stamina-exhaustion
// fallback that legacy callers depend on (forced walk when stamina <= 0).
//
// Bootstrap returns false (state absent) - matches legacy m_bNowWalking
// zero-init in CHARACTER::Initialize. Stamina path reads via GetStamina
// which still pulls from legacy point storage.
bool CHARACTER::IsNowWalking() const
{
	const entt::entity e = AIHelpers::EcsOf(const_cast<CHARACTER*>(this));
	if (e == entt::null || !g_registry.valid(e))
		return false;
	if (const auto* state = g_registry.try_get<ecs::MovementState>(e))
		return state->isNowWalking;
	return false;
}

bool CHARACTER::IsWalking() const
{
	return IsNowWalking() || GetStamina() <= 0;
}

// Phase 15E-final.LPENTITY.4-architect.B.1.4:
// Destination read flip. GetCurrentDestX / GetCurrentDestY now read the
// ECS MovementDestination component as the authoritative source.
//
// Semantic note: ecs::MovementDestination is present only when the entity
// is actively moving (emplaced by Goto/Move, removed by Stop / arrival).
// Legacy m_posDest is always populated - Stop and similar settle paths
// set it to the current position. To preserve legacy parity, when the
// ECS component is absent we return current position via GetX/GetY.
// This matches what Stop() etc. used to do explicitly with m_posDest.
//
// Bootstrap (entity not yet ECS-registered): returns GetX/GetY which in
// turn returns 0 (per B.1.1) - matches legacy m_posDest zero-init.
//
// Phase C will redirect writes; Phase G removes the legacy field.
int32_t CHARACTER::GetCurrentDestX() const
{
	const entt::entity e = AIHelpers::EcsOf(const_cast<CHARACTER*>(this));
	if (e != entt::null && g_registry.valid(e))
	{
		if (const auto* dest = g_registry.try_get<ecs::MovementDestination>(e))
			return dest->x;
	}
	return GetX();
}

int32_t CHARACTER::GetCurrentDestY() const
{
	const entt::entity e = AIHelpers::EcsOf(const_cast<CHARACTER*>(this));
	if (e != entt::null && g_registry.valid(e))
	{
		if (const auto* dest = g_registry.try_get<ecs::MovementDestination>(e))
			return dest->y;
	}
	return GetY();
}

// Phase 15E-final.LPENTITY.4-architect.B.1.5:
// GetAddChrStateFlag composes the 4-bit bStateFlag byte from the ECS
// StatusFlags component. The 4 bits map 1:1 onto separate bool fields:
//   ADD_CHARACTER_STATE_DEAD   <-> StatusFlags.isDead
//   ADD_CHARACTER_STATE_SPAWN  <-> StatusFlags.isSpawnState
//   ADD_CHARACTER_STATE_KILLER <-> StatusFlags.isKillerMode
//   ADD_CHARACTER_STATE_PARTY  <-> StatusFlags.isPartyState
//
// Bootstrap returns 0 (status absent) - matches legacy m_bAddChrState
// zero-init in CHARACTER::Initialize.
//
// Dual-write status: per A.1 §"Field 7" all 4 bits keep both legacy and
// ECS in sync. One known transient deviation: CombatSystem on-kill sets
// ECS isDead = true but the legacy DEAD bit only follows via the
// EvEntityDied -> SetPosition(POS_DEAD) chain. Documented in 4-fixup.2.f
// as acceptable; resolves automatically at Phase G when m_bAddChrState
// deletes.
uint8_t CHARACTER::GetAddChrStateFlag() const
{
	const entt::entity e = AIHelpers::EcsOf(const_cast<CHARACTER*>(this));
	if (e == entt::null || !g_registry.valid(e))
		return 0;
	const auto* status = g_registry.try_get<ecs::StatusFlags>(e);
	if (!status)
		return 0;
	uint8_t flag = 0;
	if (status->isDead)
		flag |= ADD_CHARACTER_STATE_DEAD;
	if (status->isSpawnState)
		flag |= ADD_CHARACTER_STATE_SPAWN;
	if (status->isKillerMode)
		flag |= ADD_CHARACTER_STATE_KILLER;
	if (status->isPartyState)
		flag |= ADD_CHARACTER_STATE_PARTY;
	return flag;
}

EVENTFUNC(save_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		LOG_ERROR("save_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	LOG_TRACE("SAVE_EVENT: {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
	const entt::entity saveEntity = AIHelpers::EcsOf(ch);
	if (saveEntity != entt::null)
		g_dispatcher.trigger(ecs::EvCharSaved { saveEntity });
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

        // LPENTITY.4-fixup.2.b: mirror walk-mode toggle into ECS
        // MovementState so native dispatch emits the correct WALKMODE.
        ecs::MovementSystem::SyncWalkingWrite(EcsEntityOf(this), m_bNowWalking);

        {
            TPacketGCWalkMode p;
            p.vid = GetPacketVID();
            p.header = HEADER_GC_WALK_MODE;
            p.mode = m_bNowWalking ? WALKMODE_WALK : WALKMODE_RUN;

            PacketView(&p, sizeof(p));
        }

        if (IsNPC())
        {
            if (m_bNowWalking)
                MonsterLog("�E�´U");
            else
                MonsterLog("�ڴU");
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
        ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec / 2, GetStamina());
    else
        ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_COMMAND, "StartStaminaConsume %d %d", STAMINA_PER_STEP * passes_per_sec, GetStamina());
}

void CHARACTER::StopStaminaConsume()
{
    if (!m_bStaminaConsume)
        return;
    PointChange(POINT_STAMINA, 0);
    m_bStaminaConsume = false;
    ecs::ChatSystem::Send(AIHelpers::EcsOf(this), CHAT_TYPE_COMMAND, "StopStaminaConsume %d", GetStamina());
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

void CHARACTER::SetPosition(int pos)
{
	if (pos == POS_STANDING)
	{
		REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_DEAD);
		// LPENTITY.4-fixup.2.f: POS_STANDING clears legacy SPAWN bit too;
		// previously only ECS isSpawnState was cleared (line below) which
		// left m_bAddChrState SPAWN set, drifting from native dispatch.
		REMOVE_BIT(m_bAddChrState, ADD_CHARACTER_STATE_SPAWN);
		if (auto* runtime = ecs::TryGetRuntimeFlags(EcsEntityOf(this)))
			REMOVE_BIT(runtime->instantFlag, INSTANT_FLAG_STUN);
		const auto e = EcsEntityOf(this);
		if (e != entt::null && g_registry.valid(e))
		{
			if (g_registry.all_of<ecs::DeadTag>(e))
				g_registry.remove<ecs::DeadTag>(e);
			if (g_registry.all_of<ecs::StunTag>(e))
				g_registry.remove<ecs::StunTag>(e);
			if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
			{
				status->isDead = false;
				status->isStunned = false;
				status->isSpawnState = false;
			}
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		event_cancel(&m_pkDeadEvent);
		event_cancel(&m_pkStunEvent);
	}
	else if (pos == POS_DEAD)
	{
		SET_BIT(m_bAddChrState, ADD_CHARACTER_STATE_DEAD);
		const auto e = EcsEntityOf(this);
		if (e != entt::null && g_registry.valid(e))
		{
			g_registry.emplace_or_replace<ecs::DeadTag>(e);
			if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
				status->isDead = true;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}
	}

	if (!IsStone() && !IsPC())
	{
		switch (pos)
		{
		case POS_FIGHTING:
			if (!HasCombatState(this))
				MonsterLog("[BATTLE] enter fighting state");

			EnterBattleState(this);
			GotoState(m_stateBattle);
			break;

		default:
			if (!HasIdleState(this))
				MonsterLog("[IDLE] enter idle state");

			EnterIdleState(this);
			GotoState(m_stateIdle);
			break;
		}
	}

	if (auto* runtime = ecs::TryGetRuntimeFlags(EcsEntityOf(this)))
		runtime->position = pos;
}

bool CHARACTER::IsPosition(int pos) const
{
	return GetPosition() == pos;
}

int CHARACTER::GetPosition() const
{
	if (const auto* runtime = ecs::TryGetRuntimeFlags(EcsEntityOf(this)))
		return runtime->position;

}

float CHARACTER::GetRotation() const
{
	if (const auto* runtime = ecs::TryGetRuntimeFlags(EcsEntityOf(this)))
		return runtime->rotation;

}

bool CHARACTER::IsAlive() const
{
	return GetPosition() != POS_DEAD;
}

const int aiRecoveryPercents[10] = { 1, 5, 5, 5, 5, 5, 5, 5, 5, 5 };

EVENTFUNC(recovery_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);
	if (info == nullptr)
	{
		LOG_ERROR("recovery_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = info->ch;

	if (ch == nullptr) {
		return 0;
	}

	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkRecoveryEvent
	if (!ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch)))
	{
		if (AffectSystem::IsAffectFlag(AIHelpers::EcsOf(ch), AFF_POISON))
			return PASSES_PER_SEC(std::max((uint8_t)1, ch->GetMobTable().bRegenCycle));

#ifdef ENABLE_WOLFMAN_CHARACTER
		if (AffectSystem::IsAffectFlag(AIHelpers::EcsOf(ch), AFF_BLEEDING))
			return PASSES_PER_SEC(MAX(1, ch->GetMobTable().bRegenCycle));
#endif

#ifdef ENABLE_DS_RUNE
		if (ch->GetMobTable().dwVnum == 3996) {
			LPDUNGEON target = ch->GetDungeon();
			if (target) {
				if (target->GetFlag("floor") == 5) {
					ch->DistributeSP(ch);
					if (ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) <= ch->GetHP())
						return PASSES_PER_SEC(3);

					int iPercent = 0;
					int iAmount = 0;

					{
						iPercent = 2;
						iAmount = 15 + (ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) * iPercent) / 100;
					}

					iAmount += (iAmount * ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_HP_REGEN)) / 100;
					LOG_TRACE("RECOVERY_EVENT: {} {} HP_REGEN {} HP +{}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iPercent, ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_HP_REGEN), iAmount);
					const entt::entity recoveryEntity = AIHelpers::EcsOf(ch);
					if (recoveryEntity != entt::null)
						g_dispatcher.trigger(ecs::EvRecovery { recoveryEntity, iAmount, 0 });
					ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, iAmount, false);
					return PASSES_PER_SEC(10);
				}
			}
		}
		else if (ch->GetMobTable().dwVnum == 8202) {
			LPDUNGEON target = ch->GetDungeon();
			if (target) {
				if (target->GetFlag("floor") == 1) {
					ch->DistributeSP(ch);
					if (ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) <= ch->GetHP())
						return PASSES_PER_SEC(3);

					int iPercent = 0;
					int iAmount = 0;

					{
						iPercent = 2;
						iAmount = 15 + (ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) * iPercent) / 100;
					}

					iAmount += (iAmount * ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_HP_REGEN)) / 100;
					LOG_TRACE("RECOVERY_EVENT: {} {} HP_REGEN {} HP +{}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iPercent, ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_HP_REGEN), iAmount);
					const entt::entity recoveryEntity = AIHelpers::EcsOf(ch);
					if (recoveryEntity != entt::null)
						g_dispatcher.trigger(ecs::EvRecovery { recoveryEntity, iAmount, 0 });
					ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, iAmount, false);
					return PASSES_PER_SEC(10);
				}
			}
		}
#endif

		if (!ch->IsDoor())
		{
			const int64_t hpGain = std::max(int64_t {1}, (static_cast<int64_t>(ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch))) * ch->GetMobTable().bRegenPercent) / 100);
			ch->MonsterLog("HP_REGEN +%d", hpGain);
			const entt::entity recoveryEntity = AIHelpers::EcsOf(ch);
			if (recoveryEntity != entt::null)
				g_dispatcher.trigger(ecs::EvRecovery { recoveryEntity, static_cast<int32_t>(hpGain), 0 });
			ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, hpGain);
		}

		if (ch->GetHP() >= ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)))
		{
			ch->m_pkRecoveryEvent = nullptr;
			return 0;
		}

		return PASSES_PER_SEC(std::max((uint8_t)1, ch->GetMobTable().bRegenCycle));
	}
	else
	{
		ch->CheckTarget();
		ch->UpdateKillerMode();

		if (AffectSystem::IsAffectFlag(AIHelpers::EcsOf(ch), AFF_POISON) == true)
		{
			return 3;
		}
#ifdef ENABLE_WOLFMAN_CHARACTER
		if (AffectSystem::IsAffectFlag(AIHelpers::EcsOf(ch), AFF_BLEEDING))
			return 3;
#endif
		int iSec = (get_dword_time() - ch->GetLastMoveTime()) / 3000;

		ch->DistributeSP(ch);

		if (ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) <= ch->GetHP())
			return PASSES_PER_SEC(3);

		int iPercent = 0;
		int iAmount = 0;

		{
			iPercent = aiRecoveryPercents[std::min(9, iSec)];
			iAmount = 15 + (ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) * iPercent) / 100;
		}

		iAmount += (iAmount * ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_HP_REGEN)) / 100;

		LOG_TRACE("RECOVERY_EVENT: {} {} HP_REGEN {} HP +{}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iPercent, ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_HP_REGEN), iAmount);

		const entt::entity recoveryEntity = AIHelpers::EcsOf(ch);
		if (recoveryEntity != entt::null)
			g_dispatcher.trigger(ecs::EvRecovery { recoveryEntity, iAmount, 0 });
		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, iAmount, false);
		return PASSES_PER_SEC(3);
	}
}
void EncodeMovePacket(TPacketGCMove& pack, uint32_t dwVID, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float bRot)
{
	pack.bHeader = HEADER_GC_MOVE;
	pack.bFunc = bFunc;
	pack.bArg = bArg;
	pack.dwVID = dwVID;
	pack.dwTime = dwTime ? dwTime : get_dword_time();
	pack.bRot = bRot;
	pack.lX = x;
	pack.lY = y;
	pack.dwDuration = dwDuration;
}

