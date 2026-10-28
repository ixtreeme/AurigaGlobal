# ADR IC-020 — Natív küldetéstárgy és ellenőrzött offline tokenkonverzió

2026-10-28. Állapot: megvalósított domain, séma és szerkesztő;
a GameServer/quest VM átállítása még nincs kész. Az IC-012 sorrendje érvényes.

A 10-es kategória `item_quest` táblája megkülönbözteti a passzív tokent,
az első megfelelő kezelőnek és az összes megfelelő kezelőnek történő
használati továbbítást. A csoport üresen saját ItemKey, különben ellenőrzött
tartalomkód. Nem numerikus SIG, vnum vagy végrehajtandó fájlnév.

Tokenhez nincs használati környezet, csoport vagy megfigyelőengedély.
Használható tárgynál explicit környezetmaszk kell. A tiszta
`PlanQuestItemUse` ellenőrzi a szerveres befogadást, kulcsot, halmot,
nyugdíjazást, környezetet, megfigyelőt, harcot, szintet és a kezelő elérhetőségét.
Tulajdonló tervet ad vissza katalógusverzióval és hashsel; nem ér el registryt,
SQL-t, Lua VM-et, hálózatot. Nem fogyaszt tárgyat. A konkrét instance/revision,
session, kaszt/nem és questkezelő ellenőrzése a későbbi tulajdonosi bekötés dolga.

A küldetéstárgy megjelenési profilja földre dobott modellt tartalmazhat.
Felszerelési helyet, OnEquip kötést és passzív felszerelésbónuszt ettől nem kap.
Fejlesztési profilja None/0. Az új `019_quest_items.sql` migráció és a
`auriga.item_catalog.v13` kanonikus tartomány még nem kiadott fejlesztői séma;
a helyi meglévő v7 katalógust nem módosítja.

Az offline konverter kizárólag a bizonyítottan leképezhető `ITEM_NONE`
passzív tokeneket fordítja. A mai `ItemSystem_LegacyBridge.cpp::UseItemEx`
ITEM_NONE ága nem futtat questet vagy hatást. ITEM_QUEST esetén a használati
kezelők és csoportok portolása külön szükséges, ezért nem lesz automatikusan
passzív token. A nem leképezett mező, flag, limit, ár, apply, érték, extra proto,
fejlesztési család vagy sérült név blokkolja az adott sor konverzióját.

Régi halmozható tokennél `common/length.h::ITEM_MAX_COUNT` alapján 50 000,
nem halmozhatónál 1 a maximum. A két nem bitjeinek sorrendjét explicit
megfordítjuk az anti-flagek és az új engedélymaszk között. A kaszttiltást,
eldobási/eladási tiltást és szintlimitet veszteség nélkül képezzük le.

Az ikon és földmodell csak létező Unity-fájllal, egyező meta GUID-dal és
nem nulla szerializált hivatkozással vehető át. Minden fájl hash-e bekerül
az offline bizonyítékba. A szerver helyes lokalizált neve elsőbbséget kap a
sérült Unity-névvel szemben; a leírás nem dobható el. Sérült további szöveg
külön felülvizsgálati hiba, nem csendes helyettesítés.

A konverter új fájlt készít, SQL-végpontot nem fogad, játékadatot nem ír.
A csomag nem telepíthető release: az összes kategória és példányállapot
konverziója, tartalomellenőrzése és runtime-bekötése még szükséges.
A próbaimport kizárólag új, saját MariaDB-sandboxba történhet, majd natív
visszaolvasás és hash-/számláló-egyezés után leáll a sandbox.
