# Kapcsolható harmatok — elkészült fejlesztési szelet

2026-10-28. Ág: `codex/mission-engine`, kiinduló HEAD:
`45e76b230f83a9adc2a244881f488786679ef7e7`. A munkafa már a kezdéskor
módosított volt; a korábbi review, időzített hatás és automata helyreállítás
munkáját megőriztem. Ebben a körben nem történt commit vagy telepítés.

## Elkészült

- A `USE_NEW_POTIION` működésének célzott auditja: itemhez kötött affect,
  aktiválási zárolás, aktív online idő, azonos típus elutasítása, kizáró párok,
  házasság és csoportvezető feltétel, login-visszatöltés és feltételvesztés.
- [IC-017](../../adr/item_combat_017_toggle_effects.md): külön típusos
  kapcsolható hatás; nem gyógyító palack és nem használatkor elfogyó időzített ital.
- `ToggleEffectGroup` és `ToggleEffectProfile`: stabil csoport, variáns,
  aktív idő, feltételmaszk, bónusz, kontextus/PvP és opcionális cooldown.
- Tiszta használati és időelszámolási tervek. Saját példány újrahasználata
  kikapcsol; azonos csoport+variáns másik példánya elutasított; másik variáns
  cserét javasol. A kikapcsolás halál, retire vagy elvesztett feltétel után is
  működik. Globális/map engedély hiányában nincs aktiválás.
- Pontos egész ms-elszámolás, inaktív állapotban nincs fogyás, nulla időnél
  levétel/törlés javaslat. Nincs stackbontás, vnum/affect/socket adapter,
  aktiváláskori darabfogyasztás vagy tickenként ismételt bónuszgrant.
- V10 katalógus: két típusos tábla, háromirányú kizárólagos fogyasztható
  hivatkozás, snapshot-betöltés, kereszthivatkozás-ellenőrzés és kanonikus hash.
  38 fizikai tábla, ebből 37 a snapshot része; a kiosztó számláló külön van.
- Natív atomi katalógusmentés, a meglévő prepared SQL infrastruktúrával és
  változatlan tranzakciós limitekkel. Meglévő csoport szabályait a natív író
  is védi a módosítástól/törléstől.
- Qt tárgyszerkesztő: **Fogyasztható → Kapcsolható harmat**. Idő, variáns,
  feltételek, csoport, halálszabály, bónuszok és cooldown egy űrlapon;
  a halmozás 1-re áll. Másolás/szerkesztés előtt a megosztott profilok
  különválnak. Gyógyító, időzített és kapcsolható típus közötti váltás tesztelt.
- Javított hivatkozástulajdonos-kezelés az aktív hatások bónuszmásolásánál;
  mentett nem gyógyító tárgyról gyógyítóra váltáskor nincs `None`-ba gyűjtés.
  Az új validációs mezők magyar megnevezést kapnak.

A kategóriás identitás és az egy prototípus/példányszint modell változatlan.
A harmat profilja a kezdő időt adja, a maradék a későbbi item-instance állapota.
Nem hoz létre +0…+9 ismétlődő prototípusokat és nem állít vissza kivezetett
DragonSoul, offline shop vagy régi BattlePass funkciót.

## Ellenőrzés

| Ellenőrzés | Eredmény | Bizonyíték |
|---|---:|---|
| Teljes `ServerTests` fordítás és normál CTest | 105/105 sikeres | `build/toggle-build.log`, `build/toggle-build-final.log`, `build/toggle-verified-ctest.log` |
| Kapcsolható hatás C++ ellenőrzései | 1065 sikeres | `ToggleEffectTests`: feltételek, cserék, sérült binding, maximum kapacitás, halál, retire, lejárat, 1000 determinisztikus időkonzervációs eset |
| Qt/modell/natív SQL mentés | 35/35 sikeres | `build/toggle-editor-verified.log` |
| ASAN katalógus/domain célok | 16/16 sikeres | `build-asan/toggle-verified-tests.log` |
| Szerkesztő ASAN natív helperrel | 35/35 sikeres | `build-asan/toggle-editor-verified.log` |
| Teljes szerverkezelő GUI regresszió | 14/14 sikeres | `build/toggle-manager-gui-tests.log` |
| Magyar validációs hiba utóellenőrzése | normál + ASAN sikeres | `build/toggle-hungarian-error-test.log`, `build-asan/toggle-hungarian-error-test.log` |

Az SQL-próbák kizárólag újonnan indított, saját MariaDB 11.4.13 példányokat
használtak. A sémaellenőrzés 26 esetet futtatott, beleértve az új táblák
CHECK/FK és pontosan-egy-hatás megkötését. A natív mentés a szélső időt,
variánst, megosztott bónuszok módosítását és típusközti váltást is visszaolvassa.
A korábbi SQL-fixture-ök explicit oszloplistát kaptak a bővített sémához.

A 17 tárgyas közös fixture független Python/C++ SHA-256 eredménye:
`7ed6989da20419bfdbf9930d644498870714207a3647495ce4bc8c8df9bb3b17`.
A kapcsolható profilok minden viselkedési mezőjének hash-hatása külön tesztelt.

Az offscreen űrlapot 1400×950 és 1100×760 méretben rendereltem. A kisebb
képet vizuálisan ellenőriztem: a mezők és bónuszlista olvashatók, az oldal
görgethető. Képek: `build/toggle-effect-editor.png` és
`build/toggle-effect-editor-small.png`. Ez memóriában létrehozott teszttárgy,
nem az élő helyi adatbázis képernyője.

## Még nincs bekötve

A `SRC/ItemDomain` továbbra is izolált fejlesztési modul. A GameServer,
Database és Unity nem használja még ezt a katalógust élő tárgyakhoz.
Az ownership/revision, lock, timer, stat, cooldown, aktív binding, törlés és
receipt közös runtime commitja F3/F4; a protokoll/Unity F5, a valódi régi
adatok teljes offline konverziója F6, a helyi átállás F7.

Az owner minden ki/be kapcsolás, csere, logout és feltételváltás előtt
elszámolja az eltelt aktív időt. Ezt a tiszta tervező nem tudja egyedül
kikényszeríteni; a későbbi writer feladata, és nem tekintjük késznek.
A valódi régi APPLY-adatok teljes besorolása sem történt meg ebben a szeletben.

A meglévő helyi felhasználói katalógust és a telepített appot nem módosítottam.
A friss draft migrációlánc nem v7→v10 upgrader. Telepítés előtt külön
adatmegőrző sémafrissítés és visszaállítási próba kell. Távoli gépen és éles
adatbázison nem történt művelet.

## Következő lépés

IC-014 szerint a 4. szelet: **képességkönyvek** célzott auditja, majd a 9-es
kategória típusos modellje, katalógus-, SQL- és szerkesztőtámogatása.
Ezután a tárgyon végzett műveletek és a fennmaradó nevesített viselkedések;
az F2 kapu után következik a tartós új példánymodell és a futó szerver bekötése.
