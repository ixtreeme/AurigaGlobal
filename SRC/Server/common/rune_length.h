#include "service.h"
#include "length.h"
#include "item_length.h"

#ifdef ENABLE_RUNE_SYSTEM
enum
{
	RUNE_SUBTYPES = WEAR_MAX - WEAR_RUNE1,
	RUNE_ATTR_EACH = 2,
};

static const int32_t aApplyRuneInfo[RUNE_ATTR_EACH * RUNE_SUBTYPES][8] =
{
	// bns1 subtype1
	{
		APPLY_MAX_HP,					// type
		400,							// value at 0% - 5%
		800,							// value at 6% - 10%
		1200,							// value at 11% - 20%
		1600,							// value at 21% - 40%
		1900,							// value at 41% - 60%
		2100,							// value at 61% - 80%
		2500							// value at 81% - 100%
	},
	// bns2 subtype1
	{
		APPLY_CON,						// type
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns1 subtype2
	{
		APPLY_MAX_SP,					// type
		100,							// value at 0% - 5%
		300,							// value at 6% - 10%
		500,							// value at 11% - 20%
		600,							// value at 21% - 40%
		700,							// value at 41% - 60%
		900,							// value at 61% - 80%
		1200							// value at 81% - 100%
	},
	// bns2 subtype2
	{
		APPLY_INT,						// type
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns1 subtype3
	{
#ifdef ENABLE_STRONG_BOSS
		APPLY_ATTBONUS_BOSS,			// type
#else
		APPLY_ATTBONUS_ANIMAL,			// type
#endif
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns2 subtype3
	{
		APPLY_STR,						// type
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns1 subtype4
	{
#ifdef ENABLE_STRONG_METIN
		APPLY_ATTBONUS_METIN,			// type
#else
		APPLY_ATTBONUS_ORC,				// type
#endif
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns2 subtype4
	{
		APPLY_DEX,						// type
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns1 subtype5
	{
		APPLY_ATTBONUS_MONSTER,			// type
		2,								// value at 0% - 5%
		4,								// value at 6% - 10%
		6,								// value at 11% - 20%
		8,								// value at 21% - 40%
		10,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns2 subtype5
	{
		APPLY_PVM_CRITICAL_PCT,				// type
		2,								// value at 0% - 5%
		3,								// value at 6% - 10%
		4,								// value at 11% - 20%
		7,								// value at 21% - 40%
		9,								// value at 41% - 60%
		12,								// value at 61% - 80%
		15								// value at 81% - 100%
	},
	// bns1 subtype6
	{
#ifdef ENABLE_MEDI_PVM
		APPLY_ATTBONUS_MEDI_PVM,		// type
#else
		APPLY_NORMAL_HIT_DAMAGE_BONUS,	// type
#endif
		2,								// value at 0% - 5%
		3,								// value at 6% - 10%
		4,								// value at 11% - 20%
		6,								// value at 21% - 40%
		7,								// value at 41% - 60%
		9,								// value at 61% - 80%
		12								// value at 81% - 100%
	},
	// bns2 subtype6
	{
		APPLY_ATT_GRADE_BONUS,			// type
		30,								// value at 0% - 5%
		40,								// value at 6% - 10%
		50,								// value at 11% - 20%
		70,								// value at 21% - 40%
		90,								// value at 41% - 60%
		110,							// value at 61% - 80%
		150								// value at 81% - 100%
	},
	// bns1 subtype7
	{
		APPLY_ATTBONUS_HUMAN,	// type
		10,								// value at 0% - 5%
		10,								// value at 6% - 10%
		10,								// value at 11% - 20%
		10,								// value at 21% - 40%
		10,								// value at 41% - 60%
		10,								// value at 61% - 80%
		10								// value at 81% - 100%
	},
	// bns2 subtype7
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		APPLY_RESIST_MEZZIUOMINI,		// type
#else
		APPLY_PENETRATE_PCT,			// type
#endif
		10,								// value at 0% - 5%
		10,								// value at 6% - 10%
		10,								// value at 11% - 20%
		10,								// value at 21% - 40%
		10,								// value at 41% - 60%
		10,								// value at 61% - 80%
		10								// value at 81% - 100%
	},
};
#endif
