# Teljes droplista-munkalap és régi TXT-s scriptmunkafolyamat

Állapot: **javaslat**, a teljes munkalap és a TXT-adapter még nem készült el. A 2026-11-17-i kérésből rögzített igények. Az élő tömeges hozzáadás, eltávolítás, valamint esély-/darabszámmódosítás már külön megvalósult.

## Razor visszajelzése

A felhasználó által megosztott képernyőkép szerint Razor a Notepad++ teljes állományon működő keresését, összes egyező cseréjét, találatkiemelését és oszlopos kijelölését használja. A csoportonként megnyitandó szerkesztők és a gombbal egyenként felveendő sorok lassítják a megszokott munkát. A felhasználó külön megerősítette: a saját script **régi TXT-droplistát** olvas/ír. A scriptet és mintafájlját még nem kaptuk meg, ezért pontos kompatibilitást nem igazoltunk.

A cél egy teljes állományként áttekinthető, billentyűzetről gyorsan módosítható felület, amely ugyanazt a típusos katalógus-validálást és élő kiadási útvonalat használja, mint a meglévő szerkesztők.

## Javasolt fő felület: Teljes droplista

Egyetlen táblázat minden láda és mobdrop bejegyzéseivel. Egy sor egy jutalombejegyzés; a csoportok részletes ablakának megnyitása a rutinszerkesztéshez nem szükséges. A látható oszlopok: **Láda / szörny neve, Tárgy neve, Darab, Esély / súly, Várható esély, Fejlesztés**. A forrástípus és sorsolási mód egyértelmű jelölést kap. A belső kulcsok a modellben maradnak, a normál felület nevekkel dolgozik. Az egyes sorok opcionális részletei külön kérhetők.

| Munkaművelet | Javasolt működés |
|---|---|
| Keresés az egész állományban | Teljes láda- és mobdrop-adatállomány, találatszám, összes találat színes kiemelése; külön „csak találatok” szűrés. |
| Összes egyező cseréje | Korlátozható a kijelölésre, oszlopra vagy a teljes listára; előnézet az érintett forrásokkal és sorokkal. |
| Oszlopos szerkesztés | Téglalapos cellakijelölés, több külön kijelölés, egy érték alkalmazása a kijelölt mezőkre. |
| Másolás / beillesztés | Többsoros, tabulátoros másolás és beillesztés; sor duplázása, több új sor együttes bevitele. |
| Billentyűzetes munka | Enter/Tab cellák között, Ctrl+C/V, Ctrl+F/H, Ctrl+Z/Y; a pontos oszlopkijelölési gesztust Razorral ellenőrizni kell. |
| Visszavonás | A még nem mentett változtatások összevont Undo/Redo lépésekben. |
| Élő mentés | Piszkozat → változások áttekintése → egy ellenőrzött kiadás → minden futó core visszaigazolása. |

A csere típusos művelet: például **Darab oszlop: 5 → 10**, vagy egy kiválasztott meglévő tárgy cseréje másik meglévő tárgyra. Egy névcsere ne nevezze át véletlenül a katalógus tárgydefinícióját. A hibás szám vagy ismeretlen tárgy a piszkozatban legyen megjelölve; ne induljon teljes katalógusvalidálás minden billentyűleütés után. A nagy táblázat modellből, a látható cellák megjelenítésével működjön, soronként létrehozott több ezer állandó widget nélkül. A már elért katalógusgyorsítást meg kell tartani.

## A régi script bevonása

Javasolt folyamat: **export → Razor scriptje vagy Notepad++ → ellenőrzött visszaolvasás → változások előnézete → élő mentés**. A kézi szerkesztést és a script használatát ugyanaz a beolvasó ellenőrizze. Ismeretlen vagy kétértelmű bejegyzésnél érthető, sorhoz kötött hiba kell; ne történjen részleges élesítés.

A pontos adaptert a tényleges script és egy bemeneti/kimeneti TXT-pár alapján kell megtervezni. A régi formátum számazonosítói és az új típusos tárgykulcsok között ellenőrzött megfeleltetés kellhet; az újonnan létrehozott tárgyakhoz nem feltétlenül létezik régi számazonosító. Nem szabad a hiányzó megfeleltetéseket kitalálni vagy a 637 halasztott tételt automatikusan beemelni. Szükség esetén kísérő azonosítótérkép vagy a script bemeneti rétegének kis módosítása szükséges. A futó szerver továbbra is a típusos, verziózott adatbázis-kiadást használja.

Az app egy kiválasztott, ellenőrzött kimeneti fájlt olvasson vissza; a script automatikus futtatását külön kell meghatározni a tényleges script ismeretében. Fájl puszta átírása ne publikáljon felülvizsgálat nélkül éles dropokat.

## Javasolt megvalósítási sorrend

1. Teljes táblázat, közvetlen többcellás szerkesztés, keresés/kiemelés, másolás/beillesztés és Undo/Redo.
2. Kijelölésre vagy oszlopra korlátozott tömeges csere, részletes változás-előnézet és a meglévő közös élő mentés bekötése.
3. A tényleges TXT-script adaptere, veszteségmentes oda-vissza próbák, hibás import és ütköző szerkesztők vizsgálata.
4. Razor valós feladataival használhatósági próba: sok csoport áttekintése, minden egyező darabszám átállítása, tárgycsere több forrásban, scriptelt változtatás betöltése. Mért válaszidők a teljes helyi adathalmazon.
