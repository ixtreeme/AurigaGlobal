#pragma once

#ifndef VC_EXTRALEAN

class CNetworkAddress
{
	public:
		static bool GetHostName(char* szName, int size);

	public:
		CNetworkAddress();
		~CNetworkAddress();

		void Clear();

		bool Set(const char* c_szAddr, uint16_t port);

		void SetLocalIP();
		void SetIP(DWORD ip);
		void SetIP(const char* c_szIP);
		bool SetDNS(const char* c_szDNS);

		void SetPort(uint16_t port);

		int GetPort();
		int GetSize();

		void GetIP(char* szIP, int len);

		uint32_t GetIP();

		operator const SOCKADDR_IN&() const;


	private:
		bool IsIP(const char* c_szAddr);

	private:
		SOCKADDR_IN m_sockAddrIn;
};

#endif