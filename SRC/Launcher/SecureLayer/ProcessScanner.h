#pragma once

#include <vector>
#include <string>
#include <cstdint>


bool InitIxacIntegrityWatchdog();

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
using CRCPair = std::pair<uint32_t, std::string>;

// ---------------------------------------------------------------------------
// ProcessScanner modul API
// ---------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------
//   - Windows.h, tlhelp32.h
// ---------------------------------------------------------------------------
//
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


bool ProcessScanner_Create();

void ProcessScanner_ReleaseQuitEvent();

void ProcessScanner_Destroy();

bool ProcessScanner_PopProcessQueue(std::vector<CRCPair>* outPairs);

