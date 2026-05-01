#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "utils.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#ifdef ENABLE_HWID
#include "hwidmanager.h"

ACMD(do_blockhwid)
{
	char arg[256];
	argument = one_argument(argument, arg, sizeof(arg));

	if (!*arg) {
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Usage: blockhwid <name>");
		return;
	}

	const char* targetname = arg;

	if (strcmp(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), targetname) == 0) {
		return;
	}

	CHwidManager::Instance().SendBlockHwid(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), targetname);
}

ACMD(do_unblockhwid)
{
	char arg[256];
	argument = one_argument(argument, arg, sizeof(arg));

	if (!*arg) {
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, "Usage: unblockhwid <name>");
		return;
	}

	const char* targetname = arg;

	if (strcmp(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), targetname) == 0) {
		return;
	}

	CHwidManager::Instance().SendUnblockHwid(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), targetname);
}
#endif


