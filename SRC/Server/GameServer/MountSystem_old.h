#ifndef	__HEADER_MOUNT_SYSTEM__
#define	__HEADER_MOUNT_SYSTEM__


class CHARACTER;

class CMountActor
{
protected:
	friend class CMountSystem;

	CMountActor(LPCHARACTER owner, uint32_t vnum);

	virtual ~CMountActor();

	virtual bool	Update(uint32_t deltaTime);
	virtual bool	_UpdateFollowAI();
	
private:
	bool Follow(float fMinDistance = 50.f);

public:
	LPCHARACTER		GetCharacter()	const					{ return m_pkChar; }
	LPCHARACTER		GetOwner()	const						{ return m_pkOwner; }
	uint32_t			GetVID() const							{ return m_dwVID; }
	uint32_t			GetVnum() const							{ return m_dwVnum; }
	void			SetName();
	bool			Mount(LPITEM mountItem);
	void			Unmount();
	uint32_t			Summon(LPITEM pSummonItem, bool bSpawnFar = false);
	void			Unsummon();
	bool			IsSummoned() const			{ return nullptr != m_pkChar; }
	void			SetSummonItem (LPITEM pItem);
	uint32_t			GetSummonItemVID () { return m_dwSummonItemVID; }
#ifdef ENABLE_COSTUME_MOUNT
	void	UpdateMountSkin();
#endif
private:
	uint32_t			m_dwVnum;
	uint32_t			m_dwVID;
	uint32_t			m_dwLastActionTime;
	uint32_t			m_dwSummonItemVID;
	uint32_t			m_dwSummonItemVnum;

	short			m_originalMoveSpeed;

	std::string		m_name;

	LPCHARACTER		m_pkChar;
	LPCHARACTER		m_pkOwner;
};

class CMountSystem
{
public:
	typedef	std::unordered_map<uint32_t,	CMountActor*>		TMountActorMap;

public:
	CMountSystem(LPCHARACTER owner);
	virtual ~CMountSystem();

	CMountActor*	GetByVID(uint32_t vid) const;
	CMountActor*	GetByVnum(uint32_t vnum) const;

	bool		Update(uint32_t deltaTime);
	void		Destroy();

	size_t		CountSummoned() const;

public:
	void		SetUpdatePeriod(uint32_t ms);

	void		Summon(uint32_t mobVnum, LPITEM pSummonItem, bool bSpawnFar);

	void		Unsummon(uint32_t mobVnum, bool bDeleteFromList = false);
	void		Unsummon(CMountActor* mountActor, bool bDeleteFromList = false);
	
	void		Mount(uint32_t mobVnum, LPITEM mountItem);
	void		Unmount(uint32_t mobVnum);

	void		DeleteMount(uint32_t mobVnum);
	void		DeleteMount(CMountActor* mountActor);

#ifdef ENABLE_COSTUME_MOUNT
	void	UpdateMountSkin();
#endif
private:
	TMountActorMap	m_mountActorMap;
	LPCHARACTER		m_pkOwner;
	uint32_t			m_dwUpdatePeriod;
	uint32_t			m_dwLastUpdateTime;
	LPEVENT			m_pkMountSystemUpdateEvent;
};

#endif
