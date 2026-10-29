# ADR IC-030 — Foglalatkarbantartás és teljes ékszerhasználati terv

2026-10-29. F2, IC-012/IC-014 5. szelet; IC-021 folytatása.
Kiinduló HEAD: `501ca1f8`. Csak helyi fejlesztés, friss tesztséma.

## Forrás és megőrzött viselkedés

- `ItemSystem_LegacyBridge.cpp::UseItemEx`, `USE_CLEAN_SOCKET`: minden
  törött foglalat tisztítása, egy forrásdarab fogyasztása. A régi ág közben
  tömöríti a foglalatokat és a maradék helyeket normál minőségre írhatja.
- Ugyanitt `RefineItem`, `USE_DETACHMENT`: minden ép beépített kő visszaadása,
  helyükön törött foglalat, egy tekercs fogyasztása. A tárgyvisszaadás és a
  célírás külön lépések; nincs az összes kőre közös helyfoglalási kapu.
- `UseItemEx`, 71109/72719: az utolsó ép kő visszaadása, helyén normál üres
  foglalat, egy forrásdarab fogyasztása. Sikertelen visszaadáskor nem fogy.
- `USE_ADD_ACCESSORY_SOCKET`, `USE_PUT_INTO_ACCESSORY_SOCKET/BELT_SOCKET`:
  cél- és anyagellenőrzés, kapacitás-/töltésnövelés. A már elkészült
  `AccessorySocketProfile` a típusos esély, egység, idő és kompatibilitás
  igazságforrása; nem készül párhuzamos új szabálytábla ezekhez.

## Új szerződés

A fogyaszthatók negyedik, kizárólagos viselkedése a névvel regisztrált
`SocketServiceProfile`. Három zárt művelet: `CleanBroken`, `ExtractAll`,
`ExtractLast`. A közös fejben csak stabil identitás, művelet és darabszám
van; nincs tetszőleges target callback, régi vnum vagy value-tömb.

A tisztítás minden törött helyet ugyanott ürít ki. A teljes kiemelés minden
ép követ visszaad és törött helyet hagy. Az utolsó kő kiemelése a legnagyobb
foglalt indexet üríti. **Minden megváltozott hely megőrzi a minőségét**;
nincs tömörítés vagy aranyról normálra visszaírás. Ez a régi reprezentáció
hibájának szándékos korrekciója. A zárt, üres és már törött egyéb helyek
nem változnak. Hatástalan művelet nem fogyaszt forrást.

Az egész foglalatállapot közös validátora ellenőrzi a formát, létező
beépített ItemKey-t, fejlesztési szintet, fegyver/testpáncél-kompatibilitást,
minőséget és a kizárócsoportokat. A behelyezés is ezt a validátort hívja.
Ismert, de retire-olt beépített követ ki lehet venni; nem vész el az adat.
A forrás és a céltárgy retire állapotban nem használható.

Egy kiemelt kő egy darabot, a beépített ItemKey-t és példányszintet hordozza.
Kiemeléshez minden eredményhez egyedi új instance ID és foglalt inventory-
helytoken kell. Hiányzó, dupla, fölösleges vagy a bemeneti tárgyakra mutató
foglalás az **egész** tervet elutasítja. A domain a token valódiságát nem
állítja: a szerveres owner ellenőrzi és foglalja a helyet, majd az aktuális
inventory revision ellen újraellenőrzi a commit előtt. Nincs földre dobó
vagy részleges AutoGive fallback. Stackbe olvasztás e műveletben nincs.

`PlanAccessoryService` ugyanazzal a teljes befogadással burkolja a meglévő
ékszerfoglalat-unlock/charge matematikát. A cél saját profilja és az anyag
kulcsa dönt; nem kliens által megadott esély. A szerver rögzített dobást ad,
0/100% végpontnál nulla dobással. Kudarc esetén a profil szerinti anyag
fogy, állapot nem nő. Tele kapacitás, tiltott anyag, időzített/örök töltés
keverése, kevés darab és hibás állapot elutasított.

Mindkét terv birtokolja a sessiont, operation ID-t, ruleset-/inventory-
revisiont, mindkét példány teljes snapshotját és a katalóguspint. Csak
tulajdonolt inventory-tárgy, nem kifutott revision, külön source/target,
zár-/csere-/felszerelés-mentes állapot használható. A forrás minimumszintje,
kaszt-/nem-/harcpolitikája és az observer-tilalom is ellenőrzött. A cél
viselhetősége nem feltétele a karbantartásnak.

## SQL, szerkesztő, kiadási kapu

Friss v23 lánc: `000..029`, `auriga.item_catalog.v23`. Új, négymezős
`socket_service_profile`, nullable `item_consumable.socket_service_profile_id`,
pontosan egy viselkedést kikényszerítő CHECK és FK. Kanonikus hashben a
fogyasztható új hivatkozása és az 50-es logikai tábla is szerepel.

A Qt-fogyasztható űrlapon művelet és darabszám választható. A másolt tárgy
eltérő profilja nem módosítja az eredetit. Típusváltáskor a régi kapcsolat
kiürül és a gazdátlan profil takarítható; inaktív űrlappiszkozat nem mentődik.

Ez F2 domain/katalógus/szerkesztő. Nem élő inventory-writer. F3/F4-ben egy
receipt + fogyasztás + teljes foglalatcsere + minden visszaadott kő létrehozása
egyetlen gazdasági commit. CommitUnknown esetén tartós operation ID szerinti
egyeztetés kell. Statpublikálás csak sikeres commit után, egy forrás teljes
cseréjével történhet. A valódi tartalom és beágyazott régi foglalatok offline
konverziója F6. Az aktív v7, a telepített app, Unity és távoli gép érintetlen.
