#ifndef HWID_MANAGER_H_
#define HWID_MANAGER_H_
#include <memory>
#include "constants.h"
#include "input.h"

class CHwidManager : public singleton<CHwidManager>
{
	public:
		CHwidManager();
		virtual ~CHwidManager();

		void CleanBlocked();
		void InitializeBlocked();
		bool IsBlocked(const char * hwid);
		void AddHwidToAccount(const char * login, const char * hwid);

		void SendBlockHwid(const char * whoname, const char * targetname);
		void RecvBlockHwid(const char * whoname, const char * targetname);
		void SendUnblockHwid(const char * whoname, const char * targetname);
		void RecvUnblockHwid(const char * whoname, const char * targetname);

	protected:
		std::vector<std::string> m_blocked_hwid;
};
#endif

