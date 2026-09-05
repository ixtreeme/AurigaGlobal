#ifndef __INC_REFINE_H
#define __INC_REFINE_H

#include "constants.h"
#include <entt/entt.hpp>

#define REFINE_INCREASE "REFINE.INCREASE_PERCENTAGE"


enum
{
	BLACKSMITH_MOB = 20016, // 확률 개량
	ALCHEMIST_MOB = 20001, // 100% 개량 성공

	BLACKSMITH_WEAPON_MOB = 20044,
	BLACKSMITH_ARMOR_MOB = 20045,
	BLACKSMITH_ACCESSORY_MOB = 20046,

	DEVILTOWER_BLACKSMITH_WEAPON_MOB = 20074,
	DEVILTOWER_BLACKSMITH_ARMOR_MOB = 20075,
	DEVILTOWER_BLACKSMITH_ACCESSORY_MOB = 20076,

	BLACKSMITH2_MOB	= 20091,
};



#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	enum
	{
		REFINE_PERCENTAGE_LOW = 5,
		REFINE_PERCENTAGE_MEDIUM = 10,
		REFINE_PERCENTAGE_EXTRA = 15,
		
		REFINE_VNUM_POTION_LOW = 56001,
		REFINE_VNUM_POTION_MEDIUM = 56002,
		REFINE_VNUM_POTION_EXTRA = 56003,	
		
	};
#endif


class CRefineManager : public singleton<CRefineManager>
{
	typedef std::map<uint32_t, TRefineTable> TRefineRecipeMap;
	public:
	CRefineManager();
	virtual ~CRefineManager();

	bool	Initialize(TRefineTable * table, int size);
	const TRefineTable* GetRefineRecipe(uint32_t id);


#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	bool	GetPercentage(LPCHARACTER ch, uint8_t lLow, uint8_t lMedium, uint8_t lExtra, uint8_t total, entt::entity item);
	void	Increase(LPCHARACTER ch, uint8_t lLow, uint8_t lMedium, uint8_t lExtra);
	void	Reset(LPCHARACTER ch);
	void	Reset_percent(LPCHARACTER ch);
	int		Result(entt::entity ch);
#endif



	private:
	TRefineRecipeMap    m_map_RefineRecipe;

};
#endif
