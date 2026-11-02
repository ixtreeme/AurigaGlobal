# Doboz- és kulcsos ládanyitás — C13

2026-11-02. Szerver `codex/mission-engine`, Unity `/main/codex-mission-engine`,
Unity 6.3 `6000.3.27f1`, **Unity changeset 47**.

## Eredmény

A sima doboz és az egyező kulcsot kérő láda teljes gazdasági nyitása elkészült.
Egy tranzakció fogyasztja az inputokat, adja az összes tárgyat/Yangot és
rögzíti a kisorsolt eredményt. A megtelt leltár vagy a Yang-korlát nem
használható kedvező eredményre újrasorsoláshoz: minden lehetséges eredmény
befogadása a loot RNG előtt történik. Az egylövetű preview és receipt védi
az ismétlést, az owner-fence a közben megváltozó inventoryt.

Unity: **I → láda kiválasztása → Nyitás**, jutalmak és esélyek, tényleges
kulcsválasztó, megerősítés, szerveres eredmény és új preview. A legutolsó
felhasznált kulcs elfogyásakor a következő preview másik megfelelő kulcsot keres.
A kliens nem fogyaszt vagy jutalmaz helyben.

Határok és wire: [IC-041](../../adr/item_combat_041_container_opening.md).
Most tárgy és Yang adható, ritka példánybónusz-sorsolás nélkül. Ha a profil
akár egy nem támogatott XP/spawn/státuszhatásos sort tartalmaz, az egész
nyitás elutasított, nem csupán az adott sor. A PetEgg, scripted questes
nyitás, teljes legacy konverzió és a további tárgyhasználatok nyitottak.

## Ellenőrzések

- Release GameServer, ItemBenchTool és célzott tesztprogramok lefordultak.
- **16/16 CTest-csoport**: `container_opening`, `containers`, `persistent_items`,
  `typed_inventory_wire`, `typed_ground`, `item_persistence_completion`,
  `item_persistence_mariadb`, `npc_shops`, `player_trades`, `crafting`,
  `equipment_stats`, `item_missions`, `typed_item_grants`, `shop_lifecycle`,
  `exchange`, `server_package_tools`.
- Az új nyitási teszt a közös `ServerTests` buildcélhoz és az opcionális
  sanitizerhez is kapcsolódik; az újragenerált cél külön is sikeres.
- Nyitási tervező **26**, meglévő konténerdomain **72**, tartós tárgy **188**,
  MariaDB-s integráció **1216** ellenőrzés. Teljes item/Yang/receipt együttírás,
  replay és módosított replay, wallet-CAS teljes rollback és hiányzó receipt,
  régi session tiltása, pontos újratöltés, RNG előtti hely-/pénz-/profilkapu,
  kulcsazonosság, saját kötés, minimumszint, egész input felszabaduló helye,
  nyolc bizonytalan tárgysor és a szabályos üres independent eredmény.
- Unity **3429** új ellenőrzés: natív golden, minden TCP-vágás és csonkolás,
  hibás mezők, pontos CG233/sequence, Open action, valós dropdown-template
  megnyitása, megerősítés, pending, eredmény, stale és relog/session reset.
- Korábbi Unity-regressziók újra sikeresek: Mission **3871**, equipment **708**,
  crafting **3699**, csere **4553**, NPC-bolt **4044**, inventory **4179**.
- **5/5 valódi TLS-fázis, 122 ellenőrzés**. A teszt a tényleges inventory
  Nyitás gombjáról indul, megerősít és a valódi kulcs-dropdownnal új nonce-ot
  kér. Azonos confirm nem ad új jutalmat; régi nonce és módosult inventory
  elutasított. Rossz kulcs, nem támogatott profil és minimumszint nem fogyaszt.
- A több soros kulcsos jutalom teljesen megérkezett; az elfogyott alternatív
  kulcs után a következő előnézet a megmaradt kulcsot választotta. Üres
  independent eredmény a tényleges körben is előfordult, és egyszer fogyasztott.
- 180 teli cellán a részhalmos forrás nyitása változatlan állapottal NoRoom.
  Egész doboz, illetve doboz+kulcs elfogyásakor a felszabaduló hely fogadta a
  jutalmat. Pontos ID, hely, szint, darab és Yang relog után is megegyezett.
  A nagy Yang-egyenleg potenciális túlcsordulása nem választott helyette
  kedvező tárgyeredményt, és nem fogyasztott.
- A `Logs/typed-container-preview.png` felület vizuálisan ellenőrzött.

## Felügyelet nélküli Unity-futás

A `PrepareUnityValidationSnapshot.py` külön, byte-másolatot készített az
aktuális kódból, a saját Addressables/SBP
csomagokból és a szükséges font/locale/golden/ikon assetekből.
A teszt-Editor grafikával futott `-batchmode` módban, hogy renderelt UI-PNG is
készüljön. **4302 forrásfájl** hashével ellenőrzött snapshot, benne
**1700 változatlan C# fájllal**. A végső futás után az indított teszt-Editor
automatikusan kilépett.

A hideg Editor induláskor törölheti a Temp mappát. A C13 harness a még át
nem vett kérést újraküldi; a `running` jel után nem indít második tesztet.
A batch watchdog az eredményt a snapshot gyökerében is megőrzi, így az
Editor kilépésekor eltűnő Temp nem veszíti el a teszteredményt.
A fixture jelszava és TLS-adatai privát fájlban maradnak, nem kerülnek commitba.
Ez nem standalone játékosbuild vagy a teljes világasset-készlet ellenőrzése.

## Helyi csomag és újrafuttatás

Saját csomag `D:\AurigaGlobal\Server2-Local`, dedikált PC #1610.
A protókat a natív katalógusszerkesztő adta hozzá, a teljes tárolt tulajdonosi
állomány validálása után lépett a pin. Meglévő tartalom nem lett átírva.
Katalógus/bináris/manifest mentése: `run/deployments/container-opening-20261102T151404`;
végső runtime mentése `run/deployments/container-final-20261102T152653`.
Minden fixture-reset előtti állapot és pozíció:
`run/container-live/20261102T153511/`. A régi Mission-receiptek megmaradtak.

GameServer SHA-256:
`804eedbf57604eb885b6d2a66326365ab4075ecb5078d48e73b346f6b6ba8154`.
A kézi fixture 1000 Yanggal, normál és kulcsos ládákkal, két megfelelő
kulcsfajtával, rossz kulccsal és külön nem támogatott dobozzal próbálható.
A korábbi +1 fegyver, +0 vért, italok és anyagok is rendelkezésre állnak.
A helyi szerver mind az öt saját folyamata fut.

A telepített szerverkezelő `ItemBenchTool` segédje is frissült, hogy a
Tárgyteszt már tartósan tudja kezelni a ládákat. Az eredeti manifest-hash
ellenőrzése és mentés után csak ez a helper és a hozzá tartozó manifesttag
változott; az új telepített helperrel a leltár betöltése és a gear adása is
ellenőrzött. Mentés: `run/manager-updates/container-opening-20261102T155056`.

Újrafuttatók: `SeedContainerUnityLive.py` (saját leállított csomag és privát
alapfixture), `PrepareUnityValidationSnapshot.py` (friss külön célmappa), majd
a snapshoton batch Editor és `ContainerUnityLiveSmoke.py`. Az eredeti
projektbe nem került tesztkérés. Az eredeti ProjectSettings, csomagmanifest,
lock és AI-beállítások, valamint a felhasználói jelenet/kapcsolati asset és
további projektbeállítások változásai megmaradtak és kimaradtak a commitból.

Naplók: `build/container-opening-secure-build.log`,
`build/container-opening-final-ctest.log`, `build/container-opening-editor-complete.log`,
`build/container-opening-live-harness-verified.log` és Unity
`METIN2_TYPED_CONTAINER_*`. FreeBSD és standalone player nem futott.
