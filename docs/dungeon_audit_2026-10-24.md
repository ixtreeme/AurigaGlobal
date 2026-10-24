# Dungeonvizsgálat és javítások – 2026-10-24

A vizsgálat a jelenlegi C++ forrásra és a használt szervercsomag aktív dungeon
questjeire terjedt ki. A távoli források, regenek, pályabeállítások és szükséges
SQL-prototípusok lekérdezése olvasással történt. A távoli fordítási és futtatási
próbák külön, automatikusan törölt ideiglenes mappában dolgoztak. Futó szerver,
éles questfájl, bináris és adatbázissor nem változott.

## Vizsgált rendszer

| Megvalósítás | Aktív dungeonok |
| --- | --- |
| Lua, 15 forrás | Catacombe, Christmas 1, Christmas 2, Devil Tower, Dragon Lair, Spider, Enchanted Forest, Meley, Monkey Tower, Nemere, Owl, Razador, Slime, Temple Ochao, Underwater |
| C++, 9 vezérlő | Orc, Triton Temple, Valentine, Easter, Pyramid, Nightmare, Rune, Halloween 2022, Viking |

Az aktív questforrásokat a tényleges `quest_list` alapján vizsgáltuk. A helyi
csomag dungeonforrásai és a távoli példány ugyanazokat az eredeti fájlokat
tartalmazták. A hivatkozott regenfájlok megvannak; a literális belépési és
visszatérési koordinátákat összevetettük a pályák helyével és kiterjedésével.
A statikus vizsgálat önmagában nem ellenőrzi a kliens geometriáját és az
egyes koordináták járhatóságát.

Lost Castle be van kapcsolva a fordításba, de a bejárati/login/kill integrációja
inaktív, és a 380-as map hiányzik a használt mapindexből. Az alapvető instancia-
ellenőrzéseket megkapta, de nem aktiváltuk. A Hydra megmaradt segédkódja és
regenei mellett nincs aktív teljes dungeonvezérlő ebben a questlistában;
a Zodiac belépési questje lezárt tartalmat jelez. Ezek bekapcsolása külön feladat.

## Majom dungeon: több egymástól független elakadás

- A quest ütemezte a `monkey_tower_razor93_checker` időzítőt, de nem volt hozzá
  eseménykezelő. Most ellenőrzi az aktuális szint célpontjainak elfogyását,
  így egy kimaradó kill-visszahívás után is feloldódik a kapu vagy a boss.
- A számlálás a megfelelő prototípusokra szűr: az első emeleten 5121/5122,
  a végső őröknél 5125/5126. A kapu és a boss tényleges VID-jét tároljuk.
  A 30111-es kapu az SQL-ben BUILDING típusú, ezért önmagában nem volt
  beszámolódó szörny; nem ezt tekintjük az eredeti hiba bizonyított okának.
- A külön megadott 5125 kill-kezelők közül az első eltakarta a későbbi
  bossfeloldást. Egy kezelőbe került a drop és az állapotellenőrzés.
- Az utolsó őrhullám egyszer töltődik be, nem termeli újra a már leölt őröket.
- A szobor öt lándzsáját a dungeon közös flagje számolja. Több partytag is
  hozzájárulhat, és az ötödik lándzsa lezárja a szakaszt, anélkül hogy egy
  sebezhetetlen szobor HP-ját előbb nullára csökkentené.
- Az Owl pályáról átmásolt hibás teleportokat a majom pályán lévő pontokra
  cseréltük. A visszalépés csak a tényleges 1/2/5/13 szakaszokat kezeli, az
  időablaka 300 másodperc. Hiányzó kapu/szobor/boss nem hagy bent ragadt partit.
- A befejezés a 2832-es boss és a 367-es alapmap adatait használja; a korábbi
  5162/18 páros másik dungeonből maradt benne. A befejezés egyszer hajtódik végre.

A regressziós menet az első 48+25 majmot, a kaput, a metineket, két játékos öt
lándzsáját, az utolsó 31+16 őrt, a boss feloldását és halálát is végigfuttatja.
Külön teszt van a kimaradó kill-eseményre, hiányzó spawnra és négy visszalépési
pontra. A teleportok világkoordinátái: 1: `(611800,44700)`, 2: `(629100,43900)`,
5: `(653900,43700)`, 13: `(649100,66700)`.

## Közös dungeon- és harci javítások

Az ECS-ben a kilépési pont eltárolása és a befejezett warp célpontjának
nullázása leválasztotta a játékost a dungeonről. Ez a Nemere feltételes
kill-kezelőjét és más, `pc.in_dungeon()` feltételt használó questet is érintett.
A tagság most a tényleges mapváltásnál szűnik meg. A részletes Nemere/piramis
előzmény a `dungeon_audit_2026-10-23.md` dokumentumban található.

A `CountMonster` a már halott vagy más dungeonhöz tartozó ECS-entitásokat
kiszűri. Opcionális prototípusszűrése a Lua `d.count_monster(map_index, vnum)`
függvényből is elérhető, az eddigi egyargumentumos hívás megmaradt.

A takarító kill megszünteti az ideiglenes sebezhetetlenséget, és a
`INSTANT_FLAG_NO_REWARD` jelzéssel nem ismétli meg a korábbi játékossebzés
alapján a lootot és quest-killeket. A regen leállítása megelőzi a takarítást.

A nullás EXP-jű mobnak is lehet érvényes megölője: a `DistributeExp` előbb
feloldja a legnagyobb érvényes sebzőt, és EXP-jutalom nélkül is visszaadja.
Másik mapon lévő, eltűnt vagy túl távoli sebző nem kaphat kill-jóváírást.
Ez egy általános rejtett hiba: a most aktív Lua kill-célpontok SQL-prototípusai
pozitív EXP-vel rendelkeznek, ezért a majomjelentést nem erre vezetjük vissza.

## C++ vezérlők

| Érintett rész | Javítás |
| --- | --- |
| Mind a 9 aktív vezérlő | Érvényes PC és ugyanazon privát mapon lévő áldozat szükséges a killhez. Az érintett tárgyátadások és jutalom-NPC kattintások is ellenőrzik a mapot. |
| Orc, Triton, Valentine, Easter, Pyramid | Az indítás az első tényleges érkezéskor történik, nem az NPC-kattintás utáni, üres mapra lefutó előkészítéskor. Nem szükséges a partyvezető első érkezése. |
| Valentine, Easter | A holttestek nem számítanak élő célpontnak, és a megölt mobot nem vonjuk le kétszer. A 2/3. szakasz visszalépése a második szobába visz. |
| Nightmare, Rune, Halloween, Viking | Login hozzárendeli a karaktert az ECS dungeonhöz; a sikertelen warp megőrzi a visszalépési adatokat. |
| Rune | A partytag első érkezése is elindítja a runt. A 2–4. emeleten csak az aktuális feladathoz és emelethez tartozó boss léptethet. |
| Pyramid | Helyes visszalépési koordináta és megőrzött adatok elutasított teleportnál. |
| Halloween | A végső boss után a jutalomláda is létrejön; a láda ténylegesen átadja a 30934-es jutalmat. |
| Halloween, Viking | A jutalom átvett jelzését csak sikeres tárgyátadás után írjuk be; sikertelen átadás után újrapróbálható, siker után nem duplázható. |

A Halloween/Viking jutalomtárgyak létezését olvasott SQL-adatokkal is ellenőriztük.

## Lua questek

Mind a 15 quest első érkezése közös instanciaflaggel, egyszer indítja az
előkészítést. A befejezett menet killjei nem indítják újra a befejezést.
A takarítás leállítja a regent.

| Quest | További javítás |
| --- | --- |
| Meley | A jutalomhoz tényleges befejezés és 20-as végállapot kell; a Bezárás gomb nem adhat jutalmat. Egy játékos nem veheti fel többször. |
| Dragon Lair, Spider | Hiányzó bossnál a valódi `idx` kerül a takarításnak a nem definiált `arg` helyett. |
| Razador | Az előkészítési időzítő a saját instanciaazonosítójával küldi a kezdőüzenetet. |
| Christmas 2 | Az első metinszakasz a saját időzítőjét állítja le a másik timer helyett. |
| Underwater | A továbbléptető NPC csak a megfelelő, 7-es szakaszban indít teleportot. |
| Owl, Devil Tower, Underwater | A kijelzett 50 perc megfelel a meglevő 2999 másodperces limitnek; a tényleges időkeret nem változott. |

## Mi származik az ECS átállásból?

- **Bizonyított új ECS-hiba:** a `8c23c9a4` dungeonmigráció tette a
  `ClearDungeonIfOtherMap` hívást a `SetWarpLocationRaw` tárolófüggvénybe.
  A megelőző verzióban ez a mellékhatás nem volt meg.
- **Bizonyított korábbi C++ hiba:** a nulla EXP miatti korai visszatérés már
  a `40f0ec06` előtti `char_battle.cpp`-ben is benne volt; a harci ECS
  áthelyezés átvette. A Valentine bejáratból ütemezett korai előkészítése
  a dungeon ECS átállása előtti változatban is megtalálható.
- **Meglevő Lua-hibák:** a majom rossz koordinátái, hiányzó timerkezelője,
  játékosonkénti lándzsaszámlálója és hibás befejezési adatai az átvett,
  ténylegesen használt Lua-forrásokban voltak. A questekhez nincs elegendő
  korábbi Git-előzmény a pontos keletkezési dátum bizonyításához. Nem állítjuk,
  hogy az összes hiba az ECS-migráció során vagy azelőtt keletkezett.

## Ellenőrzés és korlátok

A natív tesztek tényleges C++ implementációt és tényleges Lua-forrást futtatnak.
A hálózat, SQL, játékosvilág, spawn és regen szolgáltatásai izolált tesztkörnyezetben
helyettesítettek. A Lua-kezelőket a valódi parser és `HandlerCode` állítja össze,
így a duplikált eseménykezelők eltakarását is reprodukálják.

- Windows: GameServer és ServerPackageCheck fordítás; öt sikeres CTest-csoport:
  dungeon 201, combat 1759, spatial 1016, Lua/source 108, quest runtime 658 ellenőrzés.
- FreeBSD: a tényleges célgépen, külön ideiglenes fordításban fut a dungeon,
  combat és Lua/source regresszió. Ez nem a teljes távoli GameServer újrafordítása.
- Csomageszközök: 37 teszt, köztük ismert eredeti frissítése, LF/CRLF kezelés,
  megváltozott egyedi quest megőrzése, ismételt alkalmazás, hibás csomag elutasítása.
- A helyi csomag ideiglenes másolatában mind a 15 csere alkalmazható; a natív
  betöltő az egész aktív questfára `QUEST_SOURCES_OK quests=69 handlers=533` eredményt ad.
- Három szándékosan visszatett majomhiba mindegyikét elkapja egy teszt: korai
  kapunyitás, játékosonkénti szoborszámláló, Owl-koordinátára teleport.

A célgép a vizsgálatkor FreeBSD 14.2 / Clang 18 volt; nem feltételeztük a korábban
tervezett 15.1-es verziót. A teljes, játékossal bejárt minden lehetséges útvonal
nincs lefedve. A járhatóság, a kliensen látható spawnok és UI-üzenetek, a party
ki-/belépés és a tényleges tárgykézbesítés játéktesztje továbbra is szükséges.

## Telepítendő állományok és játékpróba

A javított források a `SRC/Server/Resources/quest/Quest/dungeon` mappában vannak.
Az új csomagok összeállításakor automatikusan bekerülnek, ismert eredeti
hash alapján; egyedileg szerkesztett questet a konvertáló megőriz és jelez.
A manifest a `SRC/Server/Resources/dungeon-fixes.json`.

Meglévő szervernél a jelenlegi GameServer célrendszeren fordított binárisa
és a 15 érintett quest együtt telepítendő, leállított game core-ok mellett.
Az új `d.count_monster(map, vnum)` miatt a puszta quest-hotreload egy korábbi
binárisra nem elegendő. Adatbázis-migráció nem szükséges. Részletes lépések:
`SRC/Server/Resources/README.md`.

A következő játékpróba ajánlott sorrendje:

1. Majom teljes menet egyedül, majd partyban: az utolsó mob nyissa a kaput,
   öt, két játékos által beadott lándzsa is léptessen; az utolsó őr oldja a bosst.
2. Majom megszakított kapcsolat utáni visszalépés mind a négy szakaszban.
3. Nemere bossbefejezés, piramis késői betöltés és visszalépés.
4. Valentine/Easter utolsó metin/mob azonnali továbblépése és 2. szobás rejoin.
5. Rune/Nightmare nem vezető partytag első érkezése, boss-sorrend.
6. Halloween/Viking jutalom egyszeri átvétele, Meley Bezárás gomb és ismételt kérés.
7. A többi aktív Lua dungeon indítása, befejezése és lejárati takarítása.
