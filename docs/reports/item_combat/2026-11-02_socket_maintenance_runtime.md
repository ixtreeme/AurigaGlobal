# C15 — Foglalatkarbantartás: szerver és Unity ellenőrzése

2026-11-02. Natív ág `codex/mission-engine`, Unity 6.3 `6000.3.27f1`,
UVCS `/main/codex-mission-engine`, kiinduló cs48, elkészült cs49.
Döntés és protokoll: [IC-043](../../adr/item_combat_043_socket_maintenance.md).

## Elkészült

Törött foglalatok tisztítása, minden ép kő és az utolsó ép kő kiemelése.
Unityben a tényleges leltár **Foglalatok** gombja nyitja a szerveres
céltárgylistát, előtte/utána állapotot és megerősítést. A source fogyása,
teljes socket-csere és minden visszakapott kő egy SQL itemtranzakció.
Az eszköz teljes elfogyásával felszabaduló cella is használható.

A céltárgy ID-je, szintje, kötése és bónuszai, a kő típusa/szintje és a
foglalat minősége megmarad. Minden kő külön tartós ID-t kap; nincs részleges
visszaadás, stackmerge vagy földre dobó fallback. Ismert, retire-olt embedded
kő is visszanyerhető, szűken validált tranzakciós kivétellel; az általános
új-retired-tárgy tiltás megmarad.

## Ellenőrzések

- GameServer Release és a szükséges item tesztbinárisok lefordultak.
- **21/21 CTest-csoport sikeres.** Új socket maintenance **36**, korábbi
  socket service **102**, persistent item **188** ellenőrzés. Továbbá token,
  container, inventory/ground, shop/trade/craft/equipment/Mission és a
  kapcsolódó szerver-regressziók sikeresek.
- Saját izolált MariaDB: **1259 ellenőrzés**, és az ItemBench MariaDB-csoport
  is átment. Target CAS-hiba esetén a source, target és minden új kő
  visszagörgetett, nincs sikerreceipt. Replay nem fogyaszt/képez új példányt;
  módosított visszaadott kőszint és régi login elutasított. A teljes eredmény
  pontos ID/szint/bónusz/foglalat/wallet adatokkal visszatöltött.
- A kivezetett kő kivételének negatív esetei: többletdarab, hiányzó
  eszközfogyasztás, vissza nem cserélt socket, sérült későbbi before image.
  A write set átrendezése ugyanazzal a kanonikus digesttel elfogadott.
  Hibás vagy dupla output-layout a foglalás előtt elutasított.
- Unity C#-fordítás és **4763 új ellenőrzés**: natív golden GC235, minden
  csonkolás/TCP-vágás, hibás mezők, teljes kő-visszaadási bizonyítás,
  megmaradó arany minőség, pontos CG235/sequence, tényleges dropdown-template,
  megerősítés/pending/stale/reset, runtime dispatch és elutasító UI-szöveg.
- Korábbi Unity-csoportok is sikeresek: token **2785**, container **3429**,
  Mission **3871**, equipment **708**, crafting **3699**, trade **4553**,
  shop **4044**, inventory **4179**.
- **5/5 valódi Unity TLS-fázis, 172 ellenőrzés** a végső binárissal és
  forrással. A tényleges inventorygomb, céltárgy-dropdown és megerősítő gomb
  indítja a műveleteket. Régi nonce, megváltoztatott cél és időközbeni
  inventorymozgatás nem fogyasztott. Azonos confirm egyszeri eredmény.
- Tisztítás megőrizte az arany foglalatot. Utolsó kiemelés csak a legnagyobb
  foglalt indexet ürítette; az összes kiemelés megőrizte a +3 kő szintjét
  és a +1 fegyvert. A következő tisztítás nem tömörített és nem fokozott le.
- Hatástalan művelet, minimumszint, kevés eszköz, felszerelt cél, hiányzó cél
  és nem szolgáltató eszköz elutasított. Wallet végig változatlan.
- Teljes, 180 cellás leltárnál a részhalmos eszközzel a kétkő-kiemelés
  változatlan állapottal NoRoom; tisztítás ugyanott szabad cella nélkül is
  sikeres. Egy teljes eszköz helyét egy kiemelt kő elfoglalhatja, a többi
  beépített hely változatlan. A retire-olt +3 kő is pontosan visszakerült.
  A magas fegyver miatt az ilyen teli leltár **178 példányból** áll.
- Relog után minden ID, hely, szint, darab, kötés, bónusz, foglalat és Yang
  megegyezett. Az előző belépés nonce-a új írást nem indíthatott.

## Teszt-Editor és megőrzött felhasználói állapot

Az aktuális forrás külön másolata a `PrepareUnityValidationSnapshot.py`
eszközzel készült, a saját Addressables/SBP csomagokkal. **4322 forrásfájl**,
benne **1708 változatlan C# fájl** hash-ellenőrzése sikeres. A végső teszt-Editor
grafikás `-batchmode` módban futott és kilépett; az eredeti projekt nem kapott
tesztkérést. A `Logs/typed-socket-preview.png` vizuálisan ellenőrzött.

Az eredeti ProjectSettings hash változatlan:
`81b962db8bbddd8f97b48fc74e73e6f06b275e4153465f0bea5998f722032c98`.
A felhasználói scene/connection, csomagmanifest/lock, AI- és egyéb
projektbeállítások változásai megmaradtak és kimaradnak a C15 check-inből.
A source-only snapshot világasset-hookjainak hiányzó tartalma és az
AI-szolgáltatások figyelmeztetései nem e funkció fordítási hibái.
Standalone player, teljes világasset-build és FreeBSD-build nem futott.

## Helyi csomag

`D:\AurigaGlobal\Server2-Local`, dedikált PC #1610, térkép41,
kereskedő közelében. Additív protók, összes tárolt owner ellenőrzése és
pinfrissítés, tulajdonazonosság ellenőrzése minden fixture-reset előtt.
A történeti item-/Mission-receiptek megmaradtak.

- Első katalógus/bináris/manifest mentése:
  `run/deployments/socket-maintenance-20261102T175434`.
- Végső bináris mentése: `run/deployments/socket-final-20261102T180038`.
- Végső élő fázisok előtti és utáni állapotok:
  `run/socket-live/20261102T180102`.
- Telepített ItemBenchTool és manifest mentése:
  `run/manager-updates/socket-maintenance-20261102T180331`.
- GameServer SHA-256:
  `681af58fb49566e8d3027d7ce2f6ba35ef7dea11abdd74611fc75f4969f2e740`.

A kézi fixture 1000 Yangot, mindhárom karbantartó eszközből 3 darabot és
egy +1 fegyvert tartalmaz normál +3 kővel, arany törött hellyel és arany +0
kővel. A korábbi pénzjegyek, ládák, kulcsok, felszerelés, italok és anyagok
is rendelkezésre állnak. A telepített helperrel ellenőrzött pontos leltár és
foglalatok; az 1/99-es csatornák öt saját szerverfolyamata fut.

Újrafuttatás: `SeedSocketUnityLive.py` saját leállított csomaggal, privát C14
alapfixture-rel és natív toolokkal; friss `PrepareUnityValidationSnapshot.py`
projekt, batch Editor és `SocketUnityLiveSmoke.py`. A privát fiókadatok az
ignorált build könyvtárban maradnak. A test bench explicit foglalatállapota
is a natív grant/writer útján kerül be, nincs kézi item SQL.

Fő naplók: `build/socket-maintenance-hardened-build.log`,
`build/socket-maintenance-hardened-ctest.log`,
`build/socket-maintenance-editor-final.log`,
`build/socket-maintenance-live-harness-final.log`,
`build/socket-maintenance-snapshot-verification.json`.
