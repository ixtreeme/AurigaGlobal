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
