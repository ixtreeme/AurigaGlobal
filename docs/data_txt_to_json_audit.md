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

- **Phase 1 - A csalad + infra (KESZ, 15f49bb3)**: `JsonFile` helper,
  `CJsonGroupLoader`/`CJsonGroupTreeLoader`, fogyasztok atkotese
  (mob_manager, item_manager_read_tables, battle_pass, dragon_soul,
  shop_manager), `CTextFileLoader`/`file_loader`/`group_text_parse_tree`
  torlese. 10 adatfajl JSON-ban, a txt-k torolve.
- **Phase 2 - B + C (KESZ)**: blend, cube, fishing (cp949 name dekod),
  gaya, guild_renewal_levelup; shop_table_ex olvasas JSON-ra allitva
  (a fajl tovabbra sem letezik; a shopok DB-bol).
- **Phase 3 - D + E (KESZ)**: regen csalad + map meta JSON-ban; a
  quest lua regen utvonalak is `.json`-ra irva; `map_0/` +
  `share/locale/germany---------/` torolve. A `map_config.txt` (kikapcsolt
  ENABLE_MAP_TELEPORTER + regi kliens) es a MapProperty.txt kimaradt.
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


## 6. Implementacios statusz (2026-10-13)

### Phase 1 (KESZ, commit 15f49bb3)

- Uj infrastruktura: `core/JsonFile.{h,cpp}` (nlohmann parse + log) es a ket
  loader: `core/json_group_loader.{h,cpp}` (CJsonGroupLoader, a
  CTextFileLoader API-parja) es `core/json_group_tree.{h,cpp}`
  (CJsonGroupNode/CJsonGroupTreeLoader, a CGroupTextParseTreeLoader parja).
- A JSON szerkezet: `{ "tokens": {kulcs: ertek|tomb}, "groups":
  [{name, tokens, groups}], "columns": [...] }`; kulcsok/group nevek
  lowercase-elve (mint a regi `stl_lowers`), ertekek valtozatlanok.
- Konverter: `SRC/Tools/txt2json/txt2json_group.py` (tokens|rows mod);
  a regi parser break/exit viselkedeset is masolja; nem-UTF-8 fajlokat
  (group.txt, dragon_soul_table.txt) latin-1-gyel byte-huen olvas.
- Atkotve: mob_manager (group), item_manager_read_tables (common/etc/mob
  drop, special_item_group, drop_item_group, ori_to_new_table mask),
  battle_pass, dragon_soul_table, shop_manager. Az etc_drop_item es
  ori_to_new_table uj semantikus JSON (`[]`).
- Torolt loaderek: `text_file_loader.{h,cpp}`, `file_loader.{h,cpp}`,
  `group_text_parse_tree.{h,cpp}` (a ShopLifecycleTests CMake listaja is
  frissitve).
- Az elo `share/data`-ban: group/group_group/common_drop_item/
  mob_drop_item/special_item_group/drop_item_group/dragon_soul_table/
  battle_pass/etc_drop_item/ori_to_new_table -> `.json` (a txt-k torolve).
- Paritas: txt vs JSON group-szamok egyeznek (926/216/183/98/35/12), a
  kulcs-mezok spot-checkje egyezik.

### Phase 2 (KESZ)

- `blend.json`: `{"blends":[{item_vnum, apply_type, apply_value[5],
  apply_duration[5]}]}`; a fogyaszto ugyanazt a hibat adja hianyos
  tombre, mint a regi parser.
- `cube.json`: `{"cubes":[{npcs[], items[{vnum,count}],
  rewards[{vnum,count}], percent, category, allow_copy, gaya, gold}]}`;
  a regi FN_check_cube_data validacio valtozatlan, a hibas section
  kiesik. (A "49051" nevu sor a regi parserben sem volt hasznalva.)
- `fishing.json`: `{"fish":[{name, vnum, dead_vnum, grill_vnum, prob[4],
  difficulty, time_type, length_range[3], used_table[10]}]}`; a nev
  cp949 -> UTF-8 dekodolas (a kliens oldali korea nevek); a regi parser
  "utolso tab utani oszlop elveszik" viselkedese megmaradt; hianyzo
  oszlop -> 0.
- `gaya.json`: `{"items":[{item, gaya, count}], "config":{...}}`; a
  config kulcsok a regi TOKEN-ek lowercase-ei; a fajlvegi `##nev`
  reszek a regi parserben sem szamitottak (value3-ba estek, a vnum
  szamitott csak).
- `guild_renewal_levelup.json`: `{"levels":[{level, yang,
  items[{vnum,count}] x5}]}`; validacio 21..NSBUZERANT (30) es
  "van-e ertelmes sor" ugyanaz. A `share/conf` es `share/data` peldany
  is JSON-ra valtott, a loadersorrend valtozatlan.
- `shop_table_ex` olvasas `shop_table_ex.json`-ra allitva (a fajl az elo
  adatban nem letezik; a shopok tovabbra is DB-bol toltodnek, a hiany
  ugyanugy csak log).
- Build: GameServer Debug + Release zold; ShopLifecycleTests zold.

### Hatralevo

- Phase 4: questnpc/questcategory.
- Phase 5: Database import txt-k (kulon dontes) + junk (.txt0218/.bak/
  .bck/.ryuga, comon_Drop_item_input_format_razor93.txt) torles.


## 7. Phase 3 statusz (2026-10-13)

### Regen csalad (D)

- Konverter: `SRC/Tools/txt2json/txt2json_regen.py`. A regi `get_word`/
  `read_line` token-stream portja (whitespace tokenek, `//` sorvegig,
  `e` tipus z utan zar, EOF-nal felbevágott bejegyzes eldobva).
- JSON: `{"regens":[{type, x, y, range_x, range_y, z, dir, time,
  percent, count, vnum}]}`; a `time` nyers string ("10s", "1h30m"),
  a `percent` benne marad de a loader nem hasznalja (mint eddig); a
  kozeppont/sugar szamitas (`sx=(x-rx)*100`, `ex=(x+rx)*100`) a
  betolteskor tortenik, igy a REGEN struct valtozatlan maradt.
- Szerver: `regen_do` / `regen_load_in_file` / `regen_load` (es az
  EXCEPTION ag) JSON-t olvas; a `get_word`/`next_line`/`read_line` es az
  `ERegenModes` torolve. A típusfeloldas (`m`/`g`/`ga`/`e`/`r`/`s`,
  ismeretlen -> exit(1)) bitazonos.
- Erintett utvonalak: `map/*/regen.json|npc.json|boss.json|stone.json`,
  `dungeon/**/*.json` (kemenyitett konstansok: LostCastle, Halloween2022,
  Pyramid, Rune, Viking, CombatSystem), `BattleArena.h` regen00.json,
  `wedding.cpp` npc.json; a quest lua-kban (aktiv `Quest/`, `pre_qc/`,
  `Quest non usati/`) 131 `.txt` -> `.json` csere.
- Paritas: 342 regen fajl txt vs JSON bejegyzes-szam egyezik (0 elteres).

### Map meta (E)

- Konverter: `SRC/Tools/txt2json/txt2json_map.py` (index/setting/town/
  dungeon mod).
- `map/index` -> `map/index.json` (`{"maps":[{index,name}]}`, 48 map);
  `input_db.cpp` az `index.json`-t tolti.
- `<map>/Setting.txt` -> `setting.json` (`cell_scale`, `map_size`,
  `base_position`; a tobbi KV - HeightScale/ViewRadius/TextureSet/
  Environment - nem kellett a loadernek);
- `<map>/Town.txt` -> `town.json` (`spawn`, opcionalis `empire_spawn`);
- `<map>/dungeon.txt` -> `dungeon.json` (`positions`; jelenleg egyik
  mapon sem letezik, de a loader tudja).
- `sectree_manager.cpp` Build/LoadSettingFile/LoadMapRegion/LoadDungeon
  JSON-ra atirva, a `Setting.txt`/`Town.txt`/`regen.txt`/`npc.txt`/
  `boss.txt`/`stone.txt`/`dungeon.txt` utvonalak `.json`-ra.
- Kimaradt (szandekosan): `map/map_config.txt` - az ENABLE_MAP_TELEPORTER
  nincs bekapcsolva es a regi kliens is olvassa; `MapProperty.txt` es
  `metin.txt` - nincs szerver olvaso.

### Adat rendezes

- Az elo `share/data`-ban 493 txt torolve (minden olyan, aminek van
  `.json` parja), 1.7 MB.
- `share/data/map_0/` (177 txt, 6.9 MB) es
  `share/locale/germany---------/` (45.8 MB backup) torolve - egyik sem
  olvasott (a `share/locale/*.tgz` backupok megmaradtak).
- Megtartva: gaya per-player allapotfajlok, whisper.txt, quest
  questnpc/questcategory (Phase 4), Database import / junk (Phase 5).

### Build

- GameServer Debug + Release zold.


## 8. Tipusos group/drop JSON (2026-10-13)

A Phase 1-ben letrehozott strukturális (token-tomb) JSON helyett a group/drop
tablak tipusos, nevesitett mezos JSON-t kaptak; a `CJsonGroupLoader` es a
token-alapu szerkezet teljesen megszunt.

### Kommentkezeles

A `//` a sor vegig tarto komment. A konverter a `//` elotti adatreszt tartja
meg (`400//metin szilank` -> 400, `5//Az elet vize` -> 5); ha egy token csak
komment volt, eltunik. Ez javitja azt a regi esetet is, amikor a komment
szokozzel volt leválasztva (a regi parser ilyenkor 0-t olvasott).

### Sémák (fajlonkent)

- `group.json`: `{name, vnum, leader:{name,vnum}, members:[{name,vnum}]}`
- `group_group.json`: `{name, vnum, members:[{vnum, prob}]}` (a prob a regi
  parserben is olvasva, de nem hasznalva)
- `mob_drop_item.json`: `{name, mob, type: kill|drop|limit|thiefgloves,
  kill_drop?, level_limit?, items:[{item, count, chance, rare?}]}`
  (kill: int chance + rare 0..100; drop/limit/thiefgloves: float chance)
- `drop_item_group.json`: `{name, vnum, mob, items:[{item, chance, count}]}`
- `special_item_group.json`: `{name, vnum, type?: pct|quest|special|attr,
  items:[{item, count, prob, rare?}]}` vagy attr eseten
  `{attrs:[{apply, value}], effect}`
- `common_drop_item.json`: `{name, rank, items:[{level_start, level_end,
  chance, item}]}`

### Szerver

- `mob_manager.cpp` (LoadGroup/LoadGroupGroup) es
  `item_manager_read_tables.cpp` (ReadCommonDropItemFile/ReadSpecialDropItemFile/
  ReadMonsterDropItemGroup/ReadDropItemGroup + a ConvSpecialDropItemFile GM
  dump) kozvetlenul nlohmann-nal olvassa a tipusos JSON-t; a regi
  validaciok (count >= 1, kill chance != 0, rare clamp, ismeretlen tipus ->
  hiba) valtozatlanok.
- A `#ifdef NEW_READ_COMMON_DROP_ITEM` halott (nyers fopen) aga torolve.
- A special_item_group regi korea nev-alias osszehasonlitasa torolve (az
  adatban csak "exp" fordul elo).
- `json_group_loader.{h,cpp}` torolve; a `TTokenVector` typedef a
  `json_group_tree.h`-ba kerult.

### Konverter pipeline

1. `txt2json_group.py <mode> <in.txt> <out.json>`: txt -> strukturális JSON
   (a tokenek kommentestul megmaradnak).
2. `txt2json_typed.py <mode> <in.json> <out.json>`: strukturális -> tipusos
   JSON (komment kivagas + mezok).

A fenti fajlok ujrageneralasahoz a ket lepes egyutt hasznalando (uj kliens
adat eseten). Az elo `share/data`-ban a tipusos JSON-ok vannak (group,
group_group, mob_drop_item, drop_item_group, special_item_group,
common_drop_item).

### Build

- GameServer Debug + Release zold; ShopLifecycleTests zold.


## 9. Kiterjesztes az osszes konvertalt fajlra (2026-10-13)

A tipusos kezeles (nevesitett mezok, `//` komment kivagas) kiterjedt a
maradek konvertalt fajlokra is.

### Named rows: battle_pass, dragon_soul_table, shop_table_ex

- Uj JSON forma: `{name, columns:[...], rows:{"1": {oszlop: ertek, ...}},
  groups:[...]}` - a korabbi `tokens: {"1": [ertek1, ertek2]}` helyett a
  sorok nevesitett oszlopokkal.
- A `CJsonGroupTreeLoader` (json_group_tree.cpp) az uj formatumot olvassa;
  a cellakat az oszlop-sorrend szerint vektorba rendezi, igy a fogyasztok
  (battle_pass.cpp, dragon_soul_table.cpp, shop_manager.cpp) valtozatlanok.
- `battle_pass.json` es `dragon_soul_table.json` ujragenerálva;
  `shop_table_ex.json` egyelore nem letezik (a shopok DB-bol mennek), de a
  betolto es a konverter kesz hozza.

### Regen

- `type` mostantol szemantikus: `mob`, `group`, `group_aggressive`
  (a regi "ga"), `exception`, `group_group` (regi "r"/"ra"), `anywhere`
  (regi "s").
- `time` egesz masodperc (a regi "10s"/"1h30m" forma a konverzio alatt
  szamolodik ki).
- `percent` int (a regi parser is olvasta, de nem hasznalta - megtartva).
- A `regen.cpp` a szemantikus tipust es a kesz masodpercet olvassa.

### Egyeb

- `ori_to_new_table.json`: `{"pairs": [{"ori": ..., "new": ...}]}` a
  korabbi [ori, new] parok helyett; a `ReadItemVnumMaskTable` ezt olvassa.
  (Az elo adat ures: `{"pairs": []}`.)
- `setting.json`: a teljes Setting.txt tartalom, nevesitett mezokkel
  (`script_type`, `cell_scale`, `height_scale`, `view_radius`, `map_size`,
  `base_position`, `texture_set`, `environment`). 69 mapnal a
  `share/locale/germany.tgz`-beli eredeti txt alapjan dusitva (csak akkor,
  ha a hasznalt mezok egyeztek), 6 mapnal (nincs tarball pelda) maradt a
  szukseges 3 mezo.
- `txt2json_map.py` setting modja is a teljes mezoket allitja elo.

### Validacio

- A `share/locale/germany.tgz` eredeti txt-ivel szemben a regen fajlok es
  a dragon_soul_table ujrafuttatott pipeline-eredmenye bitazonos a live
  JSON-nal; a tarball-beli battle_pass.txt regebbi (3 group a mostani 2
  helyett), ott a live a mervado.
- Regen tipus-eloszlas a live adatban: mob 13415, group 21228,
  group_aggressive 1650, group_group 11375, anywhere 1.
- GameServer Debug + Release es ShopLifecycleTests zold.

### Konverter

- `txt2json_typed.py` uj modok: `named_rows` (battle_pass/dragon_soul/
  shop_table_ex) es `regen`.
