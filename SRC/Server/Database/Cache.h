#pragma once
#include <common/cache.h>
#include <common/service.h>

class CItemCache : public cache<TPlayerItem>
{
    public:
	CItemCache();
	virtual ~CItemCache();

	void Delete();
	virtual void OnFlush();
};

class CPlayerTableCache : public cache<TPlayerTable>
{
    public:
	CPlayerTableCache();
	virtual ~CPlayerTableCache();

	virtual void OnFlush();

	uint32_t GetLastUpdateTime() { return m_lastUpdateTime; }
};

// MYSHOP_PRICE_LIST
class CItemPriceListTableCache : public cache< TItemPriceListTable >
{
    public:

	/// Constructor
	CItemPriceListTableCache(void);
	virtual ~CItemPriceListTableCache();

	void	UpdateList(const TItemPriceListTable* pUpdateList);

	virtual void	OnFlush(void);

    private:

	static const int	s_nMinFlushSec;		///< Minimum cache expire time
};
// END_OF_MYSHOP_PRICE_LIST

#ifdef __SKILL_COLOR_SYSTEM__
class CSKillColorCache : public cache<TSkillColor>
{
	public:
		CSKillColorCache();
		virtual ~CSKillColorCache();

		virtual void OnFlush();
};
#endif
