#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

#include "../../Launcher/SecureLayer/obfuscate.h"

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

typedef LONG NTSTATUS;
inline HMODULE g_hDll = nullptr;


// ===== NTDLL Structs =====
typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemHandleInformation = 16
} SYSTEM_INFORMATION_CLASS;

typedef enum _THREADINFOCLASS
{
    ThreadBasicInformation = 0,
    ThreadQuerySetWin32StartAddress = 9
} THREADINFOCLASS;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
    ULONG       ProcessId;
    UCHAR       ObjectTypeNumber;
    UCHAR       Flags;
    USHORT      Handle;
    PVOID       Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
    ULONG HandleCount;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

// ==== NT function typedef =====
using NtQuerySystemInformation_t = NTSTATUS(NTAPI*)(
    SYSTEM_INFORMATION_CLASS,
    PVOID,
    ULONG,
    PULONG
    );

using NtQueryInformationThread_t = NTSTATUS(NTAPI*)(
    HANDLE,
    THREADINFOCLASS,
    PVOID,
    ULONG,
    PULONG
    );

// ========================================================================
// =======================  AntiHook::Core  ================================
// ========================================================================

bool IXAC_ReportCheat();


namespace AntiHook::Core
{
    // ===== FNV-1a hash =====
    uint32_t Fnv1a(const uint8_t* data, size_t len);

    // ===== .text section info =====
    struct SectionInfo
    {
        uint8_t* base;
        size_t   size;
    };

    const SectionInfo& GetTextSection();
    void SetTextSection(const SectionInfo& s);

    uint32_t GetTextExpectedHash();
    void SetTextExpectedHash(uint32_t h);

    // ===== Module info =====
    struct ModuleInfo
    {
        uintptr_t base;
        uintptr_t end;
        std::wstring name;
        std::wstring path;
    };

    std::wstring ToLower(const std::wstring& in);
    bool IsModuleWhitelisted(const std::wstring& name);

    std::wstring GetProcessImageName(DWORD pid);

    std::vector<ModuleInfo> BuildModuleMap();
    void CheckLoadedModules();

    // ===== NT Resolving =====
    bool ResolveNt();
    bool ResolveNtThread();

    NtQuerySystemInformation_t GetNtQuerySystemInformation();
    NtQueryInformationThread_t GetNtQueryInformationThread();
}
