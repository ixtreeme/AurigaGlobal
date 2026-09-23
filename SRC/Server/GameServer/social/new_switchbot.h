#pragma once

#ifdef ENABLE_SWITCHBOT
#include <common/tables.h>
#include <entt/entt.hpp>

class SwitchbotHelper
{
public:
	enum class Result { Success, InvalidTarget, NoPayment, RollFailed };
	struct Outcome {
		Result result { Result::InvalidTarget };
		uint32_t materialVnum { 0 }; // Captured before the last material may be destroyed.
	};

	static Outcome TrySwitch(entt::entity owner, entt::entity item, uint8_t slot);
	static bool IsValidItem(entt::entity item);
};

// A player's switchbot job is ecs::SwitchbotState on its own registry-owned
// entity; there is no heap switchbot object. The manager is the player-id
// index over those entities and the entry point the packet handlers use.
class CSwitchbotManager : public singleton<CSwitchbotManager>
{
public:
	CSwitchbotManager();
	virtual ~CSwitchbotManager();

	void Initialize();
	void RegisterItem(uint32_t player_id, uint32_t item_id, uint16_t wCell);
	void UnregisterItem(uint32_t player_id, uint16_t wCell);
	void Start(uint32_t player_id, uint8_t slot, std::vector<TSwitchbotAttributeAlternativeTable> vec_alternatives);
	void Stop(uint32_t player_id, uint8_t slot);

	bool IsActive(uint32_t player_id, uint8_t slot);
	bool IsWarping(uint32_t player_id);
	void SetIsWarping(uint32_t player_id, bool warping);

	entt::entity FindSwitchbot(uint32_t player_id);

	void P2PSendSwitchbot(uint32_t player_id, uint16_t wTargetPort);
	void P2PReceiveSwitchbot(TSwitchbotTable table);

	void SendItemAttributeInformations(entt::entity ch);
	void SendSwitchbotUpdate(uint32_t player_id);

	void EnterGame(entt::entity ch);

protected:
	// The index is service state: player id to the switchbot entity.
	std::map<uint32_t, entt::entity> m_map_Switchbots;
};
#endif
