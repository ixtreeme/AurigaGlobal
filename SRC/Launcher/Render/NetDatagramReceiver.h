#pragma once

#ifndef VC_EXTRALEAN

class CNetDatagramReceiver
{
	public:
		CNetDatagramReceiver();
		virtual ~CNetDatagramReceiver();

		void Clear();
		bool Bind(uint32_t dwAddress, WORD wPortIndex);
		bool isBind();

		bool Process();
		bool Recv(void * pBuffer, int iSize);
		bool Peek(void * pBuffer, int iSize);

		void SetRecvBufferSize(int recvBufSize);

	protected:
		bool m_isBind;

		uint32_t m_dwPortIndex;

		SOCKET m_Socket;
		SOCKADDR_IN m_SockAddr;

		int		m_recvBufCurrentPos;
		int		m_recvBufCurrentSize;

		char*	m_recvBuf;
		int		m_recvBufSize;
};

#endif