#pragma once

#pragma warning(disable:4702)
#pragma warning(disable:4100)
#pragma warning(disable:4201)
#pragma warning(disable:4511)
#pragma warning(disable:4663)
#pragma warning(disable:4018)
#pragma warning(disable:4245)
#pragma warning(disable:4273)

//#if _MSC_VER >= 1400
////if don't use below, time_t is 64bit
//#define _USE_32BIT_TIME_T
//#endif

#include <iterator>
#include "../Render/StdAfx.h"
#include "../Python/StdAfx.h"
#include "../Game/StdAfx.h"
#include "../Script/StdAfx.h"
#include "../AudioLib/StdAfx.h"
#include "../Effect/StdAfx.h"
#include "../Terrain/StdAfx.h"
#include "../SpeedTree/StdAfx.h"

#ifndef __D3DRM_H__
#define __D3DRM_H__
#endif

#ifdef snprintf
#undef snprintf
#endif

#undef BYTE


#include <dshow.h>
//#include <qedit.h>
#include "Locale.h"

#include "GameType.h"

extern uint32_t __DEFAULT_CODE_PAGE__;

#define APP_NAME "Wom2"

enum
{
	POINT_MAX_NUM = 255,
	CHARACTER_NAME_MAX_LEN = 24,
#if defined(LOCALE_SERVICE_JAPAN)
	PLAYER_NAME_MAX_LEN = 16,
#else
	PLAYER_NAME_MAX_LEN = 12,
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
	OFFLINE_SHOP_NAME_MAX_LEN = 40 + CHARACTER_NAME_MAX_LEN +1,
	OFFLINE_SHOP_ITEM_MAX_LEN = 48,
#endif
};

void initudp();
void initapp();
void initime();
void initsystemSetting();
void initchr();
void initchrmgr();
void initChat();
void initTextTail();
void initime();
void initItem();
void initNonPlayer();
void initnet();
void initPlayer();
void initSectionDisplayer();
void initServerStateChecker();
void initRenderTarget();
void initTrade();
void initMiniMap();
void initProfiler();
void initEvent();
void initeffect();
void initsnd();
void initeventmgr();
void initBackground();
void initwndMgr();
void initshop();
void initpack();
void initskill();
#ifdef NEW_PET_SYSTEM
	void initskillpet();
#endif
void initfly();
void initquest();

#ifdef ENABLE_CONFIG_MODULE
void initcfg();
#endif

void initsafebox();
void initguild();
void initMessenger();
#ifdef __ENABLE_NEW_OFFLINESHOP__
void initofflineshop();
#endif
#ifdef ENABLE_ACCE_SYSTEM
void initAcce();
#endif
#ifdef ENABLE_SWITCHBOT
void initSwitchbot();
#endif
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
void intcuberenewal();
#endif

void initWiki();

extern const std::string& ApplicationStringTable_GetString(uint32_t dwID);
extern const std::string& ApplicationStringTable_GetString(uint32_t dwID, std::string& key);

extern const char* ApplicationStringTable_GetStringz(uint32_t dwID);
extern const char* ApplicationStringTable_GetStringz(uint32_t dwID, LPCSTR szKey);

extern void ApplicationSetErrorString(const char* szErrorString);
#ifdef ENABLE_VOTE4BUFF
extern std::string httpGet(const std::string& url);
#endif

