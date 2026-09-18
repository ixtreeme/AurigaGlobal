#ifndef __INC_METIN_II_GAME_PVP_H__
#define __INC_METIN_II_GAME_PVP_H__

#include <entt/entt.hpp>


// CPVP���� uint32_t ���̵� �ΰ��� �޾Ƽ� m_dwCRC�� ���� ������ �ִ´�.
// CPVPManager���� �̷��� ���� CRC�� ���� �˻��Ѵ�.
class CPVP
{
	public:
		friend class CPVPManager;

		typedef struct _player
		{
			uint32_t	dwPID;
			uint32_t	dwVID;
			bool	bAgree;
			bool	bCanRevenge;

			_player() : dwPID(0), dwVID(0), bAgree(false), bCanRevenge(false)
			{
			}
		} TPlayer;

		CPVP(uint32_t dwID1, uint32_t dwID2);
		CPVP(CPVP & v);
		~CPVP();

		void	Win(uint32_t dwPID); // dwPID�� �̰��!
		bool	CanRevenge(uint32_t dwPID); // dwPID�� ������ �� �־�?
		bool	IsFight();
		bool	Agree(uint32_t dwPID);

		void	SetVID(uint32_t dwPID, uint32_t dwVID);
		void	Packet(bool bDelete = false);

		void	SetLastFightTime();
		uint32_t	GetLastFightTime();

		uint32_t 	GetCRC() { return m_dwCRC; }

	protected:
		TPlayer	m_players[2];
		uint32_t	m_dwCRC;
		bool	m_bRevenge;

#ifdef ENABLE_PVP_ADVANCED
		LPEVENT	m_pAdvancedDuelTimer;
#endif

		uint32_t   m_dwLastFightTime;
};

class CPVPManager : public singleton<CPVPManager>
{
	typedef std::map<uint32_t, std::unordered_set<CPVP*> > CPVPSetMap;

	public:
	CPVPManager();
	virtual ~CPVPManager();

#ifdef ENABLE_NEWSTUFF
	bool			IsFighting(uint32_t dwPID);
#endif

	void			Insert(entt::entity character, entt::entity victim);
	bool			CanAttack(entt::entity character, entt::entity victim, bool bIsFarmMap = false);
	bool			Dead(entt::entity character, uint32_t dwKillerPID);	// PVP�� �־��� �������� ����
	void			GiveUp(entt::entity character, uint32_t dwKillerPID);
	void			Connect(entt::entity character);
#ifdef ENABLE_PVP_ADVANCED
	void			Decline(entt::entity character, entt::entity victim);
#endif
	void			Disconnect(entt::entity character);

	void			SendList(LPDESC d);
	void			Delete(CPVP * pkPVP);

	void			Process();

	public:
	CPVP *			Find(uint32_t dwCRC);
	protected:
	void			ConnectEx(entt::entity character, bool bDisconnect);

	std::map<uint32_t, CPVP *>	m_map_pkPVP;
	CPVPSetMap		m_map_pkPVPSetByID;
};

#endif
