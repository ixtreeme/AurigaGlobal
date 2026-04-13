#include "LauncherGuard.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <algorithm>
#include <cwctype>

#include <sodium.h>

#include "obfuscate.h"

#pragma comment(lib, "psapi.lib")


namespace
{
    constexpr size_t HASH_LEN = crypto_generichash_blake2b_BYTES_MAX;

    static const wchar_t* GetExpectedPatcherName()
    {
        static constexpr wchar_t kName[] = L"AurigaPatcher.exe";
        return kName;
    }

    static const unsigned char kHashXorKeyPart1[HASH_LEN / 2] =
    {
        0x7B, 0x1D, 0x7C, 0x88, 0xAC, 0xA3, 0x83, 0x49,
		0x75, 0x23, 0x20, 0x8C, 0xDF, 0x09, 0xDF, 0x63,
		0x60, 0xED, 0xC2, 0x08, 0x82, 0x5A, 0x2B, 0x92,
		0x40, 0x33, 0x3E, 0x8F, 0xBD, 0x2E, 0xA6, 0xC0
    };

    static const unsigned char kHashXorKeyPart2[HASH_LEN / 2] =
    {
        0x89, 0x26, 0x12, 0xDD, 0x0C, 0x01, 0xE3, 0xE4,
		0xC3, 0xD4, 0x9D, 0x58, 0x48, 0x15, 0x39, 0x9C,
		0xF0, 0x9D, 0x25, 0xEE, 0x00, 0xEC, 0xEA, 0xC1,
		0x65, 0x62, 0xE2, 0x9C, 0xEE, 0x48, 0xB7, 0x63
    };

    static const unsigned char kHashObfPart1[HASH_LEN / 2] =
    {
        0x11, 0xCD, 0xC1, 0x84, 0x5A, 0xB6, 0x03, 0x67,
		0x4D, 0x82, 0x86, 0xB6, 0xE3, 0xC6, 0x50, 0x42,
		0x3C, 0x94, 0xE6, 0xDC, 0x1B, 0xBF, 0x92, 0xC7,
		0x9B, 0xD6, 0x60, 0xCB, 0x40, 0xBA, 0x3F, 0x2D
    };

    static const unsigned char kHashObfPart2[HASH_LEN / 2] =
    {
        0x3E, 0xAF, 0x35, 0xE4, 0x9E, 0xC8, 0xB3, 0x9C,
		0xDC, 0x6A, 0xCE, 0x6F, 0x17, 0x6B, 0xFF, 0xDE,
		0xD8, 0x18, 0xB2, 0x76, 0xF1, 0x9D, 0xA4, 0x13,
		0xDC, 0x32, 0xFD, 0x30, 0x61, 0xA1, 0xCD, 0x03
    };

    static void LG_GetExpectedPatcherHash(unsigned char out[HASH_LEN])
    {
        for (size_t i = 0; i < HASH_LEN / 2; ++i)
            out[i] = static_cast<unsigned char>(kHashObfPart1[i] ^ kHashXorKeyPart1[i]);

        for (size_t i = 0; i < HASH_LEN / 2; ++i)
            out[i + HASH_LEN / 2] = static_cast<unsigned char>(kHashObfPart2[i] ^ kHashXorKeyPart2[i]);
    }

    [[noreturn]] void ShowErrorAndExit(const wchar_t* msg)
    {
        MessageBoxW(nullptr, msg, AY_OBFUSCATE(L"Auriga - Indítási hiba"), MB_ICONERROR | MB_OK);
        ExitProcess(0);
    }

    std::wstring ToLower(const std::wstring& s)
    {
        std::wstring out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return out;
    }

    DWORD GetParentProcessId(DWORD pid)
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        if (!Process32FirstW(hSnap, &pe))
        {
            CloseHandle(hSnap);
            return 0;
        }

        DWORD parentPid = 0;

        do
        {
            if (pe.th32ProcessID == pid)
            {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));

        CloseHandle(hSnap);
        return parentPid;
    }

    std::wstring GetProcessImagePath(DWORD pid)
    {
        std::wstring result;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProc)
            return result;

        wchar_t buffer[MAX_PATH] = { 0 };
        DWORD size = MAX_PATH;

        if (GetModuleFileNameExW(hProc, nullptr, buffer, size) != 0)
        {
            result.assign(buffer);
        }

        CloseHandle(hProc);
        return result;
    }

    bool ComputeFileHash(const std::wstring& path, unsigned char out[HASH_LEN])
    {
        if (sodium_init() == -1)
            return false;

        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f)
            return false;

        crypto_generichash_state st;
        crypto_generichash_init(&st, nullptr, 0, HASH_LEN);

        unsigned char buf[64 * 1024];
        size_t readBytes = 0;
        while ((readBytes = fread(buf, 1, sizeof(buf), f)) > 0)
        {
            crypto_generichash_update(&st, buf, readBytes);
        }

        fclose(f);

        crypto_generichash_final(&st, out, HASH_LEN);
        return true;
    }

    bool CompareHash(const unsigned char* a, const unsigned char* b, size_t len)
    {
        unsigned char diff = 0;
        for (size_t i = 0; i < len; ++i)
            diff |= static_cast<unsigned char>(a[i] ^ b[i]);
        return diff == 0;
    }

}


__declspec(noinline)
bool LG_EnsureLaunchedFromPatcher()
{
    const DWORD currentPid = GetCurrentProcessId();
    const DWORD parentPid = GetParentProcessId(currentPid);

    if (parentPid == 0 || parentPid == currentPid)
    {
        ShowErrorAndExit(AY_OBFUSCATE(L"A játékot kizárólag a hivatalos patcheren keresztül lehet indítani. (Parent PID hiba)"));
    }

    std::wstring parentPath = GetProcessImagePath(parentPid);
    if (parentPath.empty())
    {
        ShowErrorAndExit(AY_OBFUSCATE(L"A játékot kizárólag a hivatalos patcheren keresztül lehet indítani. (Parent path hiba)"));
    }

    std::wstring fileName = parentPath;
    const size_t pos = parentPath.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
        fileName = parentPath.substr(pos + 1);

    std::wstring parentLower = ToLower(fileName);
    std::wstring expectedLower = ToLower(GetExpectedPatcherName());

    if (parentLower != expectedLower)
    {
        ShowErrorAndExit(AY_OBFUSCATE(L"A játékot kizárólag a hivatalos patcheren keresztül lehet indítani. (Nem a patcher indította)"));
    }

    unsigned char hash[HASH_LEN] = { 0 };
    unsigned char expected[HASH_LEN] = { 0 };

    if (!ComputeFileHash(parentPath, hash))
    {
        SecureZeroMemory(hash, HASH_LEN);
        ShowErrorAndExit(AY_OBFUSCATE(L"A patcher hitelesítése sikertelen (hash hiba)."));
    }

    LG_GetExpectedPatcherHash(expected);

    const bool ok = CompareHash(hash, expected, HASH_LEN);

    SecureZeroMemory(hash, HASH_LEN);
    SecureZeroMemory(expected, HASH_LEN);

    if (!ok)
    {
        ShowErrorAndExit(AY_OBFUSCATE(L"A patcher hitelesítése sikertelen (ismeretlen indító)."));
    }

    return true;
}
