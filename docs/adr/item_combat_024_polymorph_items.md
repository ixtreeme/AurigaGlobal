# ADR IC-024 — Átváltoztató gömbök és gyakorlókönyvek

2026-10-29. IC-012 F2, helyi fejlesztés. Kezdő HEAD:
`04ee1b4ed4f39afa228abae4cca218803121999c`, `codex/mission-engine`.

## Típusos tartalom és példányállapot

A 13-as kategória két működése az elfogyó gömb és a megmaradó gyakorlókönyv.
Az `item_polymorph` külön `polymorph_profile`-ra hivatkozik. A profilban 41,
0..40 készségszinthez tartozó explicit szabály van. Könyvnél négy mesterfok
adja az átváltozás és a formabónusz időtartamát. A runtime nem értelmez régi
itemazonosítót, általános értéktömböt, socketet vagy azonosító-tartományt.

A `polymorph_form` tartós formakódot, formán belüli azonosítót, mob-sablon
azonosítót és könyvbónusz-típust tárol. A mob-sablon a mob névterébe tartozik,
nem régi itemkulcs. A már létező forma jelentése és azonosítója az adminisztratív
íróval nem módosítható és nem törölhető. Új forma hozzáadható; használatlan
formát sem metszünk ki, mert egy játékospéldány még hivatkozhat rá.

A prototípus vagy rögzített formát használ, vagy a konkrét példány típusos
`boundForm` állapotát. A kettő együtt érvénytelen. A könyvfokozat és a hátralévő
gyakorlásszám szintén példányállapot. Gömbnél ezek nullák, könyvnél a halomméret
1. A könyvfokozat maximuma profiladat, 0..255; nem kell fokozatonként külön
prototípus. Nem az általános felszerelés-`refinement_level` mezőt használja.

A formákra a natív validátor egyedi pozitív ID-t/kódot, legalább 8-as
mob-sablonazonosítót és ismert bónusztípust követel. A tényleges mob létezését,
szintjét, átváltozásra engedélyezett állapotát és a kliensassetet a későbbi
owner/cutover ellenőrzi. Nem másolunk második mobadatbázist az itemkatalógusba.

## Mellékhatásmentes használati terv

A `PlanPolymorph` birtokolt tervet készít, SQL-, RNG-, Lua-, hálózati és registry-
hozzáférés nélkül. Ellenőrzi a szerveres admissiont, operation ID-t, ruleset
revisiont, tulajdonost, instance/revisiont, zárolást/cserét/viselést, kategóriát,
halmot, karakter- és készségszinteket, osztályt/nemet és a használati policyt.
Ismeretlen vagy hiányzó formából nem készül fogyasztási terv.

Használatkor tiltott a lovaglás, a már aktív átváltozás és az azt tiltó párbaj.
A szerveres mob-snapshotnak egyeznie kell a formával, nem nulla mobkatalógus-
revisionnel. Gömbnél a mob szintje szigorúan kisebb legyen, mint:

`karakterszint + max(0, engedmény − floor(karakterszint × számláló / nevező))`.

A sikeres gömb egy darabot fogyaszt. A könyv megmarad; sikeres használatakor
egy hátralévő gyakorlást veszít, kizárólag ha az érték pozitív. Esélye:

`clamp(karakterszint − mobszint + alapesély + készségszint + könyvfokozat, 0, 100)%`.

0% és 100% esetén a rögzített dobás 0; máskor 1..10000. A sikertelen használat
nem készít effektet, és nem fogyaszt vagy módosít tárgyállapotot. A matematikai
műveletek egészek; a UTC-határidő nem haladhatja meg az `INT64_MAX` értéket.

Könyvfejlesztéshez kész gyakorlás, könyvműködés és a beállított maximum alatti
fokozat kell. Az új fokozat az előző +1, az új gyakorlásszám pedig az
**előző fokozat × gyakorlási szorzó**, a vizsgált forrás képlete szerint.
A 0→1 átmenet ezért 0 gyakorlást állít be, ha ezt a képletet választja a tartalom.
Ez nem javítandó hallgatólagos +1 eltolással. A 10→11 lépés tesztelt.

Az effekt külön hordozza a mob statjainak arányát, az átváltozás és bónusz
határidejét, valamint a bónusz fajtáját, egységét és mennyiségét. A támadás/
védelem bázispont, a mozgás **additív sebességpont**. Az utóbbi nem százalék.
A bónusz nélküli terv nem hordoz kísértethatáridőt vagy mennyiséget.

Ez a modul még nem perzisztencia- vagy effektíró. Az F3/F4 owner újraellenőrzi
a sessiont, item- és mobkatalógus-pineket, pontos revisiont és karakterállapotot;
operation receipt + fogyasztás/könyvhaladás + effekt egyetlen atomikus művelet.
CommitUnknown után tartós operation-ID egyeztetés szükséges újrapróbálás előtt.
Effektet és Mission/Mission Pass eseményt csak sikeres commit után alkalmazhat.
A halál, kijelentkezés, lejárat, stat-újraszámítás és kliensszinkron még F4/F5.

A jelenlegi általános loot-bejegyzés nem hordoz formakötést vagy kezdeti
könyvállapotot. Emiatt a katalógus elutasítja a példányhoz kötött gömböt és
bármely gyakorlókönyvet ilyen jutalomként. Rögzített formájú gömb adható.
A későbbi típusos grantot kell kiegészíteni; nem adunk ki használhatatlan
tárgyat hiányos állapottal. A közönséges smith-recept sem alakíthat át
polimorf tárgyat, mert elveszíthetné a kötést és a könyvhaladást.

## Szerkesztő, séma és mentés

A Qt-panel külön mutatja a formát/használatot és a készség-/időszabályokat.
Gömb- és könyvmódban az alkalmazható mezők jelennek meg. Könyvnél a halomméret
inaktív és 1. Új forma a panelből hozzáadható, jelentése később nem írható át.
Közösen használt profil szerkesztése copy-on-write; a másolt tárgy változása
nem módosítja az eredeti profilját. A mentést a meglévő natív helper és az
S_SQL prepared-tranzakció végzi, katalógusrevision/számláló zárolással.
Hibás részírásnál a profil, szabálysorok, tárgy és ID-kiosztás együtt rollbackel.

Új, még nem kiadott v17 séma: `auriga.item_catalog.v17`, kanonikus tagek 37..39.
Az öt új SQL-tábla a `023_polymorph_items.sql` fájlban van; teljes friss lánc
000..023. A már telepített v7-et ez a draft nem frissíti helyben. A meglevő
felhasználói katalógus új, leállított sandboxba átemelve natívan visszaolvasható,
kulcsai, cellái és számlálói megmaradnak. A játékba telepítés továbbra is külön
F6/F7 munkafázis, a teljes konverzió és a fogyasztók átállítása után.

## Forrásaudit: eltérések és publikálási korlátok

Közvetlenül vizsgált szimbólumok:

- `ecs/systems/ItemSystem_LegacyBridge.cpp::ItemProcess_Polymorph`;
- `entity/polymorph.cpp::CPolymorphUtils` és könyvműveletei;
- `ecs/systems/AffectSystem.cpp::GetPolymorphPower/SetPolymorph`;
- `ecs/systems/PointSystem.cpp::GetPolymorphPoint`;
- `quest/questlua_pc.cpp::pc_give_polymorph_book/pc_upgrade_polymorph_book`;
- `core/constants.cpp::aiPolymorphPowerByLevel`.

A mentésben hat prototípus van: 50322 gyakorlókönyv, valamint 70104..70107 és
71093 gömb. Formájuk régen socket0, a könyv gyakorlása socket1 és fokozata
socket2. A négy vizsgált instance/award táblában ezekhez nincs sor. Ez csak
a rögzített mentésre és a kísérő auditban felsorolt táblákra állítás.

A régi gömb üres/ismeretlen forma esetén is elfogyott. Az új terv ezt elutasítja,
fogyasztás nélkül. Hibás migrációs bemenetet megőrzünk/karanténba helyezünk.
A régi nyers `BookUpgrade` nem ellenőrizte a típust, kész gyakorlást és maximumot;
az új domain ezeket megköveteli. A `GiveBook` LevelLimit paramétere a vizsgált
kódban nincs alkalmazva; nem találunk ki hozzá historikus minimumszintet.

A könyv 10/15/20/25 **másodpercig** tart; a perc-szorzás kommentelt.
A bónusz egy másodperccel korábban jár le. A forrás 101 és 1901 formára
mozgáspont-bónuszt regisztrál; az attack/defense regisztráció üres volt.
A gömb készségfüggő ideje 5/10/35 perc, támadásbónusza 200%/300%.
Ezek szerkeszthető forrássablonok, nem jóváhagyott új balance.

Az `aiPolymorphPowerByLevel` csak **40 explicit inicializálót** tartalmaz,
miközben a lekérdezés a 40-es indexet is eléri. Az utolsó index emiatt 0.
A szerkesztő a teljes 41 sort kiteszi, ezt az eltérést jelzi, és nem javítja
észrevétlenül 100%-ra. A publikált érték véglegesítése és az esetleges
`poly` eseményflag vizsgálata a teljes tartalomkonverzió kiadási feltétele.

A hat sor auditált, de ettől még egyik sem teljesen konvertált/publikálható
tartalom. Policyk, assetek, konkrét formakötések, kezdeti könyvállapot,
grantok, quest- és gazdasági referenciák továbbra is F3..F6 feladatok.
A régi runtime nem kap új item-vnum adaptert, és nem lett visszaállítva DS,
offline shop vagy régi BattlePass.
