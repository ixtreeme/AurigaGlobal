# C41 — Original Nexoria inventory and storage

Inventory and account storage now use the supplied `Leltar` (1160×1262) and
`Raktar` (1100×1050) PSD/JSX layouts. Thirty native Photoshop exports preserve
the original effects and geometry; originals are never saved by the exporter.
The shared manifest preserves C40's shop, trade and menu assets. Both export
tools retain existing GUIDs and the other registered window designs.

The inventory retains the source category controls, equipment positions,
illustration, details/set/shop actions, pager and currency footer. Sample items,
levels, quantities and money have been removed. Current server metadata and
persistent instance IDs drive every rendered item and action. Equipment uses
the actual supported slot types at the source positions. The source warrior is
decorative artwork, not a live appearance preview. The unimplemented weight
value is replaced with real occupied-cell capacity. Favorites are local
preferences scoped by server/character name and persistent item ID.

Source grids are viewports over the complete server grids: the inventory's
four-column view supports horizontal/vertical scrolling; storage keeps its
nine-column silhouette and dims non-existent padding cells. Neither view
remaps or predicts logical anchors, multi-cell footprints or container IDs.
Every real server row, column and page remains accessible. Stored-item name
search has its own closable window.

Inventory and storage have separate canvases, visibility and close lifecycles.
The storage quantity, inspection, rearrangement and two-stage confirmation UI
is a separate helper. Its X preserves the main views. Closing storage also
hides its helper; closing inventory preserves storage. X works while a request
is pending, and late replies no longer resurrect dismissed storage. Read-only
account storage explains the keeper requirement directly on the main surface.
Password management, automatic sorting and bulk withdrawal stay disabled;
the source's password-protection claim is excluded. Other storage routes retain
their existing authoritative command model, but their own authored designs
are outside this work unit. Existing costume/rune/pet entry points remain.

No gameplay packet layout, codec or SQL schema changed. Native storage/runtime
and inventory-wire tests passed 2/2. The full snapshot suite passed 37/37; after
the final read-only guidance change, the active Editor UI gate passed, including
28 dedicated inventory/storage assertions and the existing UI/window galleries.
The basic live harness now waits for an inventory snapshot instead of requiring
unrelated pet seals when running its UI-only flow. On the owned local LAN server,
1681 checks and one relogin verified buy/sell, equip, deposit/withdraw and exact
restoration of IDs, units, locations, equipment and wallet. Two existing dedicated
test accounts were reused; the original demo owners retain all items and money.

The set helper also retains the real combat-stat, rune and companion entry points.
The Windows player checks verified the actual HUD and I-key routes, both X controls,
independent main windows, horizontal scrollbar and vertical mouse-wheel input,
read-only keeper guidance, and the authored ESC menu. Category filtering also
passed in the preceding player. Final changes clarify read-only storage and
retain the separate combat-stat entry, whose values and close lifecycle were
also checked through the real Windows buttons.
Saved-account bytes and all item/wallet state were restored exactly; only the
monotonic session generation is excluded from equality. Header dragging passed
event-level tests but did not visibly move a window in native input attempts;
physical header dragging remains unconfirmed. No performance result is claimed.

Final Windows and Android builds and an unsigned iOS export use the frozen
source. Local catalogues, Android package/two-ABI native libraries and iOS local
network/native-library settings were checked. Physical mobile devices and Mac
linking/signing remain untested. Player-only refreshes reuse existing content
and require 6 GiB free; full content builds retain the 20 GiB guard. All ten
pre-build settings files, recovery scenes and unrelated pending edits are
preserved. The Editor returns to Windows. Previous R9 delivery files remain.

Delivery: `C:/AurigaGlobal/LocalPlaytest/Nexoria/Nexoria-Windows-Local-r11.zip`,
`Android/NexoriaLocal-r11.apk`, `iOS-r11/Unity-iPhone.xcodeproj`.
Work stays local on Git `20261103` and UVCS
`/main/unity63-integration-20261103`, using Unity `6000.3.27f1`.
No Git commit/push, Unity checkin or VPS operation was performed. Git remains
`a577d68e` and UVCS remains cs:100 with pending edits. Other catalogue windows
are still to be replaced; this is the inventory/storage work unit.
