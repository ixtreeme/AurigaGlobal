# C44 — az eredeti leltár teljes, helyi átállítása

Az eredeti **szabika (PID 3)** karakter **76 példánya, összesen 593 darabja**
átkerült az új tárgyrendszerbe. A saját fiókjával, a helyi TLS-szerveren,
Unity 6.3-ból végzett tényleges kilépés és újrabelépés után is ugyanazok a
példányazonosítók, típusok, szintek, darabszámok és elhelyezések töltődnek be.
Az új, önálló Nexoria-leltár már adatokat kap a „betöltés folyamatban” helyett.

Csak helyi módosítások: szerver `20261103`, Unity UVCS
`/main/unity63-integration-20261103`, `6000.3.27f1` Editor.
Nem történt push, Git-commit vagy Unity-checkin; a Unity changeset továbbra is
`cs:100`, a szerver HEAD `a577d68e`.

## Konvertált tartalom és működés

A C43 1426 kiadott natív definíciója mellé **70 új definíció** került,
beleértve az eredeti leltár további típusait és a ládajutalmak teljes
tárgyhivatkozási lezárását. A helyi katalógus összesen **1496 definíciót** tartalmaz.

- Nap- és Hold-elixír: a részben elhasznált töltet és a bekapcsolt állapot
  átkerül. HP-kapacitás 7 000 000, SP-kapacitás 700 000; az eredeti
  313 293 / 160 elhasznált egység nem töltődik vissza az import miatt.
  Az eredeti `AffectSystem` szerinti felső korlát másodpercenként 7% maximális
  HP/SP. A régi mentés nem tartalmazott másodpercen belüli tick-fázist;
  az új óra a következő egy másodperces tickkel indul.
- Öt időzített ital: az eredeti APPLY tulajdonság, érték és időtartam alapján.
  Azonos célpontra ható italok közös csoportot kapnak. A 39019 megmaradt
  régi megnevezése ellenére a forrás APPLY70 szabályát követi: maximális SP%.
- Megbűvölő tárgy: normál bónuszcsere, az eredeti 0/10/40/35/15%-os
  tiereloszlással. A ritka slotok és a tárgy többi állapota megmarad.
- Koncentrált olvasás és Ördögűző tekercs: a natív könyvsegéd-töltetek útján.
- Utalvány és az 50k/100k Yang-rudak: karakter-, illetve fiókpénztárcával.
  A Sárkányérme-utalvány az önálló fiókpénztárcához irányít.
- Mindkét Áldástekercs és **91**, az eredeti felszereléscsaládokhoz tartozó
  fejlesztési recept. A receptköltségek és alapanyagok hivatkozásai is natívak.
- Kagyló: 50% kődarab, 45% üres eredmény, 2% fehér, 2% kék, 1% vörös gyöngy.
  Az üres eredmény külön, nulla jutalmú sorsolási típus; ilyenkor is elfogy a
  kagyló. A szerver és Unity GC233 codecje, ablaka és nyugtája ezt is kezeli.
- KK-láda és mind a 48 lehetséges képességkönyve: az eredeti jutalomsúlyokkal,
  konkrét natív könyv- és képességhivatkozásokkal.
- A két Bátorság köpeny és a Teleport gyűrű: natív, név szerinti beépített
  quest-handler. A köpeny csak sikeres tárgyfogyasztási commit után hat.
  A gyűrű a szokásos quest-választóablakot használja, a forrás tényleges
  koordinátáival és szintfeltételeivel; a megszakítás nem fogyasztja el.
- Napkristály és a meglévő passzív alapanyagok: a hordott stackek változatlanul
  megmaradnak. A forrásban használati handler nélküli token nem kap kitalált
  aktiválási műveletet.

A fegyverek C43-as fix sebzése változatlan: a régi minimum és maximum pontos
átlaga, hozzáadva a fejlesztési értéket, fél pontok kerekítése nélkül.

## Atomi tulajdonosimport és adatmegőrzés

Az importáló most vegyes típusú teljes tulajdonost is kezel. Minden eredeti
sor mind a 26 mezője ellenőrzött és archivált. Megmarad mind a nyolc
bónuszslot, zárolás, foglalat, beágyazott kő és ékszerállapot is.
A karaktertárolókat név szerint különbözteti meg a fiókraktáraktól;
azonos numerikus owner ID nem jelent közös tulajdonost.

Egyetlen SQL-tranzakció hozza létre a natív tárgyakat, tárolókat, pénztárcákat,
nyugtát és archívumot, törli az átvitt régi tárgysorokat, és vezeti át a két
automata affectet. A gyermekrekordok kötött JSON_TABLE bemenettel férnek el a
meglévő 512 lépéses / 4096 paraméteres korláton belül. Nem emeltünk globális
tranzakciós limiteket. A csomagok csak a ténylegesen használt tárolókat adják át.

Az eredeti karakter átállításakor az **649 173 Yang** és a fiók
**847 950 Sárkányérméje** megmaradt. A régi `account.coins` értékének átadása
és nullázása ugyanabban az importban történt, archivált állapottal.
A régi automata affectek eltűntek, a többi affect azonosítója és bónusza
megmaradt. A meglévő játékkód szokásos online időfogyását és a belépéskor
frissülő drop-policy jelölőt a visszaolvasás külön kezeli.

Az eredeti karakter átállítása előtti friss, teljes SQL-mentés és sikeres
visszaállítási próba:
`D:/AurigaGlobal/Server2-Local/run/item-migration/20261110-original-before-c44`.
A nyugtával azonos kérés újrabelépés után is idempotens; eltérő tartalmú
újrajátszás elutasított. A korábbi natív tulajdonosok adatai és a tíz
ellenőrzött aktív Unity-beállítás megmaradtak. A korábbi Unity-munkaterületeket
nem módosítottuk.

## Lefuttatott ellenőrzések

- Natív domain- és wire-tesztek: **11 CTest, 0 hiba**; ezen belül az üres
  súlyozott ládaeredmény is külön ellenőrzött.
- Új konverziós állapottesztek: **4 sikeres Python-teszt**.
- Friss MariaDB-séma: **41 sikeres teszt**.
- Teljes friss mentés visszaállítása és import külön MariaDB-példányon:
  76 tárgy / 593 darab, szándékos tranzakcióhiba teljes rollbackkel,
  két affect és fiókvaluta, pontos visszaolvasás, azonos és eltérő replay.
- A publikus offline konverter CLI-je is ugyanazt a végső katalógushasht
  állította elő, mint a telepített adatbázis.
- Aktív Unity-források fordítása a projekt rögzített Roslyn/Unity 6.3
  eszközeivel; a runtime és Editor assembly is sikeres.
- **37 Unity offline validátor, 0 hiba** az ellenőrzött forrásmásolaton.
- A teljes leltár másolata, valódi helyi TLS-kapcsolattal: **3058 ellenőrzés,
  két belépés**. Elixírkapcsolás, öt ital, könyvsegédek, mindkét láda,
  utalvány és mindkét Yang-rúd, köpenyek és gyűrűmenü.
- Eredeti fiók, eredeti karakter: **982 ellenőrzés, két belépés**,
  teljes leltár és állapotmegőrzés. Tárgyfogyasztó használatok ezen a
  karakteren nem futottak.
- Külön műveleti próba a másolaton: **1243 ellenőrzés, két belépés**,
  bónuszcsere és mindkét áldástekercs tényleges használata, mentett eredménnyel.
  A beállított öt italhatás újrabelépés után is megvolt; ezeket a külön
  próba a szokásos natív törlésművelettel kikapcsolta.
- Mindhárom sikeres élő futáshoz további 700 companion wire-ellenőrzés tartozik.
- Végső SQL-visszaolvasás: minden eredeti példány fingerprintje pontos;
  az aktív elixírek töltete fogyhatott a tényleges gyógyulástól, de nem nőtt meg.

## Továbbra is nyitott határ

Ez az eredeti karakter teljes leltárának átállítása, **nem az összes régi
tartalom konverziójának lezárása**. A teljes prototípus-, drop-, bolt-,
NPC-crafting- és régi quest-hivatkozásrendszer átállítása még hátravan.
A C43 korábbi blokkolt felszerelési családjai is külön munkák.
A 28442-es forráskő következő fejlesztési célja nem lezárt; ehhez nem került
kitalált recept a katalógusba.

A gyűrű menüje és megszakítása ellenőrzött; minden célterületre való tényleges
átlépést és mind a 48 könyv külön olvasását nem jártuk végig.
Teljes világjelenetes grafikai próba, FreeBSD-fordítás és új
Windows/Android/iOS kliensbuild ebben a körben nem készült. A korábbi exportok
nem tartalmazzák automatikusan ezeket a helyi Unity-módosításokat.
A VPS változatlan.
