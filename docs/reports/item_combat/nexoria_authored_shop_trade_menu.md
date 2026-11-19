# C40 — Authored Nexoria shop, player trade and ESC menu

The NPC shop, two-party trade and ESC menu now use the supplied `Bolt`,
`Kereskedes` and `Rendszer_Menu` PSD/JSX layouts. Their original native
Photoshop artwork, positions and dimensions replace the previous control
arrangements. Example names, items and prices are excluded; live server data
fills those areas. The four shop property rows use the actual item state.

Every functional view has its own canvas and close lifecycle. Trade inspection
and quantity actions have a separate helper window; closing it preserves the
trade, and closing trade hides its helper and retains the existing server
cancellation semantics. Buy/sell and final trade confirmation still wait for
authoritative replies. Locked offers have a single explicit modify action.

The real Escape/system-dialog route and the HUD now open the same authored
menu. A Windows player check caught the remaining legacy Escape route after
the initial isolated UI tests; the fix is covered by production-route
regressions and a fresh Windows player check. Menu buttons retain the working
help, settings and session commands. Profile name, level and selected channel
come from the current client state. Extra trade portrait/profile fields and
channel switching are not provided by the current backend; sample data is not
substituted. The existing supplied kit remains in the item helper dialogs.

Desktop windows fit within bounded areas, have header-drag handlers, support X
and Escape dismissal, and consume clicks over blank panel areas. X, Escape and
the settings route passed the final native Windows player smoke. Two native
mouse drag attempts did not visibly move the menu, so physical dragging remains
an open check despite passing event-level layout validation. Small-screen
overflow retains an accessible pinned close control. No central item manager
or new cross-feature tabs were introduced. Existing settings retain their
authored page navigation and 20 implemented bindings.

Native tests passed 2/2. The final full snapshot suite passed 36/36, including
91 authored-window checks, 448 existing-window checks, 90 catalogue screens
and 21 typed-operation views. The full active Editor UI gate also passed.
The LAN shop/storage flow passed 1675 checks and one relogin; the two-client
trade flow passed 125 checks over two cases with both clients reconnecting.
Only two fresh dedicated accounts were provisioned. Both pre-existing demo
owners retain every item and wallet field. The Windows smoke preserves saved
account bytes and all item/wallet state; only the monotonic session generation
is excluded from economy equality. No performance benchmark is claimed.

Local Windows and Android builds and an unsigned iOS export succeeded from
the final source. LAN catalogue, Android package/two-ABI cipher/STL and iOS
local-network/native-library checks passed. Earlier content bundles are reused.
Physical phones, Mac linking and signing remain untested. All ten pre-build
settings files and unrelated pending workspace changes are preserved; the
Editor returns to Windows. Original PSDs and earlier delivery archives remain.

Work stays local on Git `20261103` and UVCS
`/main/unity63-integration-20261103`, Editor `6000.3.27f1`. No new Git commit,
push, Unity checkin or VPS operation was performed. Git remains `a577d68e`
and the Unity workspace remains on changeset 100 with these local edits.

Delivery is under `C:/AurigaGlobal/LocalPlaytest/Nexoria`:
`Nexoria-Windows-Local-r9.zip`, `Android/NexoriaLocal-r9.apk` and
`iOS-r9/Unity-iPhone.xcodeproj`. Other windows, including inventory and storage,
still need their original-layout rebuilds; this does not claim a complete UI
replacement. Exact hashes and verification records are in the evidence JSON.
