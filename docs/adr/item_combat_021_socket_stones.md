# ADR IC-021 — Foglalatkövek és típusos foglalatállapot

2026-10-28. Állapot: natív domain, v14 fejlesztői katalógus, SQL és Qt
szerkesztő megvalósítva, helyben ellenőrizve. A GameServer/Database/Unity
runtime átállítása továbbra is IC-012 későbbi fázisa.

## Jelenlegi viselkedés bizonyítékai

- `ItemSystem_LegacyBridge.cpp::UseItemEx`, ITEM_METIN ág: a cél nem lehet
  cserében vagy felszerelve; csak fegyver/testpáncél megfelelő wear flaggel.
  `KET_BONUSZOS_KOVEK` mellett mindkét csoport mindkét másik csoporttal
  összehasonlított, a nulla csoport nem tilt. A flag a `common/service.h`-ban
  engedélyezett. Az első megfelelő nyitott foglalatba kerül a kő.
- Ugyanitt: a sikertelen próba is elfogyasztja a követ, és a foglalatot a
  régi 28960-as sentinelre állítja. Az aktív `ENABLE_BUG_FIXES` ág egy darabot
  fogyaszt a halomból. A törött foglalat közvetlenül nem írható felül.
- `common/item_length.h`: három foglalat; a régi 0 zárt, 1/2 nyitott minőség,
  a beágyazott azonosító más állapot. `core/config.cpp` és `GameConfig.cpp`:
  `stone_chance`, alapból 30%, a mai szerver 1..100 közé korlátozza.
- `ItemSystem.cpp::MakeItemPrototype`: a jelenlegi fejlesztési szint a név
  +N végéből származik. Az új runtime ezt nem olvassa vissza: példányszint
  és explicit fejlesztési profil váltja fel.

## Új szerződés

A 7-es kategória `item_socket_stone` táblája behelyezhető követ és külön
kőszilánkot ismer. A kompatibilitás fegyver/testpáncél maszk, minimum normál
vagy arany foglalat és két opcionális, stabil csoportkód. Nem kerül át ide
értéktömb, régi item-ID vagy wear-bitből végzett runtime típusfeloldás.
A szilánk nem helyezhető be, nincs hatása és nem fejleszthető.

A behelyezhető kő alapbónusza a közös ItemBase profil, fejlesztése az új
`RefinementKind::SocketStone` (5) profil szintenkénti bónusza. Nincsenek
rejtett támadás/védelem raw nyereségek. A foglalatban egy ItemKey és konkrét
példányszint él; +0…+30 vagy +255 nem igényel új prototípust.
Kizárólag a beépített kő járul majd hozzá a célfelszerelés statjaihoz;
az inventoryban hordozott kő nem felszerelés.

A zárt/üres/beépített/törött foglalat külön enumállapot. Zártnál nincs
minőség, kő és szint; üres/törött állapotban nincs kő és szint; beépítettnél
létező kőkulcs és érvényes szint kell. A minőség behelyezés és törés után
is megmarad. Nincs 28960-sentinel vagy 1/2-ként értelmezett item-ID.

A tiszta `PrepareSocketStoneInsert` csak szerveresen befogadott kérésből
dolgozik: két külön tulajdonolt instance, nem nulla és nem kifutott revision,
zár/csere/felszerelés, halomhatár, szint, harc, célkompatibilitás, az összes
foglalat és keresztcsoport-ütközés ellenőrzött. Kizárólag az első kompatibilis
üres foglalat változhat. A döntés a szerver explicit sikeresélyét és rögzített
dobását használja; 0/100% esetén nincs RNG-dobás. Hiányzó szabály nem lesz
hallgatólagosan 30%.

A terv birtokolja a teljes bemeneti állapotot, az operation ID-t, ruleset
revisiont, katalógusrevisiont és hasht. Egy darab fogyasztását és egyetlen
foglalat cseréjét javasolja, siker és kudarc esetén is. Nem ír ECS-t, SQL-t,
hálózatot, Lua-t és nem hív RNG-t. A későbbi owner ellenőrzi a sessiont,
kasztot/nemet és helyet, majd receipt + fogyasztás + foglalatcsere atomikusan
commitolható. CommitUnknown esetén tartós operation ID szerinti egyeztetés kell.

A közös gazdasági fejlesztőrecept behelyezhető kőhöz is használható akkor,
ha a konkrét forráspéldány darabszáma pontosan egy. A halmozható prototípus
önmagában nem tiltás; count>1 továbbra is hiba. Nincs egész halmot egy
költségért fejlesztő művelet, második gazdasági író vagy legacy receptfeloldó.

## Séma és kiadási határ

A `020_socket_stones.sql` és a `auriga.item_catalog.v14` kanonikus tartomány
új, nem kiadott séma. A teljes friss létrehozási lánc 000…020. A 004-es
draft refinement-kind CHECK-je explicit nevet kapott, hogy az új draft
szabálya ellenőrizhetően bővíthető legyen. Ez nem a már telepített v7-en
futtatható helybeni frissítő: annak adatait az offline export/felkészítés
őrzi meg és új sémán ellenőrzi. Az aktív adatbázis és a telepített app érintetlen.

A régi 189 kő minden sora külön auditbejegyzést kapott. A kizárócsoport
egyezése nem bizonyít prototípusazonosságot; a sorokat nem egyesítjük pusztán
ennek alapján. A teljes családkonverzió, régi apply/policy/ár/extra proto,
beágyazott játékospéldányok, kiemelés/tisztítás és runtime statbekötés még
további implementáció. A DragonSoul rendszerhez ez nem ad új integrációt.
