#pragma once

/*
#include "../eterLib/NetDatagramReceiver.h"
#include "../eterLib/NetDatagramSender.h"
#include "Packet.h"

class CPythonNetworkDatagram : public CSingleton<CPythonNetworkDatagram>
{
	public:
		CPythonNetworkDatagram();
		virtual ~CPythonNetworkDatagram();

		void Destroy();

		// With Server
		void SetRecvBufferSize(uint32_t dwSize);
		void SetConnection(const char * c_szIP, WORD wPortIndex);
		void SendToServer(const void * c_pBuffer, uint32_t dwSize);
		void Bind(uint32_t dwAddress, WORD wPortIndex);

		// With UDP Senders
		void RegisterSender(uint32_t dwID, uint32_t dwAddress, WORD wPortIndex);
		void DeleteSender(uint32_t dwID);

		void Select(uint32_t dwID);
		void SendToSenders(const void * c_pBuffer, uint32_t dwSize);

		// Regulary update function
		void Process();
		void SendCharacterStatePacket(uint32_t dwVID, uint32_t dwCmdTime, const TPixelPosition& c_rkPPosDst, float fDstRot, UINT eFunc, UINT uArg);

	protected:
		BOOL CheckPacket(TPacketHeader * piRetHeader);
		BOOL GetSenderPointer(uint32_t dwID, CNetDatagramSender ** ppSender);
		BOOL RecvStateWalkingPacket();

	protected:
		// Sender Map
		typedef std::map<uint32_t, CNetDatagramSender*> TNetSenderMap;
		typedef TNetSenderMap::iterator TNetSenderMapIterator;
		// Sender List
		typedef std::list<CNetDatagramSender*> TNetSenderList;
		typedef TNetSenderList::iterator TNetSenderListIterator;

	protected:
		// Sender
		TNetSenderMap m_NetSenderMap;
		TNetSenderList m_NetSenderList;

		// Connection with server
		CNetDatagramSender m_NetSender;
		CNetDatagramReceiver m_NetReceiver;

	private:
		CDynamicPool<CNetDatagramSender> m_NetSenderPool;
};
*/