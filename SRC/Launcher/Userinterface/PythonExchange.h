#pragma once

#include "Packet.h"

/*
 *	교환 창 관련
 */
class CPythonExchange : public CSingleton<CPythonExchange>
{
	public:
		enum
		{
#ifdef ENABLE_NEW_EXCHANGE_WINDOW
			EXCHANGE_ITEM_MAX_NUM = 24,
#else
			EXCHANGE_ITEM_MAX_NUM = 12,
#endif
		};

		typedef struct trade
		{
			char					name[CHARACTER_NAME_MAX_LEN + 1];

#if defined (ENABLE_LEVEL_IN_TRADE) && defined(ENABLE_NEW_EXCHANGE_WINDOW)
			uint32_t					level;
#endif

			uint32_t					item_vnum[EXCHANGE_ITEM_MAX_NUM];
			int							item_count[EXCHANGE_ITEM_MAX_NUM];
			uint32_t					item_metin[EXCHANGE_ITEM_MAX_NUM][ITEM_SOCKET_SLOT_MAX_NUM];
			TPlayerItemAttribute	item_attr[EXCHANGE_ITEM_MAX_NUM][ITEM_ATTRIBUTE_SLOT_MAX_NUM];

			uint8_t					accept;

			int64_t				elk;

#ifdef ATTR_LOCK
			short					lockedattr[EXCHANGE_ITEM_MAX_NUM];
#endif
#ifdef ENABLE_NEW_EXCHANGE_WINDOW
			uint32_t					race;
#endif
		} TExchangeData;

	public:
		CPythonExchange();
		virtual ~CPythonExchange();

		void			Clear();

		void			Start();
		void			End();
		bool			isTrading();

		// Interface

		void			SetSelfName(const char *name);
		void			SetTargetName(const char *name);

		char			*GetNameFromSelf();
		char			*GetNameFromTarget();

#if defined (ENABLE_LEVEL_IN_TRADE) && defined(ENABLE_NEW_EXCHANGE_WINDOW)
		void			SetSelfLevel(uint32_t level);
		void			SetTargetLevel(uint32_t level);

		uint32_t			GetLevelFromSelf();
		uint32_t			GetLevelFromTarget();
#endif

		void			SetElkToTarget(int64_t elk);
		void			SetElkToSelf(int64_t elk);

		int64_t		GetElkFromTarget();
		int64_t		GetElkFromSelf();


#ifdef ENABLE_NEW_EXCHANGE_WINDOW
		void			SetSelfRace(uint32_t race);
		void			SetTargetRace(uint32_t race);
		uint32_t			GetRaceFromSelf();
		uint32_t			GetRaceFromTarget();
#endif

		void			SetItemToTarget(uint32_t pos, uint32_t vnum, int count);
		void			SetItemToSelf(uint32_t pos, uint32_t vnum, int count);

		void			SetItemMetinSocketToTarget(int pos, int imetinpos, uint32_t vnum);
		void			SetItemMetinSocketToSelf(int pos, int imetinpos, uint32_t vnum);

		void			SetItemAttributeToTarget(int pos, int iattrpos, uint8_t byType, short sValue);
		void			SetItemAttributeToSelf(int pos, int iattrpos, uint8_t byType, short sValue);

		void			DelItemOfTarget(uint8_t pos);
		void			DelItemOfSelf(uint8_t pos);

		uint32_t			GetItemVnumFromTarget(uint8_t pos);
		uint32_t			GetItemVnumFromSelf(uint8_t pos);

		int				GetItemCountFromTarget(uint8_t pos);
		int				GetItemCountFromSelf(uint8_t pos);

		uint32_t			GetItemMetinSocketFromTarget(uint8_t pos, int iMetinSocketPos);
		uint32_t			GetItemMetinSocketFromSelf(uint8_t pos, int iMetinSocketPos);

		void			GetItemAttributeFromTarget(uint8_t pos, int iAttrPos, uint8_t* pbyType, short * psValue);
		void			GetItemAttributeFromSelf(uint8_t pos, int iAttrPos, uint8_t* pbyType, short * psValue);

		void			SetAcceptToTarget(uint8_t Accept);
		void			SetAcceptToSelf(uint8_t Accept);

#ifdef ATTR_LOCK
		void			SetItemAttrLocked(int iPos, short dwTransmutation, bool bSelf);
		short			GetItemAttrLocked(int iPos, bool bSelf);
#endif

		bool			GetAcceptFromTarget();
		bool			GetAcceptFromSelf();

		bool			GetElkMode();
		void			SetElkMode(bool value);

	protected:
		bool				m_isTrading;

		bool				m_elk_mode;   // 엘크를 클릭해서 교환했을때를 위한 변종임.
		TExchangeData		m_self;
		TExchangeData		m_victim;
};
