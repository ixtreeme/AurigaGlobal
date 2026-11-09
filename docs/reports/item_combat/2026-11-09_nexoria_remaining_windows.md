# C38 — Separate Nexoria windows for the existing non-item UI

The existing character, skills, emotions, guild pages, social list, private conversations, map, options and other catalogue windows now use independent Nexoria frames. The prepared assets come from `D:/Nexoria UI`, including the character, guild, social, settings, whisper, map/help PSDs and the `Ablakok` designs. The importer records 96 profiles (90 catalogue screens and six runtime windows) and copies 84 supplied PNGs byte-for-byte, preserving their Unity GUIDs. It does not generate replacement artwork.

Source branches remain Git `20261103` and Unity Version Control `/main/unity63-integration-20261103`; Editor `6000.3.27f1`. Unity changeset **99** contains 199 reviewed source/artwork paths. This is a client presentation change. Server packet layouts, SQL, native item authorization and legacy-item migration are unchanged.

## Behavior

The original controllers and bound controls are retained. Character/skills/emotions/quests and the five existing guild pages share their respective authoritative model while occupying separate canvases. Closing one view preserves the others. The existing mission entry routes to the typed mission window. Graphics, sound and the three existing game-option pages have separate windows. Private whispers keep their individual conversation lifecycle. Social and guild input/question/helper dialogs receive their own frames.

The character status page uses the supplied two-column layout with live attributes, combat values, HP/SP, identity and XP. Existing stat-allocation callbacks are preserved. The settings views retain the working presets, camera/fog/range controls and real audio sliders. The no-guild state displays a hint rather than an empty member table. Unsupported guild-base and unused option placeholders remain disabled.

Desktop windows have finite dimensions, draggable headers, focus ordering and independent X/Escape dismissal. Larger content scrolls inside its own frame; the close control stays reachable. Safe-area and touch sizing reuse the existing Nexoria layout support. Modal hide/restore remembers each independent view. Password clearing and the existing NPC escape/refine cancellation semantics are retained. A private-dialog teardown recursion discovered by the new regression was fixed before delivery; recovery scene files from the interrupted Editor session were preserved under `Assets/_Recovery` and excluded from source control.

## Validation

Final result and artifact metadata are recorded in the accompanying evidence JSON. Logs and full captures are local under `build/c38-*`.

- Native `storage_runtime` and `typed_inventory_wire`: 2/2 passed. No new native gameplay changes require a server rebuild.
- Snapshot Unity compilation and regression packages: 34/34 passed (`build/c38-offline12.log`). Nexoria UI: 196 checks; remaining windows: 448 checks, including all 90 catalogue windows rendered/opened/closed, real X raycast targets, independent character/guild views, dragging, modal restore, original stat callback, NPC escape answer, password clearing, independent private whispers and real social/guild helper routes. The existing 21 native operation-window gallery also passed.
- The full active project passed the same UI/remaining-window checks before player builds. This does not claim a clean unrelated full-world Console: the isolated source snapshot lacks some world/VFX/ocean assets, and unrelated existing import/plugin diagnostics are outside C38.
- Real LAN native-item flow: **1,210 checks and one relogin passed** (`build/c38-basic-live3.log`). It checks character/skill/sound windows before and after relogin alongside HUD, shop buy/sell, equipment and storage deposit/withdrawal. It compares the complete saved instance/count/location dictionary and exact wallet state, using the existing dedicated test inventory without resetting it.
- The exporter was reproduced in a separate output project. All 84 art hashes match the supplied source files; generated profile/provenance JSON and texture metadata reproduce exactly with existing GUIDs preserved (`build/c38-exporter-proof.json`).

## Delivery and limits

Only the owned local server package `D:/AurigaGlobal/Server2-Local` is used, at `192.168.50.52`: auth TCP30052, CH99 TCP30078, content TCP30090. The stale local stack was gracefully restarted for the live test; its database and playtest accounts were retained. No VPS operation occurred. Earlier portable Windows archives and the previous Unity workspaces are preserved. Existing import/resolver/project-setting changes and recovered scenes are excluded from the source checkin.

The local delivery root is `C:/AurigaGlobal/LocalPlaytest/Nexoria`. Windows delivery uses the new `Nexoria-Windows-Local-r6.zip`; Android uses `Android/NexoriaLocal.apk`; iOS uses `iOS/Unity-iPhone.xcodeproj`. The guide is `HELYI-TESZT.txt`. Private account details remain separately on the server machine; they are not embedded in the portable archive.

Physical Android/iOS gameplay, Mac signing/linking and external-device LAN connectivity remain acceptance work. The simulated mobile captures and local content checks do not establish physical-device usability or firewall reachability. The supplied catalogue also includes designs for gameplay that was never implemented: styling those existing screens does not add their missing backend, and this change does not claim pixel-perfect final parity for every PSD or replace the authentication/character-creation scenes.

Final delivery checks: Windows standalone reached `manual-ready` on `metin2_map_main_razor93`; the second dedicated account retained all 7 item instances/counts and exactly 10000 Yang. The original saved-account file was restored. Windows/Android player builds and the unsigned iOS export completed; final content catalogue, APK manifest/two-ABI cipher/STL and iOS LAN/native-library checks passed. Existing compiled Addressables content was reused for this player/resource refresh. The Editor returned to Windows and all ten pre-build settings files were verified/restored against the captured baseline.

Windows r6: **410,941,493 bytes**, SHA256 `6c24814d6601a1b99e32f68d42a5e297e4082526723f8692b6b98a8a5282c9d9`, ZIP CRC passed. Android APK: **702,861,825 bytes**, SHA256 `8d51349302f67d3a4484e1f8eceaadcaf5d8a6a3efc5b6c3e5f7fe9c6415ed16`.
