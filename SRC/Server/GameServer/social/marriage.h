#ifndef __MARRIAGE_H
#define __MARRIAGE_H

namespace ecs { struct CoupleState; }

namespace marriage
{
	extern const int MARRIAGE_POINT_PER_DAY;

	// An engaged or married couple is ecs::CoupleState on its own
	// registry-owned entity, indexed by both player ids in CManager. It is not
	// on either character: it is DB-backed and exists while both are offline,
	// and the partners who are online are validated entities inside it. Every
	// function takes the couple entity and reads a retired one as no couple.
	namespace MarriageSystem
	{
		// Read access to the couple; null for a retired or foreign entity.
		const ecs::CoupleState* State(entt::entity couple);

		uint32_t GetOther(entt::entity couple, uint32_t PID);
		bool IsOnline(entt::entity couple);
		bool IsNear(entt::entity couple);

		int GetMarriagePoint(entt::entity couple);
		int GetMarriageGrade(entt::entity couple);
		int GetBonus(entt::entity couple, uint32_t dwItemVnum, bool bShare = true, entt::entity me = entt::null);

		void Save(entt::entity couple);
		void SetMarried(entt::entity couple);
		void Update(entt::entity couple, uint32_t point);

		void WarpToWeddingMap(entt::entity couple, uint32_t dwPID);
		void RequestEndWedding(entt::entity couple);
	}

	class CManager : public singleton<CManager>
	{
		public:
			CManager();
			virtual ~CManager();

			bool	Initialize();
			void	Destroy();

			// The couple entity for a player id, or entt::null.
			entt::entity	Get(uint32_t dwPlayerID);

			bool	IsMarriageUniqueItem(uint32_t dwItemVnum);

			bool	IsMarried(uint32_t dwPlayerID);
			bool	IsEngaged(uint32_t dwPlayerID);
			bool	IsEngagedOrMarried(uint32_t dwPlayerID);

			void	RequestAdd(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2);
			void	Add(uint32_t dwPID1, uint32_t dwPID2, time_t tMarryTime, const char* szName1, const char* szName2);

			void	RequestUpdate(uint32_t dwPID1, uint32_t dwPID2, int iUpdatePoint, uint8_t byMarried);
			void	Update(uint32_t dwPID1, uint32_t dwPID2, int32_t lTotalPoint, uint8_t byMarried);

			void	RequestRemove(uint32_t dwPID1, uint32_t dwPID2);
			void	Remove(uint32_t dwPID1, uint32_t dwPID2);


			void	Login(entt::entity ch);

			void	Logout(uint32_t pid);
			void	Logout(entt::entity ch);

			void	WeddingReady(uint32_t dwPID1, uint32_t dwPID2, uint32_t dwMapIndex);
			void	WeddingStart(uint32_t dwPID1, uint32_t dwPID2);
			void	WeddingEnd(uint32_t dwPID1, uint32_t dwPID2);

			void	RequestEndWedding(uint32_t dwPID1, uint32_t dwPID2);

			template <typename Func>
				Func	for_each_wedding(Func f);

		private:
			// The index is service state: both player ids to the couple entity.
			std::map<uint32_t, entt::entity> m_MarriageByPID;
			std::set<std::pair<uint32_t, uint32_t> > m_setWedding;
	};

	template <typename Func>
		Func CManager::for_each_wedding(Func f)
		{
			for (auto it = m_setWedding.begin(); it!=m_setWedding.end(); ++it)
			{
				const entt::entity couple = Get(it->first);
				if (couple != entt::null)
					f(couple);
			}
			return f;
		}

}

#endif
