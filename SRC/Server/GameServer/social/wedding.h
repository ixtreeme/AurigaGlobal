#ifndef __WEDDING_H
#define __WEDDING_H

// Wedding ceremony maps are ECS state now: WeddingMapState on a registry-owned
// entity, indexed by the durable private map index in WeddingManager. There is
// no heap WeddingMap object and no raw map pointer on the character.

#include <cstdint>
#include <map>

#include <entt/entt.hpp>

#include "marriage.h"

namespace marriage
{
	const uint32_t WEDDING_MAP_INDEX = 81;

	// Native API over ecs::WeddingMapState. Every entry point validates the
	// entity before use, so a retired or recycled map handle is a no-op.
	namespace WeddingSystem
	{
		uint32_t GetMapIndex(entt::entity map);
		void SetEnded(entt::entity map);
		void WarpAll(entt::entity map);
		void DestroyAll(entt::entity map);
#ifdef TEXTS_IMPROVEMENT
		void Notice(entt::entity map, uint8_t type, uint32_t idx, const char* format, ...);
#endif
		void IncMember(entt::entity map, entt::entity character);
		void DecMember(entt::entity map, entt::entity character);
		bool IsMember(entt::entity map, entt::entity character);
		void SetDark(entt::entity map, bool set);
		void SetSnow(entt::entity map, bool set);
		void SetMusic(entt::entity map, bool set, const char* musicFileName);
		bool IsPlayingMusic(entt::entity map);
		void SendLocalEvent(entt::entity map, entt::entity character);
		void ShoutInMap(entt::entity map, uint8_t type, const char* msg);

		// The character-side relation: the map the character is on. Both take
		// the characters' side, not the map's.
		void SetMemberMap(entt::entity character, entt::entity map);
		entt::entity GetMemberMap(entt::entity character);
	}

	class WeddingManager : public singleton<WeddingManager>
	{
		public:
			WeddingManager();
			virtual ~WeddingManager();

			bool IsWeddingMap(uint32_t dwMapIndex);

			void Request(uint32_t dwPID1, uint32_t dwPID2);
			bool End(uint32_t dwMapIndex);

			void DestroyWeddingMap(entt::entity mapEntity);

			entt::entity Find(uint32_t dwMapIndex);

		private:
			uint32_t __CreateWeddingMap(uint32_t dwPID1, uint32_t dwPID2);

		private:

			std::map<uint32_t, entt::entity> m_mapWedding;
	};
}
#endif
