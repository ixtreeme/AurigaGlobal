#ifndef __PRIV_MANAGER_H
#define __PRIV_MANAGER_H

#include <entt/entity/entity.hpp>

class CPrivManager : public singleton<CPrivManager>
{
	public:
		CPrivManager();

		void RequestGiveGuildPriv(uint32_t guild_id, uint8_t type, int value, time_t dur_time_sec);
		void RequestGiveEmpirePriv(uint8_t empire, uint8_t type, int value, time_t dur_time_sec);
		void RequestGiveCharacterPriv(uint32_t pid, uint8_t type, int value);

		void GiveGuildPriv(uint32_t guild_id, uint8_t type, int value, uint8_t bLog, time_t end_time_sec);
		void GiveEmpirePriv(uint8_t empire, uint8_t type, int value, uint8_t bLog, time_t end_time_sec);
		void GiveCharacterPriv(uint32_t pid, uint8_t type, int value, uint8_t bLog);

		void RemoveGuildPriv(uint32_t guild_id, uint8_t type);
		void RemoveEmpirePriv(uint8_t empire, uint8_t type);
		void RemoveCharacterPriv(uint32_t pid, uint8_t type);

		int GetPriv(entt::entity character, uint8_t type);
		int GetPrivByEmpire(uint8_t bEmpire, uint8_t type);
		int GetPrivByGuild(uint32_t guild_id, uint8_t type);
		int GetPrivByCharacter(uint32_t pid, uint8_t type);

	public:
		struct SPrivEmpireData
		{
			int m_value;
			time_t m_end_time_sec;
		};

		SPrivEmpireData* GetPrivByEmpireEx(uint8_t bEmpire, uint8_t type);

		struct SPrivGuildData
		{
			int		value;
			time_t	end_time_sec;
		};

		const SPrivGuildData*	GetPrivByGuildEx( uint32_t dwGuildID, uint8_t byType ) const;

	private:
		SPrivEmpireData m_aakPrivEmpireData[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
		std::map<uint32_t, SPrivGuildData> m_aPrivGuild[MAX_PRIV_NUM];
		std::map<uint32_t, int> m_aPrivChar[MAX_PRIV_NUM];
};
#endif
