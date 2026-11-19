#pragma once
#include <common/cache.h>
#include <common/service.h>
#include <common/tables.h>

class CPlayerTableCache : public cache<TPlayerTable>
{
    public:
	CPlayerTableCache();
	virtual ~CPlayerTableCache();

	virtual void OnFlush();

	uint32_t GetLastUpdateAgeSeconds() const { return cache<TPlayerTable>::GetLastUpdateAgeSeconds(); }
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
