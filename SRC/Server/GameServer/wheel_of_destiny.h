#pragma once

/*
* Created by blackdragonx61
* Date:05.09.2020
*/

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
class CWheelDestiny
{
public:
	CWheelDestiny(LPCHARACTER ch);
	~CWheelDestiny();
	void TurnWheel();
	void GiveMyFuckingGift();
	uint32_t GetGiftVnum() const;
	bool IsTurning() const { return m_bTurning; }
	void SetTurning(bool b) { m_bTurning = b; }
private:
	void SetGift(const uint32_t vnum, const std::uint8_t count);
	int PickAGift();
	bool m_bTurning = false;
	std::uint8_t GetGiftCount() const;
	std::uint16_t GetTurnCount() const;
	std::uint8_t GetChance() const;

	LPCHARACTER ch;
	uint32_t gift_vnum;
	std::uint8_t gift_count;
	std::uint16_t turn_count;
};
#endif
