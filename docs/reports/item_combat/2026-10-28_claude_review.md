# Itemkatalógus és szerkesztő — átvételi ellenőrzés, javítás és folytatás

2026-10-28. Ellenőrzött kiinduló HEAD: `45e76b23`.
Ág: `codex/mission-engine`. A munkafa az ellenőrzés kezdetén tiszta volt.

## Átnézett változások

- `94d29303`: SQL-betöltő és a Qt tárgyszerkesztő befejezése.
- `290f8460`: item-runtime-first döntés és régi tartalomleltár.
- `c4ccb126`: típusos ládák, kulcsok, lootprofilok, SQL és szerkesztő.
- `45e76b23`: használati tárgyak leltára és pénzjegykategória.

Átnéztem az új domainterveket, a katalógus/hash/SQL-dekóder változásait,
a mentési sorrendet és a profilok másolását/takarítását, a Qt felület
háttérműveleteit, valamint a hozzájuk tartozó teszteket. A leltárdokumentum
nem minden régi használati ág végigkövetésének újraellenőrzése: a még
nem portolt viselkedések teljes auditja továbbra is külön kapu.

A friss alapállapot-build és a meglévő normál CTest **102/102** eredménnyel
lefutott; a külön Qt-kör 17 tesztje szintén sikeres volt. A további szélsőérték-
próbák az alábbi hiányosságokat találták. Ezért a korábbi zöld eredményeket
nem tekintettem teljes körű átvételi bizonyítéknak.

## Talált és javított hibák

### P2 — Érvényes nagy fejlesztési görbe nem menthető

A `CatalogEditorTool::Commit` soronként külön prepared lépést készített.
Egy +0…+255 görbe, szintenként külön bónuszprofillal, több mint 512 lépést
igényelt. A domainellenőrzés sikeres volt, a mentés azonban „Tul sok valtozas
egy menteshez” hibával leállt. A reprodukció naplója:
`build/claude-review-curve-repro.log`.

A javított `AddBatches` legfeljebb 100 sort küld egy bound JSON paraméterben,
és `JSON_TABLE` segítségével írja a típusos céloszlopokat. A JSON csak a
prepared paraméter szállítási formája; nem kerül JSON item-proto az SQL-be.
Megmarad a sima INSERT egyediütközés-kezelése, az FK-k szerinti sorrend és
az egyetlen tranzakció. UPDATE/DELETE esetén az érintett sorok száma is
előfeltétel. A közös SQL-motor 512 lépéses, 4096 paraméteres és byte-kerete
nem változott.

A regresszió +255-ig három külön bónuszt ad minden szinten, visszaolvassa,
teljesen átírja, majd +0-ra csökkenti a görbét. Egy másik teszt adatbázis-
triggerrel szándékosan elutasítja a kategóriasort: az előtte írt összes profil
és az azonosítószámláló léptetése is visszagördül. A vizsgálat saját,
eldobható MariaDB 11.4.13 példányon futott.

### P2 — Túlnövelt halom alapján is készülhet műveleti terv

A `PlanTokenRedeem` és a `PlanContainerOpen` nem utasította el a prototípus
`maxStack` értékénél nagyobb bemeneti halmot. A pénzjegy így érvénytelen
állapot alapján is jóváírási tervet adhatott. A ládánál sem a doboz, sem a
felhasznált kulcs halomhatára nem volt ellenőrizve.

Mindkét API `InvalidStack` hibát ad ilyen állapotra. A ládánál az elutasítás
a loot-döntés előtt történik; ezt a teszt az RNG-hívások hiányával is méri.
A nagy számos túlcsorduláspróba saját, valóban UINT32_MAX stacklimites
prototípust használ. A két domainterv jelenleg nincs bekötve a futó
GameServerbe; ez nem élő exploit-reprodukció.

### P2 — Mentés közben begépelt értékek elveszhetnek

Az `ItemPage.controls` a gombokat zárolta, az adatlap mezőit azonban nem.
A háttérben indított mentés az indításkori dokumentumot kapja, majd siker
után újratölti az űrlapot. A közben begépelt módosításokat ez eldobhatta.

A teljes tárgyadatlap a háttérművelet végéig zárolódik. A Qt-teszt külön
ellenőrzi a busy és szabad állapotot. A célváltási és ablakbezárási,
már meglévő művelet-/dirty-védelmek megmaradtak.

### Ellenőrzési rés — A natív szerkesztő nem kapott explicit ASAN-t

A `CatalogEditorTool` cél nem linkelte az `auriga_sanitizers` interfészt,
és önálló target buildnél nem kapta meg a runtime DLL másolását. Ezek
bekerültek a CMake-be. A generált projektben az instrumentáció ténylegesen
ellenőrzött; a teljes szerkesztői SQL/Qt-teszt az ASAN exe-vel is lefutott.

## Folytatás: prototípus másolása

Elkészült a **Másolat új tárgyként** művelet. A mentett forrástárgy adataiból
új, még nem mentett dokumentum lesz, új kategórián belüli azonosítóval és
egyedi, legfeljebb 96 karakteres tartalomkóddal. Névkulcs, asset, fejlesztés
és bónuszok átvehetők; példányállapotot nem másol.

Az új dokumentum egyszerű profiljai mindig deduplikálási/másolási útvonalra
kerülnek. Így a másolat megváltoztatott használati, támadási, megjelenési vagy
ellenállási profilja nem írhatja át a forrástárgy kizárólagosan használt
profilját sem. A modellteszt teljes forrásdokumentum-egyezést mér a másolat
módosítása után. Qt-teszt ellenőrzi az új draft kulcsát és elvethetőségét.

## Friss eredmények

| Ellenőrzés | Eredmény |
|---|---:|
| ServerTests build, teljes normál CTest | 102/102 |
| Szerkesztő: modell + Qt + natív validátor + saját MariaDB | 23/23 |
| Célzott ASAN domain/dekóder CTest | 13/13 |
| Szerkesztő az ASAN-instrumentált natív eszközzel, saját MariaDB-vel | 23/23 |
| Teljes szerverkezelő GUI eldobható csomagmásolaton | 14/14 |
| Becsomagolt v7 natív validátor és frozen manager worker `status` | sikeres |

A pontos forráshasheket és naplóhivatkozásokat a jelentés melletti
`2026-10-28_claude_review.evidence.json` rögzíti. A Python és a MariaDB
folyamata nem ASAN-instrumentált; az állítás a natív C++ modulokra vonatkozik.

A friss GUI a `D:/AurigaGlobal/Server2-Local/manager` alá települt a meglévő
manifest-ellenőrzött frissítővel. Az indító:
`D:/AurigaGlobal/Server2-Local/Szerverkezelo.cmd`. A frissítési napló:
`build/claude-review-manager-install.log`; a lecserélt kezelőfájlok mentése a
csomag `run/manager-updates` mappájában van. A megnyitott
`C:/Users/ixtre/Desktop/ServerManager (1)` külön másolat: annak ablakát nem
állítottam le és fájljait nem cseréltem. Az új funkciókhoz a csomag indítója
használható.

Az offscreen Qt-megjelenítési próba nyolc mesterséges prototípust csak
memóriában tartott; képe `build/claude-review-item-editor.png`. Nem töltött
teszttárgyat a meglévő `protos` adatbázisba.

## Határ

Nem készült új timed-effect kategória ebben a körben: az átvett szerkesztő és
a nyolc jelenlegi kategória megbízható ellenőrzését és használhatóságát
zártam le. A következő tartalmi port az IC-014 időzített hatásainak tételes
APPLY/stat-leképezése, majd a típusos hatásprofilok. Az instance/runtime és
Unity-csere az IC-012 további kapuja; a szerkesztő módosításai addig nem
aktiválnak új tárgymodellt a futó GameServerben.

A tesztek saját ideiglenes adatbázisokban futottak; a meglévő tesztadatokhoz
és a távoli géphez nem készült írás.
