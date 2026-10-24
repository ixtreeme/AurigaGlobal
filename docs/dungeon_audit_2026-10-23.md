# Nemere és piramis: forrásaudit és regressziós tesztek

A vizsgálat a Nemere boss halálakor elmaradó befejezésére és a piramis üresnek
tűnő indulására készült. A távoli tesztszerveren kizárólag olvasás történt:
konfiguráció, dungeonforrás, mob-prototípusok és a bejelentett próbák naplói.
A javítások a Git-munkafában vannak; a futó szerver binárisát nem cseréltük le.

## Nemere: a kilépési pont törölte a tagságot

A `nemere_zone.lua` belépéskor `pc.set_warp_location(219, 5354, 14284)` hívással
beállítja a kijáratot. A `MovementSystem::SetWarpLocationRaw` korábban ekkor
`ClearDungeonIfOtherMap` hívást is végzett. A játékos a 352-es map privát
pályáján állt, a mentett kijárat viszont a 219-es mapra mutatott; a hívás ezért
leválasztotta a dungeonről.

A `when 6191.kill with pc.in_dungeon()` esemény nem futott le tagság nélkül.
A 2026-10-23 17:37:32-es távoli napló igazolja, hogy maga a 6191-es halálesemény
elérte a questkezelőt; a boss 3000 EXP-s prototípusa is jelen van az SQL-ben.
Nem hiányzó boss vagy hiányzó kill-bejelentés okozta ezt a kódhibát.

Ugyanez a hibás mellékhatás a `WarpEnd` által nullázott célpontnál is jelentkezett.
A javítás után a célpont eltárolása és nullázása megőrzi a tagságot. A leválasztás
a tényleges `WarpSet` és az érvényes célpályára történő `Show` útján történik;
azonos mapra helyezés vagy elutasított `Show` nem választ le.

## Piramis: az indítás megelőzte a kliens érkezését

Az NPC-re kattintás létrehozta az instanciát, rögtön beállította a `floor=2`
állapotot, és egy másodperc múlva létrehozta a metineket, kiküldte az egyszeri
kezdőüzenetet. A később belépő játékosnál a `floor=2` miatt már nem történt
indítás. A távoli naplóban 17:38:56-kor kezdődött a warp, a játékos visszatöltése
pedig csak 17:38:58 után történt, így a kezdőüzenet üres pályára mehetett.

Új runnál az instancia `floor=0` állapotban vár. Az első tényleges belépés
hozzárendeli a játékost a dungeonhöz, és egyszer ütemezi az előkészítést. Nem
szükséges, hogy a party vezetője töltsön be először. Ismételt belépés nem
indítja újra a folyamatban lévő vagy befejezett runt. Ha a privát instancia
már megszűnt, a játékost a lobbyba irányítjuk.

A régi belépéskor sem bizonyított a metinek létrehozásának elmaradása:
az első metin a belépési ponttól X irányban 88 cellával távolabb van. A
kezdőüzenet hiánya és a kliens látótávolsága együtt üres pálya benyomását
keltheti. A forrásban és az eredeti Lua-ban szereplő spawnpozíciókat nem
változtattuk meg; ezeket a következő játéktesztben külön ellenőrizni kell.

## Piramis: visszalépés és kill-elszigetelés

A visszalépés más pályához tartozó `(218600, 348900)` koordinátákat használt
a 357-es privát mapazonosító mellé. A `WarpSet` ellenőrzi a koordinátákból
feloldott alapmapot, így ezt elutasíthatta. Most az eredeti belépés
`(979600, 2366000)` pontját használja. Sikertelen warp esetén megmaradnak
a visszalépési adatok; csak siker után nullázódnak.

A kill-feldolgozó a megölt mob mapját is ellenőrzi: másik privát instanciából
származó mob nem csökkentheti a metinszámlálót.

## Automatikus ellenőrzések

- `SpatialLifecycleTests`: a valódi mozgáskódon ellenőrzi a Nemere-kijárat
  mentését, a célpont nullázását, az azonos mapon maradást, a sikertelen mapra
  helyezést és a tényleges kilépéskor szükséges leválasztást.
- `LuaSourceTests` / `NPCSourceTests`: a valódi questparser és az NPC-szkriptek
  összeállítása után Lua 5.5-ben futtatja a Nemere típusú, `6191.kill` feltételes
  kezelőt. Tagság nélkül, eltérő mapon és hibás emeleten nincs befejezés;
  megfelelő állapotban lefut a befejezés és az exit NPC létrehozása.
- `DungeonLifecycleTests`: a valódi piramisvezérlőt és `dungeon.cpp`-t futtatja
  izolált világgal és vezérelhető időzítővel. Késleltetett érkezés, ismételt
  login, 7 metin → kő → mobhullám → boss → kijárat, eltérő instancia,
  elsőként belépő partytag, megszűnt instancia és visszalépés is szerepel.

A moblétrehozás, hálózat, SQL és regenbetöltés a tesztben helyettesített
szolgáltatás. A teljes klienssel történő végigjátszást a tesztek nem fedik le.

```powershell
cmake --build build --config RelWithDebInfo --target SpatialLifecycleTests DungeonLifecycleTests LuaSourceTests GameServer --parallel 6
ctest --test-dir build -C RelWithDebInfo -R '^(spatial_lifecycle|dungeon_lifecycle|lua_sources|quest_runtime)$' --output-on-failure
```

Telepítés után külön ellenőrizendő: Nemere boss halála utáni befejezés és
kijárat, a piramis kezdőüzenete hosszabb betöltésnél, a hét metin helye,
a teljes menet és a megszakított kapcsolat utáni visszalépés.
