# Mobok üldözésének megállása és klienspozíció-eltérése – 2026-10-24

## A játékpróbából igazolt fő hiba

Az első megállítási javítás után a játékos továbbra is jelezte a hibát.
A Unityből felvett tiszta, 90 másodperces futásban 28 819 mozgást indító WAIT
és 155 támadási parancs érkezett az 5122-es majomhajítókhoz. Egyes mobok
két, egymástól körülbelül 85 cm-re lévő célpontot kaptak felváltva, minden
szerverpulzusban. A felvétel parancsidőt, időtartamot, célpontot és
megjelenített pozíciót együtt rögzít.

Két egymásra épülő szerverhiba volt:

1. A fő ciklus az AI-t futtatta a mozgás léptetése előtt. Az AI új `Goto`
   parancsa az aktuális időre állította a mozgás kezdetét, majd a mozgási
   rendszer ugyanabban a milliszekundumban nulla eltelt idővel futott. Ha az
   AI minden pulzusban újracélzott, a szerveroldali mob nem haladt előre,
   miközben a Unity mozgott a kiküldött célpontok felé.
2. A `Goto` változatlan célpontnál helyesen hamisat ad vissza, de a `Follow`
   ezt sikertelen üldözésként továbbadta. A `GotoNearTarget` ezért a 80%-os
   megállási sugárról a 90%-os tartalékra váltott. A következő pulzusban újra
   a 80%-os pontra célzott. Ez mozdulatlan játékosnál is fenntartotta az
   időzítés újraindítását és a csomagáradatot.

Az új `UpdateCharacterFrame` előbb lépteti a korábbi mozgást, utána futtatja
a karakterfrissítést és az AI-t. Ezt a függvényt használja a tényleges szerver
fő ciklusa és a natív regresszió is. A `Follow` a már aktív, azonos célpontú
mozgást sikeres folytatásként kezeli: nem küld új csomagot és nem indít új
mozgási órát. A valóban változó célpont továbbra is újracélozható.

Az új mozgási regresszió az előző sorrenddel elbukott:
`AI retargeting starved movement: character still at previous frame's position`.
A harci regresszió a korábbi `Follow` kóddal szintén elbukott:
`active primary chase was reported as a failure and invoked the fallback`.
A javítással mindkettő sikeres. A mozgási teszt váltakozó megállási pontot,
folyamatosan változó célt és a 32 bites időszámláló átfordulását is vizsgálja.

Helyi mérési fájl: a Unity projekt
`Logs/MobChase/20261024-132633.csv` állománya. Az Editorban a
`Metin2/Diagnostics/Record mob chase (90 seconds)` menü újra elindíthatja a
célzott felvételt. Ez diagnosztika, nem a normál játékhoz szükséges funkció.
Scriptet nem szabad a hálózati játékpróba közben újrafordítani: a Unity
domain-újratöltése elveszítheti a futó kapcsolat és az entitásszótár állapotát.

## Korábban javított, kapcsolódó hiba

Az AI és a mozgás interpolációja külön frissül. Az `AISystem::StateBattle`
támadási ága nem törölte az előző `MovementDestination` komponenst. Ha a
játékos közelített az üldöző mobhoz, a mob már támadhatott, miközben a mozgási
rendszer továbbvitte a korábbi, a játékos körüli megállási ponthoz. A támadási
várakozási idő alatt is megmaradhatott ez az elavult mozgás.

A normál `MovementSystem::Stop` harci célpontot is töröl és az AI-t üresjáratba
teszi, ezért erre nem használható. Az új `StopForAttack` csak a mozgást
szakítja meg. A célpont, harci állapot és támadási időzítés megmarad. A kliens
WAIT csomagban kapja meg a tényleges megállási helyet.

Az előkészítés és a csomagküldés közben indított új mozgás, célpontváltás vagy
entitástörlés elsőbbséget élvez; ezeket nem követi elavult támadás.

## Eredet és hatókör

Vizsgált alap: `94fbba00ff0a54d76fc67b8fb8a958b8582532cd`,
`ECS_+_BGFX` ág. A távoli források a javítás előtt megegyeztek a helyi
AI-, mozgási és combat-forrásokkal.

A friss `f2235923` metszőolló-támadáskorlátozása csak játékosokra vonatkozik;
a `24d0e5c1` és `26b2223a` jutalom- és dungeonmódosításai sem ezt az
üldözési ágat módosították. Az érintett AI-rész története a korábbi ECS
átalakításhoz (`342432add`) vezet vissza. A külön mozgási léptetés a
`5b8e723e9` változtatás óta a karakterfrissítés után futott. Az AI
átalakítása és a két frissítés sorrendje együtt tette lehetővé a mozgás
kiéheztetését; az újracélzás hibás sikertelenségi eredménye ezt felerősítette.
Ez a vizsgált hibára vonatkozik,
nem igazolja minden megfigyelt kliensoldali tünet eredetét.

Az 5122-es majomhajító a távoli `mob_proto` szerint távolsági mob, 850-es
támadási távolsággal. Ennek az adatbázisbeli értékét nem változtatjuk meg.
A távolságtartó felállás önmagában elvárt működés; a hibás továbbmozgás külön
kezelendő. A korábbi Unity megérkezési javításokat ez a módosítás egészíti ki.

## Ellenőrzés

Az új AI-regresszió az eredeti kóddal négy esetben összesen 12 ellenőrzésen
elbukott. A javítással közelharci és távolsági mobnál, támadás előtt és
várakozási idő alatt is sikeres. Külön eset ellenőrzi, hogy a még túl távoli
célpont üldözése megmarad.

A `SpatialLifecycleTests` a tényleges mozgási rendszert futtatja: megállás
után a következő interpoláció nem mozdítja a mobot; új üldözés indítható;
visszahívásból indított új mozgás és entitástörlés biztonságosan kezelhető.

Windows és FreeBSD alatt is lefordult a `GameServer`; az `ai_state`,
`spatial_lifecycle` és `combat_state` tesztcsoport mindkét rendszeren sikeres
(3/3). A távoli új binárissal mind a 13 game-core konfigurációellenőrzése is
sikeres. A fordítás, telepítés és ellenőrzés naplói az adott munkaállomás
`build/mob-server-*` fájljaiban vannak.

## Telepítés

A helyi `D:/AurigaGlobal/Server2-Local/bin/GameServer.exe` frissült, előtte
a korábbi bináris mentése elkészült a csomag `run/deployments/mob-chase-*`
mappájában. A helyi szervert nem indítottuk el.

A FreeBSD tesztgépen a módosított forrás- és tesztfájlok a
`/Auriga/Source` alá került, a felülírt fájlokról mentés készült. A kész
binárisokat a szerverkezelő ellenőrzött telepítője másolta a
`/Auriga/Server/bin` mappába. Telepítési mentés:
`/Auriga/Server/run/deployments/60827afdedbd13047db50c49/backup`.

Az első aktiválási kísérlet a 30052-es port foglaltsági ellenőrzésénél
megállt, ezért abból még nem következett sikeres telepítés. A későbbi
telepítés sikeres naplóját és a tényleges állapotot külön ellenőriztük.
A távoli forrás a helyivel a sorvégek egységesítése után bájtra azonos.
A legutóbbi fordított és telepített GameServer SHA-256 értéke egyaránt
`a85a2680b248cd29ef2eb62d8a8b802a081e69525633a3e2dd52fd158e8dd230`.

A felügyelő szabályos leállítása után ugyanaz a kiválasztás indult újra:
`database`, `ch1-core1`, `ch1-core2`, `ch99-core99`, `login`. Az ellenőrzés
`listening` állapotot, mind az öt folyamat futását és az új GameServer
SHA-256-egyezését igazolta. A `server.json` és a TLS-fájlok ellenőrzőösszege
változatlan. A külső MariaDB-szolgáltatást nem állítottuk le.

Távoli ellenőrzési napló:
`/Auriga/Server/run/deployments/mob-chase-20261024/current-install-verification.json`.
Az ismételt FreeBSD-ellenőrzés helyi naplója:
`build/mob-current-freebsd-verification.json` (3/3 teszt, 13/13 konfiguráció).

## Sikeres játékpróba a friss szerverrel

A felhasználó visszajelzése: „Most már megközelíthetők és üthetők”.
Az új felvétel: `Logs/MobChase/20261024-182252.csv` a Unity projektben.
Ebben 25 majomhajító szerepel, mind a 25-höz érkezett támadási parancs.
A felvételen 544 WAIT és 336 támadási parancs volt, szemben a korábbi
28 819 WAIT és 155 támadási paranccsal. A két játékpróba mozgássora és az
entitások jelenléti ideje eltér, ezért ezek nyers eseményszámok, nem azonos
terheléssel mért teljesítményeredmények.

A korábbi 21 908 gyors A–B–A célpontváltás helyett 22 maradt. Ezekben a
célponteltérés csak 1–1,4 cm volt, és az üldözés hátralévő ideje csökkent;
a korábbi körülbelül 85 cm-es, folyamatosan újrainduló váltogatás nem tért
vissza ezen a játékpróbán. Az összehasonlítás géppel olvasható helyi eredménye:
`build/mob-capture-comparison.json`.

A konkrét majomhajítós hibát így a natív regressziók mellett a friss
tesztszerveren végzett Unity játékpróba és a felvétel is ellenőrizte.
