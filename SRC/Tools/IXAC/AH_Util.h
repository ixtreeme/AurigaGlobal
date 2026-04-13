#pragma once
#include <strsafe.h>

#include "../../Launcher/SecureLayer/obfuscate.h"


static const char* LOG_FILE = AY_OBFUSCATE("UserData/AC/_log.txt");

void AppendLog(const char* fmt, ...);
DWORD FindExternalThreadOwner();
DWORD FindWhoOpenedMyProcess();
DWORD FindProcessOwningMemory(uintptr_t base);
bool FindSuspiciousModuleInside(DWORD pid, std::string& outPath);
bool HasManualMapRegion(DWORD pid);


void LogCheatFingerprint(DWORD pid);
