#pragma once

#include <entt/entt.hpp>

#include <common/building.h>

namespace building
{
	class CLand;

	namespace ObjectSystem
	{
		entt::entity Create(const TObject& data, uint32_t vid);
		bool IsValid(entt::entity object);
		uint32_t GetID(entt::entity object);
		uint32_t GetVID(entt::entity object);
		uint32_t GetVnum(entt::entity object);
		uint32_t GetGroup(entt::entity object);
		CLand* GetLand(entt::entity object);
		entt::entity GetNPCEntity(entt::entity object);
		void Destroy(entt::entity object);
		bool Show(entt::entity object, int32_t mapIndex, int32_t x, int32_t y);
		void RegenNPC(entt::entity object);
		void ApplySpecialEffect(entt::entity object);
		void Reconstruct(entt::entity object, uint32_t vnum);
	}

	class CLand
	{
		public:
			CLand(TLand * pData);
			~CLand();

			void	Destroy();
			bool IsDestroying() const { return m_destroying; }

			const TLand & GetData();
			void	PutData(const TLand * data);

			uint32_t	GetID() const { return m_data.dwID; }
			void	SetOwner(uint32_t dwGID);
			uint32_t	GetOwner() const { return m_data.dwGuildID; }

			void	InsertObject(entt::entity object);
			entt::entity FindObject(uint32_t dwID);
			entt::entity FindObjectByVID(uint32_t dwVID);
			entt::entity FindObjectByVnum(uint32_t dwVnum);
			entt::entity FindObjectByGroup(uint32_t dwGroupVnum);
			entt::entity FindObjectByNPC(entt::entity npc);
			void UnregisterObject(entt::entity object);
			void DeleteObject(uint32_t dwID);

			bool	RequestCreateObject(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, float xRot, float yRot, float zRot, bool checkAnother);
			void	RequestDeleteObject(uint32_t dwID);
			void	RequestDeleteObjectByVID(uint32_t dwVID);

			void	RequestUpdate(uint32_t dwGuild);

			// LAND_CLEAR
			void	ClearLand();
			// END_LAND_CLEAR

			// BUILD_WALL
			bool RequestCreateWall(int32_t nMapIndex, float rot);
			void RequestDeleteWall();

			bool RequestCreateWallBlocks(uint32_t dwVnum, int32_t nMapIndex, char wallSize, bool doorEast, bool doorWest, bool doorSouth, bool doorNorth);
			void RequestDeleteWallBlocks(uint32_t dwVnum);
			// END_BUILD_WALL

			uint32_t GetMapIndex() { return m_data.lMapIndex; }

		protected:
			TLand			m_data;
			bool m_destroying { false };
			std::map<uint32_t, entt::entity> m_objectsByID;
			std::map<uint32_t, entt::entity> m_objectsByVID;

			// BUILD_WALL
		private :
			void DrawWall(uint32_t dwVnum, int32_t nMapIndex, int32_t& centerX, int32_t& centerY, char length, float zRot);
			// END_BUILD_WALL
	};

	class CManager : public singleton<CManager>
	{
		public:
			CManager();
			virtual ~CManager();

			void	Destroy();

			void	FinalizeBoot();

			bool	LoadObjectProto(const TObjectProto * pProto, int size);
			TObjectProto * GetObjectProto(uint32_t dwVnum);

			bool	LoadLand(TLand * pTable);
			CLand *	FindLand(uint32_t dwID);
			CLand *	FindLand(int32_t lMapIndex, int32_t x, int32_t y);
			CLand *	FindLandByGuild(uint32_t GID);
			void	UpdateLand(TLand * pTable);

			bool	LoadObject(TObject * pTable, bool isBoot=false);
			void	DeleteObject(uint32_t dwID);
			void	UnregisterObject(entt::entity object);

			entt::entity FindObjectByVID(uint32_t dwVID);

			void	SendLandList(LPDESC d, int32_t lMapIndex);

			// LAND_CLEAR
			void	ClearLand(uint32_t dwLandID);
			void	ClearLandByGuildID(uint32_t dwGuildID);
			// END_LAND_CLEAR

		protected:
			std::vector<TObjectProto>		m_vec_kObjectProto;
			std::map<uint32_t, TObjectProto *>	m_map_pkObjectProto;

			std::map<uint32_t, CLand *>		m_map_pkLand;
			std::map<uint32_t, entt::entity> m_objectsByID;
			std::map<uint32_t, entt::entity> m_objectsByVID;
	};
}
