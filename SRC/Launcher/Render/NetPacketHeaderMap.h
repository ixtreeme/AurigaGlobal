#pragma once

#include <map>

class CNetworkPacketHeaderMap
{
	public:
#ifdef __ENABLE_LARGE_DYNAMIC_PACKET__
		typedef struct SPacketType
		{
			SPacketType(int iSize = 0, int bType = 0)
			{
				iPacketSize = iSize;
				iPacketType = bType;
			}

			int iPacketSize;
			int iPacketType;
		} TPacketType;
#else
		typedef struct SPacketType
		{
			SPacketType(int iSize = 0, bool bFlag = false)
			{
				iPacketSize = iSize;
				isDynamicSizePacket = bFlag;
			}

			int iPacketSize;
			bool isDynamicSizePacket;
		} TPacketType;
#endif

	public:
		CNetworkPacketHeaderMap();
		virtual ~CNetworkPacketHeaderMap();

		void Set(int header, TPacketType rPacketType);
		bool Get(int header, TPacketType * pPacketType);

	protected:
		std::map<int, TPacketType> m_headerMap;
};
