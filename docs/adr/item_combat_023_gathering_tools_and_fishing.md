# ADR IC-023 — Gyűjtőeszközök és halászati tárgyak

2026-10-28. IC-012 F2, helyi fejlesztés. Kezdő HEAD: `19971874`.

## Típusos prototípusok

A 11-es kategória eszközei: csákány, horgászbot, fejsze, metszőolló.
Nem halmozhatók. Külön `tool_profile` és `tool_level` írja le a szintenkénti
erőt, gyűjtési bónuszt, szükséges gyakorlópontot, pontos N/D pontszerzési esélyt,
fejlesztési sikeresélyt és kudarcot. A bot első/második zsákmánytáblája explicit
enum, más eszköznél nincs halászati tábla; nincs tárgyazonosító-tartományból
kiszámolt runtime választás.

A maximum egyetlen szerkesztett igazságforrása a közös refinement profil.
A gyakorlási profilnak pontosan +0..+max sorai vannak, legfeljebb 256.
Az utolsó sorban nincs további gyakorlás vagy fejlesztés. A `Tool` refinement
kind nyers harci alapértéket nem adhat, csak explicit szintbónuszt. A natív
modell nem olvassa a régi vnumot, értéktömböt vagy socketet.

A gyakorlópont a példány külön állapota. Egy sikeres gyűjtési pontlépés
legfeljebb egy pontot ad, a szükséges pontig. A pontos N/D arányt nem kerekítjük
bázispontra. A pont- és fejlesztési döntés a szerver által rögzített dobást kapja;
0%/100% és teljes/maximális állapot esetén nincs rejtett RNG-hívás.

Sikeres fejlesztés ugyanazon ItemKey következő szintjére visz, 0 gyakorlóponttal.
Kudarc vagy az aktuális szinten tart és `pont - floor(pont * lossBp / 10000)`
pontot hagy, vagy explicit megadott (akár azonos) szintre állít 0 ponttal.
Nem készül szintenként új prototípus. A tesztben +10→+11 és +30 maximum is van.
Közönséges smith-recept eszköz forrás-/célállapotra nem publikálható: nem
kerülheti meg a szükséges gyakorlópontot.

A 12-es kategóriában élő hal, döglött hal és csali van. Élő hal használata
egy darabot fogyaszt és ellenőrzött lootprofilt alkalmaz; a jutalom csak tárgy,
ritka attribútumdobás nélkül. Döglött halhoz opcionális, explicit sütési
eredmény-ItemKey tartozik (fogyasztható kategória). A teljes kijelölt halom
ugyanannyi eredményt ad. Kisebb cél-stacklimitnél az owner több halomra osztja
a grantot; a terv nem csonkolja és nem dobja el a többletet.

Csali alkalmazásakor egy darab fogy. Saját, felszerelt horgászbot kell;
folyamatban lévő halászat közben tiltott a csere. A csali ereje felülírja,
nem növeli a bot korábbi csalijának erejét. A gyakorlópont és a csali továbbra
is két külön állapot, nem közösen értelmezett általános socketmező.

## Mellékhatásmentes tervek és owner

A `PlanToolAction`, `PlanFishProcess`, `PlanApplyBait` kizárólag birtokolt
bemenetből dolgozik. Szerveres admission, operation ID, ruleset revision,
instance/revision/tulajdon, halom/szint, zárolás/csere, minimumszint és
harci use-policy ellenőrzött. Gyakorláshoz viselt, fejlesztéshez levett eszköz
kell. A terv megőrzi a bemenetet, katalógusrevisiont/hasht és az eredményt.

Ez nem felszerelés- vagy perzisztenciaíró. Az F3/F4 owner feladata még a session,
osztály/nem, hely, befejezett erőforrás-művelet, NPC és tábortűz ellenőrzése,
az exact revision újraellenőrzése, az összes kimeneti slot foglalása, majd az
operation receipt + fogyasztás + állapotcsere/grant atomi commitja.
CommitUnknown esetén az operation ID tartós egyeztetése előzi meg az újrapróbálást.
A Mission/Mission Pass események csak a commitolt változásból születhetnek.

## Szerkesztő és séma

Az eszközpanelen a gyakorolhatóság, erő, esély és kudarc szintenként látható.
A Fejlesztés fül maximumának állítása együtt mozgatja a gyakorlási táblát;
az ideiglenes csökkentés/visszaemelés megtartja a még nem mentett sorokat.
A végső sor nem szerkeszthető fejlődési mezőit a panel nullázza. A kudarc
nem alkalmazható mezői inaktívak. Közös profil szerkesztése copy-on-write.

A halpanelen csak a kiválasztott működés mezői aktívak. Élő halnál a
lootválasztó csak tárgyat kínál; a ritka attribútummező nem jelenik meg.
A natív validátor ettől függetlenül is elutasítja a hibás tartalmat.

Új, nem kiadott v16 séma, `auriga.item_catalog.v16`, hash-tagek 34..36,
000..022 friss SQL-lánc. A már telepített v7 nem frissül helyben ezzel a drafttal.
A meglévő felhasználói katalógus új sandboxba átemelve és visszaolvasva ellenőrzött.
Az aktív adatbázis és telepített app ebben a lépésben nem változik.

## Forrásaudit és korlátok

A mentésben 40 bot, 10 csákány, 10 fejsze, 10 olló, 25 élő hal, 25 döglött hal,
4 csali szerepel. További 14 resource-anyag és egy tábortűz külön feldolgozandó.
A 40 botból hat refined_vnum önmagára mutat; ebből nem készül automatikus recept.

A fejsze és olló a jelenlegi runtime kód szerint ID-tartományról felismert eszköz,
miközben a prototípusa fegyver. A harci/appearance/policy jelentését a konverter
még nem rendezte: nem dobjuk el a fegyveradatait pusztán a besorolás alapján.
Az auditált sorok egyike sem lett ettől a munkától teljesen konvertált tartalom.

`world/fishing.cpp` helyben definiálja az `ENABLE_FISHINGROD_RENEWAL` makrót,
az új ActivitySystem botfejlesztése másik fordítási egységben vizsgálja azt.
Ezért a tényleges build preprocesszált útvonalát a konverternek külön igazolnia
kell; a két fájl szövege alapján nem választunk hallgatólagos kudarc-balanszot.
Az új modell mindkét szükséges kudarcformát explicit módon képes leírni.

A tábortűz spawnja, a teljes fogási táblák, resource-feldolgozás, assetek,
bonus/policy megfeleltetések és a tartós játékospéldányok teljes konverziója,
valamint a Database/GameServer/Unity bekötés továbbra is külön munka.
