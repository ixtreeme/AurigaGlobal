# C14 — Pénzjegyek beváltása: közös szerver–Unity ellenőrzés

2026-11-02. Szerverág `codex/mission-engine`, Unity 6.3
`6000.3.27f1`, UVCS `/main/codex-mission-engine`, kiinduló cs47, elkészült cs48.
Döntés: [IC-042](../../adr/item_combat_042_token_redemption.md).

## Elkészült viselkedés

Az új CurrencyToken Yang-változata a tényleges leltár **Beváltás** gombjáról
használható. Darabszám választható, a szerver az egészében beférő darabokat
és a pontos jóváírást idézi. A részhalom megtartja ID-jét és helyét;
teljes fogyáskor csak a forrás tűnik el. A pénz és a tárgy egy SQL-commit.
Az egyenlegkorlát a meglévő typed wallet `INT64_MAX` értéke.

Sárkányérmét még a legacy account-writer birtokol: az ilyen utalvány
Unsupported állapotot mutat, és nem fogy. Nincs kitalált új karakteregyenleg
vagy legacy visszaírás. Ez a C14 tudatos határa.

## Eredmények

- GameServer Release és a szükséges natív tesztbinárisok lefordultak.
- **18/18 CTest-csoport sikeres**, köztük token redemption **28**, korábbi
  currency token **26**, container opening **26**, container **72**,
  persistent item **188**, item-only Mission **23** ellenőrzés.
- Külön, tulajdonolt MariaDB-sandbox: **1239 ellenőrzés**. A részhalom és
  a wallet pontosan visszatöltődik; azonos receipt replay nem ír újra;
  módosított payload elutasított; wallet CAS-hiba után az item/wallet/fence
  és receipt változatlan; régi belépés nem válthat be újabb darabot.
- Unity C#-fordítás, **2785 új ellenőrzés**: natív golden GC234, minden
  csonkolás/TCP-vágás, hibás quote, pontos CG234/sequence, Redeem action,
  mennyiségcsere, megerősítés/pending, nincs optimista item-/pénzmódosítás,
  stale/reset és normál runtime dispatch.
- Korábbi Unity-csoportok is átmentek: container **3429**, Mission **3871**,
  equipment **708**, crafting **3699**, trade **4553**, shop **4044**,
  inventory **4179**.
- **5/5 valódi Unity TLS-fázis, 135 ellenőrzés**. A tényleges inventorygomb,
  darabszámmező, frissítés és megerősítés indította a beváltást. 10×25 Yangos
  forrásból 3 fogyott, 75 Yang érkezett és ugyanabban a halomban 7 maradt.
  Azonos confirm, módosított mennyiség és lecserélt nonce nem ír újra.
- Egy másik tárgy mozgatása elavította a quote-ot. Account-utalvány,
  minimumszint, túl nagy darabkérés és nem pénzjegy nem fogyott el.
- `INT64_MAX-62` egyenlegnél a kért 10 helyett 2 darab fogyott és 50 Yang
  érkezett; 8 megmaradt. Egy egész darabhoz kevés hely esetén nulla fogyás.
  `INT64_MAX` értékű egységből 2 helyett 1 darab váltható be üres walletbe,
  túlcsordulás nélkül. A további beváltás változatlan állapottal elutasított.
- 180 teli cella mellett a teljes pénzjegy beváltható: 179 anyaghalom
  megmarad és 1 cella felszabadul. Minden tárgy ID/hely/szint/darab és a
  teljes 64 bites pénzösszeg újrabelépés után is pontosan egyezett;
  az előző belépés nonce-a nem működött.

## Unity tesztkörnyezet

A `PrepareUnityValidationSnapshot.py` az aktuális forrás és a saját
Addressables/SBP csomagok byte-másolatát készítette külön projektbe.
**4312 forrásfájl**, benne **1704 változatlan C# fájl** hash-ellenőrzése
sikeres. A megadott ProjectSettings hash változatlan:
`81b962db8bbddd8f97b48fc74e73e6f06b275e4153465f0bea5998f722032c98`.
A teszt-Editor grafikával, `-batchmode` módban futott és magától kilépett.

A `Logs/typed-token-preview.png` vizuálisan ellenőrzött: a 64 bites
egyenleg, részleges fogyás, maradék és megerősítő gomb olvasható.
Az uGUI EditMode ága `SetTextWithoutNotify` mellett is küld változási
eseményt; explicit guard választja el a szerver quote-jának kitöltését a
kézi mennyiségmódosítástól. Ezt a működő UI-teszt is ellenőrzi.

A source-only másolat hiányzó világassetekhez kötődő korábbi Editor-hookjai
és AI-szolgáltatási figyelmeztetései nem részei ennek a funkciónak. Nem
futott standalone player, teljes világasset-build vagy FreeBSD-build.
A felhasználói jelenet, kapcsolati asset, csomagok és projektbeállítások
változásai megmaradtak, nem részei a C14 check-innek.

## Helyi csomag és újrafuttatás

`D:\AurigaGlobal\Server2-Local`, dedikált PC #1610, térkép41, kereskedő
közelében. A protók a katalógusszerkesztővel additívan kerültek be; az összes
tárolt tulajdonos ellenőrzése után lépett a pin. Nem történt régi protó
vagy történeti receipt törlés. Minden fixture-reset előtti állapot mentett.

- Katalógus/bináris/manifest mentése:
  `run/deployments/token-redemption-20261102T172547`.
- Élő fázisok mentései: `run/token-live/20261102T173141`.
- Telepített ItemBenchTool frissítésének mentése:
  `run/manager-updates/token-redemption-20261102T173331`.
- GameServer SHA-256:
  `375736180b9247c12472fe2e2f284682f3305acd67292fb8e6e83c13be8edb86`.

A kézi próba 1000 Yanggal, 10 normál pénzjeggyel, 3 account-utalvánnyal,
2 nagy értékű pénzjeggyel indul. A korábbi ládák/kulcsok, +1 fegyver,
+0 vért, italok és anyagok is megvannak. Az eredeti 1/99-es csatornaválasztás
öt saját szerverfolyamata fut. A telepített manager helperrel is ellenőrzött
a pontos leltár és pénz.

Újrafuttatás: `SeedTokenUnityLive.py` leállított saját csomaggal, privát C13
alapfixture-rel és natív toolokkal; `PrepareUnityValidationSnapshot.py`
friss külön projektbe; batch Editor és `TokenUnityLiveSmoke.py`. A privát
fájlok és fiókjelszavak az ignorált build könyvtárban maradnak.

Naplók: `build/token-redemption-first-build.log`,
`build/token-redemption-final-ctest.log`, `build/token-redemption-editor4.log`,
`build/token-redemption-live-harness4.log`,
`build/token-redemption-snapshot-verification.json`.
