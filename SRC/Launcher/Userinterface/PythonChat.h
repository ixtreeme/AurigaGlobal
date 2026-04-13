#pragma once

#include "Packet.h"
#include "AbstractChat.h"

class CWhisper
{
	public:
		typedef struct SChatLine
		{
			CGraphicTextInstance Instance;

			SChatLine()
			{
			}
			~SChatLine()
			{
				Instance.Destroy();
			}

			static void DestroySystem();

			static SChatLine* New();
			static void Delete(SChatLine* pkChatLine);

			static CDynamicPool<SChatLine> ms_kPool;
		} TChatLine;

		typedef std::deque<TChatLine*> TChatLineDeque;
		typedef std::list<TChatLine*> TChatLineList;

	public:
		CWhisper();
		~CWhisper();

		void Destroy();

		void SetPosition(float fPosition);
		void SetBoxSize(float fWidth, float fHeight);
		void AppendChat(int iType, const char* c_szChat);
		void Render(float fx, float fy);

	protected:
		void __Initialize();
		void __ArrangeChat();


	protected:
		float m_fLineStep;
		float m_fWidth;
		float m_fHeight;
		float m_fcurPosition;

		TChatLineDeque m_ChatLineDeque;
		TChatLineList m_ShowingChatLineList;

	public:
		static void DestroySystem();

		static CWhisper* New();
		static void Delete(CWhisper* pkWhisper);

		static CDynamicPool<CWhisper>		ms_kPool;
};

class CPythonChat : public CSingleton<CPythonChat>, public IAbstractChat
{
	public:
		enum EWhisperType
		{
			WHISPER_TYPE_CHAT = 0,
			WHISPER_TYPE_NOT_EXIST = 1,
			WHISPER_TYPE_TARGET_BLOCKED = 2,
			WHISPER_TYPE_SENDER_BLOCKED = 3,
			WHISPER_TYPE_ERROR = 4,
			WHISPER_TYPE_GM = 5,
#if defined(BL_OFFLINE_MESSAGE)
			WHISPER_TYPE_OFFLINE = 6,
#endif
			WHISPER_TYPE_SYSTEM = 0xFF
		};

		enum EBoardState
		{
			BOARD_STATE_VIEW,
			BOARD_STATE_EDIT,
			BOARD_STATE_LOG,
		};

		enum
		{
			CHAT_LINE_MAX_NUM = 300,
#ifdef ENABLE_NEW_CHAT
			CHAT_LINE_COLOR_ARRAY_MAX_NUM = 13,
#else
			CHAT_LINE_COLOR_ARRAY_MAX_NUM = 3,
#endif
		};

		typedef struct SChatLine
		{
			int iType;
			float fAppendedTime;
			D3DXCOLOR aColor[CHAT_LINE_COLOR_ARRAY_MAX_NUM];
			CGraphicTextInstance Instance;
#ifdef ENABLE_MULTI_LANGUAGE
			CGraphicImageInstance pLanguage;
#endif
			CGraphicImageInstance pEmpire;

			SChatLine();
			virtual ~SChatLine();

			void SetColor(uint32_t dwID, uint32_t dwColor);
			void SetColorAll(uint32_t dwColor);
			D3DXCOLOR & GetColorRef(uint32_t dwID);
			static void DestroySystem();

			static SChatLine* New();
			static void Delete(SChatLine* pkChatLine);

			static CDynamicPool<SChatLine> ms_kPool;
		} TChatLine;

		typedef struct SWaitChat
		{
			int iType = 0;
			std::string strChat = "";

			uint32_t dwAppendingTime = 0;
		} TWaitChat;

		typedef std::deque<TChatLine*> TChatLineDeque;
		typedef std::list<TChatLine*> TChatLineList;

		typedef std::map<std::string, CWhisper*> TWhisperMap;
		typedef std::set<std::string> TIgnoreCharacterSet;
		typedef std::list<TWaitChat> TWaitChatList;

		typedef struct SChatSet
		{
			int					m_ix;
			int					m_iy;
			int					m_iHeight;
			int					m_iStep;
			float				m_fEndPos;

			int					m_iBoardState;
			std::vector<int>	m_iMode;

			TChatLineList		m_ShowingChatLineList;

			bool CheckMode(uint32_t dwType)
			{
				if (dwType >= m_iMode.size())
					return false;

				return m_iMode[dwType] ? true : false;
			}

			SChatSet()
			{
				m_iBoardState = BOARD_STATE_VIEW;

				m_ix = 0;
				m_iy = 0;
				m_fEndPos = 1.0f;
				m_iHeight = 0;
				m_iStep = 15;

				m_iMode.clear();
				m_iMode.resize(ms_iChatModeSize, 1);
			}

			static int ms_iChatModeSize;
		} TChatSet;

		typedef std::map<int, TChatSet> TChatSetMap;

	public:
		CPythonChat();
		virtual ~CPythonChat();

		void SetChatColor(UINT eType, UINT r, UINT g, UINT b);

		void Destroy();
		void Close();

		int CreateChatSet(uint32_t dwID);
		void Update(uint32_t dwID);
		void Render(uint32_t dwID);
		void RenderWhisper(const char * c_szName, float fx, float fy);

		void SetBoardState(uint32_t dwID, int iState);
		void SetPosition(uint32_t dwID, int ix, int iy);
		void SetHeight(uint32_t dwID, int iHeight);
		void SetStep(uint32_t dwID, int iStep);
		void ToggleChatMode(uint32_t dwID, int iMode);
		void EnableChatMode(uint32_t dwID, int iMode);
		void DisableChatMode(uint32_t dwID, int iMode);
		void SetEndPos(uint32_t dwID, float fPos);

		int  GetVisibleLineCount(uint32_t dwID);
		int  GetEditableLineCount(uint32_t dwID);
		int  GetLineCount(uint32_t dwID);
		int  GetLineStep(uint32_t dwID);

		// Chat
		void AppendChat(int iType, const char * c_szChat);
		void AppendChatWithDelay(int iType, const char * c_szChat, int iDelay);
		void ArrangeShowingChat(uint32_t dwID);

#ifdef ENABLE_MULTI_LANGUAGE
		bool IsFilteredLanguage(std::string szLanguage);
#endif

		// Ignore
		void IgnoreCharacter(const char * c_szName);
		bool IsIgnoreCharacter(const char * c_szName);

		// Whisper
		CWhisper * CreateWhisper(const char * c_szName);
		void AppendWhisper(int iType, const char * c_szName, const char * c_szChat);
		void ClearWhisper(const char * c_szName);
		bool GetWhisper(const char * c_szName, CWhisper ** ppWhisper);
		void InitWhisper(PyObject * ppyObject);

	protected:
		void __Initialize();
		void __DestroyWhisperMap();

		TChatLineList * GetChatLineListPtr(uint32_t dwID);
		TChatSet * GetChatSetPtr(uint32_t dwID);

		void UpdateViewMode(uint32_t dwID);
		void UpdateEditMode(uint32_t dwID);
		void UpdateLogMode(uint32_t dwID);

		uint32_t GetChatColor(int iType);

	protected:
		TChatLineDeque						m_ChatLineDeque;
		TChatLineList						m_ShowingChatLineList;
		TChatSetMap							m_ChatSetMap;
		TWhisperMap							m_WhisperMap;
		TIgnoreCharacterSet					m_IgnoreCharacterSet;
		TWaitChatList						m_WaitChatList;

		D3DXCOLOR m_akD3DXClrChat[CHAT_TYPE_MAX_NUM];
};