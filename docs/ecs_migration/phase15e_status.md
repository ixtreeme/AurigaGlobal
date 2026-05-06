# Phase 15E-final.LPENTITY.4-architect — running status

Updated through commit `5d35f5b` (D.6 fixup-7; Phase D end-to-end WinTest pass).

## Phase progress

| Phase | Step | Status | Commit | Notes |
| ----- | ---- | ------ | ------ | ----- |
| A     | -    | DONE   | bff448d | A.1-A.4 design docs landed |
| A.0   | -    | DONE   | local   | inventory match report; PASS |
| B.1.1 | m_pos getter rewrite | DONE | 925a2ff | atomic with MirrorLegacyMovement guard removal; previous attempt 9d27603+fefce11 reverted (a7a5b6a, 0324b0c) due to runtime regression |
| B.1.1.pre.1 | POSITION_READ_DRIFT detector | DONE | f16a96b | sweep at ~1Hz; gated by AURIGA_LPENTITY_FIXUP_AUDIT |
| B.1.2 | m_dwMoveStartTime + m_dwMoveDuration getters | DONE | 2f8fdbd | new GetCurrentMoveStartTime accessor; 4 direct field reads converted |
| B.1.3 | m_bNowWalking via IsWalking + IsNowWalking | DONE | dc22803 | 2 packet emission sites converted; SetNowWalking body retains direct field reads (write path) |
| B.1.4 | m_posDest GetCurrentDestX/Y + direct reads | DONE | 5b0f1e3 | semantic note: ECS MovementDestination is move-only; getter falls back to GetX/Y when component absent (matches legacy m_posDest settle semantic) |
| B.1.5 | m_bAddChrState reads | DONE | 3251616 | new GetAddChrStateFlag composes 4 bits from StatusFlags; 3 read sites converted |
| B.1.6 | m_pSectree migration | DEFERRED | - | ECS SectorPlacement only stores indices, not LPSECTREE pointer. Lookup cost per call is high for the 60+ call sites. Decision: defer to Phase G. The pointer is a runtime cache, not part of the desync semantic. |
| B.1.7 | m_map_view direct reads | DEFERRED | - | Per A.2 §5: m_map_view writes wait for VisibilitySystem (Phase D). Reads at the visibility-maintenance sites (ViewCleanup, UpdateSectree, ViewReencode) get rewritten as part of Phase D entirely. PacketView's m_map_view loop is part of the f76a3f1 band-aid scheduled for deletion in Phase D.8. |
| B.1.8 | m_posStart | DEFERRED | - | Field has no current ECS twin per A.2 §2. Used only in CalculateMoveDuration body. A.2 §2 m_posStart row decision: "Convert CalculateMoveDuration to take start position as a function argument (or compute it inline from current Position at the moment Goto is called). No new ECS component needed." This is a Phase C refactor, not a Phase B read-flip. |
| B.1.9 | m_iViewAge | DEFERRED | - | Per A.2 §5: "m_iViewAge migrates last, only after polling is replaced." Phase D removes UpdateSectree polling and m_iViewAge with it. |
| C.1   | char-path m_pos write removal | DONE | 1251702 | SetXYZ removed from CHARACTER::Sync, SessionSystem::Show, PlayerRuntimeSystem::SetDetails; ECS Position via SyncPositionComponents is sole writer for character paths. CheckAllPositionDrift detector deleted. |
| C.2   | timing + walking write removal | DONE | 9c95cac | m_dwMoveStartTime, m_dwMoveDuration, m_bNowWalking writes removed from CalculateMoveDuration, SetNowWalking, PlayerRuntimeSystem Initialize. SyncTimingWrite + SyncWalkingWrite are sole writers. CheckMovementDrift timing+walking subsections deleted. |
| C.3   | m_posDest write removal | DONE | 912effa | m_posDest writes removed across CHARACTER::Sync/Stop/Goto, AffectSystem stun/shock, SessionSystem Show, PlayerRuntimeSystem SetDetails/MountVnum/Initialize. SyncDestinationWrite + SyncDestinationClear sole writers. m_posStart pulled into local in CalculateMoveDuration (B.1.8 refactor folded in). CheckMovementDrift dest subsection deleted. |
| C.4   | m_bAddChrState write removal | DONE | 7634caa | 12 SET_BIT/REMOVE_BIT(m_bAddChrState, ...) sites removed across AffectSystem (polymorph SPAWN), CombatSystem (SetKillerMode), MovementSystem (SetPosition POS_STANDING/POS_DEAD), SessionSystem (Show SPAWN), SocialSystem (SetParty), PlayerRuntimeSystem (Initialize). StatusFlags 4 bits sole source. CheckMovementDrift state_flags subsection deleted - body now empty no-op shim until Phase G. GetAddChrStateForAudit removed; CheckCharacterInsertParity bStateFlag uses GetAddChrStateFlag on both sides. |

## Phase B effective end

Phase B is read-flip migration. The 4 deferred fields each fail one of the
preconditions:

  - B.1.6 m_pSectree: pointer/cache mismatch with ECS SectorPlacement
    (which holds indices). Pure read-flip would impose a per-call
    SECTREE_MANAGER hash lookup on 60+ hot-path call sites. The
    migration belongs in Phase G alongside the legacy field deletion.

  - B.1.7 m_map_view: reads are entangled with the visibility
    machinery (UpdateSectree polling, ViewInsert/Remove). Phase D
    rewrites that machinery wholesale; touching m_map_view reads
    individually before that rewrite invents work that Phase D
    would undo.

  - B.1.8 m_posStart: no ECS twin. The architect-doc decision is to
    fold the 2 read sites into a function-argument refactor, which
    is a write-side change (Phase C).

  - B.1.9 m_iViewAge: A.2 §5 mandate.

The 5 completed commits cover the 6 movement-related fields that
caused the original two-client desync:

  Position (B.1.1), MovementDestination (B.1.4),
  MovementState.moveStartTime (B.1.2), .moveDuration (B.1.2),
  .isNowWalking (B.1.3), StatusFlags 4 bits (B.1.5).

These are exactly the fields enumerated in the desync audit
phase15e_final_lpentity_4_visibility_desync_audit.txt as the source
of the broken broadcast / packet content.

## Acceptance gate per commit (Phase B)

| Commit | Builds clean | WinTest | Drift detectors | Cleanup | Architect doc ref |
| ------ | ------------ | ------- | --------------- | ------- | ----------------- |
| 925a2ff | PASS | DEFERRED to batch | n/a (still pre-flip) | atomic with guard removal | A.2 §2 m_pos |
| f16a96b | PASS | DEFERRED | n/a | new audit, no dead body | A.2 §2 m_pos |
| 2f8fdbd | PASS | DEFERRED | n/a | n/a | A.2 §2 m_dwMoveStartTime/Duration |
| dc22803 | PASS | DEFERRED | n/a | n/a | A.2 §2 m_bNowWalking |
| 5b0f1e3 | PASS | DEFERRED | n/a | n/a | A.2 §2 m_posDest |
| 3251616 | PASS | DEFERRED | n/a | n/a | A.2 §2 m_bAddChrState |

WinTest deferred per user instruction: "haladjunk tovább és amikor meg
leszünk egy nagyobb résszel akkor tesztelünk, mert lehet épp azok a
változások fognak hiányozni ebben a részben amik szükségesek a
másikból". Test gate at the end of the 5-commit B chunk, before
proceeding to Phase C.

## Phase C effective end

Phase C migrated the writes for the 5 movement-related fields whose reads
flipped in Phase B (m_pos, m_dwMoveStartTime/Duration, m_bNowWalking,
m_posDest, m_bAddChrState). After C.1-C.4, every legacy character-write
path that used to set one of these fields now goes through the ECS sync
helpers (`SyncPositionComponents`, `SyncTimingWrite`, `SyncWalkingWrite`,
`SyncDestinationWrite`, `SyncDestinationClear`) or directly emplaces the
StatusFlags 4 bits.

Field-by-field consequence:

  - m_pos: char-path writes go via SyncPositionComponents (which still
    mirrors to m_pos for non-char readers and Phase G deletion).
  - m_posDest: ECS MovementDestination component is sole writer; legacy
    m_posDest no longer maintained. Direct field reads at non-character
    sites still work because the readers on those paths use the new
    GetCurrentDestX/Y getters which fall back to GetX/Y when the ECS
    component is absent (matches legacy m_posDest "settled at current"
    semantic).
  - m_dwMoveStartTime / m_dwMoveDuration: SyncTimingWrite is sole writer.
  - m_bNowWalking: SyncWalkingWrite is sole writer.
  - m_bAddChrState: 4 StatusFlags ECS bits are sole source; the
    bStateFlag byte is now composed via GetAddChrStateFlag.

Drift-detector consequence:

  - CheckAllPositionDrift deleted (C.1).
  - CheckMovementDrift body emptied progressively through C.2 (timing,
    walking), C.3 (dest), C.4 (state_flags). Now an empty no-op shim;
    deletes in Phase G.
  - CheckCharacterInsertParity remains active. Each field comparison
    after Phase C either sources both sides from ECS (tautological but
    harmless) or compares the native pack against the still-mirrored
    legacy field (catches Sync-helper regressions).

## Acceptance gate per commit (Phase C)

| Commit  | Builds clean | WinTest         | Architect doc ref |
| ------- | ------------ | --------------- | ----------------- |
| 1251702 | PASS         | DEFERRED        | A.2 §2 m_pos      |
| 9c95cac | PASS         | DEFERRED        | A.2 §2 timing + walking |
| 912effa | PASS         | DEFERRED        | A.2 §2 m_posDest  |
| 7634caa | PASS         | DEFERRED        | A.2 §2 m_bAddChrState |

WinTest deferred per user instruction (batch test at the end of a larger
migration chunk).

## Phase D progress (event-driven VisibilitySystem)

Phase D replaces the polling-based visibility machinery with an
event-driven VisibilitySystem keyed on PositionChangedEvent. Subsumes
B.1.7 (m_map_view) and B.1.9 (m_iViewAge). The PacketView f76a3f1
hybrid broadcast band-aid deletes in D.8.

| Step | Commit | Status | Notes |
| ---- | ------ | ------ | ----- |
| D.1  | d8eb598 | DONE | PositionChangedEvent struct in events.hpp; carries old + new (x, y, z, mapIndex). |
| D.2  | 7ce28e9 | DONE | Triggers wired into SyncPositionComponents (idempotent-write filter) and SpatialService::InsertEntity (spawn case with oldMapIndex==0 sentinel). |
| D.3  | 136c6e3 | DONE | VisibilitySystem skeleton: Init/Shutdown public API, no-op OnPositionChanged. Wired into main.cpp boot. |
| D.4  | 959319c | DONE | Diff-and-update handler: ComputeViewersAt (sectree query at arbitrary mapIndex+x+y), InsertBidirectional/RemoveBidirectional, leaving/entering loops emit SendInsert/SendRemove. Character-only scope guard. Runs IN PARALLEL with legacy UpdateSectree polling. |
| D.5  | 85486e6 | DONE | DriftSweep: 5s-throttled sweep compares maintained ViewerMap to sectree truth; per-entity 30s log throttle. Logs [VISIBILITY_DRIFT]. AURIGA_LPENTITY_FIXUP_AUDIT-gated. Wired into main.cpp tick loop. |
| D.6  | 4200181 | DONE  | Disable legacy UpdateSectree polling for ENTITY_CHARACTER. Symmetric Insert/RemoveBidirectional updates all 4 ECS sides. Symmetric SendInsert/SendRemove emission. ViewCleanup + ViewReencode for chars walk ECS ViewerMap/ViewMap. ValidateViewMapMirror silenced for chars (m_map_view intentionally diverges). |
| D.7  | -      | ABSORBED | After D.6 the `++m_iViewAge` line is unreachable for chars; the field still serves non-char polling. Full removal deferred to Phase G alongside the legacy field deletion (no separate Phase D commit). |
| D.8  | db68f73 | DONE | PacketView body collapsed to ECS ViewerMap walk + self. Sectree-walk fallback retained for non-char sources whose ViewerMap is incomplete (theoretical - no current caller). f76a3f1 hybrid band-aid retired. |

## Phase D fixup series (post-WinTest landed during user verification)

The Phase D landing exposed a sequence of latent failure modes that the
drift detector did not catch. Each fixup was a targeted patch landed
between WinTest sessions; together they make the Phase D ECS-authoritative
visibility model work end-to-end.

| Step    | Commit  | Symptom                                            | Fix |
| ------- | ------- | -------------------------------------------------- | --- |
| fixup-1 | 462647e | Two clients invisible to each other on login spawn | Explicit spawn-shaped PositionChangedEvent in CHARACTER::Show bChangeTree=true branch (CharacterFactory pre-emplaces Position to the same coords Show is called with, so the SyncPositionComponents idempotent filter suppresses the event). |
| fixup-2 | ebee677 | Other character disappears after server backport on fast mount | Same explicit event in the bChangeTree=false (intra-sectree) branch, covering anti-cheat rubberband and intra-sectree warps. |
| fixup-3 | 1500b59 | Map-reload effect on backport, peers visibly flicker | Sectree boundary crossing in MirrorLegacyMovement: the D.6 UpdateSectree stub disabled m_pSectree maintenance during cross-tile movement. The next Show call sees stale m_pSectree, hits bChangeTree=true, and fires a destructive ViewCleanup-then-rebuild sequence. Re-insert into the correct sectree on every Position write side-effect. |
| fixup-4 | 07b5b86 | Metin fragments / dropped items / destroyed buildings stay rendered | VisibilityService::GetEntitiesInRange used PlayerRuntime::GetSectree which only resolves character entities. Replaced with SectorAt(mapIndex, x, y) reading ECS Position+MapIndex - works uniformly for all spatial kinds. |
| fixup-5 | bfa5b0e | Item destroy timer fires but ground items stay rendered | Ordering bug: DestroyItemEntityAndLegacy destroyed the ECS entity before calling ITEM_MANAGER::RemoveItem, so RemoveFromGround's `g_registry.valid()` check fell to the silent legacy fallback. Inverted: legacy first, ECS second. |
| fixup-6 | 81d5698 | Metin shatter animation freezes - corpse fragments stay | Same ordering bug in the character destroy path: CHARACTER_MANAGER::DestroyCharacter ran EntityFactory::Destroy first, which nulled the ECS handle before ~CEntity::Destroy could call ViewCleanup. Explicit ch->ViewCleanup() invocation while the ECS entity is still valid. |
| fixup-7 | 5d35f5b | fixup-6 landed but Metin fragments still stay | The D.6 ViewCleanup char-branch internally called legacy viewer->ViewRemove(this, false), which guards on viewer.m_map_view.find(entity) - and post-D.6 every viewer's m_map_view is frozen at spawn time. Replaced the legacy call with a direct EntityNetworkDispatch::SendRemove that bypasses the stale-mirror check. ECS-side cleanup still goes through MirrorViewClear which iterates the authoritative ECS state. |

The fixup series converges on a single architectural insight: post-D.6 the
legacy m_map_view is a write-only stale store on character paths, and any
read or guard on its content silently fails. Phase G will delete m_map_view
outright; until then, every code path that reads or writes m_map_view on a
character must either be migrated to ECS or treated as no-op fallback.

## Next milestone (HARD GATE)

WinTest the combined Phase B + Phase C + Phase D-skeleton chunk before
D.6 lands. Acceptance criteria:

1. Two-client position sync stable (the original desync symptom).
2. Walk / run / mount / dash all visible on both clients.
3. Map warp - new map mobs/stones visible.
4. Stun / dash dest reset, party flag toggle, killer-mode flag toggle,
   spawn flag toggle on dead/standing all visible on both clients.
5. [POSITION_READ_DRIFT] count zero across the session.
6. [INSERT_PARITY] count zero (the parity detector continues to verify
   the Sync-helper writes against the native packet builder).
7. [VISIBILITY_DRIFT] count zero across the session - this is the new
   D.5 gate. Any non-zero count means the new event-driven handler
   (D.4) and the legacy UpdateSectree polling diverge from the sectree
   truth. D.6 must NOT land until the count is zero.

If any of (1)-(4) fails: investigate at source. The Phase C migration
removed dual-write at the legacy fields; if a regression appears it
means a path is still reading the legacy field somewhere we missed
during Phase B. Find that read site, convert it to the ECS getter.

If (5) or (6) fail: a Sync-helper call site is missing. Find the legacy
field write that should have a corresponding sync call.

If (7) fails: the diff-handler's diff is missing some path, OR a
Position writer doesn't go through SyncPositionComponents. Inspect the
[VISIBILITY_DRIFT] entity to identify which side is wrong (only_in_mirror
= stale entry not removed; only_in_truth = entry never inserted).

## After WinTest passes

Phase E (BroadcastService unification), Phase F (native character
dispatch re-enable), Phase G (legacy field deletion + audit TU
deletion) follow. Phase D is feature-complete; the ECS ViewerMap is
event-driven and authoritative for character visibility, and the
f76a3f1 PacketView band-aid has retired.

Phase D outcome inventory:
  * Character visibility maintained event-driven via PositionChangedEvent.
  * Both ViewMap and ViewerMap symmetric updates from a single move.
  * Idle-character m_map_view staleness no longer matters (PacketView
    walks ECS instead).
  * Legacy m_map_view, m_iViewAge, ViewAgeMap remain for items / buildings
    / shops, plus dormant on chars - all delete in Phase G.
  * [VISIBILITY_DRIFT] detector active, gated by AURIGA_LPENTITY_FIXUP_AUDIT.
