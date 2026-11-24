#ifndef __INC_GUILD_RENEWAL_BY_RAZOR93_H__
#define __INC_GUILD_RENEWAL_BY_RAZOR93_H__

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <array>

#include <entt/entity/fwd.hpp>
#include <item/GuildProgress.hpp>

class CGuild;

class CGuildRenewal
{
public:
	static CGuildRenewal& instance();


	// Sends full storage and contribution state to a character.
	void SendFullStateTo(entt::entity chEntity);

	// P2P: notify other cores that renewal storage/money/contrib changed
	void P2P_BroadcastRefresh(uint32_t guildId);
	// P2P: called on other cores to reload cache + refresh online members
	void OnP2PRefresh(uint32_t guildId);

	// A disbanded guild's caches must not survive it.
	void InvalidateGuild(uint32_t guildId);


	bool DepositItem(entt::entity character, uint32_t target, uint64_t itemId, uint32_t count);
	bool PayCustom(entt::entity character, uint32_t target, int64_t yang, const std::array<uint32_t,5>& counts,
		std::optional<auriga::item::ItemInstanceId> source = {});

	// Level-up guild using stored items+money (levels 21..60)
	bool TryLevelUp(entt::entity chEntity);

private:
	CGuildRenewal();

	struct StorageSlot
	{
		uint32_t vnum{0};
		uint32_t count{0};
	};

	struct Contrib
	{
		int64_t paidMoney{ 0 };
		int64_t paidItemTotal{ 0 };
		uint8_t paidFlag{ 0 };
		int64_t lastPayTime{ 0 };
	};


	struct LevelReq
	{
		int64_t yang{0};
		std::array<uint32_t,5> vnum{{0,0,0,0,0}};
		std::array<uint32_t,5> count{{0,0,0,0,0}};
	};

	struct GuildCache
	{
		bool loaded{false};
		int64_t money{0};
		std::vector<StorageSlot> storage;
		std::unordered_map<uint32_t, Contrib> contrib;
	};

	GuildCache& GetCache(uint32_t guildId);
	void EnsureLoaded(uint32_t guildId);
	void InvalidateCache(uint32_t guildId); // clears local cache so next load pulls from DB

	// Level-up requirements
	void EnsureLevelReqLoaded();
	bool LoadLevelReqFromJson(const char* filename);

	auriga::item::GuildProgress ProgressSnapshot(uint32_t guildId, uint8_t currentLevel);
	bool RequestProgress(entt::entity character, auriga::item::GuildProgress progress,
		std::vector<auriga::item::MissionItemAmount> costs, std::optional<auriga::item::ItemInstanceId> source = {});
	uint64_t Storage_Count(uint32_t guildId, uint32_t vnum) const;

private:
	std::unordered_map<uint32_t, GuildCache> m_cache;
	bool m_levelReqLoaded{false};
	std::unordered_map<uint8_t, LevelReq> m_levelReqByTargetLevel; // key: 21..60
};

#endif // ENABLE_GUILD_RENEWAL_BY_RAZOR93

#endif // __INC_GUILD_RENEWAL_BY_RAZOR93_H__