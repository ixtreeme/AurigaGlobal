#pragma once

#include <windows.h>
#include "Singleton.h"

#include <cstdint>

class CTimer : public CSingleton<CTimer>
{
	public:
		CTimer();
		virtual ~CTimer();

		void	Advance();
		void	Adjust(int iTimeGap);
		void	SetBaseTime();

		float	GetCurrentSecond() const;
		uint32_t	GetCurrentMillisecond() const;

		float	GetElapsedSecond();
		uint32_t	GetElapsedMilliecond();

		void	UseCustomTime();

	protected:
		bool	m_bUseRealTime;
		uint32_t	m_dwBaseTime;
		uint32_t	m_dwCurrentTime;
		float	m_fCurrentTime;
		uint32_t	m_dwElapsedTime;
		int		m_index;
};

BOOL	ELTimer_Init();

uint32_t	ELTimer_GetMSec();

VOID	ELTimer_SetServerMSec(uint32_t dwServerTime);
uint32_t	ELTimer_GetServerMSec();

VOID	ELTimer_SetFrameMSec();
uint32_t	ELTimer_GetFrameMSec();
uint32_t ELTimer_GetServerFrameMSec();

// Both gaps fit in 32 bits, but their weighted sum may not.
constexpr int32_t ELTimer_AverageNetworkGap(int32_t average, int32_t gap)
{
	return static_cast<int32_t>((int64_t(average) * 70 + int64_t(gap) * 30) / 100);
}

// Network deadlines are less than half a uint32_t cycle apart. Subtract before
// comparing so commands scheduled across the 49-day timer wrap still run.
constexpr bool ELTimer_IsTimeBefore(uint32_t now, uint32_t deadline)
{
	return static_cast<int32_t>(now - deadline) < 0;
}
