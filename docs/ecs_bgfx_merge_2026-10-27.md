# ECS + BGFX változások beolvasztása a Mission Engine ágba

Dátum: 2026-10-27.

Forrás: `origin/ECS_+_BGFX`, `0e0535ff`.
Cél: `codex/mission-engine`, korábbi állapota `c273291f`.
Összeolvasztási commit: `a0c04c83`.

## Áthozott munka

A friss fetch után három, a célágból hiányzó commit volt a forráságon:

- `05f25488`: kémküldetés, natív történetvezérlés, saját szereplők,
  navigáció, tulajdonoshoz kötött láthatóság/harc és cinematic lock.
- `0f922e60`: a forráság korábbi összeolvasztása.
- `0e0535ff`: karavánküldetés, útvonal/navigáció, jármű és találkozások,
  privát térképpéldányok, utas-/ülésállapot és saját szabálytesztek.

A teljes hiányzó ágtörténet bekerült. A közös előzmény `9cda87fd`;
az ezen már szereplő hibajavításokat nem másoltuk újra külön cherry-pickkel.
A két szülővel rendelkező merge megőrzi a Mission Engine korábbi commitjait
és a társ új küldetéseinek eredeti történetét is.

A story források mellett bekerültek a navigációs adatok, a karaván erdei
térképének konfigurációi, valamint a heartbeat-, mozgás-, láthatóság-, harc-,
NPC-kattintás- és parancskezelési bekötések.

Ezek a kolléga önálló natív C++ küldetései. A Mission Engine ágon együtt
fordulnak a fejlesztésünkkel; ettől még nem `.aurmission` gráffá alakított
Mission Engine-tartalmak.

## Ütközés és szükséges kompatibilitási javítások

A `MovementSystem.cpp` két ütköző szakaszában a másik ág még a régi
BattlePass online-időzítőjét módosította. A Mission Engine ágon ez a rendszer
már kivezetett, ezért a törlés maradt meg. Az új story lockok bekerültek.
A hozzá kapcsolódó, már inaktív BattlePass-tesztszakaszok sem kerültek vissza.
A szezonális Mission Pass és az offline shop korábbi kivezetése megmarad.

A tényleges Windows GameServer-build felfedte, hogy a karavánban használt
`near` és `far` lokális változók ütköznek a Windows fejlécmakróival. Ezek
`origin`, `aheadIndex`, illetve `nearby` nevet kaptak, az algoritmus és a
csomagformátum megváltoztatása nélkül.
A karaván szabálytesztje Windows alatt már a valódi SDK-fejléccel együtt
fordul, hogy ezt a makróütközést a külön tesztbuild is felismerje.

Az izolált harci, ló- és packet-dispatch tesztek a teljes story szolgáltatás
helyett explicit tesztfüggőséget kapnak. A packet fixture saját EnTT-állapotból
oldja fel a térképindexet és a player ID-t. A CombatStateTests a többi SQL-seam
teszthez hasonlóan a valódi `QueueDrain.cpp` cancellation helperét linkeli;
élő SQL worker nem került ezekbe az egységtesztekbe.

## Ellenőrzés és telepítési határ

A Windows x64 / RelWithDebInfo GameServer, Database és LoginServer buildje
sikeres. A végső célzott CTest-kör **53/53 normál** és **41/41 ASAN** sikeres,
köztük a kém- és karavánküldetés szabálytesztje, valamint a harci, mozgási,
láthatósági/packet- és ló-regressziók. A normál kör öt izolált helyi
MariaDB-integrációt is tartalmaz; új ASAN MariaDB-kör nem futott. Az ASAN
kör tesztbinárisokra vonatkozik, nem új teljes ASAN GameServer-buildre.
Ez célzott regresszió, nem az összes létező teszt vagy játékbeli küldetés
elfogadása.

A navigációs fájlok verziója, dimenziója, hossza és járhatósági jelzői
érvényesek. A karaván 998 útvonalpontján a szerver betöltőjével azonos
kétcellás környezet és legfeljebb 100 egységnyi magasságkülönbség ellenőrzött.
A hét erdei térkép-JSON beolvasható.

A forráság alapértelmezései megmaradtak: a karaván konfigurációja
`enabled: false`, a kémküldetéshez kikapcsolt példa-konfiguráció tartozik.
Az összeolvasztás nem állítja át a tényleges szervercsomag konfigurációját.
A játékbeli, Unityvel végzett küldetés-elfogadás külön lépés.

A tényleges játékadatbázis, a külön Unity workspace és a távoli gép változatlan.
A Git-munka helyi; push vagy telepítés nem része ennek a munkafázisnak.
A merge előtti állapot a `codex/mission-engine-before-ecs-merge-20261027`
helyi biztonsági ágon is megmarad.

Helyi bizonyítékok a `build/ecs-mission-merge-20261027/` mappában:

- `native-final-build.log`, `asan-final-build.log`.
- `native-test-names.txt`, `asan-test-names.txt`.
- `quest-asset-validation.json`.
- `native-regression.log`, `asan-regression.log`.
- `final-git-verification.json`.

A lezárás előtti új fetch szerint a forráság változatlanul `0e0535ff`;
a `HEAD..origin/ECS_+_BGFX` hiányzó commitjainak száma **0**.
A Mission Engine és a szezonális Mission Pass saját forráskönyvtárai, valamint
a behozott küldetésadatok a várt eredeti változattal egyeznek.
