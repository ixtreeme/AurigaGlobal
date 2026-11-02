# C16 — Kőbehelyezés: szerver és Unity ellenőrzése

2026-11-02. Natív ág `codex/mission-engine`, Unity 6.3 `6000.3.27f1`,
UVCS `/main/codex-mission-engine`, kiinduló cs49, elkészült cs50.
Döntés és határok: [IC-044](../../adr/item_combat_044_socket_insertion.md).

## Eredmény

**I → behelyezhető kő → Behelyezés**: saját leltárbeli fegyver/testpáncél
választása, pontos szerveres esély, sikeres és törött kimenet, megerősítés.
Siker és szabályos kudarc is egy követ fogyaszt. A minőség, a kő ItemKey-je
és példányszintje, a céltárgy többi állapota megmarad. A source debit,
socket-csere és receipt egy SQL itemtranzakció; wallet nem változik.
Teli leltárban sem kell új kőpéldánynak helyet foglalni.

## Ellenőrzések

- GameServer Release és a kapcsolódó natív tesztek lefordultak.
- **23/23 CTest-csoport sikeres**: új insertion **34**, korábbi socket stone
  **85**, maintenance **36**, socket service **102**, persistent item **188**,
  továbbá inventory/ground, token/container, shop/trade/craft/equipment/Mission
  és kapcsolódó szervercsoportok.
- Saját MariaDB-sandbox: **1281 ellenőrzés**, plusz az ItemBench MariaDB-teszt.
  Siker és fogyasztó kudarc atomi, CAS-hiba mindent visszagörget és nem ír
  sikerreceiptet. A sikertelen dobás replay-e nem dob újra. Módosított
  rögzített dobás és régi session elutasított. Visszatöltéskor pontos a
  source ID/count/revision, target socket/grade/level/attributes és wallet.
- A domain 0/100% végpontjai és a 3000/3001 bp döntési határ ellenőrzött.
  Azonos előnézet mindkét döntéshez, eltérő digest a konkrét eredményhez.
  Normál/arany hely, fegyver és testpáncél, csoportütközés, kötés,
  revision-wrap, foreign/escrow/equipped bemenet, hibás esély/dobás és
  teljes/részhalom ellenőrzött. Nem ábrázolható egyedi kötés nem vész el.
- Unity C#-fordítás és **4168 új ellenőrzés**: natív GC235 v2 golden, minden
  csonkolás/TCP-vágás, esély/slot/ItemKey/szint/minőség/kudarcállapot
  bizonyítás, tényleges inventorygomb és megerősítés, pending, siker/kudarc,
  stale/reset és normál runtime dispatch. Kliens nem küld esélyt vagy dobást.
- Minden korábbi Unity-csoport is átment: maintenance **4763**, token **2785**,
  container **3429**, Mission **3871**, equipment **708**, crafting **3699**,
  trade **4553**, shop **4044**, inventory **4179**.
- **5/5 valódi Unity TLS-fázis, 267 ellenőrzés.** A helyi konfiguráció
  meglévő **85%** esélyét a szerver pontosan idézte; ez nem lett átállítva.
  A vegyes eredményt kereső ciklus **4 próbából 3 sikert és 1 kudarcot** látott.
  Azonos confirm nem fogyasztott és nem dobott újra, módosított cél/régi
  nonce/inventorymozgatás a korábbi előnézettel nem írt.
- A behelyezett +3 kő felszereléskor pontosan **12000 raw** védelmi bónuszt
  adott. A C15 kiemelés megőrizte típusát/szintjét; tisztítás után a slot
  ugyanúgy normál üres lett, az arany másik hely nem változott. Kiemelés
  után felszerelve a bónusz eltűnt, a leltárban maradó kövek nem adták tovább.
- A 180 cellás teli leltár 178 példányból állt a magas célfegyver miatt.
  Részhalom egy darabja és az utolsó forráskő is megfelelően fogyott;
  arany-only forrás a normál helyet átugrotta, a minőség mindkét eredménynél
  megmaradt. Teljes fogyás után is stabil a receipt/replay.
- Konfliktus, hiányzó megfelelő hely, rossz céltípus, minimumszint, szilánk,
  felszerelt/hiányzó cél és nem szolgáltató forrás nem fogyasztott. Bound
  kő unbound fegyverbe tiltott; saját bound célban mindkét kötés megmaradt.
- Relog után minden ID, hely, darab, szint, binding, bónusz, socket és Yang
  pontosan egyezett. Az előző belépés nonce-a nem működött.

## Tesztprojekt és javított újrafuttatás

A végső Editor az aktuális forrás külön másolatán futott. **5414 forrásfájl**
hashével egyezik a kanonikus projekt; **1710 C# fájl** a snapshotban is
byte-azonos. A grafikus batch Editor kilépett. A
`Logs/typed-insertion-preview.png` vizuálisan ellenőrzött; a 30%-ot mutató
kép a natív golden példája, az élő helyi konfiguráció 85%.

A source-only másolat automatikus UI-katalógusépítője a hiányzó atlaszok
miatt batch-ben kilépett. A `PrepareUnityValidationSnapshot.py` már a valódi
UI sprite-katalógust, manifestet, atlaszokat és további textúrafüggőségeket
is másolja, metával együtt. 1089 releváns fájl, 15,6 MiB; nem lett kikapcsolva
a produkciós ellenőrzés, és nem készült helyettesítő rajz. GUID-kereséshez
rg-t használ, a nagy modellek összes metaadatának lassú Python-beolvasása helyett.

A metadata-golden frissítése csak a behelyezhető kő új Socket actionjét
rögzíti; natív és Unity fixture ugyanaz. A korábbi GC235 v1 golden továbbra
is olvasható, a szerver GC235 v2-t küld. Külön tesztfixture-javítás volt a
teljes Item-array mezők és a tényleges +3 forrásszint megadása, illetve az
aktuális ch99-es esély ellenőrzése egy beégetett 30%-os feltételezés helyett.

Az eredeti ProjectSettings hash változatlan:
`81b962db8bbddd8f97b48fc74e73e6f06b275e4153465f0bea5998f722032c98`.
A felhasználói scene/connection, csomagmanifest/lock, AI- és egyéb
projektbeállítások megmaradtak és kimaradnak a C16 check-inből.
Standalone player, teljes világasset-build és FreeBSD-build nem futott.

## Helyi csomag és újrafuttatás

`D:\AurigaGlobal\Server2-Local`, dedikált PC #1610, térkép41, kereskedő
közelében. Additív katalógus, összes tárolt tulajdonos ellenőrzése a pin
frissítése előtt, azonosított saját karakter minden fixture-reset előtt.
Régi item-/Mission-receiptek megmaradtak.

- Katalógus/bináris/manifest mentése:
  `run/deployments/socket-insertion-20261102T202955`.
- Végső élő fázisok mentése: `run/insertion-live/20261102T205204`.
- Telepített ItemBenchTool és manifest mentése:
  `run/manager-updates/socket-insertion-20261102T205613`.
- GameServer SHA-256:
  `67f8281b36e1bdafc0fda26afb440a11a30cc00c3f119d258c94c949e27f919d`.

A kézi fixture 1000 Yangot, +3 normál/arany/armor-only köveket és egy +1
fegyvert tartalmaz zárt, normál üres és arany üres hellyel. A korábbi C15
eszközök/feltöltött cél és C14 pénzjegyek, ládák, kulcsok, gear, italok,
anyagok is megvannak. Az 1/99-es csatornák öt saját szerverfolyamata fut.

Újrafuttatás: `SeedInsertionUnityLive.py` leállított saját csomaggal,
privát C15 alapfixture-rel és natív toolokkal; külön
`PrepareUnityValidationSnapshot.py` projekt, batch Editor és
`InsertionUnityLiveSmoke.py`. A vegyes kimenetű ciklus 1–99%-os tesztesélyhez
való; az aktuális 85%-ot változtatás nélkül használta. A privát adatok és
jelszavak az ignorált build könyvtárban maradnak.

Naplók: `build/socket-insertion-first-build.log`,
`build/socket-insertion-final-verified-ctest.log`,
`build/socket-insertion-editor6.log`, `build/socket-insertion-live-harness6.log`,
`build/socket-insertion-snapshot-verification.json`.
