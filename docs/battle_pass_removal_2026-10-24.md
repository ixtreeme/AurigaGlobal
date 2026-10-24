# A régi BattlePass kódjának kivezetése — 2026-10-24

A felhasználó az audit után jóváhagyta a régi BattlePass szerver- és
klienskódjának, illetve kizárólagos felületének eltávolítását. Ez az audit
2. megvalósítási fázisa. **Az új történeti Mission Engine még nincs
megvalósítva. A különálló, szezonális Auriga Mission Pass megmarad.**

Munkaágak:

- Szerver: `D:/AurigaGlobal/LiveWork/AurigaGlobal`, `codex/mission-engine`,
  kiindulás: `9cda87fd`.
- Unity: `D:/Auriga-Unity/SRC/AurigaUnity`,
  `/main/codex-mission-engine`, kiindulás: changeset 29.

## Eltávolított működés

- A `GameServer/combat/battle_pass.{cpp,h}` és `battle_pass_input.cpp`,
  a pont-/affect-/ECS-állapot, időzítők, bejelentkezési automatikus aktiválás,
  küldetéskövetés és régi jutalmazó/rangsor-kezelés.
- Az integrációk a harc, tárgykezelés, bolt, horgászat, mozgás,
  dungeonok és quest bindingek közös fájljaiból. A `70611` aktiválási és
  boost-használati ágai is megszűntek; nincs helyettük kompatibilitási adapter.
- A régi CG 40, GC 160/161/162, Game→DB 82/83/84 és DB→Game 78/79
  üzenetdeklarációk és handlerek. A megmaradó hálózati azonosítókat nem
  számoztuk át; a régi azonosítókat nem osztottuk ki más funkciónak.
- A régi BattlePass SQL-lekérdezései, mentése, rangsorbetöltése és
  konfigurációs mezője. Az SQL-séma és az adatbázisok változatlanok.
- A Unity régi panelje, lokalizációs segédje, codec-/session-/runtime-útjai,
  F8 megnyitása, inventory- és mobilbelépési pontjai.
- A kizárólagos desktop- és mobilprefabok, layoutok, képek,
  `*_battlepass.json` fordítások és önálló Wiki-oldalak. A közös
  inventory-prefabokból a teljes gombhierarchia és az elárvult responsive
  bejegyzés is kikerült; a JSON szülőindexei újra lettek képezve.
- A régi Python-kliens megfelelő panelje, layoutjai, callbackjei és
  feature flag használatai; a C++ Launcher exportjai és csomagútjai.
- A csomagoló BattlePass-konfiguráció-előállítása és a szerkesztőből a már
  nem létező `pc.open_battle_pass_ranking` ajánlása.

A szerverágban 16 kizárólagos fájl törlődött, 94 közös fájl változott.
A Unityben 252 fájl törlődött, a hozzájuk tartozó metaállományokkal együtt;
40 közös fájl változott. A Plastic a könyvtárak törlését összesítve is
mutathatja. A pontos fájllista és az előtte/utána SHA-256 értékek:
[módosítási jegyzék](battle_pass_removal_2026-10-24_manifest.json).

## Megőrzött tulajdon és közös infrastruktúra

A szezonális Mission Pass 34 kizárólagos forrás-, teszt-, konfiguráció-,
fordítás- és grafikai fájlja SHA-256 szerint változatlan. A
`/missionpass` és `amp_*` protokoll, valamint a saját felülete megmaradt.

A nyolc korábbi helyi Unity-módosítás szintén bájtra azonos maradt,
beleértve a jelenetet és a projektbeállításokat. A korábbi mobmozgás-javítás
forrásait nem cseréltük le.

A közös inventory, tárgyfogyasztás, loot, pontszámok, horgászat, dungeon-
előrehaladás, bolt és régi NPC-questfuttató megmaradt. A PvP-halálértesítés
korábban a BattlePass kapcsolója alá volt téve, de nem a BattlePass
tulajdona: önálló kódként megőriztük. A Szerencsekerék 70610-es jegye és
fogyasztása is megmaradt; a téves BattlePass-hibaüzenete javítva lett.

Dokumentált, inaktív szöveges kivételek:

- A már kiadott tárgyak katalógusbejegyzései, nevei, ikonjai és leírásai.
  A tárgyakat nem vontuk vissza a játékosoktól.
- A közös, számozott szerverüzenet-fordítások és a közös kosztüm-Wiki
  történeti jutalomleírásai. Az üzenetazonosítókat nem számozzuk át.
- A megtartott Mission Pass saját elkülönítést magyarázó megjegyzései,
  illetve a `MissionPassRulesTests` érvénytelen `"battlepass"` eseményét
  elutasító tesztesete.
- Az `AIStateTests` „battle pass” szövege a harci állapotgép egy
  futtatási körét jelenti; nem a törölt szolgáltatásra hivatkozik.
- Korábbi auditok, migrációs SQL-ek és történeti dokumentáció.

Nincs ezek mögött régi BattlePass panel, regisztrált csomagkezelő,
aktiválási API vagy jutalmazó háttérrendszer.

## Ellenőrzések

Windows/MSVC, `RelWithDebInfo` alatt sikeresen elkészült:

- `GameServer`, `Database`, `LoginServer`;
- a Unity 2022.3.62f3 435 runtime C# forrásának fordítása;
- a Unity 411 Editor C# forrásának fordítása az új runtime ellen.

A C# fordítás a projekt tényleges Unity/Bee referenciáit használta, külön
kimeneti könyvtárba. Ez **nem önálló Unity Player-build**.

**20/20 célzott CTest-csoport sikeres:** `server_config`,
`server_package_tools`, `server_manager_tools`, `item_attributes`,
`switchbot_lifecycle`, `quest_runtime`, `lua_sources`, `quest_data`,
`login_db_link`, `player_load`, `player_create`, `database_time`,
`async_sql_prepared`, `dungeon_lifecycle`, `shop_lifecycle`, `item_runtime`,
`spatial_lifecycle`, `combat_state`, `affect_lifecycle`, `mission_pass_rules`.

A megtartott tárgykezelési fixture a gyűjtögetési rendszerek új refine
belépési pontjaihoz is igazítva lett. A dublőr csak a tesztelt, nem
gyűjtögető eszköz átadását engedi tovább; tényleges axe/shears refine
hívásra továbbra is hibát jelez, nem rejti el a hiányzó szolgáltatást.

Külön, headless Unity-projektben sikeres:

- a valódi `Metin2ProtocolCodecValidator`, benne a nyugdíjazott GC
  160/161/162 azonosítók elutasításával;
- a változatlan szezonális Mission Pass valódi transporttesztje:
  túlméretes átvitel elutasítása, chunkos snapshot, claim/claimall parancsok;
- completion értesítés, deduplikálás, napi reset, logout és az inputot
  nem blokkoló megjelenítés ellenőrzése;
- a törölt panel és megnyitási/küldési API-k hiánya a runtime assemblyben.

Statikus asset-ellenőrzések: a 4 módosított prefab lokális fileID-i
érvényesek, a layoutok szülőindexei érvényesek, 128 eltávolított
asset-GUID-ra és 60 eltávolított beágyazott sprite-ra nem maradt hivatkozás
a megtartott `Assets` fájlokban. A régi UI-t előállító layoutok és a
mobilos forrásprefabok is ki lettek vezetve.

A módosított Python 2 kliensforrások nyelvtani ellenőrzése sikeres.
A Git whitespace-ellenőrzése tiszta. A fordításokban meglévő, nem végzetes
figyelmeztetések maradtak.

Naplók az ignorált `build/battlepass-removal/` könyvtárban:
`native-verified-build.log`, `game-final-build.log`,
`native-tests-final.log`, `unity-runtime/compile-final.log`,
`unity-runtime/editor-compile-final.log`, `unity-headless-final.log`,
`verification.json`. A pontos forrásmentés:
`build/battlepass-removal/backups/manifest.json` és a mellette levő
`server/`, `unity/` könyvtárak.

Nem futott teljes Unity-belépési/játékmenet-próba, önálló Player-build,
FreeBSD-fordítás vagy új MariaDB-integrációs kör. A felsorolt ellenőrzések
nem a még meg nem írt Mission Engine működését igazolják.

## Adatok, visszaállítás és későbbi telepítés

Ebben a fázisban **nem történt DB DROP, séma-/adatmódosítás, helyi
szervercsomag-csere vagy távoli telepítés**. A működő példányok és a
`db/schema.sql` változatlanok. Nem készült új DB-mentés vagy restore-próba;
ezek a későbbi adatátvezetés és takarítás kötelező feltételei.

A források visszanyerhetők a kiinduló Git-commitból, a Unity changeset 29-ből
és a felsorolt, ellenőrzőösszeges mentésből. Egy közös fájl visszaállításakor
előbb össze kell vetni az azóta érkezett módosításokat: a teljes workspace
felülírása nem része a visszaállításnak. A korábbi nyolc helyi Unity-fájl
saját mentéssel rendelkezik.

A `TPlayerTable` régi mezőjének eltávolítása módosítja a belső Game/DB
bináris szerződést. Később az összetartozó szolgáltatásokat ugyanabból a
forrásverzióból kell fordítani és összehangoltan telepíteni. Új és régi
GameServer/Database binárisok keverése nem támogatott.

A 9 jövőbeli lejárat, a régi affectek, final_reward flag-ek és a
BattlePass-táblák rendezése az audit adatbiztonsági tervének külön feladata.
A korábbi completed jelző önmagában nem bizonyítja, hogy egy jutalom még
jár: automatikus újraosztás nem történt. Éles átállás előtt friss,
konzisztens mentés, restore-próba és a megőrzendő jogok egyszeri
exportjának/átvezetésének igazolása szükséges. A táblák törléséhez külön
jóváhagyás kell.

A következő megvalósítási lépés az audit 3. fázisa: a natív, önálló
Mission graphmag és determinisztikus tesztjei, még játékosadatok és
éles jutalmazás bekötése nélkül.
