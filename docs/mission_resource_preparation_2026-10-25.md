# Mission Engine 4/D–4a – erőforrás-átmenetek előkészítése

Dátum: 2026-10-25. Ág: `codex/mission-engine`. Kód: `20e1dd77`.

## Elkészült rész

A `MissionResourcePlan` egy pontos `PinnedReward` és egy karakter-erőforrás
pillanatképe alapján teljes, determinisztikus tervet készít. A csomag tárgyat,
XP-t, pénzt és tárgyfogyasztást együtt tartalmazhat; bármely ellenőrzési hiba
esetén semmilyen részterv nem kerül ki a függvényből.

Ez az előkészítés és annak tartós archívuma. **Még nincs tényleges XP-/pénz- vagy
fogyasztáskézbesítés, élő karaktercache-zárolás, illetve Mission gameplay bekötés.**
A meglévő `IssueItemAwards` továbbra is csak tiszta tárgycsomagot fogad el;
kevert csomagból nem adja ki külön a tárgyakat.

## Az átmenet szabályai

- A forrásban karakterazonosító és külön **erőforrás-revision** van. Ez nem a
  Mission checkpoint/storage revision, és az élő cache-ben még nincs bekötve.
- A tárgyfogyasztás már meglévő, megfelelő tulajdonosú Inventory/ExtraInventory
  stackekből történik, stabil window/category/cell/id sorrendben. Felszerelt,
  zárolt, kereskedésben lévő, lezárt vagy törlésre váró tárgy nem fogyasztható.
- A jutalomként kapott tárgyakkal nem finanszírozható ugyanennek a csomagnak a költsége.
- Minden érintett tárgyprototípushoz külön grant/consume korlátot kér az API.
  Ezek hiteles élő adatból származó értékeiért a későbbi adapter felel.
- A pénzkorlát kizáró: a jelenlegi szerver alapértéke `999000000000`.
  A negatív forrás, a túlcsordulás vagy a limit elérése az egész csomagot elutasítja.
- Az XP a megadott szinttáblán több szinten is átvezethető, a jelenlegi PointSystem
  float32 alapú negyedszint-küszöbeivel. A korábban megszerzett negyedszintet egy
  XP-vesztés után nem osztja ki újra.
- Számolja a statpontokat, a statplafont és az utolsó statpontot adó szint
  szabályát; a skill/subskill pontokat a régi szint és skillgroup alapján adja.
- A szintlépési HP/SP-sorsolás **explicit bemenet**, soha nem történik RNG a
  tervezőben vagy visszatöltéskor. A konkrét sorsolás és a megengedett tartomány
  az archívumba kerül.
- A blokkolt XP-t force szabály nélkül, a maximális szinten elvesző XP-t mindig
  elutasítja. Így egy kevert jutalom egyetlen összetevője sem tűnik el csendben.

A `Before`/`After` a pénzt, az alap progressziós mezőket és a forrás tárgyak
fogyasztás utáni maradékát írja le. A jutalomként kapott új tárgyakat nem teszi
be a leltárba: azok későbbi, közös tranzakciós award-jogosultsága külön feladat.
A tárgypillanatkép gazdasági mezőket tartalmaz, nem a teljes socket/attribute
rekordot; az élő író revision/fence szerződésének minden érintett módosítást
le kell fednie.

A `RefillVitals` jelzés szándék. A szintlépésből következő teljes HP/SP/stamina
újraszámítás, quest/guild/party értesítés, csomagküldés és ECS-frissítés még a
hiteles élő adapter feladata. A tervező nem hívja a mentést is kiváltó régi
`PointSystem::Change` útvonalat.

## Tartós formátum és adatbázis

Az `AURRES01` boríték 64 bájtos, verziózott fejléccel, SHA-256 lenyomattal
és kanonikus JSON-törzzsel tárolja a teljes bemenetet és a kiszámolt eredményt.
Legfeljebb 1024 forrástárgy, 249 szintlépés és 2 MiB tartalom engedett.
A visszaolvasó a JSON mélységét és elemszámát is korlátozza, majd az egész
eredményt újraszámolja a pontos reward-pin alapján. A helyes hash önmagában
nem elegendő: az ismeretlen mező, hamis postimage, eltérő reward vagy nem
kanonikus tartalom elutasításra kerül.

Új tábla a külön Mission-adatbázisban: `mission_engine_resource_plans`.
A sémaverzió **4**, összesen **10 saját InnoDB-tábla és 9 FK**. A negyedik
migráció additív, nem módosítja a legacy player/account/common/log táblákat.
A generált `db/mission_schema.sql`, az explicit `init-mission-db` és az új
csomagok összeállítása a negyedik migrációt is tartalmazza. Régi, 3-as sémán
az új író elutasítja a műveletet; a frissítés leállított szerver mellett végzendő.

Az `ArchiveResources` ellenőrzi és zárolja a Mission karakter owner-epoch/token,
storage revision és checkpoint hash állapotát, majd a teljes pending actiont.
A terv, a definíció, a payload, a forrásrevision és a műveleti nyugta együtt commitol.
Ez sem entitlementet, sem outbox-nyugtázást, sem player/item változást nem hoz létre.

Az action+decision hash kulcs változtathatatlan tartalmat azonosít. Egy actionhöz
legfeljebb 16 eltérő javaslat archiválható; azonos tartalom új művelettokennel is
újra használható. A limit után az új terv visszautasításra kerül, nincs automatikus
régi tervtörlés. A későbbi koordinátornak az író zárolása után kell terveznie,
nem minden gameplay tickben új javaslatot gyártania. Egy historikus javaslat
megőrzése önmagában nem választja ki az alkalmazandó eredményt.

A limit ellenőrzése aktuális zároló olvasás: egy korábban létrejött REPEATABLE
READ nézet sem rejtheti el a másik író utolsó bejegyzését. Bizonytalan COMMIT
után a meglévő `Resolve`/`DecodeResolution` várja meg a karakter írólakat és
ellenőrzi a pontos műveletnyugtát. A régi actions migráció újrafuttatása nem szűkíti
vissza a már frissebb adatbázis művelettípus-korlátját. A korábbi entitlement request hash formátuma
változatlan maradt.

A `ReadResources` már az SQL-projekcióban kizárja a 2 MiB-nál nagyobb blobot,
mielőtt az AsyncSQL kliensoldalon materializálná. A visszaolvasás tulajdonoshoz
kötött, de egy callback pillanatában továbbra is kell a session/generáció ellenőrzése.

## Alkalmazási és újrapróbálási szerződés

Az `InspectResourceApplication` eredménye:

| Állapot | Jelentés |
|---|---|
| Ready | Az aktuális forrás pontosan egyezik a terv bemenetével; ez önmagában még nem végrehajtási engedély. |
| Reconcile | Nyugta nélkül a postimage látszik; további egyeztetés kell, nem tekinthető igazolt alkalmazásnak. |
| Stale | Megváltozott erőforrásállapot; a régi tervet nem szabad ráírni. |
| AlreadyApplied | A hiteles erőforrásíró tartós nyugtája ezt az actiont, döntést és eredményrevisiont igazolja. |
| Invalid | Hibás karakter, adat, állapot vagy ellentmondó nyugta. |

A `ResourceEffectReceipt` itt csak a későbbi hiteles író interfészének típusa.
**Az archívumtábla és a Mission műveletnyugta nem ilyen hatásnyugta.** Nem
állítunk elő hatásnyugtát pusztán abból, hogy egyenlőnek látszanak az összegek.

## Miért következik külön a tényleges erőforrásíró?

A jelenlegi `CPlayerTableCache::OnFlush` abszolút karakterállapotot ment,
a `CItemCache::OnFlush` pedig item REPLACE/DELETE műveleteket küld. Egy ettől
független közvetlen SQL-es Mission-jutalmat egy régi cache vagy már sorban álló
mentés később felülírhatna. Az XP ezen túl szintlépési mellékhatásokat is kivált.

A következő lépés ezért az egyetlen hiteles erőforrásíró és a Database/GameServer
átadási protokollja: session/core-generáció, minden írásra érvényes revision,
a régi írók és függő mentések megállítása/kiürítése, tartós effect receipt és
közös commit a teljes jutalomra/költségre. Csak ezután következhet az ECS
postimage alkalmazása, publikálása és crash utáni egyeztetése. Ez még nyitott
munkafázis, a Gameplay kapu továbbra is zárt.

## Ellenőrzés

- GameServer és Database: sikeres Windows x64 RelWithDebInfo fordítás normál és ASAN módban.
- Natív erőforrástervező, codec és egyeztetés: **1562 ellenőrzés**, normál és ASAN.
- Valódi Mission store/action/item/resource archive: az utolsó normál és ASAN
  körben **1299 + 102** cold-process ellenőrzés, **öt** valódi elveszett COMMIT-válasszal.
- Célzott regresszió **21/21**, ASAN CTest **12/12**; az utolsó módosítások után
  az érintett erőforrás-/DB-próbák külön újra sikeresek.
- Telepítő/config/bootstrap teszt **13/13**. Meglévő kind-5 nyugtával ismételt
  telepítés és az eredeti 17 archivált erőforrásterv megőrzése is ellenőrzött.
- SHA-256: **34 szezonális Mission Pass fájl és 8 korábbi Unity-módosítás változatlan**.

A pontos eredmények a központi `mission_engine_status_and_next_steps_2026-10-25.md`
dokumentumban vannak. A próbák a tényleges natív tervezőt, codecet és store-t
futtatják; külön ideiglenes MariaDB-példányon vizsgálják a rollbacket,
tokenütközést, tulajdonos/checkpoint eltérést, cancelled actiont, a blobkorlátot,
a párhuzamos limitellenőrzést, az elveszett COMMIT-választ és a kényszerített
MariaDB-újraindítás utáni új C++ folyamatot.

Futó játékszerver, valódi játékosadatbázis, Unity-forrás és a szezonális Mission
Pass ebben a munkafázisban nem módosult. FreeBSD build és játékbeli elfogadás
nem történt.
