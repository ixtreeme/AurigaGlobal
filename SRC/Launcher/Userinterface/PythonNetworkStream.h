#pragma once

#include "../Render/FuncObject.h"
#include "../Render/NetStream.h"
#include "../Render/NetPacketHeaderMap.h"
#ifdef ENABLE_SWITCHBOT
#include "PythonSwitchbot.h"
#endif
#include "InsultChecker.h"

#include "packet.h"

class CInstanceBase;
class CNetworkActorManager;
struct SNetworkActorData;
struct SNetworkUpdateActorData;

class CPythonNetworkStream : public CNetworkStream, public CSingleton<CPythonNetworkStream>
{
	public:
		enum
		{
			SERVER_COMMAND_LOG_OUT = 0,
			SERVER_COMMAND_RETURN_TO_SELECT_CHARACTER = 1,
			SERVER_COMMAND_QUIT = 2,

			MAX_ACCOUNT_PLAYER
		};

		enum
		{
			ERROR_NONE,
			ERROR_UNKNOWN,
			ERROR_CONNECT_MARK_SERVER,
			ERROR_LOAD_MARK,
			ERROR_MARK_WIDTH,
			ERROR_MARK_HEIGHT,

			// MARK_BUG_FIX
			ERROR_MARK_UPLOAD_NEED_RECONNECT,
			ERROR_MARK_CHECK_NEED_RECONNECT,
			// END_OF_MARK_BUG_FIX
		};

		enum
		{
			ACCOUNT_CHARACTER_SLOT_ID,
			ACCOUNT_CHARACTER_SLOT_NAME,
			ACCOUNT_CHARACTER_SLOT_RACE,
			ACCOUNT_CHARACTER_SLOT_LEVEL,
			ACCOUNT_CHARACTER_SLOT_STR,
			ACCOUNT_CHARACTER_SLOT_DEX,
			ACCOUNT_CHARACTER_SLOT_HTH,
			ACCOUNT_CHARACTER_SLOT_INT,
			ACCOUNT_CHARACTER_SLOT_PLAYTIME,
			ACCOUNT_CHARACTER_SLOT_FORM,
			ACCOUNT_CHARACTER_SLOT_ADDR,
			ACCOUNT_CHARACTER_SLOT_PORT,
			ACCOUNT_CHARACTER_SLOT_GUILD_ID,
			ACCOUNT_CHARACTER_SLOT_GUILD_NAME,
			ACCOUNT_CHARACTER_SLOT_CHANGE_NAME_FLAG,
			ACCOUNT_CHARACTER_SLOT_HAIR,
#ifdef ENABLE_ACCE_SYSTEM
			ACCOUNT_CHARACTER_SLOT_ACCE,
#endif
		};

		enum
		{
			PHASE_WINDOW_LOGO,
			PHASE_WINDOW_LOGIN,
			PHASE_WINDOW_SELECT,
			PHASE_WINDOW_CREATE,
			PHASE_WINDOW_LOAD,
			PHASE_WINDOW_GAME,
			PHASE_WINDOW_EMPIRE,
			PHASE_WINDOW_NUM,
		};

	public:
		CPythonNetworkStream();
		virtual ~CPythonNetworkStream();

		bool SendSpecial(int nLen, void * pvBuf);


		void StartGame();
		void Warp(int32_t lGlobalX, int32_t lGlobalY);

		void SetWaitFlag();

		void Discord_Start();
		void Discord_Close();
		void Discord_Update(int lvl = 0);

		void SendEmoticon(UINT eEmoticon);

		void ExitApplication();
		void ExitGame();
		void LogOutGame();
		void AbsoluteExitGame();
		void AbsoluteExitApplication();

		void EnableChatInsultFilter(bool isEnable);
		bool IsChatInsultIn(const char* c_szMsg);
		bool IsInsultIn(const char* c_szMsg);

		uint32_t GetGuildID();

		UINT UploadMark(const char* c_szImageFileName);
		UINT UploadSymbol(const char* c_szImageFileName);

		bool LoadInsultList(const char* c_szInsultListFileName);
	

		UINT		GetAccountCharacterSlotDatau(UINT iSlot, UINT eType);
		const char* GetAccountCharacterSlotDataz(UINT iSlot, UINT eType);

		// SUPPORT_BGM
		const char*		GetFieldMusicFileName();
		float			GetFieldMusicVolume();
		// END_OF_SUPPORT_BGM

		bool IsSelectedEmpire();

		void ToggleGameDebugInfo();
#if defined(__BL_PICK_FILTER__)
		void OpenPickUpWindow();
#endif
		void SetMarkServer(const char* c_szAddr, UINT uPort);
		void ConnectLoginServer(const char* c_szAddr, UINT uPort);
		void ConnectGameServer(UINT iChrSlot);

		void SetLoginInfo(const char* c_szID, const char* c_szPassword);
		void SetLoginKey(uint32_t dwLoginKey);
		void ClearLoginInfo( void );

		void SetHandler(PyObject* poHandler);
		void SetPhaseWindow(UINT ePhaseWnd, PyObject* poPhaseWnd);
		void ClearPhaseWindow(UINT ePhaseWnd, PyObject* poPhaseWnd);
		void SetServerCommandParserWindow(PyObject* poPhaseWnd);
#ifdef ENABLE_OPENSHOP_PACKET
		bool SendOpenShopPacket(int32_t shopid);
#endif
		bool SendSyncPositionElementPacket(uint32_t dwVictimVID, int32_t dwVictimX, int32_t dwVictimY);

		bool SendAttackPacket(
#ifdef ENABLE_NEW_ATTACK_METHOD
		uint32_t type,
#endif
		UINT uMotAttack, uint32_t dwVIDVictim
#ifdef ENABLE_NEW_ATTACK_METHOD
		, uint32_t crc32
#endif
		);
		bool SendCharacterStatePacket(const TPixelPosition& c_rkPPosDst, float fDstRot, uint8_t eFunc, uint8_t uArg);
		bool SendUseSkillPacket(uint32_t dwSkillIndex, uint32_t dwTargetVID=0);

#ifdef ENABLE_SKILL_COLOR_SYSTEM
		bool SendSkillColorPacket(uint8_t bSkillSlot, uint32_t dwColor1, uint32_t dwColor2, uint32_t dwColor3, uint32_t dwColor4, uint32_t dwColor5);
#endif

		bool SendTargetPacket(uint32_t dwVID);

		// OLDCODE:
		bool SendCharacterStartWalkingPacket(float fRotation, long lx, long ly);
		bool SendCharacterEndWalkingPacket(float fRotation, long lx, long ly);
		bool SendCharacterCheckWalkingPacket(float fRotation, long lx, long ly);

		bool SendCharacterPositionPacket(uint8_t iPosition);

		bool SendItemUsePacket(TItemPos pos);
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		bool WheelDestiny(const uint8_t option);
#endif
		bool SendItemUseToItemPacket(TItemPos source_pos, TItemPos target_pos);
		bool SendItemDropPacket(TItemPos pos, int64_t elk);
		bool SendItemDestroyPacket(TItemPos pos);
		bool SendItemDivisionPacket(TItemPos pos);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		bool Envanter_paketi(/*TItemPos pos*/);
#endif
		bool SendItemDropPacketNew(TItemPos pos, int64_t elk, int count);
		bool SendItemMovePacket(TItemPos pos, TItemPos change_pos, int num);
		bool SendItemPickUpPacket(uint32_t vid);

		bool SendWikiRequestInfo(unsigned long long retID, uint32_t vnum, bool isMob);
		void ToggleWikiWindow();

		bool SendQuickSlotAddPacket(uint8_t wpos, uint8_t type, WORD pos);
		bool SendQuickSlotDelPacket(uint8_t wpos);
		bool SendQuickSlotMovePacket(uint8_t wpos, uint8_t change_pos);

		// PointReset 개 임시
		bool SendPointResetPacket();

		// Shop
		bool SendShopEndPacket();
		bool SendShopBuyPacket(uint8_t bPos);
#ifdef ENABLE_BUY_STACK_FROM_SHOP
		bool SendShopBuyMultiplePacket(uint8_t p, uint8_t c);
#endif
		bool SendShopSellPacket(uint8_t bySlot);
#ifdef ENABLE_EXTRA_INVENTORY
		bool SendShopSellPacketNew(TItemPos pos, int byCount);
#else
		bool SendShopSellPacketNew(uint8_t bySlot, int byCount);
#endif

		// Exchange
		bool SendExchangeStartPacket(uint32_t vid);
		bool SendExchangeItemAddPacket(TItemPos ItemPos, uint8_t byDisplayPos);
		bool SendExchangeElkAddPacket(int64_t elk);
		bool SendExchangeItemDelPacket(uint8_t pos);
		bool SendExchangeAcceptPacket();
		bool SendExchangeExitPacket();

#ifdef ENABLE_EVENT_MANAGER
		bool RecvEventManager();
#endif

		// Quest
		bool SendScriptAnswerPacket(int iAnswer);
		bool SendScriptButtonPacket(unsigned int iIndex);
		bool SendAnswerMakeGuildPacket(const char * c_szName);
		bool SendQuestInputStringPacket(const char * c_szString);
		bool SendQuestConfirmPacket(uint8_t byAnswer, uint32_t dwPID);

		// Event
		bool SendOnClickPacket(uint32_t vid);

		// Fly
		bool SendFlyTargetingPacket(uint32_t dwTargetVID, const TPixelPosition& kPPosTarget);
		bool SendAddFlyTargetingPacket(uint32_t dwTargetVID, const TPixelPosition& kPPosTarget);
		bool SendShootPacket(UINT uSkill);

		// Command
		bool ClientCommand(const char * c_szCommand);
		void ServerCommand(char * c_szCommand);

		// Emoticon
		void RegisterEmoticonString(const char * pcEmoticonString);

		// Party
		bool SendPartyInvitePacket(uint32_t dwVID);
		bool SendPartyInviteAnswerPacket(uint32_t dwLeaderVID, uint8_t byAccept);
		bool SendPartyRemovePacket(uint32_t dwPID);
		bool SendPartySetStatePacket(uint32_t dwVID, uint8_t byState, uint8_t byFlag);
		bool SendPartyUseSkillPacket(uint8_t bySkillIndex, uint32_t dwVID);
		bool SendPartyParameterPacket(uint8_t byDistributeMode);

		// SafeBox
		bool SendSafeBoxMoneyPacket(uint8_t byState, uint32_t dwMoney);
		bool SendSafeBoxCheckinPacket(TItemPos InventoryPos, uint32_t bySafeBoxPos);
		bool RecvMountInventoryPacket();
		bool SendSafeBoxCheckoutPacket(uint32_t bySafeBoxPos, TItemPos InventoryPos);
		bool SendSafeBoxItemMovePacket(uint32_t bySourcePos, uint8_t byTargetPos, int byCount);
		// Mall
		bool SendMallCheckoutPacket(uint8_t byMallPos, TItemPos InventoryPos);

		bool SendMountInventoryCheckinPacket(TItemPos inventoryPos, uint16_t wMountPos);
		bool SendMountInventoryCheckoutPacket(uint16_t wMountPos, TItemPos inventoryPos);
		bool SendMountInventoryItemMovePacket(uint16_t wMountPos, uint16_t wDestPos);

		// Guild
		bool SendGuildAddMemberPacket(uint32_t dwVID);
		bool SendGuildRemoveMemberPacket(uint32_t dwPID);
		bool SendGuildChangeGradeNamePacket(uint8_t byGradeNumber, const char * c_szName);
		bool SendGuildChangeGradeAuthorityPacket(uint8_t byGradeNumber, uint8_t byAuthority);
		bool SendGuildOfferPacket(uint32_t dwExperience);
		bool SendGuildPostCommentPacket(const char * c_szMessage);
		bool SendGuildDeleteCommentPacket(uint32_t dwIndex);
		bool SendGuildRefreshCommentsPacket(uint32_t dwHighestIndex);
		bool SendGuildChangeMemberGradePacket(uint32_t dwPID, uint8_t byGrade);
		bool SendGuildUseSkillPacket(uint32_t dwSkillID, uint32_t dwTargetVID);
		bool SendGuildChangeMemberGeneralPacket(uint32_t dwPID, uint8_t byFlag);
		bool SendGuildInvitePacket(uint32_t dwVID);
		bool SendGuildInviteAnswerPacket(uint32_t dwGuildID, uint8_t byAnswer);
		bool SendGuildChargeGSPPacket(uint32_t dwMoney);
		bool SendGuildDepositMoneyPacket(uint32_t dwMoney);
		bool SendGuildWithdrawMoneyPacket(uint32_t dwMoney);
#ifdef NEW_PET_SYSTEM
		bool PetSetNamePacket(const char * petname);
#endif
		// Mall
		bool RecvMallOpenPacket();
		bool RecvMallItemSetPacket();
		bool RecvMallItemDelPacket();
#ifdef __ENABLE_NEW_OFFLINESHOP__
		bool RecvOfflineshopPacket();


		bool RecvOfflineshopShopList();
		bool RecvOfflineshopShopOpen();
		bool RecvOfflineshopShopOpenOwner();
		bool RecvOfflineshopShopOpenOwnerNoShop();
		bool RecvOfflineshopShopClose();
		bool RecvOfflineshopShopFilterResult();
		bool RecvOfflineshopOfferList();
		bool RecvOfflineshopShopSafeboxRefresh();
		bool RecvOfflineshopShopBuyItemFromSearch();

		bool RecvOfflineshopAuctionList();
		bool RecvOfflineshopOpenMyAuction();
		bool RecvOfflineshopOpenMyAuctionNoAuction();
		bool RecvOfflineshopOpenAuction();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		bool RecvOfflineshopInsertEntity();
		bool RecvOfflineshopRemoveEntity();

		void SendOfflineshopOnClickShopEntity(uint32_t dwPickedShopVID);
#endif


		void SendOfflineshopShopCreate(const offlineshop::TShopInfo& shopInfo, const std::vector<offlineshop::TShopItemInfo>& items);
		void SendOfflineshopChangeName(const char* szName);
		void SendOfflineshopForceCloseShop();

		void SendOfflineshopRequestShopList();

		void SendOfflineshopOpenShop(uint32_t dwOwnerID);
		void SendOfflineshopOpenShopOwner();

		void SendOfflineshopBuyItem(uint32_t dwOwnerID, uint32_t dwItemID, bool isSearch, long long TotalPriceSeen);
		
		void SendOfflineshopAddItem(offlineshop::TShopItemInfo& itemInfo);
		void SendOfflineshopRemoveItem(uint32_t dwItemID);
		void SendOfflineShopEditItem(uint32_t dwItemID, const offlineshop::TPriceInfo& price);

		void SendOfflineshopFilterRequest(const offlineshop::TFilterInfo& filter);
		
		void SendOfflineshopOfferCreate(const offlineshop::TOfferInfo& offer);
		void SendOfflineshopOfferAccept(uint32_t dwOfferID);
		void SendOfflineshopOfferCancel(uint32_t dwOfferID, uint32_t dwOwnerID);
		void SendOfflineshopOfferListRequest();

		void SendOfflineshopSafeboxOpen();
		void SendOfflineshopSafeboxGetItem(uint32_t dwItemID);
		void SendOfflineshopSafeboxGetValutes(const offlineshop::TValutesInfo& valutes);
		void SendOfflineshopSafeboxClose();

		//AUCTION
		void SendOfflineshopAuctionListRequest();
		void SendOfflineshopAuctionOpen(uint32_t dwOwnerID);
		void SendOfflineshopAuctionAddOffer(uint32_t dwOwnerID, const offlineshop::TPriceInfo& price);
		void SendOfflineshopAuctionExitFrom(uint32_t dwOwnerID);
		void SendOfflineshopAuctionCreate(const TItemPos& pos, const offlineshop::TPriceInfo& price, uint32_t dwDuration);
		void SendOfflineshopAuctionOpenMy();
		void SendOfflineshopCloseMyAuction();
		
		void SendOfflineshopCloseBoard();
#endif

		// Lover
		bool RecvLoverInfoPacket();
		bool RecvLovePointUpdatePacket();

		// Dig
		bool RecvDigMotionPacket();

		// Fishing
		bool SendFishingPacket(int iRotation);
#ifdef ENABLE_NEW_FISHING_SYSTEM
		bool SendFishingPacketNew(int r, int i);
		bool RecvFishingNew();
#endif
		bool SendGiveItemPacket(uint32_t dwTargetVID, TItemPos ItemPos, int iItemCount);

		// Private Shop
		bool SendBuildPrivateShopPacket(const char * c_szName, const std::vector<TShopItemTable> & c_rSellingItemStock
#ifdef KASMIR_PAKET_SYSTEM
		, uint32_t dwKasmirNpc, uint8_t bKasmirBaslik
#endif
		);

		// Refine
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		bool SendRefinePacket(uint8_t byPos, uint8_t byType, uint8_t bLow, uint8_t bMedium, uint8_t bExtra, uint8_t bTotal);
#else
		bool SendRefinePacket(uint8_t byPos, uint8_t byType);
#endif
		bool SendSelectItemPacket(uint32_t dwItemPos);

#ifdef ENABLE_MULTI_LANGUAGE
		bool	SendChangeLanguage(const char * lang);
		bool	SendChangeLanguagePacket(uint8_t bLanguage);
		void	SendRequestTargetLang(const char * targetName);
		bool	RecvTargetLang();
#endif
		// Client Version
		bool SendClientVersionPacket();

		// CRC Report
		bool __SendCRCReportPacket();

		// 용홍석 강화
		bool SendDragonSoulRefinePacket(uint8_t bRefineType, TItemPos* pos);
#ifdef ENABLE_DS_REFINE_ALL
		bool SendDragonSoulRefineAllPacket(uint8_t subheader, uint8_t type, uint8_t grade);
#endif

		// Handshake
		bool RecvHandshakePacket();
		bool RecvHandshakeOKPacket();
#ifdef ENABLE_MAP_TELEPORTER
		void SendMapTeleporterPacket(int iMapCode);
#endif
#ifdef _IMPROVED_PACKET_ENCRYPTION_
		bool RecvKeyAgreementPacket();
		bool RecvKeyAgreementCompletedPacket();

#endif

#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
		bool CubeRenewalMakeItem(int index_item, int count_item, int index_item_improve);
		bool CubeRenewalClose();
		bool RecvCubeRenewalPacket();
#endif

		// ETC
		uint32_t GetMainActorVID();
		uint32_t GetMainActorRace();
		uint8_t GetMainActorEmpire();
		uint32_t GetMainActorSkillGroup();
		void SetEmpireID(uint8_t dwEmpireID);
		uint8_t GetEmpireID();
		void __TEST_SetSkillGroupFake(int iIndex);
#ifdef ENABLE_ACCE_SYSTEM
		bool	SendAcceClosePacket();
		bool	SendAcceAddPacket(TItemPos tPos, uint8_t bPos);
		bool	SendAcceRemovePacket(uint8_t bPos);
		bool	SendAcceRefinePacket();
#endif

	//////////////////////////////////////////////////////////////////////////
	// Phase 관련
	//////////////////////////////////////////////////////////////////////////
	public:
		void SetOffLinePhase();
		void SetHandShakePhase();
		void SetLoginPhase();
		void SetSelectPhase();
		void SetLoadingPhase();
		void SetGamePhase();
		void ClosePhase();

		// Login Phase
		bool SendLoginPacket(const char * c_szName, const char * c_szPassword);
		bool SendLoginPacketNew(const char * c_szName, const char * c_szPassword);


		bool SendEnterGame();

		// Select Phase
		bool SendSelectEmpirePacket(uint8_t dwEmpireID);
		bool SendSelectCharacterPacket(uint8_t account_Index);
		bool SendChangeNamePacket(uint8_t index, const char *name);
		bool SendCreateCharacterPacket(uint8_t index, const char *name, uint8_t job, uint8_t shape, uint8_t byStat1, uint8_t byStat2, uint8_t byStat3, uint8_t byStat4
#if defined(ENABLE_PLAYER_PIN_SYSTEM)
			, const char* pin
#endif
		);
		bool SendDestroyCharacterPacket(uint8_t index, const char * szPrivateCode);
#if defined(ENABLE_PLAYER_PIN_SYSTEM)
		bool SendCharacterPinCodePacket(uint8_t bIndex, const char* c_szPinCode);
#endif

		// Main Game Phase
		bool SendC2CPacket(uint32_t dwSize, void * pData);
		bool SendChatPacket(const char * c_szChat, uint8_t byType = CHAT_TYPE_TALKING);
		bool SendWhisperPacket(const char * name, const char * c_szChat);
		
		bool SendMessengerAddByVIDPacket(uint32_t vid);
		bool SendMessengerAddByNamePacket(const char * c_szName);
		bool SendMessengerRemovePacket(const char * c_szKey, const char * c_szName);
#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
		bool SendWhisperAdminPacket(const char* c_szText, const char* c_szLang, int color);
#endif
	protected:
		bool OnProcess();	// State들을 실제로 실행한다.
		void OffLinePhase();
		void HandShakePhase();
		void LoginPhase();
		void SelectPhase();
		void LoadingPhase();
		void GamePhase();

		bool __IsNotPing();

		void __DownloadMark();
		void __DownloadSymbol(const std::vector<uint32_t> & c_rkVec_dwGuildID);

		void __PlayInventoryItemUseSound(TItemPos uSlotPos);
		void __PlayInventoryItemDropSound(TItemPos uSlotPos);
		//void __PlayShopItemDropSound(UINT uSlotPos);
		void __PlaySafeBoxItemDropSound(UINT uSlotPos);
		void __PlayMallItemDropSound(UINT uSlotPos);

		bool __CanActMainInstance();

		enum REFRESH_WINDOW_TYPE
		{
			RefreshStatus = (1 << 0),
			RefreshAlignmentWindow = (1 << 1),
			RefreshCharacterWindow = (1 << 2),
			RefreshEquipmentWindow = (1 << 3),
			RefreshInventoryWindow = (1 << 4),
			RefreshExchangeWindow = (1 << 5),
			RefreshSkillWindow = (1 << 6),
			RefreshSafeboxWindow  = (1 << 7),
			RefreshMessengerWindow = (1 << 8),
			RefreshGuildWindowInfoPage = (1 << 9),
			RefreshGuildWindowBoardPage = (1 << 10),
			RefreshGuildWindowMemberPage = (1 << 11),
			RefreshGuildWindowMemberPageGradeComboBox = (1 << 12),
			RefreshGuildWindowSkillPage = (1 << 13),
			RefreshGuildWindowGradePage = (1 << 14),
			RefreshTargetBoard = (1 << 15),
			RefreshMallWindow = (1 << 16),
		};

		void __RefreshStatus();
#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
		void __RefreshMountCount();
#endif
		void __RefreshAlignmentWindow();
		void __RefreshCharacterWindow();
		void __RefreshEquipmentWindow();
		void __RefreshInventoryWindow();
		void __RefreshExchangeWindow();
		void __RefreshSkillWindow();
		void __RefreshSafeboxWindow();
		void __RefreshMessengerWindow();
		void __RefreshGuildWindowInfoPage();
		void __RefreshGuildWindowBoardPage();
		void __RefreshGuildWindowMemberPage();
		void __RefreshGuildWindowMemberPageGradeComboBox();
		void __RefreshGuildWindowSkillPage();
		void __RefreshGuildWindowGradePage();
		void __RefreshTargetBoardByVID(uint32_t dwVID);
		void __RefreshTargetBoardByName(const char * c_szName);
		void __RefreshTargetBoard();
		void __RefreshMallWindow();

	protected:
		bool RecvObserverAddPacket();
		bool RecvObserverRemovePacket();
		bool RecvObserverMovePacket();

		// Common
		bool RecvErrorPacket(int header);
		bool RecvPingPacket();
		bool RecvDefaultPacket(int header);
		bool RecvPhasePacket();

		// Login Phase
		bool __RecvLoginSuccessPacket3();
		bool __RecvLoginSuccessPacket4();
		bool __RecvLoginFailurePacket();
		bool __RecvEmpirePacket();
		bool __RecvLoginKeyPacket();

		// Select Phase
		bool __RecvPlayerCreateSuccessPacket();
		bool __RecvPlayerCreateFailurePacket();
		bool __RecvPlayerDestroySuccessPacket();
		bool __RecvPlayerDestroyFailurePacket();
		bool __RecvPreserveItemPacket();
		bool __RecvPlayerPoints();
		bool __RecvChangeName();
#if defined(ENABLE_PLAYER_PIN_SYSTEM)
		bool __RecvPlayerPinCodePacket();
#endif

		// Loading Phase
		bool RecvMainCharacter();
		bool RecvMainCharacter2_EMPIRE();
		bool RecvMainCharacter3_BGM();
		bool RecvMainCharacter4_BGM_VOL();

		void __SetFieldMusicFileName(const char* musicName);
		void __SetFieldMusicFileInfo(const char* musicName, float vol);
		// END_OF_SUPPORT_BGM

		// Main Game Phase
		bool RecvWarpPacket();
		bool RecvPVPPacket();
		bool RecvDuelStartPacket();
        bool RecvGlobalTimePacket();
		bool RecvCharacterAppendPacket();
		bool RecvCharacterAdditionalInfo();
		bool RecvCharacterAppendPacketNew();
		bool RecvCharacterUpdatePacket();
		//bool RecvCharacterUpdatePacketNew();
		bool RecvCharacterDeletePacket();
		bool RecvChatPacket();
		//bool RecvGoldChange();
		bool RecvOwnerShipPacket();

		bool RecvSyncPositionPacket();
		bool RecvWhisperPacket();
		bool RecvPointChange();					// Alarm to python
		bool RecvChangeSpeedPacket();

		bool RecvStunPacket();
		bool RecvDeadPacket();
		bool RecvCharacterMovePacket();

		bool RecvItemSetPacket();					// Alarm to python
		bool RecvItemSetPacket2();					// Alarm to python
		bool RecvItemUsePacket();					// Alarm to python
		bool RecvItemUpdatePacket();				// Alarm to python
		bool RecvItemGroundAddPacket();
		bool RecvItemGroundDelPacket();
		bool RecvItemOwnership();

		bool RecvQuickSlotAddPacket();				// Alarm to python
		bool RecvQuickSlotDelPacket();				// Alarm to python
		bool RecvQuickSlotMovePacket();				// Alarm to python

		bool RecvCharacterPositionPacket();
		bool RecvMotionPacket();

		bool RecvShopPacket();
		bool RecvShopSignPacket();
#ifdef ENABLE_FAKE_SHOP_HEADER
		bool RecvFakeShopSignPacket();
#endif
		bool RecvExchangePacket();

		// Quest
		bool RecvScriptPacket();
		bool RecvQuestInfoPacket();
		bool RecvQuestConfirmPacket();
		bool RecvRequestMakeGuild();

		// Skill
		bool RecvSkillLevel();
		bool RecvSkillLevelNew();
		bool RecvSkillCoolTimeEnd();

		// Target
		bool RecvTargetPacket();
		bool RecvViewEquipPacket();
		bool RecvDamageInfoPacket();
#ifdef ENABLE_SEND_TARGET_INFO
		bool RecvTargetInfoPacket();

		public:
			bool SendTargetInfoLoadPacket(uint32_t dwVID);
#endif



		// Fly
		bool RecvCreateFlyPacket();
		bool RecvFlyTargetingPacket();
		bool RecvAddFlyTargetingPacket();

		// Messenger
		bool RecvMessenger();

		// Guild
		bool RecvGuild();
#ifdef ENABLE_MULTI_LANGUAGE
		bool RecvRequestChangeLanguage();
#endif

		// Party
		bool RecvPartyInvite();
		bool RecvPartyAdd();
		bool RecvPartyUpdate();
		bool RecvPartyRemove();
		bool RecvPartyLink();
		bool RecvPartyUnlink();
		bool RecvPartyParameter();

		// SafeBox
		bool RecvSafeBoxSetPacket();
		bool RecvSafeBoxDelPacket();
		bool RecvSafeBoxWrongPasswordPacket();
		bool RecvSafeBoxSizePacket();
		bool RecvSafeBoxMoneyChangePacket();

		// Fishing
		bool RecvFishing();

#ifdef ENABLE_ITEMSHOP
		bool RecvItemShop();
#endif

		// Dungeon
		bool RecvDungeon();

		// Time
		bool RecvTimePacket();

		// WalkMode
		bool RecvWalkModePacket();

		// ChangeSkillGroup
		bool RecvChangeSkillGroupPacket();

		// Refine
		bool RecvRefineInformationPacket();
		bool RecvRefineInformationPacketNew();

		// Use Potion
		bool RecvSpecialEffect();

		// 서버에서 지정한 이팩트 발동 패킷.
		bool RecvSpecificEffect();

		// 용혼석 관련
		bool RecvDragonSoulRefine();

		bool RecvWikiPacket();

		// MiniMap Info
		bool RecvNPCList();
#ifdef ENABLE_ATLAS_BOSS
		bool RecvBossList();
#endif
		bool RecvLandPacket();
		bool RecvTargetCreatePacket();
		bool RecvTargetCreatePacketNew();
		bool RecvTargetUpdatePacket();
		bool RecvTargetDeletePacket();

		// Affect
		bool RecvAffectAddPacket();
		bool RecvAffectRemovePacket();

		// Channel
		bool RecvChannelPacket();

		// Acce
#ifdef ENABLE_ACCE_SYSTEM
		bool	RecvAccePacket(bool bReturn = false);
#endif

#ifdef ENABLE_RANKING
		bool	RecvRankingTable();
#endif
		// @fixme007
		bool RecvUnk213();

	protected:
		// 이모티콘
		bool ParseEmoticon(const char * pChatMsg, uint32_t * pdwEmoticon);

		// 파이썬으로 보내는 콜들
		void OnConnectFailure();
		void OnScriptEventStart(int iSkin, int iIndex);

		void OnRemoteDisconnect();
		void OnDisconnect();

		void SetGameOnline();
		void SetGameOffline();
		bool IsGameOnline();

#ifdef ENABLE_BATTLE_PASS
	public:
		bool SendBattlePassAction(uint8_t bAction);

	protected:
		bool RecvBattlePassPacket();
		bool RecvBattlePassRankingPacket();
		bool RecvBattlePassUpdatePacket();
#endif


	protected:
		bool CheckPacket(TPacketHeader * pRetHeader);

		void __InitializeGamePhase();
		void __InitializeMarkAuth();
		void __GlobalPositionToLocalPosition(int32_t& rGlobalX, int32_t& rGlobalY);
		void __LocalPositionToGlobalPosition(int32_t& rLocalX, int32_t& rLocalY);

		bool __IsPlayerAttacking();
		bool __IsEquipItemInSlot(TItemPos Cell);
		void __ShowMapName(int32_t lLocalX, int32_t lLocalY);
		void __LeaveOfflinePhase() {}
		void __LeaveHandshakePhase() {}
		void __LeaveLoginPhase() {}
		void __LeaveSelectPhase() {}
		void __LeaveLoadingPhase() {}
		void __LeaveGamePhase();

		void __ClearNetworkActorManager();

		void __ClearSelectCharacterData();

		// DELETEME
		//void __SendWarpPacket();

		void __RecvCharacterAppendPacket(SNetworkActorData * pkNetActorData);
		void __RecvCharacterUpdatePacket(SNetworkUpdateActorData * pkNetUpdateActorData);

		void __FilterInsult(char* szLine, UINT uLineLen);

		void __SetGuildID(uint32_t id);

	protected:
		TPacketGCHandshake m_HandshakeData;
		uint32_t m_dwChangingPhaseTime;
		uint32_t m_dwBindupRetryCount;
		uint32_t m_dwMainActorVID;
		uint32_t m_dwMainActorRace;
		uint8_t m_dwMainActorEmpire;
		uint32_t m_dwMainActorSkillGroup;
		bool m_isGameOnline;
		bool m_isStartGame;
#if defined(ENABLE_PLAYER_PIN_SYSTEM)
		bool m_bVerifiedPIN;
#endif

		uint32_t m_dwGuildID;
		uint8_t m_dwEmpireID;

		struct SServerTimeSync
		{
			uint32_t m_dwChangeServerTime;
			uint32_t m_dwChangeClientTime;
		} m_kServerTimeSync;

		void __ServerTimeSync_Initialize();
		//uint32_t m_dwBaseServerTime;
		//uint32_t m_dwBaseClientTime;

		uint32_t m_dwLastGamePingTime;

		std::string	m_stID;
		std::string	m_stPassword;
		std::string	m_strLastCommand;
		std::string	m_strPhase;
		uint32_t m_dwLoginKey;
		bool m_isWaitLoginKey;

		std::string m_stMarkIP;

		CFuncObject<CPythonNetworkStream>	m_phaseProcessFunc;
		CFuncObject<CPythonNetworkStream>	m_phaseLeaveFunc;

		PyObject*							m_poHandler;
		PyObject*							m_apoPhaseWnd[PHASE_WINDOW_NUM];
		PyObject*							m_poSerCommandParserWnd;
#ifdef ENABLE_NEW_FISHING_SYSTEM
		bool m_phaseWindowGame;
#endif
		TSimplePlayerInformation			m_akSimplePlayerInfo[PLAYER_PER_ACCOUNT4];
		uint32_t								m_adwGuildID[PLAYER_PER_ACCOUNT4];
		std::string							m_astrGuildName[PLAYER_PER_ACCOUNT4];
		bool m_bSimplePlayerInfo;

		CRef<CNetworkActorManager>			m_rokNetActorMgr;

		bool m_isRefreshStatus;
		bool m_isRefreshCharacterWnd;
		bool m_isRefreshEquipmentWnd;
		bool m_isRefreshInventoryWnd;
		bool m_isRefreshExchangeWnd;
		bool m_isRefreshSkillWnd;
		bool m_isRefreshSafeboxWnd;
		bool m_isRefreshMallWnd;
		bool m_isRefreshMessengerWnd;
		bool m_isRefreshGuildWndInfoPage;
		bool m_isRefreshGuildWndBoardPage;
		bool m_isRefreshGuildWndMemberPage;
		bool m_isRefreshGuildWndMemberPageGradeComboBox;
		bool m_isRefreshGuildWndSkillPage;
		bool m_isRefreshGuildWndGradePage;

		// Emoticon
		std::vector<std::string> m_EmoticonStringVector;



		struct SMarkAuth
		{
			CNetworkAddress m_kNetAddr;
			uint32_t m_dwHandle;
			uint32_t m_dwRandomKey;

			SMarkAuth() : m_dwHandle(0), m_dwRandomKey(0) {}
		} m_kMarkAuth;



		uint32_t m_dwSelectedCharacterIndex;

		CInsultChecker m_kInsultChecker;

		bool m_isEnableChatInsultFilter;
		bool m_bComboSkillFlag;

	private:
		struct SDirectEnterMode
		{
			bool m_isSet;
			uint32_t m_dwChrSlotIndex;
		} m_kDirectEnterMode;

		void __DirectEnterMode_Initialize();
		void __DirectEnterMode_Set(UINT uChrSlotIndex);
		bool __DirectEnterMode_IsSet();

	public:
		uint32_t EXPORT_GetBettingGuildWarValue(const char* c_szValueName);
		PyObject* GetPhaseWindow(int phase) { return m_apoPhaseWnd[phase]; }

	private:
		struct SBettingGuildWar
		{
			uint32_t m_dwBettingMoney;
			uint32_t m_dwObserverCount;
		} m_kBettingGuildWar;

		CInstanceBase * m_pInstTarget;

		void __BettingGuildWar_Initialize();
		void __BettingGuildWar_SetObserverCount(UINT uObserverCount);
		void __BettingGuildWar_SetBettingMoney(UINT uBettingMoney);

#ifdef ENABLE_SWITCHBOT
	public:
		bool RecvSwitchbotPacket();

		bool SendSwitchbotStartPacket(uint8_t slot, std::vector<CPythonSwitchbot::TSwitchbotAttributeAlternativeTable> alternatives);
		bool SendSwitchbotStopPacket(uint8_t slot);
#endif
#ifdef TEXTS_IMPROVEMENT
	protected:
		bool RecvChatPacketNew();
#endif
#ifdef ENABLE_MULTI_NAMES
	public:
		bool IsTransName(uint32_t race);

	private:
		std::vector<uint32_t> m_autotrans;
#endif
};
