#ifndef __INC_METIN_II_GAME_DRAGON_SOUL_H__
#define __INC_METIN_II_GAME_DRAGON_SOUL_H__

#include <common/length.h>
#include <entt/entt.hpp>
#include <memory>
#include "ecs/components/item_components.hpp"

class DragonSoulTable;

class DSManager : public singleton<DSManager>
{
public:
	DSManager();
	~DSManager();
	bool	ReadDragonSoulTableFile(const char * c_pszFileName);

	void	GetDragonSoulInfo(uint32_t dwVnum, OUT uint8_t& bType, OUT uint8_t& bGrade, OUT uint8_t& bStep, OUT uint8_t& bRefine) const;
	uint16_t	GetBasePosition(entt::entity item) const;
	bool	IsValidCellForThisItem(entt::entity item, const TItemPos& Cell) const;
	int		GetDuration(entt::entity item) const;

	bool	ExtractDragonHeartEcs(entt::entity owner, entt::entity item, entt::entity extractor = entt::null);

	bool	PullOutEcs(entt::entity owner, TItemPos DestCell, IN OUT entt::entity& item, entt::entity extractor = entt::null);

	bool	DoRefineGrade(entt::entity ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineGradeEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineStep(entt::entity ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineStepEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineStrength(entt::entity ch, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
	bool	DoRefineStrengthEcs(entt::entity owner, TItemPos (&aItemPoses)[DRAGON_SOUL_REFINE_GRID_SIZE]);
#ifdef ENABLE_DS_REFINE_ALL
	void DoRefineAll(entt::entity ch, uint8_t subheader, uint8_t type, uint8_t grade);
	void DoRefineAllEcs(entt::entity owner, uint8_t subheader, uint8_t type, uint8_t grade);
#endif

	bool	DragonSoulItemInitialize(entt::entity item);

	bool	IsTimeLeftDragonSoul(entt::entity item) const;
	int		LeftTime(entt::entity item) const;
	bool	ActivateDragonSoul(entt::entity item);
	bool	DeactivateDragonSoul(entt::entity item, bool bSkipRefreshOwnerActiveState = false);
	bool	IsActiveDragonSoul(entt::entity item) const;
#ifdef ENABLE_DS_ENCHANT
	enum class EnchantResult { Success, InvalidTarget, Active, InvalidGrade, InvalidMaterial, Failed };
	EnchantResult EnchantWithItemCost(entt::entity owner, entt::entity item, entt::entity material);
	bool	PutAttributes(entt::entity item);
#endif
	bool	RefreshItemAttributes(entt::entity item);
private:
	void	SendRefineResultPacket(entt::entity ch, uint8_t bSubHeader, const TItemPos& pos);

	void	RefreshDragonSoulState(entt::entity owner);

	uint32_t	MakeDragonSoulVnum(uint8_t bType, uint8_t grade, uint8_t step, uint8_t refine);

#ifndef ENABLE_DS_ENCHANT
	bool	PutAttributes(entt::entity item);
#endif
	bool PrepareAttributes(entt::entity item, ecs::ItemAttributes& result, bool refresh);
	std::unique_ptr<DragonSoulTable> m_pTable;
};

#endif
