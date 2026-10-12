#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <common/length.h>
#include <common/singleton.h>

enum EMotionMode
{
	MOTION_MODE_GENERAL,
	MOTION_MODE_ONEHAND_SWORD,
	MOTION_MODE_TWOHAND_SWORD,
	MOTION_MODE_DUALHAND_SWORD,
	MOTION_MODE_BOW,
	MOTION_MODE_BELL,
	MOTION_MODE_FAN,
	MOTION_MODE_HORSE,
	MOTION_MODE_MAX_NUM
};

enum EPublicMotion
{
	MOTION_NONE,
	MOTION_WAIT,
	MOTION_WALK,
	MOTION_RUN,
	MOTION_CHANGE_WEAPON,
	MOTION_DAMAGE,
	MOTION_DAMAGE_FLYING,
	MOTION_STAND_UP,
	MOTION_DAMAGE_BACK,
	MOTION_DAMAGE_FLYING_BACK,
	MOTION_STAND_UP_BACK,
	MOTION_DEAD,
	MOTION_DEAD_BACK,
	MOTION_NORMAL_ATTACK,
	MOTION_COMBO_ATTACK_1,
	MOTION_COMBO_ATTACK_2,
	MOTION_COMBO_ATTACK_3,
	MOTION_COMBO_ATTACK_4,
	MOTION_COMBO_ATTACK_5,
	MOTION_COMBO_ATTACK_6,
	MOTION_COMBO_ATTACK_7,
	MOTION_COMBO_ATTACK_8,
	MOTION_INTRO_WAIT,
	MOTION_INTRO_SELECTED,
	MOTION_INTRO_NOT_SELECTED,
	MOTION_SPAWN,
	MOTION_FISHING_THROW,
	MOTION_FISHING_WAIT,
	MOTION_FISHING_STOP,
	MOTION_FISHING_REACT,
	MOTION_FISHING_CATCH,
	MOTION_FISHING_FAIL,
	MOTION_STOP,
	MOTION_SPECIAL_1,
	MOTION_SPECIAL_2,           // 34
	MOTION_SPECIAL_3,			// 35
	MOTION_SPECIAL_4,			// 36
	MOTION_SPECIAL_5,			// 37
	PUBLIC_MOTION_END,
	MOTION_MAX_NUM = PUBLIC_MOTION_END,
};

typedef uint32_t MOTION_KEY;

#define MAKE_MOTION_KEY(mode, index)			( ((mode) << 24) | ((index) << 8) | (0) )
#define MAKE_RANDOM_MOTION_KEY(mode, index, type)	( ((mode) << 24) | ((index) << 8) | (type) )

#define GET_MOTION_MODE(key)				((uint8_t) ((key >> 24) & 0xFF))
#define GET_MOTION_INDEX(key)				((uint16_t) ((key >> 8) & 0xFFFF))
#define GET_MOTION_SUB_INDEX(key)			((uint8_t) ((key) & 0xFF))

// Motion timings baked from the client .msa files into the player DB by
// SRC/Tools/motion_bake.py (mob_motion / mob_skill_splash). The server never
// reads .msa or motlist.txt; players send their own move duration in the
// move packet.
class CMotionManager : public singleton<CMotionManager>
{
	public:
		CMotionManager() = default;
		virtual ~CMotionManager() = default;

		// Loads mob_motion / mob_skill_splash from the player DB and fills the
		// CMob skill splash data. Called after the mob proto load.
		bool				Build();

		// Animation duration in seconds; 0 when unknown.
		float				GetMotionDuration(uint32_t dwVnum, uint32_t dwKey);

		// Baked movement speed in px/s; 0 when unknown.
		float				GetMoveSpeed(uint32_t dwVnum, bool walking) const;

		struct SMotionData
		{
			float	walkSpeed { 0.0f };
			float	runSpeed { 0.0f };
			float	attackDuration { 0.0f };
			float	skillDuration[MOB_SKILL_MAX_NUM] { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		};

	protected:
		// uint32_t = MONSTER VNUM or MOUNT VNUM
		std::map<uint32_t, SMotionData>	m_map_motion;
};
