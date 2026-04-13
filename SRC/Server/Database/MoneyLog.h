#pragma once

#include <map>


class CMoneyLog : public singleton<CMoneyLog>
{
    public:
	CMoneyLog();
	virtual ~CMoneyLog();

	void Save();
	void AddLog(uint8_t utype, uint32_t dwVnum, int64_t iGold);

    private:
		std::map<uint32_t, int64_t> m_MoneyLogContainer[MONEY_LOG_TYPE_MAX_NUM];
};
