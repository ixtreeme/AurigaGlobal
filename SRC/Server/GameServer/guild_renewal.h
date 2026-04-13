#ifndef __INC_GUILD_RENEWAL_BY_RAZOR93_H__
#define __INC_GUILD_RENEWAL_BY_RAZOR93_H__

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <array>

class CHARACTER;
class CGuild;

class CGuildRenewal
{
public:
	static CGuildRenewal& instance();


	void NotifyUnpaidTaxOnLogin(CHARACTER* ch);
	// Sends full storage/tax/contribution state to a character.
	void SendFullStateTo(CHARACTER* ch);

	// P2P: notify other cores that renewal storage/money/contrib changed
	void P2P_BroadcastRefresh(uint32_t guildId);
	// P2P: called on other cores to reload cache + refresh online members
	void OnP2PRefresh(uint32_t guildId);


	// Deposit item from player's INVENTORY into guild storage.
	bool DepositItem(CHARACTER* ch, uint16_t invCell, uint32_t count);

	// Deposit yang from player into guild money.
	bool DepositYang(CHARACTER* ch, int64_t yang);

	// Guild leader sets a tax request (deadline unix timestamp + per member requirements)
	bool SetTaxRequest(CHARACTER* leader, int deadlineUnix,
		int64_t perMemberMoney,
		const std::array<uint32_t,5>& vnums,
		const std::array<uint32_t,5>& counts);

	// Member pays current tax request (moves required items/yang into guild storage/money)
	bool PayTax(CHARACTER* ch);

	// Kis ado: member pays custom amounts (yang + up to 5 items)
	bool PayCustom(CHARACTER* ch, int64_t yang, const std::array<uint32_t,5>& vnums, const std::array<uint32_t,5>& counts);

	// Level-up guild using stored items+money (levels 21..60)
	bool TryLevelUp(CHARACTER* ch);

private:
	CGuildRenewal();

	struct StorageSlot
	{
		uint32_t vnum{0};
		uint32_t count{0};
	};

	struct Tax
	{
		bool active{false};
		int deadline{0};
		int64_t perMemberMoney{0};
		std::array<uint32_t,5> vnum{{0,0,0,0,0}};
		std::array<uint32_t,5> count{{0,0,0,0,0}};
	};

	struct Contrib
	{
		int64_t paidMoney{ 0 };
		int64_t paidItemTotal{ 0 };
		uint8_t paidFlag{ 0 };
		int lastPayTime{ 0 };
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
		Tax tax{};
		std::vector<StorageSlot> storage;
		std::unordered_map<uint32_t, Contrib> contrib;
	};

	GuildCache& GetCache(uint32_t guildId);
	void EnsureLoaded(uint32_t guildId);
	void InvalidateCache(uint32_t guildId); // clears local cache so next load pulls from DB

	// Level-up requirements
	void EnsureLevelReqLoaded();
	bool LoadLevelReqFromFile(const char* filename);

	// DB helpers
	void DB_SaveMoney(uint32_t guildId);
	void DB_SaveTax(uint32_t guildId);
	void DB_SaveSlot(uint32_t guildId, uint16_t slot);
	void DB_SaveContrib(uint32_t guildId, uint32_t pid);

	// Storage helpers
	bool Storage_Add(uint32_t guildId, uint32_t vnum, uint32_t count);
	bool Storage_Remove(uint32_t guildId, uint32_t vnum, uint32_t count);
	uint64_t Storage_Count(uint32_t guildId, uint32_t vnum) const;

	// Inventory helpers
	uint64_t CountItemVnum(CHARACTER* ch, uint32_t vnum) const;
	bool RemoveItemVnum(CHARACTER* ch, uint32_t vnum, uint32_t count);

private:
	std::unordered_map<uint32_t, GuildCache> m_cache;
	bool m_levelReqLoaded{false};
	std::unordered_map<uint8_t, LevelReq> m_levelReqByTargetLevel; // key: 21..60
};

#endif // ENABLE_GUILD_RENEWAL_BY_RAZOR93

#endif // __INC_GUILD_RENEWAL_BY_RAZOR93_H__