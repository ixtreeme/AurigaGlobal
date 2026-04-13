#include "AH_SignerGuard.h"
#include "AH_Core.h"

#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <cstdio>

#pragma comment(lib, "crypt32.lib")

namespace
{
    std::wstring g_BlockedSigner; // lower-case subject, amit tiltunk

    std::wstring ToLowerW(const std::wstring& s)
    {
        std::wstring out = s;
        for (auto& c : out)
            c = towlower(c);
        return out;
    }

    // Lekéri egy PE fájl aláírójának subject nevét (ha van)
    bool GetFileSignerSubject(const std::wstring& filePath, std::wstring& outSubject)
    {
        outSubject.clear();

        HCERTSTORE hStore = nullptr;
        HCRYPTMSG  hMsg = nullptr;
        BOOL       ok = CryptQueryObject(
            CERT_QUERY_OBJECT_FILE,
            filePath.c_str(),
            CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
            CERT_QUERY_FORMAT_FLAG_BINARY,
            0,
            nullptr,
            nullptr,
            nullptr,
            &hStore,
            &hMsg,
            nullptr);

        if (!ok || !hStore || !hMsg)
        {
            if (hMsg)   CryptMsgClose(hMsg);
            if (hStore) CertCloseStore(hStore, 0);
            return false;
        }

        DWORD cbData = 0;
        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &cbData))
        {
            CryptMsgClose(hMsg);
            CertCloseStore(hStore, 0);
            return false;
        }

        std::vector<BYTE> buffer(cbData);
        if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, buffer.data(), &cbData))
        {
            CryptMsgClose(hMsg);
            CertCloseStore(hStore, 0);
            return false;
        }

        auto* psi = reinterpret_cast<PCMSG_SIGNER_INFO>(buffer.data());

        CERT_INFO certInfo{};
        certInfo.Issuer = psi->Issuer;
        certInfo.SerialNumber = psi->SerialNumber;

        PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
            hStore,
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
            0,
            CERT_FIND_SUBJECT_CERT,
            &certInfo,
            nullptr);

        CryptMsgClose(hMsg);
        CertCloseStore(hStore, 0);

        if (!pCertContext)
            return false;

        wchar_t nameBuf[512];
        DWORD len = CertGetNameStringW(
            pCertContext,
            CERT_NAME_SIMPLE_DISPLAY_TYPE,
            0,
            nullptr,
            nameBuf,
            static_cast<DWORD>(std::size(nameBuf)));

        CertFreeCertificateContext(pCertContext);

        if (len <= 1)
            return false;

        outSubject.assign(nameBuf);
        return true;
    }

} // anonymous namespace

namespace AntiHook::SignerGuard
{
    void SetBlockedSigner(const std::wstring& subject)
    {
        g_BlockedSigner = ToLowerW(subject);
    }

    void ScanProcesses(const char* logFile)
    {
        if (g_BlockedSigner.empty())
            return;

        FILE* f = std::fopen(logFile, "a");
        if (!f)
            return;


        DWORD selfPid = GetCurrentProcessId();

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
        {
            std::fprintf(f, AY_OBFUSCATE("CreateToolhelp32Snapshot(PROCESS) failed\n"));
            std::fclose(f);
            return;
        }

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        bool foundBlocked = false;

        if (Process32FirstW(snap, &pe))
        {
            do
            {
                if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4)
                    continue; // System / Idle

                if (pe.th32ProcessID == selfPid)
                    continue;

                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (!hProc)
                    continue;

                wchar_t pathBuf[MAX_PATH];
                DWORD   sz = MAX_PATH;
                if (!QueryFullProcessImageNameW(hProc, 0, pathBuf, &sz))
                {
                    CloseHandle(hProc);
                    continue;
                }

                CloseHandle(hProc);

                std::wstring imagePath(pathBuf);
                std::wstring signer;
                if (!GetFileSignerSubject(imagePath, signer))
                    continue;

                std::wstring signerLower = ToLowerW(signer);

                if (signerLower == g_BlockedSigner)
                {
                    std::fwprintf(
                        f,
                        AY_OBFUSCATE(L"[ANTIHOOK] BLOCKED SIGNER PROCESS: PID=%lu IMAGE=%s SIGNER=%s\n",
                        static_cast<unsigned long>(pe.th32ProcessID)),
                        imagePath.c_str(),
                        signer.c_str()
                    );
                    foundBlocked = true;
                    break; // elég egyet találnunk
                }

            } while (Process32NextW(snap, &pe));
        }

        CloseHandle(snap);

        std::fclose(f);

        if (foundBlocked)
        {
            // Ha ilyen aláíróval fut valami, nem küldjük tovább játszani.
            IXAC_ReportCheat();
            TerminateProcess(GetCurrentProcess(), 0x5E1F); // "SIGN"
        }
    }
}
