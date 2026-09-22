#ifndef __INC_METIN_II_GAME_SECTREE_MANAGER_H__
#define __INC_METIN_II_GAME_SECTREE_MANAGER_H__

#include <common/service.h>
#include "sectree.h"
#include <Core/Logging.hpp>

typedef struct SMapRegion
{
	int32_t			index;
	int32_t			sx, sy, ex, ey;
	PIXEL_POSITION	posSpawn;

	bool		bEmpireSpawnDifferent;
	PIXEL_POSITION	posEmpire[3];

	std::string		strMapName;
} TMapRegion;

struct TAreaInfo
{
	int32_t sx, sy, ex, ey, dir;
	TAreaInfo(int32_t sx, int32_t sy, int32_t ex, int32_t ey, int32_t dir)
		: sx(sx), sy(sy), ex(ex), ey(ey), dir(dir)
		{}
};

struct npc_info
{
	uint8_t bType;
#ifdef ENABLE_MULTI_NAMES
	uint32_t		name;
#else
	const char*	name;
#endif
	int32_t x, y;
	npc_info(uint8_t bType,
#ifdef ENABLE_MULTI_NAMES
	uint32_t name
#else
	const char* name
#endif
	, int32_t x, int32_t y)
		: bType(bType), name(name), x(x), y(y)
		{}
};

#ifdef ENABLE_ATLAS_BOSS
struct boss_info
{
	uint8_t 		bType;
#ifdef ENABLE_MULTI_NAMES
	uint32_t		szName;
#else
	const char*	szName;
#endif
	int32_t 		lX;
	int32_t		lY;
	int32_t		lTime;
	boss_info(uint8_t bType,
#ifdef ENABLE_MULTI_NAMES
	uint32_t szName
#else
	const char* szName
#endif
	, int32_t lX, int32_t lY, int32_t lTime)
		: bType(bType), szName(szName), lX(lX), lY(lY), lTime(lTime)
		{}
};
#endif

typedef std::map<std::string, TAreaInfo> TAreaMap;

typedef struct SSetting
{
	int32_t			iIndex;
	int32_t			iCellScale;
	int32_t			iBaseX;
	int32_t			iBaseY;
	int32_t			iWidth;
	int32_t			iHeight;

	PIXEL_POSITION	posSpawn;
} TMapSetting;

class SECTREE_MAP
{
	public:
		typedef std::map<uint32_t, LPSECTREE> MapType;

		SECTREE_MAP();
		SECTREE_MAP(SECTREE_MAP & r);
		virtual ~SECTREE_MAP();
		void DrainEntities();
		bool IsDraining() const { return m_draining; }

		bool Add(uint32_t key, LPSECTREE sectree) {
			if (m_draining) return false;
			return map_.insert(MapType::value_type(key, sectree)).second;
		}

		LPSECTREE	Find(uint32_t dwPackage);
		LPSECTREE	Find(uint32_t x, uint32_t y);
		void		Build();

		TMapSetting	m_setting;

		template< typename Func >
		void for_each( Func & rfunc )
		{
			// <Factor> Using snapshot copy to avoid side-effects
			FCollectEntity collector;
			for (auto it = map_.begin(); it != map_.end(); ++it)
			{
				LPSECTREE sectree = it->second;
				sectree->Collect(collector);
			}
			collector.ForEach(rfunc);
		}

		void DumpAllToSysErr() {
			for (auto i = map_.begin(); i != map_.end(); ++i)
			{
				LOG_ERROR("SECTREE {:x}({}, {})", i->first, i->first & 0xffff, i->first >> 16);
			}
		}

	private:
		MapType map_;
		bool m_draining = false;
};

enum EAttrRegionMode
{
	ATTR_REGION_MODE_SET,
	ATTR_REGION_MODE_REMOVE,
	ATTR_REGION_MODE_CHECK,
};

class SECTREE_MANAGER : public singleton<SECTREE_MANAGER>
{
	public:
		SECTREE_MANAGER();
		virtual ~SECTREE_MANAGER();

		LPSECTREE_MAP GetMap(int32_t lMapIndex);
		LPSECTREE 	Get(int32_t dwIndex, uint32_t package);
		LPSECTREE 	Get(int32_t dwIndex, int32_t x, int32_t y);

		template< typename Func >
		void for_each(int32_t iMapIndex, Func & rfunc )
		{
			LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( iMapIndex );
			if ( pSecMap )
			{
				pSecMap->for_each( rfunc );
			}
		}

		int		LoadSettingFile(int32_t lIndex, const char * c_pszSettingFileName, TMapSetting & r_setting);
		bool		LoadMapRegion(const char * c_pszFileName, TMapSetting & r_Setting, const char * c_pszMapName);
		int		Build(const char * c_pszListFileName, const char* c_pszBasePath);
		LPSECTREE_MAP BuildSectreeFromSetting(TMapSetting & r_setting);
		bool		LoadAttribute(LPSECTREE_MAP pkMapSectree, const char * c_pszFileName, TMapSetting & r_setting);
		void		LoadDungeon(int iIndex, const char * c_pszFileName);
		bool		GetValidLocation(int32_t lMapIndex, int32_t x, int32_t y, int32_t& r_lValidMapIndex, PIXEL_POSITION & r_pos, uint8_t empire = 0);
		bool		GetSpawnPosition(int32_t x, int32_t y, PIXEL_POSITION & r_pos);
		bool		GetSpawnPositionByMapIndex(int32_t lMapIndex, PIXEL_POSITION & r_pos);
		bool		GetRecallPositionByEmpire(int32_t iMapIndex, uint8_t bEmpire, PIXEL_POSITION & r_pos);

		const TMapRegion *	GetMapRegion(int32_t lMapIndex);
		int32_t			GetMapIndex(int32_t x, int32_t y);
		const TMapRegion *	FindRegionByPartialName(const char* szMapName);

		bool		GetMapBasePosition(int32_t x, int32_t y, PIXEL_POSITION & r_pos);
		bool		GetMapBasePositionByMapIndex(int32_t lMapIndex, PIXEL_POSITION & r_pos);
		bool		GetMovablePosition(int32_t lMapIndex, int32_t x, int32_t y, PIXEL_POSITION & pos);
		bool		IsMovablePosition(int32_t lMapIndex, int32_t x, int32_t y);
		bool		GetCenterPositionOfMap(int32_t lMapIndex, PIXEL_POSITION & r_pos);
		bool        GetRandomLocation(int32_t lMapIndex, PIXEL_POSITION & r_pos, uint32_t dwCurrentX = 0, uint32_t dwCurrentY = 0, int iMaxDistance = 0);

		int32_t		CreatePrivateMap(int32_t lMapIndex);	// returns new private map index, returns 0 when fail
		void		DestroyPrivateMap(int32_t lMapIndex);

		TAreaMap&	GetDungeonArea(int32_t lMapIndex);
		void		SendNPCPosition(entt::entity ch);
		void		InsertNPCPosition(int32_t lMapIndex, uint8_t bType,
#ifdef ENABLE_MULTI_NAMES
		uint32_t szName
#else
		const char* szName
#endif
		, int32_t x, int32_t y);
#ifdef ENABLE_ATLAS_BOSS
		void		SendBossPosition(entt::entity ch);
		void		InsertBossPosition(int32_t lMapIndex, uint8_t bType,
#ifdef ENABLE_MULTI_NAMES
		uint32_t szName
#else
		const char* szName
#endif
		, int32_t lX, int32_t lY, int32_t lTime);
#endif
		uint8_t		GetEmpireFromMapIndex(int32_t lMapIndex);

		void		PurgeMonstersInMap(int32_t lMapIndex);
		void		PurgeStonesInMap(int32_t lMapIndex);
		void		PurgeNPCsInMap(int32_t lMapIndex);
		size_t		GetMonsterCountInMap(int32_t lMapIndex);
		size_t	GetMonsterCountInMap(int32_t lMapIndex, uint32_t dwVnum);

		bool		ForAttrRegion(int32_t lMapIndex, int32_t lStartX, int32_t lStartY, int32_t lEndX, int32_t lEndY, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode);

		bool		SaveAttributeToImage(int32_t lMapIndex, const char * c_pszFileName, LPSECTREE_MAP pMapSrc = nullptr);
#ifdef __VERSION_162__
		void	GetRestartCityPos(int iMapIndex, int iEmpire, int &iTargetX, int &iTargetY, int &iTargetZ);
		void	AddRestartCityPos(int iMapIndex = 0, int iEmpire = 0, int iX = 0, int iY = 0, int iZ = 0);
#endif
	private:

		bool		ForAttrRegionRightAngle(int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode );

		bool		ForAttrRegionFreeAngle(int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode );

		bool		ForAttrRegionCell(int32_t lMapIndex, int32_t lCX, int32_t lCY, uint32_t dwAttr, EAttrRegionMode mode );

		static uint16_t			current_sectree_version;
		std::map<uint32_t, LPSECTREE_MAP>	m_map_pkSectree;
		std::map<int, TAreaMap>	m_map_pkArea;
		std::vector<TMapRegion>		m_vec_mapRegion;
		std::map<uint32_t, std::vector<npc_info> > m_mapNPCPosition;
#ifdef ENABLE_ATLAS_BOSS
		std::map<uint32_t, std::vector<boss_info> > m_mapBossPosition;
#endif
		// <Factor> Circular private map indexing
		typedef std::unordered_map<int32_t, int> PrivateIndexMapType;
		PrivateIndexMapType next_private_index_map_;
};

#endif

