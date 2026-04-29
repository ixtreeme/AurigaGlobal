#ifndef	__HEADER_NEWPET_SYSTEM__
#define	__HEADER_NEWPET_SYSTEM__

#include <entt/entt.hpp>

class CHARACTER;

// TODO: 펫으로서의 능력치? 라던가 친밀도, 배고픔 기타등등... 수치
struct SNewPetAbility
{
};

/**
*/
class CNewPetActor //: public CHARACTER
{
public:
	enum ENewPetOptions
	{
		EPetOption_Followable		= 1 << 0,
		EPetOption_Mountable		= 1 << 1,
		EPetOption_Summonable		= 1 << 2,
		EPetOption_Combatable		= 1 << 3,		
	};


protected:
	friend class CNewPetSystem;

	CNewPetActor(LPCHARACTER owner, uint32_t vnum, uint32_t options = EPetOption_Followable | EPetOption_Summonable);
//	CPetActor(LPCHARACTER owner, uint32_t vnum, const SPetAbility& petAbility, uint32_t options = EPetOption_Followable | EPetOption_Summonable);

	virtual ~CNewPetActor();

	virtual bool	Update(uint32_t deltaTime);

protected:
	virtual bool	_UpdateFollowAI();				///< 주인을 따라다니는 AI 처리
	virtual bool	_UpdatAloneActionAI(float fMinDist, float fMaxDist);			///< 주인 근처에서 혼자 노는 AI 처리

	/// @TODO
	//virtual bool	_UpdateCombatAI();

private:
	bool Follow(float fMinDistance = 50.f);

public:
	LPCHARACTER		GetCharacter()	const					{ return m_pkChar; }
	LPCHARACTER		GetOwner()	const						{ return m_pkOwner; }
	uint32_t			GetVID() const							{ return m_dwVID; }
	uint32_t			GetVnum() const							{ return m_dwVnum; }

	bool			HasOption(ENewPetOptions option) const		{ return m_dwOptions & option; }

	void			SetName(const char* petName);
	void			SetLevel(uint32_t level);

	bool			Mount();
	void			Unmount();

	uint32_t			Summon(const char* petName, entt::entity pSummonItem, bool bSpawnFar = false);
	void			Unsummon();

	bool			IsSummoned() const			{ return nullptr != m_pkChar; }
	void			SetSummonItem (entt::entity pItem);
	uint32_t			GetSummonItemVID () { return m_dwSummonItemVID; }
	uint32_t			GetSummonItemID () { return m_dwSummonItemID; }
	uint32_t			GetEvolution() { return m_dwevolution; }
	uint32_t			GetLevel() { return m_dwlevel; }
	void			SetEvolution(int lv);
	void			SetExp(uint32_t exp, int mode);
	uint32_t			GetExp() { return m_dwexp; }
	uint32_t			GetExpI() { return m_dwexpitem; }
	void			SetNextExp(int nextExp);
	int				GetNextExpFromMob() { return m_dwExpFromMob; }
	int				GetNextExpFromItem() { return m_dwExpFromItem; }
	int				GetLevelStep() { return m_dwlevelstep; }

	void			IncreasePetBonus();
	void			SetItemCube(int pos, int invpos);
	void			ItemCubeFeed(int type);
	void			DoPetSkill(int skillslot);
	void			UpdateTime(bool now = false);
#ifdef ENABLE_NEW_PET_EDITS
	int				ResetSkills();
	int				ResetSkill(int iType);
	bool			IncreasePetSkill(int iSlot, int iType);
	bool			IncreasePetSkillByBook(entt::entity bookItem);
#else
	bool			IncreasePetSkill(int skill);
#endif
	bool			IncreasePetEvolution();

	// 버프 주는 함수와 거두는 함수.
	// 이게 좀 괴랄한게, 서버가 ㅄ라서, 
	// POINT_MOV_SPEED, POINT_ATT_SPEED, POINT_CAST_SPEED는 PointChange()란 함수만 써서 변경해 봐야 소용이 없는게,
	// PointChange() 이후에 어디선가 ComputePoints()를 하면 싹다 초기화되고, 
	// 더 웃긴건, ComputePoints()를 부르지 않으면 클라의 POINT는 전혀 변하지 않는다는 거다.
	// 그래서 버프를 주는 것은 ComputePoints() 내부에서 petsystem->RefreshBuff()를 부르도록 하였고,
	// 버프를 빼는 것은 ClearBuff()를 부르고, ComputePoints를 하는 것으로 한다.
	void			GiveBuff();
	void			ClearBuff();
#ifdef ENABLE_COSTUME_PET
	void	UpdatePetSkin();
#endif
	void	ChangeName(const char * name);
private:
	int			m_dwlevelstep; //Step livello del pet da da 0 a 4
	int			m_dwExpFromMob; //Exp richiesta per il level 90% del tot
	int			m_dwExpFromItem; //Exp richiesta per il level 10% del tot
	int			m_dwexpitem; // Exp corrente presa dagli item
	int			m_dwevolution; //Stato evoluzione del pet da 1 a 4
	int			m_dwTimePet; //Tempo per il pet
	int			m_dwslotimm;

	uint32_t		m_dwImmTime;

	int				m_dwpetslotitem[9];
	int				m_dwskill[4];
	int				m_dwskillslot[4];
	int				m_dwbonuspet[3][2];
	uint32_t			m_dwVnum;
	uint32_t			m_dwVID;
	uint32_t			m_dwlevel;	
	uint32_t			m_dwexp;
	uint32_t			m_dwOptions;
	uint32_t			m_dwLastActionTime;
	uint32_t			m_dwSummonItemVID;
	uint32_t			m_dwSummonItemID;
	uint32_t			m_dwSummonItemVnum;

	uint32_t			m_dwduration;
	uint32_t			m_dwtduration;
#ifdef ENABLE_NEW_PET_EDITS
	int32_t			lMinAge;
	uint32_t			dwMinAge;
	uint8_t			m_idx;
#endif
	short			m_originalMoveSpeed;

	std::string		m_name;

	LPCHARACTER		m_pkChar;					// Instance of pet(CHARACTER)
	LPCHARACTER		m_pkOwner;

//	SPetAbility		m_petAbility;				// 능력치
};

/**
*/
class CNewPetSystem
{
public:
	typedef	std::unordered_map<uint32_t,	CNewPetActor*>		TNewPetActorMap;		/// <VNUM, NewPetActor> map. (한 캐릭터가 같은 vnum의 펫을 여러개 가질 일이 있을까..??)

public:
	CNewPetSystem(LPCHARACTER owner);
	virtual ~CNewPetSystem();

	CNewPetActor*	GetByVID(uint32_t vid) const;
	CNewPetActor*	GetByVnum(uint32_t vnum) const;

	bool		Update(uint32_t deltaTime);
	void		Destroy();

	size_t		CountSummoned() const;			///< 현재 소환된(실체화 된 캐릭터가 있는) 펫의 개수
#ifdef ENABLE_COSTUME_PET
	void	UpdatePetSkin();
#endif

public:
	void		SetUpdatePeriod(uint32_t ms);

	CNewPetActor*	Summon(uint32_t mobVnum, entt::entity pSummonItem, const char* petName, bool bSpawnFar, uint32_t options = CNewPetActor::EPetOption_Followable | CNewPetActor::EPetOption_Summonable);

	void		Unsummon(uint32_t mobVnum, bool bDeleteFromList = false);
	void		Unsummon(CNewPetActor* petActor, bool bDeleteFromList = false);
	void		UnsummonAll(LPCHARACTER ch);

	// TODO: 진짜 펫 시스템이 들어갈 때 구현. (캐릭터가 보유한 펫의 정보를 추가할 때 라던가...)
	CNewPetActor*	AddPet(uint32_t mobVnum, const char* petName, const SNewPetAbility& ability, uint32_t options = CNewPetActor::EPetOption_Followable | CNewPetActor::EPetOption_Summonable | CNewPetActor::EPetOption_Combatable);

	
#ifdef ENABLE_NEW_PET_EDITS
	int			ResetSkills();
	int			ResetSkill(int iType);
	bool		IncreasePetSkill(int iSlot, int iType);
	bool		IncreasePetSkillByBook(entt::entity bookItem);
#else
	bool		IncreasePetSkill(int skill);
#endif
	bool		IncreasePetEvolution();	
	
	void		DeletePet(uint32_t mobVnum);
	void		DeletePet(CNewPetActor* petActor);
	void		RefreshBuff();
	bool		IsActivePet();
	uint32_t		GetNewPetITemID();
	void		SetExp(int iExp, int mode);
	int			GetEvolution();
	int			GetLevel();
	int			GetExp();
	int			GetLevelStep();
	void		SetItemCube(int pos, int invpos);
	void		ItemCubeFeed(int type);
	void		DoPetSkill(int skillslot);
	void		UpdateTime();
#ifdef ENABLE_NEW_PET_EDITS
	int			GetNextExpFromMob();
#endif
	void	ChangeName(const char * name);

private:
	TNewPetActorMap	m_petActorMap;
	LPCHARACTER		m_pkOwner;					///< 펫 시스템의 Owner
	uint32_t			m_dwUpdatePeriod;			///< 업데이트 주기 (ms단위)
	uint32_t			m_dwLastUpdateTime;
	LPEVENT			m_pkNewPetSystemUpdateEvent;
	LPEVENT			m_pkNewPetSystemExpireEvent;
};

/**
// Summon Pet
CPetSystem* petSystem = mainChar->GetPetSystem();
CPetActor* petActor = petSystem->Summon(~~~);

uint32_t petVID = petActor->GetVID();
if (0 == petActor)
{
	ERROR_LOG(...)
};


// Unsummon Pet
petSystem->Unsummon(petVID);

// Mount Pet
petActor->Mount()..


CPetActor::Update(...)
{
	// AI : Follow, actions, etc...
}

*/



#endif	//__HEADER_PET_SYSTEM__
