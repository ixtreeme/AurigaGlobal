#pragma once

#ifdef ENABLE_HWID
#include <string>
#include "../Base/Singleton.h"

class CHwidManager : public CSingleton <CHwidManager> {
	public:
		CHwidManager();

		std::string getMachineGUID(){ return m_MachineGUID; };
		std::string getMacAddr(){ return m_MacAddr; };
		std::string getHDDSerial(){ return m_HDDSerial; };
		std::string getHDDModel(){ return m_HDDModel; };
		std::string getCPUid(){ return m_CpuId; };

	private:
		std::string m_MachineGUID;
		std::string m_MacAddr;
		std::string m_HDDSerial;
		std::string m_HDDModel;
		std::string m_CpuId;
};
#endif
