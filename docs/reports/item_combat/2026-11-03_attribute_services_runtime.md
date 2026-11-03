# C18 — Bónuszszolgáltatások: ellenőrzések és helyi próba

2026-11-03. Szerződés: [IC-046](../../adr/item_combat_046_attribute_services_runtime.md).

## Eredmény

Hozzáadás, újrasorsolás, védelem, védelem áthelyezése és feloldás működik
a szerveren és Unityben. Normál/ritka/extra csoport, esély, szinteloszlás,
súly, fogyasztás és célkövetelmény a katalógusból jön. A védett sor megmarad;
a source/target és a teljes receipt egyetlen tranzakció. Az előnézet nem sorsol.

## Lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer és érintett natív célok | sikeres |
| CTest célzott regresszió | **27/27 csoport** |
| AttributeRuntimeTests | **38 ellenőrzés** |
| IC-031 InstanceAttributeTests | **137 ellenőrzés** |
| Külön, saját MariaDB: ItemPersistenceSqlTests | **1380 ellenőrzés** |
| Unity 6000.3.27f1 scriptfordítás | sikeres |
| Új golden, codec, TCP-framing, metadata v2, UI és dispatcher | **4912 ellenőrzés** |
| Korábbi C4–C17 Unity validátorok | mind sikeres |
| Valódi Unity → TLS → GameServer → SQL | **4/4 fázis, 338 ellenőrzés** |
| Canonical Unity snapshot összevetése | **5430 fájl, 1716 byteazonos C# fájl** |
| UI-képernyőkép vizuális ellenőrzése | költség, esély, védett sor, jelöltek és megerősítés elférnek |

A 27 csoportban az attribútumok mellett a korábbi accessory/socket,
inventory/ground, mentés/SQL/próbapad, doboz/pénzjegy, bolt, kereskedés,
crafting, stat, Mission, csomagkezelő és grant/életciklus regressziói futottak.
Az SQL-próba CAS-verseny teljes rollbackjét, fogyasztás és védelem közös
commitját, eltérő rögzített dobású receipt elutasítását, védett sor
megtartását, relock/unlockot és a relog utáni teljes állapotot is ellenőrizte.

## Élő fázisok

1. **Három csoport, öt művelet.** Az igazi inventory `Bónuszok` gombja és
   megerősítése hozzáadást végzett. Öt külön normál sor, statikus kizárás,
   védelem, újrasorsolás, másik sorra áthelyezés és feloldás. A lock-eszköz
   teljes halma elfogyott; replay nem igényelt megmaradó source itemet.
   Két ritka és egy extra sor közösen zárta ki az ismétlést. Ritka/extra
   reroll megtartotta a másik csoportot. Fegyver, páncél és ékszer is
   módosítható volt. Relog után a teljes inventory egyezett; régi nonce nem fogyasztott.
2. **Valódi véletlen és kudarc.** Az 50%-os fogyasztó és nem fogyasztó
   hozzáadás sikere/kudarca is megjelent. A félig lefedett szinteloszlás
   eredménytelen dobását a szerver nem normalizálta/ismételte, semmi nem
   fogyott/változott. A végső futás a három eszköznél 3/2/3 próbából látta
   mindkét kimenetet. Nonce-ismétlésnek nem volt új gazdasági hatása.
3. **Teli leltár és valódi stat.** A 180 cellás, háromcellás fegyver miatt
   178 példányos leltárban is hozzáadható öt normál sor, védhető és
   újrasorsolható a tárgy outputhely nélkül. Felszerelve minden bónusz
   pontosan egyszer jelent meg a GC231-ben. Felszerelt cél tiltott; levétel
   és relog után a teljes állapot megmaradt.
4. **Elutasítás és C17 együttműködés.** Minimumszint, célmaszk, elégtelen
   halom, hiányzó sor/védelem és jelölt nélküli profil nem fogyasztott.
   Másik inventory-mozgatás a régi quote-ot RNG előtt érvénytelenítette.
   Viselt C17 ékszer checkpointja közben az igazi bónuszmegerősítő ablak
   nyitva maradt, a művelet a friss fence-en is sikerült.

## Helyi állapot és újrafuttatás

A dedikált PID 1610 karakter visszakapta az összes C4–C17 próbatárgyat,
három üres bónuszcélt (+1 fegyver, +1 páncél, +1 ékszer), valamint a kilenc
normál/ritka/extra eszközt 60–60 darabos halomban. **I → eszköz → Bónuszok**,
majd céltárgy és megerősítés. Nincs rejtett Yang-költség; kiindulás 1000 Gold.

Az öt saját gyermekfolyamat fut: adatbázis-szerver, login, két ch1 core és
ch99 core. A kőbehelyezés korábbi 85%-os beállítása változatlan. A telepített
GameServer és manager helper hashét/manifestjét ellenőriztük. A telepítés
katalógus-, karakter- és binárismentéseket készített a csomag `run` könyvtárába.

Újrafuttatás: `SeedAttributeUnityLive.py` leállított saját csomaggal,
`PrepareUnityValidationSnapshot.py`, `AttributeUnityLiveSmoke.py` és az
`attribute-live` Editor bridge. A privát kapcsolati fixture nem követett
build fájl; a logok, képernyőkép és hash-ellenőrzések is itt találhatók.

A snapshot canonical forrásokat, tényleges Packages- és UI-függőségeket
másolt. A fordított C# byteazonos volt. A felhasználó jelenet-, kapcsolat-,
Packages- és ProjectSettings-módosításai megmaradtak, nem kerülnek a C18
checkinbe. A hiányzó teljes világ/VFX/Ocean assetek snapshot-jelzései nem
érintették a tárgyvalidációt.

## Még nyitott

Teljes Unity player build, FreeBSD és többcore-os terhelés/hibainjektálás
nem futott ebben a szeletben. Kapcsolt/Gauss addon, jelmez/stóla/rúna/reset,
teljes konverzió és kiadási kapuk külön munka. A C17 időcheckpoint korlátja változatlan.
