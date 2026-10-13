# C-stilusu fajlkezeles + kezi memoria -> modern C++23 RAII (audit)

Statusz: TERV (2026-10-13). A projekt C++23-at hasznal (CMAKE_CXX_STANDARD 23),
a fmt es az std::filesystem elerheto. Cel: a megmaradt `FILE*`/fopen/fclose
alapu fajlkezeles es a hozza tartozo kezi buffer-memoria kivaltasa RAII-ra.

## Konvenciok

- Szoveges/strukturalt fajlok: `std::ifstream` / `std::ofstream`
  (`std::getline`, `std::istringstream` a tokenekhez).
- Ahol C-API kell (pl. crash handler, fscanf-os legacy parolas):
  `auriga::FilePtr` (unique_ptr<FILE, FileCloser>) a `core/FileHandle.h`-bol.
- Utvonal/epites: `std::filesystem` + `fmt::format` (nem `snprintf` + `char[256]`).
- Letezes-ellenorzes: `auriga::FileExists` (std::filesystem, hibakod, nem dob).
- Binaris betoltes: `std::vector<std::byte>`/`std::unique_ptr<T[]>` a
  `M2_NEW ... [n]` + kezi `M2_DELETE` helyett.
- Nem cel: a motor teljes `M2_NEW`/`M2_DELETE` (debug allocator) atirasa es
  az event/regen/char objektumok ownership-je - az kulon, nagy refaktor.
  Itt csak a fajlkezelo kodban levo kezi foglalasokat valtjuk ki.

## Inventory (fajlkezeles)

### A fazis - runtime allapotfajlok + egyszeru kiiratok (KESZ)

| Fajl | Fajl / mit csinal | Csere |
|---|---|---|
| ecs/systems/GayaSystem.cpp | gaya/<nev>_gaya_system.txt: CheckItemsFull (letezes), RefreshItemsMarket/UpdateSlot ("w" iras), UpdateItems0 (olvasas+strtok), UpdateItems ("a") | ofstream/ifstream + fmt path, nincs char[257] |
| social/shop_manager.cpp | ReadShopTableEx: csak letezes-ellenorzes fopen-nal | FileExists |
| entity/mob_manager.cpp | DumpRegenCount: mob_count tabla iras | ofstream + fmt |
| item/item_manager_idrange.cpp | touch(): .killscript letrehozas (append) | ofstream app |
| network/packet_info.cpp | Log(): packet statisztika iras | ofstream + fmt |
| core/profiler.h | Log()/Print(FILE*): profiler riport | ofstream / std::ostream& |
| core/crash_report.cpp | crash.txt iras kozben (FilePtr, nem iostream - crash-safe) | FilePtr |
| Database/ClientManager.cpp | UsageLog(): usage.txt append | ofstream + fmt |

### B fazis

| Fajl | Fajl / mit csinal | Csere |
|---|---|---|
| command/whisper_admin.cpp | whisper.txt: GetLang/GetColor olvasas, SaveConfig iras | ifstream mar van -> ofstream, char[256] kivezetes |
| network/ip_ban.cpp | BANIP: betoltes (fgets+sscanf), append, ReadBannedIP | FilePtr (parolas) / ofstream |
| item/item_manager.cpp | lotto.txt: load_lotto (olvasas), lotto() (append) | ifstream / ofstream |
| core/config.cpp | CRC (fscanf), VERSION (fgets), state_user_count (fscanf), __LoadDefaultCMDFile (fgets), map_config funcTakeLines (feature off) | FilePtr / ifstream + fmt |
| network/desc.h/.cpp | m_pLogFile (FILE* tag, sehol nem nyilik - halott) | std::ofstream tag + Log() |

### C fazis

| Fajl | Fajl / mit csinal | Csere |
|---|---|---|
| guild/MarkManager.cpp | guild mark binaris fajlok (fopen/fread/fwrite x4) | FilePtr |
| guild/MarkConvert.cpp | regi mark adat konverzio (x2) | FilePtr |
| world/sectree_manager.cpp | LoadAttribute: server_attr binaris betoltes + M2_NEW uint8_t[]/uint32_t[] puffer + abort utani kezi felszabaditasok | FilePtr + std::vector, RAII felszabaditas |
| core/empire_text_convert.cpp | langX.cvt olvasas (fopen/fread) | FilePtr + std::vector |
| item/item_manager_read_tables.cpp | ConvSpecialDropItemFile: debug dump iras (fprintf) | ofstream + fmt |

## Megjegyzesek

- A `DESC::Log` es a profiler `Log/Print` API-t megtartjuk (hivasok miatt),
  csak a belso implementacio valt.
- A crash handlerben szandekosan nem iostream: a crash kontextusban a
  FilePtr (unique_ptr + fclose) a biztonsagos valasztas.
- A gaya per-player fajlok es a whisper/lotto tovabbra is szoveges
  allapotfajlok (nem JSON) - itt csak a kezeles modja valtozik.


## Statusz (2026-10-13): mindharom fazis KESZ

- **A+B fazis** (commit 287f32a0): `core/FileHandle.h` (FilePtr RAII +
  OpenFile + FileExists) letrejott; atirva ofstream/ifstream + fmt-re:
  GayaSystem (5 hely: letezes, 2x "w" iras, olvasas+parolas, "a"),
  shop_manager letezes, mob_manager DumpRegenCount, item_manager_idrange
  touch, packet_info Log, profiler Log/Print, crash_report (FilePtr,
  crash-safe), whisper_admin, ip_ban (BANIP olvasas/append/tool main),
  item_manager lotto.txt, config.cpp (CRC, VERSION, state_user_count,
  CMD fajl, map_config funcTakeLines), desc.h/desc.cpp m_pLogFile
  (halott FILE* tag -> std::ofstream), Database UsageLog (usage.txt).
- **C fazis**: MarkManager (mark index + binaris symbol load/save),
  MarkConvert (legacy mark konverzio, std::filesystem),
  sectree_manager LoadAttribute (FilePtr + std::vector<uint8_t/uint32_t>
  a M2_NEW tombok + kezi felszabaditasok helyett),
  empire_text_convert (FilePtr), ConvSpecialDropItemFile (ofstream).
- Vegallapot: a GameServer/Database/LoginServer kodban **nincs tobb
  fopen/fclose** (kiveve maga a FileHandle helper). A kezi `char[256]`
  utvonal-epitesek helyett `fmt::format` + `std::string`.
- A nem fajlkezeleshez tartozo motor-szintu `M2_NEW`/`M2_DELETE` es
  poolok atirasa szandekosan kimaradt (kulon, nagy refaktor; itt csak a
  fajlkezelo kod kezi foglalasai szuntek meg).
- Build: GameServer Debug + Release, Database Debug + Release,
  ShopLifecycleTests zold.
- Megjegyzes: az `empire_text_convert.cpp` legacy (nem UTF-8) karakteres
  literalt tartalmaz; a patchelest byte-szinten kellett elvegezni, hogy
  a litera ne séruljön.
- A `DESC::Log` es a profiler `Log`/`Print` API valtozatlan (hivasok
  miatt); a profiler `Print` mostantol `std::ostream&`-re ir.
