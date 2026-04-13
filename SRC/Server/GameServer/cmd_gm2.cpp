#include "stdafx.h"
#include "utils.h"
#include "char.h"
#ifdef ENABLE_HWID
#include "hwidmanager.h"

ACMD(do_blockhwid)
{
	char arg[256];
	argument = one_argument(argument, arg, sizeof(arg));

	if (!*arg) {
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: blockhwid <name>");
		return;
	}

	const char* targetname = arg;

	if (strcmp(ch->GetName(), targetname) == 0) {
		return;
	}

	CHwidManager::Instance().SendBlockHwid(ch->GetName(), targetname);
}

ACMD(do_unblockhwid)
{
	char arg[256];
	argument = one_argument(argument, arg, sizeof(arg));

	if (!*arg) {
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: unblockhwid <name>");
		return;
	}

	const char* targetname = arg;

	if (strcmp(ch->GetName(), targetname) == 0) {
		return;
	}

	CHwidManager::Instance().SendUnblockHwid(ch->GetName(), targetname);
}
#endif
