#pragma once
#include "StdAfx.h"
#include "PythonCharacterManager.h"
#include "PythonPlayer.h"
#include "PythonGuild.h"

namespace Discord
{
	const auto DiscordClientID = "1033737110609465365";
	using DCDATA = std::pair<std::string, std::string>;

	DCDATA GetRaceData(int lv)
	{
		auto pInstance = CPythonCharacterManager::Instance().GetMainInstancePtr();
		if (!pInstance)
			return {"", ""};

		auto RACENUM = pInstance->GetRace();
		auto PlayerRaceImage = std::to_string(RACENUM);
		std::string PlayerName = "[Lv. ";
		PlayerName += std::to_string(lv == 0 ? pInstance->GetLevel() : lv);
		PlayerName += "] ";
		PlayerName += pInstance->GetNameString();
		return {PlayerRaceImage, PlayerName.c_str()};
	}
}
