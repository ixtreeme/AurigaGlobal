#pragma once

#include <string>
#include <windows.h>
#include "../Userinterface/CNetworkAdapter.h"

class CMachineGuid
{
public:
	CMachineGuid();
	~CMachineGuid();
	std::string getMachineGUID() { return m_MachineGUID; };

private:
	LONG GetStringRegKey(HKEY hKey, const std::wstring &strValueName, std::wstring &strValue, const std::wstring &strDefaultValue);

private:
	std::string m_MachineGUID;
};
