#pragma once

#include <set>
#include <entt/entity/entity.hpp>


#define OXEVENT_MAP_INDEX 113

struct tag_Quiz
{
	char level;
	char Quiz[256];
	bool answer;
};

enum OXEventStatus
{
	OXEVENT_FINISH = 0, // OX이벤트가 완전히 끝난 상태
	OXEVENT_OPEN = 1,	// OX이벤트가 시작됨. 을두지(20012)를 통해서 입장가능
	OXEVENT_CLOSE = 2,	// OX이벤트의 참가가 끝남. 을두지(20012)를 통한 입장이 차단됨
	OXEVENT_QUIZ = 3,	// 퀴즈를 출제함.

	OXEVENT_ERR = 0xff
};

class COXEventManager : public singleton<COXEventManager>
{
	private :
		std::set<entt::entity> m_participants;
		std::set<entt::entity> m_attenders;
		std::set<entt::entity> m_missed;
        uint64_t m_roundRevision { 0 };
        bool m_rewarding { false };
        bool m_checkingAnswer { false };
        bool m_closing { false };

		std::vector<std::vector<tag_Quiz> > m_vec_quiz;

		LPEVENT m_timedEvent {};

	protected :
		bool CheckAnswer();

		bool EnterAudience(entt::entity pChar);
		bool EnterAttender(entt::entity pChar);

	public :
        ~COXEventManager();
		bool Initialize();
		void Destroy();

		OXEventStatus GetStatus();
		void SetStatus(OXEventStatus status);

		bool LoadQuizScript(const char* szFileName);

		bool Enter(entt::entity pChar);

		bool CloseEvent();

		void ClearQuiz();
		bool AddQuiz(unsigned char level, const char* pszQuestion, bool answer);
		bool ShowQuizList(entt::entity character);

		bool Quiz(unsigned char level, int timelimit);
		bool GiveItemToAttender(uint32_t dwItemVnum,
#ifdef ENABLE_NEW_STACK_LIMIT
		int 
#else
		uint8_t 
#endif
		count);

		bool CheckAnswer(bool answer);
		void WarpToAudience();

		bool LogWinner();

		uint32_t GetAttenderCount();
};
