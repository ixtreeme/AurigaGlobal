# C26q — A tervezett fejlődőpet-rendszer lezárása

Szerverág: `20261103`. Unity-ág: `/main/unity63-integration-20261103`,
Editor: `6000.3.27f1`.
A C26k–q munka párosított Unity changesetjei: `cs:83`–`cs:86`.

Állapot: a C26c–p tervezett fejlődőpet-funkciói szerveren és Unityben
elkészültek; a C26q végső helyi regressziója és helyreállítása sikeres.

## Elkészült funkciók

| Terület | Szerver és Unity | Részletes bizonyíték |
|---|---|---|
| Keltetés, egyedi név, születési tulajdonságok | C26c | [IC-060](../../adr/item_combat_060_typed_pet_hatching.md) |
| Idézés, visszahívás, aktív életidő, újrabelépés | C26d | IC-060 |
| Etetés, újraélesztés, tárgy-EXP | C26e | IC-060 |
| Harci EXP, szintlépés és evolúció | C26f | IC-060 |
| Képességhelyek, tanulás és rangfejlesztés | C26g | IC-060 |
| Mind a 18 tanult képességhatás | C26h | [Harci mátrix](2026-11-07_pet_passive_live_combat.md) |
| Életkor és három születési tulajdonság bónuszai | C26i | [Természetes bónuszok](2026-11-07_pet_age_birth_bonuses.md) |
| Egy vagy minden képesség tárgyas elfelejtése | C26j | [Képességtörlés](2026-11-07_pet_skill_reset.md) |
| Életidő növelése és külön tárgyas korlátlanítás | C26k | [Életidő és enchant](2026-11-07_pet_life_enchantment.md) |
| Mindhárom születési tulajdonság fejlesztése | C26l | Életidő és enchant |
| Teljes példányállapotú szállítódoboz | C26m | [Doboz](2026-11-07_pet_carrier.md) |
| Megjelenési tárgyak, négy tartós időzítő | C26n | [Megjelenések](2026-11-07_pet_skins.md) |
| Tanulási várakozás tárgyból szerzett kihagyása | C26o | [Várakozáskihagyás és donor](2026-11-07_pet_study_bypass_sacrifice.md) |
| Másik fejlődő pet végleges felhasználása etetéshez | C26p | Várakozáskihagyás és donor |

Az új műveletek a külön típusos leltár és a **Pet adatai** ablak útján
érhetők el. A korábbi felhasználói döntés szerint a normál legacy leltár
és a meglévő karakterek teljes konverziója külön munka. A hátas felülése
nem része a fejlődőpet-csomagnak.

## Teljes folyamat- és adatbázis-újraindítás

A `PetColdRestartUnityLive.py` három, korábban már végigtesztelt helyi
karakteren idézte meg a petet, majd Unityből kilépett. Ezután az öt saját
szerverfolyamat és a saját MariaDB szabályosan leállt és új folyamatként
újraindult. A mentés előtti/utáni SQL-pillanatkép byte-ra azonos:
petpéldányok és blobok, pénztárcák, képességtanulási adatok és határidők,
munkamenetkerítések, karakter-EXP és képességcheckpoint.

A tárolási ujjlenyomat:
`236d4619cddcdbdb6f98bb269acc9b1af5f964733ae73ef6bea96d1cb862f80b`.
A három kiválasztott tulajdonoson kívüli **620 tárgy**, valamint a korábbi
pénztárcák és tanulási adatok változatlanok maradtak.

| Pet | Unity előkészítés | Újrabelépés az új folyamatokba | Eredmény |
|---|---:|---:|---|
| Korlátlanított, fejlesztett születési tulajdonságokkal | 246 ellenőrzés | 257 ellenőrzés | Az életidő továbbra is korlátlan, a tulajdonságok és statok megmaradtak |
| Véges aktív életidő, felvett megjelenés, 1. rangú képesség | 291 | 316 | Ugyanaz a skinpéldány és kötés, helyes követőalak és bónuszok |
| Véges aktív életidő, 3. rangú képesség, elköltött kihagyótöltetek | 246 | 263 | A rang, nulla töltet és a pontos karaktercheckpoint visszaállt |

Az aktív életidő csak az online követés alatt csökkent. A kilépések közötti
SQL-állapot is változatlan maradt. A próba végén a három pet visszahívott,
a teszt során felvett állandó megjelenés levett állapotba került.

Bizonyíték: `build/c26q-cold-restart.log`,
`build/c26q-cold-restart/evidence.json` és a hat Unity-futás naplója.
A nyers SQL-pillanatképek és a hozzáférési adatokat tartalmazó fixture-ök
az ignorált `build/` könyvtárban maradnak.

## Végső regresszió

A közös tulajdonosi skinállapotot használó végső GameServer builden mind
a négy óra újra átment, **1019 Unity-ellenőrzéssel**. A korlátlan skin
kötése, levétele és visszahívott állapotú újrabelépése megmaradt. Az első
használati határidő nem indult újra belépéskor; a tárgy visszahívott peten
is lejárt. Az aktív órából visszahívva és offline nem fogyott idő, a
követés újraindításakor viszont elfogyott és visszaállt az alapforma.
A létrehozási óra felvétel nélkül is lejárt. Mindhárom lejárt skin eltűnt
az újrabelépéses mentésből, a pet születése és fejlődése megmaradt.
Az SQL-utóellenőrzés **697 korábbi tárgyat** és az összes korábbi
pénztárca/tanulási adatot változatlannak találta. A próba világa helyreállt.

Bizonyíték: `build/c26q-skins-live.log`,
`build/c26q-skins-sql.launcher.log`, `build/c26q-skins-restore.launcher.log`.

A teljes 18 hatásos harci mátrix is sikeres, **3607 Unity-ellenőrzéssel**.
Minden hatás külön pecsétet és tényleges könyvtanulást kapott. Minden
esetben átment az alapérték, idézés, aktív újrabelépés, visszahívás és
visszahívott újrabelépés. A Unity statablak az aktív rangértéket mutatta;
a mérés tényleges találatot, beérkező sebzést, EXP-t vagy aranyjutalmat
ellenőrzött.

| Ellenőrzött csoport | Mért eredmény a tesztprofilokkal |
|---|---|
| Egész és százalékos max. HP | 31260 → 31510, illetve 39075 |
| Szörny-, normál PvE-, főellenfél-, metin- és fizikai támadás | 417 → 626 sebzés |
| Szörnyellenállás | 433 → 217 sebzés |
| Játékos-, harcos- és Sura-ellenállás | 409 → 205 sebzés |
| Nindzsa- és sámánellenállás | 386 → 193 sebzés |
| PvE kritikus esély | 417 → 835, kritikus találati jelző 33 |
| Harci EXP | 100 → 200 jutalom |
| Aranydobás | Aktívan és aktív újrabelépés után minden ölés adott aranyat |
| Blokkolás | 409 → 0, találati jelző 8 |
| Nyílkikerülés | 433 → 0, találati jelző 4 |

Az SQL-utóellenőrzés mind a 18 képességet külön petben, 1. rangon és
visszahívott állapotban olvasta vissza. **727 korábbi tárgy** és a korábbi
pénztárca-/tanulási adatok változatlanok maradtak. Az ideiglenes hat
szörnyprototípus eltűnt, a regenfájl és manifest helyreállt. A szokásos
öt helyi szerverfolyamat újra fut.

Bizonyíték: `build/c26q-combat-live.log`,
`build/c26q-combat-sql.launcher.log`, `build/c26q-combat-restore.launcher.log`,
`build/c26q-final-regression.log` (`skins=4 combat_effects=18`).

## Fordítás és további ellenőrzések

- Windows GameServer és mindkét natív kezelőeszköz lefordult.
- Kilenc releváns natív próba sikeres, köztük a MariaDB-integráció,
  karakter-CAS-visszagördülés és a donor teljes auditállapotának mentése.
- Hat pethez kapcsolódó Qt/model próba sikeres.
- Az izolált Unity 6.3 projekt lefordult: **7771 petcsomag-, 1640
  felszerelési/stat-, 340 companion- és 4179 leltárellenőrzés** sikeres.
  A petablakok, codecok és tesztfuttatók 14 forrás-/metaállománya
  hash-re egyezik az aktív Unity workspace állományaival:
  `build/c26q-unity-source-identity.json`.
- A C26o és C26p élő Unity-próbái 602 és 803 ellenőrzést végeztek,
  az erőforrások és a petállapot újrabelépés utáni ellenőrzésével.
- A friss szerverbinárisok, natív katalógusellenőrző és a Qt-kezelő
  a saját `Server2-Local` csomagban vannak, az előző verziók mentésével.

Az élő próbák valós helyi TLS-kapcsolaton, Unity Editorból futnak,
elkülönített karakterekkel és visszaállítható próbaszörnyekkel. Ez nem
FreeBSD-build, terheléses próba vagy minden grafikai asset vizuális
ellenőrzése. A csökkentett Unity-validáló másolatból hiányzó SpeedTree-,
VFX- és Crest-adatok külön indítási hibajelzései a petpróbától függetlenek.
