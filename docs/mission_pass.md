# Auriga Mission Pass

A separate Lua-free account/season system. The old Battle Pass is neither called
nor modified. General account premium is unrelated to Mission Pass entitlement.

## Features

* 42-day season draft, 40 reward levels, 1,000 XP per level; level zero initially.
* 10 daily, 12 weekly and 14 season missions. Daily reset 00:00 UTC; weekly reset
  Monday 00:00 UTC. All listed objectives are available, not a random subset.
* Hunting mobs, bosses and metins; solo/party/guild dungeons; woodcutting,
  herbalism, mining, fishing, tool training/upgrades, active time and guild EXP.
* Unique plant/boss/dungeon objectives count stable species/type IDs, not VIDs.
* Combat targets may be at most 15 levels below the character.
* Solo tracks everyone who entered the instance, so leaving a group just before
  completion cannot turn a group run into a solo run. Completion hooks are only
  on successful C++ dungeon endings, never timeout/abort cleanup.
* One completion per run/account, including alternative characters.
* Active minutes require a meaningful server-side activity within 90 seconds;
  idle, login/selection screens and disconnected time do not accrue XP.
* Free and premium reward tracks, individual claim and claim-all. Premium
  activation/purchasing is intentionally NOT part of this system.
* Responsive desktop/portrait UI, safe area, locale-based English/Hungarian text,
  item icons/tooltips, countdown, live progress and separate entry button.
* Dedicated version-1 amp_* messages over the existing authenticated server
  command transport. No legacy Battle Pass packet or UI entry point is reused.

## Persistence and delivery

Account state lives in player.account_mission_pass. state_json is the authoritative
compact progress/unique-subject snapshot, xp is its checked materialized XP value,
revision is an optimistic-concurrency guard. Only current reset buckets are kept;
earned season XP is retained. The original progress/subject tables are reserved
for reporting and are not authoritative.

Gameplay events queue on the game thread; SQL runs asynchronously. A compare-and-
swap transaction locks the account row, verifies revision and atomically records
the new state plus a unique batch receipt. Concurrent cores reload/reapply on
conflict. An ambiguous response retries the SAME batch token, so committed XP is
not awarded again. State is published only after SQL confirmation. Queued,
uncommitted events can be lost on a hard process crash, like other unsaved game
state; the system does not promise impossible crash-free in-memory persistence.
Graceful shutdown must allow queued writes to drain before terminating cores.

Batches are coalesced over three seconds after a successful write. Activities with
no remaining matching objective are not queued. No synchronous SQL is added to
combat, gathering or guild donation.

Rewards are reserved and inserted into the existing InnoDB item_award table in ONE
transaction. Outbox uniqueness is (account, season, level, track). Retries and
concurrent requests cannot insert a second award. Inventory-full cannot lose a
reward because delivery is to Item Mall storage, not an unguarded GiveItem call.
The existing database core refreshes awards every five seconds. Close/reopen
storage after delivery refresh; the Mission Pass has a Reward storage button.
Already delivered rewards keep their original item even if the catalog changes.

Premium entitlement is premium_activated_at on that account/season row. NULL means
absent. Neither the client nor the general account premium columns can grant it.
A later premium service can activate it; already earned premium levels then unlock.
No rewards may be claimed before season start or at/after its exclusive end.

## Content and editable rewards

config/mission_pass.json.example is a disabled template with example dates.
The live file is /Auriga/Server/data/mission_pass.json. Deployment sets a real
42-day window and synchronizes player.mission_pass_season and mission_pass_reward.

Tools/GenerateMissionPassContent.py in the Unity workspace authors the draft,
both locale files and the placeholder reward catalog. Forty free rewards include
red/blue potions, herbs, logs, pearls, Blessing Scrolls, Enchant/Reinforce Item and
a Blacksmith Handbook milestone. The locked premium placeholders double counts.
The owner approved replaceable starter rewards; all item IDs are validated against
live item_proto before enabling. Replace item/count values, not mission IDs/goals
inside an already active season. Reward slots are read from the database in each
snapshot, so a catalog update cannot display one item and deliver another.

The draft offers 69,300 XP in six full weeks against 40,000 required XP. Skipping
activities is supported. Actual availability and economy still need normal
player feedback; high-level characters must hunt appropriate targets.

## Commands and client

The in-game Mission Pass button opens the new UI. /missionpass open also opens it.
Other authenticated commands are /missionpass close, /missionpass claim LEVEL TRACK
(0 free, 1 premium) and /missionpass claimall. Only identifiers travel from client;
account, XP, premium and awarded items are always server-derived. Requests are
rate-limited, malformed chunks are rejected, transfers are capped at 60,000 bytes
and incomplete transfers time out after ten seconds.

Editor design preview:
Metin2 > Experimental > Mission Pass > Open design preview.
It asks about unsaved scenes and opens an isolated scene. This is a clearly marked
zero-progress preview; it never grants rewards or sends game requests.

No Windows/Android player build is created by server deployment. Unity Editor
uses the current client source; already distributed players need a later client
build/patch containing the new code.

### Presentation and completion notifications

The window is centered and capped at 1120x630 logical units on desktop, with
gold/teal fantasy artwork, separate mission and reward panes and horizontally
scrollable reward tracks. Narrow screens use mission/reward tabs and safe-area
layout instead of shrinking desktop text to unreadable sizes.

After a successful durable progress commit, the server sends
`amp_complete SEASON MISSION RESET_UTC XP` for newly completed missions. Initial
snapshots and account refreshes do not replay historical completions. Conflicts
rebase the event batch and ambiguous SQL retries retain the completion candidates.
The client derives the mission's locale key, deduplicates season/mission/reset,
and displays a centered four-second notification only while the panel is closed.
Multiple completions queue; the unscaled fade does not block pointer input.
Opening the panel dismisses queued notifications; logout clears presentation and
session dedup state. Progress snapshots never trigger completion notifications.

## Deployment and verification

1. Run rules/content/Unity protocol tests and isolated MariaDB transaction tests.
2. Review remote hashes. Preserve independent live source edits with reviewed
   three-way overlays, never overwrite divergent remote gameplay fixes.
3. Upload with Server Manager backup receipts and build on FreeBSD.
4. Back up existing Mission Pass data/config/routines; validate the item catalog
   and empty table postfix. Apply additive migrations with the socket administrator.
5. Grant only EXECUTE on the three Mission Pass procedures to the existing runtime
   SQL principal, not blanket schema-management permissions.
6. Set the enabled season and synchronized reward catalog. Stop the previously
   running instances, install the built binaries with rollback backups, restart
   exactly those instances and verify hashes, readiness and configuration logs.
7. Record source hashes, build/install IDs, data backup path and health checks in
   Library/MissionPassDeploymentState.json in the Unity workspace.

SQL/FreeBSD/Unity tests are evidence for the exercised paths, not a claim that
every possible gameplay/device combination has been manually tested.
The deployment receipt, not this document, is authoritative for live status.
