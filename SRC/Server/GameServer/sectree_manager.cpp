#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <sstream>
#include <Base/targa.h>
#include <Base/attribute.h>
#include "config.h"
#include "utils.h"
#include "sectree_manager.h"
#include "regen.h"
#include "lzo_manager.h"
#include "desc.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "item.h"
#include "item_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "start_position.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "Core/Logging.hpp"

uint16_t SECTREE_MANAGER::current_sectree_version = MAKEWORD(0, 3);

SECTREE_MAP::SECTREE_MAP()
{
	memset( &m_setting, 0, sizeof(m_setting) );
}

SECTREE_MAP::~SECTREE_MAP()
{
	MapType::iterator it = map_.begin();

	while (it != map_.end()) {
		LPSECTREE sectree = (it++)->second;
		M2_DELETE(sectree);
	}

	map_.clear();
}

SECTREE_MAP::SECTREE_MAP(SECTREE_MAP & r)
{
	m_setting = r.m_setting;

	MapType::iterator it = r.map_.begin();

	while (it != r.map_.end())
	{
		LPSECTREE tree = M2_NEW SECTREE;

		tree->m_id.coord = it->second->m_id.coord;
		tree->CloneAttribute(it->second);

		map_.insert(MapType::value_type(it->first, tree));
		++it;
	}

	Build();
}

LPSECTREE SECTREE_MAP::Find(uint32_t dwPackage)
{
	MapType::iterator it = map_.find(dwPackage);

	if (it == map_.end())
		return nullptr;

	return it->second;
}

LPSECTREE SECTREE_MAP::Find(uint32_t x, uint32_t y)
{
	SECTREEID id;
	id.coord.x = x / SECTREE_SIZE;
	id.coord.y = y / SECTREE_SIZE;
	return Find(id.package);
}

void SECTREE_MAP::Build()
{
    // Ŭ���̾�Ʈ���� �ݰ� 150m ĳ������ ������ �ֱ�����
    // 3x3ĭ -> 5x5 ĭ���� �ֺ�sectree Ȯ��(�ѱ�)
	struct neighbor_coord_s
	{
		int x;
		int y;
	} neighbor_coord[8] = {
		{ -SECTREE_SIZE,	0		},
		{  SECTREE_SIZE,	0		},
		{ 0	       ,	-SECTREE_SIZE	},
		{ 0	       ,	 SECTREE_SIZE	},
		{ -SECTREE_SIZE,	 SECTREE_SIZE	},
		{  SECTREE_SIZE,	-SECTREE_SIZE	},
		{ -SECTREE_SIZE,	-SECTREE_SIZE	},
		{  SECTREE_SIZE,	 SECTREE_SIZE	},
	};

	//
	// ��� sectree�� ���� ���� sectree�� ����Ʈ�� �����.
	//
	MapType::iterator it = map_.begin();

	while (it != map_.end())
	{
		LPSECTREE tree = it->second;

		tree->m_neighbor_list.push_back(tree); // �ڽ��� �ִ´�.

		LOG_TRACE("{}x{}", static_cast<int32_t>(tree->m_id.coord.x), static_cast<int32_t>(tree->m_id.coord.y));

		int x = tree->m_id.coord.x * SECTREE_SIZE;
		int y = tree->m_id.coord.y * SECTREE_SIZE;

		for (uint32_t i = 0; i < 8; ++i)
		{
			LPSECTREE tree2 = Find(x + neighbor_coord[i].x, y + neighbor_coord[i].y);

			if (tree2)
			{
				LOG_TRACE("   {} {}x{}", i, static_cast<int32_t>(tree2->m_id.coord.x), static_cast<int32_t>(tree2->m_id.coord.y));
				tree->m_neighbor_list.push_back(tree2);
			}
		}

		++it;
	}
}

SECTREE_MANAGER::SECTREE_MANAGER()
{
}

SECTREE_MANAGER::~SECTREE_MANAGER()
{
	/*
	   std::map<uint32_t, LPSECTREE_MAP>::iterator it = m_map_pkSectree.begin();

	   while (it != m_map_pkSectree.end())
	   {
	   M2_DELETE(it->second);
	   ++it;
	   }
	 */
}

LPSECTREE_MAP SECTREE_MANAGER::GetMap(int32_t lMapIndex)
{
	const auto it = m_map_pkSectree.find(lMapIndex);

	if (it == m_map_pkSectree.end())
		return nullptr;

	return it->second;
}

LPSECTREE SECTREE_MANAGER::Get(int32_t dwIndex, uint32_t package)
{
	LPSECTREE_MAP pkSectreeMap = GetMap(dwIndex);

	if (!pkSectreeMap)
		return nullptr;

	return pkSectreeMap->Find(package);
}

LPSECTREE SECTREE_MANAGER::Get(int32_t dwIndex, int32_t x, int32_t y)
{
	SECTREEID id;
	id.coord.x = x / SECTREE_SIZE;
	id.coord.y = y / SECTREE_SIZE;
	return Get(dwIndex, id.package);
}

// -----------------------------------------------------------------------------
// Setting.txt �� ���� SECTREE �����
// -----------------------------------------------------------------------------
int SECTREE_MANAGER::LoadSettingFile(int32_t lMapIndex, const char * c_pszSettingFileName, TMapSetting & r_setting)
{
	memset(&r_setting, 0, sizeof(TMapSetting));

	FILE * fp = fopen(c_pszSettingFileName, "r");

	if (!fp)
	{
		LOG_ERROR("cannot open file: {}", c_pszSettingFileName);
		return 0;
	}

	char buf[256], cmd[256];
	int iWidth = 0, iHeight = 0;

	while (fgets(buf, 256, fp))
	{
		sscanf(buf, " %s ", cmd);

		if (!strcasecmp(cmd, "MapSize"))
		{
			sscanf(buf, " %s %d %d ", cmd, &iWidth, &iHeight);
		}
		else if (!strcasecmp(cmd, "BasePosition"))
		{
			sscanf(buf, " %s %d %d", cmd, &r_setting.iBaseX, &r_setting.iBaseY);
		}
		else if (!strcasecmp(cmd, "CellScale"))
		{
			sscanf(buf, " %s %d ", cmd, &r_setting.iCellScale);
		}
	}

	fclose(fp);

	if ((iWidth == 0 && iHeight == 0) || r_setting.iCellScale == 0)
	{
		LOG_ERROR("Invalid Settings file: {}", c_pszSettingFileName);
		return 0;
	}

	r_setting.iIndex = lMapIndex;
	r_setting.iWidth = (r_setting.iCellScale * 128 * iWidth);
	r_setting.iHeight = (r_setting.iCellScale * 128 * iHeight);
	return 1;
}

LPSECTREE_MAP SECTREE_MANAGER::BuildSectreeFromSetting(TMapSetting & r_setting)
{
	LPSECTREE_MAP pkMapSectree = M2_NEW SECTREE_MAP;

	pkMapSectree->m_setting = r_setting;

	int32_t x, y;
	LPSECTREE tree;

	for (x = r_setting.iBaseX; x < r_setting.iBaseX + r_setting.iWidth; x += SECTREE_SIZE)
	{
		for (y = r_setting.iBaseY; y < r_setting.iBaseY + r_setting.iHeight; y += SECTREE_SIZE)
		{
			tree = M2_NEW SECTREE;
			tree->m_id.coord.x = x / SECTREE_SIZE;
			tree->m_id.coord.y = y / SECTREE_SIZE;
			pkMapSectree->Add(tree->m_id.package, tree);
			LOG_TRACE("new sectree {} x {}", static_cast<int32_t>(tree->m_id.coord.x), static_cast<int32_t>(tree->m_id.coord.y));
		}
	}

	if ((r_setting.iBaseX + r_setting.iWidth) % SECTREE_SIZE)
	{
		tree = M2_NEW SECTREE;
		tree->m_id.coord.x = ((r_setting.iBaseX + r_setting.iWidth) / SECTREE_SIZE) + 1;
		tree->m_id.coord.y = ((r_setting.iBaseY + r_setting.iHeight) / SECTREE_SIZE);
		pkMapSectree->Add(tree->m_id.package, tree);
	}

	if ((r_setting.iBaseY + r_setting.iHeight) % SECTREE_SIZE)
	{
		tree = M2_NEW SECTREE;
		tree->m_id.coord.x = ((r_setting.iBaseX + r_setting.iWidth) / SECTREE_SIZE);
		tree->m_id.coord.y = ((r_setting.iBaseX + r_setting.iHeight) / SECTREE_SIZE) + 1;
		pkMapSectree->Add(tree->m_id.package, tree);
	}

	return pkMapSectree;
}

void SECTREE_MANAGER::LoadDungeon(int iIndex, const char * c_pszFileName)
{
	FILE* fp = fopen(c_pszFileName, "r");

	if (!fp)
		return;

	int count = 0; // for debug

	while (!feof(fp))
	{
		char buf[1024];

		if (nullptr == fgets(buf, 1024, fp))
			break;

		if ((buf[0] == '#' || buf[0] == '/') && buf[1] == '/')
			continue;

		std::istringstream ins(buf, std::ios_base::in);
		std::string position_name;
		int32_t x, y, sx, sy, dir;

		ins >> position_name >> x >> y >> sx >> sy >> dir;

		if (ins.fail())
			continue;

		x -= sx;
		y -= sy;
		sx *= 2;
		sy *= 2;
		sx += x;
		sy += y;

		m_map_pkArea[iIndex].insert(std::make_pair(position_name, TAreaInfo(x, y, sx, sy, dir)));

		count++;
	}

	fclose(fp);

	LOG_TRACE("Dungeon Position Load [{:3}]{} count {}", iIndex, c_pszFileName, count);
}

bool SECTREE_MANAGER::LoadMapRegion(const char * c_pszFileName, TMapSetting & r_setting, const char * c_pszMapName)
{
	FILE * fp = fopen(c_pszFileName, "r");

	if ( test_server )
		LOG_TRACE("[LoadMapRegion] file({})", c_pszFileName);

	if (!fp)
		return false;

	int32_t iX=0, iY=0;
	PIXEL_POSITION pos[3] = { {0,0,0}, {0,0,0}, {0,0,0} };

	fscanf(fp, " %d %d ", &iX, &iY);

	int iEmpirePositionCount = fscanf(fp, " %d %d %d %d %d %d ",
			&pos[0].x, &pos[0].y,
			&pos[1].x, &pos[1].y,
			&pos[2].x, &pos[2].y);

	fclose(fp);

	if( iEmpirePositionCount == 6 )
	{
		for ( int n = 0; n < 3; ++n )
			LOG_TRACE("LoadMapRegion {} {} ", pos[n].x, pos[n].y);
	}
	else
	{
		LOG_TRACE("LoadMapRegion no empire specific start point");
	}

	TMapRegion region;

	region.index = r_setting.iIndex;
	region.sx = r_setting.iBaseX;
	region.sy = r_setting.iBaseY;
	region.ex = r_setting.iBaseX + r_setting.iWidth;
	region.ey = r_setting.iBaseY + r_setting.iHeight;

	region.strMapName = c_pszMapName;

	region.posSpawn.x = r_setting.iBaseX + (iX * 100);
	region.posSpawn.y = r_setting.iBaseY + (iY * 100);

	r_setting.posSpawn = region.posSpawn;

	LOG_TRACE("LoadMapRegion {} x {} ~ {} y {} ~ {}, town {} {}", region.index, region.sx, region.ex, region.sy, region.ey, region.posSpawn.x, region.posSpawn.y);

	if (iEmpirePositionCount == 6)
	{
		region.bEmpireSpawnDifferent = true;

		for (int i = 0; i < 3; i++)
		{
			region.posEmpire[i].x = r_setting.iBaseX + (pos[i].x * 100);
			region.posEmpire[i].y = r_setting.iBaseY + (pos[i].y * 100);
		}
	}
	else
	{
		region.bEmpireSpawnDifferent = false;
	}

	m_vec_mapRegion.emplace_back(region);

	LOG_TRACE("LoadMapRegion {} End", region.index);
	return true;
}

bool SECTREE_MANAGER::LoadAttribute(LPSECTREE_MAP pkMapSectree, const char * c_pszFileName, TMapSetting & r_setting)
{
	FILE * fp = fopen(c_pszFileName, "rb");

	if (!fp)
	{
		LOG_ERROR("SECTREE_MANAGER::LoadAttribute : cannot open {}", c_pszFileName);
		return false;
	}

	int iWidth, iHeight;

	fread(&iWidth, sizeof(int), 1, fp);
	fread(&iHeight, sizeof(int), 1, fp);

	size_t maxMemSize = LZOManager::instance().GetMaxCompressedSize(sizeof(uint32_t) * (SECTREE_SIZE / CELL_SIZE) * (SECTREE_SIZE / CELL_SIZE));

	unsigned int uiSize;
	lzo_uint uiDestSize;

//#ifndef _MSC_VER
//	uint8_t abComp[maxMemSize];
//#else
	uint8_t* abComp = M2_NEW uint8_t[maxMemSize];
//#endif
	uint32_t * attr = M2_NEW uint32_t[maxMemSize];

	for (int y = 0; y < iHeight; ++y)
		for (int x = 0; x < iWidth; ++x)
		{
			// UNION ���� ��ǥ�� ���ĸ��� uint32_t���� ���̵�� ����Ѵ�.
			SECTREEID id;
			id.coord.x = (r_setting.iBaseX / SECTREE_SIZE) + x;
			id.coord.y = (r_setting.iBaseY / SECTREE_SIZE) + y;

			LPSECTREE tree = pkMapSectree->Find(id.package);

			// SERVER_ATTR_LOAD_ERROR
			if (tree == nullptr)
			{
				LOG_ERROR("FATAL ERROR! LoadAttribute({}) - cannot find sectree(package={:x}, coord=({}, {}), map_index={}, map_base=({}, {}))", c_pszFileName, static_cast<uint32_t>(id.package), static_cast<int32_t>(id.coord.x), static_cast<int32_t>(id.coord.y), r_setting.iIndex, r_setting.iBaseX, r_setting.iBaseY);
				LOG_ERROR("ERROR_ATTR_POS({}, {}) attr_size({}, {})", x, y, iWidth, iHeight);
				LOG_ERROR("CHECK! 'Setting.txt' and 'server_attr' MAP_SIZE!!");

				pkMapSectree->DumpAllToSysErr();
				abort();

				M2_DELETE_ARRAY(attr);
#ifdef _MSC_VER
				M2_DELETE_ARRAY(abComp);
#endif
				return false;
			}
			// END_OF_SERVER_ATTR_LOAD_ERROR

			if (tree->m_id.package != id.package)
			{
				LOG_ERROR("returned tree id mismatch! return {}, request {}", tree->m_id.package, id.package);
				fclose(fp);

				M2_DELETE_ARRAY(attr);
#ifdef _MSC_VER
				M2_DELETE_ARRAY(abComp);
#endif
				return false;
			}

			fread(&uiSize, sizeof(int), 1, fp);
			fread(abComp, sizeof(char), uiSize, fp);

			//LZOManager::instance().Decompress(abComp, uiSize, (uint8_t *) tree->GetAttributePointer(), &uiDestSize);
			uiDestSize = sizeof(uint32_t) * maxMemSize;
			LZOManager::instance().Decompress(abComp, uiSize, (uint8_t *) attr, &uiDestSize);

			if (uiDestSize != sizeof(uint32_t) * (SECTREE_SIZE / CELL_SIZE) * (SECTREE_SIZE / CELL_SIZE))
			{
				LOG_ERROR("SECTREE_MANAGER::LoadAttribte : {} : {} {} size mismatch! {}", c_pszFileName, static_cast<int32_t>(tree->m_id.coord.x), static_cast<int32_t>(tree->m_id.coord.y), uiDestSize);
				fclose(fp);

				M2_DELETE_ARRAY(attr);
#ifdef _MSC_VER
				M2_DELETE_ARRAY(abComp);
#endif
				return false;
			}

			tree->BindAttribute(M2_NEW CAttribute(attr, SECTREE_SIZE / CELL_SIZE, SECTREE_SIZE / CELL_SIZE));
		}

	fclose(fp);

	M2_DELETE_ARRAY(attr);
#ifdef _MSC_VER
	M2_DELETE_ARRAY(abComp);
#endif
	return true;
}

bool SECTREE_MANAGER::GetRecallPositionByEmpire(int32_t iMapIndex, uint8_t bEmpire, PIXEL_POSITION & r_pos)
{
	auto it = m_vec_mapRegion.begin();

	// 10000�� �Ѵ� ���� �ν��Ͻ� �������� �����Ǿ��ִ�.
	if (iMapIndex >= 10000)
	{
		iMapIndex /= 10000;
	}
#ifdef __VERSION_162__
	{
		int32_t iTargetX = 0, iTargetY = 0, iTargetZ = 0;
		GetRestartCityPos(iMapIndex, bEmpire, iTargetX, iTargetY, iTargetZ);
		if ((iTargetX != 0) || (iTargetY != 0) || (iTargetZ != 0))
		{
			r_pos.x = iTargetX * 100;
			r_pos.y = iTargetY * 100;
			r_pos.z = iTargetZ * 100;
			return true;
		}
	}
#endif

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (rRegion.index == iMapIndex)
		{
			if (rRegion.bEmpireSpawnDifferent && bEmpire >= 1 && bEmpire <= 3)
				r_pos = rRegion.posEmpire[bEmpire - 1];
			else
				r_pos = rRegion.posSpawn;

			return true;
		}
	}

	return false;
}

bool SECTREE_MANAGER::GetCenterPositionOfMap(int32_t lMapIndex, PIXEL_POSITION & r_pos)
{
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (rRegion.index == lMapIndex)
		{
			r_pos.x = rRegion.sx + (rRegion.ex - rRegion.sx) / 2;
			r_pos.y = rRegion.sy + (rRegion.ey - rRegion.sy) / 2;
			r_pos.z = 0;
			return true;
		}
	}

	return false;
}

bool SECTREE_MANAGER::GetSpawnPositionByMapIndex(int32_t lMapIndex, PIXEL_POSITION& r_pos)
{
	if (lMapIndex> 10000) lMapIndex /= 10000;
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (lMapIndex == rRegion.index)
		{
			r_pos = rRegion.posSpawn;
			return true;
		}
	}

	return false;
}

bool SECTREE_MANAGER::GetSpawnPosition(int32_t x, int32_t y, PIXEL_POSITION & r_pos)
{
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (x >= rRegion.sx && y >= rRegion.sy && x < rRegion.ex && y < rRegion.ey)
		{
			r_pos = rRegion.posSpawn;
			return true;
		}
	}

	return false;
}

bool SECTREE_MANAGER::GetMapBasePositionByMapIndex(int32_t lMapIndex, PIXEL_POSITION & r_pos)
{
	if (lMapIndex> 10000) lMapIndex /= 10000;
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		//if (x >= rRegion.sx && y >= rRegion.sy && x < rRegion.ex && y < rRegion.ey)
		if (lMapIndex == rRegion.index)
		{
			r_pos.x = rRegion.sx;
			r_pos.y = rRegion.sy;
			r_pos.z = 0;
			return true;
		}
	}

	return false;
}

bool SECTREE_MANAGER::GetMapBasePosition(int32_t x, int32_t y, PIXEL_POSITION & r_pos)
{
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (x >= rRegion.sx && y >= rRegion.sy && x < rRegion.ex && y < rRegion.ey)
		{
			r_pos.x = rRegion.sx;
			r_pos.y = rRegion.sy;
			r_pos.z = 0;
			return true;
		}
	}

	return false;
}

const TMapRegion * SECTREE_MANAGER::FindRegionByPartialName(const char* szMapName)
{
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		//if (rRegion.index == lMapIndex)
		//return &rRegion;
		if (rRegion.strMapName.find(szMapName))
			return &rRegion; // ĳ�� �ؼ� ������ ����
	}

	return nullptr;
}

const TMapRegion * SECTREE_MANAGER::GetMapRegion(int32_t lMapIndex)
{
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (rRegion.index == lMapIndex)
			return &rRegion;
	}

	return nullptr;
}

int32_t SECTREE_MANAGER::GetMapIndex(int32_t x, int32_t y)
{
	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (x >= rRegion.sx && y >= rRegion.sy && x < rRegion.ex && y < rRegion.ey)
			return rRegion.index;
	}

	LOG_INFO("SECTREE_MANAGER::GetMapIndex({}, {})", x, y);

	std::vector<TMapRegion>::iterator i;
	for (i = m_vec_mapRegion.begin(); i !=m_vec_mapRegion.end(); ++i)
	{
		TMapRegion & rRegion = *i;
		LOG_INFO("{}: ({}, {}) ~ ({}, {})", rRegion.index, rRegion.sx, rRegion.sy, rRegion.ex, rRegion.ey);
	}

	return 0;
}

int SECTREE_MANAGER::Build(const char * c_pszListFileName, const char* c_pszMapBasePath)
{
	if (true == test_server)
	{
		LOG_INFO("[BUILD] Build {} {} ", c_pszListFileName, c_pszMapBasePath);
	}

	FILE* fp = fopen(c_pszListFileName, "r");

	if (nullptr == fp)
		return 0;

	char buf[256 + 1];
	char szFilename[256];
	char szMapName[256];
	int iIndex;

	while (fgets(buf, 256, fp))
	{
		// @fixme144 BEGIN
		char * szEndline = strrchr(buf, '\n');
		if (!szEndline)
			continue;
		*szEndline = '\0';
		// @fixme144 END

		if (!strncmp(buf, "//", 2) || *buf == '#')
			continue;

		sscanf(buf, " %d %s ", &iIndex, szMapName);

		snprintf(szFilename, sizeof(szFilename), "%s/%s/Setting.txt", c_pszMapBasePath, szMapName);

		TMapSetting setting;
		setting.iIndex = iIndex;

		if (!LoadSettingFile(iIndex, szFilename, setting))
		{
			LOG_ERROR("can't load file {} in LoadSettingFile", szFilename);
			fclose(fp);
			return 0;
		}

		snprintf(szFilename, sizeof(szFilename), "%s/%s/Town.txt", c_pszMapBasePath, szMapName);

		if (!LoadMapRegion(szFilename, setting, szMapName))
		{
			LOG_ERROR("can't load file {} in LoadMapRegion", szFilename);
			fclose(fp);
			return 0;
		}

		if (true == test_server)
			LOG_INFO("[BUILD] Build {} {} {} ", c_pszMapBasePath, szMapName, iIndex);

		// ���� �� �������� �� ���� ���͸� �����ؾ� �ϴ°� Ȯ�� �Ѵ�.
		if (map_allow_find(iIndex))
		{
			LPSECTREE_MAP pkMapSectree = BuildSectreeFromSetting(setting);
			LOG_INFO("[BUILD] Build {} {} [w/h {} {}, base {} {}]", c_pszListFileName, c_pszMapBasePath, setting.iWidth, setting.iHeight, setting.iBaseX, setting.iBaseY);
			m_map_pkSectree.insert(std::map<uint32_t, LPSECTREE_MAP>::value_type(iIndex, pkMapSectree));

			snprintf(szFilename, sizeof(szFilename), "%s/%s/server_attr", c_pszMapBasePath, szMapName);
			LoadAttribute(pkMapSectree, szFilename, setting);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/regen.txt", c_pszMapBasePath, szMapName);
			regen_load(szFilename, setting.iIndex, setting.iBaseX, setting.iBaseY);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/npc.txt", c_pszMapBasePath, szMapName);
			regen_load(szFilename, setting.iIndex, setting.iBaseX, setting.iBaseY);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/boss.txt", c_pszMapBasePath, szMapName);
#ifdef ENABLE_ATLAS_BOSS
			regen_load(szFilename, setting.iIndex, setting.iBaseX, setting.iBaseY, true);
#else
			regen_load(szFilename, setting.iIndex, setting.iBaseX, setting.iBaseY);
#endif

			snprintf(szFilename, sizeof(szFilename), "%s/%s/stone.txt", c_pszMapBasePath, szMapName);
			regen_load(szFilename, setting.iIndex, setting.iBaseX, setting.iBaseY);

			snprintf(szFilename, sizeof(szFilename), "%s/%s/dungeon.txt", c_pszMapBasePath, szMapName);
			LoadDungeon(iIndex, szFilename);

			pkMapSectree->Build();
		}
	}

	fclose(fp);

	return 1;
}

bool SECTREE_MANAGER::IsMovablePosition(int32_t lMapIndex, int32_t x, int32_t y)
{
	LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!tree)
		return false;

	return (!tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT));
}

bool SECTREE_MANAGER::GetMovablePosition(int32_t lMapIndex, int32_t x, int32_t y, PIXEL_POSITION & pos)
{
	int i = 0;

	do
	{
		int32_t dx = x + aArroundCoords[i].x;
		int32_t dy = y + aArroundCoords[i].y;

		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, dx, dy);

		if (!tree)
			continue;

		if (!tree->IsAttr(dx, dy, ATTR_BLOCK | ATTR_OBJECT))
		{
			pos.x = dx;
			pos.y = dy;
			return true;
		}
	} while (++i < ARROUND_COORD_MAX_NUM);

	pos.x = x;
	pos.y = y;
	return false;
}

bool SECTREE_MANAGER::GetValidLocation(int32_t lMapIndex, int32_t x, int32_t y, int32_t& r_lValidMapIndex, PIXEL_POSITION & r_pos, uint8_t empire)
{
	LPSECTREE_MAP pkSectreeMap = GetMap(lMapIndex);

	if (!pkSectreeMap)
	{
		if (lMapIndex >= 10000)
		{
/*			int32_t m = lMapIndex / 10000;
			if (m == 216)
			{
				if (GetRecallPositionByEmpire (m, empire, r_pos))
				{
					r_lValidMapIndex = m;
					return true;
				}
				else
					return false;
			}*/
			return GetValidLocation(lMapIndex / 10000, x, y, r_lValidMapIndex, r_pos);
		}
		else
		{
			LOG_ERROR("cannot find sectree_map by map index {}", lMapIndex);
			return false;
		}
	}

	int32_t lRealMapIndex = lMapIndex;

	if (lRealMapIndex >= 10000)
		lRealMapIndex = lRealMapIndex / 10000;

	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (rRegion.index == lRealMapIndex)
		{
			LPSECTREE tree = pkSectreeMap->Find(x, y);

			if (!tree)
			{
				LOG_ERROR("cannot find tree by {} {} (map index {})", x, y, lMapIndex);
				return false;
			}

			r_lValidMapIndex = lMapIndex;
			r_pos.x = x;
			r_pos.y = y;
			return true;
		}
	}

	LOG_ERROR("invalid location (map index {} {} x {})", lRealMapIndex, x, y);
	return false;
}

bool SECTREE_MANAGER::GetRandomLocation(int32_t lMapIndex, PIXEL_POSITION & r_pos, uint32_t dwCurrentX, uint32_t dwCurrentY, int iMaxDistance)
{
	LPSECTREE_MAP pkSectreeMap = GetMap(lMapIndex);

	if (!pkSectreeMap)
		return false;

	uint32_t x, y;

	std::vector<TMapRegion>::iterator it = m_vec_mapRegion.begin();

	while (it != m_vec_mapRegion.end())
	{
		TMapRegion & rRegion = *(it++);

		if (rRegion.index != lMapIndex)
			continue;

		int i = 0;

		while (i++ < 100)
		{
			x = number(rRegion.sx + 50, rRegion.ex - 50);
			y = number(rRegion.sy + 50, rRegion.ey - 50);

			if (iMaxDistance != 0)
			{
				int d;

				d = abs((float)dwCurrentX - x);

				if (d > iMaxDistance)
				{
					if (x < dwCurrentX)
						x = dwCurrentX - iMaxDistance;
					else
						x = dwCurrentX + iMaxDistance;
				}

				d = abs((float)dwCurrentY - y);

				if (d > iMaxDistance)
				{
					if (y < dwCurrentY)
						y = dwCurrentY - iMaxDistance;
					else
						y = dwCurrentY + iMaxDistance;
				}
			}

			LPSECTREE tree = pkSectreeMap->Find(x, y);

			if (!tree)
				continue;

			if (tree->IsAttr(x, y, ATTR_BLOCK | ATTR_OBJECT))
				continue;

			r_pos.x = x;
			r_pos.y = y;
			return true;
		}
	}

	return false;
}

int32_t SECTREE_MANAGER::CreatePrivateMap(int32_t lMapIndex)
{
	if (lMapIndex >= 10000) // 10000�� �̻��� ���� ����. (Ȥ�� �̹� private �̴�)
		return 0;

	LPSECTREE_MAP pkMapSectree = GetMap(lMapIndex);

	if (!pkMapSectree)
	{
		LOG_ERROR("Cannot find map index {}", lMapIndex);
		return 0;
	}

	// <Factor> Circular private map indexing
	int32_t base = lMapIndex * 10000;
	int index_cap = 10000;
	if ( lMapIndex == 107 || lMapIndex == 108 || lMapIndex == 109 ) {
		index_cap = (test_server ? 1 : 51);
	}
	PrivateIndexMapType::iterator it = next_private_index_map_.find(lMapIndex);
	if (it == next_private_index_map_.end()) {
		it = next_private_index_map_.insert(PrivateIndexMapType::value_type(lMapIndex, 0)).first;
	}
	int i, next_index = it->second;
	for (i = 0; i < index_cap; ++i) {
		if (GetMap(base + next_index) == nullptr) {
			break; // available
		}
		if (++next_index >= index_cap) {
			next_index = 0;
		}
	}
	if (i == index_cap) {
		// No available index
		return 0;
	}
	int32_t lNewMapIndex = base + next_index;
	if (++next_index >= index_cap) {
		next_index = 0;
	}
	it->second = next_index;

	/*
	int i;

	for (i = 0; i < 10000; ++i)
	{
		if (!GetMap((lMapIndex * 10000) + i))
			break;
	}

	if ( test_server )
		LOG_INFO("Create Dungeon : OrginalMapindex {} NewMapindex {}", lMapIndex, i);

	if ( lMapIndex == 107 || lMapIndex == 108 || lMapIndex == 109 )
	{
		if ( test_server )
		{
			if ( i > 0 )
				return NULL;
		}
		else
		{
			if ( i > 50 )
				return NULL;

		}
	}

	if (i == 10000)
	{
		LOG_ERROR("not enough private map index (map_index {})", lMapIndex);
		return 0;
	}

	int32_t lNewMapIndex = lMapIndex * 10000 + i;
	*/

	pkMapSectree = M2_NEW SECTREE_MAP(*pkMapSectree);
	m_map_pkSectree.insert(std::map<uint32_t, LPSECTREE_MAP>::value_type(lNewMapIndex, pkMapSectree));

	LOG_INFO("PRIVATE_MAP: {} created (original {})", lNewMapIndex, lMapIndex);
	return lNewMapIndex;
}

struct FDestroyPrivateMapEntity
{
	void operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;
			//0, "PRIVAE_MAP: removing character %s", ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data());

			if (ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null)))
				DESC_MANAGER::instance().DestroyDesc(ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null)));
			else
				M2_DESTROY_CHARACTER(ch);
		}
		else if (ent->IsType(ENTITY_ITEM))
		{
			LPITEM item = (LPITEM) ent;
			LOG_INFO("PRIVATE_MAP: removing item {}", item->GetName());

			ItemSystem::DestroyItemEntityEcs(
				(item ? item->GetEntityHandle() : entt::null),
				"PRIVATE_MAP_ITEM_CLEANUP");
		}
		else
			LOG_ERROR("PRIVAE_MAP: trying to remove unknown entity {}", ent->GetType());
	}
};

void SECTREE_MANAGER::DestroyPrivateMap(int32_t lMapIndex)
{
	if (lMapIndex < 10000) // private map �� �ε����� 10000 �̻� �̴�.
		return;

	LPSECTREE_MAP pkMapSectree = GetMap(lMapIndex);

	if (!pkMapSectree)
		return;

	// �� �� ���� ���� �����ϴ� �͵��� ���� ���ش�.
	// WARNING:
	// �� �ʿ� ������ � Sectree���� �������� ���� �� ����
	// ���� ���⼭ delete �� �� �����Ƿ� �����Ͱ� ���� �� ������
	// ���� ó���� �ؾ���
	FDestroyPrivateMapEntity f;
	pkMapSectree->for_each(f);

	m_map_pkSectree.erase(lMapIndex);


#ifdef ENABLE_DUNGEON_BUGFIXES
	int originIndex = (int)floor(lMapIndex / 10000);
	PrivateIndexMapType::iterator it = next_private_index_map_.find(originIndex);
	if (it != next_private_index_map_.end())
	{
		if (lMapIndex - originIndex * 10000 < it->second)
			it->second = lMapIndex - originIndex * 10000;
	}
#endif

	M2_DELETE(pkMapSectree);

	LOG_INFO("PRIVATE_MAP: {} destroyed", lMapIndex);
}

TAreaMap& SECTREE_MANAGER::GetDungeonArea(int32_t lMapIndex)
{
	auto it = m_map_pkArea.find(lMapIndex);

	if (it == m_map_pkArea.end())
	{
		return m_map_pkArea[-1]; // �ӽ÷� �� Area�� ����
	}
	return it->second;
}

void SECTREE_MANAGER::SendNPCPosition(LPCHARACTER ch)
{
	LPDESC d = ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null));
	if (!d)
		return;

	int32_t lMapIndex = ecs::PlayerRuntime::GetMapIndex(((ch) ? (ch)->GetEntityHandle() : entt::null));

	if (m_mapNPCPosition[lMapIndex].empty())
		return;

	TEMP_BUFFER buf;
	TPacketGCNPCPosition p;
	p.header = HEADER_GC_NPC_POSITION;
	p.count = m_mapNPCPosition[lMapIndex].size();

	TNPCPosition np = {};

	// TODO m_mapNPCPosition[lMapIndex] �� �����ּ���


	for (auto it = m_mapNPCPosition[lMapIndex].begin(); it != m_mapNPCPosition[lMapIndex].end(); ++it)
	{
		np.bType = it->bType;
#ifdef ENABLE_MULTI_NAMES
		np.name = it->name;
#else
		strlcpy(np.name, it->name, sizeof(np.name));
#endif

		LOG_INFO("Name ID: {}", np.name);
		LOG_INFO("X: {}", np.x);
		LOG_INFO("Y: {}", np.y);

		np.x = it->x;
		np.y = it->y;
		buf.write(&np, sizeof(np));
	}
	// checkoutban van egy commit

	p.size = sizeof(p) + buf.size();

	if (buf.size())
	{
		d->BufferedPacket(&p, sizeof(TPacketGCNPCPosition));
		d->Packet(buf.read_peek(), buf.size());
	}
	else
		d->Packet(&p, sizeof(TPacketGCNPCPosition));
}

void SECTREE_MANAGER::InsertNPCPosition(int32_t lMapIndex, uint8_t bType,
#ifdef ENABLE_MULTI_NAMES
uint32_t szName
#else
const char* szName
#endif
, int32_t x, int32_t y)
{
	m_mapNPCPosition[lMapIndex].emplace_back(bType, szName, x, y);
}

#ifdef ENABLE_ATLAS_BOSS
void SECTREE_MANAGER::SendBossPosition(LPCHARACTER ch)
{
	LPDESC d = ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null));
	if (!d)
		return;

	int32_t lMapIndex = ecs::PlayerRuntime::GetMapIndex(((ch) ? (ch)->GetEntityHandle() : entt::null));

	TEMP_BUFFER buf;
	TPacketGCBossPosition p;
	p.bHeader = HEADER_GC_BOSS_POSITION;
	p.wCount = m_mapBossPosition[lMapIndex].size();

	TBossPosition bp;

	for (auto it = m_mapBossPosition[lMapIndex].begin(); it != m_mapBossPosition[lMapIndex].end(); ++it)
	{
		bp.bType = it->bType;
#ifdef ENABLE_MULTI_NAMES
		bp.szName = it->szName;
#else
		strlcpy(bp.szName, it->szName, sizeof(bp.szName));
#endif
		bp.lX = it->lX;
		bp.lY = it->lY;
		bp.lTime = it->lTime;
		buf.write(&bp, sizeof(bp));
	}

	p.wSize = sizeof(p) + buf.size();

	if (buf.size())
	{
		d->BufferedPacket(&p, sizeof(TPacketGCBossPosition));
		d->Packet(buf.read_peek(), buf.size());
	}
	else
		d->Packet(&p, sizeof(TPacketGCBossPosition));
}

void SECTREE_MANAGER::InsertBossPosition(int32_t lMapIndex, uint8_t bType,
#ifdef ENABLE_MULTI_NAMES
uint32_t szName
#else
const char* szName
#endif
, int32_t lX, int32_t lY, int32_t lTime)
{
	m_mapBossPosition[lMapIndex].push_back(boss_info(bType, szName, lX, lY, lTime));
}
#endif

uint8_t SECTREE_MANAGER::GetEmpireFromMapIndex(int32_t lMapIndex)
{
	if (lMapIndex >= 1 && lMapIndex <= 20)
		return 1;

	if (lMapIndex >= 21 && lMapIndex <= 40)
		return 2;

	if (lMapIndex >= 41 && lMapIndex <= 60)
		return 3;

	if ( lMapIndex == 184 || lMapIndex == 185 )
		return 1;

	if ( lMapIndex == 186 || lMapIndex == 187 )
		return 2;

	if ( lMapIndex == 188 || lMapIndex == 189 )
		return 3;

	switch ( lMapIndex )
	{
		case 190 :
			return 1;
		case 191 :
			return 2;
		case 192 :
			return 3;
	}

	return 0;
}

class FRemoveIfAttr
{
	public:
		FRemoveIfAttr(LPSECTREE pkTree, uint32_t dwAttr) : m_pkTree(pkTree), m_dwCheckAttr(dwAttr)
		{
		}

		void operator () (LPENTITY entity)
		{
			if (!m_pkTree->IsAttr(entity->GetX(), entity->GetY(), m_dwCheckAttr))
				return;

			if (entity->IsType(ENTITY_ITEM))
			{
				LPITEM item = (LPITEM) entity;
				ItemSystem::DestroyItemEntityEcs(
					(item ? item->GetEntityHandle() : entt::null),
					"SECTREE_ATTR_ITEM_CLEANUP");
			}
			else if (entity->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) entity;

				if ((ecs::PlayerRuntime::IsPC(((ch) ? (ch)->GetEntityHandle() : entt::null))))
				{
					PIXEL_POSITION pos;

					if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ecs::PlayerRuntime::GetMapIndex(((ch) ? (ch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetEmpire(((ch) ? (ch)->GetEntityHandle() : entt::null)), pos))
						ecs::MovementSystem::WarpSet(((ch) ? (ch)->GetEntityHandle() : entt::null), pos.x, pos.y);
					else
						ecs::MovementSystem::WarpSet(((ch) ? (ch)->GetEntityHandle() : entt::null), EMPIRE_START_X(ecs::PlayerRuntime::GetEmpire(((ch) ? (ch)->GetEntityHandle() : entt::null))), EMPIRE_START_Y(ecs::PlayerRuntime::GetEmpire(((ch) ? (ch)->GetEntityHandle() : entt::null))));
				}
				else
					ch->Dead();
			}
		}

		LPSECTREE m_pkTree;
		uint32_t m_dwCheckAttr;
};

bool SECTREE_MANAGER::ForAttrRegionCell(int32_t lMapIndex, int32_t lCX, int32_t lCY, uint32_t dwAttr, EAttrRegionMode mode )
{
	SECTREEID id;

	id.coord.x = lCX / (SECTREE_SIZE / CELL_SIZE);
	id.coord.y = lCY / (SECTREE_SIZE / CELL_SIZE);

	int32_t lTreeCX = id.coord.x * (SECTREE_SIZE / CELL_SIZE);
	int32_t lTreeCY = id.coord.y * (SECTREE_SIZE / CELL_SIZE);

	LPSECTREE pSec = Get( lMapIndex, id.package );
	if ( !pSec )
		return false;

	switch (mode)
	{
		case ATTR_REGION_MODE_SET:
			pSec->SetAttribute( lCX - lTreeCX, lCY - lTreeCY, dwAttr );
			break;

		case ATTR_REGION_MODE_REMOVE:
			pSec->RemoveAttribute( lCX - lTreeCX, lCY - lTreeCY, dwAttr );
			break;

		case ATTR_REGION_MODE_CHECK:
			if ( pSec->IsAttr( lCX * CELL_SIZE, lCY * CELL_SIZE, ATTR_OBJECT ) )
				return true;
			break;

		default:
			LOG_ERROR("Unknown region mode {}", mode);
			break;
	}

	return false;
}

bool SECTREE_MANAGER::ForAttrRegionRightAngle(int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode )
{
	if (1 == lRotate/90 || 3 == lRotate/90)
	{
		for (int x = 0; x < lCH; ++x)
			for (int y = 0; y < lCW; ++y)
			{
				if ( ForAttrRegionCell( lMapIndex, lCX + x, lCY + y, dwAttr, mode ) )
					return true;
			}
	}
	if (0 == lRotate/90 || 2 == lRotate/90)
	{
		for (int x = 0; x < lCW; ++x)
			for (int y = 0; y < lCH; ++y)
			{
				if ( ForAttrRegionCell( lMapIndex, lCX + x, lCY + y, dwAttr, mode) )
					return true;
			}
	}

	return mode == ATTR_REGION_MODE_CHECK ? false : true;
}

#define min( l, r )	((l) < (r) ? (l) : (r))
#define max( l, r )	((l) < (r) ? (r) : (l))

bool SECTREE_MANAGER::ForAttrRegionFreeAngle(int32_t lMapIndex, int32_t lCX, int32_t lCY, int32_t lCW, int32_t lCH, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode )
{
	float fx1 = (-lCW/2) * sinf(float(lRotate)/180.0f*3.14f) + (-lCH/2) * cosf(float(lRotate)/180.0f*3.14f);
	float fy1 = (-lCW/2) * cosf(float(lRotate)/180.0f*3.14f) - (-lCH/2) * sinf(float(lRotate)/180.0f*3.14f);

	float fx2 = (+lCW/2) * sinf(float(lRotate)/180.0f*3.14f) + (-lCH/2) * cosf(float(lRotate)/180.0f*3.14f);
	float fy2 = (+lCW/2) * cosf(float(lRotate)/180.0f*3.14f) - (-lCH/2) * sinf(float(lRotate)/180.0f*3.14f);

	float fx3 = (-lCW/2) * sinf(float(lRotate)/180.0f*3.14f) + (+lCH/2) * cosf(float(lRotate)/180.0f*3.14f);
	float fy3 = (-lCW/2) * cosf(float(lRotate)/180.0f*3.14f) - (+lCH/2) * sinf(float(lRotate)/180.0f*3.14f);

	float fx4 = (+lCW/2) * sinf(float(lRotate)/180.0f*3.14f) + (+lCH/2) * cosf(float(lRotate)/180.0f*3.14f);
	float fy4 = (+lCW/2) * cosf(float(lRotate)/180.0f*3.14f) - (+lCH/2) * sinf(float(lRotate)/180.0f*3.14f);

	float fdx1 = fx2 - fx1;
	float fdy1 = fy2 - fy1;
	float fdx2 = fx1 - fx3;
	float fdy2 = fy1 - fy3;

	if (0 == fdx1 || 0 == fdx2)
	{
		LOG_ERROR("SECTREE_MANAGER::ForAttrRegion - Unhandled exception. MapIndex: {}", lMapIndex);
		return false;
	}

	float fTilt1 = float(fdy1) / float(fdx1);
	float fTilt2 = float(fdy2) / float(fdx2);
	float fb1 = fy1 - fTilt1*fx1;
	float fb2 = fy1 - fTilt2*fx1;
	float fb3 = fy4 - fTilt1*fx4;
	float fb4 = fy4 - fTilt2*fx4;

	float fxMin = min(fx1, min(fx2, min(fx3, fx4)));
	float fxMax = max(fx1, max(fx2, max(fx3, fx4)));
	for (int i = int(fxMin); i < int(fxMax); ++i)
	{
		float fyValue1 = fTilt1*i + min(fb1, fb3);
		float fyValue2 = fTilt2*i + min(fb2, fb4);

		float fyValue3 = fTilt1*i + max(fb1, fb3);
		float fyValue4 = fTilt2*i + max(fb2, fb4);

		float fMinValue;
		float fMaxValue;
		if (abs(int(fyValue1)) < abs(int(fyValue2)))
			fMaxValue = fyValue1;
		else
			fMaxValue = fyValue2;
		if (abs(int(fyValue3)) < abs(int(fyValue4)))
			fMinValue = fyValue3;
		else
			fMinValue = fyValue4;

		for (int j = int(min(fMinValue, fMaxValue)); j < int(max(fMinValue, fMaxValue)); ++j) {
			if ( ForAttrRegionCell( lMapIndex, lCX + (lCW / 2) + i, lCY + (lCH / 2) + j, dwAttr, mode ) )
				return true;
		}
	}

	return mode == ATTR_REGION_MODE_CHECK ? false : true;
}

bool SECTREE_MANAGER::ForAttrRegion(int32_t lMapIndex, int32_t lStartX, int32_t lStartY, int32_t lEndX, int32_t lEndY, int32_t lRotate, uint32_t dwAttr, EAttrRegionMode mode)
{
	LPSECTREE_MAP pkMapSectree = GetMap(lMapIndex);

	if (!pkMapSectree)
	{
		LOG_ERROR("Cannot find SECTREE_MAP by map index {}", lMapIndex);
		return mode == ATTR_REGION_MODE_CHECK ? true : false;
	}

	//
	// ������ ��ǥ�� Cell �� ũ�⿡ ���� Ȯ���Ѵ�.
	//

	lStartX	-= lStartX % CELL_SIZE;
	lStartY	-= lStartY % CELL_SIZE;
	lEndX	+= lEndX % CELL_SIZE;
	lEndY	+= lEndY % CELL_SIZE;

	//
	// Cell ��ǥ�� ���Ѵ�.
	//

	int32_t lCX = lStartX / CELL_SIZE;
	int32_t lCY = lStartY / CELL_SIZE;
	int32_t lCW = (lEndX - lStartX) / CELL_SIZE;
	int32_t lCH = (lEndY - lStartY) / CELL_SIZE;

	LOG_INFO("ForAttrRegion {} {} ~ {} {}", lStartX, lStartY, lEndX, lEndY);

	lRotate = lRotate % 360;

	if (0 == lRotate % 90)
		return ForAttrRegionRightAngle( lMapIndex, lCX, lCY, lCW, lCH, lRotate, dwAttr, mode );

	return ForAttrRegionFreeAngle( lMapIndex, lCX, lCY, lCW, lCH, lRotate, dwAttr, mode );
}

bool SECTREE_MANAGER::SaveAttributeToImage(int32_t lMapIndex, const char * c_pszFileName, LPSECTREE_MAP pMapSrc)
{
	LPSECTREE_MAP pMap = SECTREE_MANAGER::GetMap(lMapIndex);

	if (!pMap)
	{
		if (pMapSrc)
			pMap = pMapSrc;
		else
		{
			LOG_ERROR("cannot find sectree_map {}", lMapIndex);
			return false;
		}
	}

	int iMapHeight = pMap->m_setting.iHeight / 128 / 200;
	int iMapWidth = pMap->m_setting.iWidth / 128 / 200;

	if (iMapHeight < 0 || iMapWidth < 0)
	{
		LOG_ERROR("map size error w {} h {}", iMapWidth, iMapHeight);
		return false;
	}

	LOG_INFO("SaveAttributeToImage w {} h {} file {}", iMapWidth, iMapHeight, c_pszFileName);

	CTargaImage image;

	image.Create(512 * iMapWidth, 512 * iMapHeight);

	LOG_INFO("1");

	uint32_t * pdwDest = (uint32_t *) image.GetBasePointer();

	int pixels = 0;
	int x, x2;
	int y, y2;

	LOG_INFO("2 {}", static_cast<const void*>(pdwDest));

	uint32_t * pdwLine = M2_NEW uint32_t[SECTREE_SIZE / CELL_SIZE];

	for (y = 0; y < 4 * iMapHeight; ++y)
	{
		for (y2 = 0; y2 < SECTREE_SIZE / CELL_SIZE; ++y2)
		{
			for (x = 0; x < 4 * iMapWidth; ++x)
			{
				SECTREEID id;

				id.coord.x = x + pMap->m_setting.iBaseX / SECTREE_SIZE;
				id.coord.y = y + pMap->m_setting.iBaseY / SECTREE_SIZE;

				LPSECTREE pSec = pMap->Find(id.package);

				if (!pSec)
				{
					LOG_ERROR("cannot get sectree for {} {} {} {}", static_cast<int32_t>(id.coord.x), static_cast<int32_t>(id.coord.y), pMap->m_setting.iBaseX, pMap->m_setting.iBaseY);
					continue;
				}

				pSec->m_pkAttribute->CopyRow(y2, pdwLine);

				if (!pdwLine)
				{
					LOG_ERROR("cannot get attribute line pointer");
					M2_DELETE_ARRAY(pdwLine);
					continue;
				}

				for (x2 = 0; x2 < SECTREE_SIZE / CELL_SIZE; ++x2)
				{
					uint32_t dwColor;

					if (IS_SET(pdwLine[x2], ATTR_WATER))
						dwColor = 0xff0000ff;
					else if (IS_SET(pdwLine[x2], ATTR_BANPK))
						dwColor = 0xff00ff00;
					else if (IS_SET(pdwLine[x2], ATTR_BLOCK))
						dwColor = 0xffff0000;
					else
						dwColor = 0xffffffff;

					*(pdwDest++) = dwColor;
					pixels++;
				}
			}
		}
	}

	M2_DELETE_ARRAY(pdwLine);
	LOG_INFO("3");

	if (image.Save(c_pszFileName))
	{
		LOG_INFO("SECTREE: map {} attribute saved to {} ({} bytes)", lMapIndex, c_pszFileName, pixels);
		return true;
	}
	else
	{
		LOG_ERROR("cannot save file, map_index {} filename {}", lMapIndex, c_pszFileName);
		return false;
	}
}

struct FPurgeMonsters
{
	void operator() (LPENTITY ent)
	{
		if ( ent->IsType(ENTITY_CHARACTER) == true )
		{
			LPCHARACTER lpChar = (LPCHARACTER)ent;

#ifdef __NEWPET_SYSTEM__
			if (lpChar->IsMonster() == true && !lpChar->IsPet() && !lpChar->IsNewPet())
#else
			if ( lpChar->IsMonster() == true && !lpChar->IsPet())
#endif
			{
				M2_DESTROY_CHARACTER(lpChar);
			}
		}
	}
};

void SECTREE_MANAGER::PurgeMonstersInMap(int32_t lMapIndex)
{
	LPSECTREE_MAP sectree = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if ( sectree != nullptr)
	{
		struct FPurgeMonsters f;

		sectree->for_each( f );
	}
}

struct FPurgeStones
{
	void operator() (LPENTITY ent)
	{
		if ( ent->IsType(ENTITY_CHARACTER) == true )
		{
			LPCHARACTER lpChar = (LPCHARACTER)ent;

			if ( ecs::PlayerRuntime::IsStone(((lpChar) ? (lpChar)->GetEntityHandle() : entt::null)) == true )
			{
				M2_DESTROY_CHARACTER(lpChar);
			}
		}
	}
};

void SECTREE_MANAGER::PurgeStonesInMap(int32_t lMapIndex)
{
	LPSECTREE_MAP sectree = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if ( sectree != nullptr)
	{
		struct FPurgeStones f;

		sectree->for_each( f );
	}
}

struct FPurgeNPCs
{
	void operator() (LPENTITY ent)
	{
		if ( ent->IsType(ENTITY_CHARACTER) == true )
		{
			LPCHARACTER lpChar = (LPCHARACTER)ent;

#ifdef __NEWPET_SYSTEM__
			if (ecs::PlayerRuntime::IsNPC(((lpChar) ? (lpChar)->GetEntityHandle() : entt::null)) == true && !lpChar->IsPet() && !lpChar->IsNewPet())
#else
			if ( ecs::PlayerRuntime::IsNPC(((lpChar) ? (lpChar)->GetEntityHandle() : entt::null)) == true && !lpChar->IsPet())
#endif
			{
				M2_DESTROY_CHARACTER(lpChar);
			}
		}
	}
};

void SECTREE_MANAGER::PurgeNPCsInMap(int32_t lMapIndex)
{
	LPSECTREE_MAP sectree = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if ( sectree != nullptr)
	{
		struct FPurgeNPCs f;

		sectree->for_each( f );
	}
}

struct FCountMonsters
{
	std::map<entt::entity, entt::entity> m_map_Monsters;

	void operator() (LPENTITY ent)
	{
		if ( ent->IsType(ENTITY_CHARACTER) == true )
		{
			LPCHARACTER lpChar = (LPCHARACTER)ent;

			if ( lpChar->IsMonster() == true )
			{
				const entt::entity e = lpChar->GetEntityHandle(); if (e != entt::null) m_map_Monsters[e] = e;
			}
		}
	}
};

#ifdef __VERSION_162__
struct SRestartCityPos
{
	SRestartCityPos(int i2Empire = 0, int i2X = 0, int i2Y = 0, int i2Z = 0) : iEmpire(i2Empire), iX(i2X), iY(i2Y), iZ(i2Z) {}
	int iEmpire, iX, iY, iZ;
};

static std::map<int, SRestartCityPos> m_restart_city_pos;

void SECTREE_MANAGER::GetRestartCityPos(int iMapIndex, int iEmpire, int &iTargetX, int &iTargetY, int &iTargetZ)
{
	auto iter = m_restart_city_pos.begin();
	for (; iter != m_restart_city_pos.end(); ++iter)
	{
		if (iter->first == iMapIndex)
		{
			if (iter->second.iEmpire == iEmpire)
			{
				iTargetX = iter->second.iX;
				iTargetY = iter->second.iY;
				iTargetZ = iter->second.iZ;
				break;
			}
		}
	}

	return;
}

void SECTREE_MANAGER::AddRestartCityPos(int iMapIndex, int iEmpire, int iX, int iY, int iZ)
{
	int iTargetX = 0, iTargetY = 0, iTargetZ = 0;
	SECTREE_MANAGER::instance().GetRestartCityPos(iMapIndex, iEmpire, iTargetX, iTargetY, iTargetZ);
	if ((iTargetX != 0) && (iTargetY != 0))
		return;

	m_restart_city_pos.insert(std::make_pair(iMapIndex, SRestartCityPos(iEmpire, iX, iY, iZ)));
}
#endif

size_t SECTREE_MANAGER::GetMonsterCountInMap(int32_t lMapIndex)
{
	LPSECTREE_MAP sectree = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if ( sectree != nullptr)
	{
		struct FCountMonsters f;

		sectree->for_each( f );

		return f.m_map_Monsters.size();
	}

	return 0;
}

struct FCountSpecifiedMonster
{
	uint32_t SpecifiedVnum;
	size_t cnt;

	FCountSpecifiedMonster(uint32_t id)
		: SpecifiedVnum(id), cnt(0)
	{}

	void operator() (LPENTITY ent)
	{
		if (true == ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER pChar = static_cast<LPCHARACTER>(ent);
			if (true == ecs::PlayerRuntime::IsStone(((pChar) ? (pChar)->GetEntityHandle() : entt::null)))
			{
				if (pChar->GetMobTable().dwVnum == SpecifiedVnum)
					cnt++;
			}
		}
	}
};

size_t SECTREE_MANAGER::GetMonsterCountInMap(int32_t lMapIndex, uint32_t dwVnum)
{
	LPSECTREE_MAP sectree = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if (nullptr != sectree)
	{
		struct FCountSpecifiedMonster f(dwVnum);
		sectree->for_each( f );
		return f.cnt;
	}

	return 0;
}


