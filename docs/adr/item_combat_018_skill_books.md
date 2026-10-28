# ADR IC-018 — Képességkönyvek

2026-10-28. IC-014, 4. szelet; a 9-es kategória izolált domain/katalógus/szerkesztő portja.

## Auditált működés

`ItemSystem_LegacyBridge.cpp::UseItemEx`: az ITEM_SKILLBOOK 50300 socketből
veszi a képességet; más könyvek value0-ból, a vezetéskönyvek külön vnum-ágon
vezetésre céloznak és szintablakot ellenőriznek. A felejtőkönyv szintet vesz
vissza és képességtípus szerint pontot ad vissza. Polimorf állapot tiltott. Az 55003–55005 könyveket a régi ág előre tiltja;
a konverter ezeket nem nyilváníthatja automatikusan támogatottnak. A 50322-es
gyakorló átváltozáskönyv mobformát és külön gyakorlási állapotot kezel, ezért
a Polymorph kategóriával együtt portolandó, nem egyszerű skillolvasás.

A USE_SPECIAL külön kezeli a kombót (30/50 karakterminimum), nyelveket
(saját birodalom nyelve tiltott), átváltozást/életerőt/ellenállást,
kézművességet (50902–50904, a név ellenére nem lókönyvek), bányászatot és a
valódi lóképességpont-könyvet. Utóbbi 50-es szintet kér, 20 szabad pontnál
vagy bármelyik alternatív kiosztás 60 pontjánál tilt; sikertelen próbára is
fogy, de alapból csak siker után állít következő olvasási időt.

`SkillSystem::LearnSkillByBook`: létezik közvetlen esély és sikeres
olvasásokat számláló út. A support/passzív ág a korábbi számlálót ellenőrzi
növelés előtt: a tényleges sikerigény a táblázatban szereplő érték+1.
A master ágban a need_bookcount helyi változó 0 marad; a logolt táblázat
nem vezérli az előrehaladást. A normál alacsony szintű, nulla esélyparaméteres
ág a logolásnál negatív indexet is képezhet. Ezeket nem másoljuk át.

A könyvbónusz ágonként eltérő esélyt ad, a várakozás elhagyását külön affect
teszi lehetővé. A jelen service.h DISABLE_SKILL_BOOK_NEED_EXP-et definiál;
a régi feltételes 20000 XP-költség így a jelen buildben nem aktív. A következő
olvasás többnyire 64800–108000 másodperc, vezetésnél 18000, új supportnál
10800. A könyv által visszaadott true elfogadott próbálkozást, nem feltétlen
sikeres szintemelést jelent. Az alap mastery küszöbök 20/30/40; anti-passzív
11 helyett 20-ra ugrik. A tényleges itemadat/balance teljes konverziója F6.

## Döntés

Új típusos `SkillBookProfile`, cél- és szintlépések; `item_skill_book` a
9-es kategória gyereke. Műveletek: tanulás, felejtés, lóképességpont.
Célzás: fix képesség, példányhoz kötött képesség megadott engedélylistából,
vagy a ló-pontkészlet (nem ál-képesség ID). Nincs item vnum/socket/value
feloldás. Az egyszeri offline konverter alakítja majd át az ilyen adatokat.

A skill ID önálló képességtartalmi azonosító, 1..254; nem ItemKey.
A katalógus szerkezeti tartományt és engedélylistát ellenőriz. Az ownernek
kötelező feloldania a képesség tényleges létezését, maximumát, kaszt/csoport,
előfeltétel, globális/map és skill-tiltás szabályait. A két admission döntés
alapból tiltó; a külön skill-katalógus keresztellenőrzése a F4 kapu része.
Az izolált itemkatalógus ezt nem állítja ellenőrzöttnek.

Minden támogatott aktuális szint saját sort kap: következő szint,
karakterszint-minimum, szükséges sikeres olvasások, normál/erősített esély bp,
XP-költség, várakozási idő tartománya és sikertelenség utáni várakozás.
Ez természetesen leírja a 10→20 ugrást is. Nincs 0 esélyhez rendelt rejtett
algoritmus: 0 mindig sikertelen, 10000 mindig sikeres. Az esély és az idő
szerveres, rögzíthető döntés; a tiszta modul nem használ RNG-t vagy órát.

Tanulásnál minden siker hozzáad egyet a számlálóhoz; a szükséges olvasásszám
elérésekor egyszer emel és nulláz. A sikertelen próba megőrzi a számlálót.
Felejtés normál mastery, pozitív szint, pontosan -1 szint és +1 meghatározott
Main/Secondary/Horse pont. A felejtés megszünteti a megkezdett olvasási
előrehaladást, és a tanulási cooldown nem tiltja. A lópont külön állapot, 20/60 korláttal.
A képesség 40-es korlátja a jelen skill domainé; nem új tárgyfejlesztési
korlát. A tárgyak fejlesztési profilja és példányszintje változatlan.

Előbb teljes admission és megváltoztathatatlan, saját adatokat birtokló
PreparedSkillBookUse; utána a szerveres döntésekkel Resolve. Az eredményterv
megőrzi az ItemKey-t, a profil ID-t és a katalógus-revíziót is; a konkrét
példány/karakter és revision ellenőrzését a commit parancs köti hozzá. Érvénytelen
admission/döntés semmit nem fogyaszt. Elfogadott próbálkozás sikertelenségkor
is a profil szerinti könyv/XP/bónusz költséget tervezi. A várakozáselhagyó
token csak fennálló cooldown esetén fogy. Cooldown a képességhez/lópontkészlethez
tartozik, nem itemhez: másik könyvvel nem kerülhető meg.

Az owner egyetlen revision/receipt commitban írja az itemfogyást, képességet,
olvasásszámlálót, pontot, XP-t, cooldown-t és tokeneket. Nincs részleges
SkillLevelUp callback, amely visszautasítás után mégis elfogyasztja a könyvet.
A kész szintváltást közvetlenül kell alkalmazni; nem szabad újabb rejtett
mastery-dobást futtatni. Az ownership/persistence/runtime/Unity még F3–F5.

V11 friss draft séma; nem meglévő v7/v10 katalógus in-place frissítője.
Kizárólag saját, eldobható helyi MariaDB tesztpéldányon ellenőrizzük.
Valódi felhasználói katalógust és távoli rendszert ebben a szeletben nem írunk.
