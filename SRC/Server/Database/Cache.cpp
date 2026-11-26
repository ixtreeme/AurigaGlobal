
#include "stdafx.h"
#include "Cache.h"

#include "QID.h"
#include "ClientManager.h"
#include "Main.h"

extern CPacketInfo g_item_info;
extern int g_iPlayerCacheFlushSeconds;
extern int g_test_server;

#ifdef __SKILL_COLOR_SYSTEM__
extern int g_iSkillColorCacheFlushSeconds;

CSKillColorCache::CSKillColorCache()
{
	m_expireTime = std::chrono::seconds(MIN(1800, g_iSkillColorCacheFlushSeconds));
}

CSKillColorCache::~CSKillColorCache()
{
}

void CSKillColorCache::OnFlush()
{
	char query[QUERY_MAX_LEN];
	snprintf(query, sizeof(query),
		"INSERT INTO skill_color%s (`player_id`, "
		"`s1_col1`, `s1_col2`, `s1_col3`, `s1_col4`, `s1_col5`, "
		"`s2_col1`, `s2_col2`, `s2_col3`, `s2_col4`, `s2_col5`, "
		"`s3_col1`, `s3_col2`, `s3_col3`, `s3_col4`, `s3_col5`, "
		"`s4_col1`, `s4_col2`, `s4_col3`, `s4_col4`, `s4_col5`, "
		"`s5_col1`, `s5_col2`, `s5_col3`, `s5_col4`, `s5_col5`, "
		"`s6_col1`, `s6_col2`, `s6_col3`, `s6_col4`, `s6_col5`, " // end of skills
		"`s7_col1`, `s7_col2`, `s7_col3`, `s7_col4`, `s7_col5`, " // begin buffs
		"`s8_col1`, `s8_col2`, `s8_col3`, `s8_col4`, `s8_col5`, "
		"`s9_col1`, `s9_col2`, `s9_col3`, `s9_col4`, `s9_col5`, "
		"`s10_col1`, `s10_col2`, `s10_col3`, `s10_col4`, `s10_col5`, "
		"`s11_col1`, `s11_col2`, `s11_col3`, `s11_col4`, `s11_col5`) "
		"VALUES (%u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, " // end of skills
		"%u, %u, %u, %u, %u, " // begin buffs
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u, "
		"%u, %u, %u, %u, %u) ON DUPLICATE KEY UPDATE "
		"s7_col1=VALUES(s7_col1),s7_col2=VALUES(s7_col2),s7_col3=VALUES(s7_col3),s7_col4=VALUES(s7_col4),s7_col5=VALUES(s7_col5),"
		"s8_col1=VALUES(s8_col1),s8_col2=VALUES(s8_col2),s8_col3=VALUES(s8_col3),s8_col4=VALUES(s8_col4),s8_col5=VALUES(s8_col5),"
		"s9_col1=VALUES(s9_col1),s9_col2=VALUES(s9_col2),s9_col3=VALUES(s9_col3),s9_col4=VALUES(s9_col4),s9_col5=VALUES(s9_col5),"
		"s10_col1=VALUES(s10_col1),s10_col2=VALUES(s10_col2),s10_col3=VALUES(s10_col3),s10_col4=VALUES(s10_col4),s10_col5=VALUES(s10_col5),"
		"s11_col1=VALUES(s11_col1),s11_col2=VALUES(s11_col2),s11_col3=VALUES(s11_col3),s11_col4=VALUES(s11_col4),s11_col5=VALUES(s11_col5)", GetTablePostfix(), m_data.player_id,
		m_data.dwSkillColor[0][0], m_data.dwSkillColor[0][1], m_data.dwSkillColor[0][2], m_data.dwSkillColor[0][3], m_data.dwSkillColor[0][4],
		m_data.dwSkillColor[1][0], m_data.dwSkillColor[1][1], m_data.dwSkillColor[1][2], m_data.dwSkillColor[1][3], m_data.dwSkillColor[1][4],
		m_data.dwSkillColor[2][0], m_data.dwSkillColor[2][1], m_data.dwSkillColor[2][2], m_data.dwSkillColor[2][3], m_data.dwSkillColor[2][4],
		m_data.dwSkillColor[3][0], m_data.dwSkillColor[3][1], m_data.dwSkillColor[3][2], m_data.dwSkillColor[3][3], m_data.dwSkillColor[3][4],
		m_data.dwSkillColor[4][0], m_data.dwSkillColor[4][1], m_data.dwSkillColor[4][2], m_data.dwSkillColor[4][3], m_data.dwSkillColor[4][4],
		m_data.dwSkillColor[5][0], m_data.dwSkillColor[5][1], m_data.dwSkillColor[5][2], m_data.dwSkillColor[5][3], m_data.dwSkillColor[5][4], // end of skills
		m_data.dwSkillColor[6][0], m_data.dwSkillColor[6][1], m_data.dwSkillColor[6][2], m_data.dwSkillColor[6][3], m_data.dwSkillColor[6][4], // begin buffs
		m_data.dwSkillColor[7][0], m_data.dwSkillColor[7][1], m_data.dwSkillColor[7][2], m_data.dwSkillColor[7][3], m_data.dwSkillColor[7][4],
		m_data.dwSkillColor[8][0], m_data.dwSkillColor[8][1], m_data.dwSkillColor[8][2], m_data.dwSkillColor[8][3], m_data.dwSkillColor[8][4],
		m_data.dwSkillColor[9][0], m_data.dwSkillColor[9][1], m_data.dwSkillColor[9][2], m_data.dwSkillColor[9][3], m_data.dwSkillColor[9][4],
		m_data.dwSkillColor[10][0], m_data.dwSkillColor[10][1], m_data.dwSkillColor[10][2], m_data.dwSkillColor[10][3], m_data.dwSkillColor[10][4]);

	CDBManager::instance().ReturnQuery(query, QID_SKILL_COLOR_SAVE, 0, nullptr);

	if (g_test_server)
		LOG_INFO("SkillColorCache::Flush {} ({})", m_data.player_id, query);

	m_bNeedQuery = false;
}
#endif

//
// CPlayerTableCache
//
CPlayerTableCache::CPlayerTableCache()
{
	m_expireTime = std::chrono::seconds(MIN(300, g_iPlayerCacheFlushSeconds));
}

CPlayerTableCache::~CPlayerTableCache()
{
}

void CPlayerTableCache::OnFlush()
{
	if (g_test_server)
		LOG_INFO("PlayerTableCache::Flush : {}", m_data.name);

	char szQuery[QUERY_MAX_LEN];
	CreatePlayerSaveQuery(szQuery, sizeof(szQuery), &m_data);
	CDBManager::instance().ReturnQuery(szQuery, QID_PLAYER_SAVE, 0, nullptr);
}
