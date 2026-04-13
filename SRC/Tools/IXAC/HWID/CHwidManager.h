#pragma once
#include <string>


class CHwidManager {
	public:
		CHwidManager();

		std::string getMachineGUID(){ return m_MachineGUID; }
		std::string getMacAddr(){ return m_MacAddr; }
		std::string getHDDSerial(){ return m_HDDSerial; }
		std::string getHDDModel(){ return m_HDDModel; }
		std::string getCPUid(){ return m_CpuId; }

	private:
		std::string m_MachineGUID;
		std::string m_MacAddr;
		std::string m_HDDSerial;
		std::string m_HDDModel;
		std::string m_CpuId;
};

std::string IXAC_GetFinalHWID();