#pragma once

#include "AbstractPlayer.h"
#include "Packet.h"
#include "PythonSkill.h"

enum
{
	MAIN_RACE_WARRIOR_M,
	MAIN_RACE_ASSASSIN_W,
	MAIN_RACE_SURA_M,
	MAIN_RACE_SHAMAN_W,
	MAIN_RACE_WARRIOR_W,
	MAIN_RACE_ASSASSIN_M,
	MAIN_RACE_SURA_W,
	MAIN_RACE_SHAMAN_M,

	MAIN_RACE_MAX_NUM,
};

class CInstanceBase;

class CPythonPlayer : public CSingleton<CPythonPlayer>, public IAbstractPlayer
{
	public:
		enum
		{
			CATEGORY_NONE		= 0,
			CATEGORY_ACTIVE		= 1,
			CATEGORY_PASSIVE	= 2,
			CATEGORY_MAX_NUM	= 3,

			STATUS_INDEX_ST = 1,
			STATUS_INDEX_DX = 2,
			STATUS_INDEX_IQ = 3,
			STATUS_INDEX_HT = 4,
		};

		enum
		{
			MBT_LEFT,
			MBT_RIGHT,
			MBT_MIDDLE,
			MBT_NUM,
		};

		enum
		{
			MBF_SMART,
			MBF_MOVE,
			MBF_CAMERA,
			MBF_ATTACK,
			MBF_SKILL,
			MBF_AUTO,
		};

		enum
		{
			MBS_CLICK,
			MBS_PRESS,
		};

		enum EMode
		{
			MODE_NONE,
			MODE_CLICK_POSITION,
			MODE_CLICK_ITEM,
			MODE_CLICK_ACTOR,
			MODE_USE_SKILL,
		};

		enum EEffect
		{
			EFFECT_PICK,
			EFFECT_NUM,
		};

		enum EMetinSocketType
		{
			METIN_SOCKET_TYPE_NONE,
			METIN_SOCKET_TYPE_SILVER,
			METIN_SOCKET_TYPE_GOLD,
		};

		typedef struct SSkillInstance
		{
			uint32_t dwIndex;
			int iType;
			int iGrade;
			int iLevel;
			float fcurEfficientPercentage;
			float fnextEfficientPercentage;
			bool isCoolTime;

			float fCoolTime;
			float fLastUsedTime;
			bool bActive;
		} TSkillInstance;

		enum EKeyBoard_UD
		{
			KEYBOARD_UD_NONE,
			KEYBOARD_UD_UP,
			KEYBOARD_UD_DOWN,
		};

		enum EKeyBoard_LR
		{
			KEYBOARD_LR_NONE,
			KEYBOARD_LR_LEFT,
			KEYBOARD_LR_RIGHT,
		};

		enum
		{
			DIR_UP,
			DIR_DOWN,
			DIR_LEFT,
			DIR_RIGHT,
		};

		typedef struct SPlayerStatus
		{
			TItemData			aItem[c_Inventory_Count];
			TItemData			aDSItem[c_DragonSoul_Inventory_Count];
			TItemData			aMountItem[c_Mount_Inventory_Count];
#ifdef ENABLE_EXTRA_INVENTORY
			TItemData			aExtraItem[c_Extra_Inventory_Count];
#endif
#ifdef ENABLE_SWITCHBOT
			TItemData			aSwitchbotItem[SWITCHBOT_SLOT_COUNT];
#endif
			TQuickSlot			aQuickSlot[QUICKSLOT_MAX_NUM];
			TSkillInstance		aSkill[SKILL_MAX_NUM];
			int32_t				lQuickPageIndex;

			int64_t 			m_alPoint[POINT_MAX_NUM];

			void SetPoint(UINT ePoint, int64_t lPoint);

			int64_t GetPoint(UINT ePoint);


		} TPlayerStatus;

		typedef struct SPartyMemberInfo
		{
			SPartyMemberInfo(uint32_t _dwPID, const char * c_szName) : dwVID(0), dwPID(_dwPID), strName(c_szName),
			                                                           byState(0), byHPPercentage(0),
			                                                           sAffects{}
			{
			}

			uint32_t dwVID;
			uint32_t dwPID;
			std::string strName;
			uint8_t byState;
			uint8_t byHPPercentage;
			short sAffects[PARTY_AFFECT_SLOT_MAX_NUM];
		} TPartyMemberInfo;

		enum EPartyRole
		{
			PARTY_ROLE_NORMAL,
			PARTY_ROLE_LEADER,
			PARTY_ROLE_ATTACKER,
			PARTY_ROLE_TANKER,
			PARTY_ROLE_BUFFER,
			PARTY_ROLE_SKILL_MASTER,
			PARTY_ROLE_BERSERKER,
			PARTY_ROLE_DEFENDER,
			PARTY_ROLE_MAX_NUM,
		};

		enum
		{
			SKILL_NORMAL,
			SKILL_MASTER,
			SKILL_GRAND_MASTER,
			SKILL_PERFECT_MASTER,
		};

	
		struct SAutoPotionInfo
		{
			SAutoPotionInfo() : bActivated(false), currentAmount(0), totalAmount(0), inventorySlotIndex(0)
			{
			}

			bool bActivated;
			int32_t currentAmount;
			int32_t totalAmount;
			int32_t inventorySlotIndex;
		};

		enum EAutoPotionType
		{
			AUTO_POTION_TYPE_HP = 0,
			AUTO_POTION_TYPE_SP = 1,
			AUTO_POTION_TYPE_NUM
		};

	public:
		CPythonPlayer();
		~CPythonPlayer() override;
#ifdef __AUTO_QUQUE_ATTACK__
public:
	void		AutoFarmLoop();
	bool		AutoFarmQuqueSet(const bool isAdd, const uint32_t dwVirtualID);
	uint32_t		GetAutoFarmTarget();
	void		SetTotalAutoFarmCount(const uint8_t bCount) { bTotalQuqueAutoAttack = bCount; }
protected:
	uint8_t        bTotalQuqueAutoAttack = AUTO_QUQUE_ATTACK_MAX_TARGET;
	std::vector<uint32_t> m_vecQuqueAutoAttack;
#endif
	public:
		//void	PickCloseMoney();
		void	PickCloseItem();

		void	SetGameWindow(PyObject * ppyObject);

		void	SetObserverMode(bool isEnable);
		bool	IsObserverMode();

		void	SetQuickCameraMode(bool isEnable);

		void	SetAttackKeyState(bool isPress);

		void	NEW_GetMainActorPosition(TPixelPosition* pkPPosActor);

		bool	RegisterEffect(uint32_t dwEID, const char* c_szEftFileName, bool isCache);

		bool	NEW_SetMouseState(int eMBType, int eMBState);
		bool	NEW_SetMouseFunc(int eMBType, int eMBFunc);
		int		NEW_GetMouseFunc(int eMBT);
		void	NEW_SetMouseMiddleButtonState(int eMBState);

#ifdef ATTR_LOCK
		void	SetItemAttrLocked(TItemPos itemPos, short dwIndex);
		short	GetItemAttrLocked(TItemPos itemPos);
#endif

		void	NEW_SetAutoCameraRotationSpeed(float fRotSpd);
		void	NEW_ResetCameraRotation();

		void	NEW_SetSingleDirKeyState(int eDirKey, bool isPress);
		void	NEW_SetSingleDIKKeyState(int eDIKKey, bool isPress);
		void	NEW_SetMultiDirKeyState(bool isLeft, bool isRight, bool isUp, bool isDown);

		void	NEW_Attack();
		void	NEW_Fishing();
		bool	NEW_CancelFishing();

		void	NEW_LookAtFocusActor();
		bool	NEW_IsAttackableDistanceFocusActor();


		bool	NEW_MoveToDestPixelPositionDirection(const TPixelPosition& c_rkPPosDst);
		bool	NEW_MoveToMousePickedDirection();
		bool	NEW_MoveToMouseScreenDirection();
		bool	NEW_MoveToDirection(float fDirRot);
		void	NEW_Stop();


		// Reserved
		bool	NEW_IsEmptyReservedDelayTime(float fElapsedtime);


		// Dungeon
		void	SetDungeonDestinationPosition(int ix, int iy);
		void	AlarmHaveToGo();


		CInstanceBase* NEW_FindActorPtr(uint32_t dwVID);
		CInstanceBase* NEW_GetMainActorPtr();

		// flying target set
		void	Clear();
		void	ClearSkillDict();
		void	NEW_ClearSkillData(bool bAll = false);

		void	Update();


		// Play Time
		uint32_t	GetPlayTime();
		void	SetPlayTime(uint32_t dwPlayTime);


		// System
		void	SetMainCharacterIndex(uint32_t iIndex);

		uint32_t	GetMainCharacterIndex();
		bool	IsMainCharacterIndex(uint32_t dwIndex);
		uint32_t	GetGuildID();
		void	NotifyDeletingCharacterInstance(uint32_t dwVID);
		void	NotifyCharacterDead(uint32_t dwVID);
		void	NotifyCharacterUpdate(uint32_t dwVID);
		void	NotifyDeadMainCharacter();
		void	NotifyChangePKMode();


		// Player Status
		const char *	GetName();
		void	SetName(const char *name);

		void	SetRace(uint32_t dwRace);
		uint32_t	GetRace();

		void	SetWeaponPower(uint32_t dwMinPower, uint32_t dwMaxPower, uint32_t dwMinMagicPower, uint32_t dwMaxMagicPower, uint32_t dwAddPower);

		void	SetStatus(uint32_t dwType, int64_t lValue);
		int64_t		GetStatus(uint32_t dwType);

		// Item
		void	MoveItemData(TItemPos SrcCell, TItemPos DstCell);
		void	SetItemData(TItemPos Cell, const TItemData & c_rkItemInst);
		const TItemData * GetItemData(TItemPos Cell) const;
		void	SetItemCount(TItemPos Cell,

		int byCount

		);
		void	SetItemMetinSocket(TItemPos Cell, uint32_t dwMetinSocketIndex, uint32_t dwMetinNumber);
		void	SetItemAttribute(TItemPos Cell, uint32_t dwAttrIndex, uint8_t byType, short sValue);
		uint32_t	GetItemIndex(TItemPos Cell);
		uint32_t	GetItemFlags(TItemPos Cell);
		uint32_t	GetItemAntiFlags(TItemPos Cell);
		uint8_t	GetItemTypeBySlot(TItemPos Cell);
		uint8_t	GetItemSubTypeBySlot(TItemPos Cell);
		uint32_t	GetItemCount(TItemPos Cell);
		uint32_t	GetItemCountByVnum(uint32_t dwVnum);
#ifdef ENABLE_EXTRA_INVENTORY
		//To fix search item in refine (0)
		uint32_t	GetItemCountbyVnumExtraInventory(uint32_t dwVnum);
#endif
		uint32_t	GetItemMetinSocket(TItemPos Cell, uint32_t dwMetinSocketIndex);
		void	GetItemAttribute(TItemPos Cell, uint32_t dwAttrSlotIndex, uint8_t* pbyType, short * psValue);
		void	SendClickItemPacket(uint32_t dwIID);

		void	RequestAddLocalQuickSlot(uint32_t dwLocalSlotIndex, uint32_t dwWndType, uint32_t dwWndItemPos);
		void	RequestAddToEmptyLocalQuickSlot(uint32_t dwWndType, uint32_t dwWndItemPos);
		void	RequestMoveGlobalQuickSlotToLocalQuickSlot(uint32_t dwGlobalSrcSlotIndex, uint32_t dwLocalDstSlotIndex);
		void	RequestDeleteGlobalQuickSlot(uint32_t dwGlobalSlotIndex);
		void	RequestUseLocalQuickSlot(uint32_t dwLocalSlotIndex);
		uint32_t	LocalQuickSlotIndexToGlobalQuickSlotIndex(uint32_t dwLocalSlotIndex);

		void	GetGlobalQuickSlotData(uint32_t dwGlobalSlotIndex, uint32_t* pdwWndType, uint32_t* pdwWndItemPos);
		void	GetLocalQuickSlotData(uint32_t dwSlotPos, uint32_t* pdwWndType, uint32_t* pdwWndItemPos);
		void	RemoveQuickSlotByValue(int iType, int iPosition);

		char	IsItem(TItemPos SlotIndex);

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
		bool    IsBeltInventorySlot(TItemPos Cell);
#endif
		bool	IsInventorySlot(TItemPos SlotIndex);
		bool	IsEquipmentSlot(TItemPos SlotIndex);

		bool	IsEquipItemInSlot(TItemPos iSlotIndex);


		// Quickslot
		int		GetQuickPage();
		void	SetQuickPage(int nPageIndex);
		void	AddQuickSlot(int QuickslotIndex, char IconType, char IconPosition);
		void	DeleteQuickSlot(int QuickslotIndex);
		void	MoveQuickSlot(int Source, int Target);


		// Skill
		void	SetSkill(uint32_t dwSlotIndex, uint32_t dwSkillIndex);
		bool	GetSkillSlotIndex(uint32_t dwSkillIndex, uint32_t* pdwSlotIndex);
		int		GetSkillIndex(uint32_t dwSlotIndex);
		int		GetSkillGrade(uint32_t dwSlotIndex);
		int		GetSkillLevel(uint32_t dwSlotIndex);
		float	GetSkillCurrentEfficientPercentage(uint32_t dwSlotIndex);
		float	GetSkillNextEfficientPercentage(uint32_t dwSlotIndex);

#ifdef ENABLE_NEW_PASSIVE_SKILLS
		float	GetSkillNextEfficientPercentageByLvl(uint32_t dwSlotIndex, uint32_t dwLevel);
#endif

		void	SetSkillLevel(uint32_t dwSlotIndex, uint32_t dwSkillLevel);
		void	SetSkillLevel_(uint32_t dwSkillIndex, uint32_t dwSkillGrade, uint32_t dwSkillLevel);
		bool	IsToggleSkill(uint32_t dwSlotIndex);
		void	ClickSkillSlot(uint32_t dwSlotIndex);
		void	ChangeCurrentSkillNumberOnly(uint32_t dwSlotIndex);
		bool	FindSkillSlotIndexBySkillIndex(uint32_t dwSkillIndex, uint32_t * pdwSkillSlotIndex);

		void	SetSkillCoolTime(uint32_t dwSkillIndex);
		void	EndSkillCoolTime(uint32_t dwSkillIndex);

		float	GetSkillCoolTime(uint32_t dwSlotIndex);
		float	GetSkillElapsedCoolTime(uint32_t dwSlotIndex);
		bool	IsSkillActive(uint32_t dwSlotIndex);
		bool	IsSkillCoolTime(uint32_t dwSlotIndex);
		void	UseGuildSkill(uint32_t dwSkillSlotIndex);
		bool	AffectIndexToSkillSlotIndex(UINT uAffect, uint32_t* pdwSkillSlotIndex);
		bool	AffectIndexToSkillIndex(uint32_t dwAffectIndex, uint32_t * pdwSkillIndex);
#if defined(ENABLE_RENEWAL_AFFECT_SHOWER)
		bool	SkillIndexToAffectIndex(uint32_t dwSkillIndex, uint32_t* pdwAffectIndex);
#endif
		void	SetAffect(uint32_t uAffect);
		void	ResetAffect(uint32_t uAffect);
		void	ClearAffects();


		// Target
		void	SetTarget(uint32_t dwVID, bool bForceChange = true);
		void	OpenCharacterMenu(uint32_t dwVictimActorID);
		uint32_t	GetTargetVID();


		// Party
		void	ExitParty();
		void	AppendPartyMember(uint32_t dwPID, const char * c_szName);
		void	LinkPartyMember(uint32_t dwPID, uint32_t dwVID);
		void	UnlinkPartyMember(uint32_t dwPID);
		void	UpdatePartyMemberInfo(uint32_t dwPID, uint8_t byState, uint8_t byHPPercentage);
		void	UpdatePartyMemberAffect(uint32_t dwPID, uint8_t byAffectSlotIndex, short sAffectNumber);
		void	RemovePartyMember(uint32_t dwPID);
		bool	IsPartyMemberByVID(uint32_t dwVID);
		bool	IsPartyMemberByName(const char * c_szName);
		bool	GetPartyMemberPtr(uint32_t dwPID, TPartyMemberInfo ** ppPartyMemberInfo);
		bool	PartyMemberPIDToVID(uint32_t dwPID, uint32_t * pdwVID);
		bool	PartyMemberVIDToPID(uint32_t dwVID, uint32_t * pdwPID);
		bool	IsSamePartyMember(uint32_t dwVID1, uint32_t dwVID2);


		// Fight
		void	RememberChallengeInstance(uint32_t dwVID);
		void	RememberRevengeInstance(uint32_t dwVID);
		void	RememberCantFightInstance(uint32_t dwVID);
		void	ForgetInstance(uint32_t dwVID);
		bool	IsChallengeInstance(uint32_t dwVID);
		bool	IsRevengeInstance(uint32_t dwVID);
		bool	IsCantFightInstance(uint32_t dwVID);


		// Private Shop
		void	OpenPrivateShop();
		void	ClosePrivateShop();
		bool	IsOpenPrivateShop();



		// Stamina
		void	StartStaminaConsume(uint32_t dwConsumePerSec, uint32_t dwCurrentStamina);
		void	StopStaminaConsume(uint32_t dwCurrentStamina);


		// PK Mode
		uint32_t	GetPKMode();


		// Combo
		void	SetComboSkillFlag(bool bFlag);


		// System
		void	SetMovableGroundDistance(float fDistance);


		// Emotion
		void	ActEmotion(uint32_t dwEmotionID);
		void	StartEmotionProcess();
		void	EndEmotionProcess();


		// Function Only For Console System
		bool	__ToggleCoolTime();
		bool	__ToggleLevelLimit();

		__inline const	SAutoPotionInfo& GetAutoPotionInfo(int type) const	{ return m_kAutoPotionInfo[type]; }
		__inline		SAutoPotionInfo& GetAutoPotionInfo(int type)		{ return m_kAutoPotionInfo[type]; }
		__inline void					 SetAutoPotionInfo(int type, const SAutoPotionInfo& info)	{ m_kAutoPotionInfo[type] = info; }

	protected:
		TQuickSlot &	__RefLocalQuickSlot(int SlotIndex);
		TQuickSlot &	__RefGlobalQuickSlot(int SlotIndex);


		uint32_t	__GetLevelAtk();
		uint32_t	__GetStatAtk();
		uint32_t	__GetWeaponAtk(uint32_t dwWeaponPower);
		uint32_t	__GetTotalAtk(uint32_t dwWeaponPower, uint32_t dwRefineBonus);
		uint32_t	__GetRaceStat();
		uint32_t	__GetHitRate();
		uint32_t	__GetEvadeRate();

		void	__UpdateBattleStatus();

		void	__DeactivateSkillSlot(uint32_t dwSlotIndex);
		void	__ActivateSkillSlot(uint32_t dwSlotIndex);

		void	__OnPressSmart(CInstanceBase& rkInstMain, bool isAuto);
		void	__OnClickSmart(CInstanceBase& rkInstMain, bool isAuto);

		void	__OnPressItem(CInstanceBase& rkInstMain, uint32_t dwPickedItemID);
		void	__OnPressActor(CInstanceBase& rkInstMain, uint32_t dwPickedActorID, bool isAuto);
		void	__OnPressGround(CInstanceBase& rkInstMain, const TPixelPosition& c_rkPPosPickedGround);
		void	__OnPressScreen(CInstanceBase& rkInstMain);

		void	__OnClickActor(CInstanceBase& rkInstMain, uint32_t dwPickedActorID, bool isAuto);
		void	__OnClickItem(CInstanceBase& rkInstMain, uint32_t dwPickedItemID);
		void	__OnClickGround(CInstanceBase& rkInstMain, const TPixelPosition& c_rkPPosPickedGround);

		bool	__IsMovableGroundDistance(CInstanceBase& rkInstMain, const TPixelPosition& c_rkPPosPickedGround);

		bool	__GetPickedActorPtr(CInstanceBase** pkInstPicked);

		bool	__GetPickedActorID(uint32_t* pdwActorID);
		bool	__GetPickedItemID(uint32_t* pdwItemID);
		bool	__GetPickedGroundPos(TPixelPosition* pkPPosPicked);

		void	__ClearReservedAction();
		void	__ReserveClickItem(uint32_t dwItemID);
		void	__ReserveClickActor(uint32_t dwActorID);
		void	__ReserveClickGround(const TPixelPosition& c_rkPPosPickedGround);
		void	__ReserveUseSkill(uint32_t dwActorID, uint32_t dwSkillSlotIndex, uint32_t dwRange);

		void	__ReserveProcess_ClickActor();

		void	__ShowPickedEffect(const TPixelPosition& c_rkPPosPickedGround);
		void	__SendClickActorPacket(CInstanceBase& rkInstVictim);

		void	__ClearAutoAttackTargetActorID();
		void	__SetAutoAttackTargetActorID(uint32_t dwActorID);

		void	NEW_ShowEffect(int dwEID, TPixelPosition kPPosDst);

		void	NEW_SetMouseSmartState(int eMBS, bool isAuto);
		void	NEW_SetMouseMoveState(int eMBS);
		void	NEW_SetMouseCameraState(int eMBS);
		void	NEW_GetMouseDirRotation(float fScrX, float fScrY, float* pfDirRot);
		void	NEW_GetMultiKeyDirRotation(bool isLeft, bool isRight, bool isUp, bool isDown, float* pfDirRot);

		float	GetDegreeFromDirection(int iUD, int iLR);
		float	GetDegreeFromPosition(int ix, int iy, int iHalfWidth, int iHalfHeight);

		bool	CheckCategory(int iCategory);
		bool	CheckAbilitySlot(int iSlotIndex);

		void	RefreshKeyWalkingDirection();
		void	NEW_RefreshMouseWalkingDirection();


		// Instances
		void	RefreshInstances();

		bool	__CanShot(CInstanceBase& rkInstMain, CInstanceBase& rkInstTarget);
		bool	__CanUseSkill();

		bool	__CanMove();

		bool	__CanAttack();
		bool	__CanChangeTarget();

		bool	__CheckSkillUsable(uint32_t dwSlotIndex);
		void	__UseCurrentSkill();
		void	__UseChargeSkill(uint32_t dwSkillSlotIndex);
		bool	__UseSkill(uint32_t dwSlotIndex);
		bool	__CheckSpecialSkill(uint32_t dwSkillIndex);

		bool	__CheckRestSkillCoolTime(uint32_t dwSkillSlotIndex);
		bool	__CheckShortLife(TSkillInstance & rkSkillInst, CPythonSkill::TSkillData& rkSkillData);
		bool	__CheckShortMana(TSkillInstance & rkSkillInst, CPythonSkill::TSkillData& rkSkillData);
		bool	__CheckShortArrow(TSkillInstance & rkSkillInst, CPythonSkill::TSkillData& rkSkillData);
		bool	__CheckDashAffect(CInstanceBase& rkInstMain);

		void	__SendUseSkill(uint32_t dwSkillSlotIndex, uint32_t dwTargetVID);
		void	__RunCoolTime(uint32_t dwSkillSlotIndex);

		uint8_t	__GetSkillType(uint32_t dwSkillSlotIndex);

		bool	__IsReservedUseSkill(uint32_t dwSkillSlotIndex);
		bool	__IsMeleeSkill(CPythonSkill::TSkillData& rkSkillData);
		bool	__IsChargeSkill(CPythonSkill::TSkillData& rkSkillData);
		uint32_t	__GetSkillTargetRange(CPythonSkill::TSkillData& rkSkillData);
		bool	__SearchNearTarget();
		bool	__IsUsingChargeSkill();

		bool	__ProcessEnemySkillTargetRange(CInstanceBase& rkInstMain, CInstanceBase& rkInstTarget, CPythonSkill::TSkillData& rkSkillData, uint32_t dwSkillSlotIndex);


		// Item
		bool	__HasEnoughArrow();
		bool	__HasItem(uint32_t dwItemID);
#ifndef ENABLE_INSTANT_PICKUPd
		uint32_t	__GetPickableDistance();
#endif

		// Target
		CInstanceBase*		__GetTargetActorPtr();
		void				__ClearTarget();
		uint32_t				__GetTargetVID();
		void				__SetTargetVID(uint32_t dwVID);
		bool				__IsSameTargetVID(uint32_t dwVID);
		bool				__IsTarget();
		bool				__ChangeTargetToPickedInstance();

		CInstanceBase *		__GetSkillTargetInstancePtr(CPythonSkill::TSkillData& rkSkillData);
		CInstanceBase *		__GetAliveTargetInstancePtr();
		CInstanceBase *		__GetDeadTargetInstancePtr();

		bool				__IsRightButtonSkillMode();


		// Update
		void				__Update_AutoAttack();
		void				__Update_NotifyGuildAreaEvent();



		// Emotion
		bool				__IsProcessingEmotion();


	protected:
		PyObject *				m_ppyGameWindow;

		// Client Player Data
		std::map<uint32_t, uint32_t>	m_skillSlotDict;
		std::string				m_stName;
		uint32_t					m_dwMainCharacterIndex;
		uint32_t					m_dwRace;
		uint32_t					m_dwWeaponMinPower;
		uint32_t					m_dwWeaponMaxPower;
		uint32_t					m_dwWeaponMinMagicPower;
		uint32_t					m_dwWeaponMaxMagicPower;
		uint32_t					m_dwWeaponAddPower;

		// Todo
		uint32_t					m_dwSendingTargetVID;
		float					m_fTargetUpdateTime;

		// Attack
		uint32_t					m_dwAutoAttackTargetVID;

		// NEW_Move
		EMode					m_eReservedMode;
		float					m_fReservedDelayTime;

		float					m_fMovDirRot;

		bool					m_isUp;
		bool					m_isDown;
		bool					m_isLeft;
		bool					m_isRight;
		bool					m_isAtkKey;
		bool					m_isDirKey;
		bool					m_isCmrRot;
		bool					m_isSmtMov;
		bool					m_isDirMov;

		float					m_fCmrRotSpd;

		TPlayerStatus			m_playerStatus;

		UINT					m_iComboOld;
		uint32_t					m_dwVIDReserved;
		uint32_t					m_dwIIDReserved;

		uint32_t					m_dwcurSkillSlotIndex;
		uint32_t					m_dwSkillSlotIndexReserved;
		uint32_t					m_dwSkillRangeReserved;

		TPixelPosition			m_kPPosInstPrev;
		TPixelPosition			m_kPPosReserved;

		// Emotion
		bool					m_bisProcessingEmotion;

		// Dungeon
		bool					m_isDestPosition;
		int						m_ixDestPos;
		int						m_iyDestPos;
		int						m_iLastAlarmTime;

		// Party
		std::map<uint32_t, TPartyMemberInfo>	m_PartyMemberMap;

		// PVP
		std::set<uint32_t>			m_ChallengeInstanceSet;
		std::set<uint32_t>			m_RevengeInstanceSet;
		std::set<uint32_t>			m_CantFightInstanceSet;

		// Private Shop
		bool					m_isOpenPrivateShop;
		bool					m_isObserverMode;

		// Stamina
		bool					m_isConsumingStamina;
		float					m_fCurrentStamina;
		float					m_fConsumeStaminaPerSec;

		// Guild
		uint32_t					m_inGuildAreaID;


		// System
		bool					m_sysIsCoolTime;
		bool					m_sysIsLevelLimit;

	protected:
		// Game Cursor Data
		TPixelPosition			m_MovingCursorPosition;
		float					m_fMovingCursorSettingTime;
		uint32_t					m_adwEffect[EFFECT_NUM];

		uint32_t					m_dwVIDPicked;
		uint32_t					m_dwIIDPicked;
		int						m_aeMBFButton[MBT_NUM];

		uint32_t					m_dwTargetVID;
		uint32_t					m_dwTargetEndTime;
		uint32_t					m_dwPlayTime;

		SAutoPotionInfo			m_kAutoPotionInfo[AUTO_POTION_TYPE_NUM];

	protected:
		float					MOVABLE_GROUND_DISTANCE;

	private:
		std::map<uint32_t, uint32_t> m_kMap_dwAffectIndexToSkillIndex;
#if defined(ENABLE_RENEWAL_AFFECT_SHOWER)
		std::map<uint32_t, uint32_t> m_kMap_dwSkillIndexToAffectIndex;
#endif
#ifdef ENABLE_AUTO_PICKUP
	private:
		Event::EventHandle m_PickupEventHandle;
	public:
		void SetPickUpEventHandle(const Event::EventHandle& Handle) {
			m_PickupEventHandle = Handle;
		}
#endif
};

extern const int c_iFastestSendingCount;
extern const int c_iSlowestSendingCount;
extern const float c_fFastestSendingDelay;
extern const float c_fSlowestSendingDelay;
extern const float c_fRotatingStepTime;

extern const float c_fComboDistance;
extern const float c_fPickupDistance;
extern const float c_fClickDistance;
