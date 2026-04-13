#include <windows.h>
#include <tlhelp32.h>
#include <winhttp.h>
#include <filesystem>
#include <string>
#include <fstream>
#include <iostream>

#include "Update.h"

#pragma comment(lib, "winhttp.lib")

// ===================== Segédfüggvények =====================

std::wstring GetLauncherDir()
{
    wchar_t buf[MAX_PATH]{ 0 };
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return p.parent_path().wstring();
}

bool ReadAllText(const std::wstring& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;

    std::string tmp((std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
    out.swap(tmp);
    return true;
}

bool WriteAllText(const std::wstring& path, const std::string& data)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        return false;

    f.write(data.data(), data.size());
    return true;
}

std::string DownloadString(const std::wstring& host, const std::wstring& path)
{
    HINTERNET hSession = WinHttpOpen(L"LauncherUpdater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        NULL, NULL, 0);
    if (!hSession)
        return {};

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    BOOL b = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        NULL, 0, 0, 0);
    if (!b)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    WinHttpReceiveResponse(hRequest, NULL);

    DWORD dwSize = 0;
    std::string result;

    do
    {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || !dwSize)
            break;

        std::string buffer;
        buffer.resize(dwSize);

        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &downloaded) || !downloaded)
            break;

        result.append(buffer.data(), downloaded);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

bool DownloadFile(const std::wstring& host, const std::wstring& path, const std::wstring& saveAs)
{
    HINTERNET hSession = WinHttpOpen(L"LauncherUpdater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        NULL, NULL, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        NULL, 0, 0, 0))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpReceiveResponse(hRequest, NULL);

    HANDLE hFile = CreateFileW(saveAs.c_str(), GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwSize = 0;
    do
    {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || !dwSize)
            break;

        BYTE* buffer = new BYTE[dwSize];
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buffer, dwSize, &downloaded) || !downloaded)
        {
            delete[] buffer;
            break;
        }

        DWORD written = 0;
        WriteFile(hFile, buffer, downloaded, &written, NULL);
        delete[] buffer;
    } while (dwSize > 0);

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return true;
}

// Futó Patcher.exe lelövése (ha fut)
void KillProcessByName(const std::wstring& exeName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0)
            {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc)
                {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
}






std::string GetExeVersion(const std::wstring& exePath)
{
    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), &dummy);
    if (size == 0)
        return "";

    std::string buffer(size, 0);

    if (!GetFileVersionInfoW(exePath.c_str(), 0, size, buffer.data()))
        return "";

    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;

    if (!VerQueryValueW(buffer.data(), L"\\", (LPVOID*)&info, &len))
        return "";

    DWORD major = HIWORD(info->dwFileVersionMS);
    DWORD minor = LOWORD(info->dwFileVersionMS);
    DWORD build = HIWORD(info->dwFileVersionLS);
    DWORD revision = LOWORD(info->dwFileVersionLS);

    char out[64];
    sprintf_s(out, "%u.%u.%u.%u", major, minor, build, revision);
    return out;
}

std::wstring AnsiToWstring(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], size_needed);
    wstr.pop_back(); // remove extra '\0'
    return wstr;
}

// ===================== FŐ FUNKCIÓ: PATCHER FRISSÍTÉS =====================

bool CheckAndUpdatePatcher()
{
    std::wstring patcherPath2 = L"AurigaPatcher.exe";

    std::string version = GetExeVersion(patcherPath2);



    // --- konfiguráció ---
    const std::wstring host = L"bwmt2global.eu";        // <-- IDE a domain
    const std::wstring versionRemote = L"/PATCHER/SelfUpdate/Version.txt";  // szerver oldali verzió
    const std::wstring patcherRemote = L"/PATCHER/SelfUpdate/AurigaPatcher.exe";  // szerver oldali patcher exe
    const std::wstring patcherExeName = L"AurigaPatcher.exe";
    const std::wstring patcherNewName = L"Patcher_new.exe";
    const std::wstring patcherOldName = L"Patcher_old.exe";
    const std::wstring patcherVersionFile = AnsiToWstring(version);

    std::wstring baseDir = GetLauncherDir();

    const std::wstring localVersionPath = baseDir + L"\\" + patcherVersionFile;
    const std::wstring patcherPath = baseDir + L"\\" + patcherExeName;
    const std::wstring patcherNewPath = baseDir + L"\\" + patcherNewName;
    const std::wstring patcherOldPath = baseDir + L"\\" + patcherOldName;

    // Lokális verzió beolvasása (ha nincs, akkor "0")
    std::string localVersion = "0";
    ReadAllText(localVersionPath, localVersion);

    // Szerver verzió lekérdezése
    std::string serverVersion = DownloadString(host, versionRemote);
    if (serverVersion.empty())
    {
        MessageBoxA(NULL, "Nem sikerült lekérni a patcher verziót a szerverről.", "Launcher", MB_ICONERROR);
        return false;
    }

    // whitespace lecsupaszítása (CR/LF, space)
    auto trim = [](std::string& s)
        {
            while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
                s.pop_back();
            size_t i = 0;
            while (i < s.size() && (s[i] == '\r' || s[i] == '\n' || s[i] == ' '))
                ++i;
            if (i > 0) s.erase(0, i);
        };
    trim(localVersion);
    trim(serverVersion);

    if (localVersion == serverVersion)
    {
        // nincs frissítés, mehet tovább
        return true;
    }

    // Ha idáig jutottunk: FRISSÍTENI KELL
    std::string msg = "Új patcher verzió elérhető!\nJelenlegi: " + localVersion +
        "\nÚj: " + serverVersion + "\nLetöltés...";
    MessageBoxA(NULL, msg.c_str(), "Launcher", MB_OK);

    // Biztonságból lelőjük a futó patchert (ha fut)
    KillProcessByName(patcherExeName);

    // Új patcher letöltése
    if (!DownloadFile(host, patcherRemote, patcherNewPath))
    {
        MessageBoxA(NULL, "Nem sikerült letölteni az új patchert.", "Launcher", MB_ICONERROR);
        return false;
    }

    // Régi patcher átnevezése (ha létezik)
    try
    {
        if (std::filesystem::exists(patcherOldPath))
        {
            std::filesystem::remove(patcherOldPath);
        }

        if (std::filesystem::exists(patcherPath))
        {
            std::filesystem::rename(patcherPath, patcherOldPath);
        }

        std::filesystem::rename(patcherNewPath, patcherPath);
		std::filesystem::remove(patcherOldPath);
    }
    catch (const std::exception& e)
    {
        std::string emsg = "Fájlcsere hiba: ";
        emsg += e.what();
        MessageBoxA(NULL, emsg.c_str(), "Launcher", MB_ICONERROR);
        return false;
    }

    // Verziófájl frissítése
    WriteAllText(localVersionPath, serverVersion);

    MessageBoxA(NULL, "Patcher sikeresen frissítve.", "Launcher", MB_OK);
    return true;
}
