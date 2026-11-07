# C26o–p — Tanulási várakozás kihagyása és teljes pet felhasználása

A szerver `20261103` és a Unity `/main/unity63-integration-20261103` ágán.
Párosított Unity changeset: `cs:86`.

A képességablak **Várakozás nélkül** művelete a meglévő típusos
könyvsegédtárgyból szerzett töltetet használja. A normál fejlesztés továbbra
is kivárja a határidőt. A külön választott kihagyás egy könyvet és egy
töltetet fogyaszt, elfogadott sikertelen próbánál is. A könyv, pet, töltet,
karaktercheckpoint és a következő tanulási határidő egy SQL-tranzakció.
A közös könyves folyamat sorba állítja a közben érkező karakterfejlődést,
és újrabelépés előtt kivárja a régi mentést. Nincs új segédtárgy-kategória
vagy SQL-migráció. A GC253 v7 a töltetszámot és a kifejezett kihagyást viszi.

A katalógus külön etetési szabályt adhat fejlődő pecséthez. Az etetőablak
csak másik, visszahívott, leltárban levő és megjelenéshez nem kötött petet
ajánl. A megerősítés megmutatja a forráspet nevét, szintjét, fokozatát és
tanult képességeinek számát. A gomb **Pet végleges felhasználása**.
Az etetés egyszerre tölti a cél életidejét és jelöli megsemmisítettnek a
teljes forráspéldányt. Az eredeti teljes születési/képességállapot auditként
megmarad a megsemmisített sorban. Normál tárgyfogyasztás ezt nem teheti meg.
A GC253 v8 hordozza az egyértelmű forrásazonosságot; a korábbi v2 etetési
csomag továbbra is dekódolható.

A tárgyszerkesztő új lapja a fejlődő pecsét etetési szabályát is menti.
A mennyiség egy pet, az EXP- és közvetlen újraélesztési változat nem
választható hozzá. Az új Qt-kezelő és natív katalógusellenőrző a helyi
csomagba került, a korábbi állományok mentése mellett. A skinórák egy
tulajdonoson belül közös állapotmásolatot használnak, több skin esetén is.

Ellenőrzések:

- Windows GameServer, ItemBenchTool és CatalogEditorTool fordítás sikeres.
- Kilenc érintett natív próba sikeres: companion, fejlődés, pet care,
  lifecycle, persistence, wire, book runtime, book aid és MariaDB.
  Az SQL-próba a karakter-CAS miatti teljes visszagördülést, a sikertelen
  és sikeres kihagyást, újrabelépést és üres töltettárat is vizsgálja.
  A donor teljes mentett blobja byte-ra azonos marad a megsemmisített sorban.
- Hat pethez kapcsolódó Qt/model próba sikeres, beleértve az új pecsétformát.
- Unity 6.3 fordítás és **7771 pet-, 1640 felszerelési, 340 companion-,
  4179 leltárellenőrzés** sikeres, új packetdarabolási és csonkolási esetekkel.
- Valódi helyi TLS/Unity tanulási próba: **602 ellenőrzés**. Normál
  fejlesztés után a 0%-os kihagyott próba egy töltetet fogyasztott, a
  sikeres következő próba a harmadik rangot adta; újrabelépés után is
  750 nyers HP tanult forrás és nulla töltet állt vissza. A karakter XP-je
  megmaradt, az ismételt megerősítés nem fogyasztott újabb erőforrást.
- A petfelhasználás élő próbája: **803 ellenőrzés**. Saját pecsét,
  dobozban tárolt donor és megjelenéshez kötött donor elutasítása, teljes
  adatmegőrzés a védelmi próbák között, majd a külön megerősített
  felhasználás és replay történt. Végül újrabelépés után pontosan
  **297645 ms** visszatöltött életidő maradt; a donor nem tért vissza,
  a fogadó pet születése, fejlődése és képessége megmaradt.
- Az SQL-utóellenőrzések 552, illetve 668 korábbi tárgyat és a korábbi
  pénztárca/tanulási adatokat változatlannak találták. A tesztvilág helyreállt.

Bizonyítékok: `build/c26op-game-build2.log`,
`build/c26op-regression-tests.log` és a külön `growth_pet_progression` futás,
`build/c26op-sql-tests3.log`, `build/c26q-unity-offline.log`,
`build/c26o-unity-live.log`, `build/c26p-unity-live6.log`,
`build/c26op-manager-build.log`, `build/c26op-manager-deployment.json`.
A privát fixture-ök `build/c26o-bypass.private.json` és
`build/c26p-sacrifice4.private.json`; nem részei a commitnak.
A teljes hideg újraindítás, négy skinóra és 18 hatás záró harci mátrixa
is sikeres; a részletek a [C26q lezárási jelentésben](2026-11-07_pet_completion.md).
