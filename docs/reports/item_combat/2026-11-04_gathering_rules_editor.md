# C25d — Gyűjtőhely-szerkesztő, helyi csomag és ellenőrzések

2026-11-04. A C25c-2 szabadvízi halászat után elkészült a gyűjtőhelyek
szerverkezelőbe épített űrlapja és a runtime-mal közös natív mentési
ellenőrzése. Használat: **Tárgykatalógus → Gyűjtőhelyek**, vagy
**Adatfájlok → typed_gathering.json**. Mentéshez a játékszervert le kell
állítani; a saját MariaDB maradjon bekapcsolva. A módosítás újraindításkor él.

## Lefuttatott ellenőrzések

- GameServer, CatalogEditorTool és GatheringRulesTests Release/x64 fordítása.
- **27/27 natív item-CTest**, köztük az új **37** parserellenőrzés,
  a halászati minijáték, gyűjtési runtime és eszközök tesztjei.
- **14** új Qt-teszt: külön helyek/jutalmak szerkesztése, szabadvízi váltás,
  pontos százalék/idő és unsigned azonosítók, hibás mezőnél megtartott adat,
  forrás–űrlap váltás, támogatás nélküli forrás javítása, dirty-file őr és
  kötelező natív mentési út. **20** forráskód-szerkesztő- és **14** általános
  GUI-regresszió szintén sikeres. A forráskód-tesztek natív ellenőrzője a
  telepített csomagból futott; az első, alapértelmezett útvonalas futás három
  hiányzó segédprogram-útvonalat jelzett, az újrafuttatás 20/20 sikeres.
- **6** csomagolási teszt, a MariaDB/zlib másolásával és a hiányzó DLL
  kiadás előtti elutasításával. A valódi csomagolt segédek semleges
  könyvtárból, adatbázis nélkül is elindultak.
- Valódi helyi katalógus és nyolc világhely: futó szervernél **12**,
  leállított szervernél **25** ellenőrzés. Ismeretlen jutalom, ismétlődő
  hely/kulcs, tiltott v1 szabadvíz és ismeretlen sémaverzió elutasítva.
  A visszautasítások nem írtak fájlt vagy mentést; az érvényes, változatlan
  fájl atomi mentése pontosan egy eredetivel egyező backupot hozott létre.
- A végső telepített natív segéddel további **10** helyi ellenőrzés sikeres.
  A tényleges fagyasztott manager worker is végrehajtotta a hibás jutalom
  elutasítását és a változatlan fájl érvényes, natívan ellenőrzött atomi
  mentését. Az eredeti bájtok és az egyetlen új backup egyezése ellenőrzött.
- Unity **6000.3.27f1** elkülönített snapshot: fordítás és C4–C25c-2
  offline validátorok sikeresek, köztük **12772** minijáték-ellenőrzés.
  Ehhez az eszközfejlesztéshez nem változott klienskód vagy csomagszerződés.

## Telepítés és állapot

A helyi `Server2-Local` friss GameServerrel, natív item-segédekkel és
újrafagyasztott Qt-kezelővel frissült, teljes visszaállítható mentéssel.
A csomagolt kezelő valódi ablakáról kép készült. Az első csomag önálló
segédindítása megfogta a hiányzó MariaDB/zlib DLL-eket; a csomagkészítő és
a végső disztribúció ezt javítja. A fejlesztői PATH nem feltétele a működésnek.

A világhelyek fájlja és a katalógus tartalma változatlan. A nyolc helyet
mindhárom game core betöltötte. A korábbi 157 itemállapot és hash, a
karakter/learning-checkpoint, valamint a C25c-2 halászati eredmények megmaradtak.
A végső készlet 165 élő itemállapot. Az eredeti Unity Editor nyitva maradt;
a felhasználó más jellegű scene/asset/Packages/ProjectSettings változásai
nem kerültek ebbe a munkába. Unity továbbra is cs67.

A disztribúció minden telepített fájlja és az öt natív futtatható állománya
egyezik az ellenőrzött builddel és a manifesttel. A snapshot mind az 1751
C# fájlja és az összes saját Unity-fájl egyezik az aktív workspace-szel;
a védett ProjectSettings hash változatlan. Az öt saját szerverfolyamat fut.

Helyi, ignorált bizonyítékok: `build/gathering-editor-*` teszt-, build-,
telepítési és ellenőrzési naplók. Hitelesítő adat nem kerül a jelentésbe.

## Határok

A szerkesztő natív katalógusellenőrzése a csomag saját helyi MariaDB-jét
támogatja. Távoli vagy külső adatbázishoz nincs mentési kerülőút. A map- és
NPC-azonosítókat a tartalomkészítő adja meg; nincs terepi helyrajzoló vagy
NPC-létrehozás, és a helyek végleges játékbalansza nem készült el.

A C25c-2 **75 valódi Unity TLS/UI-ellenőrzése** ugyanebben a munkamenetben,
a közös parser kiemelése előtt futott. Ezt a teljes, tárgyat módosító
minijáték-próbát a parser kiemelése után nem ismételtük meg. Az új parser
utáni ellenőrzés natív teszt, valódi editormentés, core-indítás és Unity
offline fordítás/validálás. Nincs új FreeBSD-, stressz- vagy crash-próba.

Nyitott marad a végleges tartalomkonverzió, további itemkategóriák,
általános typed Mission-események, fiókpénztárca, legacy átállás és F7.
