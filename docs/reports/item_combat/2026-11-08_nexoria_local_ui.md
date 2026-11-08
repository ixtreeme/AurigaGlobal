# C36 — Nexoria UI and local native-item playtest

Status: first local Nexoria/native-item playtest milestone ready. Windows player, Android APK and unsigned iOS Xcode export completed. No VPS rollout. Complete visual parity and physical mobile acceptance remain open.

Unity source: changeset **97**, branch `/main/unity63-integration-20261103`. Paired server source: C36 on Git branch `20261103`.

## Scope

The active `20261103` server and Unity integration branches now use the Nexoria presentation for the primary native inventory, HUD, login/selection screens and shared window controls. The source artwork is the user's `D:/Nexoria UI` collection. The imported runtime kit contains 68 existing exported textures/font assets, with new Unity GUIDs; this is not a claim that every one of the 212 PSD layouts has been recreated independently.

The HUD reads HP/SP/XP from server point packets, Yang from the native wallet, and Mission objectives from background native Mission snapshots. Unsolicited snapshots do not open the journal. The character window now reads equipment combat values from GC231. Primary inventory, trade, shop, crafting, storage, rune/soul/helper entry points lead to the native item controls. Skill and emotion registrations remain connected.

Native hotbar entries store the complete persistent item ID as a string in device-local, character/realm-scoped preferences. They use the same native action handlers as the inventory. Missing items cannot produce a stale item-use request. Left click or 1–8 uses an entry; selecting a usable inventory item and clicking a slot assigns it. Shift-click clears it. Touch long-press assigns the selected item or clears an entry. These UI preferences are not synchronized between devices.

Shared frames/buttons and legacy board artwork use the Nexoria kit while retaining existing widget identities and controller bindings. Touch windows have safe-area viewports, scrolling and a close control that invokes the original callback and respects pending operations. The mobile movement/combat/skill controls remain active; duplicate old status/Yang displays are hidden.

Server changes are limited to background Mission replies and refusing legacy shop fallback for an owner already using native items. No packet layout, persistence schema or automatic legacy-item conversion changed in C36.

## Local setup

- Owned server package: `D:/AurigaGlobal/Server2-Local`.
- LAN game bind/advertised address: `192.168.50.52`.
- Authentication: TCP30052; isolated playtest channel CH99: TCP30078.
- SQL/database endpoints remain local. Nothing was deployed to the VPS.
- Two new dedicated native-item accounts were created. Existing owners and legacy inventories were preserved.
- Private credentials: `D:/AurigaGlobal/Server2-Local/run/nexoria-playtest/BELEPES.private.txt`.
- Private fixture/config: `D:/AurigaGlobal/Server2-Local/run/nexoria-playtest/connection.private.json`.
- Native starter items include weapon, armour, recovery supply, material, pet/mount seals and sample Mission material. Nearby NPCs provide native shop9003, warehouse9005 and workshop20016. NPC9001 exercises refusal of an unconverted shop.

Use these dedicated characters for the first tests. Existing characters and newly created legacy characters are not automatically converted into native owners by this UI change. The native catalogue still includes development content; a complete retail catalogue and all old reward/quest/APPLY adapters remain separate work.

The local build command is `Metin2.Editor.Metin2NexoriaLocalBuild.RunFromCommandLine`, using the pinned Unity6000.3.27f1 and `-buildTarget Win64`, `Android` or `iOS`. `METIN2_NEXORIA_BUILD_ROOT` selects the output directory. It builds platform Addressables and the player; it does not publish. The explicit `METIN2_NEXORIA_LOCAL` player define uses the LAN content service and excludes the production patch/updater startup. Ordinary release builds retain their existing updater path. Build settings are restored after the operation.

Serve generated `Content` with `tools/local_playtest/serve_content.py --package D:/AurigaGlobal/Server2-Local --content <build-root>/Content`. It binds this machine's owned game address on TCP30090. `tools/local_playtest/Enable-NexoriaLan.ps1` prepares narrowly scoped inbound rules for the owned auth/game executables and content port, limited to the local subnet. Applying those Windows Firewall rules requires an administrator shell; they have not been applied during this non-administrator session.

## Checks actually run

- Full GameServer build after both C36 adapter changes: passed (`build/c36-game-build3.log`). Installed only in the owned local package.
- Selected native regression suite: 57/57 passed, 167.35s (`build/c36-native-tests-final.log`). Includes six integration groups.
- Unity6.3 stripped-source offline compilation and existing validator suites: passed. Final C36 Nexoria validator: 51 checks (`build/c36-unity-offline14.log`), including native wallet/stat display, all eight real slot handlers, native pointer action, full64-bit ID persistence, character scope, missing-item/relogin behavior, window routes, safe legacy-HUD hiding and Android/iOS safe areas.
- Full active project catalogue audit: 130 generated UI windows, 178 checks passed (`build/c36-active-import.log`). The initial cold import stalled at its end and was restarted with its completed cache.
- Native Mission LAN run: passed, 1,686 checks and nine relogins (`build/c36-mission-live2.log`). Independent SQL checks confirmed durable rewards/progression and preservation of 1,158 protected items.
- Two-client native trade through the inventory entry: passed, 125 checks (`build/c36-trade-unity.log`). Covers partial stacks, full instances, Yang, duplicate/stale approvals, full inventories, disconnect and both relogins. Dedicated test inventories were left usable afterward.
- Actual native-mode LAN basic UI run: passed, 1,264 checks and one relogin (`build/c36-basic-live2.log`). Uses the new HUD/inventory shop and storage buttons, checks native stack top-up/partial sale, equipment and exact restored wallet/item state. Clicking an unconverted merchant produced a native refusal and no legacy GC38 shop session.
- Local endpoint guards: five checks passed; Python/PowerShell helper syntax passed. Content HTTP read and traversal-refusal probes passed. These same-machine probes do not establish external-device firewall access.
- Windows/Android/iOS/portrait layout renders generated and inspected from the source snapshot. They are synthetic UI captures, not physical mobile-device or full-world proof.
- Production-scene Play Mode: passed (`build/c36-world3.log`), with native local login, actual character-selection button, Razor93 map entry, the real HUD and inventory. The first scene run exposed a Unity6.3 native-component fake-null issue in the new legacy-HUD hiding code; it was corrected and covered by the added offline check. A separate camera render produced the world/UI proof at `build/c36-world-captures/world3.png`; its camera-space capture includes post-processing and is not a pixel-parity acceptance image.
- Windows x64 Development player and full platform Addressables: build passed (`build/c36-windows-local.log`). Artifacts were moved to `C:/AurigaGlobal/LocalPlaytest/Nexoria` to preserve free space on the source drive. The independent player authenticated on the local server, entered Razor93 and produced a nonblank world/HUD/native-inventory screenshot (`build/c36-player-live/world-start.png`). Its later FPS benchmark rejected its render-frame count; the overall performance benchmark did not pass. Do not interpret this entry/render proof as a performance result. The exact original saved-account file was restored after the run.
- First Android target compilation exposed 28 references from pre-existing Windows-only Editor diagnostics to unavailable Windows APIs. Platform guards now restrict those capture tools, GM-whisper checks and Windows particle experiments appropriately; common validation remains available. The final Android build passed script and native-player compilation.
- Independent Windows manual-entry harness with the second (Razor) test account: passed to `manual-ready` on the correct local map (`build/c36-player-manual/result.json`). No gameplay automation or FPS claim is part of that entry check. The owned test process was then closed, and the exact saved-account file was restored again.
- Portable Windows archive created and CRC-tested: `C:/AurigaGlobal/LocalPlaytest/Nexoria/Nexoria-Windows-Local.zip`, 417,053,469 bytes; SHA256 `3638ba348af6cd8e19aa8ebb2a2ebc341bd4d88f0857105e64534514f1578247`. Credentials are supplied separately, not embedded in this archive.
- Latest source-snapshot validator: 51 C36 checks passed (`build/c36-unity-offline14.log`). Added checks cover portrait HUD overlap, left-side controls after rotation and scrolling all the way to the right-side controls. Touch layout now resolves the current viewport geometry before centering, and portrait HUD navigation uses a separate row. Updated synthetic captures were inspected; portrait inventory content scrolls horizontally when wider than the viewport. The stripped snapshot also emits unrelated auto-start diagnostics about intentionally omitted world/VFX assets; this is not a clean full-world Console audit.

Final local builds use product name `Nexoria Local` and mobile application identifier `com.razor93games.nexoria.local`, restored after building. This separates the mobile test installation and its saved preferences from the existing client. The iOS export adds `NSLocalNetworkUsageDescription` for direct LAN connections, following [Apple's local-network guidance](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy). Actual permission-prompt behavior remains a device check.

Android APK build6 passed (`build/c36-android-local6.log`). Manifest inspection confirms `Nexoria Local`, package `com.razor93games.nexoria.local`, targetSDK36 and ARM32/ARM64. The APK contains the native cipher and shared C++ runtime for both architectures. Artifact: `C:/AurigaGlobal/LocalPlaytest/Nexoria/Android/NexoriaLocal.apk`, 424,485,414 bytes; SHA256 `51f2e5a62db5a3bdc7c740ed93ea9ca0ee0c871458c88ced5e2dedd9993ab1f9` (`build/c36-apk-proof.json`). No phone installation or gameplay claim is made.

First player attempts exposed Android migration issues: FramePacing's Prefab dependency requires shared STL, and the older custom Gradle template omitted the pinned NDK version. The Gradle postprocessor now supplies `ANDROID_STL=c++_shared` for FramePacing, consistent with [Android's C++ runtime guidance](https://developer.android.com/ndk/guides/cpp-support), and `mainTemplate.gradle` includes Unity6.3's `NDKVERSION` placeholder. The final generated project uses the installed NDK27.2.12479018 rather than Gradle's conflicting default28.2. Build output and cached runtime URLs were independently checked to use the platform-specific LAN content path. Intermediate Android3/4/5 attempts did not produce a validated APK.

The builder supports an explicit player-only refresh (`METIN2_NEXORIA_REUSE_CONTENT=1`) for unchanged, completed local content. Its guard accepts both LF and CRLF markers and verifies the host, cached runtime settings and platform catalogue. Completed content and completed player markers are separate. Android6 reuses Android4's validated content; the final Windows refresh can reuse the first Windows catalogue because subsequent runtime edits changed mobile behavior, not serialized asset types.

iOS Xcode export passed with IL2CPP code generation (`build/c36-ios-local.log`), output `C:/AurigaGlobal/LocalPlaytest/Nexoria/iOS/Unity-iPhone.xcodeproj`. The exported plist has display name `Nexoria Local`, a Hungarian local-network purpose string and the development HTTP allowance. The export contains the native cipher library and platform-specific Addressables runtime settings. This is an unsigned Xcode project; it has not been linked, signed or run using Xcode on a Mac.

Final Windows refresh passed (`build/c36-windows-local2.log`) with the latest runtime source and `Nexoria Local` product identity. The independent final player re-entered Razor93 with the second test account (`build/c36-player-final/result.json`). SQL state before and after entry retained all seven persistent item instances/counts and the exact 10,000 Yang balance (`build/c36-player-final/economy-preservation.json`). Its temporary saved-account entry was removed and the original account file restored. This manual-entry check does not measure FPS or assert a render benchmark result.

The final portable archive is `C:/AurigaGlobal/LocalPlaytest/Nexoria/Nexoria-Windows-Local-r2.zip`, **417,116,605 bytes**, SHA256 **a961cf1df23e7a731acc672ef037308335ece46de89ab99e8557dabe47f6f096**. ZIP CRC verification passed; credentials are supplied separately. `C:/AurigaGlobal/LocalPlaytest/Nexoria/HELYI-TESZT.txt` contains startup, controls, dedicated-account, LAN firewall and restart instructions. The first archive is retained as earlier evidence; use **r2** for the current Windows test.

Final artifact checks confirmed that Windows, Android and iOS catalogue hashes served by the owned LAN content service match their generated files (`build/c36-final-artifacts-proof.json`). The iOS export contains its own LAN runtime URL, bundle identifier, native cipher library, purpose string and local HTTP allowance. These are same-host checks, not proof of external-device connectivity. Ten known temporary build/import settings were restored byte-for-byte to their pre-build revisions, and only the reviewed C36 source/artwork paths were checked in. Generated cache, Addressables link files and resolver backups are excluded.

For the first mobile import, the owned batch was restarted with temporary parallel import enabled after checking the texture/model postprocessors. Its completed artifacts were preserved and four import workers were used. This setting is confined to build preparation and is excluded from the gameplay checkin. See [Unity6.3 parallel import](https://docs.unity.com/en-us/engine/6000.3/manual/assets-and-media/import-assets/parallel-import).

## Remaining validation

`Metin2NexoriaWorldSmoke.RunFromCommandLine` exercises the real scene and can be repeated with a new capture path. The administrator-only LAN firewall helper has not been applied, so a second computer/phone still needs that setup and an actual connection test.

No Android device was attached to ADB. Android touch/gameplay on hardware and iOS signing/device installation remain untested. An iOS Xcode export does not replace Mac signing and an actual device run. The shared theme is a functional migration step; complete visual parity with every supplied PSD and final world/platform acceptance are still required before describing the entire UI/HUD replacement as finished.
