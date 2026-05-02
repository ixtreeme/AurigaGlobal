#ifndef __INC_METIN_II_GAME_ITEM_H__
#define __INC_METIN_II_GAME_ITEM_H__

#include "entity.h"

class CItem : public CEntity
{
	protected:
		// override methods from ENTITY class
		virtual void	EncodeInsertPacket(LPENTITY entity);
		virtual void	EncodeRemovePacket(LPENTITY entity);

	public:
		CItem(uint32_t dwVnum);
		virtual ~CItem();

		int			GetLevelLimit();

		bool		CheckItemUseLevel(int nLevel);

		bool		IsPCBangItem();

		int32_t		FindApplyValue(uint8_t bApplyType);

		bool		IsStackable()		{ return (GetFlag() & ITEM_FLAG_STACKABLE)?true:false; }

		void		Initialize();
		void		Destroy();

		void		Save();

		void		SetWindow(uint8_t b)	{ m_bWindow = b; }
		uint8_t		GetWindow()		{ return m_bWindow; }

		void		SetID(uint32_t id)		{ m_dwID = id;	}
		uint32_t		GetID()			{ return m_dwID; }

		void			SetProto(const TItemTable * table);
		TItemTable const *	GetProto()	{ return m_pProto; }

#ifdef ENABLE_ITEM_EXTRA_PROTO
		void		SetExtraProto(TItemExtraProto* Proto);
		TItemExtraProto* GetExtraProto();
		bool		HasExtraProto() { return m_ExtraProto != nullptr; }
#endif

#ifdef ATTR_LOCK
		short	GetLockedAttr() const	{return m_sLockedAttr;}
		void	SetLockedAttr(short sIndex);
		void	AddLockedAttr();
		void	ChangeLockedAttr();
		void	RemoveLockedAttr();
		bool	CheckHumanApply();
#endif

		int64_t		GetGold();
		int64_t		GetShopBuyPrice();


#ifdef ENABLE_MULTI_NAMES
		const char *	GetName(uint8_t Lang=0);
#else
		const char *	GetName()		{ return m_pProto ? m_pProto->szLocaleName : NULL; }
#endif

		const char *	GetBaseName()		{ return m_pProto ? m_pProto->szName : nullptr; }
		uint8_t		GetSize()		{ return m_pProto ? m_pProto->bSize : 0;	}

		void		SetFlag(int32_t flag)	{ m_lFlag = flag;	}
		int32_t		GetFlag()		{ return m_lFlag;	}

		void		AddFlag(int32_t bit);
		void		RemoveFlag(int32_t bit);

		uint32_t		GetWearFlag()		{ return m_pProto ? m_pProto->dwWearFlags : 0; }
		uint32_t		GetAntiFlag()		{ return m_pProto ? m_pProto->dwAntiFlags : 0; }
		uint32_t		GetImmuneFlag()		{ return m_pProto ? m_pProto->dwImmuneFlag : 0; }

		void		SetVID(uint32_t vid)	{ m_dwVID = vid;	}
		uint32_t		GetVID()		{ return m_dwVID;	}

		bool		SetCount(int count);
		int			GetCount();

		uint32_t		GetVnum() const		{ return m_dwMaskVnum ? m_dwMaskVnum : m_dwVnum;	}
		uint32_t		GetOriginalVnum() const		{ return m_dwVnum;	}
		uint8_t		GetType()	{ return m_pProto ? m_pProto->bType : 0;	}
		uint8_t		GetSubType() const	{ return m_pProto ? m_pProto->bSubType : 0;	}
		uint8_t		GetLimitType(uint32_t idx) const { return m_pProto ? m_pProto->aLimits[idx].bType : 0;	}
		int32_t		GetLimitValue(uint32_t idx) const { return m_pProto ? m_pProto->aLimits[idx].lValue : 0;	}
#ifdef ENABLE_NEW_USE_POTION
		uint8_t	GetApplyType(uint32_t idx) const { return m_pProto ? m_pProto->aApplies[idx].bType : 0;}
		int32_t	GetApplyValue(uint32_t idx) const { return m_pProto ? m_pProto->aApplies[idx].lValue : 0;}
#endif
		int32_t		GetValue(uint32_t idx);

		void		SetCell(LPCHARACTER ch, uint16_t pos)	{ m_pOwner = ch, m_wCell = pos;	}
		uint16_t		GetCell()				{ return m_wCell;	}

		LPITEM		RemoveFromCharacter();
#ifdef __HIGHLIGHT_SYSTEM__
		bool	AddToCharacter(LPCHARACTER ch, TItemPos Cell, bool isHighLight = true);
#else
		bool	AddToCharacter(LPCHARACTER ch, TItemPos Cell);
#endif
		LPCHARACTER	GetOwner()		{ return m_pOwner; }

		LPITEM		RemoveFromGround();
		bool		AddToGround(int32_t lMapIndex, const PIXEL_POSITION & pos, bool skipOwnerCheck = false);

		int			FindEquipCell(LPCHARACTER ch, int bCandidateCell = -1);
		bool		IsEquipped() const		{ return m_bEquipped;	}
		bool		EquipTo(LPCHARACTER ch, uint8_t bWearCell);
		bool		IsEquipable() ;

		bool		CanUsedBy(LPCHARACTER ch);

		bool		DistanceValid(LPCHARACTER ch);

		void		UpdatePacket();
		void		UsePacketEncode(LPCHARACTER ch, LPCHARACTER victim, struct packet_item_use * packet);

		void		SetExchanging(bool isOn = true);
		bool		IsExchanging()		{ return m_bExchanging;	}

		bool		IsTwohanded();

		bool		IsPolymorphItem();

		void		ModifyPoints(bool bAdd);	// 아이템의 효과를 캐릭터에 부여 한다. bAdd가 false이면 제거함

		bool		CreateSocket(uint8_t bSlot, uint8_t bGold);
		const int32_t*	GetSockets()		{ return &m_alSockets[0];	}
		int32_t		GetSocket(int i)	{ return m_alSockets[i];	}

		void		SetSockets(const int32_t* al);
		void		SetSocket(int i, int32_t v, bool bLog = true);

		int		GetSocketCount();
		bool		AddSocket();

		const TPlayerItemAttribute* GetAttributes()		{ return m_aAttr;	}
		const TPlayerItemAttribute& GetAttribute(int i)	{ return m_aAttr[i];	}

		uint8_t		GetAttributeType(int i)	{ return m_aAttr[i].bType;	}
		short		GetAttributeValue(int i){ return m_aAttr[i].sValue;	}

		void		SetAttributes(const TPlayerItemAttribute* c_pAttribute);

		int		FindAttribute(uint8_t bType);
		bool		RemoveAttributeAt(int index);
		bool		RemoveAttributeType(uint8_t bType);

		bool		HasAttr(uint8_t bApply);
		bool		HasRareAttr(uint8_t bApply);

		void		SetDestroyEvent(LPEVENT pkEvent);
		void		StartDestroyEvent(int iSec=300);

		uint32_t		GetRefinedVnum()	{ return m_pProto ? m_pProto->dwRefinedVnum : 0; }
		uint32_t		GetRefineFromVnum();
		int		GetRefineLevel();

		void		SetSkipSave(bool b)	{ m_bSkipSave = b; }
		bool		GetSkipSave()		{ return m_bSkipSave; }

		bool		IsOwnership(LPCHARACTER ch);
		void		SetOwnership(LPCHARACTER ch, int iSec = 10);
		void		SetOwnershipEvent(LPEVENT pkEvent);

		uint32_t		GetLastOwnerPID()	{ return m_dwLastOwnerPID; }
		

#ifdef ENABLE_BATTLE_PASS
		bool		HaveOwnership() { return (m_pkOwnershipEvent ? true : false); }
#endif

		int		GetAttributeSetIndex(); // 속성 붙는것을 지정한 배열의 어느 인덱스를 사용하는지 돌려준다.
		void		AlterToMagicItem();
		void		AlterToSocketItem(int iSocketCount);

		uint16_t		GetRefineSet()		{ return m_pProto ? m_pProto->wRefineSet : 0;	}

		void		StartUniqueExpireEvent();
		void		SetUniqueExpireEvent(LPEVENT pkEvent);

		void		StartTimerBasedOnWearExpireEvent();
		void		SetTimerBasedOnWearExpireEvent(LPEVENT pkEvent);

		void		StartRealTimeExpireEvent();
		bool		IsRealTimeItem();
		bool		IsRealTimeFirstUseItem();
		bool		IsUnlimitedTimeUnique();

		void		StopUniqueExpireEvent();
		void		StopTimerBasedOnWearExpireEvent();
		void		StopAccessorySocketExpireEvent();

		//			일단 REAL_TIME과 TIMER_BASED_ON_WEAR 아이템에 대해서만 제대로 동작함.
		int			GetDuration();

		int		GetAttributeCount();
		void		ClearAttribute();
		void		ChangeAttribute(const int* aiChangeProb= nullptr);
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
		bool ChangeKKAK(int iAddonType = 0);
		void		AddAttribute2(uint8_t bType, short sValue);
#endif
		void		AddAttribute();
		void		AddAttribute(uint8_t bType, short sValue);

		void		ApplyAddon(int iAddonType);

		int		GetSpecialGroup() const;
		bool	IsSameSpecialGroup(const LPITEM item) const;

		// ACCESSORY_REFINE
		// 액세서리에 광산을 통해 소켓을 추가
		bool		IsAccessoryForSocket();

		int		GetAccessorySocketGrade();
		int		GetAccessorySocketMaxGrade();
		int		GetAccessorySocketDownGradeTime();

		void		SetAccessorySocketGrade(int iGrade
#ifdef ENABLE_INFINITE_RAFINES
		, bool infinite = false
#endif
		);
		void		SetAccessorySocketMaxGrade(int iMaxGrade);
		void		SetAccessorySocketDownGradeTime(uint32_t time);

		void		AccessorySocketDegrade();

		// 악세사리 를 아이템에 밖았을때 타이머 돌아가는것( 구리, 등 )
		void		StartAccessorySocketExpireEvent();
		void		SetAccessorySocketExpireEvent(LPEVENT pkEvent);

		bool		CanPutInto(LPITEM item);
#ifdef ENABLE_INFINITE_RAFINES
		bool		CanPutInto2(LPITEM item);
#endif
		// END_OF_ACCESSORY_REFINE

		void		CopyAttributeTo(LPITEM pItem);
		void		CopySocketTo(LPITEM pItem);

		int			GetRareAttrCount();
		bool		AddRareAttribute();
		bool		ChangeRareAttribute();
		bool		AddRareAttribute8();
		bool		ChangeRareAttribute8();

		void		AttrLog();

		void		Lock(bool f) { m_isLocked = f; }
		bool		isLocked() const { return m_isLocked; }

	private :
		void		SetAttribute(int i, uint8_t bType, short sValue);
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
		void		SetAttribute2(int i, uint8_t bType, short sValue);
		bool		AddRareAttribute3(uint8_t bApply, short sValue);
#endif
	public:
		void		SetForceAttribute(int i, uint8_t bType, short sValue);

	protected:
		bool		EquipEx(bool is_equip);
		bool		Unequip();
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
		void		AddAttr4(uint8_t bApply, uint8_t bLevel);
#endif
		void		AddAttr(uint8_t bApply, uint8_t bLevel);
		void		PutAttribute(const int * aiAttrPercentTable);
		void		PutAttributeWithLevel(uint8_t bLevel);

	public:
		void		AddRareAttribute2(const int * aiAttrPercentTable = nullptr);
	protected:
		void		AddRareAttr(uint8_t bApply, uint8_t bLevel);
		void		PutRareAttribute(const int * aiAttrPercentTable);
		void		PutRareAttributeWithLevel(uint8_t bLevel);
		bool		HasAnyRareAttr(uint8_t bApply) const;
	protected:
		friend class CInputDB;
		bool		OnAfterCreatedItem();			// 서버상에 아이템이 모든 정보와 함께 완전히 생성(로드)된 후 불리우는 함수.

	public:
		bool		IsRideItem();
		bool		IsRamadanRing();

		void		ClearMountAttributeAndAffect();
		bool		IsNewMountItem();

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		bool		IsMountItem();
#endif

		// 독일에서 기존 캐시 아이템과 같지만, 교환 가능한 캐시 아이템을 만든다고 하여,
		// 오리지널 아이템에, 교환 금지 플래그만 삭제한 새로운 아이템들을 새로운 아이템 대역에 할당하였다.
		// 문제는 새로운 아이템도 오리지널 아이템과 같은 효과를 내야하는데,
		// 서버건, 클라건, vnum 기반으로 되어있어
		// 새로운 vnum을 죄다 서버에 새로 다 박아야하는 안타까운 상황에 맞닿았다.
		// 그래서 새 vnum의 아이템이면, 서버에서 돌아갈 때는 오리지널 아이템 vnum으로 바꿔서 돌고 하고,
		// 저장할 때에 본래 vnum으로 바꿔주도록 한다.

		// Mask vnum은 어떤 이유(ex. 위의 상황)로 인해 vnum이 바뀌어 돌아가는 아이템을 위해 있다.
		void		SetMaskVnum(uint32_t vnum)	{	m_dwMaskVnum = vnum; }
		uint32_t		GetMaskVnum()			{	return m_dwMaskVnum; }
		bool		IsMaskedItem()	{	return m_dwMaskVnum != 0;	}

		// 용혼석
		bool		IsDragonSoul();
		int		GiveMoreTime_Per(float fPercent);
		int		GiveMoreTime_Fix(uint32_t dwTime);
#ifdef ENABLE_SOUL_SYSTEM
	public:
		void		StartSoulItemEvent();
		void		SetSoulItemEvent(LPEVENT pkEvent);
#endif

	private:
		TItemTable const * m_pProto;		// 프로토 타잎

		uint32_t		m_dwVnum;
		LPCHARACTER	m_pOwner;

		uint8_t		m_bWindow;		// 현재 아이템이 위치한 윈도우
		uint32_t		m_dwID;			// 고유번호
		bool		m_bEquipped;	// 장착 되었는가?
		uint32_t		m_dwVID;		// VID
		uint16_t		m_wCell;		// 위치
		int		m_dwCount;		// 개수
#ifdef ATTR_LOCK
		short		m_sLockedAttr;
#endif
#ifdef ENABLE_ITEM_EXTRA_PROTO
		TItemExtraProto* m_ExtraProto;
#endif
		int32_t		m_lFlag;		// 추가 flag
		uint32_t		m_dwLastOwnerPID;	// 마지막 가지고 있었던 사람의 PID

		bool		m_bExchanging;	///< 현재 교환중 상태

		int32_t		m_alSockets[ITEM_SOCKET_MAX_NUM];	// 아이템 소캣
		TPlayerItemAttribute	m_aAttr[ITEM_ATTRIBUTE_MAX_NUM];

		LPEVENT		m_pkDestroyEvent;
		LPEVENT		m_pkExpireEvent;
#ifdef ENABLE_SOUL_SYSTEM
		LPEVENT		m_pkSoulItemEvent;
#endif
		LPEVENT		m_pkUniqueExpireEvent;
		LPEVENT		m_pkTimerBasedOnWearExpireEvent;
		LPEVENT		m_pkRealTimeExpireEvent;
		LPEVENT		m_pkAccessorySocketExpireEvent;
		LPEVENT		m_pkOwnershipEvent;
		uint32_t		m_dwOwnershipPID;

		bool		m_bSkipSave;

		bool		m_isLocked;

		uint32_t		m_dwMaskVnum;
		uint32_t		m_dwSIGVnum;
	public:
		void SetSIGVnum(uint32_t dwSIG)
		{
			m_dwSIGVnum = dwSIG;
		}
		uint32_t	GetSIGVnum() const
		{
			return m_dwSIGVnum;
		}
#ifdef ENABLE_EXTRA_INVENTORY
		bool	IsExtraItem();
		uint8_t	GetExtraCategory();
#endif
#ifdef ENABLE_RUNE_SYSTEM
	public:
		bool	IsRune();
		int32_t	GetRuneAttrType(int c);
		int32_t	GetRuneAttrValue(int c, int32_t lTime);
		void	InitializeRune();
		void	ChangeRuneAttr(int32_t lTime);
		void	ActivateRuneBonus();
		void	DeactivateRuneBonus();
		void	DeactivateRuneBonusRefresh();
		void	ActivateRune();
		void	DeactivateRune();
#endif
};

EVENTINFO(item_event_info)
{
	LPITEM item;
	char szOwnerName[CHARACTER_NAME_MAX_LEN];

	item_event_info()
	: item( nullptr )
	{
		::memset( szOwnerName, 0, CHARACTER_NAME_MAX_LEN );
	}
};

EVENTINFO(item_vid_event_info)
{
	uint32_t item_vid;
#ifdef ENABLE_NEW_USE_POTION
	bool newpotion;
#endif

	item_vid_event_info()
	: item_vid( 0 )
#ifdef ENABLE_NEW_USE_POTION
	, newpotion(false)
#endif
	{
	}
};

#endif
