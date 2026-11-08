# IC-067 — Típusos kovácssegédek (C32)

## Katalógus

A v30 katalógus végleges 22 `ForgeAid` kategóriát ad. A
`036_forge_aids.sql` új `item_forge_aid` és `forge_aid_level` táblákat
hoz létre; meglévő játékostárgyat vagy tárolóelrendezést nem alakít át.
A viselkedést típusos szabály határozza meg, nincs vnum-/value-slot-dispatch.

Egy segéd tekercs vagy esélynövelő. A szabály megadja a fogyó darabszámot,
a célkategóriák maszkját (fegyver 1, páncél 2, ékszer 4, behelyezhető
foglalatkő 8), a forrásszint-tartományt, az esélymódot és a kudarcszabályt.
A szintenkénti esélytábla teljes, egyedi, rendezett tartomány; minden
esély bázispontban értendő. A tartalom szerkeszthető 0–255 között.

| Szerkeszthető minta | Esély | Kudarc |
| --- | --- | --- |
| Áldástekercs | Recept szerinti | Egy szint csökkenés; +0 megmarad |
| Mágikus fém | Recept szerinti | Megsemmisülés |
| Sárkányisten-tekercs | 100/75/65/55/45/40/35/25/20%, +8 felett a minta 20% | Egy szint csökkenés |
| Kovácskönyv | 100/100/90/80/70/60/50/30/20%, +8 felett a minta 20% | Egy szint csökkenés |
| Musin | 100% | Megmaradás |
| Emléktekercs | 100%, beállított pontos forrásszint; a minta +0 | Megmaradás |
| Feketesárkány-tekercs | 80%, a mintában +4 foglalatkő | +3-ra csökkenés |
| Kis/közepes/nagy esélynövelő | +5/+10/+15 százalékpont, külön 1/2/3 csoport | A tekercs vagy recept szabálya |

Egy kísérlethez egy tekercs és csoportonként legfeljebb egy esélynövelő
választható. A hozzáadott esély 100%-nál telítődik. Tekercs nélkül
esélynövelő csak közeli, normál kovácsnál használható (20016/20091,
azonos térkép, szerveroldali 2000-es távolság). A tekercs önállóan működik.
A szokásos kovácsművelet és a könyvolvasási segédek külön útjai megmaradnak.

## Egyetlen tartós kísérlet

Az előnézet nem használ RNG-t és nem fogyaszt. A szerver nonce-ot és
operation ID-t hoz létre, majd eltárolja a konkrét célpéldány, kiválasztott
segédek, kiválasztott alapanyag-stackek és Yang előtti állapotát.
Megerősítéskor újra ellenőrzi a jogosultságot, a kovácsot és az állapotot.
A próbálkozás csak ezután kap torzítatlan kriptográfiai dobást.

A `ForgeOutcome=0x3201` Service-írás együtt módosítja a célpéldányt,
fogyasztja a segédeket és anyagokat, és vonja le a Yangot. A költségek
siker és kudarc esetén is teljesek. Elutasítás, régi előnézet vagy SQL-hiba
nem fogyaszt. A kiválasztott segéd nem fizetheti ki önmagát alapanyagként.
Azonos nonce/operation ismétlése a korábbi eredményt adja, újabb levonás nélkül.

A cél ID-ja megmarad. A szintváltozás őrzi a kötést, attribútumokat és
foglalatokat. Megsemmisüléskor nulla darabos, helyfoglalás nélküli auditrekord
marad. A natív writer az `FGR1` receiptből és a katalógusból újraszámítja
a teljes elvárt eredményt; a megváltoztatott költség/payload elutasított.
A függő írás a meglévő actorfoglalást és bejelentkezési session-fence-et használja.

## Szerver és Unity

- `CG 212`: 58 byte + sequence. Művelet, nonce, cél ID, tekercs ID,
  három esélynövelő ID, recept ID és kovács VID. Műveletek: előnézet,
  megerősítés, megszakítás.
- `GC 212 v1`: uint32 méretű, pontosan 80 byte-os válasz; a kiválasztás,
  státusz, végső esély, kudarcszabály/-szint, Yang és szintátmenet szerepel benne.
  Előtte teljes szerveroldali leltárfrissítés érkezik.
- `GC 225 v17`: a v16 mezők után jelenlétjel, típusos kovácssegéd-szabály,
  majd a szintenkénti esélytábla. A nem felszerelt tárolók is átvihetők.
- `GC 227 v4`: az eddigi uint32 actionmező új `Forge=262144` bitje.
  Korábbi tárgyakhoz a korábbi wire-verzió marad, ha nincs kovácssegéd.

A Unity-leltár **Kovácssegéd** gombja megnyitja az önálló ablakot.
A cél, recept, tekercs és három segédcsoport külön választható. A közeli
kovácshoz a kijelölt NPC-t használja. Az **Áttekintés** után a szerver pontos
esélye és kudarcszabálya látható; a **Megerősítés** a tartós eredményre vár.
A kliens nem jósolja meg a szintet és nem von le helyben költséget.

A kezelőben az új kategória tíz szerkeszthető mintát, minden szabálymezőt,
szintenkénti esélytáblát és tartományigazító gombot kapott. A klónozás,
katalógusellenőrzés és tartós publikálás a meglévő natív útvonalat használja.
