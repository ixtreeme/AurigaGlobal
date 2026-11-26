# Unity typed item receivers and hover tooltips

This work retires the live Unity receivers for raw item set/update/delete,
raw ground-add, shop/trade, switchbot, item-use presentation, old item shops,
search results and refine previews. Reserved old frames are consumed without
decoding, player-item mutation or legacy UI publication. Character points,
typed item/shop/trade/storage/equipment protocols and shared ground delete (27)
and ownership (31) frames remain live.

The packet entry decoders for the retired routes are editor-only historical
fixture helpers. Imported read-only artwork records and old UI shells are
separate from the authoritative typed inventory; they cannot be populated by
those game packets. The final audit of other historical client code remains a
separate work unit.

## Item presentation

The old ItemToolTip implementation interpreting prototype values, old socket
positions, vnum refinement and raw attribute records is replaced with one
shared typed presenter. Owned item text comes from the server metadata and the
actual typed instance: description, level and class/sex restrictions, equipment
slot, stats, bonuses and locks, stones, accessory charges, pet/costume clocks,
runes, sashes and other explicit typed state. No instance properties are inferred
from the artwork catalog.

Imported unowned artwork cards can show the catalog name and description only.
Their presentation API is explicitly named ShowCatalogPreview; it does not
invent prices, instance bonuses or expiration. Native offers use their typed
metadata. Typed chat links keep their server lookup; old transcript labels do
not reconstruct an owned item from a raw payload.

Metin2TypedItemHover connects pointer enter/exit to the shared tooltip. Bag,
equipment, costume/special cells use the existing native drag binding; storage,
trade and NPC-shop cells have explicit bindings. The native quickbar and viewed
equipment use the same presenter. Hover reads current state without changing
selection, moving an item or sending an item operation.

The hover owner is fenced so an old cell's exit/destruction cannot hide a newer
tooltip. Canvas closure, removed items, login clearing and drag start dismiss it.
Clocks refresh while visible; unchanged text does not rebuild its rows. Tooltip
graphics do not intercept pointer raycasts, and the popup is raised over its
source window. Locale IDs remain authoritative, including language changes
while the popup is open.

## Verification scope

The four raw-tooltip parity validators retire with the old renderer. The new
Metin2TypedTooltipValidator covers actual pointer events, typed descriptions and
statistics, locked bonuses, stone state, ownership fencing, dismissal, live
language changes, unowned offers and receiver retirement. Mixed tooltip
assertions now use typed state or check artwork-only presentation.

No server packet layout, server runtime source, item catalog, player data or
quest package is changed by this work unit. The compatible local server source
is main commit 7d56955b. Unrelated Mission Studio, mission-engine and Unity
pending edits are excluded.

## Verified local result (2026-11-26)

Unity Version Control changeset **223 on /main** contains the 49 scoped files.
The compatible server remains Git main 7d56955b; this unit requires no server
binary or game-data deployment. Existing pending mission/studio edits are not
part of the changeset.

The pinned Unity 6000.3.27f1 Editor compiled the changed runtime and Editor
sources. The focused validator runs completed successfully:

| Unity validator | Checks |
| --- | ---: |
| Typed tooltip and retired receivers | 67 |
| Typed item integration | 14,090 |
| Typed equipment integration | 708 |
| Typed ammunition | 1,340 |
| Typed trade integration | 4,558 |
| Typed NPC shop integration | 17,983 |
| Typed storage | 4,667 |
| **Total** | **43,413** |

All **45 native server CTest tests passed** against the compatible server
build. Runtime C# compilation also passed with the actual player response
file after removing Editor defines/references. Reflection on that compiled
assembly confirmed that the retired packet entry decoders are absent. This
is a runtime-assembly check, not a packaged standalone player build.

The isolated local TLS game fixture passed real server/client dispatch:
authoritative item metadata and native hover; persistent 64-bit item moves;
partial-stack drop, typed ground publication, pickup and exact totals after
reconnect; two-account trade with both confirmations and persisted ownership.
The previously retired affect/clock paths also remained correct: old item
affects were rejected, ordinary effects remained, recovery supply and online/
wall/worn clocks persisted or expired according to their native rules.

A separate graphical Play Mode scene used the production native inventory and
the captured initial 227/225 server frames. Actual mouse movement over an
occupied cell displayed its shared typed tooltip; moving the pointer away
dismissed it. No item click or operation was needed for the hover. This
graphical component check was separate from the connected TLS fixture.

Local evidence is retained under
D:/AurigaGlobal/LiveWork/AurigaMergeBuild/unity-item-receivers-retirement:
suite.log, visual-suites-verified.log, server-tests.log, player-runtime.log,
verified-source-hashes.json, visual-visible.txt, visual-exit.txt and
typed-item-tooltip.png. The connected fixture result is
quest-redesign-release-20261125/game-result47.txt in the same build workspace.
The temporary graphical helper and temporary scripting define were removed.

The larger historical mixed validators were updated where their tooltip
contracts changed, but are not counted as passing whole suites here. Imported
read-only artwork cards and Editor historical fixtures are retained as
described above; auditing those remaining historical sources is a separate
unit.
