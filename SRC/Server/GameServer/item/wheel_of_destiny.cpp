#include "stdafx.h"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/AIHelpers.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "wheel_of_destiny.h"
#include "char_interface.hpp"
#include "../ecs/CharacterAccessors.hpp"

#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)

static constexpr auto WheelPrice = 1; // 1M
static constexpr auto WheelItemMax = 16;

//vnum, count(max 255), chance(max 255)
static constexpr std::tuple<uint32_t, std::uint8_t, std::uint8_t> m_Data[WheelItemMax] =
{
	{ 39066, 10, 0 },	 //Gaya
	{ 39067, 10, 25 },
	{ 70027, 1, 0 },
	{ 89007, 1, 0 },
	{ 99999, 200, 0 }, //Run-pont
	{ 70027, 3, 0 },
	{ 70610, 3, 0 },
	{ 72100, 10, 0 },
	{ 611586, 1, 25 },
	{ 30617, 1, 0 },
	{ 30618, 1, 0 },
	{ 80008, 5, 0 },
	{ 89006, 1, 0 },
	{ 71107, 5, 0 },
	{ 71129, 5, 0 },
	{ 71123, 5, 0 },

};

CWheelDestiny::CWheelDestiny(entt::entity owner)
	: m_owner(owner), gift_vnum(0), gift_count(1), turn_count(0)
{
	const entt::entity chEntity = m_owner;
	for (auto i = 0; i < WheelItemMax; i++)
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "BINARY_WHEEL_ICON %lu %d %d", std::get<0>(m_Data[i]), std::get<1>(m_Data[i]), i);
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "BINARY_WHEEL_OPEN %d %d", WheelPrice, ecs::PlayerRuntime::GetWheelFreeCount(m_owner));
}

CWheelDestiny::~CWheelDestiny() {
	if (GetGiftVnum())
		LOG_INFO("<CWheelDestiny> player({}) didn't get his gift(vnum: {}({}.x))!!", ecs::PlayerRuntime::GetName(m_owner), GetGiftVnum(), GetGiftCount());
}

template <typename T> std::string NumberToMoneyString(T val)
{
	constexpr int comma = 3;
	auto str = std::to_string(val);
	auto pos = static_cast<int>(str.length()) - comma;

	while (pos > 0) {
		str.insert(pos, ".");
		pos -= comma;
	}

	return str;
}

void CWheelDestiny::TurnWheel()
{
	const entt::entity chEntity = m_owner;

	//m_bTurning = true;





	if (GetGiftVnum()) {
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "Please wait!");
		return;
	}

	const auto WheelFreeCount = ecs::PlayerRuntime::GetWheelFreeCount(m_owner);

	if (WheelFreeCount < 1 && ecs::PointSystem::GetGold(chEntity) < WheelPrice) {
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "You need %s yang for <Turning Wheel>", NumberToMoneyString(WheelPrice).c_str());
		return;
	}

	auto Rand = PickAGift();
	if (Rand == -1) {
		LOG_ERROR("CWheelDestiny::TurnWheel() Error Pick Gift ({})", ecs::PlayerRuntime::GetName(chEntity));
		return;
	}

	if (WheelFreeCount > 0) {
		ecs::PlayerRuntime::SetWheelFreeCount(m_owner, -1);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "FREE");
	}
	else
		ecs::PointSystem::Change(chEntity, POINT_GOLD, -WheelPrice);

	//vnum, count, spin count, pos
	ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "BINARY_WHEEL_TURN %lu %d %d %d", GetGiftVnum(), GetGiftCount(), number(1, 8), Rand);

	turn_count++;
}

std::uint8_t CWheelDestiny::GetChance() const
{
#undef max
	const auto TurnCount = GetTurnCount();
	if (TurnCount >= 10 && TurnCount < 25)
		return 25;
	if (TurnCount >= 25 && TurnCount < 40)
		return 50;
	if (TurnCount >= 40)
		return std::numeric_limits<std::uint8_t>::max(); // 255
	return 0;
}

int CWheelDestiny::PickAGift()
{
	const auto Chance = GetChance();

	while (true) {
		const auto rand_pos = number(0, WheelItemMax - 1);
		const auto& [item, count, m_chance] = m_Data[rand_pos];

		if (Chance >= m_chance) {
			SetGift(item, count);
			return rand_pos;
		}
	}
	return -1; // error
}

void CWheelDestiny::SetGift(const uint32_t vnum, const std::uint8_t count)
{
	gift_vnum = vnum;
	gift_count = count;
}

void CWheelDestiny::GiveMyFuckingGift()
{
	const auto GiftVnum = GetGiftVnum();

	if (GiftVnum) {
		ItemSystem::AutoGiveItemEcs(m_owner, GiftVnum, GetGiftCount());
		SetGift(0, 1); // reset
	}
	else
		LOG_ERROR("Dude, where is the gift_vnum? <player: {}>", ecs::PlayerRuntime::GetName(m_owner));
}

uint32_t CWheelDestiny::GetGiftVnum() const
{
	return gift_vnum;
}

std::uint8_t CWheelDestiny::GetGiftCount() const
{
	return gift_count;
}

std::uint16_t CWheelDestiny::GetTurnCount() const
{
	return turn_count;
}
#endif


