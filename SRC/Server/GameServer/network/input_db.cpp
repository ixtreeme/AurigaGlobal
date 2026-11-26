#include "stdafx.h"
#include "../item/TypedBooks.hpp"
#include <common/character_creation.h>
#include <Core/packet_string.h>
#include <common/time_64_packets.h>
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/components/skill_components.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "constants.h"
#include "config.h"
#include "utils.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "packet.h"
#include "protocol.h"
#include "mob_manager.h"
#include "sectree_manager.h"
#include "skill.h"
#include "questmanager.h"
#include "p2p.h"
#include "guild.h"
#include "guild_manager.h"
#include "start_position.h"
#include "party.h"
#include "banword.h"
#include "priv_manager.h"
#include "db.h"
#include "building.h"

#include "wedding.h"
#include "login_data.h"
#include "unique_item.h"

#include "affect.h"
#include "motion.h"

#include "dev_log.h"
#include <Core/Logging.hpp>

#include "log.h"

#include "horsename_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "gm.h"
#include "map_location.h"


#include "shutdown_manager.h"
#include <common/CommonDefines.h>

#include "desc_client.h"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/VIDRegistry.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/components/vital_components.hpp"
#include "../ecs/components/inventory_components.hpp"
#include "../item/ItemStore.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/TypedInventorySync.hpp"
#include "../item/TypedEquipment.hpp"
#include "../item/TypedAccessoryClock.hpp"
#include "../item/TypedRecovery.hpp"
#ifdef ENABLE_HWID
#include "hwidmanager.h"
#endif

#define MAPNAME_DEFAULT	"none"

bool GetServerLocation(TAccountTable & rTab, uint8_t bEmpire)
{
	bool bFound = false;

	for (auto& player : rTab.players)
	{
		if (0 == player.dwID)
			continue;

		bFound = true;
		int32_t lIndex = 0;

		if (!CMapLocation::instance().Get(
#ifdef ENABLE_GENERAL_CH
rTab.bChannel,
#endif
player.x,
player.y,
					lIndex,
player.lAddr,
player.wPort))
		{
			LOG_ERROR("location error name {} mapindex {} {} x {} empire {}", player.szName, lIndex, player.x, player.y, rTab.bEmpire);

			player.x = EMPIRE_START_X(rTab.bEmpire);
			player.y = EMPIRE_START_Y(rTab.bEmpire);

			lIndex = 0;

			if (!CMapLocation::instance().Get(
#ifdef ENABLE_GENERAL_CH
rTab.bChannel,
#endif
player.x, player.y, lIndex, player.lAddr, player.wPort))
			{
				LOG_ERROR("cannot find server for mapindex {} {} x {} (name {})", lIndex, player.x, player.y, player.szName);

				continue;
			}
		}

		struct in_addr in;
		in.s_addr = player.lAddr;
		LOG_INFO("success to {}:{}", inet_ntoa(in), player.wPort);
	}

	return bFound;
}


void CInputDB::LoginSuccess(uint32_t dwHandle, const char *data)
{
	LOG_INFO("LoginSuccess");

	TAccountTable* pTab = (TAccountTable*)data;

	LPDESC d = DESC_MANAGER::instance().FindByHandle(dwHandle);

	if (!d)
	{
		LOG_INFO("CInputDB::LoginSuccess - cannot find handle [{}]", pTab->login);

		TLogoutPacket pack;

		strlcpy(pack.login, pTab->login, sizeof(pack.login));
		db_clientdesc->DBPacket(HEADER_GD_LOGOUT, dwHandle, &pack, sizeof(pack));
		return;
	}

	if (strcmp(pTab->status, "OK"))
	{
		LOG_INFO("CInputDB::LoginSuccess - status[{}] is not OK [{}]", pTab->status, pTab->login);

		TLogoutPacket pack;

		strlcpy(pack.login, pTab->login, sizeof(pack.login));
		db_clientdesc->DBPacket(HEADER_GD_LOGOUT, dwHandle, &pack, sizeof(pack));

		LoginFailure(d, pTab->status);
		return;
	}

	for (int i = 0; i != PLAYER_PER_ACCOUNT; ++i)
	{
		TSimplePlayer& player = pTab->players[i];
		LOG_INFO("\tplayer({}).job({})", player.szName, player.byJob);
	}

	bool bFound = GetServerLocation(*pTab, pTab->bEmpire);

//#ifdef ENABLE_GENERAL_CH
//	pTab->bChannel = g_bChannel;
//#endif

	d->BindAccountTable(pTab);


	if (!bFound)
	{
		TPacketGCEmpire pe;
		pe.bHeader = HEADER_GC_EMPIRE;
		pe.bEmpire = pTab->bEmpire; // zero requests real empire selection for a new account
		d->Packet(&pe, sizeof(pe));
	}
	else
	{
		TPacketGCEmpire pe;
		pe.bHeader = HEADER_GC_EMPIRE;
		pe.bEmpire = d->GetEmpire();
		d->Packet(&pe, sizeof(pe));
	}

	d->SetPhase(PHASE_SELECT);
	d->SendLoginSuccessPacket();

	// __SHUTDOWN::Shutdown Register
	CShutdownManager::Instance().AddDesc(d);

	LOG_INFO("InputDB::login_success: {}", pTab->login);
}

void CInputDB::PlayerCreateFailure(LPDESC d, uint8_t bType)
{
	if (!d)
		return;

	TPacketGCCreateFailure pack;

	pack.header	= HEADER_GC_CHARACTER_CREATE_FAILURE;
	pack.bType	= bType;

	d->Packet(&pack, sizeof(pack));
}

void CInputDB::PlayerCreateSuccess(LPDESC d, const char * data)
{
	if (!d)
		return;

	TPacketDGCreateSuccess * pPacketDB = (TPacketDGCreateSuccess *) data;

	if (pPacketDB->bAccountCharacterIndex >= PLAYER_PER_ACCOUNT)
	{
		d->Packet(encode_byte(HEADER_GC_CHARACTER_CREATE_FAILURE), 1);
		return;
	}

	int32_t lIndex;

	if (!CMapLocation::instance().Get(
#ifdef ENABLE_GENERAL_CH
d->GetAccountTable().bChannel,
#endif
				pPacketDB->player.x,
				pPacketDB->player.y,
				lIndex,
				pPacketDB->player.lAddr,
				pPacketDB->player.wPort))
	{
		LOG_ERROR("InputDB::PlayerCreateSuccess: cannot find server for mapindex {} {} x {} (name {})", lIndex, pPacketDB->player.x, pPacketDB->player.y, pPacketDB->player.szName);
	}

	TAccountTable & r_Tab = d->GetAccountTable();
	r_Tab.players[pPacketDB->bAccountCharacterIndex] = pPacketDB->player;

	TPacketGCPlayerCreateSuccess pack;

	pack.header = HEADER_GC_CHARACTER_CREATE_SUCCESS;
	pack.bAccountCharacterIndex = pPacketDB->bAccountCharacterIndex;
	pack.player = pPacketDB->player;

	d->Packet(&pack, sizeof(TPacketGCPlayerCreateSuccess));


}

void CInputDB::PlayerDeleteSuccess(LPDESC d, const char * data)
{
	if (!d)
		return;

	uint8_t account_index;
	account_index = decode_byte(data);
	d->BufferedPacket(encode_byte(HEADER_GC_CHARACTER_DELETE_SUCCESS),	1);
	d->Packet(encode_byte(account_index),			1);

	d->GetAccountTable().players[account_index].dwID = 0;
}

void CInputDB::PlayerDeleteFail(LPDESC d)
{
	if (!d)
		return;

	d->Packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID),	1);
	//d->Packet(encode_byte(account_index),			1);

	//d->GetAccountTable().players[account_index].dwID = 0;
}

void CInputDB::ChangeName(LPDESC d, const char * data)
{
	if (!d)
		return;

	TPacketDGChangeName * p = (TPacketDGChangeName *) data;

	TAccountTable & r = d->GetAccountTable();

	if (!r.id)
		return;

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		if (r.players[i].dwID == p->pid)
		{
			strlcpy(r.players[i].szName, p->name, sizeof(r.players[i].szName));
			r.players[i].bChangeName = 0;

			TPacketGCChangeName pgc;

			pgc.header = HEADER_GC_CHANGE_NAME;
			pgc.pid = p->pid;
			strlcpy(pgc.name, p->name, sizeof(pgc.name));

			d->Packet(&pgc, sizeof(TPacketGCChangeName));
			break;
		}
}

//#define ENABLE_GOHOME_IF_MAP_NOT_EXIST
// Typed items (item system F4, IC-034): claim a new session and load the
// character's typed state on the item store thread. A provisioned character
// exclusively uses typed equipment. Every character must load native state before entering the world.
static void LoadTypedItems(entt::entity character)
{
	namespace items = auriga::item::runtime;
	static uint64_t s_lastToken = 0;
	const uint64_t token = ++s_lastToken;
	const uint32_t pid = ecs::PlayerRuntime::GetPlayerID(character);
	g_registry.emplace_or_replace<ecs::TypedItemLogin>(character, token);
	if (!items::ItemStore::Instance().Running())
	{
		g_registry.get<ecs::TypedItemLogin>(character).mode = ecs::TypedItemLogin::Mode::Unavailable;
        if (auto* desc = ecs::PlayerRuntime::GetDesc(character)) desc->SetPhase(PHASE_CLOSE);
		return;
	}
    g_registry.emplace_or_replace<ecs::TypedEquipmentProjection>(character); // no combat until owner mode is known
	const bool queued = items::RequestPlayerLoginLoad(pid, [pid, token](items::LoginLoad result) {
		const entt::entity entity = CHARACTER_MANAGER::instance().FindEntityByPID(pid);
		auto* login = entity != entt::null && g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItemLogin>(entity) : nullptr;
		if (!login || login->token != token)
			return; // logged out, or a newer login owns the session now
		if (result.loaded)
		{
			login->mode = ecs::TypedItemLogin::Mode::Native;
			LOG_INFO("TypedItems: pid {} session {}: {} items", pid, result.session, result.loaded->items.size());
			g_registry.emplace_or_replace<ecs::TypedItems>(entity, std::move(*result.loaded));
            SkillSystem::InstallSkillPalette(entity, result.palette);
            if(!ApplyTypedBookLogin(entity)){if(auto* desc=ecs::PlayerRuntime::GetDesc(entity))desc->SetPhase(PHASE_CLOSE);return;}
            StartTypedAccessoryClock(entity);StartTypedRecoveryClock(entity);
			PlayerItemActions::SetItemLoaded(entity);
			SendTypedInventory(entity); // only once the client is in game
		}
        else
        {
            login->mode = ecs::TypedItemLogin::Mode::Unavailable;
            LOG_ERROR("TypedItems: pid {} load failed (error {})", pid, static_cast<int>(result.loaded.error()));
            if (auto* desc = ecs::PlayerRuntime::GetDesc(entity)) desc->SetPhase(PHASE_CLOSE);
        }
	});
	if (!queued)
    {
        LOG_ERROR("TypedItems: pid {} load could not be queued", pid);
        if (auto* desc = ecs::PlayerRuntime::GetDesc(character)) desc->SetPhase(PHASE_CLOSE);
    }
}

void CInputDB::PlayerLoad(LPDESC d, const char * data)
{
	TPlayerTable * pTab = (TPlayerTable *) data;

	if (!d)
		return;

	int32_t lMapIndex = pTab->lMapIndex;
	PIXEL_POSITION pos;

	if (lMapIndex == 0)
	{
		lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(pTab->x, pTab->y);

		if (lMapIndex == 0)
		{
			lMapIndex = EMPIRE_START_MAP(d->GetAccountTable().bEmpire);
			pos.x = EMPIRE_START_X(d->GetAccountTable().bEmpire);
			pos.y = EMPIRE_START_Y(d->GetAccountTable().bEmpire);
		}
		else
		{
			pos.x = pTab->x;
			pos.y = pTab->y;
		}
	}
	pTab->lMapIndex = lMapIndex;

	// ----
	// by rtsummit
	if (!SECTREE_MANAGER::instance().GetValidLocation(pTab->lMapIndex, pTab->x, pTab->y, lMapIndex, pos, d->GetEmpire()))
	{
		LOG_ERROR("InputDB::PlayerLoad : cannot find valid location {} x {} (name: {})", pTab->x, pTab->y, pTab->name);
#ifdef ENABLE_GOHOME_IF_MAP_NOT_EXIST
		lMapIndex = EMPIRE_START_MAP(d->GetAccountTable().bEmpire);
		pos.x = EMPIRE_START_X(d->GetAccountTable().bEmpire);
		pos.y = EMPIRE_START_Y(d->GetAccountTable().bEmpire);
#else
		d->SetPhase(PHASE_CLOSE);
		return;
#endif
	}

	pTab->x = pos.x;
	pTab->y = pos.y;
	pTab->lMapIndex = lMapIndex;

	if (ecs::IsCharacter(d->GetEntity()) || d->IsPhase(PHASE_GAME))
	{
		LOG_ERROR("login state already has main state (character {} {})", ecs::PlayerRuntime::GetName(d->GetEntity()), entt::to_integral(d->GetEntity()));
		return;
	}

	if (ecs::IsCharacter(CHARACTER_MANAGER::Instance().FindPCEntity(pTab->name)))
	{
		LOG_ERROR("InputDB: PlayerLoad : {} already exist in game", pTab->name);
		return;
	}

	const entt::entity createdEntity = CHARACTER_MANAGER::instance().CreateCharacterEntity(pTab->name, pTab->id);
	if (createdEntity == entt::null || !g_registry.valid(createdEntity)) {
		LOG_ERROR("PlayerLoad: failed to create character for pid={}", pTab->id);
		return;
	}
	// SetPlayerProto computes points: seed the ECS archetype before it runs.
	const entt::entity chEntity = EntityFactory::CreatePC(g_registry, *pTab, d, ecs::PlayerRuntime::GetPacketVID(createdEntity));
	ecs::PlayerRuntime::SetPlayerProto(chEntity, pTab);
	ecs::PlayerRuntime::SetEmpire(chEntity, d->GetEmpire());
	d->BindCharacter(chEntity);
	g_registry.get<ecs::Experience>(chEntity).next = ecs::PlayerRuntime::GetNextExp(chEntity);
	LOG_INFO("ECS: PC entity created VID={} pid={}", ecs::PlayerRuntime::GetPacketVID(chEntity), ecs::PlayerRuntime::GetPlayerID(chEntity));
	LoadTypedItems(chEntity);

	{
		// P2P Login
		TPacketGGLogin p;

		p.bHeader = HEADER_GG_LOGIN;
		strlcpy(p.szName, ecs::PlayerRuntime::GetName(chEntity).data(), sizeof(p.szName));
		p.dwPID = (ecs::PlayerRuntime::GetPlayerID(chEntity));
		p.bEmpire = ecs::PlayerRuntime::GetEmpire(chEntity);
		p.lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(ecs::PlayerRuntime::GetX(chEntity), ecs::PlayerRuntime::GetY(chEntity));
		p.bChannel = g_bChannel;

		P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGLogin));

		char buf[51];

		snprintf(buf, sizeof(buf), "%s %lld %d %d %u",

				inet_ntoa(ecs::PlayerRuntime::GetDesc(chEntity)->GetAddr().sin_addr), ecs::PointSystem::GetGold(chEntity), g_bChannel, ecs::PlayerRuntime::GetMapIndex(chEntity), CombatSystem::GetAlignment(chEntity));
		LogManager::instance().CharLog(chEntity, 0, "LOGIN", buf);

	}

	d->SetPhase(PHASE_LOADING);
	NetworkSyncSystem::MainCharacterPacket(chEntity);

	int32_t lPublicMapIndex = lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex;
	//if (!map_allow_find(lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex) || !CheckEmpire(chEntity, lMapIndex))
	if (!map_allow_find(lPublicMapIndex))
	{
		LOG_ERROR("InputDB::PlayerLoad : entering {} map is not allowed here (name: {}, empire {})", lMapIndex, pTab->name, d->GetEmpire());

		ecs::MovementSystem::SetWarpLocation(chEntity, EMPIRE_START_MAP(d->GetEmpire()),
				EMPIRE_START_X(d->GetEmpire()) / 100,
				EMPIRE_START_Y(d->GetEmpire()) / 100);

		d->SetPhase(PHASE_CLOSE);
		return;
	}

	quest::CQuestManager::instance().BroadcastEventFlagOnLogin(chEntity);

	PlayerItemActions::SendQuickslots(chEntity);

	NetworkSyncSystem::PointsPacket(chEntity);
	SkillSystem::SendSkillLevelPacket(chEntity);

	LOG_INFO("InputDB: player_load {} {}x{}x{} LEVEL {} MOV_SPEED {} JOB {} ATG {} DFG {} GMLv {}", pTab->name, ecs::PlayerRuntime::GetX(chEntity), ecs::PlayerRuntime::GetY(chEntity), ecs::PlayerRuntime::GetZ(chEntity), (ecs::PointSystem::GetLevel(chEntity)), ecs::PointSystem::Get(chEntity, POINT_MOV_SPEED), ecs::PlayerRuntime::GetJob(chEntity), ecs::PointSystem::Get(chEntity, POINT_ATT_GRADE), ecs::PointSystem::Get(chEntity, POINT_DEF_GRADE), ecs::PlayerRuntime::GetGMLevel(chEntity));

}

void CInputDB::Boot(const char* data)
{
	signal_timer_disable();

	uint32_t dwPacketSize = decode_4bytes(data);
	data += 4;

	uint8_t bVersion = decode_byte(data);
	data += 1;

	LOG_INFO("BOOT: PACKET: {}", dwPacketSize);
	LOG_INFO("BOOT: VERSION: {}", bVersion);
	if (bVersion != DATABASE_BOOT_VERSION)
	{
		LOG_ERROR("boot version error");
		thecore_shutdown();
        return;
	}

	LOG_INFO("sizeof(TMobTable) = {}", sizeof(TMobTable));
	LOG_INFO("sizeof(TSkillTable) = {}", sizeof(TSkillTable));
	LOG_INFO("sizeof(TBanwordTable) = {}", sizeof(TBanwordTable));
	LOG_INFO("sizeof(TLand) = {}", sizeof(building::TLand));
	LOG_INFO("sizeof(TObjectProto) = {}", sizeof(building::TObjectProto));
	LOG_INFO("sizeof(TObject) = {}", sizeof(building::TObject));
	//ADMIN_MANAGER
	LOG_INFO("sizeof(TAdminManager) = {}", sizeof (TAdminInfo));
	//END_ADMIN_MANAGER

	uint16_t size;

	/*
	 * MOB
	 */

	if (decode_2bytes(data)!=sizeof(TMobTable))
	{
		LOG_ERROR("mob table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	LOG_INFO("BOOT: MOB: {}", size);

	if (size)
	{
		CMobManager::instance().Initialize((TMobTable *) data, size);
		data += size * sizeof(TMobTable);
	}

	/*
	 * SKILL
	 */

	if (decode_2bytes(data) != sizeof(TSkillTable))
	{
		LOG_ERROR("skill table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	LOG_INFO("BOOT: SKILL: {}", size);

	if (size)
	{
		if (!CSkillManager::instance().Initialize((TSkillTable *) data, size))
		{
			LOG_ERROR("cannot initialize skill table");
			thecore_shutdown();
			return;
		}

		data += size * sizeof(TSkillTable);
	}


	/*
	 * BANWORDS
	 */

	if (decode_2bytes(data) != sizeof(TBanwordTable))
	{
		LOG_ERROR("ban word table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;

	CBanwordManager::instance().Initialize((TBanwordTable *) data, size);
	data += size * sizeof(TBanwordTable);

	{
		using namespace building;

		/*
		 * LANDS
		 */

		if (decode_2bytes(data) != sizeof(TLand))
		{
			LOG_ERROR("land table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		TLand * kLand = (TLand *) data;
		data += size * sizeof(TLand);

		for (uint16_t i = 0; i < size; ++i, ++kLand)
			CManager::instance().LoadLand(kLand);

		/*
		 * OBJECT PROTO
		 */

		if (decode_2bytes(data) != sizeof(TObjectProto))
		{
			LOG_ERROR("object proto table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		CManager::instance().LoadObjectProto((TObjectProto *) data, size);
		data += size * sizeof(TObjectProto);

		/*
		 * OBJECT
		 */
		if (decode_2bytes(data) != sizeof(TObject))
		{
			LOG_ERROR("object table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		TObject * kObj = (TObject *) data;
		data += size * sizeof(TObject);

		for (uint16_t i = 0; i < size; ++i, ++kObj)
			CManager::instance().LoadObject(kObj, true);
	}

	// Wire format: explicit int64 (a time_t merete platformfuggo).
	int64_t serverTime = 0;
	std::memcpy(&serverTime, data, sizeof(serverTime));
	set_global_time(static_cast<time_t>(serverTime));
	data += sizeof(serverTime);

	//ADMIN_MANAGER
	int ChunkSize = decode_2bytes(data );
	data += 2;
	int HostSize = decode_2bytes(data );
	data += 2;
	LOG_INFO("GM Value Count {} {}", HostSize, ChunkSize);
	for (int n = 0; n < HostSize; ++n )
	{
		gm_new_host_inert(data );
		LOG_INFO("GM HOST : IP[{}] ", data);
		data += ChunkSize;
	}


	data += 2;
	int adminsize = decode_2bytes(data );
	data += 2;

	for (int n = 0; n < adminsize; ++n )
	{
		tAdminInfo& rAdminInfo = *(tAdminInfo*)data;

		gm_new_insert(rAdminInfo );

		data += sizeof(rAdminInfo );
	}

	//END_ADMIN_MANAGER

	uint16_t endCheck=decode_2bytes(data);
	if (endCheck != 0xffff)
	{
		LOG_ERROR("boot packet end check error [{:x}]!=0xffff", endCheck);
		thecore_shutdown();
		return;
	}
	else
		LOG_INFO("boot packet end check ok [{:x}]==0xffff", endCheck);
	data +=2;

    const std::string szMapIndexFileName = LocaleService_GetMapPath() + "/index.json";
	LOG_INFO("LoadLocaleFile: MapIndex: {}", szMapIndexFileName);
	if (!SECTREE_MANAGER::instance().Build(szMapIndexFileName.c_str(), LocaleService_GetMapPath().c_str()))
	{
		LOG_ERROR("cannot load MapIndex: {}", szMapIndexFileName);
		thecore_shutdown();
		return;
	}

	// END_OF_LOCALE_SERVICE


	building::CManager::instance().FinalizeBoot();

	CMotionManager::instance().Build();

	signal_timer_enable(30);

	if (test_server)
	{
		CMobManager::instance().DumpRegenCount("mob_count");
	}

}



//
//


void CInputDB::LoginAlready(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	{
		TPacketDGLoginAlready * p = (TPacketDGLoginAlready *) c_pData;

		LPDESC d2 = DESC_MANAGER::instance().FindByLoginName(p->szLogin);

		if (d2)
			d2->DisconnectOfSameLogin();
		else
		{
			TPacketGGDisconnect pgg;

			pgg.bHeader = HEADER_GG_DISCONNECT;
			strlcpy(pgg.szLogin, p->szLogin, sizeof(pgg.szLogin));

			P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGDisconnect));
		}
	}
	// END_OF_INTERNATIONAL_VERSION

	LoginFailure(d, "ALREADY");
}

void CInputDB::EmpireSelect(LPDESC d, const char * c_pData)
{
	LOG_INFO("EmpireSelect {}", static_cast<const void*>(d));

	if (!d)
		return;

	TAccountTable & rTable = d->GetAccountTable();
	rTable.bEmpire = *(uint8_t *)c_pData;

	TPacketGCEmpire pe;
	pe.bHeader = HEADER_GC_EMPIRE;
	pe.bEmpire = rTable.bEmpire;
	d->Packet(&pe, sizeof(pe));

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		if (rTable.players[i].dwID)
		{
			rTable.players[i].x = EMPIRE_START_X(rTable.bEmpire);
			rTable.players[i].y = EMPIRE_START_Y(rTable.bEmpire);
		}

	GetServerLocation(d->GetAccountTable(), rTable.bEmpire);

	d->SendLoginSuccessPacket();
}

void CInputDB::MapLocations(const char * c_pData)
{
	uint8_t bCount = *(uint8_t *) (c_pData++);

	LOG_TRACE("InputDB::MapLocations {}", bCount);

	TMapLocation * pLoc = (TMapLocation *) c_pData;

	while (bCount--)
	{
		for (int i = 0; i < MAP_ALLOW_LIMIT; ++i)
		{
			if (0 == pLoc->alMaps[i])
				break;

			CMapLocation::instance().Insert(pLoc->alMaps[i], pLoc->szHost, pLoc->wPort
#ifdef ENABLE_GENERAL_CH
			                                , pLoc->bChannel
#endif
			);
		}

		pLoc++;
	}
}

void CInputDB::P2P(const char * c_pData)
{
	TPacketDGP2P * p = (TPacketDGP2P *) c_pData;

	P2P_MANAGER& mgr = P2P_MANAGER::instance();

	if (false == DESC_MANAGER::instance().IsP2PDescExist(p->szHost, p->wPort))
	{
		LOG_INFO("InputDB:P2P {}:{}", p->szHost, p->wPort);
	    LPCLIENT_DESC pkDesc = DESC_MANAGER::instance().CreateConnectionDesc(
		    p->szHost, p->wPort, PHASE_P2P, false);
		mgr.RegisterConnector(pkDesc);
		pkDesc->SetP2P(p->szHost, p->wPort, p->bChannel);
	}
}


void CInputDB::Time(const char * c_pData)
{
	// Wire format: explicit int64 (a time_t merete platformfuggo).
	int64_t serverTime = 0;
	std::memcpy(&serverTime, c_pData, sizeof(serverTime));
	set_global_time(static_cast<time_t>(serverTime));
}

void CInputDB::ReloadProto(const char * c_pData)
{
	uint16_t wSize;

	/*
	 * Skill
	 */
	wSize = decode_2bytes(c_pData);
	c_pData += sizeof(uint16_t);
	if (wSize) CSkillManager::instance().Initialize((TSkillTable *) c_pData, wSize);
	c_pData += sizeof(TSkillTable) * wSize;

	/*
	 * Banwords
	 */

	wSize = decode_2bytes(c_pData);
	c_pData += sizeof(uint16_t);
	CBanwordManager::instance().Initialize((TBanwordTable *) c_pData, wSize);
	c_pData += sizeof(TBanwordTable) * wSize;

	/*
	 * MONSTER
	 */
	wSize = decode_2bytes(c_pData);
	c_pData += 2;
	LOG_INFO("RELOAD: MOB: {}", wSize);

	if (wSize)
	{
		CMobManager::instance().Initialize((TMobTable *) c_pData, wSize);
		c_pData += wSize * sizeof(TMobTable);
	}

	CMotionManager::instance().Build();

	CHARACTER_MANAGER::instance().for_each_pc([](entt::entity ch) { ecs::PointSystem::Compute(ch); });
}


void CInputDB::AuthLogin(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	uint8_t bResult = *(uint8_t *) c_pData;

	TPacketGCAuthSuccess ptoc;

	ptoc.bHeader = HEADER_GC_AUTH_SUCCESS;

	if (bResult)
	{
		ptoc.dwLoginKey = d->GetLoginKey();
	}
	else
	{
		ptoc.dwLoginKey = 0;
	}

	ptoc.bResult = bResult;

	d->Packet(&ptoc, sizeof(TPacketGCAuthSuccess));

	LOG_INFO("AuthLogin result {} key {}", bResult, d->GetLoginKey());
}

void CInputDB::ChangeEmpirePriv(const char* c_pData)
{
	TPacketDGChangeEmpirePriv* p = (TPacketDGChangeEmpirePriv*) c_pData;

	// ADD_EMPIRE_PRIV_TIME
	CPrivManager::instance().GiveEmpirePriv(p->empire, p->type, p->value, p->bLog, p->end_time_sec);
	// END_OF_ADD_EMPIRE_PRIV_TIME
}


void CInputDB::ChangeCharacterPriv(const char* c_pData)
{
	TPacketDGChangeCharacterPriv* p = (TPacketDGChangeCharacterPriv*) c_pData;
	CPrivManager::instance().GiveCharacterPriv(p->pid, p->type, p->value, p->bLog);
}

void CInputDB::MoneyLog(const char* c_pData)
{
	TPacketMoneyLog * p = (TPacketMoneyLog *) c_pData;

	if (p->type == 4) // QUEST_MONEY_LOG_SKIP
		return;

		LogManager::instance().MoneyLog(p->type, p->vnum, p->gold);
}


void CInputDB::SetEventFlag(const char* c_pData)
{
	TPacketSetEventFlag* p = (TPacketSetEventFlag*) c_pData;
	if (!packet_string::IsTerminated(p->szFlagName) || !p->szFlagName[0])
		return;
	quest::CQuestManager::instance().SetEventFlag(p->szFlagName, p->lValue);
}

void CInputDB::CreateObject(const char * c_pData)
{
	using namespace building;
	CManager::instance().LoadObject((TObject *) c_pData);
}

void CInputDB::DeleteObject(const char * c_pData)
{
	using namespace building;
	CManager::instance().DeleteObject(*(uint32_t *) c_pData);
}

void CInputDB::UpdateLand(const char * c_pData)
{
	using namespace building;
	CManager::instance().UpdateLand((TLand *) c_pData);
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void CInputDB::Notice(const char * c_pData)
{
	char szBuf[256+1];
	strlcpy(szBuf, c_pData, sizeof(szBuf));

	SendNotice(szBuf);
}


// MYSHOP_PRICE_LIST
// END_OF_MYSHOP_PRICE_LIST


//RELOAD_ADMIN
void CInputDB::ReloadAdmin(const char * c_pData )
{
	gm_new_clear();
	int ChunkSize = decode_2bytes(c_pData );
	c_pData += 2;
	int HostSize = decode_2bytes(c_pData );
	c_pData += 2;

	for (int n = 0; n < HostSize; ++n )
	{
		gm_new_host_inert(c_pData );
		c_pData += ChunkSize;
	}


	c_pData += 2;
	int size = 	decode_2bytes(c_pData );
	c_pData += 2;

	for (int n = 0; n < size; ++n )
	{
		tAdminInfo& rAdminInfo = *(tAdminInfo*)c_pData;

		gm_new_insert(rAdminInfo );

		c_pData += sizeof (tAdminInfo );

		const entt::entity pChar = CHARACTER_MANAGER::instance().FindPCEntity(rAdminInfo.m_szName );
		if (pChar != entt::null)
		{
			ecs::PlayerRuntime::RefreshGMLevel(pChar);
		}
	}

}
//END_RELOAD_ADMIN




////////////////////////////////////////////////////////////////////
// Analyze
////////////////////////////////////////////////////////////////////
int CInputDB::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	switch (bHeader)
	{
	case HEADER_DG_BOOT:
		Boot(c_pData);
		break;

	case HEADER_DG_PING:
		if (db_clientdesc)
			db_clientdesc->DBPacket(HEADER_GD_PONG, 0, nullptr, 0);
		break;

	case HEADER_DG_PONG:
		break;

	case HEADER_DG_LOGIN_SUCCESS:
		LoginSuccess(m_dwHandle, c_pData);
		break;

	case HEADER_DG_LOGIN_NOT_EXIST:
		LoginFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), "NOID");
		break;

	case HEADER_DG_LOGIN_WRONG_PASSWD:
		LoginFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), "WRONGPWD");
		break;

	case HEADER_DG_LOGIN_ALREADY:
		LoginAlready(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_LOAD_SUCCESS:
		PlayerLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_CREATE_SUCCESS:
		PlayerCreateSuccess(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_CREATE_FAILED:
		PlayerCreateFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), 0);
		break;

	case HEADER_DG_PLAYER_CREATE_ALREADY:
		PlayerCreateFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), 1);
		break;

	case HEADER_DG_PLAYER_DELETE_SUCCESS:
		PlayerDeleteSuccess(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_LOAD_FAILED:
		//LOG_INFO("PLAYER_LOAD_FAILED");
		break;

	case HEADER_DG_PLAYER_DELETE_FAILED:
		//LOG_INFO("PLAYER_DELETE_FAILED");
		PlayerDeleteFail(DESC_MANAGER::instance().FindByHandle(m_dwHandle));
		break;

	case HEADER_DG_ITEM_LOAD:
        LOG_ERROR("Retired Database item-load packet refused");
        if(auto* d=DESC_MANAGER::instance().FindByHandle(m_dwHandle))d->SetPhase(PHASE_CLOSE);
        break;

	case HEADER_DG_QUEST_LOAD:
		QuestLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_AFFECT_LOAD:
		AffectLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;


	case HEADER_DG_EMPIRE_SELECT:
		EmpireSelect(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_MAP_LOCATIONS:
		MapLocations(c_pData);
		break;

	case HEADER_DG_P2P:
		P2P(c_pData);
		break;

	case HEADER_DG_GUILD_SKILL_UPDATE:
		GuildSkillUpdate(c_pData);
		break;

	case HEADER_DG_GUILD_LOAD:
		GuildLoad(c_pData);
		break;

	case HEADER_DG_GUILD_SKILL_RECHARGE:
		GuildSkillRecharge();
		break;

	case HEADER_DG_GUILD_EXP_UPDATE:
		GuildExpUpdate(c_pData);
		break;

	case HEADER_DG_PARTY_CREATE:
		PartyCreate(c_pData);
		break;

	case HEADER_DG_PARTY_DELETE:
		PartyDelete(c_pData);
		break;

	case HEADER_DG_PARTY_ADD:
		PartyAdd(c_pData);
		break;

	case HEADER_DG_PARTY_REMOVE:
		PartyRemove(c_pData);
		break;

	case HEADER_DG_PARTY_STATE_CHANGE:
		PartyStateChange(c_pData);
		break;

	case HEADER_DG_PARTY_SET_MEMBER_LEVEL:
		PartySetMemberLevel(c_pData);
		break;

	case HEADER_DG_TIME:
		Time(c_pData);
		break;

	case HEADER_DG_GUILD_ADD_MEMBER:
		GuildAddMember(c_pData);
		break;

	case HEADER_DG_GUILD_REMOVE_MEMBER:
		GuildRemoveMember(c_pData);
		break;

	case HEADER_DG_GUILD_CHANGE_GRADE:
		GuildChangeGrade(c_pData);
		break;

	case HEADER_DG_GUILD_CHANGE_MEMBER_DATA:
		GuildChangeMemberData(c_pData);
		break;

	case HEADER_DG_GUILD_DISBAND:
		GuildDisband(c_pData);
		break;

	case HEADER_DG_RELOAD_PROTO:
		ReloadProto(c_pData);
		break;

	case HEADER_DG_GUILD_WAR:
		GuildWar(c_pData);
		break;

	case HEADER_DG_GUILD_WAR_SCORE:
		GuildWarScore(c_pData);
		break;

#ifdef ADVANCED_GUILD_INFO
	case HEADER_DG_GUILD_WAR_RESET:
		GuildResetStats(c_pData);
		break;
#endif

	case HEADER_DG_GUILD_LADDER:
		GuildLadder(c_pData);
		break;

	case HEADER_DG_GUILD_SKILL_USABLE_CHANGE:
		GuildSkillUsableChange(c_pData);
		break;

	case HEADER_DG_CHANGE_NAME:
		ChangeName(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_AUTH_LOGIN:
		AuthLogin(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_CHANGE_EMPIRE_PRIV:
		ChangeEmpirePriv(c_pData);
		break;

	case HEADER_DG_CHANGE_GUILD_PRIV:
		ChangeGuildPriv(c_pData);
		break;

	case HEADER_DG_CHANGE_CHARACTER_PRIV:
		ChangeCharacterPriv(c_pData);
		break;

	case HEADER_DG_MONEY_LOG:
		MoneyLog(c_pData);
		break;

	case HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE:
		GuildWithdrawMoney(c_pData);
		break;

	case HEADER_DG_GUILD_MONEY_CHANGE:
		GuildMoneyChange(c_pData);
		break;

	case HEADER_DG_SET_EVENT_FLAG:
		SetEventFlag(c_pData);
		break;

	case HEADER_DG_CREATE_OBJECT:
		CreateObject(c_pData);
		break;

	case HEADER_DG_DELETE_OBJECT:
		DeleteObject(c_pData);
		break;

	case HEADER_DG_UPDATE_LAND:
		UpdateLand(c_pData);
		break;

	case HEADER_DG_NOTICE:
		Notice(c_pData);
		break;

	case HEADER_DG_GUILD_WAR_RESERVE_ADD:
		GuildWarReserveAdd((TGuildWarReserve *) c_pData);
		break;

	case HEADER_DG_GUILD_WAR_RESERVE_DEL:
		GuildWarReserveDelete(*(uint32_t *) c_pData);
		break;

	case HEADER_DG_GUILD_WAR_BET:
		GuildWarBet((TPacketGDGuildWarBet *) c_pData);
		break;

	case HEADER_DG_MARRIAGE_ADD:
		MarriageAdd((TPacketMarriageAdd*) c_pData);
		break;

	case HEADER_DG_MARRIAGE_UPDATE:
		MarriageUpdate((TPacketMarriageUpdate*) c_pData);
		break;

	case HEADER_DG_MARRIAGE_REMOVE:
		MarriageRemove((TPacketMarriageRemove*) c_pData);
		break;

	case HEADER_DG_WEDDING_REQUEST:
		WeddingRequest((TPacketWeddingRequest*) c_pData);
		break;

	case HEADER_DG_WEDDING_READY:
		WeddingReady((TPacketWeddingReady*) c_pData);
		break;

	case HEADER_DG_WEDDING_START:
		WeddingStart((TPacketWeddingStart*) c_pData);
		break;

	case HEADER_DG_WEDDING_END:
		WeddingEnd((TPacketWeddingEnd*) c_pData);
		break;

	// RELOAD_ADMIN
	case HEADER_DG_RELOAD_ADMIN:
		ReloadAdmin(c_pData );
		break;
	//END_RELOAD_ADMIN
#ifdef ENABLE_EVENT_MANAGER
	case HEADER_DG_EVENT_MANAGER:
		EventManager(c_pData);
		break;
#endif
	case HEADER_DG_ACK_CHANGE_GUILD_MASTER :
		this->GuildChangeMaster((TPacketChangeGuildMaster*) c_pData);
		break;
	case HEADER_DG_ACK_SPARE_ITEM_ID_RANGE :

		break;

	case HEADER_DG_UPDATE_HORSE_NAME :
	case HEADER_DG_ACK_HORSE_NAME :
		CHorseNameManager::instance().UpdateHorseName(
				((TPacketUpdateHorseName*)c_pData)->dwPlayerID,
				((TPacketUpdateHorseName*)c_pData)->szHorseName);
		break;

	case HEADER_DG_NEED_LOGIN_LOG:
		DetailLog( (TPacketNeedLoginLogInfo*) c_pData );
		break;
	case HEADER_DG_ITEMAWARD_INFORMER:
		ItemAwardInformer((TPacketItemAwardInfromer*) c_pData);
		break;
#if defined(BL_OFFLINE_MESSAGE)
	case HEADER_DG_RESPOND_OFFLINE_MESSAGES:
		ReadOfflineMessages(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif
	case HEADER_DG_RESPOND_CHANNELSTATUS:
		RespondChannelStatus(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	case HEADER_DG_CHANNEL_RESULT:
		ChangeChannel(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif


#ifdef __SKILL_COLOR_SYSTEM__
	case HEADER_DG_SKILL_COLOR_LOAD:
		SkillColorLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif
#ifdef ENABLE_HWID
	case HEADER_DG_BLOCKHWID:
		{
						break;
		}
		break;
	case HEADER_DG_UNBLOCKHWID:
		{
						break;
		}
#endif
	default:
		return (-1);
	}

	return 0;
}

bool CInputDB::Process(LPDESC d, const void * orig, int bytes, int & r_iBytesProceed)
{
	const char *	c_pData = (const char *) orig;
	uint8_t		bHeader, bLastHeader = 0;
	int			iSize;
	int			iLastPacketLen = 0;

	for (m_iBufferLeft = bytes; m_iBufferLeft > 0;)
	{
		if (m_iBufferLeft < 9)
			return true;

		bHeader		= *((uint8_t *) (c_pData));	// 1
		m_dwHandle	= *((uint32_t *) (c_pData + 1));	// 4
		iSize		= *((uint32_t *) (c_pData + 5));	// 4

		LOG_TRACE("DBCLIENT: header {} handle {} size {} bytes {}", bHeader, m_dwHandle, iSize, bytes);

		if (iSize < 0 || !time64::ValidDatabasePacketSize(bHeader, static_cast<size_t>(iSize)))
		{
			LOG_ERROR("DB packet layout mismatch: header {} size {}", bHeader, iSize);
			d->SetPhase(PHASE_CLOSE);
			return false;
		}

		if (m_iBufferLeft - 9 < iSize)
			return true;

		const char * pRealData = (c_pData + 9);

        int analyzed = 0;
        if (bHeader == HEADER_DG_PLAYER_SESSION_HELLO)
        {
            auto link = auriga::player_session::DecodeHello(
                {reinterpret_cast<const uint8_t*>(pRealData), size_t(iSize)}, true);
            if (d != db_clientdesc || !link || !db_clientdesc->AcceptPlayerSessionHello(*link))
            {
                d->SetPhase(PHASE_CLOSE);
                return false;
            }
            db_clientdesc->NegotiatePlayerCache();
        }
        else if (bHeader == HEADER_DG_PLAYER_CACHE_HELLO)
        {
            auto link = auriga::player_cache_wire::DecodeHello(
                {reinterpret_cast<const uint8_t*>(pRealData), size_t(iSize)}, true);
            if (d != db_clientdesc || !link || !db_clientdesc->AcceptPlayerCacheHello(*link))
            {
                d->SetPhase(PHASE_CLOSE);
                return false;
            }
        }
        else if (bHeader == HEADER_DG_PLAYER_CACHE_RECEIPT)
        {
            auto receipt = auriga::player_cache_wire::DecodeReceipt(
                {reinterpret_cast<const uint8_t*>(pRealData), size_t(iSize)});
            if (d != db_clientdesc || !receipt || receipt->key.link != db_clientdesc->PlayerSessionLink())
            {
                d->SetPhase(PHASE_CLOSE);
                return false;
            }
            db_clientdesc->AcceptPlayerCacheReceipt(m_dwHandle, *receipt);
        }
        else if (bHeader == HEADER_DG_PLAYER_SESSION_REPLY)
        {
            auto reply = auriga::player_session::DecodeReply(
                {reinterpret_cast<const uint8_t*>(pRealData), size_t(iSize)});
            if (d != db_clientdesc || !db_clientdesc->PlayerSessionsReady() || !reply ||
                reply->key.link != db_clientdesc->PlayerSessionLink())
            {
                d->SetPhase(PHASE_CLOSE);
                return false;
            }
            auto *target = DESC_MANAGER::instance().FindByHandle(m_dwHandle);
            bool ownsCharacter = false;
            if (target && !target->IsPhase(PHASE_CLOSE) && target->GetAccountTable().id == reply->key.account)
                for (const auto &player : target->GetAccountTable().players)
                    ownsCharacter |= player.dwID == reply->key.character;
            if (ownsCharacter && reply->header != HEADER_DG_PLAYER_LOAD_SUCCESS &&
                reply->header != HEADER_DG_PLAYER_LOAD_FAILED)
            {
                const auto entity = target->GetEntity();
                ownsCharacter = ecs::IsCharacter(entity) &&
                    ecs::PlayerRuntime::GetPlayerID(entity) == reply->key.character &&
                    target->PlayerSession().MatchesEntity(reply->key, entt::to_integral(entity));
            }
            if (ownsCharacter && target->PlayerSession().Accept(*reply))
            {
                if (reply->header == HEADER_DG_PLAYER_LOAD_FAILED) target->SetPhase(PHASE_CLOSE);
                else
                {
                    analyzed = Analyze(d, reply->header, reinterpret_cast<const char*>(reply->payload.data()));
                    if (reply->header == HEADER_DG_PLAYER_LOAD_SUCCESS)
                    {
                        const auto entity = target->GetEntity();
                        if (!ecs::IsCharacter(entity) || ecs::PlayerRuntime::GetPlayerID(entity) != reply->key.character ||
                            !target->PlayerSession().BindEntity(entt::to_integral(entity)))
                            target->SetPhase(PHASE_CLOSE);
                    }
                }
            }
        }
#ifdef ENABLE_ITEMSHOP
        else if (bHeader == HEADER_DG_ITEMSHOP)
        {
            if (d == db_clientdesc && iSize == 1) ItemShop(pRealData);
        }
#endif
        else
        {
            auto *target = DESC_MANAGER::instance().FindByHandle(m_dwHandle);
            // A legacy/unscoped late reply cannot bypass an already versioned request.
            if (!(auriga::player_session::IsLoadHeader(bHeader) && target && target->PlayerSession().Versioned()))
                analyzed = Analyze(d, bHeader, pRealData);
        }
        if (analyzed < 0)
		{
			LOG_ERROR("in InputDB: UNKNOWN HEADER: {}, LAST HEADER: {}({}), REMAIN BYTES: {}, DESC: {}", bHeader, bLastHeader, iLastPacketLen, m_iBufferLeft, d->GetSocket());

			//printdata((uint8_t*) orig, bytes);
			//d->SetPhase(PHASE_CLOSE);
		}

		c_pData		+= 9 + iSize;
		m_iBufferLeft	-= 9 + iSize;
		r_iBytesProceed	+= 9 + iSize;

		iLastPacketLen	= 9 + iSize;
		bLastHeader	= bHeader;
	}

	return true;
}


void CInputDB::DetailLog(const TPacketNeedLoginLogInfo* info)
{
	const entt::entity character = CHARACTER_MANAGER::instance().FindEntityByPID( info->dwPlayerID );

	if (ecs::IsCharacter(character))
	{
		LogManager::instance().DetailLoginLog(true, character);
	}
}


void CInputDB::RespondChannelStatus(LPDESC desc, const char* pcData)
{
	if (!desc) {
		return;
	}
	const int nSize = decode_4bytes(pcData);
	pcData += sizeof(nSize);

	uint8_t bHeader = HEADER_GC_RESPOND_CHANNELSTATUS;
	desc->BufferedPacket(&bHeader, sizeof(uint8_t));
	desc->BufferedPacket(&nSize, sizeof(nSize));
	if (0 < nSize) {
		desc->BufferedPacket(pcData, sizeof(TChannelStatus)*nSize);
	}
	uint8_t bSuccess = 1;
	desc->Packet(&bSuccess, sizeof(bSuccess));
	desc->SetChannelStatusRequested(false);
}

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
void CInputDB::ChangeChannel(LPDESC d, const char* pcData)
{
	if (!d || !ecs::IsCharacter(d->GetEntity()))
	{
		LOG_ERROR("Change channel request with empty or invalid description handle!");
		return;
	}

	TPacketReturnChannel* p = (TPacketReturnChannel*)pcData;
	if (!p->lAddr || !p->port) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(d->GetEntity(), CHAT_TYPE_INFO, 636, "");
#endif
		return;
	}

	// Execute
	ecs::MovementSystem::StartChannelSwitch(d->GetEntity(), p->lAddr, p->port);
}
#endif

#if defined(BL_OFFLINE_MESSAGE)
#include "buffer_manager.h"
void CInputDB::ReadOfflineMessages(LPDESC desc, const char* pcData)
{
	if (!desc || !ecs::IsCharacter(desc->GetEntity()))
		return;

	if (ecs::PlayerRuntime::IsBlockMode(desc->GetEntity(), BLOCK_WHISPER))
		return;

	auto p = reinterpret_cast<const TPacketDGReadOfflineMessage*>(pcData);

	TPacketGCWhisper pack;
	int len = MIN(CHAT_MAX_LEN, strlen(p->szMessage) + 1);
	pack.bHeader = HEADER_GC_WHISPER;
	pack.wSize = static_cast<uint16_t>(sizeof(TPacketGCWhisper) + len);
	pack.bType = WHISPER_TYPE_OFFLINE;
	strlcpy(pack.szNameFrom, p->szFrom, sizeof(pack.szNameFrom));

	TEMP_BUFFER buf;
	buf.write(&pack, sizeof(TPacketGCWhisper));
	buf.write(p->szMessage, len);
	desc->Packet(buf.read_peek(), buf.size());
}
#endif
#ifdef ENABLE_EVENT_MANAGER
void CInputDB::EventManager(const char* c_pData)
{
	CHARACTER_MANAGER& chrMngr = CHARACTER_MANAGER::Instance();
	const uint8_t subIndex = *(uint8_t*)c_pData;
	c_pData += sizeof(uint8_t);
	if (subIndex == EVENT_MANAGER_LOAD)
	{
		chrMngr.ClearEventData();
		const uint8_t dayCount = *(uint8_t*)c_pData;
		c_pData += sizeof(uint8_t);

		const bool updateFromGameMaster = *(bool*)c_pData;
		c_pData += sizeof(bool);

		for (uint32_t x = 0; x < dayCount; ++x)
		{
			const uint8_t dayIndex = *(uint8_t*)c_pData;
			c_pData += sizeof(uint8_t);

			const uint8_t dayEventCount = *(uint8_t*)c_pData;
			c_pData += sizeof(uint8_t);

			if (dayEventCount > 0)
			{
				std::vector<TEventManagerData> m_vec;
				m_vec.resize(dayEventCount);
				memcpy(&m_vec[0], c_pData, dayEventCount*sizeof(TEventManagerData));
				c_pData += dayEventCount * sizeof(TEventManagerData);
				chrMngr.SetEventData(dayIndex, m_vec);

			}
		}
		if (updateFromGameMaster)
			chrMngr.UpdateAllPlayerEventData();
	}
	else if (EVENT_MANAGER_EVENT_STATUS == subIndex)
	{
		const uint16_t& eventID = *(uint16_t*)c_pData;
		c_pData += sizeof(uint16_t);
		const bool& eventStatus = *(bool*)c_pData;
		c_pData += sizeof(bool);
		const int& endTime = *(int*)c_pData;
		c_pData += sizeof(int);
		char endTimeText[25];
		strlcpy(endTimeText, c_pData, sizeof(endTimeText));
		c_pData += sizeof(endTimeText);
		chrMngr.SetEventStatus(eventID, eventStatus, endTime, endTimeText);
	}
}
#endif
