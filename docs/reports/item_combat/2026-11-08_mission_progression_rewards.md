# C35 — Mission XP-, szint- és fiókvaluta-jutalom

A `20261103` szerverágon és a `/main/unity63-integration-20261103`
Unity-ágon elkészült a közös jutalomátvétel. A párosított Unity-mentés **cs:96**. A Mission-napló és a valódi
Lua-quest ugyanazt a pinned, tranzakciós műveletet használja. A kliens
megerősítés előtt látja az XP-t, közvetlen szintnövekedést és fiókvalutát,
majd kizárólag a szerver tartós eredményét alkalmazza.

Részletes szerződés: [IC-070](../../adr/item_combat_070_mission_progression_rewards.md).
Fordítható minta: `config/mission_engine_progression.example.json`.

## Helyi Unity-játékpróba

A pinned Unity **6000.3.27f1** Editor, a valódi helyi TLS/session útvonal,
natív GameServer és Database, tényleges napló- és NPC-párbeszédgombok
szerepeltek. Három küldetés, három karakter és két fiók vett részt.
**2211 ellenőrzés és kilenc újrabelépés sikeres.** Az ellenőrzésszám a rendes
játékospont-csomagok dispatchét is tartalmazza, nem ennyi külön küldetést.

| Eset | Igazolt eredmény |
| --- | --- |
| XP és fiókjutalom | Az első, tárgyleadást is végző küldetés 100 XP-t és 50 DC-t ad. Az 5. szint, 100 XP és 50 DC újrabelépve is megmarad. |
| Közvetlen szintjutalom | A valódi Lua-párbeszédből átvett második jutalom +1 szintet és 75 DC-t ad. A coroutine már a rendezett tárgyállapotot látja. A karakter 6. szintű, XP-je 0, közös egyenlege 125 DC. |
| XP-ből szintlépés | A mentett erdei ág és időzítő utáni 7300 XP a tényleges XP-táblával 7. szintet és 100 maradék XP-t eredményez, további 25 DC mellett. |
| Rendes játékosállapot | A GC16/17 csomagokból frissülő Unity játékosállapot szintje és XP-je minden jutalomnál egyezik a Mission-naplóval. |
| Másik karakter | Ugyanazon fiók második karaktere 150 DC-t lát, de 5. szintű, 0 XP-s és önálló, még el sem indított küldetéshaladása van. Visszalépve az eredeti karakter teljesített küldetése és fejlődése változatlan. |
| Ismétlés és elutasítás | Az eredeti kérés ismétlése nem ad új tárgyat, XP-t, szintet vagy DC-t. A korai és NPC nélküli átvétel elutasított; a már elutasított kérés háttérhaladás után is elutasított marad. |

A natív SQL-visszaolvasás ugyanazt az eredményt igazolja: **7. szint,
100 XP, 10 statpont, 2 fő képességpont, 3-as fejlődési revízió, 150 DC**.
A két szintlépés HP/SP-növekménye a kaszt 36–44 / 18–22 tartományába esik,
és mindkettő megmarad. A másik fiók egyenlege nulla. Három egyedi reward
ledger és két egyszer feldolgozott gyűjtési esemény maradt.

**1138 korábbi típusos tárgyrekord**, más tulajdonosok pénztárcái és
tanulási adatai, valamint más karakterek Mission-fejlécei változatlanok.
A tesztkarakterek legacy tárgyat nem kaptak.

## Fordítás és regresszió

- A GameServer és Database, továbbá a natív item- és Mission-eszközök
  fordítása sikeres. A közös fejlődési mentés miatt mindkét szerverbináris
  és az új snapshotot olvasó helyi eszközök együtt kerültek telepítésre.
- **57 natív tesztcsoport sikeres**, 133,71 másodperc alatt. Ebben a könyv-,
  pet-, tárgy-, pénztárca- és Mission-regresszió, valódi MariaDB, valamint a
  Mission store elveszett COMMIT-/crash-helyreállítási próbái is szerepelnek.
- Az új natív/SQL-esetek tárgy nélküli, együttes XP-/szint-/fiókjutalmat,
  több szinten átmenő XP-t és maradékot, szint- és HP-túllépést, schema 3
  csomagroundtripet, BOK2 checkpointot, késői hiba teljes rollbackjét,
  idegen fiók és pénztárcatúllépés tiltását, replayt és újrabelépést vizsgálnak.
  A valódi késői legacy UPDATE sem XP-t, sem szintet/statot/HP/SP-t nem
  tud visszaállítani a korábbi állapotra.
- Unity-fordítás és a teljes offline validátorcsomag sikeres. A Mission-
  validátor **3236 ellenőrzése** v1/v2 natív goldent, minden csonkolást,
  TCP-darabolási pontot, valódi naplógombokat és a jutalomelőnézetet vizsgál.
- A csomagpélda fordítása, a globális protokollazonosítók ütközéspróbája,
  a quest API-katalógus és a diff-ellenőrzés sikeres.

Bizonyítékok: `build/c35-native-tests-final.log`,
`build/c35-unity-offline1.log`, `build/c35-unity-live1.log`,
`build/c35-local-sql-verify1.log`, `build/c35-validation-evidence.json`
és `build/c35-deployment-evidence.json`.

## Hatókör és takarítás

Új adatbázis-migráció nem szükséges; a meglévő revíziózott snapshot BOK2
formátuma tárolja a fejlődést, a BOK1 olvasható marad. A régi cash importja
és a normál legacy leltár átállítása nem történt meg. A teljes Mission-
kampány, Mission Studio és a további világ-eseményadapterek külön munka.

A próba forráshash szerint egyeztetett, elkülönített Unity-projektben futott.
Teljes világjelenetes grafikai próba, standalone kliens, FreeBSD és
terheléses próba nem futott. A snapshot korábbi VFX-/Crest-hiányüzenetei
nem ehhez a változáshoz tartoznak.

A külön NPC és Mission-csomag a próba végén visszaállítva; a már publikált
tesztquest csak handler nélküli tombstone-ként marad, megőrizve a tartós
quest-/state-azonosítókat. Az elkülönített tesztkarakterek és auditadatok
megmaradnak. A friss helyi szerver a tiszta bootcsomaggal újraindult; mindkét pinned TLS-végpont elérhető.
