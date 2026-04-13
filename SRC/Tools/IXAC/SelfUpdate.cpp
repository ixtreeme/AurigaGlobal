#include "SelfUpdate.h"
#include "BuildInfo.h"
#include "UpdatePopup.h"

#include "resource.h"

#include <windows.h>
#include <winhttp.h>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

#include "../../Launcher/SecureLayer/obfuscate.h"

#pragma comment(lib, "winhttp.lib")

extern HMODULE g_hDll;

// -----------------------------------------------------------------------
// Segédfüggvények
// -----------------------------------------------------------------------

static std::wstring GetDllDir()
{
    wchar_t buf[MAX_PATH]{ 0 };
    GetModuleFileNameW(g_hDll, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return p.parent_path().wstring();
}

static std::string TrimCopy(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == '\r' || s[i] == '\n' || s[i] == ' '))
        ++i;
    if (i > 0) s.erase(0, i);
    return s;
}

// -----------------------------------------------------------------------
// SHA256 – saját implementáció, nem hív semmilyen kriptográfiai API-t
// -----------------------------------------------------------------------

static const uint32_t kShaK[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

struct Sha256Ctx
{
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[64];
};

static inline uint32_t Rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void Sha256Transform(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t(block[i*4])<<24)|(uint32_t(block[i*4+1])<<16)
              |(uint32_t(block[i*4+2])<<8)|uint32_t(block[i*4+3]);
    for (int i = 16; i < 64; ++i)
    {
        uint32_t s0 = Rotr32(w[i-15],7)^Rotr32(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = Rotr32(w[i-2],17)^Rotr32(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a=state[0],b=state[1],c=state[2],d=state[3],
             e=state[4],f=state[5],g=state[6],h=state[7];

    for (int i = 0; i < 64; ++i)
    {
        uint32_t S1  = Rotr32(e,6)^Rotr32(e,11)^Rotr32(e,25);
        uint32_t ch  = (e&f)^(~e&g);
        uint32_t t1  = h + S1 + ch + kShaK[i] + w[i];
        uint32_t S0  = Rotr32(a,2)^Rotr32(a,13)^Rotr32(a,22);
        uint32_t maj = (a&b)^(a&c)^(b&c);
        uint32_t t2  = S0 + maj;

        h=g; g=f; f=e; e=d+t1;
        d=c; c=b; b=a; a=t1+t2;
    }

    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static void Sha256Init(Sha256Ctx& ctx)
{
    ctx.state[0]=0x6a09e667; ctx.state[1]=0xbb67ae85;
    ctx.state[2]=0x3c6ef372; ctx.state[3]=0xa54ff53a;
    ctx.state[4]=0x510e527f; ctx.state[5]=0x9b05688c;
    ctx.state[6]=0x1f83d9ab; ctx.state[7]=0x5be0cd19;
    ctx.count = 0;
}

static void Sha256Update(Sha256Ctx& ctx, const uint8_t* data, size_t len)
{
    size_t bufUsed = size_t(ctx.count) & 63;
    ctx.count += len;

    if (bufUsed)
    {
        size_t need = 64 - bufUsed;
        if (len < need)
        {
            memcpy(ctx.buf + bufUsed, data, len);
            return;
        }
        memcpy(ctx.buf + bufUsed, data, need);
        Sha256Transform(ctx.state, ctx.buf);
        data += need;
        len  -= need;
    }

    while (len >= 64)
    {
        Sha256Transform(ctx.state, data);
        data += 64;
        len  -= 64;
    }

    if (len)
        memcpy(ctx.buf, data, len);
}

static void Sha256Final(Sha256Ctx& ctx, uint8_t digest[32])
{
    uint64_t bitCount = ctx.count * 8;
    uint8_t pad = 0x80;
    Sha256Update(ctx, &pad, 1);

    uint8_t zero = 0;
    while ((ctx.count & 63) != 56)
        Sha256Update(ctx, &zero, 1);

    uint8_t len64[8];
    for (int i = 7; i >= 0; --i) { len64[i] = uint8_t(bitCount); bitCount >>= 8; }
    Sha256Update(ctx, len64, 8);

    for (int i = 0; i < 8; ++i)
    {
        digest[i*4+0] = uint8_t(ctx.state[i] >> 24);
        digest[i*4+1] = uint8_t(ctx.state[i] >> 16);
        digest[i*4+2] = uint8_t(ctx.state[i] >> 8);
        digest[i*4+3] = uint8_t(ctx.state[i]);
    }
}

// Fájl SHA256 hash kiszámítása – csak ReadFile, semmi CryptoAPI
static std::string Sha256OfFile(const std::wstring& filePath)
{
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return {};

    Sha256Ctx ctx;
    Sha256Init(ctx);

    uint8_t block[65536];
    DWORD bytesRead = 0;
    while (ReadFile(hFile, block, sizeof(block), &bytesRead, NULL) && bytesRead > 0)
        Sha256Update(ctx, block, bytesRead);

    CloseHandle(hFile);

    uint8_t digest[32];
    Sha256Final(ctx, digest);

    static const char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (int i = 0; i < 32; ++i)
    {
        result += kHex[(digest[i] >> 4) & 0xF];
        result += kHex[digest[i] & 0xF];
    }
    return result;
}

static bool VerifyFileHash(const std::wstring& filePath, const std::string& expectedHex)
{
    std::string actual = Sha256OfFile(filePath);
    if (actual.empty()) { DeleteFileW(filePath.c_str()); return false; }

    std::string exp = expectedHex;
    for (auto& c : exp) if (c >= 'A' && c <= 'F') c = char(c - 'A' + 'a');

    if (actual != exp) { DeleteFileW(filePath.c_str()); return false; }
    return true;
}

// -----------------------------------------------------------------------
// HTTPS helper
// -----------------------------------------------------------------------

struct WinHttpHandles
{
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    ~WinHttpHandles()
    {
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
    }
};

static bool OpenHttpsRequest(WinHttpHandles& h,
                              const std::wstring& host,
                              const std::wstring& path)
{
    h.hSession = WinHttpOpen(AY_OBFUSCATE(L"IXAC-Updater/1.0"),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h.hSession) return false;

    h.hConnect = WinHttpConnect(h.hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!h.hConnect) return false;

    h.hRequest = WinHttpOpenRequest(h.hConnect, AY_OBFUSCATE(L"GET"),
        path.c_str(), NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!h.hRequest) return false;

    DWORD secProto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
                   | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(h.hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
        &secProto, sizeof(secProto));

    DWORD secFlags = 0;
    WinHttpSetOption(h.hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
        &secFlags, sizeof(secFlags));

    if (!WinHttpSendRequest(h.hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0)) return false;
    if (!WinHttpReceiveResponse(h.hRequest, NULL)) return false;

    DWORD status = 0, statusSz = sizeof(status);
    WinHttpQueryHeaders(h.hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSz,
        WINHTTP_NO_HEADER_INDEX);
    if (status != 200) return false;

    return true;
}

static std::string DownloadString(const std::wstring& host, const std::wstring& path)
{
    WinHttpHandles h;
    if (!OpenHttpsRequest(h, host, path)) return {};

    std::string result;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(h.hRequest, &dwSize) || !dwSize) break;
        std::string buf(dwSize, '\0');
        DWORD got = 0;
        if (!WinHttpReadData(h.hRequest, buf.data(), dwSize, &got) || !got) break;
        result.append(buf.data(), got);
    } while (dwSize > 0);
    return result;
}

static bool DownloadFile(const std::wstring& host, const std::wstring& path,
                          const std::wstring& saveAs)
{
    WinHttpHandles h;
    if (!OpenHttpsRequest(h, host, path)) return false;

    HANDLE hFile = CreateFileW(saveAs.c_str(), GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD dwSize = 0;
    bool ok = true;
    do {
        if (!WinHttpQueryDataAvailable(h.hRequest, &dwSize) || !dwSize) break;
        std::vector<BYTE> buf(dwSize);
        DWORD got = 0;
        if (!WinHttpReadData(h.hRequest, buf.data(), dwSize, &got) || !got)
            { ok = false; break; }
        DWORD written = 0;
        if (!WriteFile(hFile, buf.data(), got, &written, NULL) || written != got)
            { ok = false; break; }
    } while (dwSize > 0);

    CloseHandle(hFile);
    if (!ok) DeleteFileW(saveAs.c_str());
    return ok;
}

// -----------------------------------------------------------------------
// Verzió lekérés
// -----------------------------------------------------------------------

static std::string GetLocalVersion()
{
    char buf[64];
    sprintf_s(buf, "%d.%d.%d.%d",
        IXAC_VERSION_MAJOR, IXAC_VERSION_MINOR,
        IXAC_VERSION_PATCH, IXAC_BUILD_NUMBER);
    return buf;
}

static std::string GetServerVersion()
{
    return DownloadString(OBF_W(L"ixac.bwmt2global.eu"),
                          OBF_W(L"/PATCHER/SelfUpdate/IXAC.version"));
}

static std::string GetServerHash()
{
    return DownloadString(OBF_W(L"ixac.bwmt2global.eu"),
                          OBF_W(L"/PATCHER/SelfUpdate/IXAC.sha256"));
}

// -----------------------------------------------------------------------
// Updater kicsomagolás és futtatás
// -----------------------------------------------------------------------

static bool ExtractUpdaterExe(std::wstring& outPath)
{
    wchar_t tempDir[MAX_PATH], tempFile[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempDir)) return false;
    if (!GetTempFileNameW(tempDir, AY_OBFUSCATE(L"IXU"), 0, tempFile)) return false;

    HRSRC hRes = FindResourceW(g_hDll,
        MAKEINTRESOURCEW(IDR_IXAC_UPDATER), MAKEINTRESOURCEW(RT_RCDATA));
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(g_hDll, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(g_hDll, hRes);
    if (!size) return false;
    void* pData = LockResource(hData);
    if (!pData) return false;

    HANDLE hFile = CreateFileW(tempFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, NULL);
    CloseHandle(hFile);

    if (!ok || written != size) { DeleteFileW(tempFile); return false; }
    outPath = tempFile;
    return true;
}

void PerformSelfUpdate(const std::wstring& newDllPath)
{
    std::wstring dllDir     = GetDllDir();
    std::wstring targetDll  = dllDir + OBF_W(L"\\IXAC.dll");
    std::wstring restartExe = dllDir + OBF_W(L"\\AurigaPatcher.exe");

    std::wstring updaterPath;
    if (!ExtractUpdaterExe(updaterPath)) return;

    std::wstring cmdLine =
        L"\"" + updaterPath + L"\" \"" +
        targetDll   + L"\" \"" +
        newDllPath  + L"\" \"" +
        restartExe  + L"\"";

    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        ExitProcess(0);
    }
}

// -----------------------------------------------------------------------
// Fő update folyamat
// -----------------------------------------------------------------------

bool CheckForUpdate()
{
    std::string local  = GetLocalVersion();
    std::string server = TrimCopy(GetServerVersion());
    if (server.empty() || local == server) return false;

    std::string expectedHash = TrimCopy(GetServerHash());
    if (expectedHash.size() != 64) return false;

    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(g_hDll, dllPath, MAX_PATH);
    std::wstring dir    = std::filesystem::path(dllPath).parent_path().wstring();
    std::wstring newDll = dir + OBF_W(L"\\IXAC_new.dll");

    if (!DownloadFile(OBF_W(L"ixac.bwmt2global.eu"),
                      OBF_W(L"/PATCHER/SelfUpdate/IXAC.dll"), newDll))
        return false;

    if (!VerifyFileHash(newDll, expectedHash)) return false;

    PerformSelfUpdate(newDll);
    return true;
}
