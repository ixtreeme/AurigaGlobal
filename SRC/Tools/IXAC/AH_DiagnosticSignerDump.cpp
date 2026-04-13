#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <cstdio>
#include <vector>
#include <string>

#include "AH_DiagnosticSignerDump.h"

#pragma comment(lib, "crypt32.lib")

static bool GetSigner(const std::wstring& file, std::wstring& outSubject)
{
    outSubject.clear();

    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;

    BOOL ok = CryptQueryObject(
        CERT_QUERY_OBJECT_FILE,
        file.c_str(),
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY,
        0,
        nullptr,
        nullptr,
        nullptr,
        &hStore,
        &hMsg,
        nullptr
    );

    if (!ok || !hStore || !hMsg)
        return false;

    DWORD cb = 0;
    if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &cb))
        return false;

    std::vector<BYTE> buf(cb);
    if (!CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, buf.data(), &cb))
        return false;

    auto* psi = reinterpret_cast<PCMSG_SIGNER_INFO>(buf.data());

    CERT_INFO ci{};
    ci.Issuer = psi->Issuer;
    ci.SerialNumber = psi->SerialNumber;

    PCCERT_CONTEXT ctx = CertFindCertificateInStore(
        hStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_SUBJECT_CERT,
        &ci,
        nullptr
    );

    if (!ctx)
        return false;

    wchar_t nameBuf[512];
    DWORD len = CertGetNameStringW(
        ctx,
        CERT_NAME_SIMPLE_DISPLAY_TYPE,
        0,
        nullptr,
        nameBuf,
        512
    );

    if (len > 1)
        outSubject = nameBuf;

    CertFreeCertificateContext(ctx);
    CertCloseStore(hStore, 0);
    CryptMsgClose(hMsg);
    return !outSubject.empty();
}

void DumpAllProcessSigners()
{
    FILE* f = fopen("UserData/AC/signer_dump.txt", "w");
    if (!f) return;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe))
    {
        do {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!h) continue;

            wchar_t path[MAX_PATH];
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(h, 0, path, &len))
            {
                std::wstring subj;
                if (GetSigner(path, subj))
                {
                    fwprintf(f, L"PID=%lu   %s   SIGNER=%s\n",
                        pe.th32ProcessID, path, subj.c_str());
                }
                else
                {
                    fwprintf(f, L"PID=%lu   %s   SIGNER=<none>\n",
                        pe.th32ProcessID, path);
                }
            }

            CloseHandle(h);

        } while (Process32NextW(snap, &pe));
    }

    fclose(f);
    CloseHandle(snap);
}
