# C17 — Ékszerfoglalatok: ellenőrzések és helyi próba

2026-11-02. Szerződés: [IC-045](../../adr/item_combat_045_accessory_socket_runtime.md).

## Eredmény

Szerver és Unity együtt kezeli a profilos ékszer kapacitásnyitását,
időzített/végleges töltését, anyagfogyását, esélyét, mentett viselési
idejét, fokozatlejáratát és tényleges felszerelési bónuszát. Külön
megerősítés van; a nonce nem ismételhet gazdasági hatást. Másik anyag,
kevert élettartam, felszerelt cél és régi login elutasított.

## Lefutott ellenőrzések

| Ellenőrzés | Eredmény |
|---|---|
| Release GameServer és érintett natív célok | sikeres |
| CTest célzott regresszió | **25/25 csoport** |
| AccessoryRuntimeTests | **30 ellenőrzés** |
| Saját, külön MariaDB instance: ItemPersistenceSqlTests | **1302 ellenőrzés** |
| Unity 6.3 / 6000.3.27f1 scriptfordítás | sikeres |
| Natív v3 golden → Unity codec, TCP-framing, UI és dispatcher | **3397 ellenőrzés** |
| Korábbi C4–C16 Unity validátorok | mind sikeres |
| Valódi Unity → helyi TLS → GameServer → SQL | **3/3 fázis, 122 ellenőrzés** |
| Canonical Unity snapshot összevetés | **5420 forrásfájl; 1712 byteazonos C# fájl** |
| Helyi GameServer és telepített manager helper hash/manifest | egyező |
| Képernyőkép vizuális ellenőrzése | olvasható, a költség/esély/kimenetek és megerősítés elférnek |

A 25 CTest-csoport az új accessory runtime és a meglévő accessory domain
mellett a kőbehelyezést/karbantartást, ládát, pénzjegyet, wire/ground utat,
mentést/SQL-t/próbapadot, boltot, kereskedést, craftingot, equipmentet,
Missiont, csomagkezelőt és a grant/életciklus regressziókat is futtatta.

A külön SQL-próba a cél CAS-versenyét, a teljes művelet rollbackjét,
nyugtaismétlést, anyagfogyást, megmaradó szintet/azonosítót, felszerelést,
levételbe illesztett időt és a következő sessionben azonos maradékidőt méri.
A 30 domainellenőrzés a nyers/bp/egész minimumot, előjelet, nullát és a
példánybónusz szorzásának kizárását is ellenőrzi.

## Élő TLS-fázisok

1. Profilból származó 50%-os nyitás. A végső futás két próbából látott
   sikert és kudarcot. Mindkettő pontosan két anyagot fogyasztott, a cél a
   megfelelő szerveres kimenet lett. Ugyanaz a nonce nem dobott/fogyasztott
   újra. Rossz anyaggal a feltöltés nem változtatott semmit.
2. Garantált nyitás és két feltöltés: az új fokozat ideje pontosan beállt.
   Felszerelve a védelem 12000 raw, alsó fokozatban 11000 raw, lejárva
   10000 raw. Levétel, 1,5 másodperc offline idő és visszalépés azonos
   maradékidőt adott; újra felszerelve az utolsó fokozat is lejárt.
   A token- és ládaelőnézet legalább egy idő-checkpoint után is sikeresen
   megerősíthető volt. A ládából kapott tárgy elfoglalta a korábbi szabad
   cellát; a levétel új szabad helyet választott.
3. Teljes, 180 cellás inventory: nyitás és végleges töltés új outputhely
   nélkül működött. Időzített/végleges anyag nem keverhető. Az igazi Unity
   megerősítő gomb emelte a végleges fokozatot, viseléskor nem fogyott az
   állapot. Felszerelt cél tiltott; logout/relog megtartotta a teljes
   inventoryt, a régi nonce semmit nem fogyasztott.

## Helyi tesztkészlet

Az ellenőrzött, dedikált PID 1610 karakter 1000 Gold mellett visszakapta a
korábbi C4–C16 próbatárgyakat, egy üres, háromfoglalatos ékszert és nyitó,
időzített, illetve végleges anyagot 60–60 darabos halomban. A manuális
ékszer garantált profilt kapott; a fokozatidők 30/20/10 perc. Használat:
**I → anyag → Foglalatok → ékszer → nyitás vagy feltöltés → megerősítés**.
Az aktív fokozat csak a viselt ékszerből ad bónuszt.

A saját helyi DB, login, két ch1 core és a ch99 core (öt azonosított
gyermekfolyamat) fut. A korábbi kőbehelyezési beállítás **85% maradt**.
A telepített GameServer SHA-256:
`3c3c1cd1e48abb985db1aff58f16529581866986ecd869f7207c796215da280d`.

A tesztek a canonical forrásokból másolt elkülönített Unity-projektben
futottak. A tényleges Packages és UI-függőségek kerültek át; nincs módosított
teszt-runtime. A felhasználói jelenet-, kapcsolat-, Packages- és
ProjectSettings-módosításokat nem checkineltük, a ProjectSettings korábbi
hash-e változatlan. A snapshotban a hiányzó teljes világ/VFX/Ocean assetek
jelzései nem érintették a lefordult tárgykódot és a lefutott próbákat.

Újrafuttatás: `SeedAccessoryUnityLive.py` a leállított, saját csomaghoz;
`PrepareUnityValidationSnapshot.py`; majd `AccessoryUnityLiveSmoke.py` és
a projekt rögzített Unity Editora. A privát kapcsolati fixture a build
könyvtárban marad, nem kerül verziókezelésbe. A helyi műveletek katalógus-,
owner- és binárismentéseket készítettek a csomag `run` könyvtárában.

## Határok

A teljes Unity player build, FreeBSD, több core közötti átadás alatti
hibainjektálás, hálózati/SQL-terhelés és folyamatkilövéses időelszámolás
nem volt ennek a próbának része. A másodperces időcheckpoint és a közös
commit útvonal nem jelent teljes, crashbiztos viselésnaplót: váratlan
leállásnál vagy handoff-versenynél a még nem mentett rövid maradék
visszakapható. Ez a kiadási kapuknál nyitott követelmény.

Következő: a bónuszhozzáadás, -váltás és -zárolás közös szerver–Unity útja.
