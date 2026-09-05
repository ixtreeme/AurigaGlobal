#ifndef __INC_METIN_II_GAME_INPUT_PROCESSOR__
#define __INC_METIN_II_GAME_INPUT_PROCESSOR__

#include <entt/entt.hpp>

#include "packet_info.h"

enum
{
	INPROC_CLOSE,
	INPROC_HANDSHAKE,
	INPROC_LOGIN,
	INPROC_MAIN,
	INPROC_DEAD,
	INPROC_DB,
	INPROC_UDP,
	INPROC_P2P,
	INPROC_AUTH,
};

void LoginFailure(LPDESC d, const char * c_pszStatus);
extern void SendShout(const char * szText, uint8_t bEmpire);

class CInputProcessor
{
	public:
		CInputProcessor();
		virtual ~CInputProcessor() {};

		virtual bool Process(LPDESC d, const void * c_pvOrig, int iBytes, int & r_iBytesProceed);
		virtual uint8_t GetType() = 0;

		void BindPacketInfo(CPacketInfo * pPacketInfo);
		void Pong(LPDESC d);
		virtual void Handshake(LPDESC d, const char * c_pData);
		void Version(entt::entity ch, const char* c_pData);

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData) = 0;

		CPacketInfo * m_pPacketInfo;
		int	m_iBufferLeft;

		CPacketInfoCG 	m_packetInfoCG;
};

class CInputClose : public CInputProcessor
{
	public:
		virtual uint8_t	GetType() { return INPROC_CLOSE; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData) { return m_iBufferLeft; }
};

class CInputHandshake : public CInputProcessor
{
	public:
		CInputHandshake();
		virtual ~CInputHandshake();

		virtual uint8_t	GetType() { return INPROC_HANDSHAKE; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);

	protected:
		void		GuildMarkLogin(LPDESC d, const char* c_pData);

		CPacketInfo *	m_pMainPacketInfo;
};

class CInputLogin : public CInputProcessor
{
	public:
		virtual uint8_t	GetType() { return INPROC_LOGIN; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);

	protected:
		void		Login(LPDESC d, const char * data);
		void		LoginByKey(LPDESC d, const char * data);

		void		CharacterSelect(LPDESC d, const char * data);
		void		CharacterCreate(LPDESC d, const char * data);
		void		CharacterDelete(LPDESC d, const char * data);

		void		Entergame(LPDESC d, const char * data);
		void		Empire(LPDESC d, const char * c_pData);
		void		GuildMarkCRCList(LPDESC d, const char* c_pData);
		// MARK_BUG_FIX
		void		GuildMarkIDXList(LPDESC d, const char* c_pData);
		// END_OF_MARK_BUG_FIX
		void		GuildMarkUpload(LPDESC d, const char* c_pData);
		int			GuildSymbolUpload(LPDESC d, const char* c_pData, size_t uiBytes);
		void		GuildSymbolCRC(LPDESC d, const char* c_pData);
		void		ChangeName(LPDESC d, const char * data);
};

class CInputMain : public CInputProcessor
{
	public:
		virtual uint8_t	GetType() { return INPROC_MAIN; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);

	protected:
		void		Attack(entt::entity character, const uint8_t header, const char* data);
		int			Whisper(entt::entity character, const char * data, uint64_t uiBytes);
#ifdef __NEWPET_SYSTEM__
		void		BraveRequestPetName(entt::entity character, const char* c_pData);
#endif
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		void		InventoryExpansion(entt::entity character, const char * data);
#endif
		int			Chat(entt::entity character, const char * data, uint32_t uiBytes);
		void		ItemUse(entt::entity character, const char * data);
		void		ItemDrop(entt::entity character, const char * data);
		void		ItemDrop2(entt::entity character, const char * data);
		void		ItemDestroy(entt::entity character, const char * data);
		void		ItemDivision(entt::entity character, const char * data);
		void		ItemMove(entt::entity character, const char * data);
		void		ItemPickup(entt::entity character, const char * data);
		void		ItemToItem(entt::entity character, const char * pcData);
		void		QuickslotAdd(entt::entity character, const char * data);
		void		QuickslotDelete(entt::entity character, const char * data);
		void		QuickslotSwap(entt::entity character, const char * data);
		int			Shop(entt::entity character, const char * data, size_t uiBytes);
		void		OnClick(entt::entity character, const char * data);
		void		Exchange(entt::entity character, const char * data);
		void		Position(entt::entity character, const char * data);
		void		Move(entt::entity character, const char * data);
		int			SyncPosition(entt::entity character, const char * data, uint64_t uiBytes);
		void		FlyTarget(entt::entity character, const char * pcData, uint8_t bHeader);
		void		UseSkill(entt::entity character, const char * pcData);

#ifdef ENABLE_BATTLE_PASS	
		int 		BattlePass(entt::entity character, const char * data, size_t uiBytes);
#endif

#ifdef __SKILL_COLOR_SYSTEM__
		void		SetSkillColor(entt::entity character, const char * pcData);
#endif
		

		void		ScriptAnswer(entt::entity character, const void * pvData);
		void		ScriptButton(entt::entity character, const void * pvData);
		void		ScriptSelectItem(entt::entity character, const void * pvData);

		void		QuestInputString(entt::entity character, const void * pvData);
		void		QuestConfirm(entt::entity character, const void* pvData);
		void		Target(entt::entity character, const char * pcData);
		void		Warp(entt::entity character, const char * pcData);
		void		SafeboxCheckin(entt::entity character, const char * c_pData);
		void		SafeboxCheckout(entt::entity character, const char * c_pData, bool bMall);
		void		SafeboxItemMove(entt::entity character, const char * data);

		void            MountInventoryCheckin(entt::entity character, const char* c_pData);
		void            MountInventoryCheckout(entt::entity character, const char* c_pData);
		static void            MountInventoryItemMove(entt::entity character, const char* data);

		int			Messenger(entt::entity character, const char* c_pData, uint64_t uiBytes);

		void 		PartyInvite(entt::entity character, const char * c_pData);
		void 		PartyInviteAnswer(entt::entity character, const char * c_pData);
		void		PartyRemove(entt::entity character, const char * c_pData);
		void		PartySetState(entt::entity character, const char * c_pData);
		void		PartyUseSkill(entt::entity character, const char * c_pData);
		void		PartyParameter(entt::entity character, const char * c_pData);
#ifdef __INGAME_WIKI__
		void		RecvWikiPacket(entt::entity character, const char * c_pData);
#endif
		int			Guild(entt::entity character, const char * data, size_t uiBytes);
		void		AnswerMakeGuild(entt::entity character, const char* c_pData);

		void		Fishing(entt::entity character, const char* c_pData);
		void		ItemGive(entt::entity character, const char* c_pData);
		void		Hack(entt::entity character, const char * c_pData);
		int			MyShop(entt::entity character, const char * c_pData, size_t uiBytes);

#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
		void 		CubeRenewalSend(entt::entity character, const char* data);
#endif


		void		Refine(entt::entity character, const char* c_pData);
#ifdef ENABLE_ACCE_SYSTEM
		void		Acce(entt::entity character, const char* c_pData);
#endif
#ifdef ENABLE_MAP_TELEPORTER
		void		MapTeleporter(entt::entity character, TPacketCGMapTeleporter* pPack);
#endif
		void		Roulette(entt::entity character, const char* c_pData);
#ifdef ENABLE_SWITCHBOT
		int			Switchbot(entt::entity character, const char* data, size_t uiBytes);
#endif
#ifdef __SEND_TARGET_INFO__
		void		TargetInfoLoad(entt::entity character, const char* c_pData);
#endif
#ifdef ENABLE_MULTI_LANGUAGE
		void	ChangeLanguage(entt::entity character, uint8_t bLanguage);
		void	RequestLanguage(entt::entity character, const char* targetName);
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
		void FishingNew(entt::entity character, const char* c_pData);
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
		void WheelDestiny(entt::entity character, const char* data);
#endif
};

class CInputDead : public CInputMain
{
	public:
		virtual uint8_t	GetType() { return INPROC_DEAD; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);
};

class CInputDB : public CInputProcessor
{
public:
	virtual bool Process(LPDESC d, const void * c_pvOrig, int iBytes, int & r_iBytesProceed);
	virtual uint8_t GetType() { return INPROC_DB; }

protected:
	virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);

protected:
	void		MapLocations(const char * c_pData);
	void		LoginSuccess(uint32_t dwHandle, const char *data);
	void		PlayerCreateFailure(LPDESC d, uint8_t bType);	// 0 = 일반 실패 1 = 이미 있음
	void		PlayerDeleteSuccess(LPDESC d, const char * data);
	void		PlayerDeleteFail(LPDESC d);
	void		PlayerLoad(LPDESC d, const char* data);
	void		PlayerCreateSuccess(LPDESC d, const char * data);
	void		Boot(const char* data);
	void		QuestLoad(LPDESC d, const char * c_pData);
	void		SafeboxLoad(LPDESC d, const char * c_pData);
	void		SafeboxChangeSize(LPDESC d, const char * c_pData);
	void		SafeboxWrongPassword(LPDESC d);
	void		SafeboxChangePasswordAnswer(LPDESC d, const char* c_pData);
	void		MallLoad(LPDESC d, const char * c_pData);
	void		EmpireSelect(LPDESC d, const char * c_pData);
	void		P2P(const char * c_pData);
	void		ItemLoad(LPDESC d, const char * c_pData);
	void		AffectLoad(LPDESC d, const char * c_pData);

#ifdef ENABLE_BATTLE_PASS
	void		BattlePassLoad(LPDESC d, const char * c_pData);
	void		BattlePassLoadRanking(LPDESC d, const char * c_pData);
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	void		SkillColorLoad(LPDESC d, const char * c_pData);
#endif

	void		GuildLoad(const char * c_pData);
	void		GuildSkillUpdate(const char* c_pData);
	void		GuildSkillRecharge();
	void		GuildExpUpdate(const char* c_pData);
	void		GuildAddMember(const char* c_pData);
	void		GuildRemoveMember(const char* c_pData);
	void		GuildChangeGrade(const char* c_pData);
	void		GuildChangeMemberData(const char* c_pData);
	void		GuildDisband(const char* c_pData);
	void		GuildLadder(const char* c_pData);
	void		GuildWar(const char* c_pData);
	void		GuildWarScore(const char* c_pData);
	
#ifdef ADVANCED_GUILD_INFO	
	void		GuildResetStats(const char* c_pData);
#endif		
	
	void		GuildSkillUsableChange(const char* c_pData);
	void		GuildMoneyChange(const char* c_pData);
	void		GuildWithdrawMoney(const char* c_pData);
	void		GuildWarReserveAdd(TGuildWarReserve * p);
	void		GuildWarReserveUpdate(TGuildWarReserve * p);
	void		GuildWarReserveDelete(uint32_t dwID);
	void		GuildWarBet(TPacketGDGuildWarBet * p);
	void		GuildChangeMaster(TPacketChangeGuildMaster* p);

	void		LoginAlready(LPDESC d, const char * c_pData);

	void		PartyCreate(const char* c_pData);
	void		PartyDelete(const char* c_pData);
	void		PartyAdd(const char* c_pData);
	void		PartyRemove(const char* c_pData);
	void		PartyStateChange(const char* c_pData);
	void		PartySetMemberLevel(const char* c_pData);

	void		Time(const char * c_pData);

	void		ReloadProto(const char * c_pData);
	void		ChangeName(LPDESC d, const char * data);

	void		AuthLogin(LPDESC d, const char * c_pData);
	void		AuthLoginOpenID(LPDESC d, const char * c_pData);
	void		ItemAward(const char * c_pData);

	void		ChangeEmpirePriv(const char* c_pData);
	void		ChangeGuildPriv(const char* c_pData);
	void		ChangeCharacterPriv(const char* c_pData);

	void		MoneyLog(const char* c_pData);

	void		SetEventFlag(const char* c_pData);

	void		BillingRepair(const char * c_pData);
	void		BillingExpire(const char * c_pData);
	void		BillingLogin(const char * c_pData);
	void		BillingCheck(const char * c_pData);
	void		VCard(const char * c_pData);

	void		CreateObject(const char * c_pData);
	void		DeleteObject(const char * c_pData);
	void		UpdateLand(const char * c_pData);

	void		Notice(const char * c_pData);
	void		MarriageAdd(TPacketMarriageAdd * p);
	void		MarriageUpdate(TPacketMarriageUpdate * p);
	void		MarriageRemove(TPacketMarriageRemove * p);

	void		WeddingRequest(TPacketWeddingRequest* p);
	void		WeddingReady(TPacketWeddingReady* p);
	void		WeddingStart(TPacketWeddingStart* p);
	void		WeddingEnd(TPacketWeddingEnd* p);

#ifdef ENABLE_ITEMSHOP
	void		ItemShop(LPDESC d, const char* c_pData);
#endif


	// MYSHOP_PRICE_LIST
	/// 아이템 가격정보 리스트 요청에 대한 응답 패킷(HEADER_DG_MYSHOP_PRICELIST_RES) 처리함수
	/**
	* @param	d 아이템 가격정보 리스트를 요청한 플레이어의 descriptor
	* @param	p 패킷데이터의 포인터
	*/
	void		MyshopPricelistRes( LPDESC d, const TPacketMyshopPricelistHeader* p );
	// END_OF_MYSHOP_PRICE_LIST
	//
	//RELOAD_ADMIN
	void ReloadAdmin( const char * c_pData );
	//END_RELOAD_ADMIN
#ifdef ENABLE_EVENT_MANAGER
	void		EventManager(const char* c_pData);
#endif

	void		DetailLog(const TPacketNeedLoginLogInfo* info);
	// 독일 선물 기능 테스트
	void		ItemAwardInformer(TPacketItemAwardInfromer* data);
#if defined(BL_OFFLINE_MESSAGE)
	void		ReadOfflineMessages(LPDESC desc, const char* pcData);
#endif
	void		RespondChannelStatus(LPDESC desc, const char* pcData);
#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	void		ChangeChannel(LPDESC desc, const char* pcData);
#endif


	protected:
		uint32_t		m_dwHandle;
};

class CInputUDP : public CInputProcessor
{
	public:
		CInputUDP();
		virtual bool Process(LPDESC d, const void * c_pvOrig, int iBytes, int & r_iBytesProceed);

		virtual uint8_t GetType() { return INPROC_UDP; }
		void		SetSockAddr(struct sockaddr_in & rSockAddr) { m_SockAddr = rSockAddr; };

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);

	protected:
		void		Handshake(LPDESC lpDesc, const char * c_pData);
		void		StateChecker(const char * c_pData);

	protected:
		struct sockaddr_in	m_SockAddr;
		CPacketInfoUDP 		m_packetInfoUDP;
};

class CInputP2P : public CInputProcessor
{
	public:
		CInputP2P();
		virtual uint8_t	GetType() { return INPROC_P2P; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);

	public:
		void		Setup(LPDESC d, const char * c_pData);
		void		Login(LPDESC d, const char * c_pData);
		void		Logout(LPDESC d, const char * c_pData);
		int			Relay(LPDESC d, const char * c_pData, size_t uiBytes);
#ifdef ENABLE_FULL_NOTICE
		int			Notice(LPDESC d, const char * c_pData, uint64_t uiBytes, bool bBigFont=false);
#else
		int			Notice(LPDESC d, const char * c_pData, size_t uiBytes);
#endif
#ifdef TEXTS_IMPROVEMENT
		int	NoticeNew(LPDESC d, const char * c_pData, uint64_t uiBytes);
#endif
		int			Guild(LPDESC d, const char* c_pData, size_t uiBytes);
		void		Shout(const char * c_pData);
		void		Disconnect(const char * c_pData);
		void		MessengerAdd(const char * c_pData);
		void		MessengerRemove(const char * c_pData);
		void		MessengerMobile(const char * c_pData);
		void		FindPosition(LPDESC d, const char* c_pData);
		void		WarpCharacter(const char* c_pData);
		void		GuildWarZoneMapIndex(const char* c_pData);
		void		Transfer(const char * c_pData);
		void		LoginPing(LPDESC d, const char * c_pData);
		void		BlockChat(const char * c_pData);
		void		PCBangUpdate(const char* c_pData);
		void		IamAwake(LPDESC d, const char * c_pData);
#ifdef ENABLE_SWITCHBOT
		void		Switchbot(LPDESC d, const char* c_pData);
#endif

	protected:
		CPacketInfoGG 	m_packetInfoGG;
};

class CInputAuth : public CInputProcessor
{
	public:
		CInputAuth();
		virtual uint8_t GetType() { return INPROC_AUTH; }

	protected:
		virtual int	Analyze(LPDESC d, uint8_t bHeader, const char * c_pData);
		int auth_OpenID(const char *authKey, const char *ipAddr, char *rID);

	public:
		void		Login(LPDESC d, const char * c_pData);
		void		LoginOpenID(LPDESC d, const char * c_pData);		//2012.07.19 OpenID : 김용욱
};

#endif /* __INC_METIN_II_GAME_INPUT_PROCESSOR__ */

