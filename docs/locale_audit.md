# Locale rendszer audit (mi kell meg, mi maradek)

Kerdes: a regi locale-rendszer arra szolgalt, hogy ugyanaz a game binar
tobb nyelven fusson (mas adatkonyvtar, mas charset, mas nevvalidacio). A
multilang mostanra a kliensben van, ezert at kell nezni, mi kell belole.

Forras: `SRC/Server` + a live `srv1` adatkonyvtarak.

## 1. Jelenlegi mukodes (mibol jon a locale)

GameServer (`core/config.cpp`):
- Indulaskor a **common DB `locale` tablabol** olvassa a `LOCALE` sort:
  `SELECT mKey, mValue FROM locale` -> `LocaleService_Init(value)`.
- `LocaleService_Init("germany")` beallitja:
  - `g_stLocale` = MySQL charset (`latin1`),
  - `g_stServiceBasePath` = `locale/germany` (minden data file innen),
  - `g_stQuestDir` / `g_setQuestObjectDir`,
  - `check_name` / `is_twobyte` nevre szabott validaciot,
  - `PK_PROTECT_LEVEL` alapot (germany=15),
  - `g_eLocalType` / `LC_*` (lasd 4. pont: halott).
- A `locale` tabla tovabbi sorai nem nyelviek: `SKILL_POWER_BY_LEVEL*`
  (skill power tablak) - ezek **kellenek**.

Database (`Database/ClientManager.cpp:3650-4130`):
- `InitializeLocalization()` szinten a `locale` tabla `LOCALE` sorat olvassa,
  es **felulirja** a conf.json erteket:
  - `germany` -> `g_stLocale="latin1"`, `g_stLocaleNameColumn="locale_name"`
  - `hungary/czech/poland/romania` -> `latin2` / `locale_name`
  - `ymir` -> `euckr` / `name`, `japan` -> `sjis` / `locale_name`, stb.
- Tehat a `db/conf.json` `locale`/`name_column` ertekeket a DB LOCALE sor
  donti el; a conf csak addig szamit, amig nincs LOCALE sor.
- `g_stLocaleNameColumn` a `item_proto` / `mob_proto` nevkolumnat valasztja
  (a betolto queryk `%s`-sel epitik be; `ClientManagerBoot.cpp`).

LoginServer: a `locale` csak MySQL charset a configbol; nincs DB feluliras.

## 2. Ami ma tenylegesen kell

| Elem | Miert kell | Mostani forras |
|---|---|---|
| MySQL charset (`g_stLocale`) | nev/chat/string tarolas a DB-ben, helyes kodolas | DB LOCALE sor (Database+GameServer), Login conf |
| Adat eleresi ut (`LocaleService_GetBasePath`) | mob group/drop/quest/shop/fishing/cube/blend/battle_pass/gaya/map config mind innen jon | DB LOCALE sor -> `locale/<nev>` |
| `check_name` / `is_twobyte` | karakter/pet/ceh nev validacio | LocaleService init |
| `locale` tabla `SKILL_POWER_BY_LEVEL*` sorai | skill power tablak betoltese | DB (common) |
| `PK_PROTECT_LEVEL` alap | csak ha nincs `pk_protect_level` a confban | LocaleService init |
| `name_column` | proto nevek (log/quest/ellenorzes) | DB LOCALE sor (live: `locale_name`) |

## 3. Ami a szerveroldali nyelvi rendszer maradeka

Ezek nem a charset/path miatt kellenek, hanem a regi "tobb nyelv egy
binarisban" modell miatt; a kliens-oldali multilanggal reszben feleslegesek:

1. **DB proto nevkolumnak** (`locale_name`, `gb2312name`, `locale_name_<lang>`
   az `ENABLE_MULTI_NAMES` alatt) + a Database LOCALE -> name_column mapping.
   - Kliens oldali nevekhez nem kell; a szerver log/quest szovegekhez meg
     hasznalhato. Atallni `"name"`-re csak adatellenorzessel szabad.
2. **`LocaleService_Init` 30 nyelvi aga** - ugyanaz a logika 30x; egy
   parameterezett init (path + charset + name rule) kivaltja.
3. **`empire_text_convert`** (`lang1..3.cvt`): a CJK birodalmak kozotti
   karakterkodolas-atirasra valo; latin1-nel nincsenek `.cvt` fajlok ->
   gyakorlatilag no-op. Eltavolithato.
4. **Szerveroldali Lua szotarak**: `translate.lua` (`gameforge` tabla) es
   `quest/locale.lua` (`locale.GM_SKILL_NAME_DICT`) - **ezeket a live
   questek hasznaljak**, tehat tartalom, nem a locale-service resze.
   Eltavolitasuk csak a questek atirasa utan biztonsagos.
5. `ENABLE_MULTI_LANGUAGE` / `ENABLE_MULTI_NAMES` define-ok aktivak; a
   `translate/<lang>/translate.lua` tobbnyelvi betolto blokk viszont
   `ENABLE_MULTI_LANGUAGE_SYSTEM` alatt van, ami **nincs definialva** ->
   halott kod.

## 4. Bizonyitottan halott kod (kivezethető)

- `g_stLocaleFilename` (`locale_string.txt`): csak irva van, olvaso nincs.
- `g_iUseLocale`: csak irva van, olvaso nincs.
- `LC_GetLocalType`, `LC_Is*`, `g_eLocalType`: csak deklaracio/beallitas,
  hivas nincs a szerverben.
- `EMPIRE_LANGUAGE_CHECK` mar confbol megy; a `lang*.cvt` fajlok hianya
  miatt a `ConvertEmpireText` gyakorlatilag hatastalan latin locale-n.
- GameServer `-n <locale>` CLI argumentum: a `conf.json` atallas ota
  figyelmen kivul (logolva).

## 5. Kovetkeztetes a mostani conf.json-okra

- `db/conf.json` `"locale": "latin1"` **jelenleg nem e ervényes**, mert a
  Database felulirja a common DB `locale` tabla `LOCALE` sorabol.
- `auth/conf.json` `"locale": "latin1"` ervenyes (LoginServer nem olvassa a
  locale tablat).
- GameServer `conf.json`-ban nincs locale kulcs: a charsetet es az adat-utat
  a DB LOCALE sor adja (`locale/germany` es `latin1`, ha a sor "germany").

Ellenorzes a live DB-n (egy lekerdezes):
`SELECT mKey, mValue FROM common.locale;`  (a `LOCALE` sor erteke mondja meg
a tenyleges charsetet es name kolumnat).

## 6. Javasolt celallapot (single-locale, conf-vezérelt)

1. GameServer `conf.json` schema bovitese:
   - `"server": { "locale": "latin1", "locale_service": "germany" }`
     (charset + adatkonyvtar; utobbi a `locale/<nev>` pathhoz).
2. `config_init`: a DB `LOCALE` sor helyett a conf ertekeit hasznalja:
   `LocaleService_Init(cfg.server.localeService)`, majd `g_stLocale` a
   confbol (a DB charset-felulirast ki kell venni a GameServer oldalon is,
   mert ott is a locale tablabol jon).
3. Database: az `InitializeLocalization` LOCALE-sor charset/name_column
   felulirasanak torlese; helyette a conf `server.locale` +
   `server.name_column` (live ertek: `locale_name`, amig az adat ilyen).
4. LoginServer: valtozatlan.
5. Fazis 2 (cleanup): halott szimbolumok + empire cvt + 30 nyelvi ag
   osszevonasa.
6. Fazis 3 (opcionalis, adat-ellenorzessel): `name_column` -> `"name"`,
   `ENABLE_MULTI_NAMES` proto kolumnak es a `locale_name_*` mirror write
   elhagyasa.

Ezzel a szerver egy nyelven fut, a charset/nev/ut a conf.json-bol jon, a
`locale` tabla pedig mar csak a SKILL_POWER sorok miatt kell.

## 7. Kockazatok / nyitott kerdesek

- A live `locale/germany` konyvtar neve es a DB `LOCALE` sor erteke
  egyezik-e (ha a sor pl. "hungary", akkor latin2 + mas nevek jelenleg).
- A `mob_proto`/`item_proto` `locale_name` kolumna tartalma (ha ures,
  `name`-re kell valtani).
- A questek `gameforge[...]`/`locale.*` hasznalata: tartalom, nem
  tavolithato el a locale-refaktorral egyutt.

## 8. Implementalt kivaltas (2026-10-12)

- GameConfig: uj server.locale (charset, pl. "latin1") es
  server.locale_service (pl. "germany") kulcs.
- GameServer config.cpp: ha a locale_service meg van adva, a
  LocaleService_Init ezzel fut, es a locale tabla LOCALE sorat **nem
  olvassa**; ha nincs, a regi DB-sor logika marad (fallback). A locale
  kulcs felulirja a g_stLocale charsetet a service init utan.
- Database ClientManager::InitializeLocalization: a 341 soros
  LOCALE-sor -> charset/name-column tabla **torolve**; a charset es a
  name_column a conf.json-bol jon. A DB_NAME_COLUMN sor explicit
  feluliraskent meg marad.
- Live conf.json-ok frissitve: core-okban
  "locale": "latin1", "locale_service": "germany"; db/conf.json
  "name_column": "locale_name" (a jelenlegi effektiv ertek megorzese).
- A locale/germany adatkonyvtar es a Quest-Lua szotarak valtozatlanok.

Hatralevo (kulon fazis): holt szimbolumok (g_stLocaleFilename,
g_iUseLocale, LC_*), a 30 nyelvi init-ag osszevonasa, empire cvt es
ENABLE_MULTI_NAMES kivezetes.