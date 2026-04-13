#include "Anti_VM.h"

#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>
#include <intrin.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <comdef.h>
#include <Wbemidl.h>



using std::string;
using std::vector;

static bool iequals(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (tolower(a[i]) != tolower(b[i])) return false;
    return true;
}
static bool contains_ci(const string& hay, const string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(hay.begin(), hay.end(),
        needle.begin(), needle.end(),
        [](char a, char b) { return tolower(a) == tolower(b); });
    return it != hay.end();
}

// ---------------- CPUID checks ----------------
bool cpuid_hypervisor_flag()
{
    int cpuinfo[4] = { 0,0,0,0 };
    __cpuid(cpuinfo, 1);
    // ECX bit 31 = hypervisor present
    return ((cpuinfo[2] >> 31) & 1) != 0;
}

string cpuid_hypervisor_vendor()
{
    // CPUID leaf 0x40000000 returns hypervisor vendor string in EBX, ECX, EDX
    int info[4] = { 0,0,0,0 };
    __cpuid(info, 0x40000000);
    char vendor[13] = { 0 };
    *reinterpret_cast<int32_t*>(&vendor[0]) = info[1]; // EBX
    *reinterpret_cast<int32_t*>(&vendor[4]) = info[3]; // EDX
    *reinterpret_cast<int32_t*>(&vendor[8]) = info[2]; // ECX
    return string(vendor);
}

// ---------------- MAC prefix checks ----------------
const vector<string> vm_mac_prefixes = {
    "00:05:69","00:0C:29","00:1C:14","00:50:56", // VMware
    "08:00:27", // VirtualBox
    "00:15:5D", // Hyper-V
    "00:1C:42", // Parallels
    "52:54:00"  // QEMU/KVM
};

bool check_mac_prefix_vm()
{
    ULONG outBufLen = 0;
    DWORD ret = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &outBufLen);
    if (ret != ERROR_BUFFER_OVERFLOW) return false;
    std::vector<uint8_t> buf(outBufLen);
    PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &outBufLen) != NO_ERROR) return false;

    for (PIP_ADAPTER_ADDRESSES it = adapters; it != nullptr; it = it->Next) {
        if (it->PhysicalAddressLength >= 6) {
            char mac[64];
            sprintf_s(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                it->PhysicalAddress[0], it->PhysicalAddress[1], it->PhysicalAddress[2],
                it->PhysicalAddress[3], it->PhysicalAddress[4], it->PhysicalAddress[5]);
            string macs(mac);
            std::transform(macs.begin(), macs.end(), macs.begin(), ::tolower);
            for (const auto& pref : vm_mac_prefixes) {
                string p = pref;
                std::transform(p.begin(), p.end(), p.begin(), ::tolower);
                if (macs.rfind(p, 0) == 0) return true;
            }
        }
    }
    return false;
}

// ---------------- VM indicative files / drivers ----------------
bool check_vm_indicative_files()
{
    const vector<std::wstring> files = {
        L"C:\\Windows\\System32\\drivers\\vmmouse.sys",
        L"C:\\Windows\\System32\\drivers\\vmhgfs.sys",
        L"C:\\Windows\\System32\\drivers\\vboxmouse.sys",
        L"C:\\Windows\\System32\\drivers\\vboxdrv.sys",
        L"C:\\Windows\\System32\\drivers\\vboxguest.sys",
        L"C:\\Windows\\System32\\drivers\\vm3dgl.dll"
    };
    for (auto& f : files) {
        DWORD attr = GetFileAttributesW(f.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) return true;
    }
    return false;
}

// ---------------- WMI checks (Manufacturer / Model / BIOS) ----------------
bool query_wmi_strings(vector<string>& out_strs)
{
    HRESULT hr;
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    bool coInited = SUCCEEDED(hr);
    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr)) {
        if (coInited) CoUninitialize();
        return false;
    }

    IWbemLocator* pLocator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLocator);
    if (FAILED(hr) || !pLocator) {
        if (coInited) CoUninitialize();
        return false;
    }

    IWbemServices* pSvc = nullptr;
    hr = pLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
    if (FAILED(hr) || !pSvc) {
        pLocator->Release();
        if (coInited) CoUninitialize();
        return false;
    }

    // set security levels on the proxy
    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        pSvc->Release();
        pLocator->Release();
        if (coInited) CoUninitialize();
        return false;
    }

    // Query Win32_ComputerSystem
    {
        IEnumWbemClassObject* pEnumerator = nullptr;
        hr = pSvc->ExecQuery(bstr_t("WQL"),
            bstr_t("SELECT Manufacturer, Model FROM Win32_ComputerSystem"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator) {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK) {
                VARIANT vtProp;
                // Manufacturer
                VariantInit(&vtProp);
                if (SUCCEEDED(pObj->Get(L"Manufacturer", 0, &vtProp, nullptr, nullptr)) && vtProp.vt == VT_BSTR) {
                    _bstr_t b(vtProp.bstrVal);
                    out_strs.push_back((const char*)b);
                }
                VariantClear(&vtProp);
                // Model
                VariantInit(&vtProp);
                if (SUCCEEDED(pObj->Get(L"Model", 0, &vtProp, nullptr, nullptr)) && vtProp.vt == VT_BSTR) {
                    _bstr_t b(vtProp.bstrVal);
                    out_strs.push_back((const char*)b);
                }
                pObj->Release();
            }
            pEnumerator->Release();
        }
    }

    // Query Win32_BIOS for BIOS vendor / version
    {
        IEnumWbemClassObject* pEnumerator = nullptr;
        hr = pSvc->ExecQuery(bstr_t("WQL"),
            bstr_t("SELECT Manufacturer, SerialNumber, SMBIOSBIOSVersion FROM Win32_BIOS"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
        if (SUCCEEDED(hr) && pEnumerator) {
            IWbemClassObject* pObj = nullptr;
            ULONG ret = 0;
            while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK) {
                VARIANT vtProp;
                VariantInit(&vtProp);
                if (SUCCEEDED(pObj->Get(L"Manufacturer", 0, &vtProp, nullptr, nullptr)) && vtProp.vt == VT_BSTR) {
                    _bstr_t b(vtProp.bstrVal);
                    out_strs.push_back((const char*)b);
                }
                VariantClear(&vtProp);

                VariantInit(&vtProp);
                if (SUCCEEDED(pObj->Get(L"SMBIOSBIOSVersion", 0, &vtProp, nullptr, nullptr)) && vtProp.vt == VT_BSTR) {
                    _bstr_t b(vtProp.bstrVal);
                    out_strs.push_back((const char*)b);
                }
                VariantClear(&vtProp);

                VariantInit(&vtProp);
                if (SUCCEEDED(pObj->Get(L"SerialNumber", 0, &vtProp, nullptr, nullptr)) && vtProp.vt == VT_BSTR) {
                    _bstr_t b(vtProp.bstrVal);
                    out_strs.push_back((const char*)b);
                }
                VariantClear(&vtProp);

                pObj->Release();
            }
            pEnumerator->Release();
        }
    }

    pSvc->Release();
    pLocator->Release();
    if (coInited) CoUninitialize();
    return true;
}

bool check_wmi_vm()
{
    vector<string> wmi;
    if (!query_wmi_strings(wmi)) return false;
    const vector<string> vm_signs = {
        "virtual", "vmware", "virtualbox", "vbox", "kvm", "qemu", "hyper-v", "xen", "parallels", "microsoft corporation"
    };
    for (auto& s : wmi) {
        for (auto& sig : vm_signs) {
            if (contains_ci(s, sig)) return true;
        }
    }
    return false;
}

// ---------------- Combined detection & scoring ----------------
bool is_virtual_machine_detected(int& out_score, vector<string>& out_reasons)
{
    out_score = 0;
    out_reasons.clear();

    // CPUID hypervisor flag
    if (cpuid_hypervisor_flag()) {
        out_score += 3;
        out_reasons.push_back("CPUID hypervisor bit set");
    }

    // hypervisor vendor
    string vendor = cpuid_hypervisor_vendor();
    if (!vendor.empty()) {
        // known vendor substrings
        vector<string> known = { "KVMKVMKVM","Microsoft Hv","VMwareVMware","VBoxVBoxVBox","XenVMMXenVMM","prl hyperv" };
        for (auto& k : known) {
            if (vendor.find(k) != string::npos) {
                out_score += 4;
                out_reasons.push_back("CPUID hypervisor vendor: " + vendor);
            }
        }
    }

    // MAC prefix
    if (check_mac_prefix_vm()) {
        out_score += 2;
        out_reasons.push_back("MAC prefix matches VM vendors");
    }

    // indicative files/drivers
    if (check_vm_indicative_files()) {
        out_score += 3;
        out_reasons.push_back("VM indicative driver/file present");
    }

    // WMI checks
    if (check_wmi_vm()) {
        out_score += 4;
        out_reasons.push_back("WMI indicates VM (Manufacturer/Model/Bios)");
    }

    // threshold
    //  >=6 strong VM indicator
    return out_score >= 6;
}

// ---------------- Public convenience function ----------------
void abort_if_vm_detected(bool showMessageBox)
{
    int score = 0;
    vector<string> reasons;
    if (is_virtual_machine_detected(score, reasons)) {
        // Log reasons to stderr (or game log)
        std::cerr << "[anti-vm] VM detected (score=" << score << ")\n";
        for (auto& r : reasons) std::cerr << "  - " << r << "\n";

        if (showMessageBox) {
            std::string msg = "The application is not allowed to run in virtualized environments.\n\nDetected reasons:\n";
            for (auto& r : reasons) msg += "- " + r + "\n";
            MessageBoxA(nullptr, msg.c_str(), "Error", MB_ICONERROR | MB_OK);
        }

        // Optionally perform additional cleanup here, then exit
        ExitProcess(EXIT_FAILURE);
    }
    else {
        std::cerr << "[anti-vm] No VM detected (score=" << score << ")\n";
    }
}

