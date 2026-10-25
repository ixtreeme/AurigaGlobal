# Mission Engine – verziózott natív jutalomcsomagok

**Dátum:** 2026-10-25. **Részfázis:** 4/D–1, az outbox/entitlement réteg után.
**Kódcommit:** `bd3a2fd1`.

Későbbi lépés: a `71abb5d7` commit elkészítette a közös item_award → item
kézbesítő atomi javítását, valódi tárgysémás crash/retry próbákkal.
Részletek: [4/D–2 kézbesítő](D:/AurigaGlobal/LiveWork/AurigaGlobal/docs/mission_item_award_delivery_2026-10-25.md).
A jelen dokumentum a 4/D–1 adatleírási réteget rögzíti. A Mission ledger és az
award létrehozása még nincs összekötve; a kevert jutalomcsomag nem fut élőben.

## Eredmény és határ

A reward-node szimbolikus kulcsa most egy típusos, a küldetés pontos verziójában
tárolt jutalomcsomagra mutathat. Tárgyak, XP, pénz és tárgyfogyasztási költségek
írhatók le. A csomag tartalma része a definíció lenyomatának, az archivált
`.aurmission` bájtoknak és a checkpoint/outbox verziókötésének.

**Ez adatleírás, validálás és tartós tartalomkötés; nem jutalomkézbesítő.**
Nem keletkezik tárgy, nem fogy inventory, nem változik XP vagy pénz. Nincs
éles gameplay-adapter, amely ezeket a képességeket igazolná vagy meghívná.
A ledger továbbra is jogosultsági rekord, nem tényleges hatásnyugta.

## 1. Típusos authoring

A teljes példa: `config/mission_engine_reward_package.example.json`. A külső
package-authoring schema továbbra is 1, a jutalompayloadot tartalmazó
**definíció schema értéke 2**. Egy jutalom a definíció `rewards` táblájában:

```json
"rewards": {
  "demo.bundle": {
    "schema": 1,
    "items": [{"vnum": 50011, "count": 2}],
    "experience": 1000,
    "gold": 100,
    "consume_items": [{"vnum": 50013, "count": 3}]
  }
}
```

A reward-node továbbra is `"reward": "demo.bundle"` kulccsal hivatkozik rá.
A példában szereplő vnumok izolált tesztadatok; nincs hozzájuk éles proto-
binding. A típusos forrásnak új jutalomadat esetén új mission-verziót kell adni.
Pusztán a package verziójának emelése nem jogosít fel egy régi gráf átírására.

| Mező | Jelentés |
|---|---|
| `items` | Konkrét vnum és pozitív mennyiség adása |
| `experience` | Pozitív XP-növekmény, 0/hiány esetén nincs ilyen hatás |
| `gold` | Pozitív pénznövekmény, 0/hiány esetén nincs ilyen hatás |
| `consume_items` | Előzetesen meglévő tárgyakból teljesítendő költség |

Minden mező opcionális a kötelező payload `schema: 1` mellett, de a teljes
csomag nem lehet üres. A tárgylisták sorrendje nem jelent végrehajtási sorrendet:
számszerű vnum szerint rendezett, egyedi tárolóvá normalizálódnak. Ismételt vnum
ugyanazon listában hiba; nem összeadjuk vagy felülírjuk a mennyiségeket.
A grant és a consume lista külön erőforrásoldal, ugyanazt a vnumot is tartalmazhatja.

Az elvárt kézbesítési szerződés az **egész csomagra atomi**. A kapott tárgy
nem finanszírozhatja ugyanazon csomag költségét: azt a korábban meglévő
erőforrásból kell majd hitelesen ellenőrizni, lefoglalni és egyszer elfogyasztani.
Ez a szerződés itt még nincs végrehajtva; mixed/cost tartalom nem kapcsolható
be addig, amíg a tényleges adapter ezt crash/retry alatt nem igazolja.

Nincs script, véletlen tárgygenerálás, kliens által megadott mennyiség,
negatív összeg, dinamikus kifejezés, socket vagy attribútum ebben a payload-
verzióban. Ismeretlen mező, lebegőpontos/szöveges szám és duplikált JSON-kulcs hiba.

## 2. Korlátok és teljes lefedés

- Legfeljebb 128 név szerinti jutalomcsomag definíciónként.
- Egy csomagban összesen legfeljebb 32 item/consume bejegyzés.
- Egy bejegyzés mennyisége 1–50 000; a vnum pozitív `uint32`.
- XP/pénz 0–`INT64_MAX`; a tényleges adapter ennél szigorúbb korlátot követelhet.
- Legfeljebb 512 item/consume bejegyzés definíciónként és 4096 a teljes package-ben.
- A korábbi 1 MiB/gráf és 16 MiB/package korlát megmarad.

Ha a definíció reward-táblája nem üres, minden reward-kulcsnak pontosan
megfelelő payload kell, és nincs árva payload. Több node használhat azonos
jutalomkulcsot. Régi, teljesen üres reward-tábla domain-szimulációhoz olvasható;
félig kitöltött tábla nem fogadható el.

A méretkorlátokat a JSON és a bináris betöltő **fokozatos dekódolás közben**
ellenőrzi, nem csak a már felépített teljes typed katalóguson. A közvetlen
C++-forrás a definíció- és package-validátoron is átmegy. Nincs csendes csonkítás.
A JSON DOM továbbra is teljes, legfeljebb 16 MiB-os forrásból épül; ez nem streaming JSON-parser.

## 3. Formátum és régi csomagok

A `.aurmission` konténer formátuma és 64 bájtos fejléce marad 1-es. A gráfséma:

- **1:** a korábbi, payload nélküli gráf; bájtjai és definícióhash-e megmaradnak.
- **2:** a gráfszekció 2-es verziója, definíciónként rendezett reward-táblával.

Ha bármely gráf konkrét payloadot tartalmaz, a compiler a csomag gráfsémáját
2-re állítja. A többi szekció és a natív regiszter sémaértéke 1 marad. A loader
ellenőrzi a fejléc és a gráfszekció verziójának egyezését. Egy régi program
elutasítja az új gráfsémát, nem hagyja figyelmen kívül a jutalmat.

A schema-2 payload explicit méretű, little-endian számokból, rendezett
kulcsokból és verziózott, típusos adatokból áll; nem C++ memória-dump.
Ismeretlen payload-/gráfséma, hibás szám, nem rendezett vagy duplikált lista,
túlméretezés, csonka adat és trailing bájt elutasításra kerül.

A korábbi compilerrel ténylegesen előállított, 1357 bájtos fixture:
`tests/server/fixtures/mission_package_v1.aurmission`.
SHA-256: `fd83bfeffc45e97d4bd8364fa6ccc17e7ffc0c811e3145a4030078a4f2b8a335`.
A teszt ellenőrzi az eredeti hash-t, az új loaderrel való olvasást, a teljes
bájtpontos újrafordítást és az eredeti gráfbájtokból számított régi definícióhash-t.

## 4. Változtathatatlan tartalomkötés

Payload nélküli definíció `FingerprintDefinition` értéke változatlan.
Payloadot tartalmazó definíciónál a hash külön `MEGRAPH`/2 domainjelölőt és
a reward-táblát is tartalmazó kanonikus gráfbájtokat használja. A külön
`FingerprintReward` másik domainnel, a teljes csomag tartalmát azonosítja.

Ugyanazon mission ID/verzió mellett eltérő tárgy, mennyiség, XP, pénz vagy
költség más gráflenyomatot eredményez. A registry és az SQL-archívum megtagadja
az átírást, a korábbi checkpoint nem tölthető vissza a módosított gráffal.
Egy új aktív verzió nem írja át a régi futó példányt vagy megszerzett jogosultságot.

A `PinReward(action, expectedDefinitionDigest, graph)` csak pontos egyezésnél
készít `PinnedReward` objektumot. Ellenőrzi a mission/verzió/node/reward
kapcsolatot és a gráflenyomatot, majd saját, módosító API nélküli payload-
értéket ad. Régi, csak szimbolikus jutalomhoz nem talál ki utólag payloadot.
Ez tartalomkötés, nem prototípus-, session- vagy egyenlegengedélyezés.

Az egyszeri művelet identitása továbbra is a teljes **ActionKey**, nem a
payload hash. Két külön jogosultságnak lehet azonos jutalomtartalma; ezeket
a kézbesítő nem vonhatja össze a közös tartalomhash miatt.

## 5. Natív binding és bekapcsolási kapu

A regiszter 32 korábbi kernel-primitívje mellett négy új **payload-sémát** exportál:

| ID | Tartalom |
|---|---|
| `0x5001` | Tárgyjutalom leírása |
| `0x5002` | XP-jutalom leírása |
| `0x5003` | Pénzjutalom leírása |
| `0x5004` | Tárgyköltség leírása |

Ezek `RewardPayload` kategóriájúak, `gameplay_bound=false` jelzéssel. Nem az
előirányzott `0x400x` gameplay action-handlerek helyére kerültek. A 208-as
vizuális katalógus jutalom-/inventory-node-jai ettől még nem teljesültek.

A compiler a tényleges payloadokból levezeti a kötelező natív képességeket.
A Gameplay aktiválás ezekhez külön trusted bindingot követel; továbbá:

- minden reward-node-hoz tényleges typed payload kell;
- igazolt, egész csomagra kiterjedő atomi kézbesítési szerződés kell;
- XP/pénz a kézbesítő által megadott határ alatt kell maradjon;
- minden item-vnumhoz külön grant- és consume-mennyiséghatár szükséges.

Ismeretlen prototípus, csak fogyaszthatónak igazolt tárgy jutalomként használva,
elégtelen mennyiségi binding vagy hiányzó effect-képesség esetén az egész
aktiválás elutasításra kerül. A trusted adatokat a későbbi, ténylegesen működő
natív adapter szolgáltathatja, nem a JSON, a kliens vagy egy kezelőfelületi checkbox.
Jelenleg ilyen élő adapter nincs regisztrálva. A tesztben megadott attesztációk
izolált szerződéspróbák; nem jelentenek elkészült kézbesítőt.

## 6. Outbox és ledger kapcsolata

Új SQL-migráció ehhez a részhez nem kell: a 2-es Mission-séma outboxa már
exact gráflenyomatot tárol és FK-val az archivált definícióhoz köti. A konkrét
payload az adott gráf archivált bájtjaiból állítható vissza.

A `RecordEntitlement` API most **PinnedReward** paramétert követel a puszta
ActionKey helyett. Az SQL a checkpoint/owner/revízió mellett az outbox
definícióhash-ét is összeveti a pinnel. A tartós kéréslenyomat a definíció- és
payload-hasht is tartalmazza. Azonos kulccsal, de más jutalomtartalommal
felépített pin nem könyvelhető a régi outboxhoz.

A régi, payload nélküli szimbolikus jogosultság nem kap automatikusan új
tartalmat. Ha ilyen korábbi adatot egyszer importálni kell, külön, igazolt
mapping és műveletnapló szükséges. Éles Mission-adat továbbra sem volt bekötve.

A ledger nem igazolja, hogy egy cost teljesült vagy a jutalom inventoryba
került. A jövőbeli koordinátornak a hatás végrehajtási szerződését, hiteles
erőforrásfoglalását és eredményét külön kell összekötnie az entitlementtel.

## 7. Ellenőrzés és folytatás

A végleges Windows x64 / RelWithDebInfo kód eredményei:

| Ellenőrzés | Sikeres eredmény |
|---|---|
| GameServer normál és ASAN fordítás | Lefordult |
| Célzott normál regressziós CTest | 28/28 |
| Külön ASAN CTest | 7/7 |
| Új reward-payload suite, normál és ASAN | 2109 ellenőrzés |
| Csomag / checkpoint suite | 4856 / 3227 ellenőrzés |
| Valódi Mission store/action, normál | 577 + 42 cold-process ellenőrzés |
| Valódi Mission store/action, ASAN | 575 + 42 cold-process ellenőrzés |
| Védett szezonális / meglévő Unity-fájlok | 34 / 8 változatlan |

A store polling miatt az ellenőrzések száma futásonként eltérhet. A külön
MariaDB 11.4.13 példány kényszerített leállítása után új C++ folyamat olvasta
vissza a régi payloadot az aktív új verzió mellett. A server.log valódi InnoDB
crash recoveryt rögzített. FreeBSD build, Unity end-to-end próba, éles DB-
módosítás vagy távoli telepítés ebben a részfázisban nem történt.

A próbák lefedik a valódi régi binárist, a typed roundtripet, tartalomütközést, pinninget,
capability-/prototípuskorlátokat, hibás JSON-t, csonka/mutált binárisokat és
az SQL-archívum/outbox régi jutalomtartalmának hideg visszatöltését.
A bináris költségkorlátot külön, a compiler megkerülésével összerakott túlméretes
gráfszekció is próbálja: a loader már dekódolás közben elutasítja.

Helyi naplók az ignorált `build` mappában: `mission-reward-payloads-final-build.log`,
`mission-reward-payloads-asan-build.log`, `mission-reward-payloads-regression-tests.log`,
`mission-reward-payloads-asan-unit-tests.log`, `mission-reward-payloads-asan-db-tests.log`.

Következő: a kanonikus tárgytárolóhoz kapcsolódó, tartós claim- és kézbesítési
protokoll. Ennek kell megszüntetnie a régi item INSERT és az award taken UPDATE
közötti megszakítási rést, tényleges item-sémás crash/retry/full-inventory
próbával. Utána jöhet a többi erőforráshatás és a session-/core-életciklus
bekötése. Az új adatleírás önmagában még nem engedélyezi az új Mission Engine
játékosokra kapcsolását vagy távoli/éles telepítését.
