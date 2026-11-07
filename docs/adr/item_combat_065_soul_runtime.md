# ADR IC-065 — Önálló lélektárgyak szerver–Unity runtime

2026-11-07. C30, az [IC-025](item_combat_025_independent_soul_items.md)
domainjének bekötése. Szerver: `20261103`; Unity:
`/main/unity63-integration-20261103`, Editor `6000.3.27f1`.

## Tartós állapot és műveletek

A meglévő 14-es kategória két működése marad: aktiválható vörös/kék
lélek és fejlesztőtekercs. A katalógus továbbra is v28; a futó tárgyút
nem használ vnumot, socketcsomagolást vagy DragonSoul-szabályokat.
A gyári core a saját szintjének explicit kezdeti állapotát kapja.

A SOL1 állapot 21 bájt: magic/verzió 4, töltés u32, ütéskeret u32,
részintervallum u64 és aktív jelző u8, little-endian sorrendben.
Csak core hordozhatja. Betöltéskor a saját szintjének összes korlátja,
a blob és a külön SQL-csatorna/aktív mezők egyezése is kötelező.
Hibás vagy hiányzó állapot elutasított, nincs ingyenes újragyártás.
A korábbi kategóriák új mezői NULL/0 értékűek, hash-ük változatlan.

Aktiváláshoz legalább egy ciklus töltése kell. Csatornánként egy lélek
lehet aktív; vörös és kék együtt használható. A generált SQL-oszlop
egyedi owner/csatorna indexe a hiányos hívói snapshot ellen is véd.
Az aktív core a generikus tárgyfelhasználás és áthelyezés számára zárolt.
Kikapcsolás nem tölti vissza az ütéskeretet. Az aktív szándék újrabelépés
után megmarad; halálkor a szerver kikapcsolja.

A töltés élő, online karakter saját monoton órájából gyűlik, inaktív
core esetén is. Egy egység egy katalógusintervallum; a részidő megmarad.
Teljes töltésen nincs előre felhalmozott idő, a töltés nem ad új ütést.
Öt másodperces vagy egész töltési egységnél esedékes checkpoint,
szolgáltatás, találat és kijelentkezés rendez időt. Az új login megvárja
az előző tartozását. Offline/halott idő nem termel töltést.
Sikeres szintlépés új óraszakaszt indít a következő szint kezdőértékeivel.
A checkpointok közötti processzcrash és a további tároló-/handoff-utak
közös kiadási korlátai megmaradnak.

Fejlesztés csak inaktív lélekre, létező következő szinttel és saját
Soul-tekercsből indulhat. Mindkét kimenet egy tekercset fogyaszt.
Kudarc megőrzi a lélek teljes állapotát; siker ugyanazon instance és
ItemKey következő szintjére lép, explicit kezdő töltéssel/ütéssel.
0/100% esetén nincs esélydobás, más esetben szerveres kriptográfiai
véletlen 1..10000 a döntés. Megerősítés előtt determinisztikus preflight
vizsgálja a feltételeket. A közben történt tiszta óratöltés átvezethető
a preview-n; ütés-, szint- vagy más tárgyváltozás új preview-t kér.

## Elfogadott találat

A vörös a normál közelharci/távolsági, a kék a közvetlen képességsebzés
csatornája. Játékos→szörny/metinkő PvE, pozitív elfogadott sebzés és
átváltozás nélküli támadó kell. PvP, érvénytelen cél, nulla vagy korábban
elutasított sebzés nem készít Soul-debitet. Poison/special/reflect nem
válik képességtalálattá. A typed csatorna mellett a régi Soul-számítás
nem számol másodszor.

A bekötés a közös `CombatSystem::Damage` végső elfogadási pontján van,
a korábbi visszautasítások és sebzésmódosítók után. A szakasz a teljes
ciklusok számából adódik, az eredmény egzakt
`floor(inputDamage * multiplierBp / 10000)`, overflow-védelemmel.
Egy elfogadott találat egy ütést fogyaszt; a ciklus végén töltés vonódik
le és az ütéskeret újraindul. Teljes ciklus hiányában a core kikapcsol,
azonos példányként megmarad és újratölthető.

A szerver lefoglalja a támadót és a cél teljes generációs ECS-handle-jét.
Függő Soul-találat közben új Damage nem fut rájuk, a karakterkezelő
destrukciója a befejezésig vár. Nincs blokkoló SQL-várás a játékszálon.
A végső HP-/damage-packet/quest/ledger/death folytatás az itemíró sikeres
visszajelzéséből, egyszer fut le. A normál szinkron út és visszatérési
érték Soul nélküli támadásoknál megmarad; Soul-találat késleltetett.

A 84 bájtos SLR1 bizonylat az actiont, két tárgyazonosítót, eltelt időt,
dobást, szereplői tényeket, csatornát, damage-eventet, cél-incarnációt,
bemeneti és kimeneti sebzést és szorzót tárolja. A writer újratervezi
a pontos eredményt és a levonást. A `item_soul_strike_event` owner/event
elsődleges kulcsa ugyanazt a találatot új operation ID-val is elutasítja;
azonos operation ID a meglévő receiptet adja vissza. Az event, a receipt,
a core állapota és az esetleges tekercsfogyás egy SQL-tranzakció.
CommitUnknown esetén a meglévő receipt-egyeztetés rendez, újradobás nincs.

Ez a tartós item-debit és az ECS-befejezés sorrendjét biztosítja.
A világ NPC-HP-ja nem lesz SQL-ben tárolt combat outbox: szervercrash
esetén a világ újraépül. A végső elfogadási pont előtti meglévő legacy
harci procok nem kaptak általános tranzakciós újraírást.

## Protokoll és Unity

- **CG213:** 28 bájt + sequence; preview/confirm/close, operation
  activate/deactivate/refine (1/2/4), nonce, source/target ID, nulla
  választó. Tekercsnél a source a tekercs, a target a core. Kliensidő,
  damage vagy RNG nem küldhető.
- **GC219 v1:** legalább 35 bájt, azonosítás és státusz. A fogyasztó
  fejlesztési kudarc külön `AttemptFailed=8`, a DB-hiba `Failed=7`.
  Nonce-ismétlés nem fogyaszt új tekercset és nem inicializál újra core-t.
- **GC225 v15:** a korábbi sidecarok után Soul-kind, csatorna, esély,
  SOL1 állapot és a saját szinttábla. Korábbi verziók változatlanul élnek.
- **GC227 v3:** Soul action 131072 a meglévő u32 mezőben.

A Unity külön **Lélek** gombja a saját core-t vagy tekercset nyitja.
Aktív állapot, töltés, megmaradt ütés, részidő, ciklusár, szorzó és szint
látható. Tekercsnél céllista, pontos esély, kudarc- és sikerüzenet van.
Inventoryt kizárólag szerveres GC225 változtat. A meglévő Qt-s Soul-panel
és a natív próbapad ugyanazt a gyári kezdőállapotot használja; a kezelő
csomagja a 018-as migrációt is tartalmazza.

Eredmények és határok: [C30 jelentés](../reports/item_combat/2026-11-07_soul_runtime.md).
