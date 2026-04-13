// IXAC_Net.cpp
#include "IXAC_Net.h"

#include <curl/curl.h>
#include <fstream>
#include <vector>
#include <thread>
#include <windows.h>

#include "../../Launcher/SecureLayer/obfuscate.h"
#include "HWID/CHwidManager.h"

namespace IXAC::Net
{
    static Config g_Config;
    static bool   g_CurlInited = false;

    bool Init()
    {
        if (g_CurlInited)
            return true;

        CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (res != CURLE_OK)
        {
            return false;
        }

        g_CurlInited = true;
        return true;
    }

    void Shutdown()
    {
        if (!g_CurlInited)
            return;

        curl_global_cleanup();
        g_CurlInited = false;
    }

    void SetConfig(const Config& cfg)
    {
        g_Config = cfg;
    }

    // Egyszerű fájlbeolvasás
    static bool ReadFileToBuffer(const std::string& path, std::vector<char>& outBuf)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open())
        {
            return false;
        }

        f.seekg(0, std::ios::end);
        std::streampos size = f.tellg();
        if (size <= 0)
        {
            return false;
        }
        f.seekg(0, std::ios::beg);

        outBuf.resize(static_cast<size_t>(size));
        f.read(outBuf.data(), size);

        if (!f.good())
        {
            return false;
        }

        return true;
    }

    // Egyszerű törlés – ha akarsz secure overwrite-ot, ide tudod betenni.
    static void DeleteFileSafe(const std::string& path)
    {
        if (!DeleteFileA(path.c_str()))
        {
            DWORD err = GetLastError();
        }
    }

    // A tényleges feltöltés – szinkron, ezt fogjuk threadben hívni
    static bool UploadLogBuffer(const std::vector<char>& buf)
    {
        if (!g_CurlInited)
        {
            return false;
        }

        CURL* curl = curl_easy_init();
        if (!curl)
        {
            return false;
        }

        CURLcode res;
        struct curl_slist* headers = nullptr;

        // API kulcs header
        std::string keyHeader = "X-AC-KEY: " + g_Config.apiKey;
        headers = curl_slist_append(headers, keyHeader.c_str());

        //// HWID header – EZ AZ ÚJ RÉSZ
        std::string hwid = IXAC_GetFinalHWID();
        if (!hwid.empty())
        {
            std::string hwidHeader = "X-AC-HWID: " + hwid;
            headers = curl_slist_append(headers, hwidHeader.c_str());
        }


        // Tetszőleges content-type, mert bináris logot is küldhetsz
        headers = curl_slist_append(headers, AY_OBFUSCATE("Content-Type: application/octet-stream"));

        curl_easy_setopt(curl, CURLOPT_URL, g_Config.serverUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)buf.size());

        // Time-outok
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, g_Config.connectTimeoutSec);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, g_Config.requestTimeoutSec);

        // SSL ellenőrzés
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, g_Config.verifySsl ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, g_Config.verifySsl ? 2L : 0L);

        // (Opcionális) response body ignorálása – nekünk csak a státusz kell
        curl_easy_setopt(
            curl,
            CURLOPT_WRITEFUNCTION,
            +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                return size * nmemb; // eldobjuk
            });

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return false;
        }

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return (httpCode >= 200 && httpCode < 300);
    }

    // Cheat detektálás teljes életciklus – log beolvas, upload, töröl, kilép
    static void CheatHandlerThread(std::string logPath)
    {
        std::vector<char> buf;
        bool readOk = ReadFileToBuffer(logPath, buf);
        if (!readOk)
        {
            DeleteFileSafe(logPath); // opcionális
            ExitProcess(0);
            return;
        }

        UploadLogBuffer(buf);
       

        DeleteFileSafe(logPath);
        ExitProcess(0);
    }

    void OnCheatDetected(const std::string& logPath)
    {
        try
        {
            std::thread th(CheatHandlerThread, logPath);
            th.detach();
        }
        catch (...)
        {
            ExitProcess(0);
        }
    }

} // namespace IXAC::Net
