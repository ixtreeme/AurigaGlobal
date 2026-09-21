#include "stdafx.h"
#include "ecs/systems/ViewSystem.hpp"
#include "ecs/systems/InventorySystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AcceSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/SessionSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <common/tables.h>
#include "packet.h"
#include "item.h"
#include "char_interface.hpp"
#include "item_manager.h"
#include "desc.h"
#include "char_manager.h"
#include "banword.h"
#include "buffer_manager.h"
#include "desc_client.h"
#include "config.h"
#include "event.h"
#include "locale_service.h"
#include <fstream>
#include "db.h"
#include "sectree_manager.h"
#include "sectree.h"
#include "config.h"
#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
#include "log.h"
#endif
#include "new_offlineshop.h"
#include "ecs/OfflineShopEntityRegistry.hpp"
#include "ecs/Registry.hpp"
#include "ecs/services/SpatialService.hpp"
#include "ecs/components/spatial_components.hpp"
#include "ecs/components/transform_components.hpp"
#include "new_offlineshop_manager.h"
#include "ecs/systems/OfflineShopSystem.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include <boost/mpl/min_max.hpp>
#include "Core/Logging.hpp"

//#include <offlineshop/Offlineshop.h>

#define __ENABLE_OFFLINESHOP_GM_PROTECTION__
#ifdef __ENABLE_OFFLINESHOP_GM_PROTECTION__
#define MIN_USE_OFFLINESHOP_GMLEVEL GM_IMPLEMENTOR
#define _IS_VALID_GM_LEVEL(e) (ecs::PlayerRuntime::GetGMLevel(e) == GM_PLAYER || ecs::PlayerRuntime::GetGMLevel(e) >= MIN_USE_OFFLINESHOP_GMLEVEL)
#else
#define _IS_VALID_GM_LEVEL(e) (true)
#endif

#define PI 3.14159265
#define RADIANS_PER_DEGREE (PI/180.0)
#define TORAD(a)	((a)*RADIANS_PER_DEGREE)

struct MapInfo {
	int map_index = 0;
	int32_t x,y,radius;
};

struct Range {
	size_t min_count,max_count;
	int32_t min_radius,max_radius;
};


inline std::vector<MapInfo> GetMapInfo()
{
	std::vector<MapInfo> Val;

	MapInfo red_village;
	red_village.map_index = 1;
	red_village.radius = 2765;
	red_village.x = 474000;
	red_village.y = 954700;
	Val.emplace_back(red_village);

	MapInfo yellow_village;
	yellow_village.map_index = 363;
	yellow_village.radius = 2765;
	yellow_village.x = 473290;
	yellow_village.y = 718537;
	Val.emplace_back(yellow_village);

	MapInfo blue_village;
	blue_village.map_index = 41;
	blue_village.radius = 4000;
	blue_village.x = 984800;
	blue_village.y = 272900;
	Val.emplace_back(blue_village);

	return Val;
}

inline std::vector<Range> GetRanges()
{
	std::vector<Range> Val;

	Range low_count;
	low_count.min_count = 0;
	low_count.max_count = 25;
	low_count.min_radius = 0;
	low_count.max_radius = 1400;
	Val.emplace_back(low_count);

	Range medium_low_count;
	medium_low_count.min_count = low_count.max_count;
	medium_low_count.max_count = 40;
	medium_low_count.min_radius = 0;
	medium_low_count.max_radius = 2100;
	Val.emplace_back(medium_low_count);

	Range medium_count;
	medium_count.min_count = medium_low_count.max_count;
	medium_count.max_count = 60;
	medium_count.min_radius = 0;
	medium_count.max_radius = 2100;
	Val.emplace_back(medium_count);

	Range medium_high_count;
	medium_high_count.min_count = medium_count.max_count;
	medium_high_count.max_count = 80;
	medium_high_count.min_radius = 0;
	medium_high_count.max_radius = 2500;
	Val.emplace_back(medium_high_count);

	Range high_count;
	high_count.min_count = medium_high_count.max_count;
	high_count.max_count = 100;
	high_count.min_radius = 0;
	high_count.max_radius = 3000;
	Val.emplace_back(high_count);

	return Val;
}

const std::vector<MapInfo> g_Maps = GetMapInfo();


uint32_t Offlineshop_GetMapCount(){
	return g_Maps.size();
}

void Offlineshop_GetMapIndex(size_t index, int* out_index)
{
	*out_index = g_Maps[index].map_index;
}

bool Offlineshop_CheckPositionDistance(int32_t x1, int32_t y1, int32_t x2, int32_t y2){
	const double xoff = x1 > x2? x1 - x2: x2-x1;
	const double yoff = y1 > y2? y1 - y2: y2-y1;

	#define PITAGORA(c1,c2) sqrt((c1*c1)+(c2*c2))
	const double distance = PITAGORA(xoff,yoff);

	return (distance > 350.0);
}


void Offlineshop_GetRangeByCount(size_t ent_count, int32_t radius, int32_t* out_min, int32_t* out_max)
{
	static const auto RangeVector = GetRanges();

	for(const auto& RangeElm : RangeVector)
	{
		if(ent_count > RangeElm.min_count && ent_count < RangeElm.max_count)
		{
			*out_min = RangeElm.min_radius;
			*out_max = RangeElm.max_radius;
			return;
		}
	}

	*out_min = 0;
	*out_max = radius;
}

void Offlineshop_GetNewPos(int index, size_t ent_count, int32_t* out_x, int32_t* out_y)
{
	auto& mapInfo = g_Maps[index];

	int32_t min_ =0, max_=0;
	Offlineshop_GetRangeByCount(ent_count, mapInfo.radius, &min_, &max_);

	int32_t random_distance	= number(min_, max_);
	int32_t random_degree		= number(0, 360);

	int32_t centerx = mapInfo.x;
	int32_t centery = mapInfo.y;

	if (random_degree >= 0.0 && random_degree < 90.0)
	{
		*out_x = centerx + (random_distance*cos(TORAD(random_degree)));
		*out_y = centery + (random_distance*sin(TORAD(random_degree)));
	}

	else if (random_degree >= 90.0 && random_degree < 180.0)
	{
		const float beta = 180.0-random_degree;
		*out_x = centerx - (random_distance*cos(TORAD(beta)));
		*out_y = centery + (random_distance*sin(TORAD(beta)));
	}

	else if (random_degree >= 180.0 && random_degree < 270.0)
	{
		const float beta = 270.0-random_degree;
		*out_x = centerx - (random_distance*cos(TORAD(beta)));
		*out_y = centery - (random_distance*sin(TORAD(beta)));
	}

	else
	{
		const float beta = 360.0-random_degree;
		*out_x = centerx + (random_distance*cos(TORAD(beta)));
		*out_y = centery - (random_distance*sin(TORAD(beta)));
	}
}

#ifdef __ENABLE_NEW_OFFLINESHOP__
bool MatchWearFlag(uint32_t dwWearFilter, uint32_t dwWearTable)
{
	if(dwWearFilter==0)
		return true;


	static const uint32_t flags[] = {
		ITEM_ANTIFLAG_MALE,
		ITEM_ANTIFLAG_FEMALE,
		ITEM_ANTIFLAG_WARRIOR,
		ITEM_ANTIFLAG_ASSASSIN,
		ITEM_ANTIFLAG_SURA,
		ITEM_ANTIFLAG_SHAMAN,
	};


	const size_t counts = sizeof(flags)/sizeof(uint32_t);

	for(size_t i=0; i < counts; i++)
		if(IS_SET(dwWearFilter, flags[i]) &&!IS_SET(dwWearTable, flags[i]))
			return false;
	return true;
}


bool MatchAttributes(const TPlayerItemAttribute* pAttributesFilter,const TPlayerItemAttribute* pAttributesItem)
{
	for (int i = 0; i < ITEM_ATTRIBUTE_NORM_NUM; i++)
	{
		if(pAttributesFilter[i].bType == 0)
			continue;

		bool bFound=false;

		uint8_t type	= pAttributesFilter[i].bType;
		int  val	= pAttributesFilter[i].sValue;


		for (int i = 0; i < ITEM_ATTRIBUTE_NORM_NUM; i++)
		{
			if (pAttributesItem[i].bType == type)
			{
				bFound = pAttributesItem[i].sValue >= val;
				break;
			}
		}

		if(!bFound)
			return false;
	}

	return true;
}


std::string StringToLower(const char* name, size_t len)
{
	std::string res;
	res.resize(len);

	for(size_t i=0; i < len; i++)
		res[i] = tolower(*(name + i));
	return res;
}

//topatch

//updated 25-01-2020
bool IsGoodSalePrice(const offlineshop::TPriceInfo price) {
	if (price.illYang >= GOLD_MAX) {
		return false;
	}

#ifdef __ENABLE_CHEQUE_SYSTEM__
	else if (price.iCheque >= CHEQUE_MAX){
		return false;
	}
#endif

	else {
		return true;
	}
}


bool MatchItemName(std::string stName, const char* table , const size_t tablelen)
{
	/*
	std::string stTable(table, tablelen) , stName(name, namelen);

	//checking about refinegrade into tablename
	size_t refineGrade = stTable.find('+');
	if(refineGrade != std::string::npos && refineGrade > stTable.length() - 4)
		stTable = stTable.substr(0, refineGrade);

	//checking about refinegrade into itemname
	refineGrade = stName.find('+');
	if(refineGrade != std::string::npos && refineGrade > stName.length() - 4)
		stName = stName.substr(0, refineGrade);


	return strncasecmp(stName.c_str() , stTable.c_str() , stName.length()) == 0;
	*/

	std::string stTable= StringToLower(table, tablelen);
	return stTable.find(stName) != std::string::npos;
}


bool CheckCharacterActions(entt::entity character)
{
	if(!ecs::PlayerRuntime::IsValid(character))
	{
		return false;
	}


	if(ecs::SocialSystem::HasExchange(character))
	{
		return false;
	}


	if(ecs::SessionSystem::GetSafebox(character) != entt::null)
	{
		return false;
	}


	if (ecs::SocialSystem::GetShop(character) != entt::null)
	{
		return false;
	}


	if (ecs::SessionSystem::IsCubeOpen(character))
	{
		return false;
	}

#ifdef ENABLE_ACCE_SYSTEM
	if (ecs::AcceSystem::IsOpen(character))
	{
		return false;
	}

#endif

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (AttrTransfer_is_open(character))
	{
		return false;
	}
#endif

	return true;
}

//updated 25-01-2020 //topatch
int64_t GetTotalAmountFromPrice(const offlineshop::TPriceInfo& price)
{
	return price.GetTotalYangAmount();
}


bool CheckNewAuctionOfferPrice(const offlineshop::TPriceInfo& price, const offlineshop::TPriceInfo& best)
{
	int64_t totalValueIn =0, totalValueBest=0;


	totalValueIn	= GetTotalAmountFromPrice(price);
	totalValueBest	= GetTotalAmountFromPrice(best);

	if(totalValueBest< 1000)
		totalValueBest += 1000;

	else
	{
		const float percentage = (float)OFFLINESHOP_AUCTION_RAISE_PERCENTAGE/100.0;
		totalValueBest	+=(int64_t)(((long double)(totalValueBest))*percentage);
	}


	return totalValueIn >= totalValueBest;
}


namespace offlineshop
{
	EVENTINFO(offlineshopempty_info)
	{
		int empty;

		offlineshopempty_info()
			: empty(0)
		{
		}
	};

	EVENTFUNC(func_offlineshop_update_duration)
	{
		offlineshop::GetManager().UpdateShopsDuration();
		offlineshop::GetManager().UpdateAuctionsDuration();
		offlineshop::GetManager().ClearSearchTimeMap();
		offlineshop::GetManager().ClearOfferTimeMap();
		return OFFLINESHOP_DURATION_UPDATE_TIME;
	}


	offlineshop::CShopManager& GetManager()
	{
		return offlineshop::CShopManager::instance();
	}


	offlineshop::CShop * CShopManager::PutsNewShop(TShopInfo * pInfo)
	{
		OFFSHOP_DEBUG("puts new shop %s ", pInfo->szName);

		SHOPMAP::iterator it = m_mapShops.insert(std::make_pair(pInfo->dwOwnerID, offlineshop::CShop())).first;
		offlineshop::CShop& rShop = it->second;

		rShop.SetDuration(pInfo->dwDuration);
		rShop.SetOwnerPID(pInfo->dwOwnerID);
		rShop.SetName(pInfo->szName);
#ifdef KASMIR_PAKET_SYSTEM
		rShop.SetRace(pInfo->dwKasmirNpc);
#endif
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		CreateNewShopEntities(rShop);
#endif
		return &rShop;
	}


	void CShopManager::PutsAuction(const TAuctionInfo& auction)
	{
		CAuction& obj = m_mapAuctions[auction.dwOwnerID];
		obj.SetInfo(auction);
		return;
	}


	void CShopManager::PutsAuctionOffer(const TAuctionOfferInfo& offer)
	{
		auto it = m_mapAuctions.find(offer.dwOwnerID);
		if( it == m_mapAuctions.end())
			return;

		CAuction& obj = it->second;
		obj.AddOffer(offer);
	}


	offlineshop::CShop* CShopManager::GetShopByOwnerID(uint32_t dwPID)
	{
		SHOPMAP::iterator it=m_mapShops.find(dwPID);
		if(it == m_mapShops.end())
			return nullptr;

		return &(it->second);
	}


	offlineshop::CAuction* CShopManager::GetAuctionByOwnerID(uint32_t dwPID)
	{
		AUCTIONMAP::iterator it=m_mapAuctions.find(dwPID);
		if(it == m_mapAuctions.end())
			return nullptr;

		return &(it->second);
	}


	void CShopManager::RemoveSafeboxFromCache(uint32_t dwOwnerID)
	{
		SAFEBOXMAP::iterator it = m_mapSafeboxs.find(dwOwnerID);
		if(it==m_mapSafeboxs.end())
			return;

		m_mapSafeboxs.erase(it);
	}


	void CShopManager::RemoveGuestFromShops(entt::entity character)
	{
		if(ecs::OfflineShopSystem::GetOfflineShopGuest(character))
			ecs::OfflineShopSystem::GetOfflineShopGuest(character)->RemoveGuest(character);

		ecs::OfflineShopSystem::SetOfflineShopGuest(character, nullptr);

		if(ecs::OfflineShopSystem::GetOfflineShop(character))
			ecs::OfflineShopSystem::GetOfflineShop(character)->RemoveGuest(character);

		ecs::OfflineShopSystem::SetOfflineShop(character, nullptr);
	}


	CShopManager::CShopManager()
	{
		offlineshopempty_info* info = AllocEventInfo<offlineshopempty_info>();
		m_eventShopDuration = event_create(func_offlineshop_update_duration, info, OFFLINESHOP_DURATION_UPDATE_TIME);
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		m_vecCities.resize(Offlineshop_GetMapCount());
#endif
	}


	CShopManager::~CShopManager()
	{
		Destroy();
	}


	void CShopManager::Destroy()
	{
		//deleting event
		if(m_eventShopDuration)
			event_cancel(&m_eventShopDuration);

		m_eventShopDuration = nullptr;

		//clearing containers
		m_mapOffer.clear();
		m_mapSafeboxs.clear();
		m_mapShops.clear();


#ifdef ENABLE_NEW_SHOP_IN_CITIES
		// Stage ownership before draining any public index. A callback exception
		// must not abandon the remaining city avatars or lose the failed handle.
		size_t count = 0;
		for (const auto& city : m_vecCities)
			count += city.entitiesByPID.size();
		m_retiringEntities.reserve(m_retiringEntities.size() + count);
		for (const auto& city : m_vecCities)
			for (const auto& [pid, entity] : city.entitiesByPID)
				m_retiringEntities.push_back(entity);
		m_vecCities.clear();
		ecs::OfflineShopEntityRegistry::DestroyPending(m_retiringEntities);
#endif

	}


#ifdef ENABLE_NEW_SHOP_IN_CITIES

	bool IsEmptyString(const std::string& st){
		return st.find_first_not_of(" \t\r\n") == std::string::npos;
	}


	bool CShopManager::__CanUseCity(size_t index)
	{
		int map_index=0;
		Offlineshop_GetMapIndex(index, &map_index);
		return SECTREE_MANAGER::instance().GetMap(map_index) != nullptr;
	}


	bool CShopManager::__CheckEntitySpawnPos(const int32_t x, const int32_t y, const TCityShopInfo& city)
	{
		const SHOPENTITIES_MAP& entitiesMap = city.entitiesByPID;


		for (auto it = entitiesMap.begin(); it != entitiesMap.end(); ++it)
		{
			if (!g_registry.valid(it->second))
				continue;
			const auto* pos = g_registry.try_get<ecs::Position>(it->second);
			if (pos && !Offlineshop_CheckPositionDistance(pos->x, pos->y, x, y))
				return false;
		}

		return true;
	}


	void CShopManager::__UpdateEntity(const offlineshop::CShop& rShop)
	{
		const uint32_t ownerPID = rShop.GetOwnerPID();
		const std::string name = rShop.GetName();
#ifdef KASMIR_PAKET_SYSTEM
		const uint32_t race = rShop.GetRace();
#endif
		std::vector<entt::entity> entities;
		entities.reserve(m_vecCities.size());
		for (const auto& city : m_vecCities)
		{
			const auto it = city.entitiesByPID.find(ownerPID);
			if (it != city.entitiesByPID.end())
				entities.push_back(it->second);
		}
		for (const auto entity : entities)
		{
			if (!g_registry.valid(entity) || g_registry.all_of<ecs::SpatialRetiring>(entity))
				continue;
			auto* state = g_registry.try_get<ecs::OfflineShopState>(entity);
			if (!state || state->ownerPID != ownerPID ||
				ecs::OfflineShopEntityRegistry::FindByVID(state->vid) != entity)
				continue;
			state->name = name;
#ifdef KASMIR_PAKET_SYSTEM
			state->race = race;
#endif
			// No component pointer or commerce object is used after publication.
			if (ecs::SpatialService::GetSectree(g_registry, entity))
				ecs::ViewSystem::ViewReencode(entity);
#ifdef ENABLE_OFFLINESHOP_DEBUG
			else
				LOG_ERROR("cant find sectree for entity : name {} , pid {} ", name, ownerPID);
#endif
		}
	}


	void CShopManager::CreateNewShopEntities(offlineshop::CShop& rShop)
	{
		// Snapshot commerce data before any spatial callback can remove the shop.
		const uint32_t ownerPID = rShop.GetOwnerPID();
		const std::string name = rShop.GetName();
#ifdef KASMIR_PAKET_SYSTEM
		const uint32_t race = rShop.GetRace();
#else
		const uint32_t race = 0;
#endif
		for (size_t index = 0; index < m_vecCities.size(); ++index)
		{
			if (!GetShopByOwnerID(ownerPID))
				break;
			// A repeated create must not replace the only handle to a live avatar.
			if (m_vecCities[index].entitiesByPID.contains(ownerPID))
				continue;
			int32_t shop_pos_x = 0, shop_pos_y=0;
			int iCheckCount =0;
			int map_index =0;
			Offlineshop_GetMapIndex(index, &map_index);
			const size_t ent_count = m_vecCities[index].entitiesByPID.size();
			do {
				Offlineshop_GetNewPos(index, ent_count, &shop_pos_x, &shop_pos_y);
			} while (!__CheckEntitySpawnPos(shop_pos_x, shop_pos_y, m_vecCities[index]) && iCheckCount++ < 10);

			const auto entity = ecs::OfflineShopEntityRegistry::Create(
				ownerPID, name, race, 0, static_cast<uint32_t>(map_index), shop_pos_x, shop_pos_y);
			if (entity == entt::null)
			{
				LOG_ERROR("cannot insert offline shop owner {} map {} pos {} {}",
					ownerPID, map_index, shop_pos_x, shop_pos_y);
				continue;
			}
			// Keep rollback ownership until both city indexes are committed. If a
			// cleanup callback throws, a later manager teardown can still retry it.
			try
			{
				m_retiringEntities.push_back(entity);
			}
			catch (...)
			{
				ecs::OfflineShopEntityRegistry::Destroy(entity);
				throw;
			}
			// Construction signals may have changed city ownership. Reacquire the
			// container and install both lookup paths before publishing INSERT.
			if (index >= m_vecCities.size() || !GetShopByOwnerID(ownerPID) ||
				m_vecCities[index].entitiesByPID.contains(ownerPID))
			{
				ecs::OfflineShopEntityRegistry::DestroyPending(m_retiringEntities);
				continue;
			}
			const auto vid = g_registry.get<ecs::OfflineShopState>(entity).vid;
			try
			{
				m_vecCities[index].entitiesByPID.emplace(ownerPID, entity);
				m_vecCities[index].entitiesByVID.emplace(vid, entity);
			}
			catch (...)
			{
				m_vecCities[index].entitiesByPID.erase(ownerPID);
				m_vecCities[index].entitiesByVID.erase(vid);
				ecs::OfflineShopEntityRegistry::DestroyPending(m_retiringEntities);
				throw;
			}
			std::erase(m_retiringEntities, entity);
			ecs::SpatialService::UpdateSectree(g_registry, entity);
		}
	}


	void CShopManager::DestroyNewShopEntities(const offlineshop::CShop& rShop)
	{
		if (g_bAuthServer)
			return;
		const uint32_t ownerPID = rShop.GetOwnerPID();
		m_retiringEntities.reserve(m_retiringEntities.size() + m_vecCities.size());
		for (auto& city : m_vecCities)
		{
			const auto it = city.entitiesByPID.find(ownerPID);
			if (it == city.entitiesByPID.end())
				continue;
			const auto entity = it->second;
			m_retiringEntities.push_back(entity);
			city.entitiesByPID.erase(it);
			std::erase_if(city.entitiesByVID, [entity](const auto& entry) { return entry.second == entity; });
		}
		// Every city index is retired before the first visibility callback.
		ecs::OfflineShopEntityRegistry::DestroyPending(m_retiringEntities);
	}


#endif


	CShopSafebox* CShopManager::GetShopSafeboxByOwnerID(uint32_t dwPID)
	{
		SAFEBOXMAP::iterator it = m_mapSafeboxs.find(dwPID);
		if(it == m_mapSafeboxs.end())
			return nullptr;
		return &(it->second);
	}


	//offers
	bool CShopManager::PutsNewOffer(const TOfferInfo* pInfo)
	{
		OFFERSMAP::iterator it= m_mapOffer.find(pInfo->dwOffererID);

		if (it == m_mapOffer.end())
			it = m_mapOffer.insert(std::make_pair(pInfo->dwOffererID, std::vector<TOfferInfo>())).first;

		else
		{
			auto itVec = it->second.begin();
			for (; itVec != it->second.end(); itVec++)
			{
				if(itVec->dwOfferID == pInfo->dwOfferID)
					return false;
			}
		}


		it->second.push_back(*pInfo);
		return true;
	}


	//db packets exchanging
	void CShopManager::SendShopBuyDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID, uint32_t dwItemID)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_BUY_ITEM;

		TSubPacketGDBuyItem subpack;
		subpack.dwGuestID	= dwBuyerID;
		subpack.dwOwnerID	= dwOwnerID;
		subpack.dwItemID	= dwItemID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("sending for shop %u and item %u (buyer %u) ",dwOwnerID, dwItemID, dwBuyerID);
#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
		LogManager::instance().OfflineshopLog(dwOwnerID, dwItemID, "%u is buying the item", dwBuyerID);
#endif
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopBuyDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID)
	{
		OFFSHOP_DEBUG("buyer %u , owner %u , itemid %u ",dwBuyerID, dwOwnerID, dwItemID);

		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;


		CShopItem* pItem = nullptr;
		if(!pkShop->GetItem(dwItemID, &pItem))
			return false;

		OFFSHOP_DEBUG("checked %s" , "successful");

		const entt::entity chEntity = CHARACTER_MANAGER::instance().FindEntityByPID(dwBuyerID);


		if (ecs::IsCharacter(chEntity))
		{
			OFFSHOP_DEBUG("buyer is online , name %s , item id %u ",ecs::PlayerRuntime::GetName(chEntity).data(), dwItemID);

			const entt::entity pkItem = pItem->CreateItem();
			if (!ItemSystem::IsValidItem(pkItem))
			{
				LOG_ERROR("cannot create item ( dwItemID {} , dwVnum {}, dwShopOwner {}, dwBuyer {} ) ", dwItemID, pItem->GetInfo()->dwVnum, dwOwnerID, dwBuyerID);
				return false;
			}

			TItemPos pos;
			if (!InventorySystem::CanTakeInventoryItem(chEntity, pkItem, &pos))
			{
				ItemSystem::DestroyItemEntityEcs(
			pkItem,
			"OFFLINESHOP_TEMP");

				CShopSafebox* pSafebox = ecs::OfflineShopSystem::GetShopSafebox(chEntity)? ecs::OfflineShopSystem::GetShopSafebox(chEntity) : GetShopSafeboxByOwnerID((ecs::PlayerRuntime::GetPlayerID(chEntity)));
				if (!pSafebox)
					return false;

				/*
				if(!pSafebox->AddItem(pItem))
					return false;
				*/

				SendShopSafeboxAddItemDBPacket((ecs::PlayerRuntime::GetPlayerID(chEntity)), *pItem);
				SendChatPacket(chEntity, CHAT_PACKET_RECV_ITEM_SAFEBOX);
			}

			else
			{
				InventorySystem::AddToCharacter(pkItem, chEntity, pos);
			}

			uint32_t dwItemID = pItem->GetID();
			pkShop->BuyItem(dwItemID);
		}

		else
		{
			OFFSHOP_DEBUG("buyer isn't online , item removed %u (shop %u)",dwItemID, pkShop->GetOwnerPID());

			uint32_t dwItemID = pItem->GetID();
			pkShop->BuyItem(dwItemID);
		}


		return true;
	}


	void CShopManager::SendShopEditItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID, const TPriceInfo& rPrice)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_EDIT_ITEM;

		TSubPacketGDEditItem subpack;
		subpack.dwOwnerID	= dwOwnerID;
		subpack.dwItemID	= dwItemID;
		CopyObject(subpack.priceInfo , rPrice);

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("item id %u, owner shop %u",dwItemID, dwOwnerID);
#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
		LogManager::instance().OfflineshopLog(dwOwnerID, dwItemID, "change the price of item to %lld yang "
#ifdef __ENABLE_CHEQUE_SYSTEM__
			" and %d cheque "
#endif
			, rPrice.illYang
#ifdef __ENABLE_CHEQUE_SYSTEM__
			, pPrice.iCheque
#endif
		);
#endif
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}

	//topatch
	bool CShopManager::RecvShopEditItemClientPacket(entt::entity character, uint32_t dwItemID, const TPriceInfo& price)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		//updated 25 - 01 - 2020
#if !defined(__ENABLE_FULL_YANG__) && !defined(ENABLE_FULL_YANG) && !defined(REMOVE_YANG_LIMIT)
		if (!IsGoodSalePrice(price))
			return false;
#endif

		CShop* pkShop		= ecs::OfflineShopSystem::GetOfflineShop(character);
		CShopItem* pItem	= nullptr;

		if(!pkShop->GetItem(dwItemID, &pItem))
			return false;

		TPriceInfo* pPrice = pItem->GetPrice();

		//updated 25 - 01 - 2020
#ifndef __ENABLE_CHEQUE_SYSTEM__
		if(price.illYang == pPrice->illYang)
			return true;
#endif

		ecs::OfflineShopSystem::SetUseTime(character);

		SendShopEditItemDBPacket(pkShop->GetOwnerPID(), dwItemID, price);
		return true;
	}


	void CShopManager::SendShopRemoveItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_REMOVE_ITEM;

		TSubPacketGDRemoveItem subpack;
		subpack.dwOwnerID	= dwOwnerID;
		subpack.dwItemID	= dwItemID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("owner %u , item %u ",dwOwnerID, dwItemID);
#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
		LogManager::instance().OfflineshopLog(dwOwnerID, dwItemID, "the item is removed");
#endif
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopRemoveItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		//topatch 29-10
		CheckOfferOnItem(dwOwnerID, dwItemID);

		OFFSHOP_DEBUG("owner %u , item %u", dwOwnerID, dwItemID);
		return pkShop->RemoveItem(dwItemID);
	}


	void CShopManager::SendShopAddItemDBPacket(uint32_t dwOwnerID, const TItemInfo& rItemInfo)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_ADD_ITEM;

		TSubPacketGDAddItem subpack;
		subpack.dwOwnerID	= dwOwnerID;
		CopyObject(subpack.itemInfo, rItemInfo);

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("owner %u , item vnum %u , item count %u ",dwOwnerID, rItemInfo.item.dwVnum , rItemInfo.item.dwCount);
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopAddItemDBPacket(uint32_t dwOwnerID, const TItemInfo& rItemInfo)
	{
		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		CShopItem newItem(rItemInfo.dwItemID);
		newItem.SetInfo(rItemInfo.item);
		newItem.SetPrice(rItemInfo.price);
		newItem.SetOwnerID(rItemInfo.dwOwnerID);

		OFFSHOP_DEBUG("owner %u , item id %u ",dwOwnerID, rItemInfo.dwItemID);
		return pkShop->AddItem(newItem);
	}


	//SHOPS
	void CShopManager::SendShopForceCloseDBPacket(uint32_t dwPID)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_SHOP_FORCE_CLOSE;

		TSubPacketGDShopForceClose subpack;
		subpack.dwOwnerID = dwPID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("shop %u ",dwPID);
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopForceCloseDBPacket(uint32_t dwPID)
	{
		CShop* pkShop = GetShopByOwnerID(dwPID);
		if(!pkShop)
			return false;

		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);

		if (ecs::IsCharacter(ch))
			ecs::OfflineShopSystem::SetOfflineShop(ch, nullptr);


		CShop::LISTGUEST* guests = pkShop->GetGuests();

		for (CShop::LISTGUEST::iterator it = guests->begin(); it != guests->end(); it++)
		{

			const entt::entity chGuest = CHARACTER_MANAGER::instance().FindEntityByPID(*it);

			if (!ecs::IsCharacter(chGuest)) {
				continue;
			}

			if (ecs::IsCharacter(ch) && ch == chGuest)
				SendShopOpenMyShopNoShopClientPacket(chGuest);

			else
				SendShopListClientPacket(chGuest);

			ecs::OfflineShopSystem::SetOfflineShopGuest(chGuest, nullptr);
		}


		std::set<uint32_t> setPids;

		//offers check
		CShop::VECSHOPOFFER& vec = *pkShop->GetOffers();
		for (uint32_t i = 0; i < vec.size(); i++)
		{
			//for each offer removing from buyer
			TOfferInfo& offer = vec[i];
			uint32_t buyer = offer.dwOffererID;

			//searching buyer into map
			auto itOffer = m_mapOffer.find(buyer);
			if (itOffer != m_mapOffer.end())
			{
				//searching offer id in vec
				CShop::VECSHOPOFFER& buyerOffers = itOffer->second;
				for (auto itBuyer = buyerOffers.begin(); itBuyer != buyerOffers.end(); itBuyer++)
				{
					if (itBuyer->dwOfferID == offer.dwOfferID)
					{
						buyerOffers.erase(itBuyer);
						setPids.insert(buyer);
						break;
					}
				}
			}
		}

		for (auto itpid = setPids.begin(); itpid != setPids.end(); itpid++)
		{
			const entt::entity chBuyer = CHARACTER_MANAGER::instance().FindEntityByPID(*itpid);
			if(ecs::IsCharacter(chBuyer))
				RecvOfferListRequestPacket(chBuyer);
		}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
		DestroyNewShopEntities(*pkShop);
#endif
		pkShop->Clear();

		m_mapShops.erase(m_mapShops.find(pkShop->GetOwnerPID()));
		return true;
	}


	void CShopManager::SendShopLockBuyItemDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID, int64_t TotalPriceSeen)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader = SUBHEADER_GD_BUY_LOCK_ITEM;

		TSubPacketGDLockBuyItem subpack;
		subpack.dwGuestID = dwBuyerID;
		subpack.dwOwnerID = dwOwnerID;
		subpack.dwItemID  = dwItemID;
		subpack.TotalPriceSeen = TotalPriceSeen;

		TEMP_BUFFER buff;
		buff.write(&pack,	 sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("shop %u , buyer %u , item %u (size %u) ",dwOwnerID, dwBuyerID, dwItemID, buff.size());
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}

	bool CShopManager::RecvShopLockedBuyItemDBPacket(uint32_t dwBuyerID, uint32_t dwOwnerID,uint32_t dwItemID)
	{
		CShop* pkShop	= GetShopByOwnerID(dwOwnerID);
		const entt::entity chEntity = CHARACTER_MANAGER::instance().FindEntityByPID(dwBuyerID);


		if(!ecs::IsCharacter(chEntity) || !pkShop)
			return false;

		OFFSHOP_DEBUG("found shop %u ",dwBuyerID);

		CShopItem* pkItem = nullptr;
		if(!pkShop->GetItem(dwItemID, &pkItem))
			return false;

		OFFSHOP_DEBUG("found item %u",dwItemID);

		if(!pkItem->CanBuy(chEntity))
			return false;

		OFFSHOP_DEBUG("can buy %u",dwItemID);

		TPriceInfo* pPrice = pkItem->GetPrice();
		ecs::PointSystem::Change(chEntity, POINT_GOLD, -pPrice->illYang);
#ifdef __ENABLE_CHEQUE_SYSTEM__
		ecs::PointSystem::Change(chEntity, POINT_CHEQUE, -pPrice->iCheque);
#endif

		SendShopBuyDBPacket(dwBuyerID, dwOwnerID, dwItemID);
		return true;
	}


	void CShopManager::SendShopCannotBuyLockedItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID) //topatch
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader = SUBHEADER_GD_CANNOT_BUY_LOCK_ITEM;

		TSubPacketGDCannotBuyLockItem subpack;
		subpack.dwOwnerID = dwOwnerID;
		subpack.dwItemID = dwItemID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("item id %u, owner shop %u", dwItemID, dwOwnerID);
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopExpiredDBPacket(uint32_t dwPID) //topatch
	{
		CShop* pkShop = GetShopByOwnerID(dwPID);
		if (!pkShop)
			return false;

		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);

		if (ecs::IsCharacter(ch))
			ecs::OfflineShopSystem::SetOfflineShop(ch, nullptr);


		//*getting the guest list before to remove the shop
		//*that is necessary to send the shop list packets
		CShop::LISTGUEST guests = *pkShop->GetGuests();


		std::set<uint32_t> setPids;

		//offers check
		CShop::VECSHOPOFFER& vec = *pkShop->GetOffers();
		for (uint32_t i = 0; i < vec.size(); i++)
		{
			//for each offer removing from buyer
			TOfferInfo& offer = vec[i];
			uint32_t buyer = offer.dwOffererID;

			//searching buyer into map
			auto itOffer = m_mapOffer.find(buyer);
			if (itOffer != m_mapOffer.end())
			{
				//searching offer id in vec
				CShop::VECSHOPOFFER& buyerOffers = itOffer->second;
				for (auto itBuyer = buyerOffers.begin(); itBuyer != buyerOffers.end(); itBuyer++)
				{
					if (itBuyer->dwOfferID == offer.dwOfferID)
					{
						buyerOffers.erase(itBuyer);
						setPids.insert(buyer);
						break;
					}
				}
			}
		}

		for (auto itpid = setPids.begin(); itpid != setPids.end(); itpid++)
		{
			const entt::entity chBuyer = CHARACTER_MANAGER::instance().FindEntityByPID(*itpid);
			if (ecs::IsCharacter(chBuyer))
				RecvOfferListRequestPacket(chBuyer);
		}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
		DestroyNewShopEntities(*pkShop);
#endif
		pkShop->Clear();
		m_mapShops.erase(m_mapShops.find(pkShop->GetOwnerPID()));


		for (CShop::LISTGUEST::iterator it = guests.begin(); it != guests.end(); it++)
		{
			const entt::entity chGuest = CHARACTER_MANAGER::instance().FindEntityByPID(*it);

			if (!ecs::IsCharacter(chGuest)) {
				continue;
			}

			if (ecs::IsCharacter(ch) && ch == chGuest)
				SendShopOpenMyShopNoShopClientPacket(chGuest);

			else
				SendShopListClientPacket(chGuest);

			ecs::OfflineShopSystem::SetOfflineShopGuest(chGuest, nullptr);
		}

		return true;
	}


	void CShopManager::SendShopCreateNewDBPacket(const TShopInfo& shop, std::vector<TItemInfo>& vec
	)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_SHOP_CREATE_NEW;

		TSubPacketGDShopCreateNew subpack;
		CopyObject(subpack.shop, shop);
		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		for(uint32_t i =0; i<vec.size(); i++)
			buff.write(&vec[i], sizeof(TItemInfo));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopCreateNewDBPacket(const TShopInfo& shop, std::vector<TItemInfo>& vec)
	{
		OFFSHOP_DEBUG("shop %s , shop id %u ", shop.szName, shop.dwOwnerID);

		if(m_mapShops.find(shop.dwOwnerID)!= m_mapShops.end())
			return false;


		CShop newShop;
		newShop.SetOwnerPID(shop.dwOwnerID);
		newShop.SetDuration(shop.dwDuration);
		newShop.SetName(shop.szName);
#ifdef KASMIR_PAKET_SYSTEM
		newShop.SetRace(shop.dwKasmirNpc);
#endif

		std::vector<CShopItem> items;
		items.reserve(vec.size());

		for (uint32_t i = 0; i < vec.size(); i++)
		{
			const TItemInfo& rItem = vec[i];

			CShopItem shopItem(rItem.dwItemID);

			shopItem.SetOwnerID(rItem.dwOwnerID);
			shopItem.SetPrice(rItem.price);
			shopItem.SetInfo(rItem.item);

			OFFSHOP_DEBUG("item id %u , item vnum %u , item count %u ",rItem.dwItemID, rItem.item.dwVnum , rItem.item.dwCount);

			items.push_back(shopItem);
		}

		newShop.SetItems(&items);

		OFFSHOP_DEBUG("shop %s , shop id %u inserted into map (items count %d)", shop.szName, shop.dwOwnerID, shop.dwCount);
		SHOPMAP::iterator it = m_mapShops.insert(std::make_pair(newShop.GetOwnerPID(), newShop)).first;

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		CreateNewShopEntities(it->second);
#endif


		const entt::entity chOwner = it->second.FindOwnerCharacter();
		if (ecs::IsCharacter(chOwner))
		{
			ecs::OfflineShopSystem::SetOfflineShop(chOwner, &(it->second));
			ecs::OfflineShopSystem::SetOfflineShopGuest(chOwner, &(it->second));

			it->second.AddGuest(chOwner);
			SendShopOpenMyShopClientPacket(chOwner);
		}

		return true;
	}


	void CShopManager::SendShopChangeNameDBPacket(uint32_t dwOwnerID, const char* szName)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_SHOP_CHANGE_NAME;

		TSubPacketGDShopChangeName subpack;
		subpack.dwOwnerID	= dwOwnerID;
		strncpy(subpack.szName, szName, sizeof(subpack.szName));

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("shop id %u , name [%s]",dwOwnerID, szName);
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopChangeNameDBPacket(uint32_t dwOwnerID, const char* szName)
	{
		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		pkShop->SetName(szName);
		pkShop->RefreshToOwner();

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		__UpdateEntity(*pkShop);
#endif

		OFFSHOP_DEBUG("id %u , name %s ",dwOwnerID, szName);
		return true;
	}


	//OFFER
	void CShopManager::SendShopOfferNewDBPacket(const TOfferInfo& offer)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_OFFER_CREATE;

		TSubPacketGDOfferCreate subpack;
		subpack.dwOwnerID	= offer.dwOwnerID;
		subpack.dwItemID	= offer.dwItemID;
		CopyObject(subpack.offer, offer);

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG("offerer %u , shop %u ",offer.dwOffererID , offer.dwOwnerID);
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopOfferNewDBPacket(const TOfferInfo& offer)
	{
		CShop* pkShop = GetShopByOwnerID(offer.dwOwnerID);
		if(!pkShop)
			return false;

		OFFSHOP_DEBUG("offerer %u , shop %u ", offer.dwOffererID , offer.dwOwnerID);
		if(!pkShop->AddOffer(&offer))
			return false;

		if(!PutsNewOffer(&offer))
			return false;

		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(offer.dwOffererID);
		if (ecs::PlayerRuntime::IsValid(ch))
			SendChatPacket(ch, CHAT_PACKET_OFFER_CREATE);

		return true;
	}


	void CShopManager::SendShopOfferNotifiedDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_OFFER_NOTIFIED;

		TSubPacketGDOfferNotified subpack;
		subpack.dwOfferID	= dwOfferID;
		subpack.dwOwnerID	= dwOwnerID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopOfferNotifiedDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID)
	{
		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		uint32_t dwBuyer=0;

		//edit offer in shop
		CShop::VECSHOPOFFER* vec = pkShop->GetOffers();
		for (uint32_t i = 0; i < vec->size(); i++)
		{
			TOfferInfo& offer = vec->at(i);
			if (offer.dwOfferID == dwOfferID)
			{
				OFFSHOP_DEBUG("notified offer successful %u , %u ",dwOfferID, dwOwnerID);

				offer.bNoticed = true;
				dwBuyer = offer.dwOffererID;
				break;
			}
		}

		if(dwBuyer==0)
			return false;

		OFFSHOP_DEBUG("searching dwBuyer %u in map",dwBuyer);

		//edit offer in map
		auto it = m_mapOffer.find(dwBuyer);
		if(it==m_mapOffer.end())
			return false;


		OFFSHOP_DEBUG("found buyer successful");

		//searching offer in vector
		CShop::VECSHOPOFFER& vecBuyer = it->second;

		for (auto itVec = vecBuyer.begin(); itVec != vecBuyer.end(); itVec++)
		{
			if(itVec->dwOfferID!=dwOfferID)
				continue;

			OFFSHOP_DEBUG("found offer successful");
			itVec->bNoticed=true;
			break;
		}


		return true;
	}


	void CShopManager::SendShopOfferAcceptDBPacket(const TOfferInfo& offer)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_OFFER_ACCEPT;

		TSubPacketGDOfferNotified subpack;
		subpack.dwOwnerID	= offer.dwOwnerID;
		subpack.dwOfferID	= offer.dwOfferID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	void CShopManager::SendShopOfferCancelDBPacket(const TOfferInfo& offer)
	{

		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_OFFER_CANCEL;

		TSubPacketGDOfferCancel subpack;
		subpack.dwOwnerID	= offer.dwOwnerID;
		subpack.dwOfferID	= offer.dwOfferID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopOfferCancelDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID, bool isRemovingItem) //offlineshop-updated 05/08/19
	{
		OFFSHOP_DEBUG("dwOfferID : %u , dwOwnerID %u ",dwOfferID, dwOwnerID);

		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		CShop::VECSHOPOFFER& vecOffers		= *pkShop->GetOffers();
		CShop::VECSHOPOFFER::iterator it	= vecOffers.begin();

		TOfferInfo * pInfo=nullptr;

		for (; it != vecOffers.end(); it++)
		{
			if (it->dwOfferID == dwOfferID)
			{
				pInfo = &(*it);
				break;
			}
		}

		if(!pInfo)
			return false;

		OFFSHOP_DEBUG("found offer successful : %u ",dwOfferID);


		uint32_t dwBuyerID = pInfo->dwOffererID;
		vecOffers.erase(it);


		auto iter = m_mapOffer.find(dwBuyerID);
		if (iter != m_mapOffer.end())
		{
			OFFSHOP_DEBUG("removing offer from offer vector by buyer %u ",dwBuyerID);

			std::vector<TOfferInfo>& vec = iter->second;
			auto iterVec= vec.begin();

			for (; iterVec != vec.end(); iterVec++)
			{
				if (iterVec->dwOfferID == dwOfferID)
				{
					vec.erase(iterVec);
					break;
				}
			}
		}


		//offlineshop-updated 05/08/19
		if(iter->second.empty())
			m_mapOffer.erase(iter);


		if (!isRemovingItem)
		{
			const entt::entity chOwner = CHARACTER_MANAGER::Instance().FindEntityByPID(dwOwnerID);
			if(ecs::IsCharacter(chOwner) && ecs::OfflineShopSystem::GetOfflineShopGuest(chOwner) && ecs::OfflineShopSystem::GetOfflineShopGuest(chOwner)==ecs::OfflineShopSystem::GetOfflineShop(chOwner))
				SendShopOpenMyShopClientPacket(chOwner);
		}


		const entt::entity chBuyer = CHARACTER_MANAGER::Instance().FindEntityByPID(dwBuyerID);
		if (ecs::IsCharacter(chBuyer) && ecs::OfflineShopSystem::IsLookingOfferList(chBuyer))
			RecvOfferListRequestPacket(chBuyer);


		//end

		return true;
	}


	bool CShopManager::RecvShopOfferAcceptDBPacket(uint32_t dwOfferID, uint32_t dwOwnerID)
	{
		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		CShop::VECSHOPOFFER& vecOffers		= *pkShop->GetOffers();
		CShop::VECSHOPOFFER::iterator it	= vecOffers.begin();

		TOfferInfo * pInfo=nullptr;

		for (; it != vecOffers.end(); it++)
		{
			if (it->dwOfferID == dwOfferID)
			{
				pInfo = &(*it);
				break;
			}
		}

		if(!pInfo)
			return false;

		pkShop->AcceptOffer(pInfo);

		//checking about owner refreshing info
		const entt::entity chOwner = CHARACTER_MANAGER::instance().FindEntityByPID(pkShop->GetOwnerPID());
		if(ecs::IsCharacter(chOwner) && ecs::OfflineShopSystem::GetOfflineShop(chOwner)==pkShop && ecs::OfflineShopSystem::GetOfflineShopGuest(chOwner)==pkShop)
			SendShopOpenMyShopClientPacket(chOwner);


		//removing offer from offer by buyer
		OFFERSMAP::iterator itMap = m_mapOffer.find(pInfo->dwOffererID);
		if (itMap != m_mapOffer.end())
		{
			std::vector<TOfferInfo>& vec = itMap->second;
			auto itVec = vec.begin();

			for (; itVec != vec.end(); itVec++)
			{
				if (itVec->dwOfferID == dwOfferID)
				{
					//checking if buyer was on offerlist
					const entt::entity chBuyer = CHARACTER_MANAGER::instance().FindEntityByPID(itVec->dwOffererID);
					itVec->bAccepted = true;

					if(ecs::IsCharacter(chBuyer) && ecs::OfflineShopSystem::IsLookingOfferList(chBuyer))
						RecvOfferListRequestPacket(chBuyer);

					break;
				}
			}
		}


		return true;
	}


	bool CShopManager::RecvShopSafeboxAddItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID, const TItemInfoEx& item)
	{
		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwOwnerID);
		CShopSafebox* pkSafebox = ecs::IsCharacter(ch) && ecs::OfflineShopSystem::GetShopSafebox(ch) ? ecs::OfflineShopSystem::GetShopSafebox(ch) : GetShopSafeboxByOwnerID(dwOwnerID);

		if(!pkSafebox)
			return false;

		CShopItem shopItem(dwItemID);
		shopItem.SetInfo(item);
		shopItem.SetOwnerID(dwOwnerID);

		pkSafebox->AddItem(&shopItem);
		if(ecs::IsCharacter(ch) && ecs::OfflineShopSystem::GetShopSafebox(ch))
			pkSafebox->RefreshToOwner(ch);

		OFFSHOP_DEBUG("safebox owner %u , item %u ",dwOwnerID, dwItemID);
		return true;
	}


	bool CShopManager::SendShopSafeboxAddItemDBPacket(uint32_t dwOwnerID, const CShopItem& item) {
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader = SUBHEADER_GD_SAFEBOX_ADD_ITEM;


		TSubPacketGDSafeboxAddItem subpack;
		subpack.dwOwnerID = dwOwnerID;
		CopyObject(subpack.item , *item.GetInfo());


		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
		//* add return true
		return true;
	}


	bool CShopManager::RecvShopSafeboxAddValutesDBPacket(uint32_t dwOwnerID, const TValutesInfo& valute)
	{
		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwOwnerID);
		CShopSafebox* pkSafebox = ecs::IsCharacter(ch) && ecs::OfflineShopSystem::GetShopSafebox(ch) ? ecs::OfflineShopSystem::GetShopSafebox(ch) : GetShopSafeboxByOwnerID(dwOwnerID);

		if(!pkSafebox)
			return false;


		pkSafebox->AddValute(valute);
		if(ecs::IsCharacter(ch) && ecs::OfflineShopSystem::GetShopSafebox(ch))
			pkSafebox->RefreshToOwner(ch);
		return true;
	}


	void CShopManager::SendShopSafeboxGetItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_SAFEBOX_GET_ITEM;

		TSubPacketGDSafeboxGetItem subpack;
		subpack.dwOwnerID	= dwOwnerID;
		subpack.dwItemID	= dwItemID;

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		OFFSHOP_DEBUG(" owner % u , item %u ",dwOwnerID , dwItemID);
		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	void CShopManager::SendShopSafeboxGetValutesDBPacket(uint32_t dwOwnerID, const TValutesInfo& valutes)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader	= SUBHEADER_GD_SAFEBOX_GET_VALUTES;

		TSubPacketGDSafeboxGetValutes subpack;
		subpack.dwOwnerID	= dwOwnerID;
		CopyObject(subpack.valute , valutes);

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP, 0, buff.read_peek(), buff.size());
	}


	bool CShopManager::RecvShopSafeboxLoadDBPacket(uint32_t dwOwnerID, const TValutesInfo& valute, const std::vector<uint32_t>& ids, const std::vector<TItemInfoEx>& items)
	{
		/*if(GetShopSafeboxByOwnerID(dwOwnerID))
			return false;*/

		CShopSafebox::VECITEM vec;
		vec.reserve(ids.size());

		for (uint32_t i = 0; i < ids.size(); i++)
		{
			CShopItem item(ids[i]);
			item.SetInfo(items[i]);
			item.SetOwnerID(dwOwnerID);

			vec.push_back(item);
		}


		CShopSafebox safebox;
		safebox.SetItems(&vec);
		safebox.SetValuteAmount(valute);

		m_mapSafeboxs.insert(std::make_pair(dwOwnerID, safebox));
		return true;
	}


	//patch 08-03-2020
	bool CShopManager::RecvShopSafeboxExpiredItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID) {
		CShopSafebox* data = GetShopSafeboxByOwnerID(dwOwnerID);
		if (data) {
			data->RemoveItem(dwItemID);
			data->RefreshToOwner();
		} return true;
	}


	//AUCTION
	void CShopManager::SendAuctionCreateDBPacket(const TAuctionInfo& auction)
	{
		OFFSHOP_DEBUG("auction %u, name %s, duration %u ",auction.dwOwnerID, auction.szOwnerName, auction.dwDuration);

		TPacketGDNewOfflineShop pack;
		pack.bSubHeader = SUBHEADER_GD_AUCTION_CREATE;

		TSubPacketGDAuctionCreate subpack;
		CopyObject(subpack.auction , auction);

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP , 0 , buff.read_peek() , buff.size());
	}


	void CShopManager::SendAuctionAddOfferDBPacket(const TAuctionOfferInfo& offer)
	{
		TPacketGDNewOfflineShop pack;
		pack.bSubHeader = SUBHEADER_GD_AUCTION_ADD_OFFER;

		TSubPacketGDAuctionAddOffer subpack;
		CopyObject(subpack.offer , offer);

		TEMP_BUFFER buff;
		buff.write(&pack, sizeof(pack));
		buff.write(&subpack, sizeof(subpack));

		db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP , 0 , buff.read_peek() , buff.size());
	}


	bool CShopManager::RecvAuctionCreateDBPacket(const TAuctionInfo& auction)
	{
		OFFSHOP_DEBUG("auction %u, name %s, duration %u ",auction.dwOwnerID, auction.szOwnerName, auction.dwDuration);

		//check if exist
		if(m_mapAuctions.find(auction.dwOwnerID) != m_mapAuctions.end())
			return false;

		//set info
		CAuction& obj = m_mapAuctions[auction.dwOwnerID];
		obj.SetInfo(auction);

		//check about owner
		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(auction.dwOwnerID);
		if (ecs::IsCharacter(ch))
		{
			ecs::OfflineShopSystem::SetAuction(ch, &obj);
			SendAuctionOpenAuctionClientPacket(ch, obj.GetInfo(), std::vector<TAuctionOfferInfo>());
		}

		return true;
	}


	bool CShopManager::RecvAuctionAddOfferDBPacket(const TAuctionOfferInfo& offer)
	{
		OFFSHOP_DEBUG("offer : %u auction, %u buyer",offer.dwOwnerID, offer.dwBuyerID);

		//check if exists
		auto it = m_mapAuctions.find(offer.dwOwnerID);
		if(it ==m_mapAuctions.end())
			return false;

		//adding offer
		CAuction& obj = it->second;
		obj.AddOffer(offer);

		if (obj.GetInfo().dwDuration == 0)
			obj.IncreaseDuration();
		return true;
	}


	//updated 30/09/19
	bool CShopManager::RecvAuctionExpiredDBPacket(uint32_t dwID)
	{
		OFFSHOP_DEBUG("id : %u",dwID);

		//temp container to kick guest
		CShop::LISTGUEST tempGuestList;

		//removing auction from map
		auto it = m_mapAuctions.find(dwID);
		if (it != m_mapAuctions.end())
		{
			CAuction& auct = it->second;

			OFFSHOP_DEBUG("found auction %u (guest count %u) ",dwID, auct.GetGuests().size());

			//set to null the character::auctionguest pointer
			CShop::LISTGUEST& guestList = auct.GetGuests();
			for (auto itGuest = guestList.begin(); itGuest != guestList.end(); itGuest++)
			{
				const entt::entity chGuest = CHARACTER_MANAGER::instance().FindEntityByPID(*itGuest);
				if (!ecs::IsCharacter(chGuest)) {
					continue;
				}
				ecs::OfflineShopSystem::SetAuctionGuest(chGuest, nullptr);

				OFFSHOP_DEBUG("removing guest from auction %s ", ecs::PlayerRuntime::GetName(chGuest).data());
				tempGuestList.push_back(ecs::PlayerRuntime::GetPlayerID(chGuest));
			}

			m_mapAuctions.erase(it);
		}

		uint32_t dwOwnerID = 0;
		const entt::entity owner = CHARACTER_MANAGER::instance().FindEntityByPID(dwID);

		if(ecs::IsCharacter(owner)) {
			dwOwnerID = (ecs::PlayerRuntime::GetPlayerID(owner));
			RecvAuctionListRequestClientPacket(owner, true);
			ecs::OfflineShopSystem::SetAuction(owner, nullptr);
		}

		//updated 30/08/19
		for (auto itGuests = tempGuestList.begin(); itGuests != tempGuestList.end(); itGuests++)
		{
			const entt::entity chGuest = CHARACTER_MANAGER::instance().FindEntityByPID(*itGuests);
			if (!ecs::IsCharacter(chGuest)) {
				continue;
			}

			if ((dwOwnerID != 0) && dwOwnerID == (ecs::PlayerRuntime::GetPlayerID(chGuest))) {
				continue;
			}

			RecvAuctionListRequestClientPacket(chGuest);
		}

		return false;
	}


	//client packets exchanging
	bool CShopManager::RecvShopCreateNewClientPacket(entt::entity character, TShopInfo& rShopInfo, std::vector<TShopItemInfo> & vec)
	{
		if(!ecs::IsCharacter(character) || ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		if (!InventorySystem::CanHandleItems(character) || !CheckCharacterActions(character))
		{
			SendChatPacket(character,CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}

#ifdef KASMIR_PAKET_SYSTEM
		if ((rShopInfo.dwKasmirNpc < 30000) || (rShopInfo.dwKasmirNpc > 30007))
			return false;

		if ((rShopInfo.dwKasmirNpc != 30000) && (ItemSystem::CountItem(character, 88902) < 1)) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 45, "");
#endif
			return false;
		}
#endif

		OFFSHOP_DEBUG("ch name %s , item count %u , duration %u ", ecs::PlayerRuntime::GetName(character).data(), rShopInfo.dwCount, rShopInfo.dwDuration);

		static char szNameChecked[OFFLINE_SHOP_NAME_MAX_LEN];

		//cheching about bandword
		strncpy(szNameChecked, rShopInfo.szName, sizeof(szNameChecked));
		if (CBanwordManager::instance().CheckString(szNameChecked, strlen(szNameChecked)))
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		//making full name
		snprintf(rShopInfo.szName, sizeof(rShopInfo.szName), "%s@%s" , ecs::PlayerRuntime::GetName(character).data(), szNameChecked );

		std::vector<TItemInfo> vecItem;
		vecItem.reserve(vec.size());

		rShopInfo.dwOwnerID = (ecs::PlayerRuntime::GetPlayerID(character));
		TItemInfo itemInfo;

#ifdef KASMIR_PAKET_SYSTEM
		uint32_t dwCountStyle1 = ItemSystem::CountItem(character, 88902);
		uint32_t dwCountStyle2 = 0;
#endif

		for (uint32_t i = 0; i < vec.size(); i++)
		{
			TShopItemInfo& rShopItem = vec[i];

			const entt::entity item = ItemSystem::GetItem(character, rShopItem.pos);
			if (!ItemSystem::IsValidItem(item))
				return false;

			if(IS_SET(ItemSystem::GetItemAntiFlag(item), ITEM_ANTIFLAG_GIVE))
				return false;

			if(IS_SET(ItemSystem::GetItemAntiFlag(item), ITEM_ANTIFLAG_MYSHOP))
				return false;

			if (ItemSystem::IsItemLocked(item) || ItemSystem::IsItemEquipped(item) || ItemSystem::IsItemExchanging(item))
			{
				SendChatPacket(character,CHAT_PACKET_CANNOT_DO_NOW);
				return true;
			}

#ifdef ENABLE_SOULBIND_SYSTEM
			if (ItemSystem::IsItemBound(item)){
				SendChatPacket(character, CHAT_PACKET_CANNOT_DO_NOW);
				return true;
			}
#endif

#ifdef KASMIR_PAKET_SYSTEM
			if (ItemSystem::GetItemVnum(item) == 88902) {
				dwCountStyle2 += 1;
			}
#endif

			//checking about double insert same item
			for (uint32_t j = 0; j < vec.size(); j++)
			{
				if(i==j)
					continue;

				TShopItemInfo& rShopItemCheck = vec[j];
				if(rShopItemCheck.pos == rShopItem.pos)
					return false;
			}

			ZeroObject(itemInfo);

			itemInfo.dwOwnerID = (ecs::PlayerRuntime::GetPlayerID(character));
			for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
			itemInfo.item.aAttr[attr] = ItemSystem::GetItemAttribute(item, attr);
			for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			itemInfo.item.alSockets[socket] = ItemSystem::GetItemSocket(item, socket);

			itemInfo.item.dwVnum	= ItemSystem::GetItemVnum(item);
			itemInfo.item.dwCount	= ItemSystem::GetItemCount(item);
			//patch 08-03-2020
			itemInfo.item.expiration = GetItemExpiration(item);

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
			itemInfo.item.dwTransmutation = ItemSystem::GetItemTransmutationVnum(item);
#endif
#ifdef ATTR_LOCK
			itemInfo.item.iLockedAttr = ItemSystem::GetItemLockedAttr(item);
#endif
#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
			LogManager::instance().OfflineshopLog((ecs::PlayerRuntime::GetPlayerID(character)), 0, "trying to open shop , adding item vnum %u count %u original id %u ", itemInfo.item.dwVnum, itemInfo.item.dwCount, ItemSystem::GetItemID(item));
#endif
			CopyObject(itemInfo.price, rShopItem.price);
			vecItem.push_back(itemInfo);
		}

#ifdef KASMIR_PAKET_SYSTEM
		if ((rShopInfo.dwKasmirNpc != 30000) && (dwCountStyle1 <= dwCountStyle2)) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 45, "");
#endif
			return false;
		}
#endif

		for (uint32_t i = 0; i < vec.size(); i++)
		{
			TShopItemInfo& rShopItem = vec[i];
			const entt::entity item = ItemSystem::GetItem(character, rShopItem.pos);
			const entt::entity removed = InventorySystem::RemoveFromCharacter(item);
		ItemSystem::DestroyItemEntityEcs(removed, "OFFLINESHOP_SELL");
		}

		OFFSHOP_DEBUG("ch name %s , checked successful , send to db ", ecs::PlayerRuntime::GetName(character).data());


		rShopInfo.dwDuration = MIN(rShopInfo.dwDuration , OFFLINESHOP_DURATION_MAX_MINUTES);
		SendShopCreateNewDBPacket(rShopInfo, vecItem);

#ifdef KASMIR_PAKET_SYSTEM
		if (rShopInfo.dwKasmirNpc != 30000)
			ItemSystem::RemoveSpecifyItemEcs(character, 88902, 1);
#endif

		return true;
	}

	bool CShopManager::RecvShopChangeNameClientPacket(entt::entity character, const char* szName)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

		static char szNameChecked[OFFLINE_SHOP_NAME_MAX_LEN];
		static char szFullName[OFFLINE_SHOP_NAME_MAX_LEN];

		//cheching about bandword
		strncpy(szNameChecked, szName, sizeof(szNameChecked));
		if (CBanwordManager::instance().CheckString(szNameChecked, strlen(szNameChecked)))
			return false;

		//making full name
		snprintf(szFullName, sizeof(szFullName), "%s@%s" , ecs::PlayerRuntime::GetName(character).data(), szNameChecked );
#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
		LogManager::instance().OfflineshopLog((ecs::PlayerRuntime::GetPlayerID(character)), 0, "changing shop name : %s -> %s ", ecs::OfflineShopSystem::GetOfflineShop(character)->GetName(), szFullName);
#endif

		SendShopChangeNameDBPacket((ecs::PlayerRuntime::GetPlayerID(character)), szFullName);
		return true;
	}


	bool CShopManager::RecvShopForceCloseClientPacket(entt::entity character)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
		LogManager::instance().OfflineshopLog((ecs::PlayerRuntime::GetPlayerID(character)), 0, "player asked to close shop (remain items count %u) ", ecs::OfflineShopSystem::GetOfflineShop(character)->GetItems()->size());
#endif

		ecs::OfflineShopSystem::SetUseTime(character);
		SendShopForceCloseDBPacket((ecs::PlayerRuntime::GetPlayerID(character)));
		return true;
	}


	bool CShopManager::RecvShopRequestListClientPacket(entt::entity character)
	{
		if(!ecs::PlayerRuntime::GetDesc(character))
			return false;

		SendShopListClientPacket(character);
		return true;
	}


	bool CShopManager::RecvShopOpenClientPacket(entt::entity character, uint32_t dwOwnerID)
	{
		if(!ecs::IsCharacter(character) || !ecs::PlayerRuntime::GetDesc(character))
			return false;

		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		if (ecs::OfflineShopSystem::GetOfflineShopGuest(character))
			ecs::OfflineShopSystem::GetOfflineShopGuest(character)->RemoveGuest(character);

		//offlineshop-updated 04/08/19
		if((ecs::PlayerRuntime::GetPlayerID(character)) == dwOwnerID)
			SendShopOpenMyShopClientPacket(character);
		else
			SendShopOpenClientPacket(character, pkShop);


		pkShop->AddGuest(character);
		ecs::OfflineShopSystem::SetOfflineShopGuest(character, pkShop);
		return true;
	}


	bool CShopManager::RecvShopOpenMyShopClientPacket(entt::entity character)
	{
		if(!ecs::IsCharacter(character) || !ecs::PlayerRuntime::GetDesc(character))
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		if (!ecs::OfflineShopSystem::GetOfflineShop(character))
		{
			SendShopOpenMyShopNoShopClientPacket(character);
		}


		else
		{
			SendShopOpenMyShopClientPacket(character);
			ecs::OfflineShopSystem::GetOfflineShop(character)->AddGuest(character);
			ecs::OfflineShopSystem::SetOfflineShopGuest(character, ecs::OfflineShopSystem::GetOfflineShop(character));
		}


		return true;
	}

	bool CShopManager::RecvShopBuyItemClientPacket(entt::entity character, uint32_t dwOwnerID, uint32_t dwItemID, bool isSearch, int64_t TotalPriceSeen)
	{
		OFFSHOP_DEBUG("owner %u , item id %u ", dwOwnerID, dwItemID);

		if(!ecs::IsCharacter(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		if (!InventorySystem::CanHandleItems(character) || !CheckCharacterActions(character))
		{
			SendChatPacket(character,CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}

		CShop* pkShop=nullptr;
		if(!(pkShop=GetShopByOwnerID(dwOwnerID)))
			return false;

		CShopItem* pitem=nullptr;
		if(!pkShop->GetItem(dwItemID, &pitem))
			return false;

		if(!pitem->CanBuy(character))
			return false;

		if (pitem->GetPrice()->GetTotalYangAmount() != TotalPriceSeen)
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		OFFSHOP_DEBUG("sending packet to db (buyer %u , owner %u , item %u )",(ecs::PlayerRuntime::GetPlayerID(character)) , dwOwnerID, dwItemID);

		if(isSearch)
			SendShopBuyItemFromSearchClientPacket(character, dwOwnerID, dwItemID);

		SendShopLockBuyItemDBPacket((ecs::PlayerRuntime::GetPlayerID(character)), dwOwnerID, dwItemID, TotalPriceSeen);
		return true;
	}


#ifdef ENABLE_NEW_SHOP_IN_CITIES
	bool CShopManager::RecvShopClickEntity(entt::entity character, uint32_t dwShopEntityVID)
	{
		const auto entity = ecs::OfflineShopEntityRegistry::FindByVID(dwShopEntityVID);
		if (entity != entt::null)
			for (const auto& city : m_vecCities)
			{
				const auto it = city.entitiesByVID.find(dwShopEntityVID);
				if (it == city.entitiesByVID.end() || it->second != entity)
					continue;
				const auto* state = g_registry.try_get<ecs::OfflineShopState>(entity);
				if (!state)
					break;
				const auto ownerPID = state->ownerPID;
				const auto owner = city.entitiesByPID.find(ownerPID);
				if (owner == city.entitiesByPID.end() || owner->second != entity ||
					!GetShopByOwnerID(ownerPID))
					break;
				RecvShopOpenClientPacket(character, ownerPID);
				return true;
			}

		LOG_ERROR("cannot found clicked entity , {} vid {} ", ecs::PlayerRuntime::GetName(character).data(), dwShopEntityVID);
		return false;
	}
#endif


	void CShopManager::SendShopListClientPacket(entt::entity character)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TEMP_BUFFER buff;
		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_LIST;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopList) + (m_mapShops.size() * sizeof(TShopInfo)) ;

		buff.write(&pack, sizeof(pack));

		TSubPacketGCShopList subPack;
		subPack.dwShopCount = m_mapShops.size();
		buff.write(&subPack, sizeof(subPack));


		TShopInfo shopInfo;

		auto it=m_mapShops.begin();
		for (; it != m_mapShops.end(); it++)
		{
			const CShop& rShop	= it->second;
			uint32_t dwOwner		= it->first;

			ZeroObject(shopInfo);

			shopInfo.dwCount	= rShop.GetItems()->size();
			shopInfo.dwDuration	= rShop.GetDuration();
			shopInfo.dwOwnerID	= dwOwner;
			strncpy(shopInfo.szName, rShop.GetName(), sizeof(shopInfo.szName));

			buff.write(&shopInfo, sizeof(shopInfo));
		}

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek() , buff.size());
	}


	void CShopManager::SendShopOpenClientPacket(entt::entity character, CShop* pkShop)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		CShop::VECSHOPITEM* pVec = pkShop->GetItems();
		TEMP_BUFFER buff;
		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_OPEN;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopOpen) + sizeof(TItemInfo)*pVec->size();

		buff.write(&pack, sizeof(pack));


		TSubPacketGCShopOpen subPack;
		subPack.shop.dwCount	= pVec->size();
		subPack.shop.dwDuration	= pkShop->GetDuration();
		subPack.shop.dwOwnerID	= pkShop->GetOwnerPID();
		strncpy(subPack.shop.szName, pkShop->GetName(), sizeof(subPack.shop.szName));

		buff.write(&subPack, sizeof(subPack));

		TItemInfo itemInfo;

		for (uint32_t i = 0; i < pVec->size(); i++)
		{
			CShopItem& rItem = pVec->at(i);
			ZeroObject(itemInfo);

			itemInfo.dwItemID	= rItem.GetID();
			itemInfo.dwOwnerID	= pkShop->GetOwnerPID();
			CopyObject(itemInfo.item, *(rItem.GetInfo()));
			CopyObject(itemInfo.price,*(rItem.GetPrice()));

			buff.write(&itemInfo, sizeof(itemInfo));
		}

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());
	}


	void CShopManager::SendShopOpenMyShopNoShopClientPacket(entt::entity character)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_OPEN_OWNER_NO_SHOP;
		pack.wSize		= sizeof(pack);


		ecs::PlayerRuntime::GetDesc(character)->Packet(&pack, sizeof(pack));
	}

	void CShopManager::SendShopBuyItemFromSearchClientPacket(entt::entity character, uint32_t dwOwnerID, uint32_t dwItemID)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_BUY_ITEM_FROM_SEARCH;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopBuyItemFromSearch);

		TSubPacketGCShopBuyItemFromSearch subpack;
		subpack.dwOwnerID = dwOwnerID;
		subpack.dwItemID  = dwItemID;

		TEMP_BUFFER buff;
		buff.write(&pack,		sizeof(pack));
		buff.write(&subpack,	sizeof(subpack));

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());
	}


	void CShopManager::SendShopOpenMyShopClientPacket(entt::entity character)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		if(!ecs::OfflineShopSystem::GetOfflineShop(character))
			return;

		CShop* pkShop	= ecs::OfflineShopSystem::GetOfflineShop(character);
		uint32_t dwOwnerID	= (ecs::PlayerRuntime::GetPlayerID(character));

		CShop::VECSHOPITEM*  pVec		= pkShop->GetItems();
		CShop::VECSHOPITEM*  pVecSold	= pkShop->GetItemsSold();
		CShop::VECSHOPOFFER* pVecOffer	= pkShop->GetOffers();

		TEMP_BUFFER buff;
		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_OPEN_OWNER;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopOpenOwner) + sizeof(TItemInfo)*pVec->size() + sizeof(TItemInfo)* pVecSold->size() + sizeof(TOfferInfo)*pVecOffer->size();

		buff.write(&pack, sizeof(pack));


		TSubPacketGCShopOpenOwner subPack;
		subPack.shop.dwCount	= pVec->size();
		subPack.shop.dwDuration	= pkShop->GetDuration();
		subPack.shop.dwOwnerID	= dwOwnerID;
		subPack.dwSoldCount		= pVecSold->size();
		subPack.dwOfferCount	= pVecOffer->size();

		strncpy(subPack.shop.szName, pkShop->GetName(), sizeof(subPack.shop.szName));


		OFFSHOP_DEBUG("owner %u , item count %u , duration %u offer count %u ",subPack.shop.dwOwnerID, subPack.shop.dwCount , subPack.shop.dwDuration, subPack.dwOfferCount);


		buff.write(&subPack, sizeof(subPack));

		TItemInfo itemInfo;

		for (uint32_t i = 0; i < pVec->size(); i++)
		{
			CShopItem& rItem = pVec->at(i);
			ZeroObject(itemInfo);

			itemInfo.dwItemID	= rItem.GetID();
			itemInfo.dwOwnerID	= dwOwnerID;
			CopyObject(itemInfo.item, *(rItem.GetInfo()));
			CopyObject(itemInfo.price,*(rItem.GetPrice()));

			OFFSHOP_DEBUG("item id %u , item vnum %u , item count %u ",itemInfo.dwItemID, itemInfo.item.dwVnum , itemInfo.item.dwCount);
			buff.write(&itemInfo, sizeof(itemInfo));
		}


		for (uint32_t i = 0; i < pVecSold->size(); i++)
		{
			CShopItem& rItem = pVecSold->at(i);
			ZeroObject(itemInfo);

			itemInfo.dwItemID	= rItem.GetID();
			itemInfo.dwOwnerID	= dwOwnerID;
			CopyObject(itemInfo.item, *(rItem.GetInfo()));
			CopyObject(itemInfo.price,*(rItem.GetPrice()));

			OFFSHOP_DEBUG("item id %u , item vnum %u , item count %u ",itemInfo.dwItemID, itemInfo.item.dwVnum , itemInfo.item.dwCount);
			buff.write(&itemInfo, sizeof(itemInfo));
		}


		//writing offer vector (no need to convert to some other object)
		if(!pVecOffer->empty())
			buff.write(&pVecOffer->at(0), sizeof(TOfferInfo) * pVecOffer->size());

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());


		for (auto it = pVecOffer->begin(); it != pVecOffer->end(); it++)
		{
			TOfferInfo& offer = *it;
			if (!offer.bAccepted && !offer.bNoticed)
				SendShopOfferNotifiedDBPacket(offer.dwOfferID, offer.dwOwnerID);
		}
	}


	void CShopManager::SendShopForceClosedClientPacket(uint32_t dwOwnerID)
	{
		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwOwnerID);
		const entt::entity chEntity = ch;

		if(ch == entt::null || !ecs::PlayerRuntime::GetDesc(chEntity))
			return;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_OPEN_OWNER;

		pack.wSize = sizeof(pack);
		ecs::PlayerRuntime::GetDesc(chEntity)->Packet(&pack , sizeof(pack));
	}


	//ITEMS
	bool CShopManager::RecvShopAddItemClientPacket(entt::entity character, const TItemPos& pos, const TPriceInfo& price)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		if (!InventorySystem::CanHandleItems(character) || !CheckCharacterActions(character))
		{
			SendChatPacket(character,CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}


		const entt::entity pkItem = ItemSystem::GetItem(character, pos);
		if (!ItemSystem::IsValidItem(pkItem))
			return false;

		if (ItemSystem::IsItemLocked(pkItem) || ItemSystem::IsItemEquipped(pkItem) || ItemSystem::IsItemExchanging(pkItem))
		{
			SendChatPacket(character,CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}

#ifdef ENABLE_SOULBIND_SYSTEM
		if (ItemSystem::IsItemBound(pkItem)) {
			SendChatPacket(character, CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}
#endif

		if(IS_SET(ItemSystem::GetItemAntiFlag(pkItem), ITEM_ANTIFLAG_GIVE))
			return false;
		if(IS_SET(ItemSystem::GetItemAntiFlag(pkItem), ITEM_ANTIFLAG_MYSHOP))
			return false;

//updated 25 - 01 - 2020  //topatch
#if !defined(__ENABLE_FULL_YANG__) && !defined(ENABLE_FULL_YANG) && !defined(REMOVE_YANG_LIMIT)
		if (!IsGoodSalePrice(price))
			return false;
#endif

		ecs::OfflineShopSystem::SetUseTime(character);

		TItemInfo itemInfo;
		ZeroObject(itemInfo);

		itemInfo.dwOwnerID		= (ecs::PlayerRuntime::GetPlayerID(character));
		itemInfo.item.dwVnum	= ItemSystem::GetItemVnum(pkItem);
		itemInfo.item.dwCount	= ItemSystem::GetItemCount(pkItem);
		//patch 08-03-2020
		itemInfo.item.expiration = GetItemExpiration(pkItem);

		for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
			itemInfo.item.aAttr[attr] = ItemSystem::GetItemAttribute(pkItem, attr);
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			itemInfo.item.alSockets[socket] = ItemSystem::GetItemSocket(pkItem, socket);

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		itemInfo.item.dwTransmutation = ItemSystem::GetItemTransmutationVnum(pkItem);
#endif
#ifdef ATTR_LOCK
		itemInfo.item.iLockedAttr = ItemSystem::GetItemLockedAttr(pkItem);
#endif
		CopyObject(itemInfo.price, price);

#ifdef ENABLE_NEW_OFFLINESHOP_LOGS
		LogManager::instance().OfflineshopLog((ecs::PlayerRuntime::GetPlayerID(character)), 0, "adding new item to the shop vnum %u count %u (original item ID %u) ", itemInfo.item.dwVnum, itemInfo.item.dwCount, ItemSystem::GetItemID(pkItem));
#endif

		const entt::entity removed = InventorySystem::RemoveFromCharacter(pkItem);
		ItemSystem::DestroyItemEntityEcs(removed, "OFFLINESHOP_SELL");

		SendShopAddItemDBPacket((ecs::PlayerRuntime::GetPlayerID(character)), itemInfo);
		return true;
	}


	bool CShopManager::RecvShopRemoveItemClientPacket(entt::entity character, uint32_t dwItemID)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		CShop* pkShop		= ecs::OfflineShopSystem::GetOfflineShop(character);
		CShopItem* pItem = nullptr;

		OFFSHOP_DEBUG("owner %u , item id %u ",(ecs::PlayerRuntime::GetPlayerID(character)) , dwItemID);

		if (pkShop->GetItems()->size() == 1)
		{
			SendChatPacket(character, CHAT_PACKET_CANNOT_REMOVE_LAST_ITEM);
			return false;
		}

		if(!pkShop->GetItem(dwItemID, &pItem))
			return false;

		SendShopRemoveItemDBPacket(pkShop->GetOwnerPID(), pItem->GetID());
		return true;
	}


	bool CShopManager::RecvShopEditItemDBPacket(uint32_t dwOwnerID, uint32_t dwItemID, const TPriceInfo& rPrice)
	{
		CShop* pkShop = GetShopByOwnerID(dwOwnerID);
		if (!pkShop)
			return false;

		CShopItem* pItem = nullptr;
		if (!pkShop->GetItem(dwItemID, &pItem))
			return false;

		OFFSHOP_DEBUG("owner id %u , item id %u ", dwOwnerID, dwItemID);

		CShopItem newItem(*pItem);
		newItem.SetPrice(rPrice);

		pkShop->ModifyItem(dwItemID, newItem);
		return true;
	}


	//FILTER
	bool CShopManager::RecvShopFilterRequestClientPacket(entt::entity character, const TFilterInfo& filter)
	{
		if(!ecs::IsCharacter(character))
			return false;

		//offlineshop-updated 03/08/2019
		std::vector<TItemInfo> vec;

		if (!CheckSearchTime((ecs::PlayerRuntime::GetPlayerID(character))))
		{
			SendShopFilterResultClientPacket(character, vec);
			SendChatPacket(character,CHAT_PACKET_CANNOT_SEARCH_YET);
			return true;
		}

		ecs::OfflineShopSystem::SetUseTime(character);

		//std::vector<TItemInfo> vec;
		std::string stName = StringToLower(filter.szName, strnlen(filter.szName, sizeof(filter.szName)));


		auto cit= m_mapShops.begin();
		for (; cit != m_mapShops.end(); cit++)
		{
			const CShop& rcShop = cit->second;

			//offlineshop-updated 04/08/19
			if(rcShop.GetOwnerPID() == (ecs::PlayerRuntime::GetPlayerID(character)))
				continue;


			CShop::VECSHOPITEM* pShopItems = rcShop.GetItems();

			auto cItemIter = pShopItems->begin();
			for (; cItemIter != pShopItems->end(); cItemIter++)
			{
				const CShopItem&	rItem		= *cItemIter;
				const TItemInfoEx&	rItemInfo	= *rItem.GetInfo();
				const TPriceInfo&	rItemPrice	= *rItem.GetPrice();

				TItemTable* pTable = ITEM_MANAGER::instance().GetTable(rItemInfo.dwVnum);
				if (!pTable)
				{
					LOG_ERROR("CANNOT FIND ITEM TABLE [{}]");
					continue;
				}

				if(filter.bType != ITEM_NONE && filter.bType != pTable->bType)
					continue;

				if(filter.bType != ITEM_NONE && filter.bSubType != SUBTYPE_NOSET && filter.bSubType != pTable->bSubType)
					continue;

#ifdef ENABLE_RARITY_SYSTEM
				if (filter.iRarity != -1)
				{
					TItemExtraProto* ExtraProto = ITEM_MANAGER::instance().GetExtraProto(rItemInfo.dwVnum);
					if (ExtraProto && ExtraProto->iRarity != filter.iRarity)
						continue;

					if (filter.iRarity != 0 && !ExtraProto)
						continue;
				}
#endif

				int iLimitLevel = pTable->aLimits[0].bType == LIMIT_LEVEL?pTable->aLimits[0].lValue : pTable->aLimits[1].bType == LIMIT_LEVEL? pTable->aLimits[1].lValue : 0;


				if ((filter.iLevelStart != 0 || filter.iLevelEnd != 0))
				{
					if(iLimitLevel == 0)
						continue;

					if(iLimitLevel < filter.iLevelStart && filter.iLevelStart!=0)
						continue;

					if(iLimitLevel > filter.iLevelEnd && filter.iLevelEnd!=0)
						continue;
				}


				if(filter.priceStart.illYang != 0)
					if(GetTotalAmountFromPrice(rItemPrice) < filter.priceStart.illYang)
						continue;

				if(filter.priceEnd.illYang != 0)
					if(GetTotalAmountFromPrice(rItemPrice) > filter.priceEnd.illYang)
						continue;


#ifdef ENABLE_MULTI_NAMES
				if(strnlen(filter.szName, sizeof(filter.szName)) != 0 && !MatchItemName(stName , pTable->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()] , strnlen(pTable->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()], ITEM_NAME_MAX_LEN)))
#else
				if(strnlen(filter.szName, sizeof(filter.szName)) != 0 && !MatchItemName(stName , pTable->szLocaleName , strnlen(pTable->szLocaleName, ITEM_NAME_MAX_LEN)))
#endif
					continue;


				if(!MatchWearFlag(filter.dwWearFlag, pTable->dwAntiFlags))
					continue;


				if(!MatchAttributes(filter.aAttr, rItemInfo.aAttr))
					continue;

				TItemInfo itemInfo;
				CopyObject(itemInfo.item, rItemInfo);
				CopyObject(itemInfo.price,rItemPrice);

				itemInfo.dwItemID	= rItem.GetID();
				itemInfo.dwOwnerID	= rcShop.GetOwnerPID();

				vec.push_back(itemInfo);

				if(vec.size() >= OFFLINESHOP_MAX_SEARCH_RESULT)
					break;
			}

			if(vec.size() >= OFFLINESHOP_MAX_SEARCH_RESULT)
				break;
		}


		SendShopFilterResultClientPacket(character, vec);
		return true;
	}


	void CShopManager::SendShopFilterResultClientPacket(entt::entity character, const std::vector<TItemInfo>& items)
	{
		if(!ecs::PlayerRuntime::GetDesc(character))
			return;

		TEMP_BUFFER buff;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_SHOP_FILTER_RESULT;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopFilterResult) + sizeof(TItemInfo)*items.size();
		buff.write(&pack, sizeof(pack));

		TSubPacketGCShopFilterResult subpack;
		subpack.dwCount	= items.size();
		buff.write(&subpack, sizeof(subpack));

		for (uint32_t i = 0; i < items.size(); i++)
		{
			const TItemInfo& rItemInfo= items[i];
			buff.write(&rItemInfo, sizeof(rItemInfo));
		}


		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());
	}


	//OFFERS
	bool CShopManager::RecvShopCreateOfferClientPacket(entt::entity character, TOfferInfo& offer)
	{
		if(!ecs::IsCharacter(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		//offlineshop-updated 03/08/19
		if((ecs::PlayerRuntime::GetPlayerID(character)) == offer.dwOwnerID)
			return false;

		//offlineshop-updated 04/08/19
		CShop* pkShop = GetShopByOwnerID(offer.dwOwnerID);
		if(!pkShop)
			return false;

		// fix flooding offers
		if (!CheckOfferCooldown((ecs::PlayerRuntime::GetPlayerID(character))))
			return false;

		CShopItem* item = nullptr;
		if(!pkShop->GetItem(offer.dwItemID, &item))
			return false;


#ifndef __ENABLE_CHEQUE_SYSTEM__
		//if (offer.price.illYang == 0)
		if (offer.price.illYang <= 0)
			return false;
#else
		//if (offer.price.illYang == 0 && offer.price.iCheque <= 0)
		if (offer.price.illYang <= 0 && offer.price.iCheque <= 0)
			return false;
#endif

		if(ecs::PointSystem::GetGold(character) < offer.price.illYang)
			return false;

#ifdef __ENABLE_CHEQUE_SYSTEM__
		if (ecs::PointSystem::GetReal(character, POINT_CHEQUE) < offer.price.iCheque)
			return false;
#endif

		ecs::OfflineShopSystem::SetUseTime(character);

		ecs::PointSystem::Change(character, POINT_GOLD, -offer.price.illYang);
#ifdef __ENABLE_CHEQUE_SYSTEM__
		ecs::PointSystem::Change(character, POINT_CHEQUE, -offer.price.iCheque);

		// converting won to yang
		offer.price.illYang = offer.price.GetTotalYangAmount();
		offer.price.iCheque =0;
#endif

		offer.bAccepted		= false;
		offer.bNoticed		= false;
		offer.dwOffererID	= (ecs::PlayerRuntime::GetPlayerID(character));

		//offlineshop-updated 03/08/19
		strncpy(offer.szBuyerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(offer.szBuyerName));

		SendShopOfferNewDBPacket(offer);
		return true;
	}


	bool CShopManager::RecvShopEditOfferClientPacket(entt::entity character, const TOfferInfo& offer)
	{
		if(!ecs::IsCharacter(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		//next feature
		return true;
	}


	bool CShopManager::RecvShopAcceptOfferClientPacket(entt::entity character, uint32_t dwOfferID)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetOfflineShop(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		CShop* pkShop	= ecs::OfflineShopSystem::GetOfflineShop(character);
		CShopItem* item	= nullptr;

		TOfferInfo* info = nullptr;
		CShop::VECSHOPOFFER& vec = *(pkShop->GetOffers());
		for (uint32_t i = 0; i < vec.size(); i++)
		{
			if (dwOfferID == vec[i].dwOfferID)
			{
				info = &vec[i];
				break;
			}
		}


		if(!info || info->bAccepted)
			return false;


		if((ecs::PlayerRuntime::GetPlayerID(character)) != info->dwOwnerID)
			return false;


		if(!pkShop->GetItem(info->dwItemID, &item))
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		info->bAccepted = true;
		SendShopOfferAcceptDBPacket(*info);
		return true;
	}


	bool CShopManager::RecvShopCancelOfferClientPacket(entt::entity character, uint32_t dwOfferID, uint32_t dwOwnerID)
	{
		if(!ecs::IsCharacter(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		CShop* pkShop	= GetShopByOwnerID(dwOwnerID);
		if(!pkShop)
			return false;


		CShopItem* item	= nullptr;

		TOfferInfo* info = nullptr;
		CShop::VECSHOPOFFER& vec = *(pkShop->GetOffers());
		for (uint32_t i = 0; i < vec.size(); i++)
		{
			if (dwOfferID == vec[i].dwOfferID)
			{
				info = &vec[i];
				break;
			}
		}


		if(!info || info->bAccepted)
			return false;


		if(!pkShop->GetItem(info->dwItemID, &item))
			return false;


		if((ecs::PlayerRuntime::GetPlayerID(character)) != pkShop->GetOwnerPID() && (ecs::PlayerRuntime::GetPlayerID(character)) != info->dwOffererID)
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		OFFSHOP_DEBUG("success %u offer , %u shop ", dwOfferID, dwOwnerID);
		SendShopOfferCancelDBPacket(*info);
		return true;
	}


	bool CShopManager::RecvOfferListRequestPacket(entt::entity character) //offlineshop-updated 03/08/19
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return false;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_OFFER_LIST;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopOfferList);

		TSubPacketGCShopOfferList subpack;
		subpack.dwOfferCount = 0;

		TEMP_BUFFER buff;


		OFFERSMAP::iterator it = m_mapOffer.find((ecs::PlayerRuntime::GetPlayerID(character)));
		if (it == m_mapOffer.end() || it->second.empty())
		{
			buff.write(&pack, sizeof(pack));
			buff.write(&subpack, sizeof(subpack));

			OFFSHOP_DEBUG("return because not found or empty vec : found > %s  (id %u) ",it!=m_mapOffer.end()?"TRUE":"FALSE" , (ecs::PlayerRuntime::GetPlayerID(character)));

			ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek() , buff.size());
			return true;
		}


		const std::vector<TOfferInfo>& vec = it->second;
		pack.wSize += sizeof(TOfferInfo)*vec.size();

		std::vector<TMyOfferExtraInfo> extrainfo;
		extrainfo.resize(vec.size());
		subpack.dwOfferCount = vec.size();

		OFFSHOP_DEBUG("found %u in map, size %u ",(ecs::PlayerRuntime::GetPlayerID(character)), vec.size());

		for (uint32_t i = 0; i < vec.size(); i++)
		{
			const TOfferInfo& offer = vec[i];
			CShop* pkShop = GetShopByOwnerID(offer.dwOwnerID);
			if (!pkShop)
			{
				LOG_ERROR("cannot find item's shop {} , offer id {} ", offer.dwOwnerID, offer.dwOfferID);
				return false;
			}

			CShopItem* pkItem=nullptr;
			if(!pkShop->GetItem(offer.dwItemID, &pkItem) && !pkShop->GetItemSold(offer.dwItemID, &pkItem))
			{
				LOG_ERROR("cannot find item info {} , offer id {} ", offer.dwItemID, offer.dwOfferID);
				return false;
			}

			TItemInfo& itemInfo = extrainfo[i].item;
			itemInfo.dwItemID	= offer.dwItemID;
			itemInfo.dwOwnerID	= offer.dwOwnerID;

			CopyObject(itemInfo.item, *pkItem->GetInfo());
			CopyObject(itemInfo.price, *pkItem->GetPrice());

			strncpy(extrainfo[i].szShopName , pkShop->GetName(), OFFLINE_SHOP_NAME_MAX_LEN);
		}

		pack.wSize += sizeof(TMyOfferExtraInfo)*extrainfo.size();

		buff.write(&pack,			sizeof(pack));
		buff.write(&subpack,		sizeof(subpack));
		buff.write(&vec[0],			sizeof(TOfferInfo)*vec.size());
		buff.write(&extrainfo[0] ,	sizeof(TMyOfferExtraInfo)*extrainfo.size());//offlineshop-updated 04/08/19

		//offlineshop-updated 05/08/19
		ecs::OfflineShopSystem::SetLookingOfferList(character, true);
		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());
		return true;
	}


	//SAFEBOX
	bool CShopManager::RecvShopSafeboxOpenClientPacket(entt::entity character)
	{
		if(!ecs::IsCharacter(character) || ecs::OfflineShopSystem::GetShopSafebox(character))
			return false;

		CShopSafebox* pkSafebox = GetShopSafeboxByOwnerID((ecs::PlayerRuntime::GetPlayerID(character)));
		if(!pkSafebox)
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		ecs::OfflineShopSystem::SetShopSafebox(character, pkSafebox);
		pkSafebox->RefreshToOwner(character);
		return true;
	}


	bool CShopManager::RecvShopSafeboxGetItemClientPacket(entt::entity character, uint32_t dwItemID)
	{
		if (!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetShopSafebox(character))
			return false;

		CShopSafebox* pkSafebox = ecs::OfflineShopSystem::GetShopSafebox(character);
		CShopItem* pItem=nullptr;

		if(!pkSafebox->GetItem(dwItemID, &pItem))
			return false;

		const entt::entity pkItem = pItem->CreateItem();
		if (!ItemSystem::IsValidItem(pkItem))
			return false;


		TItemPos itemPos;
		if (!InventorySystem::CanTakeInventoryItem(character, pkItem, &itemPos))
		{
			ItemSystem::DestroyItemEntityEcs(
			pkItem,
			"OFFLINESHOP_TEMP");
			return false;
		}

		ecs::OfflineShopSystem::SetUseTime(character);

		if (pkSafebox->RemoveItem(dwItemID))
		{
			pkSafebox->RefreshToOwner();
			InventorySystem::AddToCharacter(pkItem, character, itemPos);
		}

		SendShopSafeboxGetItemDBPacket((ecs::PlayerRuntime::GetPlayerID(character)), dwItemID);
		return true;
	}


	bool CShopManager::RecvShopSafeboxGetValutesClientPacket(entt::entity character, const TValutesInfo& valutes)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetShopSafebox(character))
			return false;

		if (valutes.illYang < 0)
			return false;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		if (valutes.iCheque < 0)
			return false;
#endif

#if !defined(ENABLE_FULL_YANG) && !defined(FULL_YANG)
		if(ecs::PointSystem::GetGold(character) + valutes.illYang > GOLD_MAX)
			return false;
#endif
#ifdef __ENABLE_CHEQUE_SYSTEM__
		if (ecs::PointSystem::GetReal(character, POINT_CHEQUE) + valutes.iCheque >= CHEQUE_MAX)
			return false;
#endif

		CShopSafebox* pkSafebox		= ecs::OfflineShopSystem::GetShopSafebox(character);
		CShopSafebox::SValuteAmount peekAmount(valutes);

		if(!pkSafebox->RemoveValute(peekAmount))
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		ecs::PointSystem::Change(character, POINT_GOLD, valutes.illYang);
#ifdef __ENABLE_CHEQUE_SYSTEM__
		ecs::PointSystem::Change(character, POINT_CHEQUE, valutes.iCheque);
#endif

		pkSafebox->RefreshToOwner();
		SendShopSafeboxGetValutesDBPacket((ecs::PlayerRuntime::GetPlayerID(character)), valutes);
		return true;
	}


	bool CShopManager::RecvShopSafeboxCloseClientPacket(entt::entity character)
	{
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetShopSafebox(character))
			return false;

		ecs::OfflineShopSystem::SetShopSafebox(character, nullptr);
		return true;
	}


	void CShopManager::SendShopSafeboxRefresh(entt::entity character, const TValutesInfo& valute, const std::vector<CShopItem>& vec)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetShopSafebox(character))
			return;

		ecs::OfflineShopSystem::SetUseTime(character);

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCShopSafeboxRefresh) + ((sizeof(uint32_t) + sizeof(TItemInfoEx))*vec.size());
		pack.bSubHeader	= SUBHEADER_GC_SHOP_SAFEBOX_REFRESH;


		TSubPacketGCShopSafeboxRefresh subpack;
		subpack.dwItemCount	= vec.size();
		CopyObject(subpack.valute , valute);

		TEMP_BUFFER buff;
		buff.write(&pack,		sizeof(pack));
		buff.write(&subpack,	sizeof(subpack));


		TItemInfoEx item;
		uint32_t dwItemID=0;

		for (auto it = vec.begin(); it != vec.end(); it++)
		{
			const CShopItem& shopitem = *it;

			dwItemID = shopitem.GetID();
			CopyObject(item, *shopitem.GetInfo());

			buff.write(&dwItemID,	sizeof(dwItemID));
			buff.write(&item,		sizeof(item));
		}

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek() , buff.size());
	}


	bool CShopManager::RecvAuctionListRequestClientPacket(entt::entity character, bool owner)
	{
		if(!ecs::PlayerRuntime::IsValid(character))
			return false;

		TAuctionListElement temp;
		std::vector<TAuctionListElement> vec;
		vec.reserve(m_mapAuctions.size());

		for (auto it = m_mapAuctions.begin(); it != m_mapAuctions.end(); it++)
		{
			const CAuction& rAuction = it->second;

			CopyObject(temp.actual_best, rAuction.GetBestOffer());
			CopyObject(temp.auction, rAuction.GetInfo());

			temp.dwOfferCount = rAuction.GetOffers().size();
			vec.push_back(temp);
		}

		SendAuctionListClientPacket(character, vec, owner);
		return true;
	}


	bool CShopManager::RecvAuctionOpenRequestClientPacket(entt::entity character, uint32_t dwOwnerID)
	{
		auto it = m_mapAuctions.find(dwOwnerID);
		if(it == m_mapAuctions.end())
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		it->second.AddGuest(character);
		//SendAuctionOpenAuctionClientPacket(ch, it->second.GetInfo(), it->second.GetOffers());
		return true;
	}


	bool CShopManager::RecvMyAuctionOpenRequestClientPacket(entt::entity character)
	{
		OFFSHOP_DEBUG("pid %u , exist %s ",(ecs::PlayerRuntime::GetPlayerID(character)), m_mapAuctions.find((ecs::PlayerRuntime::GetPlayerID(character))) != m_mapAuctions.end() ? "TRUE" : "FALSE" );

		ecs::OfflineShopSystem::SetUseTime(character);

		if (!ecs::OfflineShopSystem::GetAuction(character))
		{
			auto it = m_mapAuctions.find((ecs::PlayerRuntime::GetPlayerID(character)));

			if (it == m_mapAuctions.end())
				SendAuctionOpenMyAuctionNoAuctionClientPacket(character);

			else
			{
				it->second.AddGuest(character);
				//SendAuctionOpenAuctionClientPacket(character, it->second.GetInfo(), it->second.GetOffers());
			}

		}

		else
		{
			CAuction* pkAuction = ecs::OfflineShopSystem::GetAuction(character);
			pkAuction->AddGuest(character);
			//SendAuctionOpenAuctionClientPacket(character, pkAuction->GetInfo(), pkAuction->GetOffers());
		}

		return true;
	}


	bool CShopManager::RecvAuctionCreateClientPacket(entt::entity character, uint32_t dwDuration, const TPriceInfo& init_price, const TItemPos& pos)
	{
		//checking about duplicate item :D
		if(!ecs::IsCharacter(character) || ecs::OfflineShopSystem::GetAuction(character))
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		if (!InventorySystem::CanHandleItems(character) || !CheckCharacterActions(character))
		{
			SendChatPacket(character,CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}


		OFFSHOP_DEBUG("owner %u , duration %u ",(ecs::PlayerRuntime::GetPlayerID(character)), dwDuration);

		//checking about hacking duration
		dwDuration = MIN(OFFLINESHOP_DURATION_MAX_MINUTES, dwDuration);


		//checking about duplicate item
		const entt::entity item = ItemSystem::GetItem(character, pos);
		if (!ItemSystem::IsValidItem(item))
			return false;

		if(ItemSystem::IsItemEquipped(item) || ItemSystem::IsItemExchanging(item) || ItemSystem::IsItemLocked(item))
			return false;


		TItemTable* pItemTable= ITEM_MANAGER::instance().GetTable(ItemSystem::GetItemVnum(item));
		if(!pItemTable)
			return false;

		if(IS_SET(pItemTable->dwAntiFlags, ITEM_ANTIFLAG_GIVE) || IS_SET(pItemTable->dwAntiFlags , ITEM_ANTIFLAG_MYSHOP))
			return false;

#ifdef ENABLE_SOULBIND_SYSTEM
		if (ItemSystem::IsItemBound(item)) {
			return false;
		}
#endif

		ecs::OfflineShopSystem::SetUseTime(character);

		//making info
		TAuctionInfo auction;
		auction.dwDuration = dwDuration;
		auction.dwOwnerID  = (ecs::PlayerRuntime::GetPlayerID(character));
		strncpy(auction.szOwnerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(auction.szOwnerName));
		CopyObject(auction.init_price, init_price);


		TItemInfoEx& itemInfo = auction.item;
		itemInfo.dwCount	= ItemSystem::GetItemCount(item);
		itemInfo.dwVnum		= ItemSystem::GetItemVnum(item);

		//patch 08-03-2020
		itemInfo.expiration = GetItemExpiration(item);

		for (int attr = 0; attr < ITEM_ATTRIBUTE_MAX_NUM; ++attr)
			itemInfo.aAttr[attr] = ItemSystem::GetItemAttribute(item, attr);
		for (int socket = 0; socket < ITEM_SOCKET_MAX_NUM; ++socket)
			itemInfo.alSockets[socket] = ItemSystem::GetItemSocket(item, socket);

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		itemInfo.dwTransmutation = ItemSystem::GetItemTransmutationVnum(item);
#endif
#ifdef ATTR_LOCK
		itemInfo.iLockedAttr = ItemSystem::GetItemLockedAttr(item);
#endif

		//destroy/remove/send
		const entt::entity removed = InventorySystem::RemoveFromCharacter(item);
		ItemSystem::DestroyItemEntityEcs(removed, "OFFLINESHOP_SELL");
		SendAuctionCreateDBPacket(auction);
		return true;
	}


	bool CShopManager::RecvAuctionAddOfferClientPacket(entt::entity character, uint32_t dwOwnerID, const TPriceInfo& price)
	{
		//checking about guesting
		if(!ecs::IsCharacter(character) || !ecs::OfflineShopSystem::GetAuctionGuest(character) || ecs::OfflineShopSystem::GetAuctionGuest(character)->GetInfo().dwOwnerID != dwOwnerID)
			return false;

		if (!_IS_VALID_GM_LEVEL(character))
			return false;

		//check anti auto-offer
		if((ecs::PlayerRuntime::GetPlayerID(character)) == dwOwnerID)
			return false;

		//check about enough money
		if(ecs::PointSystem::GetGold(character) < price.illYang)
			return false;

#ifdef __ENABLE_CHEQUE_SYSTEM__
		if (ecs::PointSystem::GetReal(character, POINT_CHEQUE) < price.iCheque)
			return false;
#endif

#ifdef ENABLE_SOULBIND_SYSTEM
		if (item->IsSealed()) {
			SendChatPacket(character, CHAT_PACKET_CANNOT_DO_NOW);
			return true;
		}
#endif

		if (!CheckLastOfferTime((ecs::PlayerRuntime::GetPlayerID(character)))) {
			SendChatPacket(character, CHAT_PACKET_CANNOT_DO_NOW);
			return false;
		}

		//checking about raise from best buyer
		CAuction* pAuction = ecs::OfflineShopSystem::GetAuctionGuest(character);
		if(pAuction->GetBestBuyer() == (ecs::PlayerRuntime::GetPlayerID(character)))
			return false;

		OFFSHOP_DEBUG("pAuction->GetBestBuyer() = %u ",pAuction->GetBestBuyer());

		if (pAuction->GetOffers().empty())
		{
			if (price < pAuction->GetInfo().init_price)
				return false;
		}

		else
		{
			//checking about min raise amount (+10% by default)
			const TPriceInfo& bestOffer = pAuction->GetBestOffer();
			if(!CheckNewAuctionOfferPrice(price,bestOffer))
				return false;
		}

		ecs::OfflineShopSystem::SetUseTime(character);

		ecs::PointSystem::Change(character, POINT_GOLD,-price.illYang);
#ifdef __ENABLE_CHEQUE_SYSTEM__
		ecs::PointSystem::Change(character, POINT_CHEQUE, -price.iCheque);
#endif

		TAuctionOfferInfo offer;
		offer.dwBuyerID	= (ecs::PlayerRuntime::GetPlayerID(character));
		offer.dwOwnerID	= dwOwnerID;
		CopyObject(offer.price	, price);
#ifdef __ENABLE_CHEQUE_SYSTEM__
		// converting amount cheque in yang
		offer.price.illYang = offer.price.GetTotalYangAmount();
		offer.price.iCheque=0;
#endif

		strncpy(offer.szBuyerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(offer.szBuyerName));


		SendAuctionAddOfferDBPacket(offer);
		return true;
	}


	bool CShopManager::RecvAuctionExitFromAuction(entt::entity character, uint32_t dwOwnerID)
	{
		auto it = m_mapAuctions.find((ecs::PlayerRuntime::GetPlayerID(character)));
		if(it == m_mapAuctions.end())
			return false;

		ecs::OfflineShopSystem::SetUseTime(character);

		it->second.RemoveGuest(character);
		return true;
	}


	void CShopManager::SendAuctionListClientPacket(entt::entity character, const std::vector<TAuctionListElement>& auctionVec, bool owner)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_AUCTION_LIST;
		pack.wSize		= sizeof(pack) + sizeof(TSubPacketGCAuctionList) + (sizeof(TAuctionListElement)*auctionVec.size());

		TSubPacketGCAuctionList subpack;
		subpack.dwCount = auctionVec.size();
		subpack.bOwner = owner;

		TEMP_BUFFER buff;
		buff.write(&pack,		sizeof(pack));
		buff.write(&subpack,	sizeof(subpack));

		if(!auctionVec.empty())
			buff.write(&auctionVec[0], sizeof(auctionVec[0]) * auctionVec.size());

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());
	}


	void CShopManager::SendAuctionOpenAuctionClientPacket(entt::entity character, const TAuctionInfo& auction, const std::vector<TAuctionOfferInfo>& vec)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= (ecs::PlayerRuntime::GetPlayerID(character))!=auction.dwOwnerID ? SUBHEADER_GC_OPEN_AUCTION: SUBHEADER_GC_OPEN_MY_AUCTION;
		pack.wSize		= sizeof(pack)+ sizeof(TSubPacketGCAuctionOpen) + (sizeof(TAuctionOfferInfo) * vec.size());

		TSubPacketGCAuctionOpen subpack;
		CopyObject(subpack.auction, auction);
		subpack.dwOfferCount = vec.size();


		TEMP_BUFFER buff;
		buff.write(&pack,		sizeof(pack));
		buff.write(&subpack,	sizeof(subpack));

		if(!vec.empty())
			buff.write(&vec[0], sizeof(vec[0]) * vec.size());

		ecs::PlayerRuntime::GetDesc(character)->Packet(buff.read_peek(), buff.size());
	}


	void CShopManager::SendAuctionOpenMyAuctionNoAuctionClientPacket(entt::entity character)
	{
		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCNewOfflineshop pack;
		pack.bHeader	= HEADER_GC_NEW_OFFLINESHOP;
		pack.bSubHeader	= SUBHEADER_GC_OPEN_MY_AUCTION_NO_AUCTION;
		pack.wSize		= sizeof(pack);

		ecs::PlayerRuntime::GetDesc(character)->Packet(&pack, sizeof(pack));
	}


	void CShopManager::RecvCloseBoardClientPacket(entt::entity character)
	{
		if(!ecs::IsCharacter(character) || !ecs::PlayerRuntime::GetDesc(character))
			return;

		//auction
		if (ecs::OfflineShopSystem::GetAuctionGuest(character))
		{
			ecs::OfflineShopSystem::GetAuctionGuest(character)->RemoveGuest(character);
			ecs::OfflineShopSystem::SetAuctionGuest(character, nullptr);
		}


		//safebox
		if(ecs::OfflineShopSystem::GetShopSafebox(character))
			ecs::OfflineShopSystem::SetShopSafebox(character, nullptr);

		//shop
		if (ecs::OfflineShopSystem::GetOfflineShopGuest(character))
		{
			ecs::OfflineShopSystem::GetOfflineShopGuest(character)->RemoveGuest(character);
			ecs::OfflineShopSystem::SetOfflineShopGuest(character, nullptr);
		}


		if(ecs::OfflineShopSystem::GetOfflineShop(character))
			ecs::OfflineShopSystem::GetOfflineShop(character)->RemoveGuest(character);

		//offlineshop-updated 05/08/19
		ecs::OfflineShopSystem::SetLookingOfferList(character, false);
	}

	void CShopManager::RecvCloseMyAuction(entt::entity character)
	{
		if(!ecs::IsCharacter(character) || !ecs::PlayerRuntime::GetDesc(character))
			return;

		if (ecs::OfflineShopSystem::GetAuction(character))
		{
			TPacketGDNewOfflineShop pack;
			pack.bSubHeader = SUBHEADER_GD_AUCTION_CLOSE;

			TSubPacketGDAuctionClose subpack;
			CopyObject(subpack.dwOwnerID , (ecs::PlayerRuntime::GetPlayerID(character)));

			TEMP_BUFFER buff;
			buff.write(&pack, sizeof(pack));
			buff.write(&subpack, sizeof(subpack));

			db_clientdesc->DBPacket(HEADER_GD_NEW_OFFLINESHOP , 0 , buff.read_peek() , buff.size());
		}
	}

	void CShopManager::UpdateShopsDuration()
	{
		SHOPMAP::iterator it = m_mapShops.begin();
		for (; it != m_mapShops.end(); it++)
		{
			CShop& shop = it->second;

			if(shop.GetDuration() > 0)
				shop.DecreaseDuration();
		}
	}


	void CShopManager::UpdateAuctionsDuration()
	{
		AUCTIONMAP::iterator it = m_mapAuctions.begin();
		for (; it != m_mapAuctions.end(); it++)
		{
			CAuction& auction = it->second;
			auction.DecreaseDuration();
		}
	}


	void CShopManager::ClearSearchTimeMap()
	{
		m_searchTimeMap.clear();

		// fix flooding offers
		m_offerCooldownMap.clear();
	}

	// fix flooding offers
	bool CShopManager::CheckOfferCooldown(uint32_t dwPID) {
		uint32_t now = get_dword_time();
		const uint32_t cooldown_seconds = 15;

		auto it = m_offerCooldownMap.find(dwPID);
		if (it == m_offerCooldownMap.end()) {
			m_offerCooldownMap[dwPID] = now + cooldown_seconds *1000;
			return true;
		}

		if (it->second > now)
			return false;

		it->second = now + cooldown_seconds * 1000;
		return true;
	}


	bool CShopManager::CheckSearchTime(uint32_t dwPID)
	{
		auto it = m_searchTimeMap.find(dwPID);
		if (it == m_searchTimeMap.end())
		{
			m_searchTimeMap.insert(std::make_pair(dwPID, get_dword_time()));
			return true;
		}

		if(it->second + OFFLINESHOP_SECONDS_PER_SEARCH*1000 > get_dword_time())
			return false;

		it->second = get_dword_time();
		return true;
	}

	//*new check about auction offers
	bool CShopManager::CheckLastOfferTime(uint32_t dwPID)
	{
		auto it = m_offerTimeMap.find(dwPID);
		if (it == m_offerTimeMap.end())
		{
			m_offerTimeMap.insert(std::make_pair(dwPID, get_dword_time()));
			return true;
		}

		if (it->second + OFFLINESHOP_SECONDS_PER_OFFER * 1000 > get_dword_time())
			return false;

		it->second = get_dword_time();
		return true;
	}

	void CShopManager::ClearOfferTimeMap()
	{
		m_offerTimeMap.clear();
	}

	//topatch 29-10
	void CShopManager::CheckOfferOnItem(uint32_t dwOwnerID, uint32_t dwItemID)
	{
		//for (itertype(m_mapOffer) it; it != m_mapOffer.end();) {
		for (auto it = m_mapOffer.begin(); it != m_mapOffer.end();) {
			auto& vec = it->second;

			for (auto itVec = vec.begin(); itVec != vec.end();) {
				if (itVec->dwItemID == dwItemID)
					itVec = vec.erase(itVec);
				else
					itVec++;
			}

			if (vec.empty())
				it = m_mapOffer.erase(it);
			else
				it++;
		}
	}

}

#endif //__ENABLE_NEW_OFFLINESHOP__
