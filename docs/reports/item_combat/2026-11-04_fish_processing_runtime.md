# C25a — Élő hal használata és halak sütése

2026-11-04. Szerződés: [IC-053](../../adr/item_combat_053_fish_processing_runtime.md).

## Elkészült

Élő hal használata saját lootprofillal, a teljes döglötthal-halom sütése közeli
valódi tábortűznél, szerveres előnézet és külön Unity-megerősítés. Fogyasztás,
minden jutalom és receipt közösen mentődik. A halak/csalik perzisztálhatók;
csali és sütési cél nélküli hal nem kínál feldolgozási műveletet.

Ez a C25 első kész játékfolyamata. A gyűjtőeszközök mentése, felszerelése,
gyakorlása/fejlesztése, a csali alkalmazása és maga a fogás/gyűjtés még hátravan.
A normál legacy leltár átállítása a felhasználó döntése szerint továbbra is vár.

## Ténylegesen lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer / Database / LoginServer, végső GameServer | sikeres |
| Célzott item-CTest, a gathering domainnel együtt | 23/23 csoport |
| FishProcessingTests | 33 célzott ellenőrzés |
| Külön saját MariaDB, ItemPersistenceSqlTests | 1804 ellenőrzés |
| ItemBenchToolTests, külön saját MariaDB | sikeres |
| GmCommandTests, újrafordítva | 31564 ellenőrzés |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új golden/kodek/csonkítás/TCP split/dispatcher/UI validátor | 8884 ellenőrzés |
| Korábbi C4–C24 Unity-validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL/UI | 32 ellenőrzés |
| Ellenőrzött forrássnapshot | 5504 fájl, 1740 byteazonos C# |
| Korábbi helyi tárgyak | mind a 116 teljes állapot, revízió és hash egyezik |
| C22 learning, karakterfejlődés és hely | változatlan; teszt HP/SP visszaállítva |
| Helyi manager build és telepített binárisok | hash-ellenőrzött |
| Érintett Python-szintaxis, git diff check | sikeres |

A natív próbák lefedik az összes lehetséges jutalom előzetes helyfoglalását,
a hiányzó ID-foglalás és nulla operation ID elutasítását, az üres független
eredményt, a teljes sütési mennyiséget, a tűz hiányát, a jogosultságokat és
a korlátozott kombinációszámot. SQL-ben a hibás forrásrevízió a teljes jutalmat
visszagörgeti; a receipt ismétlése egyszeri fogyasztást és jutalmat hagy.
A hal és a sült tárgy új owner-sessionben is pontosan visszatöltődik.

Az élő próba az új inventorygombot és a külön megerősítést használta.
A közelben álló valódi tábortűz előnézetet engedett; az eltávolodás után a
megerősítés elutasítása a teljes halmot megtartotta. Távolról új előnézet sem
engedett sütést. Visszatérés után az élő hal egy darabot fogyasztott és három
döglött halat adott. A replay nem adott újabb jutalmat. Az előnézet után
áthelyezett forrás elutasítást kapott fogyasztás nélkül.

Az utolsó élő hal eltűnt, az eredményablak megmaradt. A szinttiltott hal,
a csali és a sütési cél nélküli hal változatlan maradt. A teljes, 23 darabos
halom sütése 23 sült tárgyat eredményezett, 4-es célhalom-korláttal több halomra
osztva. Újrabelépés után az eredménytárgyak azonosítója, darabszáma és helye
egyezett, az elfogyott források hiányoztak. Régi session-megerősítés nem
indíthatta újra a műveletet.

## Helyi tesztkészlet

Csak az igazolt `codexitem` próbakarakter kapott új tárgyakat. A teljes
player/protos SQL, korábbi binárisok, manifest, NPC-fájl, karakter és learning
mentése a csomag `run/deployments/fish-processing-*` könyvtárában van.
A régi manager teljes könyvtára külön mentésbe került. A katalógus v26 maradt;
új SQL-migráció nem szükséges.

A próba után 129 élő itemállapot van, a korábbi 116 minden mezője/hash-e
változatlan. A próbapad visszaadott két normál élő halat, egy utolsódarab-próbát
és 17 döglött halat; a 23 sült tárgy is megmaradt. A tűz egy külön helyi
NPC-fixture a próbakarakter kiindulási pontjánál. A map összes korábbi NPC-sora
megmaradt; a csomag manifestje az új NPC-fájl hash-ét is tartalmazza.
A helyi stack öt saját folyamata újra fut.

Unity: **cs64** a `/main/codex-mission-engine` ágon, kizárólag a 21 saját
fájllal. A korábbi jelenet-, kapcsolati asset-, Packages- és ProjectSettings-
módosítások kimaradtak; a ProjectSettings.asset ellenőrzött hash-e változatlan.
Az eredeti Editor nyitva maradt, az ellenőrzések külön forrássnapshotban futottak.

## Korlátok

A hiányzó SpeedTree/VFX/Crest világassetek miatt a forrássnapshot itemfolyamata
fordult és futott; teljes világjelenet-/playerbuild nem készült. Arena/PvP
tiltások fordultak, de külön élő arena/PvP-próba nem futott. A fagyasztott
manager natív segédei frissültek, külön C25a Qt UI-próba nem futott.
FreeBSD, több core közötti handoff, teljes hibainjektálás/stressz és éles
migráció nem futott. A teljes adatkonverzió, a legacy leltár átállítása,
a C19 HP/SP crash-atomikusság és az F7 kiadási kapuk nyitottak.
