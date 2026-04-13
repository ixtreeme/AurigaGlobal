#pragma once
#include <common/stl.h>

class CMapLocation : public singleton<CMapLocation> {
	public:
		typedef struct SLocation {
			uint32_t addr;
			uint16_t port;
		} TLocation;

#ifdef ENABLE_GENERAL_CH
		typedef struct SLocationTable {
			uint8_t channel;
			int32_t index;
			TLocation location;
		} TLocationTable;
#endif

		bool	Get(
#ifdef ENABLE_GENERAL_CH
uint8_t channel, 
#endif
int32_t x, int32_t y, int32_t& lIndex, uint32_t& lAddr, uint16_t & wPort);
		bool	Get(
#ifdef ENABLE_GENERAL_CH
uint8_t channel, 
#endif
		int32_t iIndex, uint32_t& lAddr, uint16_t & wPort);
		void	Insert(int32_t lIndex, const char * c_pszHost, uint16_t wPort
#ifdef ENABLE_GENERAL_CH
, uint8_t channel
#endif
		);
	protected:
#ifdef ENABLE_GENERAL_CH
		std::vector<TLocationTable> m_vector_address;
#else
		std::map<int32_t, TLocation> m_map_address;
#endif
};
