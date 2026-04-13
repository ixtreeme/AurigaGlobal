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
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/base64.h>
#include <cryptopp/hex.h>

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

    CryptoPP::SHA256 sha;
    std::string out;

    CryptoPP::StringSource s(chain, true,
        new CryptoPP::HashFilter(sha,
            new CryptoPP::Base64Encoder(
                new CryptoPP::StringSink(out), false
            )
        )
    );

    return out;
}
