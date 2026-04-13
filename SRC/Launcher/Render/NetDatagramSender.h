#pragma once

#ifndef VC_EXTRALEAN

class CNetDatagramSender
{
	public:
		CNetDatagramSender();
		virtual ~CNetDatagramSender();

		bool isSocket();

		bool SetSocket(const char * c_szIP, WORD wPortIndex);
		bool SetSocket(uint32_t dwAddress, WORD wPortIndex);
		bool Send(const void * pBuffer, int iSize);

	protected:
		bool m_isSocket;

		WORD m_dwAddress;
		WORD m_wPortIndex;

		SOCKET m_Socket;
		SOCKADDR_IN m_SockAddr;
};

#endif
