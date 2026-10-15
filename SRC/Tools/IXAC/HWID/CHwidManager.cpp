#include "CHwidManager.h"
#include "CMachineGuid.h"
#include "CHddData.h"
#include "CCpuId.h"
#include "CMacAddr.h"
#include "../../../Launcher/SecureLayer/obfuscate.h"

CHwidManager::CHwidManager() {
	m_MachineGUID	= "";
	m_MacAddr		= "";
	m_HDDSerial		= "";
	m_HDDModel		= "";
	m_CpuId			= "";

	CMachineGuid	c_MachineGuid;
	CMacAddr		c_MacAddr;
	CHddData		c_HddData;
	CCpuId			c_CpuId;

	m_MachineGUID	= c_MachineGuid.getMachineGUID();
	m_MacAddr		= c_MacAddr.getMacAddr();
	m_HDDSerial		= c_HddData.getHDDSerialNumber();
	m_HDDModel		= c_HddData.getHDDModelNumber();
	m_CpuId			= c_CpuId.getCpuID();
}

#include "CHwidManager.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace
{
    // Base64 (no line breaks) of the SHA-256 of the text, from the Windows
    // crypto API: the value Crypto++'s HashFilter + Base64Encoder produced.
    // Empty when the API fails; the login server answers an empty HWID.
    std::string Sha256Base64(const std::string& text)
    {
        UCHAR digest[32] {};
        if (!BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()),
                digest, sizeof(digest))))
            return std::string();

        const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
        DWORD length = 0;
        if (!CryptBinaryToStringA(digest, sizeof(digest), flags, nullptr, &length))
            return std::string();

        std::string encoded(length, '\0');
        if (!CryptBinaryToStringA(digest, sizeof(digest), flags, encoded.data(), &length))
            return std::string();

        encoded.resize(length);
        return encoded;
    }
}

std::string IXAC_GetFinalHWID()
{
    CHwidManager hwidManager; // Create an instance

    std::string chain;
    chain.append(hwidManager.getCPUid())
        .append(hwidManager.getHDDModel())
        .append(hwidManager.getMachineGUID())
        .append(hwidManager.getMacAddr())
        .append(hwidManager.getHDDSerial())
        .append(AY_OBFUSCATE("ASfds<W!k(T0=;d%Tc!k"));

    return Sha256Base64(chain);
}
