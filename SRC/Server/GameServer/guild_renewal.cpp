#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93

#include "guild_renewal.h"

#include "char_interface.hpp"
#include "desc.h"
#include "desc_manager.h"
#include "guild.h"
#include "guild_manager.h"
#include "p2p.h"
#include "packet.h"
#include "db.h"
#include "item.h"
#include "item_manager.h"
#include "utils.h"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"

#include <memory>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include "locale_service.h"

namespace
{
	static const int kGuildStorageSize = 15; // 5x3 (client UI)

	// Safe helper: vnum -> localized item name (or nullptr if missing)
	static const char* GetItemNameByVnum(uint32_t vnum, uint8_t lang)
	{
		TItemTable* p = ITEM_MANAGER::instance().GetTable(vnum);
		if (!p)
			return nullptr;

#ifdef ENABLE_MULTI_NAMES
		if (lang >= LANGUAGE_MAX_NUM)
			lang = 0;
		return p->szLocaleName[lang];
#else
		(void)lang;
		return p->szLocaleName;
#endif
	}


	// --------------------------------------------------------------------
	// "Kis ad" rendszer: befizetsek nyilvntartsa (item bontsban)
	// Megjegyzs: a rszletes bonts csak memriban l (upgrade utn nullzdik).
	// A 5 darab szmll a kovetkez guild szinthez tartoz req itemek sorrendjben van.
	// --------------------------------------------------------------------
	using TKisAdoItemArr = std::array<uint32_t, 5>;
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, TKisAdoItemArr>> s_kisAdoItemContrib;

	static TKisAdoItemArr& KisAdo_GetItemArr(uint32_t guildId, uint32_t pid)
	{
		return s_kisAdoItemContrib[guildId][pid]; // default init {0,0,0,0,0}
	}

	static TKisAdoItemArr KisAdo_GetItemArrCopy(uint32_t guildId, uint32_t pid)
	{
		TKisAdoItemArr z{ {0,0,0,0,0} };
		auto itG = s_kisAdoItemContrib.find(guildId);
		if (itG == s_kisAdoItemContrib.end())
			return z;
		auto itP = itG->second.find(pid);
		if (itP == itG->second.end())
			return z;
		return itP->second;
	}

	static void KisAdo_ClearGuild(uint32_t guildId)
	{
		s_kisAdoItemContrib.erase(guildId);
	}


	// --------------------------------------------------------------------
	// Kis ado - reszletes item bontas DB-ben is (multi-core szinkron miatt)
	// Table: guild_renewal_kisado_item (guild_id, pid, item0..item4)
	// --------------------------------------------------------------------
	static void KisAdo_DBSave(uint32_t guildId, uint32_t pid)
	{
		const TKisAdoItemArr& a = KisAdo_GetItemArr(guildId, pid);

		// Ha minden 0, toroljuk a sort (hogy ne nojon feleslegesen a tabla).
		if (a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0 && a[4] == 0)
		{
			DBManager::instance().DirectQuery(
				"DELETE FROM guild_renewal_kisado_item WHERE guild_id=%u AND pid=%u",
				guildId, pid);
			return;
		}

		DBManager::instance().DirectQuery(
			"REPLACE INTO guild_renewal_kisado_item(guild_id,pid,item0,item1,item2,item3,item4) "
			"VALUES(%u,%u,%u,%u,%u,%u,%u)",
			guildId, pid,
			a[0], a[1], a[2], a[3], a[4]);
	}

	static void KisAdo_DBDeleteGuild(uint32_t guildId)
	{
		DBManager::instance().DirectQuery("DELETE FROM guild_renewal_kisado_item WHERE guild_id=%u", guildId);
	}

	static void KisAdo_DBLoadGuild(uint32_t guildId)
	{
		// friss DB load: eloszor toroljuk a memoriat, majd ujratoltjuk
		KisAdo_ClearGuild(guildId);

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
			"SELECT pid,item0,item1,item2,item3,item4 FROM guild_renewal_kisado_item WHERE guild_id=%u",
			guildId));

		if (!msg || !msg->Get())
			return;

		MYSQL_RES* res = msg->Get()->pSQLResult;
		if (!res)
			return;

		const int rows = (int)mysql_num_rows(res);
		for (int i = 0; i < rows; ++i)
		{
			MYSQL_ROW row = mysql_fetch_row(res);
			if (!row)
				continue;

			uint32_t pid = (uint32_t)atoi(row[0]);
			TKisAdoItemArr arr{ {0,0,0,0,0} };
			arr[0] = (uint32_t)strtoul(row[1] ? row[1] : "0", nullptr, 10);
			arr[1] = (uint32_t)strtoul(row[2] ? row[2] : "0", nullptr, 10);
			arr[2] = (uint32_t)strtoul(row[3] ? row[3] : "0", nullptr, 10);
			arr[3] = (uint32_t)strtoul(row[4] ? row[4] : "0", nullptr, 10);
			arr[4] = (uint32_t)strtoul(row[5] ? row[5] : "0", nullptr, 10);

			s_kisAdoItemContrib[guildId][pid] = arr;
		}
	}


	// Parses "YYYY.MM.DD" -> unix at 23:59:59 localtime.
	static int ParseDeadlineYMD(const char* ymd)
	{
		if (!ymd)
			return 0;

		int y = 0, m = 0, d = 0;
		if (std::sscanf(ymd, "%d.%d.%d", &y, &m, &d) != 3)
			return 0;

		tm t = {};
		t.tm_year = y - 1900;
		t.tm_mon = m - 1;
		t.tm_mday = d;
		t.tm_hour = 23;
		t.tm_min = 59;
		t.tm_sec = 59;
		// mktime uses server local time
		return (int)mktime(&t);
	}
}

CGuildRenewal& CGuildRenewal::instance()
{
	static CGuildRenewal s;
	return s;
}

CGuildRenewal::CGuildRenewal()
{
}

CGuildRenewal::GuildCache& CGuildRenewal::GetCache(uint32_t guildId)
{
	auto& c = m_cache[guildId];
	if (c.storage.empty())
		c.storage.resize(kGuildStorageSize);
	return c;
}

void CGuildRenewal::EnsureLoaded(uint32_t guildId)
{
	auto& c = GetCache(guildId);
	if (c.loaded)
		return;

	// Money
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT money FROM guild_renewal_money WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
			if (row && row[0])
				c.money = atoll(row[0]);
		}
		else
		{
			// ensure row exists
			DBManager::instance().DirectQuery("INSERT IGNORE INTO guild_renewal_money(guild_id,money) VALUES(%u,0)", guildId);
			c.money = 0;
		}
	}

	// Tax
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT active,deadline,per_member_money,"
			"item1_vnum,item1_count,item2_vnum,item2_count,item3_vnum,item3_count,item4_vnum,item4_count,item5_vnum,item5_count "
			"FROM guild_renewal_tax WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
			if (row)
			{
				c.tax.active = (atoi(row[0]) != 0);
				c.tax.deadline = atoi(row[1]);
				c.tax.perMemberMoney = atoll(row[2]);
				for (int i = 0; i < 5; i++)
				{
					c.tax.vnum[i] = (uint32_t)atoi(row[3 + i * 2]);
					c.tax.count[i] = (uint32_t)atoi(row[4 + i * 2]);
				}
			}
		}
		else
		{
			DBManager::instance().DirectQuery(
				"INSERT IGNORE INTO guild_renewal_tax(guild_id,active,deadline,per_member_money) VALUES(%u,0,0,0)", guildId);
			c.tax = Tax{};
		}
	}

	// Storage slots
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT slot,vnum,count FROM guild_renewal_storage WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			for (uint64_t i = 0; i < msg->Get()->uiNumRows; i++)
			{
				MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
				if (!row)
					continue;
				int slot = atoi(row[0]);
				if (slot < 0 || slot >= kGuildStorageSize)
					continue;
				c.storage[slot].vnum = (uint32_t)atoi(row[1]);
				c.storage[slot].count = (uint32_t)atoi(row[2]);
			}
		}
	}

	// Contrib
	{
		auto msg = std::unique_ptr<SQLMsg>(DBManager::instance().DirectQuery(
			"SELECT pid,paid_money,paid_item_total,paid_flag,last_pay_time FROM guild_renewal_contrib WHERE guild_id=%u", guildId));
		if (msg && msg->Get() && msg->Get()->uiNumRows > 0)
		{
			for (uint64_t i = 0; i < msg->Get()->uiNumRows; i++)
			{
				MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
				if (!row)
					continue;
				uint32_t pid = (uint32_t)atoi(row[0]);
				Contrib cc;
				cc.paidMoney = atoll(row[1]);
				cc.paidItemTotal = atoll(row[2]);
				cc.paidFlag = (uint8_t)atoi(row[3]);
				cc.lastPayTime = atoi(row[4]);
				c.contrib[pid] = cc;
			}
		}
	}


	// Kis ado item bontas (DB)
	KisAdo_DBLoadGuild(guildId);

	c.loaded = true;
}

void CGuildRenewal::InvalidateCache(uint32_t guildId)
{
	auto it = m_cache.find(guildId);
	if (it == m_cache.end())
		return;

	GuildCache& c = it->second;
	c.loaded = false;
	c.money = 0;
	c.tax = Tax{};
	c.contrib.clear();
	c.storage.clear();
	KisAdo_ClearGuild(guildId);
}

void CGuildRenewal::P2P_BroadcastRefresh(uint32_t guildId)
{
	TPacketGGGuild p;
	p.bHeader = HEADER_GG_GUILD;
	p.bSubHeader = GUILD_SUBHEADER_GG_RENEWAL_REFRESH;
	p.dwGuild = guildId;

	P2P_MANAGER::instance().Send(&p, sizeof(p));
}

void CGuildRenewal::OnP2PRefresh(uint32_t guildId)
{
	// Mark local cache dirty so next EnsureLoaded pulls the newest state from DB.
	InvalidateCache(guildId);

	// Refresh only if this guild has online members on this core.
	std::vector<LPCHARACTER> members;
	members.reserve(16);

	const DESC_MANAGER::DESC_SET& set = DESC_MANAGER::instance().GetClientSet();
	for (DESC_MANAGER::DESC_SET::const_iterator it = set.begin(); it != set.end(); ++it)
	{
		LPDESC d = *it;
		if (!d)
			continue;

		LPCHARACTER member = d->GetCharacter();
		if (!member)
			continue;

		CGuild* g = ecs::SocialSystem::GetGuild(((member) ? (member)->GetEntityHandle() : entt::null));
		if (!g)
			continue;

		if (g->GetID() != guildId)
			continue;

		members.push_back(member);
	}

	if (members.empty())
		return;

	// Force load now, then broadcast fresh state to members on this core.
	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	for (LPCHARACTER m : members)
		SendFullStateTo(m);
}

void CGuildRenewal::EnsureLevelReqLoaded()
{
	if (m_levelReqLoaded)
		return;

	// ugyanonnan mint az exppettable.txt
	std::string base = LocaleService_GetBasePath();
	if (!base.empty() && base.back() != '/' && base.back() != '\\')
		base += '/';

	std::string p0 = base + "guild_renewal_levelup.txt";
	if (LoadLevelReqFromFile(p0.c_str()))
	{
		LOG_INFO("GUILD_RENEWAL: loaded levelup requirements from {}", p0.c_str());
		m_levelReqLoaded = true;
		return;
	}

	// fallback pathok (ha valamirt nem a locale basepath-bl fut)
	const char* paths[] =
	{
		"../conf/guild_renewal_levelup.txt",
		"share/conf/guild_renewal_levelup.txt",
		"./guild_renewal_levelup.txt",
	};

	for (const char* p : paths)
	{
		if (LoadLevelReqFromFile(p))
		{
			LOG_INFO("GUILD_RENEWAL: loaded levelup requirements from {}", p);
			m_levelReqLoaded = true;
			return;
		}
	}

	LOG_ERROR("GUILD_RENEWAL: FAILED to load guild_renewal_levelup.txt. Tried: {}, ../conf/, share/conf/, ./", p0.c_str());

	m_levelReqLoaded = true; // ne prblja minden open-nl jra
}


bool CGuildRenewal::LoadLevelReqFromFile(const char* filename)
{
	FILE* fp = fopen(filename, "r");
	if (!fp)
		return false;

	// ha tbb helyrl prbljuk, ne keveredjen ssze
	m_levelReqByTargetLevel.clear();

	char line[512];
	while (fgets(line, sizeof(line), fp))
	{
		// Trim leading whitespace + BOM + kommentek
		char* p = line;
		while (*p && (unsigned char)(*p) <= ' ')
			++p;

		// UTF-8 BOM
		if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
			p += 3;

		if (!*p || *p == '#' || *p == ';' || (*p == '/' && *(p + 1) == '/'))
			continue;

		int lvl = 0;            // cl ch szint (pl 22, ha most 21 a ch)
		long long yang = 0;
		unsigned v[5] = { 0,0,0,0,0 };
		unsigned c[5] = { 0,0,0,0,0 };

		int n = std::sscanf(p, "%d %lld %u %u %u %u %u %u %u %u %u %u",
			&lvl, &yang,
			&v[0], &c[0],
			&v[1], &c[1],
			&v[2], &c[2],
			&v[3], &c[3],
			&v[4], &c[4]);

		// minimum: lvl + yang
		if (n >= 2 && lvl >= 21 && lvl <= NSBUZERANT)
		{
			LevelReq r;
			r.yang = yang;
			for (int i = 0; i < 5; ++i)
			{
				r.vnum[i] = v[i];
				r.count[i] = c[i];
			}
			m_levelReqByTargetLevel[(uint8_t)lvl] = r;
		}
	}

	fclose(fp);

	if (m_levelReqByTargetLevel.empty())
	{
		LOG_ERROR("GUILD_RENEWAL: file loaded but NO valid rows parsed: {}", filename);
		return false;
	}

	return true;
}


void CGuildRenewal::DB_SaveMoney(uint32_t guildId)
{
	auto& c = GetCache(guildId);
	DBManager::instance().DirectQuery(
		"REPLACE INTO guild_renewal_money(guild_id,money) VALUES(%u,%lld)", guildId, (long long)c.money);
}

void CGuildRenewal::DB_SaveTax(uint32_t guildId)
{
	auto& c = GetCache(guildId);
	DBManager::instance().DirectQuery(
		"REPLACE INTO guild_renewal_tax(guild_id,active,deadline,per_member_money,"
		"item1_vnum,item1_count,item2_vnum,item2_count,item3_vnum,item3_count,item4_vnum,item4_count,item5_vnum,item5_count)"
		" VALUES(%u,%d,%d,%lld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u)",
		guildId,
		c.tax.active ? 1 : 0,
		c.tax.deadline,
		(long long)c.tax.perMemberMoney,
		c.tax.vnum[0], c.tax.count[0],
		c.tax.vnum[1], c.tax.count[1],
		c.tax.vnum[2], c.tax.count[2],
		c.tax.vnum[3], c.tax.count[3],
		c.tax.vnum[4], c.tax.count[4]);
}

void CGuildRenewal::DB_SaveSlot(uint32_t guildId, uint16_t slot)
{
	auto& c = GetCache(guildId);
	if (slot >= c.storage.size())
		return;

	auto& s = c.storage[slot];
	DBManager::instance().DirectQuery(
		"REPLACE INTO guild_renewal_storage(guild_id,slot,vnum,count) VALUES(%u,%u,%u,%u)",
		guildId, (unsigned)slot, (unsigned)s.vnum, (unsigned)s.count);
}

void CGuildRenewal::DB_SaveContrib(uint32_t guildId, uint32_t pid)
{
	auto& c = GetCache(guildId);
	auto it = c.contrib.find(pid);
	if (it == c.contrib.end())
		return;
	const Contrib& cc = it->second;

	DBManager::instance().DirectQuery(
		"REPLACE INTO guild_renewal_contrib(guild_id,pid,paid_money,paid_item_total,paid_flag,last_pay_time)"
		" VALUES(%u,%u,%lld,%lld,%u,%d)",
		guildId, pid,
		(long long)cc.paidMoney,
		(long long)cc.paidItemTotal,
		(unsigned)cc.paidFlag,
		cc.lastPayTime);
}

uint64_t CGuildRenewal::Storage_Count(uint32_t guildId, uint32_t vnum) const
{
	auto it = m_cache.find(guildId);
	if (it == m_cache.end() || it->second.storage.empty())
		return 0;

	uint64_t sum = 0;
	for (const auto& s : it->second.storage)
		if (s.vnum == vnum)
			sum += s.count;
	return sum;
}

bool CGuildRenewal::Storage_Add(uint32_t guildId, uint32_t vnum, uint32_t count)
{
	auto& c = GetCache(guildId);

	// stack into existing slot
	for (uint16_t i = 0; i < c.storage.size(); i++)
	{
		auto& s = c.storage[i];
		if (s.vnum == vnum && s.count > 0)
		{
			s.count += count;
			DB_SaveSlot(guildId, i);
			return true;
		}
	}

	// find empty
	for (uint16_t i = 0; i < c.storage.size(); i++)
	{
		auto& s = c.storage[i];
		if (s.vnum == 0 || s.count == 0)
		{
			s.vnum = vnum;
			s.count = count;
			DB_SaveSlot(guildId, i);
			return true;
		}
	}

	return false;
}

bool CGuildRenewal::Storage_Remove(uint32_t guildId, uint32_t vnum, uint32_t count)
{
	auto& c = GetCache(guildId);
	uint32_t need = count;

	for (uint16_t i = 0; i < c.storage.size() && need>0; i++)
	{
		auto& s = c.storage[i];
		if (s.vnum != vnum || s.count == 0)
			continue;
		uint32_t take = MIN(need, s.count);
		s.count -= take;
		need -= take;
		if (s.count == 0)
			s.vnum = 0;
		DB_SaveSlot(guildId, i);
	}

	return (need == 0);
}

uint64_t CGuildRenewal::CountItemVnum(CHARACTER* ch, uint32_t vnum) const
{
	if (!ch)
		return 0;

	uint64_t total = 0;
	const entt::entity owner = ((ch) ? (ch)->GetEntityHandle() : entt::null);
	for (uint16_t i = 0; i < INVENTORY_MAX_NUM; i++)
	{
		const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
		if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemVnum(item) == vnum)
			total += ItemSystem::GetItemCount(item);
	}

#ifdef ENABLE_EXTRA_INVENTORY
	for (uint16_t i = 0; i < EXTRA_INVENTORY_MAX_NUM; i++)
	{
		const entt::entity item = ItemSystem::GetExtraInventoryItem(owner, i);
		if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemVnum(item) == vnum)
			total += ItemSystem::GetItemCount(item);
	}
#endif

	return total;
}


bool CGuildRenewal::RemoveItemVnum(CHARACTER* ch, uint32_t vnum, uint32_t count)
{
	if (!ch)
		return false;

	const entt::entity owner = ((ch) ? (ch)->GetEntityHandle() : entt::null);
	uint32_t need = count;
	for (uint16_t i = 0; i < INVENTORY_MAX_NUM && need>0; i++)
	{
		const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
		if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemVnum(item) != vnum)
			continue;

		uint32_t cur = ItemSystem::GetItemCount(item);
		uint32_t take = MIN(need, cur);
		uint32_t newCount = cur - take;
		need -= take;
		ItemSystem::SetItemCountEcs(item, newCount);
	}

#ifdef ENABLE_EXTRA_INVENTORY
	for (uint16_t i = 0; i < EXTRA_INVENTORY_MAX_NUM && need>0; i++)
	{
		const entt::entity item = ItemSystem::GetExtraInventoryItem(owner, i);
		if (!ItemSystem::IsValidItem(item) || ItemSystem::GetItemVnum(item) != vnum)
			continue;

		uint32_t cur = ItemSystem::GetItemCount(item);
		uint32_t take = MIN(need, cur);
		uint32_t newCount = cur - take;
		need -= take;
		ItemSystem::SetItemCountEcs(item, newCount);
	}
#endif

	return (need == 0);
}


void CGuildRenewal::SendFullStateTo(CHARACTER* ch)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch || !ecs::PlayerRuntime::GetDesc(chEntity))
		return;

	CGuild* g = ecs::SocialSystem::GetGuild(chEntity);
	if (!g)
		return;

	const uint32_t guildId = g->GetID();

	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	auto& c = GetCache(guildId);

	// UI reset + guild storage money
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_clear");
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_money %lld", (long long)c.money);

	// per-player paid flag (Ad: O/X a kliensben)
	{
		uint8_t paidFlag = 0;
		auto itPaid = c.contrib.find(ecs::PlayerRuntime::GetPlayerID(chEntity));
		if (itPaid != c.contrib.end())
			paidFlag = itPaid->second.paidFlag;

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_paid %u", (unsigned)paidFlag);
	}

	// next guild level requirements (yang + 5 item)
	{
		const uint8_t target = (uint8_t)(g->GetLevel() + 1);

		long long reqYang = 0;
		uint32_t rv[5] = { 0,0,0,0,0 };
		uint32_t rc[5] = { 0,0,0,0,0 };

		auto itReq = m_levelReqByTargetLevel.find(target);
		if (itReq != m_levelReqByTargetLevel.end())
		{
			reqYang = (long long)itReq->second.yang;
			for (int i = 0; i < 5; ++i)
			{
				rv[i] = itReq->second.vnum[i];
				rc[i] = itReq->second.count[i];
			}
		}
		else
		{
			LOG_INFO("GUILD_RENEWAL: no req row for target level={} (guild level={})", (unsigned)target, g->GetLevel());
		}

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_req %lld %u %u %u %u %u %u %u %u %u %u",
			reqYang,
			rv[0], rc[0],
			rv[1], rc[1],
			rv[2], rc[2],
			rv[3], rc[3],
			rv[4], rc[4]);
	}

	// storage slots
	for (uint16_t i = 0; i < c.storage.size(); ++i)
	{
		auto& s = c.storage[i];
		if (s.vnum && s.count)
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_item %u %u %u", (unsigned)i, (unsigned)s.vnum, (unsigned)s.count);
	}

	// tax request state (kis ad rendszerben nincs kivetett ad)
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_tax 0 0 0 0 0 0 0 0 0 0 0 0 0");


	// contrib list
	for (auto& kv : c.contrib)
	{
		const TKisAdoItemArr itemArr = KisAdo_GetItemArrCopy(guildId, kv.first);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_contrib2 %u %u %lld %lld %u %u %u %u %u",
			(unsigned)kv.first,
			(unsigned)kv.second.paidFlag,
			(long long)kv.second.paidMoney,
			(long long)kv.second.paidItemTotal,
			(unsigned)itemArr[0],
			(unsigned)itemArr[1],
			(unsigned)itemArr[2],
			(unsigned)itemArr[3],
			(unsigned)itemArr[4]);

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_contrib %u %u %lld %lld",
			(unsigned)kv.first,
			(unsigned)kv.second.paidFlag,
			(long long)kv.second.paidMoney,
			(long long)kv.second.paidItemTotal);
	}

	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "gr_done");
}


bool CGuildRenewal::DepositItem(CHARACTER* ch, uint16_t invCell, uint32_t count)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch)
		return false;

	CGuild* g = ecs::SocialSystem::GetGuild(chEntity);
	if (!g)
		return false;

	// kis ad: csak fejlesztshez (20-as szinttl)
	if (g->GetLevel() < 20)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh fejlesztes csak 20-as szinttol elerheto.");
		return false;
	}

	const uint32_t guildId = g->GetID();
	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	const uint8_t target = (uint8_t)(g->GetLevel() + 1);
	auto itReq = m_levelReqByTargetLevel.find(target);
	if (itReq == m_levelReqByTargetLevel.end())
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Nincs beallitva fejlesztesi kovetelmeny ehhez a szinthez: %u", (unsigned)target);
		return false;
	}
	const LevelReq& req = itReq->second;

	const entt::entity owner = chEntity;
	const entt::entity item = ItemSystem::GetInventoryItem(owner, invCell);
	if (!ItemSystem::IsValidItem(item))
		return false;

	const uint32_t haveCount = ItemSystem::GetItemCount(item);
	if (haveCount == 0)
		return false;

	if (count == 0 || count > haveCount)
		count = haveCount;

	const uint32_t vnum = ItemSystem::GetItemVnum(item);

	// Csak azokat engedjk, amiket ppen kr a ch a kvetkez szinthez
	int reqIdx = -1;
	uint64_t totalNeedForVnum = 0;
	for (int i = 0; i < 5; ++i)
	{
		if (req.vnum[i] && req.count[i] && req.vnum[i] == vnum)
		{
			if (reqIdx == -1)
				reqIdx = i;
			totalNeedForVnum += req.count[i];
		}
	}

	if (reqIdx == -1 || totalNeedForVnum == 0)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Ezt a targyat most nem keri a ceh fejleszteshez.");
		return false;
	}

	// Ha mr megvan belle elg a ch raktrban, ne lehessen tbbet betenni
	const uint64_t haveInStorage = Storage_Count(guildId, vnum);
	if (haveInStorage >= totalNeedForVnum)
	{
		const uint8_t lang = (ecs::PlayerRuntime::GetDesc(chEntity) ? ecs::PlayerRuntime::GetDesc(chEntity)->GetLanguage() : 0);
		const char* name = GetItemNameByVnum(vnum, lang);
		if (name && *name)
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Ebbol a targybol mar eleg van a fejleszteshez: %s", name);
		else
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Ebbol a targybol mar eleg van a fejleszteshez: VNUM %u", (unsigned)vnum);
		return false;
	}

	const uint64_t remaining = totalNeedForVnum - haveInStorage;

	const uint32_t requested = count;
	const uint32_t allowed = (uint32_t)MIN((uint64_t)requested, remaining);
	if (allowed == 0)
		return false;

	// Elbb raktrba prbljuk tenni (ha tele van, ne vegyk el a jtkostl)
	if (!Storage_Add(guildId, vnum, allowed))
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh raktar tele van.");
		return false;
	}

	// Levons a jtkostl
	ItemSystem::SetItemCountEcs(item, haveCount - allowed);

	// Befizets nyilvntarts (sszestve + rszletes bonts memriban)
	auto& c = GetCache(guildId);
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(chEntity);

	Contrib& cc = c.contrib[pid];
	cc.paidItemTotal += (int64_t)allowed;
	cc.paidFlag = 1;
	cc.lastPayTime = (int)get_global_time();
	DB_SaveContrib(guildId, pid);

	KisAdo_GetItemArr(guildId, pid)[reqIdx] += allowed;
	KisAdo_DBSave(guildId, pid);

	// Info, ha kevesebbet fogadott el (mert mr csak ennyi hinyzott)
	if (allowed < requested)
	{
		const uint8_t lang = (ecs::PlayerRuntime::GetDesc(chEntity) ? ecs::PlayerRuntime::GetDesc(chEntity)->GetLanguage() : 0);
		const char* name = GetItemNameByVnum(vnum, lang);
		if (name && *name)
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Csak %u db-ot tett be (ennyire hianyzott): %s", (unsigned)allowed, name);
		else
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Csak %u db-ot tett be (ennyire hianyzott).", (unsigned)allowed);
	}

	// Kliens frissites (legalabb a befizeto + cehvezeto kapjon azonnali infot)
	SendFullStateTo(ch);
	if (LPCHARACTER master = g->GetMasterCharacter())
	{
		if (master != ch)
			SendFullStateTo(master);
	}

	return true;
}

bool CGuildRenewal::DepositYang(CHARACTER* ch, int64_t yang)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch)
		return false;

	CGuild* g = ecs::SocialSystem::GetGuild(chEntity);
	if (!g)
		return false;

	// kis ad: csak fejlesztshez (20-as szinttl)
	if (g->GetLevel() < 20)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh fejlesztes csak 20-as szinttol elerheto.");
		return false;
	}

	if (yang <= 0)
		return false;

	const uint32_t guildId = g->GetID();
	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	const uint8_t target = (uint8_t)(g->GetLevel() + 1);
	auto itReq = m_levelReqByTargetLevel.find(target);
	if (itReq == m_levelReqByTargetLevel.end())
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Nincs beallitva fejlesztesi kovetelmeny ehhez a szinthez: %u", (unsigned)target);
		return false;
	}
	const LevelReq& req = itReq->second;

	auto& c = GetCache(guildId);

	// Ha mr megvan elg yang a ch raktrban, ne lehessen tbbet betenni
	int64_t remaining = (int64_t)req.yang - (int64_t)c.money;
	if (remaining <= 0)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Mar elegendo yang van a ceh leltarban a fejleszteshez.");
		return false;
	}

	const int64_t playerGold = (int64_t)ecs::PointSystem::GetGold(chEntity);
	if (playerGold <= 0)
		return false;

	const int64_t requested = yang;
	int64_t allowed = requested;
	if (allowed > remaining)
		allowed = remaining;
	if (allowed > playerGold)
		allowed = playerGold;

	if (allowed <= 0)
		return false;

	// Levons + hozzads
	ecs::PointSystem::Change(chEntity, POINT_GOLD, -allowed, true);
	c.money += allowed;
	DB_SaveMoney(guildId);

	// Befizets nyilvntarts
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(chEntity);
	Contrib& cc = c.contrib[pid];
	cc.paidMoney += allowed;
	cc.paidFlag = 1;
	cc.lastPayTime = (int)get_global_time();
	DB_SaveContrib(guildId, pid);

	// Info, ha kevesebbet fogadott el (mert mr csak ennyi hinyzott)
	if (allowed < requested)
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Csak %lld yangot tett be (ennyire hianyzott).", (long long)allowed);

	// Kliens frissites (legalabb a befizeto + cehvezeto kapjon azonnali infot)
	SendFullStateTo(ch);
	if (LPCHARACTER master = g->GetMasterCharacter())
	{
		if (master != ch)
			SendFullStateTo(master);
	}

	return true;
}

bool CGuildRenewal::SetTaxRequest(CHARACTER* leader, int deadlineUnix, int64_t perMemberMoney,
	const std::array<uint32_t, 5>& vnums,
	const std::array<uint32_t, 5>& counts)
{
	const entt::entity leaderEntity = leader ? leader->GetEntityHandle() : entt::null;
	(void)deadlineUnix;
	(void)perMemberMoney;
	(void)vnums;
	(void)counts;

	if (!leader)
		return false;

	CGuild* g = ecs::SocialSystem::GetGuild(leaderEntity);
	if (!g)
	{
		ecs::ChatSystem::Send(leaderEntity, CHAT_TYPE_INFO, "Nincs cehed.");
		return false;
	}

	ecs::ChatSystem::Send(leaderEntity, CHAT_TYPE_INFO, "Ado rendszer ki van kapcsolva. (Kis ado: szabad befizetes van.)");
	return false;
}

bool CGuildRenewal::PayTax(CHARACTER* ch)
{
	if (!ch)
		return false;

	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "Ado rendszer ki van kapcsolva. Hasznald a Yang betesz / Targy betesz gombokat.");
	return false;
}

bool CGuildRenewal::PayCustom(CHARACTER* ch, int64_t yang, const std::array<uint32_t,5>& vnums, const std::array<uint32_t,5>& counts)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch)
		return false;

	CGuild* g = ecs::SocialSystem::GetGuild(chEntity);
	if (!g)
		return false;

	// kis ado: csak fejleszteshez (20-as szinttol)
	if (g->GetLevel() < 20)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh fejlesztes csak 20-as szinttol elerheto.");
		return false;
	}

	const uint32_t guildId = g->GetID();
	EnsureLoaded(guildId);
	EnsureLevelReqLoaded();

	const uint8_t target = (uint8_t)(g->GetLevel() + 1);
	auto itReq = m_levelReqByTargetLevel.find(target);
	if (itReq == m_levelReqByTargetLevel.end())
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Nincs beallitva fejlesztesi kovetelmeny ehhez a szinthez: %u", (unsigned)target);
		return false;
	}
	const LevelReq& req = itReq->second;

	auto& c = GetCache(guildId);	
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(chEntity);
	bool any = false;

	// 1) Yang befizetes (clamp a hianyzo osszegre)
	if (yang > 0)
	{
		int64_t remaining = (int64_t)req.yang - (int64_t)c.money;
		if (remaining > 0)
		{
			int64_t playerGold = (int64_t)ecs::PointSystem::GetGold(chEntity);
			int64_t allowed = yang;
			if (allowed > remaining) allowed = remaining;
			if (allowed > playerGold) allowed = playerGold;
			if (allowed > 0)
			{
				ecs::PointSystem::Change(chEntity, POINT_GOLD, -allowed, true);
				c.money += allowed;
				DB_SaveMoney(guildId);

				Contrib& cc = c.contrib[pid];
				cc.paidMoney += allowed;
				cc.paidFlag = 1;
				cc.lastPayTime = (int)get_global_time();
				DB_SaveContrib(guildId, pid);

				any = true;
			}
		}
	}

	// 2) Item befizetesek (vnum+count par vagy indexelt count)
	//    - ha vnums[]-ban van barmi, akkor azokat tekintjuk paroknak
	//    - kulonben counts[] a req indexekhez tartozik
	bool hasExplicitVnum = false;
	for (int i = 0; i < 5; ++i)
	{
		if (vnums[i] != 0) { hasExplicitVnum = true; break; }
	}

	std::unordered_map<uint32_t, uint64_t> needByVnum;
	for (int i = 0; i < 5; ++i)
	{
		if (req.vnum[i] && req.count[i])
			needByVnum[req.vnum[i]] += req.count[i];
	}

	std::unordered_map<uint32_t, uint64_t> reqByVnum;
	if (hasExplicitVnum)
	{
		for (int i = 0; i < 5; ++i)
		{
			if (!vnums[i] || !counts[i])
				continue;
			reqByVnum[vnums[i]] += counts[i];
		}
	}
	else
	{
		for (int i = 0; i < 5; ++i)
		{
			if (!counts[i] || !req.vnum[i])
				continue;
			reqByVnum[req.vnum[i]] += counts[i];
		}
	}

	for (const auto& kv : reqByVnum)
	{
		const uint32_t vnum = kv.first;
		uint64_t want = kv.second;
		if (want == 0)
			continue;

		auto itNeed2 = needByVnum.find(vnum);
		if (itNeed2 == needByVnum.end())
		{
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Ezt a targyat most nem keri a ceh fejleszteshez. VNUM: %u", (unsigned)vnum);
			continue;
		}

		const uint64_t needTotal = itNeed2->second;
		const uint64_t haveStorage = Storage_Count(guildId, vnum);
		if (haveStorage >= needTotal)
			continue;

		uint64_t remaining = needTotal - haveStorage;
		uint64_t allowed = want;
		if (allowed > remaining)
			allowed = remaining;

		const uint64_t havePlayer = CountItemVnum(ch, vnum);
		if (allowed > havePlayer)
			allowed = havePlayer;

		if (allowed == 0)
			continue;

		// Storage add elobb (ha tele van, ne vegyuk el)
		if (!Storage_Add(guildId, vnum, (uint32_t)allowed))
		{
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh raktar tele van.");
			continue;
		}

		if (!RemoveItemVnum(ch, vnum, (uint32_t)allowed))
		{
			// rollback
			Storage_Remove(guildId, vnum, (uint32_t)allowed);
			ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Hiba: nem sikerult levonni a targyat (VNUM %u).", (unsigned)vnum);
			continue;
		}

		Contrib& cc = c.contrib[pid];
		cc.paidItemTotal += (int64_t)allowed;
		cc.paidFlag = 1;
		cc.lastPayTime = (int)get_global_time();
		DB_SaveContrib(guildId, pid);

		int reqIdx = -1;
		for (int i = 0; i < 5; ++i)
		{
			if (req.vnum[i] && req.count[i] && req.vnum[i] == vnum)
			{
				reqIdx = i;
				break;
			}
		}
		if (reqIdx != -1)
			KisAdo_GetItemArr(guildId, pid)[reqIdx] += (uint32_t)allowed;
			KisAdo_DBSave(guildId, pid);

		any = true;
	}

	if (any)
	{
		SendFullStateTo(ch);
		if (LPCHARACTER master = g->GetMasterCharacter())
		{
			if (master != ch)
				SendFullStateTo(master);
		}
	}

	return any;
}

bool CGuildRenewal::TryLevelUp(CHARACTER* ch)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch)
		return false;

	CGuild* g = ecs::SocialSystem::GetGuild(chEntity);
	if (!g)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Nincs cehed.");
		return false;
	}

	const uint8_t curLvl = g->GetLevel();
	if (curLvl < 20)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh fejlesztes csak 20-as szinttol elerheto.");
		return false;
	}
	if (curLvl >= NSBUZERANT)
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A ceh elerte a maximum szintet.");
		return false;
	}

	const uint8_t target = curLvl + 1;

	EnsureLevelReqLoaded();
	auto itReq = m_levelReqByTargetLevel.find(target);
	if (itReq == m_levelReqByTargetLevel.end())
	{
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Nincs beallitva fejlesztesi kovetelmeny ehhez a szinthez: %u", (unsigned)target);
		return false;
	}

	const uint32_t guildId = g->GetID();
	EnsureLoaded(guildId);

	auto& c = GetCache(guildId);
	const LevelReq& req = itReq->second;

	// Money check (GUILD storage money)
	if (c.money < req.yang)
	{
		const long long diff = (long long)(req.yang - c.money);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Nincs elegendo yang a ceh leltarban. Hianyzik: %lld", diff);
		return false;
	}

	// Item check (GUILD storage items)  
	{
		std::unordered_map<uint32_t, uint32_t> needByVnum;


		for (int i = 0; i < 5; ++i)
		{
			if (!req.vnum[i] || !req.count[i])
				continue;

			needByVnum[req.vnum[i]] += req.count[i];
		}

		const uint8_t lang = (ecs::PlayerRuntime::GetDesc(chEntity) ? ecs::PlayerRuntime::GetDesc(chEntity)->GetLanguage() : 0);
		bool hasMissing = false;

		for (const auto& kv : needByVnum)
		{
			const uint32_t vnum = kv.first;
			const uint32_t need = kv.second;

			const uint32_t have = (uint32_t)Storage_Count(guildId, vnum);
			if (have < need)
			{
				hasMissing = true;
				const uint32_t diff = need - have;

				const char* name = GetItemNameByVnum(vnum, lang);
				if (name && *name)
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Hianyzik fejlesztesi targy a ceh leltarbol: %s x%u", name, (unsigned)diff);
				else
					ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Hianyzik fejlesztesi targy a ceh leltarbol: VNUM %u x%u", (unsigned)vnum, (unsigned)diff);
			}
		}

		if (hasMissing)
			return false;
	}


	// Consume items
	for (int i = 0; i < 5; ++i)
	{
		if (req.vnum[i] && req.count[i])
		{
			if (!Storage_Remove(guildId, req.vnum[i], req.count[i]))
			{
				ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Hiba: nem sikerult levonni egy fejlesztesi targyat (VNUM %u).", (unsigned)req.vnum[i]);
				return false;
			}
		}
	}

	// Consume money
	c.money -= req.yang;
	DB_SaveMoney(guildId);

	// Level up
	g->RenewalSetLevel(target);

	// ------------------------------------------------------------
	// Kis ad rendszer: fejleszts utn minden befizets nullzdik
	// ------------------------------------------------------------
	{
		// Tax rendszer kikapcsolva
		c.tax.active = false;
		c.tax.deadline = 0;
		c.tax.perMemberMoney = 0;
		c.tax.vnum.fill(0);
		c.tax.count.fill(0);
		DB_SaveTax(guildId);

		// Befizetsek trlse
		c.contrib.clear();
		DBManager::instance().DirectQuery("DELETE FROM guild_renewal_contrib WHERE guild_id=%u", guildId);
		KisAdo_ClearGuild(guildId);
		KisAdo_DBDeleteGuild(guildId);

		// Ch fejleszts raktr trlse
		for (auto& s : c.storage)
		{
			s.vnum = 0;
			s.count = 0;
		}
		DBManager::instance().DirectQuery("DELETE FROM guild_renewal_storage WHERE guild_id=%u", guildId);

		// Yang nullzs (clamp miatt elvileg mr 0, de biztos ami biztos)
		c.money = 0;
		DB_SaveMoney(guildId);
	}


	// Kliens frissites (legalabb a befizeto + cehvezeto kapjon azonnali infot)
	SendFullStateTo(ch);
	if (LPCHARACTER master = g->GetMasterCharacter())
	{
		if (master != ch)
			SendFullStateTo(master);
	}

	return true;
}



#endif // ENABLE_GUILD_RENEWAL_BY_RAZOR93

