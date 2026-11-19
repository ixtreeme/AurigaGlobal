# C42 — A normál karakter leltára és a tényleges adatkonverzió

2026-11-10. Csak helyi munka. Szerver: `20261103`; Unity:
`/main/unity63-integration-20261103`, Editor `6000.3.27f1`.
Nem történt push vagy Unity-checkin.

## A bejelentés oka

A `szabika` karakternek (pid 3) 76 régi tárgypéldánya van:
38 az INVENTORY, 8 az EQUIPMENT és 30 az EXTRA_INVENTORY tárolóban.
Nincs hozzá `item_owner_fence` játékostulajdonos az új rendszerben.
A `MissingOwner` szerverág ezért nem küldött natív leltárcsomagot, miközben
a Nexoria felület már a natív leltárablakot nyitotta meg. Az ablak határozatlan
ideig várt. A C40/C41 játékpróbák külön natív próbakaraktereket használtak;
azok sikere nem bizonyította a normál karakterek adatátállását.

A teljes régi proto és a példányok átvitele továbbra is nyitott feladat.
Az új item-műveletek runtime-megvalósítása és a régi játékadatok konverziója
két külön kiadási feltétel.

## Szerver és Unity javítás

A `TypedItemLogin` tárolja az aszinkron betöltés feloldott módját. A régi
tulajdonoshoz a szerver `typed_item_backend migration_required` parancsot
küld a meglévő CHAT_TYPE_COMMAND csatornán. A módot az Entergame előtt
befejeződő betöltés után is megőrzi. A kikapcsolt tárgyszolgáltatás külön
`unavailable` állapot. A natív leltár bináris elrendezése nem változott.

A Unity bootstrap és a külön Nexoria leltárablak megjeleníti ezt az állapotot.
Nem hoz létre üres natív tulajdonost vagy hamis leltárcsomagot. Ismeretlen
kapacitásnál a zöld kitöltés is eltűnik. A tényleges natív snapshot felülírja
az állapotot; későn érkező legacy jelzés nem rejtheti el. Kilépéskor a mód
törlődik. Az eredeti, különálló Nexoria ablak elrendezése megmarad.

Az új szerverbináris a helyi csomagba került, ellenőrzött manifesttel és
visszaállítható másolattal a `run/deployments/c42-item-backend` könyvtárban.
A Unity forrása az aktív munkatérben módosult. E munkához nem készült új
Windows/Android/iOS kiadási csomag; a korábbi R11 csomagok változatlanok.

## Konzisztens forrás és teljes elszámolás

Saját supervisorral leállított helyi szerver mellett teljes mentés készült:
`D:/AurigaGlobal/Server2-Local/run/item-migration/20261110-source-c42`.
Új MariaDB-sandboxban 230 tábla visszaállt; a konverziós 119 tábla
metaadata és összes exportált cellája pontosan egyezik.

| Forrás | Darab |
| --- | ---: |
| Régi proto | 10 986 |
| Régi példány a három példánytáblában | 3 259 |
| Megőrzött összmennyiség | 320 605 |
| Meglévő natív definíció | 506 |
| Meglévő natív példány | 1 202 |

A determinisztikus konverter már mind a 22 natív kategóriát és kiadott
számlálóit kezeli. Külön besorolást kap a lőszer, a kovácssegéd, a stóla/rúna
és a speciális bónuszművelet. A meglévő natív példányazonosítók foglaltak;
az ütközés auditált átazonosítás, nem felülírás. A forráspéldány külön
forrásazonossága és teljes sorhash-e megmarad. Ez még tervezett kiosztás:
a végső importnak a tartós allokátor foglalását is végre kell hajtania.

A friss teljes terv 49 hiányzó tulajdonost, 295 hiányos szintcsaládot és
további recept-/hivatkozási hibákat is számon tart. Sem törlés, sem hiányzó
szintek vagy viselkedések kitalálása nem történt.

## Az első tényleges tartalomfordítás

A `convert_plain_materials.py` mezőnként ellenőrzött anyagfordító.
Lefordítja az öt material-altípust, méretet, halmozhatóságot, szint- és
osztály/nem-korlátot, kereskedési tiltásokat, vételi/eladási árat, konkrét
Unity-ikont és szövegeket. A SAVE a natív tartós mentésre, a LOG a natív
tartós műveleti receiptre vezethető át. Minden további nem nulla, ismeretlen
mező, bizonytalan limit vagy hiányzó asset explicit akadály marad.

A `convert_passive_catalog.py` az anyag- és passzív questtoken-fordítást
egymás után, közös katalóguson végzi, ezért profilazonosítóik nem ütköznek.
A questikonokhoz a kliens által ténylegesen használt kanonikus assetút kerül.
A meglévő natív v30 katalógus is megőrizhető; nem szükséges v7-ként kezelni.
Az SQL-stager az összes jelenlegi táblát és a 000–036 migrációsort lefedi.

| Összevont, elkülönített próba | Darab |
| --- | ---: |
| Lefordított régi alapanyag | 427 |
| Lefordított régi passzív questtoken | 2 |
| Natív definíció a próbában | 935 |
| Tételesen elszámolt régi protósor | 10 986 |
| Még le nem fordított protósor | 10 557 |
| Átvitt játékospéldány | **0** |

A 935 tárgyas jelölt új SQL-sandboxból natívan visszaolvasva ugyanazt a
katalógushash-t adja. A korábbi natív katalógus mind a 95 táblájának minden
sora benne marad; a kiadott számlálók nem csökkennek. A teljes forráslista
`prototype_ledger`, a tulajdonosonkénti hiánylista `owner_readiness` néven
szerepel a privát `20261110-passive-c42.json` eredményben.

**Ez nem telepített teljes adatkonverzió.** A jelölt `installable_release=false`.
A 429 tartalomsor még nem jelenti a hozzájuk tartozó példányállapotok,
receptek, droppok, boltok, questhivatkozások vagy klienskiadás átállását.
A `szabika` leltárához még 49 különböző forrásprototípus fordítása és a
teljes példányállapot tartós importja hiányzik. A következő nagy tartalmi
egység a fegyver/páncél/ékszer családok, szintek, bónuszok és socketek
veszteségmentes fordítása, majd a használt fogyaszthatók és műveleti tárgyak.

## Lefuttatott ellenőrzések

- GameServer natív fordítás sikeres.
- CTest: 5/5 (`typed_inventory_wire`, `persistent_items`,
  `legacy_item_migration`, `quest_token_migration`, `plain_material_migration`).
  A migrációs regressziók között 53 teljes mapping/ledger teszt és 7 natív
  alapanyagteszt szerepel.
- Unity runtime és editor assembly fordítása sikeres a pinned fordítóval.
- Izolált Unity 6.3 snapshot: 37/37 offline item/UI validátor;
  ezen belül a leltár/raktár validátor 35 ellenőrzése.
- Valódi helyi TLS-játékmenet Unityből: külön legacy próbakarakter két
  belépéssel, majd natív próbakarakterre váltás, állapot- és ablaktulajdonlás,
  valódi natív leltár és késői státusz ellenőrzése; a végső futás 2317
  ellenőrzése sikeres.
- SQL-bizonyíték: a normál karakter 76 tárgyának minden mentett mezője
  változatlan. A teljes régi leltár-/mount-/céhraktár táblák az egyetlen új,
  hét darabos fixture-halom kivételével változatlanok; mindkét proto változatlan.

Fő bizonyítékok: `build/c42-source-restore/restore-verification.json`,
`build/c42-native-tests-final.log`, `build/c42-offline2.log`,
`build/c42-legacy-owner-live-final.log`, `build/c42-preservation-proof.json`,
`build/c42-final-catalog-proof.json`, `build/c42-passive-stage/catalog-stage-result.json`.
Az izolált Unity-projekt teljes világtartalmat nem tartalmaz; ez nem teljes
világjelenetes vagy mobil eszközös próba. A normál karakter teljes natív
leltárát jelen munkával még nem lehet játszhatóan tesztelni.
