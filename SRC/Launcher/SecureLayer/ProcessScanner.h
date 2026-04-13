#pragma once

#include <vector>
#include <string>
#include <cstdint>


bool InitIxacIntegrityWatchdog();

// ---------------------------------------------------------------------------
// CRC Pair típusdefiníció
//   Elsõ: fájl CRC (tartalom alapú hash)
//   Második: fájl elérési út (UTF-8 string formában)
// ---------------------------------------------------------------------------
using CRCPair = std::pair<uint32_t, std::string>;

// ---------------------------------------------------------------------------
// ProcessScanner modul API
// ---------------------------------------------------------------------------
// Ez a modul felelõs a futó folyamatok és moduljaik (DLL/EXE) átvizsgálásáért.
// Minden modulhoz CRC-t számít, és egy biztonságos, thread-safe gyûjtõbe helyezi.
// A háttérben futó szál periodikusan újraszkenneli a rendszert.
//
// A modul automatikusan cache-eli a már ellenõrzött fájlok CRC-jét, így
// ismételt futások esetén nem nyitja meg újra ugyanazokat a fájlokat.
//
// Modern C++17 megvalósítás, RAII és atomikus vezérlés használatával.
//
// ---------------------------------------------------------------------------
// Függõségek:
//   - Windows.h, tlhelp32.h
//   - Base/CRC32.h  (GetCRC32, GetFileCRC32 függvények)
//   - CHddData.h    (ha szükséges más modulokhoz)
// ---------------------------------------------------------------------------
//
// Példa használat:
//
//   if (ProcessScanner_Create())
//   {
//       while (true) {
//           std::vector<CRCPair> results;
//           if (ProcessScanner_PopProcessQueue(&results)) {
//               for (auto& [crc, path] : results)
//                   printf("CRC: %08X | %s\n", crc, path.c_str());
//           }
//           Sleep(1000);
//       }
//       ProcessScanner_Destroy();
//   }
//
// ---------------------------------------------------------------------------


// Inicializálja a scanner modult és elindítja a háttérszálat.
// Visszatérési érték: true, ha sikerült a szálat létrehozni.
bool ProcessScanner_Create();

// Kérés a háttérszál leállítására (nem blokkoló).
void ProcessScanner_ReleaseQuitEvent();

// Leállítja a szálat, megvárja a befejezést, majd felszabadít minden erõforrást.
void ProcessScanner_Destroy();

// Átveszi a legutóbbi szkennelés eredményét egy vektorba.
// A függvény törli a belsõ queue-t.
// Visszatérési érték: true, ha volt új adat.
bool ProcessScanner_PopProcessQueue(std::vector<CRCPair>* outPairs);

