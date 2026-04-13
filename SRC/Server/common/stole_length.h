#include "service.h"
#include "length.h"
#include "item_length.h"

#ifdef ENABLE_STOLE_COSTUME
enum stoleInfoList {
	MAX_VAR_ATTR = 4,
	MAX_ATTR = 6,
	STOLA_COMBINE_GRADE_1 = 80,
	STOLA_COMBINE_GRADE_2 = 60,
	STOLA_COMBINE_GRADE_3 = 40,
};

const int32_t stoleInfoTable[6][17] =
{
	{
		APPLY_ATTBONUS_MONSTER,		// attr1: type
		2,							// attr1: grade 1 var 1: value 1%
		2,							// attr1: grade 1 var 1: value 1%
		4,							// attr1: grade 1 var 2: value 2%
		6,							// attr1: grade 1 var 3: value 3%
		6,							// attr1: grade 2 var 1: value 4%
		6,							// attr1: grade 2 var 1: value 4%
		8,							// attr1: grade 2 var 2: value 5%
		10,							// attr1: grade 2 var 3: value 6%
		10,							// attr1: grade 3 var 1: value 7%
		10,							// attr1: grade 3 var 1: value 7%
		12,							// attr1: grade 3 var 2: value 8%
		14,							// attr1: grade 3 var 3: value 9%
		16,							// attr1: grade 4 var 1: value 10%
		16,							// attr1: grade 4 var 1: value 10%
		18,							// attr1: grade 4 var 2: value 11%
		20,							// attr1: grade 4 var 3: value 12%
	},
	{
#ifdef ENABLE_STRONG_METIN
		APPLY_ATTBONUS_METIN,		// attr2: type
#else
		APPLY_CON,					// attr2: type
#endif
		2,							// attr2: grade 1 var 1: value 1%
		2,							// attr2: grade 1 var 1: value 1%
		4,							// attr2: grade 1 var 2: value 2%
		6,							// attr2: grade 1 var 3: value 3%
		6,							// attr2: grade 2 var 1: value 4%
		6,							// attr2: grade 2 var 1: value 4%
		8,							// attr2: grade 2 var 2: value 5%
		10,							// attr2: grade 2 var 3: value 6%
		10,							// attr2: grade 3 var 1: value 7%
		10,							// attr2: grade 3 var 1: value 7%
		12,							// attr2: grade 3 var 2: value 8%
		14,							// attr2: grade 3 var 3: value 9%
		16,							// attr2: grade 4 var 1: value 10%
		16,							// attr2: grade 4 var 1: value 10%
		18,							// attr2: grade 4 var 2: value 11%
		20,							// attr2: grade 4 var 3: value 12%
	},
	{
#ifdef ENABLE_STRONG_BOSS
		APPLY_ATTBONUS_BOSS,		// attr3: type
#else
		APPLY_INT,					// attr3: type
#endif
		2,							// attr3: grade 1 var 1: value 1%
		2,							// attr3: grade 1 var 1: value 1%
		4,							// attr3: grade 1 var 2: value 2%
		6,							// attr3: grade 1 var 3: value 3%
		6,							// attr3: grade 2 var 1: value 4%
		6,							// attr3: grade 2 var 1: value 4%
		8,							// attr3: grade 2 var 2: value 5%
		10,							// attr3: grade 2 var 3: value 6%
		10,							// attr3: grade 3 var 1: value 7%
		10,							// attr3: grade 3 var 1: value 7%
		12,							// attr3: grade 3 var 2: value 8%
		14,							// attr3: grade 3 var 3: value 9%
		16,							// attr3: grade 4 var 1: value 10%
		16,							// attr3: grade 4 var 1: value 10%
		18,							// attr3: grade 4 var 2: value 11%
		20,							// attr3: grade 4 var 3: value 12%
	},
	{
		APPLY_ATTBONUS_HUMAN,		// attr4: type
		2,							// attr4: grade 1 var 1: value 1%
		2,							// attr4: grade 1 var 1: value 1%
		3,							// attr4: grade 1 var 2: value 2%
		4,							// attr4: grade 1 var 3: value 3%
		4,							// attr4: grade 2 var 1: value 4%
		4,							// attr4: grade 2 var 1: value 4%
		5,							// attr4: grade 2 var 2: value 5%
		7,							// attr4: grade 2 var 3: value 6%
		7,							// attr4: grade 3 var 1: value 7%
		7,							// attr4: grade 3 var 1: value 7%
		8,							// attr4: grade 3 var 2: value 8%
		10,							// attr4: grade 3 var 3: value 9%
		10,							// attr4: grade 4 var 1: value 10%
		10,							// attr4: grade 4 var 1: value 10%
		12,							// attr4: grade 4 var 2: value 11%
		15,							// attr4: grade 4 var 3: value 12%
	},
	{
#ifdef ENABLE_NEW_BONUS_TALISMAN
		APPLY_RESIST_MEZZIUOMINI,	// attr5: type
#else
		APPLY_STR,					// attr5: type
#endif
		2,							// attr5: grade 1 var 1: value 1%
		2,							// attr5: grade 1 var 1: value 1%
		3,							// attr5: grade 1 var 2: value 2%
		4,							// attr5: grade 1 var 3: value 3%
		4,							// attr5: grade 2 var 1: value 4%
		4,							// attr5: grade 2 var 1: value 4%
		5,							// attr5: grade 2 var 2: value 5%
		7,							// attr5: grade 2 var 3: value 6%
		7,							// attr5: grade 3 var 1: value 7%
		7,							// attr5: grade 3 var 1: value 7%
		8,							// attr5: grade 3 var 2: value 8%
		10,							// attr5: grade 3 var 3: value 9%
		10,							// attr5: grade 4 var 1: value 10%
		10,							// attr5: grade 4 var 1: value 10%
		12,							// attr5: grade 4 var 2: value 11%
		15,							// attr5: grade 4 var 3: value 12%
	},
	{
		APPLY_MAX_HP,				// attr6: type
		400,						// attr6: grade 1 var 1: value 1%
		400,						// attr6: grade 1 var 1: value 1%
		600,						// attr6: grade 1 var 2: value 2%
		800,						// attr6: grade 1 var 3: value 3%
		1000,						// attr6: grade 2 var 1: value 4%
		1000,						// attr6: grade 2 var 1: value 4%
		1200,						// attr6: grade 2 var 2: value 5%
		1400,						// attr6: grade 2 var 3: value 6%
		1600,						// attr6: grade 3 var 1: value 7%
		1600,						// attr6: grade 3 var 1: value 7%
		1800,						// attr6: grade 3 var 2: value 8%
		2000,						// attr6: grade 3 var 3: value 9%
		2400,						// attr6: grade 4 var 1: value 10%
		2400,						// attr6: grade 4 var 1: value 10%
		2700,						// attr6: grade 4 var 2: value 11%
		3000,						// attr6: grade 4 var 3: value 12%
	},
};
#endif
