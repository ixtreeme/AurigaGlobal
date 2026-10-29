# Példánybónuszok — hozzáadás, váltás, védelem és szerkesztés

2026-10-29, `codex/mission-engine`, kiinduló HEAD `0c04033b`.
[IC-031 szerződés](../../adr/item_combat_031_instance_attributes.md).

## Elkészült

- Nyolc típusos bónuszhely: legfeljebb öt normál, két ritka és egy külön
  nyolcadik. Kapacitásuk tárgyprofilonként állítható. A ritka és nyolcadik
  hely közösen tiltja az ismétlést; normál és ritka között ugyanaz a
  bónusz külön hozzájárulásként megmarad.
- Külön `Instance` bónuszforrás a generált, közös regiszterben. A korábbi
  34 ID és egység változatlan, forrásmaszkjuk bővült. Statikus alap- vagy
  fejlesztési profilt nem használunk példányadatként; régi APPLY-kódot a
  runtime nem old fel. A teljes régi bónuszkészlet leképezése még nyitott.
- Típusos profilok a csoportkapacitáshoz, statikus bónuszok kizárásához,
  választható típusokhoz, súlyokhoz és minőségi szintenkénti értékekhez.
  Az érték előjeles int64, mértékegységét kizárólag a regiszter mondja meg.
- Hozzáadási és teljes csoportváltási terv, explicit célkategória-,
  minimumszint- és előzetes darabszám-korlátokkal. Egy próbában több sor
  is kérhető, de minden új sor elkészül a fogyasztási terv előtt.
- Súlyozott típusválasztás a már kizárt halmaz nélkül, külön minőségi
  dobással. Nincs rejtett RNG vagy részleges állapotírás. A hiányos
  valószínűségi tartományt nem normalizáljuk. A választott típus kisebb
  maximuma minőségi clampet jelent, nem érték- vagy átlagolási trükköt.
- Védelem hozzáadása, áthelyezése, törlése. Első védelemhez tele normál
  csoport kell; váltás ugyanazt a védett helyet és értéket őrzi meg.
  Áthelyezés egy korlátos választás más foglalt helyek közül, újradobó
  ciklus nélkül. Hatástalan vagy hibás művelet nem fogyaszt.
- Fogyasztó és nem fogyasztó sikertelen hozzáadás külön explicit
  szabály. A befogadás közös a foglalatkarbantartással: owner/session,
  operation ID, revision, inventory, zár/csere/felszerelés, stack, szint,
  kaszt/nem, harc és observer állapot ellenőrzött.
- Teljes, kulcshoz kötött little-endian v1 állapotcodec. Megőrzi a lyukas
  helyeket, negatív értékeket és védett indexet. Szigorú verzió, bool,
  hossz, EOF és állapotvalidálás. Ez nem élő DB-writer vagy klienspacket.
- V24 friss katalógus: 85 tábla, 000..030 migrációs lánc. Snapshot,
  regiszter-ellenőrzés, kanonikus hash, SQL-olvasó és atomi adminisztratív
  tartalommentés frissítve. A meglévő v7-en ezt nem futtatjuk helyben.
- Új „Példánybónuszok” Qt-lap a fegyver/páncél/ékszer/kosztüm számára;
  név szerinti bónuszválasztó és a helyes pont/%/egész egység. Külön
  fogyasztható-űrlap a hozzáadókhoz, váltókhoz és védelmekhez. Másolat
  eltérő szabálya nem írja át az eredetit; inaktív piszkozat nem mentődik.

## Ellenőrzés

| Ellenőrzés | Eredmény |
|---|---:|
| Natív példánybónusz-ellenőrzések, normál / ASAN | 137 / 137 |
| Teljes szerver CTest, egymásutáni futás | 133/133 |
| Érintett itemmodulok ASAN CTest alatt | 30/30 |
| Qt/modell/izolált SQL-kör normál helperrel | 76/76 |
| Qt/modell/izolált SQL-kör ASAN helperrel | 76/76 |
| Friss séma belső tesztjei | 40/40 |

A teljes bónuszút hozzáadásból, védelemből, normál váltásból, védelem-
áthelyezésből/-törlésből, ritka és nyolcadik bónuszból, majd codec
visszaolvasásból áll. Külön vizsgáltuk az esély-/súlyhatárt, hiányzó vagy
fölösleges döntéseket, aliasolt/idegen állapotot, teli csoportot, egyetlen
zárolható helyet és minden csonkolt payload-prefixet.

Három külön ponton kiváltott SQL-hiba igazolja a teljes rollbacket:
bónuszérték, szolgáltatás-esély és tárgy–profil binding. A profilok,
gyermekek, protók és kategóriaszámlálók sem maradnak félkészek. A Qt
el nem kapott signal-kivételei hibának számítanak. Az űrlapokat képen is
ellenőriztük; a telepített szerverkezelőt nem cseréltük.

Független Python és natív C++ katalógus-fixture:
`23e2fbfcd54f352facafe8ffa1d02933bf62e8756992aa58029563703f386766`.
Függetlenül kódolt teljes példánybónusz-payload SHA-256:
`7ca6a95566deb1d66ccbbcbcb7cbc908b90118dc549b7a77f2200f438eb8625d`.

A meglévő egytárgyas v7 felhasználói katalógus v24 jelöltjét külön owned
MariaDB-sandboxba importáltuk, majd natívan visszaolvastuk. Kulcs, cella és
számláló megőrzött; hash:
`86eeba5ab05c284f7ea79fde210717650d5e48325cd27bfc26899bca3475019e`.
Ez nem a régi teljes item_proto vagy a játékospéldányok konverziója.
Az aktív v7 adatbázis 33 táblája és egy felhasználói protója változatlan.

Windows/MSVC és MariaDB 11.4.13 teszt történt, FreeBSD- vagy Unity-
játékteszt nem. A korábbi párhuzamos Mission SQL-instabilitás gyökérokát
ez a csomag nem javítja; a jelenlegi egymásutáni Mission-teszt sikeres.

## Megőrzött adatok és hátralévő kapuk

A fagyasztott mentés normál/ritka/nyolcadik sorsolótáblái 66/15/7 sorosak.
A célzott szolgáltatásleltár 36 protót fed le; a külön jelmezműveletek is
azonosítva, de még nincsenek automatikusan késznek minősítve. 132 régi
példánynak van bónusza, jelenlegi zárolt példány nincs. A mentésben 1587
proto `addon_type` mezője nem nulla; ez önmagában nem teljes viselkedési
besorolás. A kapcsolt Gauss-addon külön auditált lelet és nyitott port.

**A játékban még a régi itemmodell fut.** A domain kész használati tervet
ad, az új valódi owner-, gazdasági és DB-writer F3/F4. A receipt, a
fogyasztás és a teljes bónuszállapot-csere egy commitba kerül majd;
CommitUnknown után operation ID szerinti egyeztetés kell. Statpublikálás
csak commit után történhet. Típuslista-/kapacitás-/binding-változás előtt
a már kiadott példányokat is ellenőrző publikálási kapu szükséges.

A leltár minden forrássorát nem konvertált állapotban hagyja. A jelenlegi
sorsolási értékek nem írják át a korábbi, továbbra is érvényes értékeket;
teljes APPLY-mapping és valódi mentési-visszatöltési próba nélkül nincs
játékosadat-migráció. Sem távoli gép, sem Unity nem módosult.

Következik a kapcsolt addon és a fennmaradó különleges bónuszműveletek
(jelmezreset és példányadatos adók, stóla/rúna), továbbá fejlesztési
segédanyagok. Ezek után zárható az F2 kapu; F3 tartós példánymodell,
F4 fogyasztók, F5 protokoll/Unity, F6 teljes offline konverzió és F7 helyi
játékpróba továbbra is szükséges.
