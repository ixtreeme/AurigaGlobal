# Teljes item-protó konverzió – bónuszváltók és ékszerfoglalatok

Újabb részeredmény: [2026-11-14_full_item_proto_tools](2026-11-14_full_item_proto_tools.md) – 9 296 konvertált, 1 199 fennmaradó sor.


2026-11-14. **80 további forrássor** konvertálható. A teljes átállás folyamatban van.
A „konvertált” a natív katalógusba ábrázolható protót jelenti; nem igazolja minden
tárgy teljes játékbeli működését.

| Állapot | Előző | Most |
|---|---:|---:|
| Konvertált forrássor | 9 186 | 9 266 |
| Kivezetett forrássor | 491 | 491 |
| Fennmaradó akadállyal | 1 309 | 1 229 |
| Összes forrássor | 10 986 | 10 986 |

**6 348 natív definíció**, katalógusséma **34**. Az eredeti 1 574 definíció és
29,204 eredeti tartalmi sor megőrzése oszloponként ellenőrzött.
Katalógushash: `02fa8c688b50f28c432f57e7d19c64a5b05501ee52fa3f41580115828214ba4a`.

## Új működés és konverzió

- Hat normál/zodiákus bónuszváltó: 39028, 71151, 76014, 76023, 86060, 88959.
  A zodiákus tárgycsoport natív szabálymező; forrásazonosító nem kerül futásidejű
  diszpécselésbe. A zöld váltók 30-as céltárgyszint-korlátot kapnak; a 76014
  minőségi esélyei 0/10/50/39/1 százalék.
- Az addon-fegyverek átlagos- és képességsebzés-párja újrasorsolódik. A negatív
  érték, a nulla sor kihagyása, a megmaradt helyek feltöltése és a védett sor
  megőrzése tesztelt. A véletlen döntéseket a műveleti bizonylat rögzíti.
  A szerver GC236 v2 csomagját a Unity olvassa; a v1 olvasása is megmaradt.
- 45 ékszerfoglalat-anyag: a gyémánt és 44 érc natív anyagként szerepel.
  **87 új ékszerdefiníció** kapott **14 pontos kompatibilitási profilt**.
  Időszakos/végleges töltés, a forrás speciális kivételei, 24/12/6 órás aktív
  idő és 10/20/40 százalékos erősítés szerepel a natív profilokban. A jelenlegi
  buildben a nyitás és a töltés sikeresélye 100%; a konstans táblában található,
  e műveleti ágban nem használt 70%-ot nem alkalmazzuk.
- A kisállat-szállítódoboz (55002) natív, állapottartó Carrier kellék.
- Az `ITEM_ANTIFLAG_SAVE` a vizsgált forrásban nem vezérli a mentést. A tényleges
  `skipSave` útvonal ellenőrzése után a téves tiltás megszűnt; az inaktív jelző
  a soronkénti naplóban megmarad. Ez további 12 küldetéstárgy, 10 fogyasztható
  és 6 fegyverkosztüm ábrázolását is lehetővé tette. A külső questkezelők
  működését ez a változás nem valósítja meg.

## Elvégzett ellenőrzések

- GameServer és natív tesztprogramok fordítása.
- 61 külön CTest-cél legutolsó futása sikeres. Az első 60-as futásban két
  ellenőrzés hibázott: egy régi bónuszforrás-feltételezés és a katalógushash-t
  tartalmazó tesztcsomagok. A javítások után mindkettő célzott újrafutása sikeres.
  A protokollmintákban csak a 32 bájtos katalógushash változott.
- 39 teljeskonverziós teszt; 15 független Python/C++ katalógusdigest-minta.
- Admin/Qt: 71 tesztből 70 sikeres, 1 SQL-konfigurációt igénylő teszt kihagyva;
  az SQL-es szerkesztőteszt külön CTest-célként sikeres. A zodiákuscsoport
  szerkesztési és visszatöltési körét további célzott próba ellenőrzi.
- A tényleges GameServer `ItemStore` osztálya elkülönített MariaDB-ben végrehajtja
  az újrasorsolást. Azonos művelet újraküldése ugyanazt a bizonylatot használja,
  nem fogyaszt még egy eszközt és nem húz új véletlent. A páros, előjeles
  bónuszok és az egyszeri fogyás visszalépés után is megmaradnak. Az innen
  exportált GC236 csomagot a tényleges Unity ablak is feldolgozta.
- Unity **6000.3.27f1**: fordítás és 12 integrációs validátorsorozat sikeres,
  köztük az attribútum-, ékszerfoglalat-, felszerelés-, leltár-, craft-, trade-
  és shop-ellenőrzések.
- A teljes 6 348 definíciós jelölt külön, új MariaDB-adatkönyvtárban SQL-be
  írva és natívan visszaolvasva: a hash egyezik. A jelölt nincs a játékba telepítve.

## Még nyitott

| Akadálycsoport | Forrássor |
|---|---:|
| Hiányzó/hibás ikon, modell, vizuális asset | 466 |
| Hiányzó ládajutalom-csoport vagy jutalomfüggőség | 286 |
| Még hiányzó viselkedés / példányon választott funkció | 370 |
| Forrásadat, korlát vagy más szabály | 107 |

A 286 loot-akadályból 236-nál hiányzik a forrás jutalomcsoport. A viselkedési
munkák között még szerepelnek kisállatkönyvek és -kinézetek, mountkinézetek,
horgászbotok/csákányok fejlesztése, lélekmag-átmenetek és egyes kosztümszolgáltatások.
Nyitott továbbá a régebben kiadott ékszerprofilok külön korszerűsítése, az
általános születési szabályok és lejáratok bekötése, illetve a questkezelők auditja.

Teljes belépés/auth/TLS/ECS világfolyamatot e jelölttel még nem teszteltünk.
A sikeres worker–SQL–Unity kör nem helyettesíti az összes protó játékbeli próbáját.
`release_ready=false`; a telepítési védelem a hiányos teljes katalógust visszautasítja.

A helyi szerveren továbbra is 0 account, karakter, régi tárgy, natív tárgypéldány,
céh és chatelőzmény található; 94 játékállapot-tábla üres. A futó helyi katalógus
1 574 definíciós maradt. Korábbi játékosadat nem került vissza.

## Bizonyíték és folytatási alap

- [2026-11-14_full_item_proto_reroll_accessories.csv](2026-11-14_full_item_proto_reroll_accessories.csv): mind a 10 986 forrássor és a 80 újonnan konvertált sor jelölése.
- [2026-11-14_full_item_proto_reroll_accessories.evidence.json](2026-11-14_full_item_proto_reroll_accessories.evidence.json): hash-ek, tesztcélok és Unity-validátorok.
- `build/full-proto-reroll-accessory-v34.json`: tiszta, eredeti statikus bemenetekből újrafordított jelölt.
- `build/full-proto-reroll-accessory-sql/catalog-stage-result.json`: izolált SQL-visszaolvasási eredmény.
