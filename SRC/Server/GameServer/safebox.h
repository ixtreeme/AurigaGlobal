#ifndef __INC_METIN_II_GAME_SAFEBOX_H__
#define __INC_METIN_II_GAME_SAFEBOX_H__

#include <array>
#include <cstdint>
#include <memory>
#include <common/tables.h>
#include <entt/entity/entity.hpp>

class CGrid;

class CSafebox
{
	public:
		CSafebox(entt::entity owner, int iSize, uint32_t dwGold);
		~CSafebox();
		CSafebox(const CSafebox&) = delete;
		CSafebox& operator=(const CSafebox&) = delete;

		bool		Add(uint32_t dwPos, entt::entity item);
		entt::entity Get(uint32_t dwPos) const;
		entt::entity Remove(uint32_t dwPos);
		void		ChangeSize(int iSize);

		entt::entity GetItem(uint32_t bCell) const;

		bool MoveItem(uint32_t bCell, uint32_t bDestCell, uint32_t count);

		void		Save();

		bool		IsEmpty(uint32_t dwPos, uint8_t bSize);
		bool		IsValidPosition(uint32_t dwPos);

		void		SetWindowMode(uint8_t bWindowMode);

	protected:
		void		__Destroy();

		bool OwnsItem(entt::entity item, uint32_t cell) const;
		bool FitsGrid(uint32_t cell, uint8_t size) const;
		entt::entity m_owner { entt::null };
		std::array<entt::entity, SAFEBOX_MAX_NUM> m_items;
		std::unique_ptr<CGrid> m_pkGrid;
		bool m_destroying { false };
		int		m_iSize;
		int32_t		m_lGold;

		uint8_t		m_bWindowMode;
};

#endif
