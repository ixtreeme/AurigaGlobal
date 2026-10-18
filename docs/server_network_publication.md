# Server network publication contract

The GameServer publishes committed ECS state through protocol-specific events.
There is no periodic network sweep over `DirtyTag`. The former, disabled
`NetworkSyncSystem_Update` prototype has been removed: it sent a synthetic WAIT
and an incomplete full-points snapshot for unrelated changes.

## State ownership and delivery

| State | Publication path | Recipients and payload |
|---|---|---|
| Initial/full points | `NetworkSyncSystem::PointsPacket` / `BuildPointsPacket` | Owner session only; every `POINT_MAX_NUM` entry comes from `PointSystem::Get`, including non-vital bonuses and 64-bit values. Used on load and explicit full refresh. |
| Point changes, HP/SP/stamina | `PointSystem::Change` | Owner receives `CHARACTER_POINT_CHANGE` after the component changes. `bAmount` controls the displayed delta; `bBroadcast` explicitly selects `ViewSystem::PacketView`. HP also updates target and party information through the existing specialized paths. |
| Equipment | Inventory equip/unequip commits slots, item ownership and bonuses, then calls `NetworkSyncSystem::UpdatePacket`; storage-slot packets update the owner's item UI | Owner and nearby viewers receive `CHARACTER_UPDATE` with appearance parts. Full item attributes use the separate explicit equipment-inspection path; do not broadcast a private inventory as a replacement for appearance. |
| Affects | `AffectSystem` applies/removes points and flags, sends owner `AFFECT_ADD`/`AFFECT_REMOVE`, then publishes `UpdatePacket` where visual state changes | Owner receives duration/apply information; owner and nearby viewers receive the visual flags/speeds in `CHARACTER_UPDATE`. |
| Skill levels/group | `SkillSystem::SendSkillLevelPacket` / `SetSkillGroup` | Owner session only. The level snapshot contains all `SKILL_MAX_NUM` entries and requires a loaded table. Skill-changing operations explicitly publish after commit; table hydration and low-level setters are not implicit UI notifications. |
| Skill colors and other appearance state | `NetworkSyncSystem::UpdatePacket` | Owner and nearby viewers receive current parts, speeds, affect flags, state flags, alignment, PK mode, guild, mount, skill colors and language according to the compiled protocol. |
| Movement | Movement commands and client input publish MOVE/WAIT through `ViewSystem::PacketView`; `MovementSystem_Update` advances the committed movement interval and spatial state | A command preserves its destination, rotation, start time and duration. Client-predicted movement excludes its sender where specified by the command. A stat, equipment or affect update must not manufacture WAIT or restart interpolation. |
| Visibility entry/exit | `EntityNetworkDispatch::SendInsert` / `SendRemove` | ADD and its AdditionalInfo belong to insertion only. UPDATE must never replay AdditionalInfo alone: the client pairs it with its cached ADD position. |

`DirtyTag` is a generic, idempotent mutation marker shared by many gameplay
services. It has no changed-field mask, recipient set or packet ordering. Adding
it neither sends a packet nor queues a full refresh; packet publication does not
consume it. It is not a persistence acknowledgement either. New mutations must
use the appropriate state-owner operation and explicit publication rather than
relying on this marker or on a future catch-all network tick. Low-level setters
may be used while hydrating/recomputing state without independently publishing.

## Publication ordering and lifetime

Commit the authoritative component values and relationships before publishing.
Use a packet snapshot and avoid retaining component references across transport,
quest or event callbacks. After callbacks, revalidate the versioned entity and
any relationship needed for the next step. Skip missing sessions.

Appearance UPDATE requires a registered VID, live spatial entity and current
sector membership. It shares `ViewSystem::PacketView` with movement and point
broadcasts: recipient selection includes the owner once, excludes out-of-range
characters, and stops delivery to later recipients if a callback retires the
source or changes its spatial revision. The current range/observer-mode rules in
`VisibilityService::GetViewersOf` remain authoritative; this work does not switch
to the diagnostic ViewerMap mirror.

## Verification boundary

`EntityNetworkDispatchTests` captures real packet builders and recipient
dispatch at `DESC::Packet`: one owner/observer appearance UPDATE during active
movement, all compiled appearance parts, both affect words, skill colors, no
MOVE/ADD/points replay, all 255 full-points entries, private owner routing,
missing/stale sessions and revision changes between recipients.

`PointCalculationTests` checks real point changes against poisoned legacy vital
mirrors, immediate authoritative values and 64-bit HP/SP wire deltas.
`GmCommandTests` checks the real skill publisher's complete level table and its
unloaded/session/lifetime gates. Existing affect, inventory and spatial suites
exercise their actual state-owner operations with controlled leaf services.

These are protocol and server-side regression checks, not live rendering tests.
R12 still requires two connected clients: equip/unequip during movement, add and
expire affects, spend skill points/change colors, damage/recover HP/SP, enter and
leave visibility, and logout/relog with persistence.

The user-confirmed prerequisite for live client checks is the Unity client's
connection migration: it must be able to connect to the current server first.
Until then, the client portion of R12 is pending that dependency. Server-side
automation, protocol fixtures and database checks can proceed independently;
the completed server implementation does not imply verified Unity rendering.
