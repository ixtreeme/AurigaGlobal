#ifndef __INC_METIN_II_SHUTDOWNMANAGER_H__
#define __INC_METIN_II_SHUTDOWNMANAGER_H__

#include <Core/Time.h>
class CShutdownManager : public singleton<CShutdownManager>
{
public:
	CShutdownManager();
	virtual ~CShutdownManager();

	void AddDesc(LPDESC pDesc);
	void RemoveDesc(LPDESC pDesc);
	int SearchDesc(LPDESC pDesc);

	void Update();

private:
	std::list<LPDESC> m_lstDesc;
	std::tm m_time{}; time_t m_lTime{ 0 };
	bool m_bShutdownAlarm;

	void inline UpdateTime() { m_lTime = ::time(nullptr); m_time = auriga::time::LocalTime(m_lTime); }

public:
	bool CheckShutdownAge(const char* szSocialID);
	bool CheckShutdownTime();
	bool CheckCorrectSocialID(const char* szSocialID);
	bool inline CheckLocale() { return gShutdownEnable; }

private:
	int inline CharToInt(char c) { if(c >= '0' && c <= '9') return c - '0'; else return 0;} // char -> int
};
#endif
