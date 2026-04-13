// AH_PackerGuard.h
// Themida / WinLicense / VMProtect és egyéb packer detektálás
// futó processzek PE szekció nevei alapján.
//
// Miért hatékony a HLBot ellen?
//   A HLBot Themida-val van védve. A Themida mindig hozzáad .themida és .boot
//   nevű szekciókat a PE-hez - ezeket a szekció neveket a protector a memóriában
//   is meghagyja, így ReadProcessMemory nélkül, a saját process modul listájából
//   is detektálható ha egy DLL vagy EXE be van töltve.
//
//   Az ÖSSZES futó processzt is végig lehet nézni CreateToolhelp32Snapshot +
//   ReadProcessMemory segítségével a PE fejlécük ellenőrzéséhez.
#pragma once

#include <string>

namespace AntiHook::PackerGuard
{
    // Elindítja a futó processzek packer-szekció scan-jét.
    // Ha Themida/WinLicense/VMProtect szekciót talál egy nem-whitelistelt
    // processben, logol és terminál.
    void ScanProcessesForPackers(const char* logFile);
}
