#include "stdafx.h"
#include "ecs/systems/ViewSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <Core/Logging.hpp>
#include "constants.h"
#include "sectree_manager.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "config.h"
#include "packet.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "guild.h"
#include "guild_manager.h"
#include "desc.h"
#include "desc_manager.h"
#include "desc_client.h"
#include "questmanager.h"
#include "building.h"
#include "ecs/CBuildingRegistry.hpp"
#include "ecs/Registry.hpp"
#include "ecs/services/EntityNetworkDispatch.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/components/identity_components.hpp"
#include "ecs/components/spatial_components.hpp"
#include "ecs/components/transform_components.hpp"
#include "ecs/components/visibility_components.hpp"
#include <exception>

enum
{
	// ADD_SUPPLY_BUILDING
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL = 14061,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM = 14062,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE = 14063,
	// END_OF_ADD_SUPPLY_BUILDING

	FLAG_VNUM = 14200,
	WALL_DOOR_VNUM	= 14201,
	WALL_BACK_VNUM	= 14202,
	WALL_LEFT_VNUM	= 14203,
	WALL_RIGHT_VNUM	= 14204,
};

using namespace building;

namespace building::ObjectSystem
{
namespace
{
const ecs::BuildingState* State(entt::entity object)
{
    return g_registry.valid(object) ? g_registry.try_get<ecs::BuildingState>(object) : nullptr;
}

void SetGuildBonus(uint32_t guildID, uint32_t landID, int bonus)
{
    auto* guild = CGuildManager::instance().FindGuild(guildID);
    if (!guild)
        return;
    guild->SetMemberCountBonus(bonus);
    auto* land = CManager::instance().FindLand(landID);
    if (land && map_allow_find(land->GetData().lMapIndex))
        guild->BroadcastMemberCountBonus();
}

int MemberBonus(uint32_t vnum)
{
    switch (vnum) {
    case BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL: return 6;
    case BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM: return 12;
    case BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE: return 18;
    default: return 0;
    }
}

void RemoveAttributes(entt::entity object)
{
    const auto* state = State(object);
    const auto* position = g_registry.valid(object) ? g_registry.try_get<ecs::Position>(object) : nullptr;
    const auto* map = g_registry.valid(object) ? g_registry.try_get<ecs::MapIndex>(object) : nullptr;
    if (!state || !state->attributesApplied || !position || !map)
        return;
    const auto* prototype = CManager::instance().GetObjectProto(state->vnum);
    const auto snapshot = *state;
    const auto location = *position;
    const int32_t mapIndex = map->value;
    g_registry.get<ecs::BuildingState>(object).attributesApplied = false;
    if (prototype)
        SECTREE_MANAGER::instance().ForAttrRegion(mapIndex,
            location.x + prototype->lRegion[0], location.y + prototype->lRegion[1],
            location.x + prototype->lRegion[2], location.y + prototype->lRegion[3],
            static_cast<int32_t>(snapshot.rotationZ), ATTR_OBJECT, ATTR_REGION_MODE_REMOVE);
}
}

bool IsValid(entt::entity object)
{
    const auto* state = State(object);
    return state && !state->destroying && !g_registry.all_of<ecs::SpatialRetiring>(object);
}

uint32_t GetID(entt::entity object)
{
    const auto* state = State(object);
    return state ? state->objectId : 0;
}

uint32_t GetVID(entt::entity object)
{
    const auto* vid = g_registry.valid(object) ? g_registry.try_get<ecs::VIDComponent>(object) : nullptr;
    return vid ? vid->value : 0;
}

uint32_t GetVnum(entt::entity object)
{
    const auto* state = State(object);
    return state ? state->vnum : 0;
}

uint32_t GetGroup(entt::entity object)
{
    const auto* proto = CManager::instance().GetObjectProto(GetVnum(object));
    return proto ? proto->dwGroupVnum : 0;
}

CLand* GetLand(entt::entity object)
{
    const auto* state = State(object);
    return state ? CManager::instance().FindLand(state->landId) : nullptr;
}

entt::entity GetNPCEntity(entt::entity object)
{
    const auto* state = State(object);
    return state && ecs::IsCharacter(state->npc) ? state->npc : entt::null;
}

entt::entity Create(const TObject& data, uint32_t vid)
{
    if (data.dwID == 0 || vid == 0 || ecs::CBuildingRegistry::FindByID(data.dwID) != entt::null ||
        ecs::CBuildingRegistry::FindByVID(vid) != entt::null)
        return entt::null;
    const entt::entity object = g_registry.create();
    const auto rollback = [&] {
        ecs::CBuildingRegistry::Unregister(data.dwID, object);
        if (g_registry.valid(object))
            g_registry.destroy(object);
    };
    try {
        const auto prepare = [&]<class T>() {
            if (!g_registry.valid(object)) return false;
            g_registry.insert<T>(&object, &object + 1);
            return g_registry.valid(object) && g_registry.all_of<T>(object);
        };
        if (!prepare.template operator()<ecs::BuildingState>() ||
            !prepare.template operator()<ecs::Position>() ||
            !prepare.template operator()<ecs::MapIndex>() ||
            !prepare.template operator()<ecs::VIDComponent>() ||
            !IsValid(object) || !g_registry.all_of<ecs::BuildingState, ecs::Position,
                ecs::MapIndex, ecs::VIDComponent>(object)) {
            rollback();
            return entt::null;
        }
        // All state belongs to this generation from construction through teardown.
        auto& state = g_registry.get<ecs::BuildingState>(object);
        state.vnum = data.dwVnum;
        state.landId = data.dwLandID;
        state.objectId = data.dwID;
        state.life = data.lLife;
        state.rotationX = data.xRot;
        state.rotationY = data.yRot;
        state.rotationZ = data.zRot;
        if (const auto* land = CManager::instance().FindLand(data.dwLandID))
            state.guildId = land->GetOwner();
        g_registry.get<ecs::Position>(object) = {data.x, data.y, 0};
        g_registry.get<ecs::MapIndex>(object).value = data.lMapIndex;
        g_registry.get<ecs::VIDComponent>(object).value = vid;
        if (!ecs::CBuildingRegistry::Register(data.dwID, vid, object)) {
            rollback();
            return entt::null;
        }
        return object;
    } catch (...) {
        rollback();
        throw;
    }
}

void Destroy(entt::entity object)
{
    if (!IsValid(object))
        return;
    const auto state = *State(object);
    g_registry.get<ecs::BuildingState>(object).destroying = true;
    g_registry.get<ecs::BuildingState>(object).effectGuildId = 0;
    // Remove every lookup before DEL/NPC callbacks; a replacement with the same
    // DB ID or VID is not owned by this generation's cleanup.
    CManager::instance().UnregisterObject(object);
    ecs::CBuildingRegistry::Unregister(state.objectId, object);
    std::exception_ptr failure;
    const auto finish = [&](auto&& cleanup) {
        try { cleanup(); }
        catch (...) { if (!failure) failure = std::current_exception(); }
    };
    // Clear external effects before DEL/on_construct callbacks can replace this
    // building. Complete the remaining teardown even if a callback throws.
    finish([&] { RemoveAttributes(object); });
    if (state.effectGuildId)
        finish([&] { SetGuildBonus(state.effectGuildId, state.landId, 0); });
    finish([&] {
        if (g_registry.valid(object) && !g_registry.all_of<ecs::SpatialRetiring>(object))
            g_registry.insert<ecs::SpatialRetiring>(&object, &object + 1);
    });
    finish([&] { ecs::SpatialService::RemoveEntity(g_registry, object); });
    finish([&] {
        if (ecs::IsCharacter(state.npc))
            M2_DESTROY_CHARACTER(state.npc);
    });
    finish([&] { if (g_registry.valid(object)) g_registry.destroy(object); });
    if (failure)
        std::rethrow_exception(failure);
}

bool Show(entt::entity object, int32_t mapIndex, int32_t x, int32_t y)
{
    if (!IsValid(object))
        return false;
    const auto* proto = CManager::instance().GetObjectProto(GetVnum(object));
    if (!proto || !SECTREE_MANAGER::instance().Get(mapIndex, x, y)) {
        LOG_ERROR("cannot find building prototype/sectree by {}x{} mapindex {}", x, y, mapIndex);
        return false;
    }
    const TObjectProto prototype = *proto;
    RemoveAttributes(object);
    ecs::SpatialService::RemoveEntity(g_registry, object);
    if (!IsValid(object) || ecs::SpatialService::GetSectree(g_registry, object))
        return false;
    if (const auto* land = GetLand(object))
        g_registry.get<ecs::BuildingState>(object).guildId = land->GetOwner();
    if (!ecs::SpatialService::InsertEntity(g_registry, object, static_cast<uint32_t>(mapIndex), x, y, 0))
        return false;
    if (!IsValid(object))
        return false;
    const auto* position = g_registry.try_get<ecs::Position>(object);
    const auto* map = g_registry.try_get<ecs::MapIndex>(object);
    if (!position || !map || position->x != x || position->y != y || map->value != mapIndex)
        return false;
    const float rotation = g_registry.get<ecs::BuildingState>(object).rotationZ;
    SECTREE_MANAGER::instance().ForAttrRegion(mapIndex,
        x + prototype.lRegion[0], y + prototype.lRegion[1],
        x + prototype.lRegion[2], y + prototype.lRegion[3],
        static_cast<int32_t>(rotation), ATTR_OBJECT, ATTR_REGION_MODE_SET);
    g_registry.get<ecs::BuildingState>(object).attributesApplied = true;
    ecs::SpatialService::UpdateSectree(g_registry, object);
    return IsValid(object) && ecs::SpatialService::GetSectree(g_registry, object) != nullptr;
}

void Reconstruct(entt::entity object, uint32_t vnum)
{
    if (!IsValid(object))
        return;
    const auto state = *State(object);
    const auto* position = g_registry.try_get<ecs::Position>(object);
    const auto* map = g_registry.try_get<ecs::MapIndex>(object);
    if (!position || !map)
        return;
    const auto location = *position;
    const int32_t mapIndex = map->value;
    const auto* region = SECTREE_MANAGER::instance().GetMapRegion(mapIndex);
    auto* land = CManager::instance().FindLand(state.landId);
    if (!region || !land)
        return;
    const int32_t relativeX = location.x - region->sx;
    const int32_t relativeY = location.y - region->sy;
    land->RequestDeleteObject(state.objectId);
    land->RequestCreateObject(vnum, mapIndex, relativeX, relativeY,
        state.rotationX, state.rotationY, state.rotationZ, false);
}

void ApplySpecialEffect(entt::entity object)
{
    if (!IsValid(object))
        return;
    const auto state = *State(object);
    if (const int bonus = MemberBonus(state.vnum)) {
        const auto* land = GetLand(object);
        const uint32_t guildID = land ? land->GetOwner() : 0;
        if (state.effectGuildId && state.effectGuildId != guildID) {
            g_registry.get<ecs::BuildingState>(object).effectGuildId = 0;
            SetGuildBonus(state.effectGuildId, state.landId, 0);
            if (!IsValid(object)) return;
        }
        g_registry.get<ecs::BuildingState>(object).guildId = guildID;
        g_registry.get<ecs::BuildingState>(object).effectGuildId = guildID;
        SetGuildBonus(guildID, state.landId, bonus);
    }
}

void RegenNPC(entt::entity object)
{
    if (!IsValid(object) || GetNPCEntity(object) != entt::null)
        return;
    const auto* land = GetLand(object);
    if (!land)
        return;
    g_registry.get<ecs::BuildingState>(object).guildId = land->GetOwner();
    const auto state = *State(object);
    const auto* proto = CManager::instance().GetObjectProto(state.vnum);
    const auto* position = g_registry.try_get<ecs::Position>(object);
    const auto* map = g_registry.try_get<ecs::MapIndex>(object);
    if (!proto || !proto->dwNPCVnum || !position || !map)
        return;
    const TObjectProto prototype = *proto;
    const auto location = *position;
    const int32_t mapIndex = map->value;
    if (!CGuildManager::instance().FindGuild(state.guildId))
        return;
    const float rotation = state.rotationZ * 2.0f * M_PI / 360.0f;
    const int npcX = int(prototype.lNPCX * cosf(rotation) + prototype.lNPCY * sinf(rotation));
    const int npcY = int(prototype.lNPCY * cosf(rotation) - prototype.lNPCX * sinf(rotation));
    const entt::entity npc = CHARACTER_MANAGER::instance().SpawnMobEntity(prototype.dwNPCVnum,
        mapIndex, location.x + npcX, location.y + npcY, location.z, false, int(state.rotationZ));
    if (!ecs::IsCharacter(npc)) {
        LOG_ERROR("Cannot create guild npc");
        return;
    }
    if (!IsValid(object) || GetNPCEntity(object) != entt::null) {
        M2_DESTROY_CHARACTER(npc);
        return;
    }
    g_registry.get<ecs::BuildingState>(object).npc = npc;
    auto* guild = CGuildManager::instance().FindGuild(state.guildId);
    if (!guild) {
        g_registry.get<ecs::BuildingState>(object).npc = entt::null;
        M2_DESTROY_CHARACTER(npc);
        return;
    }
    ecs::SocialSystem::SetGuild(npc, guild);
    guild = CGuildManager::instance().FindGuild(state.guildId);
    if (guild && MemberBonus(state.vnum)) {
        if (auto* pc = quest::CQuestManager::instance().GetPC(guild->GetMasterPID()))
            pc->SetFlag("alter_of_power.build_level", guild->GetLevel());
    }
}
}

////////////////////////////////////////////////////////////////////////////////////

CLand::CLand(TLand * pData)
{
	memcpy(&m_data, pData, sizeof(TLand));
}

CLand::~CLand()
{
	try {
		Destroy();
	} catch (const std::exception& error) {
		LOG_ERROR("Land {} teardown callback failed after cleanup: {}", GetID(), error.what());
	} catch (...) {
		LOG_ERROR("Land {} teardown callback failed after cleanup", GetID());
	}
}

void CLand::Destroy()
{
	if (m_destroying)
		return;
	m_destroying = true;
	// Detach land lookups before any teardown callback can revisit them.
	const auto objects = std::move(m_objectsByID);
	m_objectsByID.clear();
	m_objectsByVID.clear();
	std::exception_ptr failure;
	for (const auto& [id, object] : objects) {
		try { ObjectSystem::Destroy(object); }
		catch (...) { if (!failure) failure = std::current_exception(); }
	}
	if (failure)
		std::rethrow_exception(failure);
}

const TLand & CLand::GetData()
{
	return m_data;
}

void CLand::PutData(const TLand * data)
{
	memcpy(&m_data, data, sizeof(TLand));

	if (m_data.dwGuildID)
	{
		const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(m_data.lMapIndex);

		if (r)
		{
			std::vector<entt::entity> i;

			if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(20040, i))
			{
				for (const entt::entity chEntity : i)
				{


					if (ecs::PlayerRuntime::GetMapIndex(chEntity) != m_data.lMapIndex)
						continue;

					int x = ecs::PlayerRuntime::GetX(chEntity) - r->sx;
					int y = ecs::PlayerRuntime::GetY(chEntity) - r->sy;

					if (x > m_data.x + m_data.width || x < m_data.x)
						continue;

					if (y > m_data.y + m_data.height || y < m_data.y)
						continue;

					if (ecs::IsCharacter(chEntity))
						M2_DESTROY_CHARACTER(chEntity);
				}
			}
		}
	}
}

void CLand::InsertObject(entt::entity object)
{
	if (m_destroying || !ObjectSystem::IsValid(object))
		return;
	if (g_registry.get<ecs::BuildingState>(object).landId != GetID())
		return;
	m_objectsByID.emplace(ObjectSystem::GetID(object), object);
	m_objectsByVID.emplace(ObjectSystem::GetVID(object), object);
}

entt::entity CLand::FindObject(uint32_t dwID)
{
	const auto it = m_objectsByID.find(dwID);
	return it != m_objectsByID.end() && ObjectSystem::IsValid(it->second) ? it->second : entt::null;
}

entt::entity CLand::FindObjectByGroup(uint32_t dwGroupVnum)
{
	for (const auto& [id, object] : m_objectsByID)
	{
		if (ObjectSystem::IsValid(object) && ObjectSystem::GetGroup(object) == dwGroupVnum)
			return object;
	}

	return entt::null;
}

entt::entity CLand::FindObjectByVnum(uint32_t dwVnum)
{
	for (const auto& [id, object] : m_objectsByID)
	{
		if (ObjectSystem::IsValid(object) && ObjectSystem::GetVnum(object) == dwVnum)
			return object;
	}

	return entt::null;
}

// BUILDING_NPC
entt::entity CLand::FindObjectByNPC(entt::entity npc)
{
	if (npc == entt::null)
		return entt::null;

	for (const auto& [id, object] : m_objectsByID)
	{
		if (ObjectSystem::IsValid(object) && ObjectSystem::GetNPCEntity(object) == npc)
			return object;
	}

	return entt::null;
}
// END_OF_BUILDING_NPC

entt::entity CLand::FindObjectByVID(uint32_t dwVID)
{
	const auto it = m_objectsByVID.find(dwVID);
	return it != m_objectsByVID.end() && ObjectSystem::IsValid(it->second) ? it->second : entt::null;
}

void CLand::UnregisterObject(entt::entity object)
{
	const auto byID = m_objectsByID.find(ObjectSystem::GetID(object));
	if (byID != m_objectsByID.end() && byID->second == object)
		m_objectsByID.erase(byID);
	const auto byVID = m_objectsByVID.find(ObjectSystem::GetVID(object));
	if (byVID != m_objectsByVID.end() && byVID->second == object)
		m_objectsByVID.erase(byVID);
}

void CLand::DeleteObject(uint32_t dwID)
{
	const entt::entity object = FindObject(dwID);
	if (object == entt::null)
		return;

	LOG_INFO("Land::DeleteObject {}", dwID);
	ObjectSystem::Destroy(object);
}

struct FIsIn
{
	int32_t sx, sy;
	int32_t ex, ey;

	bool bIn;
	FIsIn (int32_t sx_, int32_t sy_, int32_t ex_, int32_t ey_)
		: sx(sx_), sy(sy_), ex(ex_), ey(ey_), bIn(false)
	{}

	void operator () (entt::entity character)
	{
		if (!ecs::IsCharacter(character))
			return;

		if (ecs::PlayerRuntime::GetCharType(character) == CHAR_TYPE_MONSTER)
		{
			return;
		}
		if (sx <= ecs::PlayerRuntime::GetX(character) && ecs::PlayerRuntime::GetX(character) <= ex
			&& sy <= ecs::PlayerRuntime::GetY(character) && ecs::PlayerRuntime::GetY(character) <= ey)
		{
			bIn = true;
		}
	}
};

bool CLand::RequestCreateObject(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, float xRot, float yRot, float zRot, bool checkAnother)
{
	SECTREE_MANAGER& rkSecTreeMgr = SECTREE_MANAGER::instance();
	TObjectProto * pkProto = CManager::instance().GetObjectProto(dwVnum);

	if (!pkProto)
	{
		LOG_ERROR("Invalid Object vnum {}", dwVnum);
		return false;
	}
	const TMapRegion * r = rkSecTreeMgr.GetMapRegion(lMapIndex);
	if (!r)
		return false;

	LOG_INFO("RequestCreateObject(vnum={}, map={}, pos=({},{}), rot=({:.1f},{:.1f},{:.1f}) region({},{} ~ {},{})", dwVnum, lMapIndex, x, y, xRot, yRot, zRot, r->sx, r->sy, r->ex, r->ey);

	x += r->sx;
	y += r->sy;

	int sx = r->sx + m_data.x;
	int ex = sx + m_data.width;
	int sy = r->sy + m_data.y;
	int ey = sy + m_data.height;

	int osx = x + pkProto->lRegion[0];
	int osy = y + pkProto->lRegion[1];
	int oex = x + pkProto->lRegion[2];
	int oey = y + pkProto->lRegion[3];

	float rad = zRot * 2.0f * M_PI / 360.0f;

	int tsx = (int)(pkProto->lRegion[0] * cosf(rad) + pkProto->lRegion[1] * sinf(rad) + x);
	int tsy = (int)(pkProto->lRegion[0] * -sinf(rad) + pkProto->lRegion[1] * cosf(rad) + y);

	int tex = (int)(pkProto->lRegion[2] * cosf(rad) + pkProto->lRegion[3] * sinf(rad) + x);
	int tey = (int)(pkProto->lRegion[2] * -sinf(rad) + pkProto->lRegion[3] * cosf(rad) + y);

	if (tsx < sx || tex > ex || tsy < sy || tey > ey)
	{
		LOG_ERROR("invalid position: object is outside of land region\nLAND: {} {} ~ {} {}\nOBJ: {} {} ~ {} {}", sx, sy, ex, ey, osx, osy, oex, oey);
		return false;
	}

	// ADD_BUILDING_ROTATION
	if ( checkAnother )
	{
		if (rkSecTreeMgr.ForAttrRegion(lMapIndex, osx, osy, oex, oey, (int32_t)zRot, ATTR_OBJECT, ATTR_REGION_MODE_CHECK))
		{
			LOG_ERROR("another object already exist");
			return false;
		}
		FIsIn f (osx, osy, oex, oey);
		rkSecTreeMgr.GetMap(lMapIndex)->for_each(f);

		if (f.bIn)
		{
			LOG_ERROR("another object already exist");
			return false;
		}
	}
	// END_OF_BUILDING_NPC

	TPacketGDCreateObject p;

	p.dwVnum = dwVnum;
	p.dwLandID = m_data.dwID;
	p.lMapIndex = lMapIndex;
	p.x = x;
	p.y = y;
	p.xRot = xRot;
	p.yRot = yRot;
	p.zRot = zRot;

	db_clientdesc->DBPacket(HEADER_GD_CREATE_OBJECT, 0, &p, sizeof(TPacketGDCreateObject));
	return true;
}

void CLand::RequestDeleteObject(uint32_t dwID)
{
	if (FindObject(dwID) == entt::null)
	{
		LOG_ERROR("no object by id {}", dwID);
		return;
	}

	db_clientdesc->DBPacket(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(uint32_t));
	LOG_INFO("RequestDeleteObject id {}", dwID);
}

void CLand::RequestDeleteObjectByVID(uint32_t dwVID)
{
	const entt::entity object = FindObjectByVID(dwVID);
	if (object == entt::null)
	{
		LOG_ERROR("no object by vid {}", dwVID);
		return;
	}

	uint32_t dwID = ObjectSystem::GetID(object);
	db_clientdesc->DBPacket(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(uint32_t));
	LOG_INFO("RequestDeleteObject vid {} id {}", dwVID, dwID);
}

void CLand::SetOwner(uint32_t dwGuild)
{
	if (m_data.dwGuildID != dwGuild)
	{
		m_data.dwGuildID = dwGuild;
		RequestUpdate(dwGuild);
	}
}

void CLand::RequestUpdate(uint32_t dwGuild)
{
	uint32_t a[2];

	a[0] = GetID();
	a[1] = dwGuild;

	db_clientdesc->DBPacket(HEADER_GD_UPDATE_LAND, 0, &a[0], sizeof(uint32_t) * 2);
	LOG_INFO("RequestUpdate id {} guild {}", a[0], a[1]);
}

////////////////////////////////////////////////////////////////////////////////////

CManager::CManager()
{
}

CManager::~CManager()
{
	Destroy();
}

void CManager::Destroy()
{
	auto it = m_map_pkLand.begin();
	for ( ; it != m_map_pkLand.end(); ++it) {
		M2_DELETE(it->second);
	}
	m_map_pkLand.clear();
	m_objectsByID.clear();
	m_objectsByVID.clear();
}

bool CManager::LoadObjectProto(const TObjectProto * pProto, int size) // from DB
{
	m_vec_kObjectProto.resize(size);
	memcpy(m_vec_kObjectProto.data(), pProto, sizeof(TObjectProto) * size);

	for (int i = 0; i < size; ++i)
	{
		TObjectProto & r = m_vec_kObjectProto[i];

		// BUILDING_NPC
		LOG_TRACE("ObjectProto {} price {} upgrade {} upg_limit {} life {} NPC {}", r.dwVnum, r.dwPrice, r.dwUpgradeVnum, r.dwUpgradeLimitTime, r.lLife, r.dwNPCVnum);
		// END_OF_BUILDING_NPC

		for (int j = 0; j < OBJECT_MATERIAL_MAX_NUM; ++j)
		{
			if (!r.kMaterials[j].dwItemVnum)
				break;

			if (nullptr == ITEM_MANAGER::instance().GetTable(r.kMaterials[j].dwItemVnum))
			{
				LOG_ERROR("          mat: ERROR!! no item by vnum {}", r.kMaterials[j].dwItemVnum);
				return false;
			}

			LOG_TRACE("          mat: {} {}", r.kMaterials[j].dwItemVnum, r.kMaterials[j].dwCount);
		}

		m_map_pkObjectProto.insert(std::make_pair(r.dwVnum, &m_vec_kObjectProto[i]));
	}

	return true;
}

TObjectProto * CManager::GetObjectProto(uint32_t dwVnum)
{
	auto it = m_map_pkObjectProto.find(dwVnum);

	if (it == m_map_pkObjectProto.end())
		return nullptr;

	return it->second;
}

bool CManager::LoadLand(TLand * pTable) // from DB
{
	if (!pTable || m_map_pkLand.contains(pTable->dwID))
		return false;
	// MapAllow�� ���� ���� �������� load�� �ؾ��Ѵ�.
	//	�ǹ�(object)�� ��� ��忡 ���� �ִ��� �˱� ���ؼ��� �ǹ��� ������ ���� ��� ��� �Ҽ����� �˾��Ѵ�.
	//	���� ���� load�� ���� ������ ��� �ǹ��� ��� ��忡 �Ҽӵ� ���� ���� ���ؼ�
	//	��� �ǹ��� ���� ��� ������ ���� ���Ѵ�.

	CLand * pkLand = M2_NEW CLand(pTable);
	m_map_pkLand.insert(std::make_pair(pkLand->GetID(), pkLand));

	LOG_TRACE("LAND: {} map {} {}x{} w {} h {}", pTable->dwID, pTable->lMapIndex, pTable->x, pTable->y, pTable->width, pTable->height);

	return true;
}

void CManager::UpdateLand(TLand * pTable)
{
	CLand * pkLand = FindLand(pTable->dwID);

	if (!pkLand)
	{
		LOG_ERROR("cannot find land by id {}", pTable->dwID);
		return;
	}

	pkLand->PutData(pTable);

	const DESC_MANAGER::DESC_SET & cont = DESC_MANAGER::instance().GetClientSet();

	auto it = cont.begin();

	TPacketGCLandList p;

	p.header = HEADER_GC_LAND_LIST;
	p.size = sizeof(TPacketGCLandList) + sizeof(TLandPacketElement);

	TLandPacketElement e;

	e.dwID = pTable->dwID;
	e.x = pTable->x;
	e.y = pTable->y;
	e.width = pTable->width;
	e.height = pTable->height;
	e.dwGuildID = pTable->dwGuildID;

	LOG_INFO("BUILDING: UpdateLand {} pos {}x{} guild {}", e.dwID, e.x, e.y, e.dwGuildID);

	CGuild *guild = CGuildManager::instance().FindGuild(pTable->dwGuildID);
	while (it != cont.end())
	{
		LPDESC d = *(it++);

		if (ecs::IsCharacter(d->GetEntity()) && ecs::PlayerRuntime::GetMapIndex(d->GetEntity()) == pTable->lMapIndex)
		{
			// we must send the guild name first
			ecs::SocialSystem::SendGuildName(d->GetEntity(), guild);

			d->BufferedPacket(&p, sizeof(TPacketGCLandList));
			d->Packet(&e, sizeof(TLandPacketElement));
		}
	}
}

CLand * CManager::FindLand(uint32_t dwID)
{
	std::map<uint32_t, CLand *>::iterator it = m_map_pkLand.find(dwID);

	if (it == m_map_pkLand.end())
		return nullptr;

	return it->second;
}

CLand * CManager::FindLand(int32_t lMapIndex, int32_t x, int32_t y)
{
	LOG_INFO("BUILDING: FindLand {} {} {}", lMapIndex, x, y);

	const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(lMapIndex);

	if (!r)
		return nullptr;

	x -= r->sx;
	y -= r->sy;

	auto it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second;
		const TLand & r = pkLand->GetData();

		if (r.lMapIndex != lMapIndex)
			continue;

		if (x < r.x || y < r.y)
			continue;

		if (x > r.x + r.width || y > r.y + r.height)
			continue;

		return pkLand;
	}

	return nullptr;
}

CLand * CManager::FindLandByGuild(uint32_t GID)
{
	auto it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second;

		if (pkLand->GetData().dwGuildID == GID)
			return pkLand;
	}

	return nullptr;
}

bool CManager::LoadObject(TObject * pTable, bool isBoot) // from DB
{
	if (!pTable || !pTable->dwID || ecs::CBuildingRegistry::FindByID(pTable->dwID) != entt::null)
		return false;
	CLand * pkLand = FindLand(pTable->dwLandID);

	if (!pkLand || pkLand->IsDestroying())
	{
		LOG_INFO("Cannot find land by id {}", pTable->dwLandID);
		return false;
	}

	TObjectProto * pkProto = GetObjectProto(pTable->dwVnum);

	if (!pkProto)
	{
		LOG_ERROR("Cannot find object {} in prototype (id {})", pTable->dwVnum, pTable->dwID);
		return false;
	}

	LOG_TRACE("OBJ: id {} vnum {} map {} pos {}x{}", pTable->dwID, pTable->dwVnum, pTable->lMapIndex, pTable->x, pTable->y);

	const uint32_t vid = CHARACTER_MANAGER::instance().AllocVID();
	const entt::entity object = ObjectSystem::Create(*pTable, vid);
	if (object == entt::null)
		return false;
	// Construction observers may have started land teardown.
	pkLand = FindLand(pTable->dwLandID);
	if (!pkLand || pkLand->IsDestroying()) {
		ObjectSystem::Destroy(object);
		return false;
	}
	m_objectsByVID.emplace(vid, object);
	m_objectsByID.emplace(pTable->dwID, object);
	pkLand->InsertObject(object);

	// BUILDING_NPC
	if (!isBoot)
	{
		if (ObjectSystem::Show(object, pTable->lMapIndex, pTable->x, pTable->y))
			ObjectSystem::RegenNPC(object);
		ObjectSystem::ApplySpecialEffect(object);
	}
	// END_OF_BUILDING_NPC

	return ObjectSystem::IsValid(object);
}

void CManager::FinalizeBoot()
{
	// Visibility publication may run callbacks; do not keep map iterators or
	// component references while showing an entity.
	const auto objects = m_objectsByID;
	for (const auto& [id, object] : objects)
	{
		if (!ObjectSystem::IsValid(object))
			continue;
		const auto* position = g_registry.try_get<ecs::Position>(object);
		const auto* map = g_registry.try_get<ecs::MapIndex>(object);
		if (!position || !map)
			continue;
		const auto location = *position;
		const int32_t mapIndex = map->value;
		if (ObjectSystem::Show(object, mapIndex, location.x, location.y))
			ObjectSystem::RegenNPC(object);
		ObjectSystem::ApplySpecialEffect(object);
	}

	// BUILDING_NPC
	LOG_INFO("FinalizeBoot");
	// END_OF_BUILDING_NPC

	auto it2 = m_map_pkLand.begin();

	while (it2 != m_map_pkLand.end())
	{
		CLand * pkLand = (it2++)->second;

		const TLand & r = pkLand->GetData();

		// LAND_MASTER_LOG
		LOG_TRACE("LandMaster map_index={} pos=({}, {})", r.lMapIndex, r.x, r.y);
		// END_OF_LAND_MASTER_LOG

		if (r.dwGuildID != 0)
			continue;

		if (!map_allow_find(r.lMapIndex))
			continue;

		const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(r.lMapIndex);
		if (!region)
			continue;

		CHARACTER_MANAGER::instance().SpawnMobEntity(20040, r.lMapIndex, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
	}
}

void CManager::DeleteObject(uint32_t dwID) // from DB
{
	LOG_INFO("OBJ_DEL: {}", dwID);

	auto it = m_objectsByID.find(dwID);

	if (it == m_objectsByID.end())
		return;

	ObjectSystem::Destroy(it->second);
}

entt::entity CManager::FindObjectByVID(uint32_t dwVID)
{
	const auto it = m_objectsByVID.find(dwVID);
	return it != m_objectsByVID.end() && ObjectSystem::IsValid(it->second) ? it->second : entt::null;
}

void CManager::UnregisterObject(entt::entity object)
{
	if (auto* land = ObjectSystem::GetLand(object))
		land->UnregisterObject(object);
	const auto byID = m_objectsByID.find(ObjectSystem::GetID(object));
	if (byID != m_objectsByID.end() && byID->second == object)
		m_objectsByID.erase(byID);
	const auto byVID = m_objectsByVID.find(ObjectSystem::GetVID(object));
	if (byVID != m_objectsByVID.end() && byVID->second == object)
		m_objectsByVID.erase(byVID);
}

void CManager::SendLandList(LPDESC d, int32_t lMapIndex)
{
	TLandPacketElement e;

	TEMP_BUFFER buf;

	uint16_t wCount = 0;

	auto it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second;
		const TLand & r = pkLand->GetData();

		if (r.lMapIndex != lMapIndex)
			continue;

		//
		if (ecs::IsCharacter(d->GetEntity()))
		{
			CGuild *guild = CGuildManager::instance().FindGuild(r.dwGuildID);
			ecs::SocialSystem::SendGuildName(d->GetEntity(), guild);
		}
		//

		e.dwID = r.dwID;
		e.x = r.x;
		e.y = r.y;
		e.width = r.width;
		e.height = r.height;
		e.dwGuildID = r.dwGuildID;

		buf.write(&e, sizeof(TLandPacketElement));
		++wCount;
	}

	LOG_INFO("SendLandList map {} count {} elem_size: {}", lMapIndex, wCount, buf.size());

	if (wCount != 0)
	{
		TPacketGCLandList p;

		p.header = HEADER_GC_LAND_LIST;
		p.size = sizeof(TPacketGCLandList) + buf.size();

		d->BufferedPacket(&p, sizeof(TPacketGCLandList));
		d->Packet(buf.read_peek(), buf.size());
	}
}

// LAND_CLEAR
void CManager::ClearLand(uint32_t dwLandID)
{
	CLand* pLand = FindLand(dwLandID);

	if ( pLand == nullptr)
	{
		LOG_INFO("LAND_CLEAR: there is no LAND id like {}", dwLandID);
		return;
	}

	pLand->ClearLand();

	LOG_INFO("LAND_CLEAR: request Land Clear. LandID: {}", pLand->GetID());
}

void CManager::ClearLandByGuildID(uint32_t dwGuildID)
{
	CLand* pLand = FindLandByGuild(dwGuildID);

	if ( pLand == nullptr)
	{
		LOG_INFO("LAND_CLEAR: there is no GUILD id like {}", dwGuildID);
		return;
	}

	pLand->ClearLand();

	LOG_INFO("LAND_CLEAR: request Land Clear. LandID: {}", pLand->GetID());
}

void CLand::ClearLand()
{
	auto iter = m_objectsByID.begin();

	while ( iter != m_objectsByID.end() )
	{
		RequestDeleteObject(ObjectSystem::GetID(iter->second));
		iter++;
	}

	SetOwner(0);

	const TLand & r = GetData();
	const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(r.lMapIndex);

	CHARACTER_MANAGER::instance().SpawnMobEntity(20040, r.lMapIndex, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
}
// END_LAND_CLEAR

// BUILD_WALL
void CLand::DrawWall(uint32_t dwVnum, int32_t nMapIndex, int32_t& x, int32_t& y, char length, float zRot)
{
	int rot = (int)zRot;
	rot = ((rot%360) / 90) * 90;

	int dx=0, dy=0;

	switch ( rot )
	{
		case 0 :
			dx = -500;
			dy = 0;
			break;

		case 90 :
			dx = 0;
			dy = 500;
			break;

		case 180 :
			dx = 500;
			dy = 0;
			break;

		case 270 :
			dx = 0;
			dy = -500;
			break;
	}

	for ( int i=0; i < length; i++ )
	{
		this->RequestCreateObject(dwVnum, nMapIndex, x, y, 0, 0, rot, false);
		x += dx;
		y += dy;
	}
}


bool CLand::RequestCreateWall(int32_t nMapIndex, float rot)
{
	const bool 	WALL_ANOTHER_CHECKING_ENABLE = false;

	const TLand& land = GetData();

	int center_x = land.x + land.width  / 2;
	int center_y = land.y + land.height / 2;

	int wall_x = center_x;
	int wall_y = center_y;
	int wall_half_w = 1000;
	int wall_half_h = 1362;

	if (rot == 0.0f) 		// ���� ��
	{
		int door_x = wall_x;
		int door_y = wall_y + wall_half_h;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x, wall_y - wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 180.0f)		// ���� ��
	{
		int door_x = wall_x;
		int door_y = wall_y - wall_half_h;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x, wall_y - wall_half_h, door_x, door_y, 180.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 90.0f)		// ���� ��
	{
		int door_x = wall_x + wall_half_h;
		int door_y = wall_y;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 270.0f)		// ���� ��
	{
		int door_x = wall_x - wall_half_h;
		int door_y = wall_y;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}

	if (test_server)
	{
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + 50, 			land.y + 50, 0, 0, 0.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + land.width - 50,	land.y + 50, 0, 0, 90.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + land.width - 50,	land.y + land.height - 50, 0, 0, 180.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + 50, 			land.y + land.height - 50, 0, 0, 270.0, WALL_ANOTHER_CHECKING_ENABLE);
	}
	return true;
}

void CLand::RequestDeleteWall()
{
	auto iter = m_objectsByID.begin();

	while (iter != m_objectsByID.end())
	{
		unsigned id   = ObjectSystem::GetID(iter->second);
		unsigned vnum = ObjectSystem::GetVnum(iter->second);

		switch (vnum)
		{
			case WALL_DOOR_VNUM:
			case WALL_BACK_VNUM:
			case WALL_LEFT_VNUM:
			case WALL_RIGHT_VNUM:
				RequestDeleteObject(id);
				break;
		}


		if (test_server)
		{
			if (FLAG_VNUM == vnum)
				RequestDeleteObject(id);

		}

		iter++;
	}
}

bool CLand::RequestCreateWallBlocks(uint32_t dwVnum, int32_t nMapIndex, char wallSize, bool doorEast, bool doorWest, bool doorSouth, bool doorNorth)
{
	const TLand & r = GetData();

	int32_t startX = r.x + (r.width  / 2) - (1300 + wallSize*500);
	int32_t startY = r.y + (r.height / 2) + (1300 + wallSize*500);

	uint32_t corner = dwVnum - 4;
	uint32_t wall   = dwVnum - 3;
	uint32_t door   = dwVnum - 1;

	bool checkAnother = false;
	int32_t* ptr = nullptr;
	int delta = 1;
	int rot = 270;

	bool doorOpen[4];
	doorOpen[0] = doorWest;
	doorOpen[1] = doorNorth;
	doorOpen[2] = doorEast;
	doorOpen[3] = doorSouth;

	if ( wallSize > 3 ) wallSize = 3;
	else if ( wallSize < 0 ) wallSize = 0;

	for ( int i=0; i < 4; i++, rot -= 90 )
	{
		switch ( i )
		{
			case 0 :
				delta = -1;
				ptr = &startY;
				break;
			case 1 :
				delta = 1;
				ptr = &startX;
				break;
			case 2 :
				ptr = &startY;
				delta = 1;
				break;
			case 3 :
				ptr = &startX;
				delta = -1;
				break;
		}

		this->RequestCreateObject(corner, nMapIndex, startX, startY, 0, 0, rot, checkAnother);

		*ptr = *ptr + ( 700 * delta );

		if ( doorOpen[i] )
		{
			this->DrawWall(wall, nMapIndex, startX, startY, wallSize, rot);

			*ptr = *ptr + ( 700 * delta );

			this->RequestCreateObject(door, nMapIndex, startX, startY, 0, 0, rot, checkAnother);

			*ptr = *ptr + ( 1300 * delta );

			this->DrawWall(wall, nMapIndex, startX, startY, wallSize, rot);
		}
		else
		{
			this->DrawWall(wall, nMapIndex, startX, startY, wallSize*2 + 4, rot);
		}

		*ptr = *ptr + ( 100 * delta );
	}

	return true;
}

void CLand::RequestDeleteWallBlocks(uint32_t dwID)
{
	auto iter = m_objectsByID.begin();

	uint32_t corner = dwID - 4;
	uint32_t wall = dwID - 3;
	uint32_t door = dwID - 1;
	uint32_t dwVnum = 0;

	while ( iter != m_objectsByID.end() )
	{
		dwVnum = ObjectSystem::GetVnum(iter->second);

		if ( dwVnum == corner || dwVnum == wall || dwVnum == door )
		{
			RequestDeleteObject(ObjectSystem::GetID(iter->second));
		}
		iter++;
	}
}
// END_BUILD_WALL


