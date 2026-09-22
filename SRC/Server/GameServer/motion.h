#pragma once

#include <common/d3dtype.h>
#include <common/CommonDefines.h>

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

class CMob;

class CMotion
{
	public:
		CMotion();
		~CMotion();

		bool			LoadFromFile(const char * c_pszFileName);
		bool			LoadMobSkillFromFile(const char * c_pszFileName, CMob * pMob, int iSkillIndex);

		float			GetDuration() const;
		const D3DVECTOR &	GetAccumVector() const;

		bool			IsEmpty();

	protected:
		bool			m_isEmpty;
		float			m_fDuration;
		bool			m_isAccumulation;
		D3DVECTOR		m_vec3Accumulation;
};

typedef uint32_t MOTION_KEY;

#define MAKE_MOTION_KEY(mode, index)			( ((mode) << 24) | ((index) << 8) | (0) )
#define MAKE_RANDOM_MOTION_KEY(mode, index, type)	( ((mode) << 24) | ((index) << 8) | (type) )

#define GET_MOTION_MODE(key)				((uint8_t) ((key >> 24) & 0xFF))
#define GET_MOTION_INDEX(key)				((uint16_t) ((key >> 8) & 0xFFFF))
#define GET_MOTION_SUB_INDEX(key)			((uint8_t) ((key) & 0xFF))

class CMotionSet
{
	public:
		typedef std::map<uint32_t, CMotion *>	TContainer;
		typedef TContainer::iterator		iterator;
		typedef TContainer::const_iterator	const_iterator;

	public:
		CMotionSet();
		~CMotionSet();

		void		Insert(uint32_t dwKey, CMotion * pkMotion);
		bool		Load(const char * szFileName, int mode, int motion);

		const CMotion *	GetMotion(uint32_t dwKey) const;

	protected:
		// uint32_t = MOTION_KEY
		TContainer			m_map_pkMotion;
};

class CMotionManager : public singleton<CMotionManager>
{
	public:
		typedef std::map<uint32_t, CMotionSet *> TContainer;
		typedef TContainer::iterator iterator;

		CMotionManager();
		virtual ~CMotionManager();

		bool			Build();

		const CMotionSet *	GetMotionSet(uint32_t dwVnum);
		const CMotion *		GetMotion(uint32_t dwVnum, uint32_t dwKey);
		float			    GetMotionDuration(uint32_t dwVnum, uint32_t dwKey);

		// POLYMORPH_BUG_FIX
		float			    GetNormalAttackDuration(uint32_t dwVnum);
		// END_OF_POLYMORPH_BUG_FIX

	protected:
		// uint32_t = JOB or MONSTER VNUM
		TContainer		m_map_pkMotionSet;

		// POLYMORPH_BUG_FIX
		std::map<uint32_t, float> m_map_normalAttackDuration;
		// END_OF_POLYMORPH_BUG_FIX
};
