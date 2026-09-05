#ifndef	__HEADER_MOUNT_SYSTEM__
#define	__HEADER_MOUNT_SYSTEM__

#include <memory>
#include <unordered_map>
#include <entt/entt.hpp>

class CMountActor
{
public:
	CMountActor(entt::entity owner, uint32_t vnum);

	virtual ~CMountActor();

protected:
	friend class CMountSystem;

	virtual bool	Update(uint32_t deltaTime);
	virtual bool	UpdateFollowAI();
	
private:
	bool Follow(float fMinDistance = 50.f);

public:
	entt::entity		GetCharacter() const { return m_character; }
	entt::entity		GetOwner() const { return m_owner; }
	uint32_t			GetVID() const							{ return m_dwVID; }
	uint32_t			GetVnum() const							{ return m_dwVnum; }
	void			SetName();
	bool			Mount(entt::entity mountItem);
	void			Unmount();
	uint32_t			Summon(entt::entity pSummonItem, bool bSpawnFar = false);
	void			Unsummon();
	bool			IsSummoned() const;
	void			SetSummonItem (entt::entity pItem);
	uint32_t			GetSummonItemVID () { return m_dwSummonItemVID; }
	entt::entity		GetSummonItem() const { return m_summonItem; }
#ifdef ENABLE_COSTUME_MOUNT
	void	UpdateMountSkin();
#endif
private:
	uint32_t			m_dwVnum;
	uint32_t			m_dwVID;
	uint32_t			m_dwLastActionTime;
	uint32_t			m_dwSummonItemVID;
	uint32_t			m_dwSummonItemVnum;

	std::string		m_name;

	entt::entity		m_character { entt::null };
	entt::entity		m_owner { entt::null };
	entt::entity		m_summonItem { entt::null };
	uint32_t		m_ridingVnum { 0 };
};

class CMountSystem
{
public:
	typedef	std::unordered_map<uint32_t, std::unique_ptr<CMountActor>>		TMountActorMap;

public:
	CMountSystem(entt::entity owner);
	virtual ~CMountSystem();

	CMountActor*	GetByVID(uint32_t vid) const;
	CMountActor*	GetByVnum(uint32_t vnum) const;
	entt::entity GetOwner() const { return m_owner; }
	bool IsUpdateEvent(const LPEVENT& event) const { return event && event == m_pkMountSystemUpdateEvent; }

	bool		Update(uint32_t deltaTime);
	void		Destroy();

	size_t		CountSummoned() const;

public:
	void		SetUpdatePeriod(uint32_t ms);

	void		Summon(uint32_t mobVnum, entt::entity pSummonItem, bool bSpawnFar);

	void		Unsummon(uint32_t mobVnum, bool bDeleteFromList = false);
	void		Unsummon(CMountActor* mountActor, bool bDeleteFromList = false);
	
	void		Mount(uint32_t mobVnum, entt::entity mountItem);
	void		Unmount(uint32_t mobVnum);

	void		DeleteMount(uint32_t mobVnum);
	void		DeleteMount(CMountActor* mountActor);

#ifdef ENABLE_COSTUME_MOUNT
	void	UpdateMountSkin();
#endif
private:
	TMountActorMap	m_mountActorMap;
	entt::entity		m_owner { entt::null };
	uint32_t			m_dwUpdatePeriod;
	uint32_t			m_dwLastUpdateTime;
	LPEVENT			m_pkMountSystemUpdateEvent;
};

#endif
