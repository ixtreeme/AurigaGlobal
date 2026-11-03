# ADR IC-050 — Képességkönyvek a futó szerveren és Unityben

2026-11-03, C22. Előzmény: IC-018 tiszta könyvszabályok, IC-032 mentés.
A külön típusos tárgyút változik; a normál legacy leltár átállítása továbbra
is a felhasználó későbbi döntése.

## Használat és célzás

A tanulás, felejtés és lóképességpont-szerzés szerveroldali előnézetet és
egyszer használható megerősítést kap. Fix cél a profilból, kötött cél a
példányból származik. A kliens nem küldhet helyettesítő skill-ID-t.
A szerver feloldja a tényleges skill-protót, maximumot, kasztot/csoportot,
előfeltételt és tiltásokat, továbbá a felejtés visszatérítési pontkészletét.
Harci/ló/passzív skillnél a mastery-létra 40-es maximuma érvényes; a protó
bMaxLevel mezője itt nem önálló tanulási korlát. Másodlagos skillnél a
prototípus deklarált maximuma is korlátoz.
Az IC-018 admission ellenőrzi a szintet, nemet, mastery-t, saját birodalom
nyelvét, olvasási lépést, rendelkezésre álló darabot, XP-t és várakozást.

Elfogadott sikertelen próba is fogyasztja a deklarált könyvet és XP-t.
Sikeres olvasás a számlálót növeli; a küszöbnél pontosan egyszer emel és
nulláz. Felejtés egy szintet vesz vissza és egy megadott pontot ad vissza.
A lópont külön készlet, nem ál-képességazonosító; a 20/60 korlátok érvényesek.
A szerver generálja és a receipt rögzíti a dobást és a várakozási döntést.
A kész szint közvetlenül kerül a skillkomponensbe, második mastery-dobás nélkül.

## Tartós, közös tranzakció

`007_book_learning.sql`: nullable `book_skill_id` példányoszlop,
`item_book_learning` sidecar, `typed_book_revision` a player/player_deleted
táblákban. A régi tárgyak és műveletek hash-e változatlan. A kötött skill a
BKS1 hash-kiterjesztés része; halmozás és várakozó grant sem moshat össze
eltérő skillre kötött példányokat. Mozgatás/átadás a teljes példányt megőrzi.

A BOK1 sidecar tulajdonosi revíziót, utolsó karakter-checkpointot, skillenként
olvasásszámlálót és UTC-határidőt tárol. A célhoz tartozó határidő másik
könyvvel sem kerülhető meg, és offline is telik. A legacy másodperces mező
ugyanerre a határidőre kerekített tükör; a typed út a pontos ezredmásodpercet
használja. Az előrehaladás más úton megváltoztatott skill szintjére nem vihető át.

Egy ItemCommit írja a könyv pontos debitjét, sidecart, karakter XP-jét,
képességeit és három pontkészletét. Teljes before-state CAS, owner-fence és
receipt védi. A karakter-participant a korábbi book-revízióra hasonlít.
Sérült sidecar/hash vagy össze nem illő revízió nem állítható vissza csendben.

A karakter skilltáblája hordozható, 255 × (master:u8, level:u8,
next-read-seconds:u64 LE) kódolást kap a meglévő SQL-blobhoz. A domain
saját, korlátozott heap-táblát másol; nem helyez több ezer bájtos táblákat
minden meglévő ItemCommit stackpéldányába.

## Legacy mentés és belépés

A belső Game–Database TPlayerTable book-revíziót is visz; GameServer,
Database és LoginServer együtt telepítendő az additív migráció után.
A legacy teljes karakter-UPDATE öt könyvmezőt feltételesen ír. Régebbi
book-revízió nem írhatja felül az XP-t, skillblobot és három pontkészletet;
a többi karakteradat menthető marad. A revízió csak növekedhet.
A generált SQL védelme bináris skill-literalban levő elválasztókat is kezel,
és hiányzó védett mezőnél elutasítja a lekérdezést.

A cache régebbi karaktercsomagból sem vesz át újabb book-állapot helyett
régit. Belépéskor a Game régi cache-ből kapott checkpointját szükség esetén
a hiteles learning-sidecar javítja. A legacy másodlagos pontrekonstrukció
book-revíziós karakteren nem fut, mert különben visszavonná a felejtés
pontos visszatérítését. Könyv nélküli karakteren változatlan.

Függő könyvművelet alatt az érintett normál XP-/pont-/skillváltoztatások
a lezárásig várnak. Az új login megvárja a korábbi könyvírás rendezését.
Az eredmény csak az aktuális loginra alkalmazható. Typed owner nem léphet
vissza a régi könyvolvasási/felejtőkönyv-műveletbe; normál skillpont-kiosztása
továbbra is a meglévő skillrendszeren működik.

## Protokoll és Unity

- CG244: 26 byte + sequence, előnézet/megerősítés/bezárás, külön control és
  nonce. Megváltozott forrás, karakter-checkpoint vagy learning-revízió
  elavulttá teszi az előnézetet. Ugyanaz a megerősítés nem fogyaszthat újra.
- GC244 v1: profil/cél, aktuális és következő szint, olvasásszám, esélyek,
  darab/XP-költség, várakozás, pontok, rögzített eredmény és GC227 metadata.
- GC225 v4 csak kötött könyv jelenlétében: 16-os flag, skill:u16. A korábbi
  v1–v3 olvasható marad. GC227 v2 Book actionbit: 2048.

Unity: **külön típusos leltár → könyv → Könyvolvasás → Olvasás áttekintése
→ Olvasás megerősítése**. A felület mutatja az előrehaladást, költséget,
várakozást és eredményt; a tényleges skillablak a szokásos GC76 szintcsomagot
is megkapja. A kliens nem von le könyvet vagy XP-t saját hatáskörben.
Az utolsó egység elfogyása után az eredmény látható marad.

## Határok

A typed könyvbónusz- és várakozáselhagyó töltet atomikus fogyasztása a modell
és a natív runtime része. A töltetet adó új segédtárgyak és a legacy affectek
konverziója külön munka; a jelen élő próbakészlet normál, töltet nélküli könyveket
használ. Nincs automatikus legacy vnum/socket-feloldás vagy teljes adatkonverzió.

Ellenőrzések és fennmaradó kiadási kapuk:
[C22 jelentés](../reports/item_combat/2026-11-03_skill_books_runtime.md).
