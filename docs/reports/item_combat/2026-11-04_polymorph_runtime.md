# C24 — Átváltoztató gömbök és gyakorlókönyvek

2026-11-04. Szerződés: [IC-052](../../adr/item_combat_052_polymorph_runtime.md).

## Elkészült

Szerveres előnézet, megerősített használat, példányhoz kötött alak,
gyakorlás és könyvfejlesztés, megszüntetés, tartós UTC-lejárat, valódi race és
harci statok, együtt tesztelve a Unity-felülettel. A példánypayload és az aktív
hatás ugyanabban a tranzakciós rendszerben mentődik, mint a többi typed művelet.
A külön típusos leltár használja; a normál legacy leltár átállítása vár.

## Ténylegesen lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer / Database / LoginServer | sikeres |
| Célzott item-CTest | 21/21 csoport |
| PolymorphRuntimeTests | 93 ellenőrzés |
| PolymorphItemTests | 62 ellenőrzés |
| Külön saját MariaDB, ItemPersistenceSqlTests | 1770 ellenőrzés |
| ItemBenchToolTests, külön saját MariaDB | sikeres |
| PointCalculationTests, végső statösszetétel | 7398 ellenőrzés |
| CombatStateTests, valódi melee képlet | 2069 ellenőrzés |
| GmCommandTests, végső build | 31564 ellenőrzés |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új golden/kodek/TCP split/dispatcher/UI validátor | 11255 ellenőrzés |
| Korábbi C4–C23 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL/UI, végső build | 43 ellenőrzés |
| Kanonikus forrásokból ellenőrzött snapshot | 5490 fájl, 1736 byteazonos C# |
| Korábbi helyi tárgyak | mind a 102 teljes állapot, revízió és hash egyezik |
| C22 learning és karakterfejlődés | változatlan; teszt HP/SP visszaállítva |
| Helyi szerkesztő build és telepített binárisok | hash-ellenőrzött |
| Érintett Python-szintaxis, git diff check | sikeres |

A domain/SQL próbák ellenőrzik a forrás-CAS hibájának teljes rollbackjét,
a receipt ismétlését, a payload hashének sérülését, a sikertelen/sikeres
gyakorlást, a fejlesztést, az állapot törlését és a lejárt hatás újrabetöltését.
Halál és kényszerített törlés domainellenőrzést kapott; élő halálpróba nem futott.

A stattesztek törtszázalékos moberőt, attack/defense/movement bónuszt,
felszerelési százalékokkal való összetételt és ismételt újraszámolást fednek.
A tényleges melee képletben a típusos íj átváltozáskor nem tiltja a mob ütését,
és visszaváltozáskor visszaáll az íj közelharci korlátozása.

Az élő próba az **Átváltoztat** gombot és a külön megerősítést használta.
Valódi felszerelt tesztfegyver mellett változott a race és támadás; a könyv
mozgásbónusza a fegyver százaléka után pontosan egy pont volt. Sikeres a fix és
példányhoz kötött gömb, a replayvédelem, a már aktív alak tiltása, a garantált
könyvkudarc, a gyakorlás, a fejlesztés, a minimumszint és ismeretlen mob tiltása.
A bónusz az alak előtt lejárt; cancel után visszaálltak az értékek. Újrabelépés
ugyanazt az UTC-határidőt és könyvállapotot adta, offline lejárat után az alak
megszűnt. A végén a fegyver levétele pontosan visszaadta a kiinduló statokat.

A helyi adatbázis 101-es protója kő, ezért a fixture a tényleges mobtáblából
kiválasztott 103-as szörnyet használta. A production szörnytípus-ellenőrzés
változatlanul szigorú maradt.

## Helyi csomag

Csak az igazolt `codexitem` próbakarakter kapott új tárgyakat. Teljes
player/protos SQL-mentés, binárisok, manifest és karakter/sidecar-pillanatkép
készült a csomag `run/deployments/polymorph-*` könyvtáraiban. A korábbi 102
tárgy változatlan; a próba után 116 élő itemállapot van. A gömbpróbákból öt-öt
elérhető; a sikeres könyv 2-es fokozatú, két hátralévő gyakorlással, a kudarcot
adó könyv 1-es fokozatú, egy gyakorlással. Az új tesztfegyver a leltárban van.
A C22 learning-checkpoint és a karakterfejlődés byteazonos maradt.

A helyi Game/Database/Login és a natív segédek együtt települtek. Az új manager
teljes korábbi könyvtára külön mentésbe került; tartalmazza a 008-as migrációt
és a friss natív próbapadot. A helyi stack öt saját folyamata újra fut.
A felhasználó kanonikus Unity Editora nyitva maradt; külön forrássnapshotban
futott a fordítás és a játékfolyamat.

Unity: **cs63** a `/main/codex-mission-engine` ágon, kizárólag a 26 saját
fájllal. A korábbi jelenet-, kapcsolati asset-, Packages- és ProjectSettings-
módosítások kimaradtak; a ProjectSettings.asset ellenőrzött hash-e változatlan.

## Nyitott korlátok

A hiányzó SpeedTree/VFX/Crest világassetek miatt ez az item-folyamat
fordítása és tesztje, nem teljes világjelenet- vagy playerbuild. A fagyasztott
manager elkészült és telepített hash-ei ellenőrzöttek; külön C24 Qt UI-próba
nem futott. FreeBSD, több core közötti handoff, teljes hibainjektálás/stressz
és éles migráció nem futott. A legacy affectek és tárgyak teljes konverziója,
a normál leltár átállítása, a C19 HP/SP crash-atomikusság és az F7 kapuk nyitottak.
