#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>

// TTL cache. A lejarati idok monoton oraval (steady_clock) szamolodnak,
// igy a falora ugralasa (NTP/DST/kezi allitas) nem jar le/el koran
// cache-eket (ld. docs/chrono_time_audit.md).
template <typename T> class cache
{
	public:
	virtual ~cache() = default;

	cache(): m_bNeedQuery(false), m_expireTime(std::chrono::seconds(600))
		{
			m_lastFlushTime = std::chrono::steady_clock::now();

			memset( &m_data, 0, sizeof(m_data) );
		}

		T * Get(bool bUpdateTime = true)
		{
			if (bUpdateTime)
				m_lastUpdateTime = std::chrono::steady_clock::now();

			return &m_data;
		}

		void Put(T * pNew, bool bSkipQuery = false)
		{
			memcpy(&m_data, pNew, sizeof(T));
			m_lastUpdateTime = std::chrono::steady_clock::now();

			if (!bSkipQuery)
				m_bNeedQuery = true;
		}

		bool CheckFlushTimeout()
		{
			if (m_bNeedQuery && std::chrono::steady_clock::now() - m_lastFlushTime > m_expireTime)
				return true;

			return false;
		}

		bool CheckTimeout()
		{
			if (std::chrono::steady_clock::now() - m_lastUpdateTime > m_expireTime)
				return true;

			return false;
		}

		// Masodperc, ami a legutobbi update ota elt (monoton).
		uint32_t GetLastUpdateAgeSeconds() const
		{
			return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - m_lastUpdateTime).count());
		}

		void Flush()
		{
			if (!m_bNeedQuery)
				return;

			OnFlush();
			m_bNeedQuery = false;
			m_lastFlushTime = std::chrono::steady_clock::now();
		}

		virtual void OnFlush() = 0;


	protected:
		T       m_data;
		bool    m_bNeedQuery;
		std::chrono::steady_clock::duration m_expireTime;
		std::chrono::steady_clock::time_point m_lastUpdateTime;
		std::chrono::steady_clock::time_point m_lastFlushTime;
};
