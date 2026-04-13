#pragma once

template <typename T> class cache
{
	public:
	virtual ~cache() = default;

	cache(): m_bNeedQuery(false), m_expireTime(600), m_lastUpdateTime(0)
		{
			m_lastFlushTime = time(nullptr);

			memset( &m_data, 0, sizeof(m_data) );
		}

		T * Get(bool bUpdateTime = true)
		{
			if (bUpdateTime)
				m_lastUpdateTime = time(nullptr);

			return &m_data;
		}

		void Put(T * pNew, bool bSkipQuery = false)
		{
			memcpy(&m_data, pNew, sizeof(T));
			m_lastUpdateTime = time(nullptr);

			if (!bSkipQuery)
				m_bNeedQuery = true;
		}

		bool CheckFlushTimeout()
		{
			if (m_bNeedQuery && time(nullptr) - m_lastFlushTime > m_expireTime)
				return true;

			return false;
		}

		bool CheckTimeout()
		{
			if (time(nullptr) - m_lastUpdateTime > m_expireTime)
				return true;

			return false;
		}

		void Flush()
		{
			if (!m_bNeedQuery)
				return;

			OnFlush();
			m_bNeedQuery = false;
			m_lastFlushTime = time(nullptr);
		}

		virtual void OnFlush() = 0;


	protected:
		T       m_data;
		bool    m_bNeedQuery;
		time_t  m_expireTime;
		time_t	m_lastUpdateTime;
		time_t	m_lastFlushTime;
};
