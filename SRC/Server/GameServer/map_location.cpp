#include "stdafx.h"
#include "map_location.h"
#include "sectree_manager.h"

CMapLocation g_mapLocations;

bool CMapLocation::Get(
#ifdef ENABLE_GENERAL_CH
uint8_t channel, 
#endif
int32_t x, int32_t y, int32_t& lIndex, uint32_t& lAddr, uint16_t & wPort) {
	lIndex = SECTREE_MANAGER::instance().GetMapIndex(x, y);
	return Get(
#ifdef ENABLE_GENERAL_CH
channel, 
#endif
	lIndex, lAddr, wPort);
}

bool CMapLocation::Get(
#ifdef ENABLE_GENERAL_CH
uint8_t channel, 
#endif
int32_t iIndex, uint32_t& lAddr, uint16_t & wPort) {
	if (iIndex == 0) {
		sys_log(0, "CMapLocation::Get - Error MapIndex[%d]", iIndex);
		return false;
	}

#ifdef ENABLE_GENERAL_CH
	for (auto it = m_vector_address.begin(); it != m_vector_address.end(); ++it) {
		auto data = *it;
		if (channel == data.channel && data.index == iIndex) {
			lAddr = data.location.addr;
			wPort = data.location.port;
			return true;
		}
	}

	for (auto it = m_vector_address.begin(); it != m_vector_address.end(); ++it) {
		auto data = *it;
		if (99 == data.channel && data.index == iIndex) {
			lAddr = data.location.addr;
			wPort = data.location.port;
			return true;
		}
	}

	sys_err("CMapLocation::Get - Not Found MapIndex[%d]", iIndex);
	return false;
#else
	auto it = m_map_address.find(iIndex);
	if (m_map_address.end() == it) {
		sys_log(0, "CMapLocation::Get - Error MapIndex[%d]", iIndex);

		for (auto i = m_map_address.begin(); i != m_map_address.end(); ++i) {
			sys_log(0, "Map(%d): Server(%x:%d)", i->first, i->second.addr, i->second.port);
		}

		return false;
	}

	lAddr = it->second.addr;
	wPort = it->second.port;
	return true;
#endif
}

void CMapLocation::Insert(int32_t lIndex, const char * c_pszHost, uint16_t wPort
#ifdef ENABLE_GENERAL_CH
, uint8_t channel
#endif
) {
	TLocation loc;
	loc.addr = inet_addr(c_pszHost);
	loc.port = wPort;
#ifndef ENABLE_GENERAL_CH
	m_map_address.insert(std::make_pair(lIndex, loc));
#endif

#ifdef ENABLE_GENERAL_CH
	TLocationTable t;
	t.channel = channel;
	t.index = lIndex;
	t.location = loc;
	m_vector_address.push_back(t);
#endif

	sys_log(0, "MapLocation::Insert : %d %s %d", lIndex, c_pszHost, wPort);
}
