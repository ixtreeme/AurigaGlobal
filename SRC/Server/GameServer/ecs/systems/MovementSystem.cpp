#include "../../stdafx.h"
#include "ViewSystem.hpp"
#include "VisibilitySystem.hpp"
#include "AffectSystem.hpp"

#include "PlayerRuntimeSystem.hpp"
#include "MovementSystem.hpp"
#include "AISystem.hpp"
#include "CombatSystem.hpp"
#include "../components/ai_components.hpp"
#include "../AIHelpers.hpp"

#include <cmath>
#include <algorithm>
#include <tuple>
#include <optional>
#include <vector>

#include "../../char.h"
#include "../../desc_client.h"
#include "../../dungeon.h"
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
#include "../CharacterAccessors.hpp"
#include "../EntityFactory.hpp"
#include "../Registry.hpp"
#include "ItemSystem.hpp"
#include "PointSystem.hpp"
#include "MountSystem.hpp"
#include "../SpatialHelpers.hpp"
#include "SocialSystem.hpp"
#include "AffectSystem.hpp"
#include "../PositionSync.hpp"
#include "../components/dirty_components.hpp"
#include "../components/identity_components.hpp"
#include "../components/movement_components.hpp"
#include "../components/status_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/combat_components.hpp"
#include "../components/character_runtime_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include <Core/Logging.hpp>

void EncodeMovePacket(TPacketGCMove& pack, uint32_t dwVID, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime, float bRot);
EVENTFUNC(recovery_event);

namespace
{
    inline bool HasCombatState(entt::entity e)
    {
        return e != entt::null && g_registry.valid(e) &&
            g_registry.all_of<ecs::CombatActiveTag>(e);
    }

    inline bool HasMoveState(entt::entity e)
    {
        return e != entt::null && g_registry.valid(e) &&
            g_registry.all_of<ecs::MovementDestination>(e);
    }

    inline bool HasIdleState(entt::entity e)
    {
        if (e == entt::null || !g_registry.valid(e))
            return true;

        return !g_registry.all_of<ecs::CombatActiveTag>(e) &&
            !g_registry.all_of<ecs::MovementDestination>(e);
    }

    inline void EnterIdleState(entt::entity e)
    {
        if (e == entt::null || !g_registry.valid(e))
            return;

        g_registry.remove<ecs::CombatActiveTag>(e);
        g_registry.remove<ecs::CombatTarget>(e);
        g_registry.remove<ecs::MovementDestination>(e);
    }

    inline void EnterBattleState(entt::entity e)
    {
        if (e == entt::null || !g_registry.valid(e))
            return;
        g_registry.emplace_or_replace<ecs::CombatActiveTag>(e);
    }
    void TransitionAfterMovementStop(entt::entity entity)
    {
        if (!g_registry.valid(entity) || ecs::PlayerRuntime::IsPC(entity) ||
            g_registry.any_of<ecs::DeadTag, ecs::StunTag>(entity))
            return;
        const bool fighting = CombatSystem::GetVictim(entity) != entt::null &&
            !AIHelpers::IsCoward(entity);
        ecs::PlayerRuntime::SetPosition(entity, fighting ? POS_FIGHTING : POS_STANDING);
    }

    auto MovementValues(const ecs::MovementState& s)
    {
        return std::tie(s.moveStartTime, s.moveDuration, s.lastMoveTime, s.lastAttackTime,
            s.walkStartTime, s.stopTime, s.isWalking, s.isNowWalking, s.staminaConsume,
            s.walkPreference, s.commandRevision);
    }

    // A command owns no component references across construction/destruction
    // signals. Nested movement commands and spatial replacement win.
    struct MotionCommand
    {
        entt::entity entity;
        std::optional<ecs::Position> position;
        int32_t map = 0;
        uint64_t spatialRevision = 0, revision = 0;
        bool started = false;

        explicit MotionCommand(entt::entity e) : entity(e)
        {
            if (!g_registry.valid(e) || !g_registry.all_of<ecs::CharacterType>(e)) return;
            if (auto* p = g_registry.try_get<ecs::Position>(e)) position = *p;
            if (auto* m = g_registry.try_get<ecs::MapIndex>(e)) map = m->value;
            if (auto* r = g_registry.try_get<ecs::SpatialRevision>(e)) spatialRevision = r->value;
            if (auto* s = g_registry.try_get<ecs::MovementState>(e)) revision = s->commandRevision;
            else g_registry.insert<ecs::MovementState>(&e, &e + 1);
            if (!Current()) return;
            g_registry.get<ecs::MovementState>(e).commandRevision = ++revision;
            started = true;
        }
        bool Current() const
        {
            if (!g_registry.valid(entity) || !g_registry.all_of<ecs::CharacterType>(entity)) return false;
            const auto* s = g_registry.try_get<ecs::MovementState>(entity);
            const auto* p = g_registry.try_get<ecs::Position>(entity);
            const auto* m = g_registry.try_get<ecs::MapIndex>(entity);
            const auto* r = g_registry.try_get<ecs::SpatialRevision>(entity);
            return s && s->commandRevision == revision &&
                (m ? m->value : 0) == map && (r ? r->value : 0) == spatialRevision &&
                (position ? p && p->x == position->x && p->y == position->y && p->z == position->z : !p);
        }
        template<class T> bool Prepare() const
        {
            if (!Current()) return false;
            if (!g_registry.all_of<T>(entity)) g_registry.insert<T>(&entity, &entity + 1);
            return Current() && g_registry.all_of<T>(entity);
        }
    };

    void WriteDestination(entt::entity e, int32_t x, int32_t y)
    {
        // Retarget in place, like timing writes (no on_update contract).
        // Single-entity insert publishes construction without emplace's
        // post-callback get: an observer may stop or destroy this mover.
        if (auto* destination = g_registry.try_get<ecs::MovementDestination>(e))
            *destination = {x, y};
        else
            g_registry.insert<ecs::MovementDestination>(&e, &e + 1, ecs::MovementDestination {x, y});
    }

    bool FixedMovementRace(uint32_t race)
    {
#ifdef ENABLE_MELEY_LAIR
        if (race == 6193) return true;
#endif
#ifdef ENABLE_ANCIENT_PYRAMID
        if (race == PYRAMID_BOSSVNUM) return true;
#endif
#ifdef __DEFENSE_WAVE__
        if (race >= 3960 && race <= 3962) return true;
#endif
        return false;
    }

    int MovementDurationFactor(entt::entity e)
    {
        // GetLimitPoint(POINT_MOV_SPEED) used the same instant-point authority.
        const auto speed = std::clamp<int64_t>(ecs::PointSystem::Get(e, POINT_MOV_SPEED), 0, 350);
        return speed <= 100 ? int(200 - speed) : int(10000 / speed);
    }

    uint32_t MovementDuration(entt::entity e, const ecs::Position& from, int32_t x, int32_t y)
    {
        const double distance = std::hypot(double(from.x) - x, double(from.y) - y);
        const float millis = (static_cast<float>(distance) / ecs::MovementSystem::GetMoveMotionSpeed(e)) * 1000.0f;
        // Keep normal float/truncation and integer-factor semantics, but never
        // cast infinity/out-of-range floats or overflow the duration product.
        const auto base = static_cast<uint64_t>(std::clamp<double>(millis, 0, INT_MAX));
        return static_cast<uint32_t>(std::min<uint64_t>(
            base * MovementDurationFactor(e) / 100, INT_MAX));
    }

    // Own values, never component references: publication can retire, respawn
    // or retarget this entity (or any later entity in the tick snapshot).
    struct MovementFrame
    {
        entt::entity entity;
        ecs::Position position;
        ecs::MovementDestination destination;
        ecs::MovementState state;
        int32_t mapIndex;
        uint64_t spatialRevision;
        int32_t step;
        bool moving = true;

        bool Current(entt::registry& reg) const
        {
            if (!reg.valid(entity) ||
                !reg.all_of<ecs::Position, ecs::MapIndex, ecs::MovementState,
                    ecs::SpatialRevision, ecs::SpatialEntity, ecs::CharacterType>(entity) ||
                reg.any_of<ecs::DeadTag, ecs::StunTag>(entity))
                return false;
            const auto& pos = reg.get<ecs::Position>(entity);
            const auto* dest = reg.try_get<ecs::MovementDestination>(entity);
            const auto* speed = reg.try_get<ecs::MovementSpeed>(entity);
            if (pos.x != position.x || pos.y != position.y || pos.z != position.z ||
                reg.get<ecs::MapIndex>(entity).value != mapIndex ||
                reg.get<ecs::SpatialRevision>(entity).value != spatialRevision ||
                MovementValues(reg.get<ecs::MovementState>(entity)) != MovementValues(state) ||
                (speed ? std::max<int32_t>(1, speed->run) : 200) != step ||
                (moving ? !dest || dest->x != destination.x || dest->y != destination.y : dest != nullptr))
                return false;
            auto* tree = ecs::SectorOf(reg, entity);
            return tree && !tree->IsDestroying() && tree->Contains(entity);
        }
    };

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

void SetWalkingPreference(entt::entity e, bool walking)
{
    if (IsValid(e))
        g_registry.get_or_emplace<ecs::MovementState>(e).walkPreference = walking;
}

bool GetWalkingPreference(entt::entity e)
{
    if (!IsValid(e))
        return false;
    const auto* movement = g_registry.try_get<ecs::MovementState>(e);
    return movement && movement->walkPreference;
}

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

void SaveExitLocation(entt::entity e)
{
    if (!IsValid(e))
        return;

    auto& exit = g_registry.get_or_emplace<ecs::ExitPosition>(e);
    exit.x = ecs::PlayerRuntime::GetX(e);
    exit.y = ecs::PlayerRuntime::GetY(e);
    exit.mapIndex = ecs::PlayerRuntime::GetMapIndex(e);
    MarkDirty(e);

    if (auto* ch = CharacterOf(e))
        ch->SaveExitLocation();
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
    if (!IsValid(e) || !g_registry.all_of<ecs::CharacterType, ecs::Position>(e))
        return false;
    const auto position = g_registry.get<ecs::Position>(e);
    if (position.x == x && position.y == y) return false;
    // Legacy IsPC is the attached-descriptor test, not TagPC (e.g. clones).
    if (!PlayerRuntime::GetDesc(e) && FixedMovementRace(PlayerRuntime::GetRaceNum(e)))
        return false;
    if (const auto* destination = g_registry.try_get<ecs::MovementDestination>(e);
        destination && destination->x == x && destination->y == y)
        return false;

    MotionCommand command(e);
    if (!command.started) return false;
    if (!command.Prepare<ecs::AIState>() || !command.Prepare<ecs::DirtyTag>()) return false;
    // Preparation observers may change equipment or speed without starting
    // another move. Calculate from the final component state, not a stale copy.
    const auto duration = MovementDuration(e, position, x, y);
    const auto now = get_dword_time();
    if (!command.Current() || !g_registry.all_of<ecs::AIState>(e)) return false;
    auto& state = g_registry.get<ecs::MovementState>(e);
    state.moveStartTime = now;
    state.moveDuration = duration;
    g_registry.get<ecs::AIState>(e).stateDuration = 4;
    // Observers of the destination see its matching timing. Reentry cannot
    // overwrite the nested command after this publication.
    WriteDestination(e, x, y);
    return command.Current() && g_registry.all_of<ecs::MovementDestination>(e) &&
        g_registry.get<ecs::MovementDestination>(e).x == x &&
        g_registry.get<ecs::MovementDestination>(e).y == y;
}

void Stop(entt::entity e)
{
    MotionCommand command(e);
    if (!command.started) return;
    const bool npc = !PlayerRuntime::GetDesc(e);
    if (!command.Prepare<ecs::DirtyTag>() || (npc && !command.Prepare<ecs::AIStateMachine>()))
        return;
    const bool wasIdle = HasIdleState(e);
    auto& state = g_registry.get<ecs::MovementState>(e);
    state.moveStartTime = state.moveDuration = 0;
    g_registry.remove<ecs::CombatActiveTag>(e);
    if (!command.Current()) return;
    g_registry.remove<ecs::CombatTarget>(e);
    if (!command.Current()) return;
    g_registry.remove<ecs::MovementDestination>(e);
    if (!command.Current()) return;
    if (npc) AISystem::GotoState(e, ecs::AIFSMState::Idle);
    if (!wasIdle && command.Current()) PlayerRuntime::MonsterLog(e, "[IDLE] stop");
}

// Component-only writes for sync/spawn paths; these also supersede older commands.

void SyncDestinationWrite(entt::entity e, int32_t x, int32_t y)
{
    MotionCommand command(e);
    if (command.started) WriteDestination(e, x, y);
}

void SyncDestinationClear(entt::entity e)
{
    MotionCommand command(e);
    if (!command.started) return;
    auto& state = g_registry.get<ecs::MovementState>(e);
    state.moveStartTime = state.moveDuration = 0;
    g_registry.remove<ecs::MovementDestination>(e);
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

void SetNowWalking(entt::entity e, bool walking)
{
    if (!IsValid(e))
        return;
    auto* state = g_registry.try_get<ecs::MovementState>(e);
    if (!state || state->isNowWalking == walking)
        return;
    if (walking)
        state->walkStartTime = get_dword_time();
    SyncWalkingWrite(e, walking);
    TPacketGCWalkMode packet {};
    packet.vid = ecs::PlayerRuntime::GetPacketVID(e);
    packet.header = HEADER_GC_WALK_MODE;
    packet.mode = walking ? WALKMODE_WALK : WALKMODE_RUN;
    ecs::ViewSystem::PacketView(e, &packet, sizeof(packet));
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
    // Sectree and gameplay services share the world registry. Do not partially
    // update a different registry through the global services.
    if (&reg != &g_registry)
        return;

    std::vector<MovementFrame> frames;
    auto view = reg.view<ecs::MovementDestination, ecs::Position, ecs::MapIndex,
        ecs::MovementState, ecs::SpatialRevision, ecs::SpatialEntity, ecs::CharacterType>();
    for (auto entity : view) {
        const auto* speed = reg.try_get<ecs::MovementSpeed>(entity);
        frames.push_back({entity, view.get<ecs::Position>(entity),
            view.get<ecs::MovementDestination>(entity), view.get<ecs::MovementState>(entity),
            view.get<ecs::MapIndex>(entity).value, view.get<ecs::SpatialRevision>(entity).value,
            speed ? std::max<int32_t>(1, speed->run) : 200});
    }

    for (auto frame : frames) {
        if (!frame.Current(reg))
            continue;
        const auto entity = frame.entity;
        const auto oldPosition = frame.position;
        const auto oldState = frame.state;
        // Widen before subtracting: opposite int32 endpoints must not overflow.
        const double dx = double(frame.destination.x) - oldPosition.x;
        const double dy = double(frame.destination.y) - oldPosition.y;
        const double distance = std::hypot(dx, dy);
        const bool arrived = distance <= frame.step;
        auto position = oldPosition;
        if (arrived) {
            position.x = frame.destination.x;
            position.y = frame.destination.y;
        } else {
            position.x = int32_t(double(oldPosition.x) + std::round(dx * frame.step / distance));
            position.y = int32_t(double(oldPosition.y) + std::round(dy * frame.step / distance));
        }

        auto* target = ecs::SectorAt(frame.mapIndex, position.x, position.y);
        if (!target || target->IsDestroying())
            continue; // Never commit coordinates without a live destination sector.

        if (!reg.all_of<ecs::DirtyTag>(entity))
            reg.insert<ecs::DirtyTag>(&entity, &entity + 1);
        if (!frame.Current(reg))
            continue;

        frame.position = position;
        frame.state.lastMoveTime = tick;
        frame.state.moveDuration = arrived ? 0 : 1;
        frame.state.isWalking = frame.state.isNowWalking = !arrived;
        if (arrived) frame.state.stopTime = tick;
        else frame.state.moveStartTime = tick;
        reg.get<ecs::Position>(entity) = position;
        reg.get<ecs::MovementState>(entity) = frame.state;

        if (ecs::SectorOf(reg, entity) != target) {
            if (!target->InsertEntity(entity)) {
                // Only roll back our own unpublished write. A callback's
                // replacement, despawn or new movement always takes precedence.
                if (frame.Current(reg)) {
                    reg.get<ecs::Position>(entity) = oldPosition;
                    reg.get<ecs::MovementState>(entity) = oldState;
                }
                continue;
            }
            ++frame.spatialRevision;
        }
        if (!frame.Current(reg))
            continue;
        if (arrived) {
            frame.moving = false;
            reg.remove<ecs::MovementDestination>(entity);
            if (!frame.Current(reg))
                continue;
        }

        const bool changed = position.x != oldPosition.x || position.y != oldPosition.y;
        if (changed) {
            // Publish once, after membership and movement state are committed.
            // Visibility subscribes here; observers see the real old/new values.
            g_dispatcher.trigger(ecs::PositionChangedEvent {entity,
                oldPosition.x, oldPosition.y, oldPosition.z, position.x, position.y, position.z,
                frame.mapIndex, frame.mapIndex});
        } else {
            ecs::VisibilitySystem::Refresh(reg, entity);
        }
        if (!frame.Current(reg))
            continue;
        if (changed) {
            g_dispatcher.trigger(ecs::EvEntityMoved {entity, position.x, position.y});
            if (!frame.Current(reg))
                continue;
        }
        if (arrived)
            TransitionAfterMovementStop(entity);
    }
}

namespace ecs::PlayerRuntime {

void StartRecoveryEvent(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	if (GetCharEvent(e, CharEvent::Recovery))
		return;

	if (CombatSystem::IsDead(e) || CombatSystem::IsStun(e))
		return;

	// CHARACTER::IsNPC() is m_bCharType != CHAR_TYPE_PC - monsters and stones
	// included - so this reads the CharacterType component, not TagNPC.
	const auto* type = g_registry.try_get<ecs::CharacterType>(e);
	const bool isNotPCType = type && type->value != CHAR_TYPE_PC;
	if (isNotPCType && ecs::PointSystem::Get(e, POINT_HP) >= ecs::PointSystem::GetMaxHP(e))
		return;

#ifdef ENABLE_MELEY_LAIR
	const uint32_t racenum = GetRaceNum(e);
	if (racenum == 6193 || racenum == 6118)
		return;
#endif

	char_event_info* info = AllocEventInfo<char_event_info>();
	info->ch = e;

	// CHARACTER::IsPC() is the descriptor test, as everywhere else here.
	// The regen cycle comes off the mob table MobDataRef already points at,
	// so no legacy object is needed for it.
	int iSec = 3;
	if (!GetDesc(e))
	{
		const auto* mob = g_registry.try_get<ecs::MobDataRef>(e);
		iSec = (mob && mob->data) ? std::max<uint8_t>(1, mob->data->m_table.bRegenCycle) : 1;
	}

	SetCharEvent(e, CharEvent::Recovery,
		event_create(recovery_event, info, PASSES_PER_SEC(iSec)));
}

} // namespace ecs::PlayerRuntime

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

	ecs::ViewSystem::PacketView(GetEntityHandle(), &pack_position, sizeof(pack_position));
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
	ecs::ViewSystem::PacketView(GetEntityHandle(), &pack_position, sizeof(pack_position));
}

#ifdef ENABLE_ANCIENT_PYRAMID
#ifdef ENABLE_ANCIENT_PYRAMID
void CHARACTER::SetRotation(float fRot, bool bForce)
{
	ecs::MovementSystem::SetRotation(GetEntityHandle(), fRot, bForce);
}
#else
void CHARACTER::SetRotation(float fRot)
{
	ecs::MovementSystem::SetRotation(GetEntityHandle(), fRot);
}
#endif

namespace ecs::MovementSystem {

void SetRotation(entt::entity e, float fRot, bool bForce)
#else
void SetRotation(entt::entity e, float fRot)
#endif
{
	if (ecs::PlayerRuntime::GetDesc(e) == nullptr)
	{
		int32_t vnum = ecs::PlayerRuntime::GetRaceNum(e);
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

		if (auto* runtime = ecs::TryGetRuntimeFlags(e))
		runtime->rotation = fRot;

	if (e != entt::null && g_registry.valid(e))
	{
		if (auto* rotation = g_registry.try_get<ecs::RotationComponent>(e))
			rotation->yaw = fRot;
	}
}

} // namespace ecs::MovementSystem

// x, y 1a��A��?o��?1��U.

namespace ecs::MovementSystem {
void SetRotationToXY(entt::entity e, int32_t x, int32_t y)
{
    SetRotation(e, GetDegreeFromPositionXY(
        ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), x, y));
}

// CHARACTER::CanMove was two reads and nothing else. Both have component
// accessors, so the body moves here whole rather than being wrapped.
bool CanMove(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return false;
    if (AffectSystem::IsAffectFlag(e, AFF_STUN))
        return false;
    if (ecs::SocialSystem::GetMyShop(e))
        return false;
    return true;
}
} // namespace ecs::MovementSystem

bool CHARACTER::CannotMoveByAffect() const
{
	return (AffectSystem::IsAffectFlag(GetEntityHandle(), AFF_STUN));
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
			CombatSystem::Dead(GetEntityHandle());
		}

		return false;
	}

	ecs::MovementSystem::SetRotationToXY(GetEntityHandle(), x, y);
	// Phase C.1: legacy m_pos write removed - SyncPositionComponents below
	// emplaces ECS Position as the sole source of truth.
	ecs::SyncPositionComponents(g_registry, GetEntityHandle(), GetMapIndex(), x, y, GetZ());

	// LPENTITY.4 sync drift fix: peer-sync overrides whatever destination the
	// previous Goto/Move had recorded. Without this, EncodeInsertPacket reads
	// the stale destination, sees (dest != current pos) with iDur <= 0 (the
	// recorded move expired), and snaps pack.x/y BACK to the stale destination.
	// New viewers entering range then render the character at the prior
	// destination instead of the synced position. The two-client desync the
	// user reported (chars adjacent on one client, far apart on another)
	// reproduces from this exact divergence: client A saw the move complete,
	// sync tracked the actual position; client B never received an updated
	// MOVE packet beyond the original Goto, then a reencode/insert produced
	// pack at the old destination.
	// Phase C.3: legacy destination field write removed. SyncDestinationClear
	// removes ECS MovementDestination so GetCurrentDestX/Y falls back to
	// GetX/GetY (current position via ECS Position) - same effective
	// semantic as legacy destination = current_pos.
	ecs::MovementSystem::SyncDestinationClear(GetEntityHandle());

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

		const entt::entity e = GetEntityHandle();
		new_tree->InsertEntity(e);
		if (e != entt::null && g_registry.valid(e))
			g_registry.emplace_or_replace<ecs::ViewActiveTag>(e);
		ecs::VisibilitySystem::Refresh(g_registry, e);
	}

	return true;
}

namespace ecs::MovementSystem {

uint32_t GetMotionMode(entt::entity e)
{
    if (!IsValid(e) || AffectSystem::IsPolymorphed(e)) return MOTION_MODE_GENERAL;
    const auto weapon = ItemSystem::GetWearItem(e, WEAR_WEAPON);
    const auto* proto = ItemSystem::IsValidItem(weapon) ? ItemSystem::GetItemProto(weapon) : nullptr;
    if (!proto) return MOTION_MODE_GENERAL;
    switch (proto->bSubType) {
    case WEAPON_SWORD: return MOTION_MODE_ONEHAND_SWORD;
    case WEAPON_TWO_HANDED: return MOTION_MODE_TWOHAND_SWORD;
    case WEAPON_DAGGER: return MOTION_MODE_DUALHAND_SWORD;
    case WEAPON_BOW: return MOTION_MODE_BOW;
    case WEAPON_BELL: return MOTION_MODE_BELL;
    case WEAPON_FAN: return MOTION_MODE_FAN;
    default: return MOTION_MODE_GENERAL;
    }
}

float GetMoveMotionSpeed(entt::entity e)
{
    if (!IsValid(e)) return 300.0f;
    const auto mode = GetMotionMode(e);
    const auto race = PlayerRuntime::GetRaceNum(e);
    const bool attached = PlayerRuntime::GetDesc(e) != nullptr;
    if (!attached && mode == MOTION_MODE_GENERAL && FixedMovementRace(race)) return 100.0f;
    const auto* movement = g_registry.try_get<ecs::MovementState>(e);
    const bool walking = attached && ((movement && movement->isNowWalking) || PlayerRuntime::GetStamina(e) <= 0);
    const auto motionIndex = walking ? MOTION_WALK : MOTION_RUN;
    const auto mount = MountSystem::GetMountVnum(e);
    const CMotion* motion = nullptr;
    if (mount) {
        motion = CMotionManager::instance().GetMotion(mount, MAKE_MOTION_KEY(MOTION_MODE_GENERAL, motionIndex));
        if (!motion) motion = CMotionManager::instance().GetMotion(race, MAKE_MOTION_KEY(MOTION_MODE_HORSE, motionIndex));
    } else {
        motion = CMotionManager::instance().GetMotion(race, MAKE_MOTION_KEY(mode, motionIndex));
    }
    if (motion) {
        const float duration = motion->GetDuration();
        const float distance = -motion->GetAccumVector().y;
        if (std::isfinite(duration) && duration > 0 && std::isfinite(distance) && distance > 0) {
            const float speed = distance / duration;
            if (std::isfinite(speed) && speed > 0) return speed;
        }
    }
    return 300.0f; // Missing/malformed motion must not introduce NaN or zero division.
}

float GetMoveSpeed(entt::entity e)
{
    return GetMoveMotionSpeed(e) * 100.0f / MovementDurationFactor(e);
}

void CalculateMoveDuration(entt::entity e)
{
    if (!IsValid(e) || !g_registry.all_of<ecs::Position>(e)) return;
    MotionCommand command(e);
    if (!command.started) return;
    const auto position = g_registry.get<ecs::Position>(e);
    const auto* dest = g_registry.try_get<ecs::MovementDestination>(e);
    const auto duration = MovementDuration(e, position, dest ? dest->x : position.x, dest ? dest->y : position.y);
    const auto now = get_dword_time();
    if (!command.Current()) return;
    auto& state = g_registry.get<ecs::MovementState>(e);
    state.moveStartTime = now;
    state.moveDuration = duration;
}

} // namespace ecs::MovementSystem

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

namespace ecs::MovementSystem {

void SendMovePacket(entt::entity e, uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y,
    uint32_t dwDuration, uint32_t dwTime, float iRot)
{
    if (!g_registry.valid(e) || !g_registry.all_of<ecs::CharacterType, ecs::VIDComponent>(e))
        return;
    if (bFunc == FUNC_WAIT) {
        const auto* destination = g_registry.try_get<ecs::MovementDestination>(e);
        const auto* position = g_registry.try_get<ecs::Position>(e);
        const auto* movement = g_registry.try_get<ecs::MovementState>(e);
        x = destination ? destination->x : position ? position->x : 0;
        y = destination ? destination->y : position ? position->y : 0;
        dwDuration = movement ? movement->moveDuration : 0;
    }
    TPacketGCMove pack {};
    EncodeMovePacket(pack, g_registry.get<ecs::VIDComponent>(e).value, bFunc, bArg,
        x, y, dwDuration, dwTime,
        iRot == -1.0f ? ecs::PlayerRuntime::GetRotation(e) / 5.0f : iRot);
    ecs::ViewSystem::PacketView(e, &pack, sizeof(pack), e);
}

void Motion(entt::entity e, uint8_t motion, entt::entity victim)
{
    if (!IsValid(e) || !g_registry.all_of<ecs::CharacterType, ecs::VIDComponent>(e)) return;
    const auto* victimVID = IsValid(victim) ? g_registry.try_get<ecs::VIDComponent>(victim) : nullptr;
    packet_motion packet {};
    packet.header = HEADER_GC_MOTION;
    packet.vid = g_registry.get<ecs::VIDComponent>(e).value;
    packet.victim_vid = victimVID ? victimVID->value : 0;
    packet.motion = motion;
    // Include the source, as the original animation broadcast did. No entity
    // or component access after publication: transport may retire the source.
    ecs::ViewSystem::PacketView(e, &packet, sizeof(packet));
}

} // namespace ecs::MovementSystem

namespace ecs::MovementSystem {

uint32_t GetCurrentMoveDuration(entt::entity e)
{
    if (!IsValid(e) || !g_registry.all_of<ecs::CharacterType>(e)) return 0;
    const auto* state = g_registry.try_get<ecs::MovementState>(e);
    return state ? state->moveDuration : 0;
}

} // namespace ecs::MovementSystem

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
	const entt::entity e = GetEntityHandle();
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
// Legacy destination was always populated - Stop and similar settle paths
// set it to the current position. To preserve legacy parity, when the
// ECS component is absent we return current position via GetX/GetY.
// This matches what Stop() etc. used to do explicitly with the destination field.
//
// Bootstrap (entity not yet ECS-registered): returns GetX/GetY which in
// turn returns 0 (per B.1.1) - matches legacy destination zero-init.
//
// Phase C will redirect writes; Phase G removes the legacy field.
int32_t CHARACTER::GetCurrentDestX() const
{
	const entt::entity e = GetEntityHandle();
	if (e != entt::null && g_registry.valid(e))
	{
		if (const auto* dest = g_registry.try_get<ecs::MovementDestination>(e))
			return dest->x;
	}
	return GetX();
}

int32_t CHARACTER::GetCurrentDestY() const
{
	const entt::entity e = GetEntityHandle();
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
	const entt::entity e = GetEntityHandle();
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

	LPCHARACTER ch = ecs::LegacyCharOf(info->ch);

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	const entt::entity saveEntity = ch->GetEntityHandle();
	LOG_TRACE("SAVE_EVENT: {}", ecs::PlayerRuntime::GetName(saveEntity).data());
	if (saveEntity != entt::null)
		g_dispatcher.trigger(ecs::EvCharSaved { saveEntity });
	ch->Save();
	ch->FlushDelayedSaveItem();
	return (save_event_second_cycle);
}


void CHARACTER::SetNowWalking(bool bWalkFlag)
{
    if (IsNowWalking() != bWalkFlag)
    {
        ecs::MovementSystem::SetNowWalking(GetEntityHandle(), bWalkFlag);
        if (IsNPC())
        {
            if (bWalkFlag)
                MonsterLog("�E�´U");
            else
                MonsterLog("�ڴU");
        }
    }
}

void CHARACTER::StopStaminaConsume()
{
    if (!m_bStaminaConsume)
        return;
    PointChange(POINT_STAMINA, 0);
    m_bStaminaConsume = false;
    ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "StopStaminaConsume %d", GetStamina());
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

namespace ecs::PlayerRuntime {

void SetPosition(entt::entity e, int pos)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	if (pos == POS_STANDING)
	{
		// Phase C.4: legacy REMOVE_BIT(m_bAddChrState, DEAD/SPAWN) removed;
		// ECS StatusFlags.isDead/isSpawnState below are the sole writes.
		if (auto* runtime = ecs::TryGetRuntimeFlags(e))
			REMOVE_BIT(runtime->instantFlag, INSTANT_FLAG_STUN);

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

		CancelCharEvent(e, CharEvent::Dead);
		CancelCharEvent(e, CharEvent::Stun);
	}
	else if (pos == POS_DEAD)
	{
		// Phase C.4: legacy SET_BIT(m_bAddChrState, DEAD) removed;
		// ECS StatusFlags.isDead below is the sole write.
		g_registry.emplace_or_replace<ecs::DeadTag>(e);
		if (auto* status = g_registry.try_get<ecs::StatusFlags>(e))
			status->isDead = true;
		g_registry.emplace_or_replace<ecs::DirtyTag>(e);
	}

	// CHARACTER::IsPC() is GetDesc() != nullptr - a client is attached - and
	// not the TagPC component, so this keeps the descriptor test.
	if (!IsStone(e) && !GetDesc(e))
	{
		switch (pos)
		{
			case POS_FIGHTING:
				if (!HasCombatState(e))
					MonsterLog(e, "[BATTLE] enter fighting state");

				EnterBattleState(e);
				AISystem::GotoState(e, ecs::AIFSMState::Battle);
				break;

			default:
				if (!HasIdleState(e))
					MonsterLog(e, "[IDLE] enter idle state");

				EnterIdleState(e);
				AISystem::GotoState(e, ecs::AIFSMState::Idle);
				break;
		}
	}

	if (auto* runtime = ecs::TryGetRuntimeFlags(e))
		runtime->position = pos;
}

} // namespace ecs::PlayerRuntime

void CHARACTER::SetPosition(int pos)
{
	ecs::PlayerRuntime::SetPosition(GetEntityHandle(), pos);
}

bool CHARACTER::IsPosition(int pos) const
{
	return GetPosition() == pos;
}

int CHARACTER::GetPosition() const
{
	return ecs::PlayerRuntime::GetPosition(GetEntityHandle());
}

float CHARACTER::GetRotation() const
{
	if (const auto* runtime = ecs::TryGetRuntimeFlags(GetEntityHandle()))
		return runtime->rotation;

	return 0.0f;
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

	LPCHARACTER	ch = ecs::LegacyCharOf(info->ch);

	if (ch == nullptr) {
		return 0;
	}
	const entt::entity character = ch->GetEntityHandle();

	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkRecoveryEvent
	if (!ecs::PlayerRuntime::IsPC(character))
	{
		if (AffectSystem::IsAffectFlag(character, AFF_POISON))
			return PASSES_PER_SEC(std::max((uint8_t)1, ch->GetMobTable().bRegenCycle));


#ifdef ENABLE_DS_RUNE
		if (ch->GetMobTable().dwVnum == 3996) {
			LPDUNGEON target = ch->GetDungeon();
			if (target) {
				if (target->GetFlag("floor") == 5) {
					ch->DistributeSP((ch ? ch->GetEntityHandle() : entt::null));
					if (ecs::PointSystem::GetMaxHP(character) <= ch->GetHP())
						return PASSES_PER_SEC(3);

					int iPercent = 0;
					int iAmount = 0;

					{
						iPercent = 2;
						iAmount = 15 + (ecs::PointSystem::GetMaxHP(character) * iPercent) / 100;
					}

					iAmount += (iAmount * ecs::PointSystem::Get(character, POINT_HP_REGEN)) / 100;
					LOG_TRACE("RECOVERY_EVENT: {} {} HP_REGEN {} HP +{}", ecs::PlayerRuntime::GetName(character).data(), iPercent, ecs::PointSystem::Get(character, POINT_HP_REGEN), iAmount);
					g_dispatcher.trigger(ecs::EvRecovery { character, iAmount, 0 });
					ecs::PointSystem::Change(character, POINT_HP, iAmount, false);
					return PASSES_PER_SEC(10);
				}
			}
		}
		else if (ch->GetMobTable().dwVnum == 8202) {
			LPDUNGEON target = ch->GetDungeon();
			if (target) {
				if (target->GetFlag("floor") == 1) {
					ch->DistributeSP((ch ? ch->GetEntityHandle() : entt::null));
					if (ecs::PointSystem::GetMaxHP(character) <= ch->GetHP())
						return PASSES_PER_SEC(3);

					int iPercent = 0;
					int iAmount = 0;

					{
						iPercent = 2;
						iAmount = 15 + (ecs::PointSystem::GetMaxHP(character) * iPercent) / 100;
					}

					iAmount += (iAmount * ecs::PointSystem::Get(character, POINT_HP_REGEN)) / 100;
					LOG_TRACE("RECOVERY_EVENT: {} {} HP_REGEN {} HP +{}", ecs::PlayerRuntime::GetName(character).data(), iPercent, ecs::PointSystem::Get(character, POINT_HP_REGEN), iAmount);
					g_dispatcher.trigger(ecs::EvRecovery { character, iAmount, 0 });
					ecs::PointSystem::Change(character, POINT_HP, iAmount, false);
					return PASSES_PER_SEC(10);
				}
			}
		}
#endif

		if (!ch->IsDoor())
		{
			const int64_t hpGain = std::max(int64_t {1}, (static_cast<int64_t>(ecs::PointSystem::GetMaxHP(character)) * ch->GetMobTable().bRegenPercent) / 100);
			ch->MonsterLog("HP_REGEN +%d", hpGain);
			g_dispatcher.trigger(ecs::EvRecovery { character, static_cast<int32_t>(hpGain), 0 });
			ecs::PointSystem::Change(character, POINT_HP, hpGain);
		}

		if (ch->GetHP() >= ecs::PointSystem::GetMaxHP(character))
		{
			ecs::PlayerRuntime::SetCharEvent(ch->GetEntityHandle(), ecs::PlayerRuntime::CharEvent::Recovery, nullptr);
			return 0;
		}

		return PASSES_PER_SEC(std::max((uint8_t)1, ch->GetMobTable().bRegenCycle));
	}
	else
	{
		CombatSystem::CheckTarget(ch->GetEntityHandle());
		ch->UpdateKillerMode();

		if (AffectSystem::IsAffectFlag(character, AFF_POISON) == true)
		{
			return 3;
		}
		int iSec = (get_dword_time() - ch->GetLastMoveTime()) / 3000;

		ch->DistributeSP((ch ? ch->GetEntityHandle() : entt::null));

		if (ecs::PointSystem::GetMaxHP(character) <= ch->GetHP())
			return PASSES_PER_SEC(3);

		int iPercent = 0;
		int iAmount = 0;

		{
			iPercent = aiRecoveryPercents[std::min(9, iSec)];
			iAmount = 15 + (ecs::PointSystem::GetMaxHP(character) * iPercent) / 100;
		}

		iAmount += (iAmount * ecs::PointSystem::Get(character, POINT_HP_REGEN)) / 100;

		LOG_TRACE("RECOVERY_EVENT: {} {} HP_REGEN {} HP +{}", ecs::PlayerRuntime::GetName(character).data(), iPercent, ecs::PointSystem::Get(character, POINT_HP_REGEN), iAmount);

		g_dispatcher.trigger(ecs::EvRecovery { character, iAmount, 0 });
		ecs::PointSystem::Change(character, POINT_HP, iAmount, false);
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

