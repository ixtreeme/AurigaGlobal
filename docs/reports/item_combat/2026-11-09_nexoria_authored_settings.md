# C39 — Authored settings and the white startup grids

C38 left an important presentation gap: several windows only reused Nexoria
art around their old control layout. Its gallery tests also missed three white
auxiliary inventory grids created during real native-item client startup.
C39 corrects those startup grids and rebuilds settings from the actual supplied
design. It does not claim that every remaining catalogue window already
matches its PSD.

The native item route deliberately returns no legacy inventory prefab. The old
auxiliary-panel initialization treated that as a missing prefab and constructed
three fallback grids. Native sessions now bypass that fallback, while legacy
sessions keep their existing path.

Settings uses `D:/Nexoria UI/Beallitasok/Nexoria_Beallitasok.psd`: its frame,
crest, navigation, two columns, source X and footer buttons. Photoshop renders
23 separate assets with their original layer effects. The accompanying
authoring JSX supplies the positions of all 45 controls. The profile name comes
from the connected character rather than the design's sample name. No original
PSD is saved or replaced.

The four settings pages retain the source document's navigation. Other game
features continue to have their own windows. System/game/sound entry points
select the appropriate authored settings page. Draft changes survive page
switches; Apply saves the real bindings; Cancel, X and Escape discard uncommitted
edits. The source X remains in position at 720p, and generic text enlargement
does not alter the authored control labels. The original title area drags the
window, and the full panel catches pointer presses so blank areas cannot move
the character behind it. Expanded dropdowns use the supplied Nexoria panel art
without stretching the field arrow.

Twenty settings have real bindings, including existing graphics/audio/game
options and the local display/audio preferences. The other 25 design rows stay
disabled; importing artwork does not implement their missing gameplay,
notification, privacy or device-selection backends. The supplied static artwork
is Hungarian. This change does not provide translated settings raster art.

Validation and final artifact details are recorded in the accompanying
evidence JSON after the final build. Source branches remain Git `20261103` and
UVCS `/main/unity63-integration-20261103`, using Unity `6000.3.27f1`. All game
tests and builds use the existing local test package; no VPS operation is part
of this change.

## Validation and delivery

Unity changeset **100** contains the 66 reviewed source/art files. The native `storage_runtime` and `typed_inventory_wire` tests passed 2/2. Unity compilation and all 35 offline packages passed (`build/c39-offline6.log`), including 222 authored-settings checks, 448 existing-window checks, the 90-window catalogue and the 21 native-operation gallery. The final dropdown template/sensitivity adjustment passed the active-project UI gate again and was exercised by real mouse input in the Windows player.

The LAN native-item flow passed **1196 checks and 1 relogin**. Later adjustments affected presentation/input only. The Windows player reached the actual local map. Manual checks covered all four pages, dropdown selection, slider dragging, toggles, pending edits, Cancel restoration, source X, Escape, title dragging, a clean expanded list, wheel access to the last 240 FPS option, and a blank-panel click producing no world destination. The dedicated second account retained all 7 item instances/counts and exactly 10,000 Yang; saved-account bytes were restored.

Windows and Android player builds and the unsigned iOS Xcode export succeeded from the frozen source. Final LAN catalogue, Android package/two-ABI cipher/STL and iOS local-network/native-library checks passed. Previously compiled world content was reused. The Editor returned to Windows; all ten pre-build settings files and the nine initially pending settings files were verified/restored without including them or recovered scenes in the checkin.

Final Windows managed code is byte-identical to the interactively tested build. No performance benchmark or physical mobile-device test is claimed.

Local delivery: `C:/AurigaGlobal/LocalPlaytest/Nexoria/Nexoria-Windows-Local-r8.zip` (**412,962,398 bytes**, SHA256 `548ff3dc3217be182452b6831e53befafda7e5a2141a88ac40c3d929d5798bb6`, ZIP CRC passed), `Android/NexoriaLocal.apk`, and `iOS/Unity-iPhone.xcodeproj`. No VPS operation or publication occurred. Original PSDs, earlier Windows archives, old Unity workspaces and unrelated pending changes are preserved.
