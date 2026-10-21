#include "stdafx.h"
#include "motion.h"
#include "mob_manager.h"
#include "db.h"

#include <Core/Logging.hpp>

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace
{
	float DurationSeconds(float milliseconds)
	{
		return std::isfinite(milliseconds) && milliseconds > 0.0f ? milliseconds * 0.001f : 0.0f;
	}
	struct SSplashRow
	{
		int	skillIndex { 0 };
		uint32_t	timing { 0 };
		uint32_t	hitDistance { 0 };
	};

	void AssignMotionRow(const MYSQL_ROW row, CMotionManager::SMotionData& out)
	{
		out.walkSpeed = static_cast<float>(atof(row[1] ? row[1] : "0"));
		out.runSpeed = static_cast<float>(atof(row[2] ? row[2] : "0"));
		out.attackDuration = static_cast<float>(atof(row[3] ? row[3] : "0"));

		for (int i = 0; i < MOB_SKILL_MAX_NUM; ++i)
			out.skillDuration[i] = static_cast<float>(atof(row[4 + i] ? row[4 + i] : "0"));
	}
}

bool CMotionManager::Build()
{
	m_map_motion.clear();

	std::map<std::string, SMotionData> folderMotion;

	{
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
				"SELECT folder, walk_speed, run_speed, attack_duration, "
				"skill1_duration, skill2_duration, skill3_duration, skill4_duration, skill5_duration "
				"FROM mob_motion"));

		if (msg && msg->Get())
		{
			for (uint64_t i = 0; i < msg->Get()->uiNumRows; ++i)
			{
				MYSQL_ROW row = msg->Get()->FetchRow();

				if (!row || !row[0] || !row[0][0])
					continue;

				SMotionData data;
				AssignMotionRow(row, data);
				folderMotion[row[0]] = data;
			}
		}
	}

	std::map<std::string, std::vector<SSplashRow>> folderSplash;

	{
		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(
				"SELECT folder, skill_index, timing_ms, hit_distance "
				"FROM mob_skill_splash ORDER BY folder, skill_index, event_index"));

		if (msg && msg->Get())
		{
			for (uint64_t i = 0; i < msg->Get()->uiNumRows; ++i)
			{
				MYSQL_ROW row = msg->Get()->FetchRow();

				if (!row || !row[0] || !row[0][0])
					continue;

				SSplashRow splash;
				splash.skillIndex = row[1] ? atoi(row[1]) : 0;
				splash.timing = static_cast<uint32_t>(row[2] ? strtoul(row[2], nullptr, 10) : 0);
				// Same conversion the old .msa loader applied: the raw signed
				// position is stored in the DB and reinterpreted here.
				splash.hitDistance = static_cast<uint32_t>(static_cast<int32_t>(row[3] ? atoi(row[3]) : 0));

				folderSplash[row[0]].push_back(splash);
			}
		}
	}

	uint32_t loadedMotions = 0;
	uint32_t loadedSplashes = 0;

	CMobManager::iterator it = CMobManager::instance().begin();

	for (; it != CMobManager::instance().end(); ++it)
	{
		CMob* mob = it->second;

		if (!mob)
			continue;

		for (int i = 0; i < MOB_SKILL_MAX_NUM; ++i)
		{
			mob->m_mobSkillInfo[i].dwSkillVnum = 0;
			mob->m_mobSkillInfo[i].bSkillLevel = 0;
			mob->m_mobSkillInfo[i].vecSplashAttack.clear();
		}

		const char* folder = mob->m_table.szFolder;

		if (!folder || !folder[0])
			continue;

		const auto motionIt = folderMotion.find(folder);

		if (motionIt != folderMotion.end())
		{
			m_map_motion[mob->m_table.dwVnum] = motionIt->second;
			++loadedMotions;
		}

		const auto splashIt = folderSplash.find(folder);

		if (splashIt != folderSplash.end())
		{
			for (const SSplashRow& splash : splashIt->second)
			{
				if (splash.skillIndex < 0 || splash.skillIndex >= MOB_SKILL_MAX_NUM)
					continue;

				mob->AddSkillSplash(splash.skillIndex, splash.timing, splash.hitDistance);
				mob->m_mobSkillInfo[splash.skillIndex].dwSkillVnum = mob->m_table.Skills[splash.skillIndex].dwVnum;
				mob->m_mobSkillInfo[splash.skillIndex].bSkillLevel = mob->m_table.Skills[splash.skillIndex].bLevel;
				++loadedSplashes;
			}
		}
	}

	LOG_INFO("Motion: {} mob motions, {} skill splashes loaded from the DB", loadedMotions, loadedSplashes);
	return true;
}

float CMotionManager::GetMotionDuration(uint32_t dwVnum, uint32_t dwKey)
{
	const auto it = m_map_motion.find(dwVnum);

	if (it == m_map_motion.end())
		return 0.0f;

	if (GET_MOTION_MODE(dwKey) != MOTION_MODE_GENERAL)
		return 0.0f;

	const uint16_t index = GET_MOTION_INDEX(dwKey);

	switch (index)
	{
		case MOTION_NORMAL_ATTACK:	return DurationSeconds(it->second.attackDuration);
		case MOTION_SPECIAL_1:		return DurationSeconds(it->second.skillDuration[0]);
		case MOTION_SPECIAL_2:		return DurationSeconds(it->second.skillDuration[1]);
		case MOTION_SPECIAL_3:		return DurationSeconds(it->second.skillDuration[2]);
		case MOTION_SPECIAL_4:		return DurationSeconds(it->second.skillDuration[3]);
		case MOTION_SPECIAL_5:		return DurationSeconds(it->second.skillDuration[4]);
		default:			return 0.0f;
	}
}

float CMotionManager::GetMoveSpeed(uint32_t dwVnum, bool walking) const
{
	const auto it = m_map_motion.find(dwVnum);

	if (it == m_map_motion.end())
		return 0.0f;

	const float speed = walking ? it->second.walkSpeed : it->second.runSpeed;
	return (std::isfinite(speed) && speed > 0.0f) ? speed : 0.0f;
}
