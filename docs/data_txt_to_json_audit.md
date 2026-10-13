# data/ txt -> JSON atallas (GameServer) - audit es terv

Statusz: TERV (2026-10-13). A `share/data` alatti txt fajlok JSON-alapu
betoltesre atallitasa, paritas megorzessel; a txt-k fazisonkent torolhetok.

## 0. Hatokor

- Igeny: a GameServer **minden** `share/data`-s adatfajlja JSON-bol
  toltodjon (mint a `conf.json` atallasnal), a `.txt` fajlparserek
  megszunjenek.
- Nem resze: a `Launcher` (regi kliens) sajat olvasoi, a `.quest`/`.lua`
  quest scriptek (script-nyelv, nem adat), binaris fajlok (`.msm`,
  `server_attr`, `Mark`, `CRC/VERSION/BANIP`).
- Runtime allapotfajlok (whisper/lotto/gaya per-player) kulon dontes,
  javaslat: **kihagyni** (lasd 2/G).

## 1. Infrastruktura

- JSON lib mar van: `nlohmann/json.hpp` (GameConfig.cpp, DatabaseConfig.cpp,
  LoginConfig.cpp).
- Uj kozos helper: `GameServer/core/JsonFile.h` - `LoadJsonFile(path)`
  (ifstream + parse + LOG_ERROR), a GameConfig mintajara.
- Konverterek: `SRC/Tools/txt2json/` alatt py2.7/py3 kompatibilis
  scriptek (mint `motion_bake.py`); minden fajlcsaladhoz kulon, a
  paritast rekordszam + szumma ellenorzessel igazoljuk.
- Konvencio: a JSON ugyanott, ugyanazzal a nevvel, `.json` kiterjesztessel;
  a txt torlese a fogyaszto atallasa utan (fallback nelkul, mint a
  `.msa`-nal). A konverterek dokumentalva maradnak (uj adat = konverter
  ujrafuttatasa vagy kezi JSON szerkesztes).

## 2. Fajlcsaladok es fogyasztok

### A. CTextFileLoader "Group/Token" csalad (fo parser: core/text_file_loader.cpp)

| Fajl | Fogyaszto | Megjegyzes |
|---|---|---|
| group.txt, group_group.txt | entity/mob_manager.cpp | mob csoport spawn |
| special_item_group.txt | item/item_manager_read_tables.cpp | `//` kommentek a sorok vegen |
| mob_drop_item.txt | ugyanaz (ReadMonsterDropItemGroup) | 335 KB, gyakran szerkesztett |
| etc_drop_item.txt, common_drop_item.txt, drop_item_group.txt | ugyanaz | jelenleg mind 0 byte |
| ori_to_new_table.txt | ReadItemVnumMaskTable | jelenleg 0 byte |
| dragon_soul_table.txt | dungeon/dragon_soul_table.cpp | group_text_parse_tree |
| battle_pass.txt | combat/battle_pass.cpp | group_text_parse_tree |

Strategia: **strukturalis** konverzio (Group -> nested objektum, token-sor
-> tomb, kulcsok lowercase, mert a loader `stl_lowers`-t hasznal) +
`CJsonFileLoader` ugyanazzal az API-val (Load/SetTop/SetChildNode/
GetToken*), igy a fogyasztok logikaja valtozatlan. A vegen
`CTextFileLoader` + `group_text_parse_tree` torolheto.

### B. section/end formatus

| Fajl | Fogyaszto | Megjegyzes |
|---|---|---|
| blend.txt | item/blend_item.cpp | apply_value/apply_duration tobb ertek |
| cube.txt | item/cuberenewal.cpp | 36 KB, `#` komment |

Strategia: szemantikus JSON (tomb, tipusos mezok), a ket fogyaszto
parsere atirva. Mindketto kezzel szerkesztett adat -> olvashato schema.

### C. pozicios / tabularis

| Fajl | Fogyaszto | Megjegyzes |
|---|---|---|
| fishing.txt | world/fishing.cpp | **legacy encoding** (nem UTF-8; valoszinuleg cp949) - konverzional detektalando |
| gaya.txt | ecs/systems/GayaSystem.cpp | `ITEM vnum prez count##nev` sorok (UTF-8) |
| guild_renewal_levelup.txt | guild/guild_renewal.cpp | sima sorok; elo: `share/conf/` + `share/data/` duplikatum |
| shop_table_ex.txt | social/shop_manager.cpp | **HIANYZIK** az elo databol (a shopok DB-bol toltodnek; a hivas hibautat ad) - dontes kell: toroljuk a ReadShopTableEx hivast vagy JSON |
| skill_power.txt, leaderboard.txt, charset.txt | nincs szerver olvaso | kliens/quest oldal - nem szerver-hatokor |

### D. regen csalad (parser: world/regen.cpp read_line)

| Fajl | Megjegyzes |
|---|---|
| map/*/{regen,npc,boss,stone}.txt | 64/64/52/50 fajl, 75 map |
| dungeon/**/*.txt (regi + rune + viking + halloween + elveszett kastely stb.) | 112 fajl, kodban konstans utvonalakkal |
| metin2_map_battlearena0{1,2,3}/regen00.txt | BattleArena.h konstansok |
| metin2_map_wedding_01/npc.txt | wedding.cpp |

Grammar (`read_line`): `type sx sy ex ey z_section dir time(m/h/s) percent
count vnum` (+ exception tipus `e`), hiba eseten `exit(1)` is van benne - a
konverzional ezt pontosan kell masolni. Tovabba a kodolo a cella-kozepeket
(MODE_EX/EY) es a `*100` szorzast (sx *= 100) is.

Strategia: konverter ami a read_line-t pontosan implementalja -> JSON tomb
(mezok: type, sx, sy, ex, ey, z, dir, time, count, vnum); regen.cpp atirva
JSON olvasasra. Paritas: betoltott rekordok szama/szovegformaja + spawn
viselkedes teszt.

### E. map meta

| Fajl | Fogyaszto | Formatus |
|---|---|---|
| map/index | SECTREE_MANAGER::Build | `mapindex mapname` sorok |
| `<map>/Setting.txt` | LoadSettingFile | KV sorok (MapSize, BasePosition, CellScale stb.) |
| `<map>/Town.txt` | LoadMapRegion | `iX iY` + 3 empire spawn (6 int) |
| `<map>/dungeon.txt` | LoadDungeon | `position_name x y sx sy dir` (jelenleg egyik mapon sem letezik) |
| map/map_config.txt | core/config.cpp (funcTakeLines) | `Nev { title/desc/buttonx/buttony/coord_x/coord_y }` |

Megjegyzes: a `map/index` fajl `index mapname` formatumu (1246 byte, 75
map). A `map_0/` (177 txt) **nem olvasottnak tunik** (sehol nincs ra
hivatkozas) es a `share/locale/germany---------/` mappa backup hulladek ->
torles a fázis vegen.

FIGYELEM: a `map_config.txt`-t a kliens (Launcher) is olvassa; JSON-ra
valtasnal egyeztetni kell (a Unity ujrairas miatt elfogadhato, de a regi
kliens ezt meg hasznalja).

### F. quest

| Fajl | Fogyaszto |
|---|---|
| quest/questnpc.txt, quest/questcategory.txt | quest/questmanager.cpp (sima sorok) |

A `.quest`/`.lua` scriptek maradnak (Lua interpreter, nem adat).

### G. runtime allapot (javaslat: nem JSON)

- `whisper.txt` (admin lista, olvas+ir), `lotto.txt` (cwd, olvas+ir),
  `gaya/<nev>_gaya_system.txt` (1384 fajl), `special_item_group_vnum.txt`
  (debug dump).
- Ezek futasideju allapotok, nem konfiguraciok; JSON-ositas helyett
  kesobbiekben DB-s megoldas javasolt. A mostani fazisokban erintetlenek.

### H. Database import (kulon dontes)

- Database/ClientManagerBoot.cpp: `mob_names.txt`, `mob_proto.txt`,
  `item_names.txt`, `item_proto.txt` beolvasas import celra (client
  export). A Unity klienssel ez a folyamat ugyis valtozik -> kesobbi
  fazis, kulon dontes. `usage.txt` runtime log marad.

### I. nem-adat / hulladek

- Binaris: `map/*/server_attr`, `.msm`, `Mark`, `CRC/VERSION/BANIP/state_user_count`.
- Hulladek torles: `mob_drop_item.txt0218`, `special_item_group.txt0218`,
  `comon_Drop_item_input_format_razor93.txt`, `share/locale/germany---------/`,
  `map_0/`, `.bak`-ok.

## 3. Fazisok

- **Phase 1 - A csalad + infra**: `JsonFile` helper, `txt2json_group.py`,
  `CJsonFileLoader` (CTextFileLoader API), fogyasztok atkotese,
  `CTextFileLoader`/`group_text_parse_tree` torles. ~10 adatfajl.
- **Phase 2 - B + C**: blend, cube, fishing (encoding!), gaya,
  guild_renewal_levelup; shop_table_ex dontes.
- **Phase 3 - D + E**: regen csalad (~120 fajl) + map meta
  (index/Setting/Town/dungeon/map_config); `map_0/` + backup torles.
- **Phase 4 - F**: questnpc/questcategory -> JSON.
- **Phase 5 - H/I**: Database import (kulon dontes) + hulladek torles.

Minden fazis: konverter + paritas-ellenorzes -> Debug/Release build ->
commit -> elo szerver data frissitese (kulon lepes).

## 4. Kockazatok

- A regen `read_line` hibakezelese `exit(1)`-et tartalmaz; a konverternek
  bitre kell masolnia a szemantikat (cella-kozep, time m/h/s, exception).
- `special_item_group.txt` sorok vegen `//` kommentek; a CTextFileLoader
  token szinten kezeli; a konverternek ezt kezelni kell (a kommenteket
  el kell dobni, mert adatban nincs jelentesuk).
- Fajlkodolasok: `fishing.txt` nem UTF-8; a konverzio elott detektalni
  (cp949 gyanu). A tobbi minta UTF-8.
- A JSON kulcsok lowercase-ek (stl_lowers miatt), erre a fogyasztoi
  kodnak novelnie kell (a mostani is lowercase kulcsokat hasznal).
- A `map_config.txt`-t a regi kliens is olvassa.
- A `shop_table_ex.txt` hianyzik az elo adatbol.

## 5. Statisztika

- `share/data` txt osszesen: 2084 (root 23, map 386, map_0 177, dungeon
  112, gaya 1384 = runtime, quest 2).
- Szerver-altal olvasott, konvertalando adatfajl becsles: ~150-300 fajl
  (A/B/C ~15 + D ~120 + E 1 + 75*3 + F 2), mapankent 3-4.
