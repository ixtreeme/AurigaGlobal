#include "StdAfx.h"
#include "PythonApplication.h"
#include "Update.h"

#if defined(PYTHON_DYNAMIC_MODULE_NAME)
#include "PythonDynamicModuleNames.h"
#endif
#include "../SecureLayer/ProcessScanner.h"
#include "PythonExceptionSender.h"
#include "RegPack.h"
#include "Libs.h"
#include "resource.h"
#include "Version.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#include "../Pack/EterPackManager.h"
#include "../Render/Util.h"


#include "PythonConfig.h"

#include <iostream>


#include "../SecureLayer/ProcessCRC.h"
#ifdef ENABLE_HWID
#include "../SecureLayer/CHwidManager.h"
#endif
#if defined(ENABLE_ANTICHEAT)

#include <windows.h>
#include <ThemidaSDK.h>
#include "AntiCheat/AntiCheatSDK.h"

#endif
#include "../SecureLayer/Anti_VM.h"
#include "../SecureLayer/obfuscate.h"



#pragma comment(linker, "/NODEFAULTLIB:libci.lib")





#include <iphlpapi.h>






#include <stdlib.h>

bool __IS_TEST_SERVER_MODE__ = false;

#ifdef __USE_CYTHON__
// don't include these two files .h .cpp if you're implementing cython via .pyd
#include "PythonrootlibManager.h"
// it would be better including such file in the project, but this is easier at this moment:
//#include "PythonrootlibManager.cpp"
#endif

// #define __USE_EXTRA_CYTHON__
#ifdef __USE_EXTRA_CYTHON__
// don't include these two files .h .cpp if you're implementing cython via .pyd
#include "PythonuiscriptlibManager.h"
// it would be better including such file in the project, but this is easier at this moment:
//#include "PythonuiscriptlibManager.cpp"
#endif


extern bool SetDefaultCodePage(uint32_t codePage);


char gs_szErrorString[512] = "";

void ApplicationSetErrorString(const char* szErrorString)
{
	strcpy(gs_szErrorString, szErrorString);
}

struct ApplicationStringTable
{
	HINSTANCE m_hInstance;
	std::map<uint32_t, std::string> m_kMap_dwID_stLocale;

	ApplicationStringTable() : m_hInstance(nullptr) {}
} gs_kAppStrTable;

void ApplicationStringTable_Initialize(HINSTANCE hInstance)
{
	gs_kAppStrTable.m_hInstance = hInstance;
}

const std::string& ApplicationStringTable_GetString(uint32_t dwID, LPCSTR szKey)
{
	char szBuffer[512] = { 0 };
	char szIniFileName[256] = { 0 };
	char szLocale[256] = { 0 };

	strcpy(szLocale, LocaleService_GetLocalePath());
	if (strnicmp(szLocale, "locale/", strlen("locale/")) == 0)
		strcpy(szLocale, LocaleService_GetLocalePath() + strlen("locale/"));
	::GetPrivateProfileString(szLocale, szKey, nullptr, szBuffer, sizeof(szBuffer) - 1, szIniFileName);
	if (szBuffer[0] == '\0')
		LoadString(gs_kAppStrTable.m_hInstance, dwID, szBuffer, sizeof(szBuffer) - 1);
	if (szBuffer[0] == '\0')
		::GetPrivateProfileString("en", szKey, nullptr, szBuffer, sizeof(szBuffer) - 1, szIniFileName);
	if (szBuffer[0] == '\0')
		strcpy(szBuffer, szKey);

	std::string& rstLocale = gs_kAppStrTable.m_kMap_dwID_stLocale[dwID];
	rstLocale = szBuffer;

	return rstLocale;
}

const std::string& ApplicationStringTable_GetString(uint32_t dwID)
{
	char szBuffer[512];

	LoadString(gs_kAppStrTable.m_hInstance, dwID, szBuffer, sizeof(szBuffer) - 1);
	std::string& rstLocale = gs_kAppStrTable.m_kMap_dwID_stLocale[dwID];
	rstLocale = szBuffer;

	return rstLocale;
}

const char* ApplicationStringTable_GetStringz(uint32_t dwID, LPCSTR szKey)
{
	return ApplicationStringTable_GetString(dwID, szKey).c_str();
}

const char* ApplicationStringTable_GetStringz(uint32_t dwID)
{
	return ApplicationStringTable_GetString(dwID).c_str();
}

////////////////////////////////////////////

int Setup(LPSTR lpCmdLine); // Internal function forward



bool RunMainScript(CPythonLauncherIxtreeme& pyLauncher, const char* lpCmdLine)
{
#if defined(PYTHON_DYNAMIC_MODULE_NAME)
	initPythonApi();
#endif
	initpack();
	initdbg();
	initime();
	initgrp();
	initgrpImage();
	initgrpText();
	initwndMgr();
	/////////////////////////////////////////////
	initudp();
	initapp();
	initsystemSetting();
	initchr();
	initchrmgr();
	initPlayer();
	initItem();
	initNonPlayer();
	initTrade();
	initChat();
	initTextTail();
	initnet();
	initMiniMap();
	initProfiler();
	initEvent();
	initeffect();
	initfly();
	initsnd();
	initeventmgr();
	initshop();
	initskill();
#ifdef NEW_PET_SYSTEM
	initskillpet();
#endif
	initquest();
	initBackground();
	initMessenger();
#ifdef ENABLE_ACCE_SYSTEM
	initAcce();
#endif

#ifdef ENABLE_CONFIG_MODULE
	initcfg();
#endif

	initsafebox();
	initguild();
	initServerStateChecker();


#ifdef ENABLE_SWITCHBOT
	initSwitchbot();
#endif
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
	intcuberenewal();
#endif

	initWiki();

#ifdef __USE_CYTHON__
	// don't add this line if you're implementing cython via .pyd:
	initrootlibManager();
#endif
#ifdef __USE_EXTRA_CYTHON__
	// don't add this line if you're implementing cython via .pyd:
	inituiscriptlibManager();
#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
	initofflineshop();
#endif


	PyObject* builtins = PyImport_ImportModule("__builtin__");
#ifdef NDEBUG // @warme601 _DISTRIBUTE -> NDEBUG
	PyModule_AddIntConstant(builtins, "__DEBUG__", 1);
#else
	PyModule_AddIntConstant(builtins, "__DEBUG__", 0);
#endif
#ifdef __USE_CYTHON__
	PyModule_AddIntConstant(builtins, "__USE_CYTHON__", 1);
#else
	PyModule_AddIntConstant(builtins, "__USE_CYTHON__", 0);
#endif
#ifdef __USE_EXTRA_CYTHON__
	PyModule_AddIntConstant(builtins, "__USE_EXTRA_CYTHON__", 1);
#else
	PyModule_AddIntConstant(builtins, "__USE_EXTRA_CYTHON__", 0);
#endif


	// RegisterCommandLine
	{
		std::string stRegisterCmdLine;

		const char* loginMark = "-cs";
		const char* loginMark_NonEncode = "-ncs";
		const char* seperator = " ";

		std::string stCmdLine;
		const int CmdSize = 3;
		std::vector<std::string> stVec;
		SplitLine(lpCmdLine, seperator, &stVec);
		if (CmdSize == stVec.size() && stVec[0] == loginMark)
		{
			char buf[MAX_PATH];	//TODO ľĆ·ˇ ÇÔĽö std::string ÇüĹÂ·Î ĽöÁ¤
			base64_decode(stVec[2].c_str(), buf);
			stVec[2] = buf;
			string_join(seperator, stVec, &stCmdLine);
		}
		else if (CmdSize <= stVec.size() && stVec[0] == loginMark_NonEncode)
		{
			stVec[0] = loginMark;
			string_join(" ", stVec, &stCmdLine);
		}
		else
			stCmdLine = lpCmdLine;

		PyModule_AddStringConstant(builtins, "__COMMAND_LINE__", stCmdLine.c_str());
	}
	{
		std::vector<std::string> stVec;
		SplitLine(lpCmdLine, " ", &stVec);

		if (stVec.size() != 0 && "--pause-before-create-window" == stVec[0])
		{
			system("pause");
		}

#ifdef __USE_CYTHON__
		if (!pyLauncher.RunLine("import rootlib\nrootlib.moduleImport('system')"))
#else
		if (!pyLauncher.RunFile("system.py"))
#endif
		{
			TraceError("RunMain Error");
			return false;
		}
	}

	return true;
}



bool Main(HINSTANCE hInstance, LPSTR lpCmdLine)
{


	uint32_t dwRandSeed = time(nullptr) + reinterpret_cast<uint32_t>(GetCurrentProcess());
	srandom(dwRandSeed);
	srand(random());


	SetLogLevel(1);

	if (!Setup(lpCmdLine))
		return false;

#ifdef _DEBUG
	OpenConsoleWindow();
	OpenLogFile(true); // true == uses syserr.txt and log.txt
#else
	OpenLogFile(false); // false == uses syserr.txt only
#endif



	static CLZO							lzo;
	static CEterPackManager				EterPackManager;

#ifdef ENABLE_HWID
	static CHwidManager HwidManager;
#endif

	if (!PackInitialize("pack"))
	{
		LogBox("Pack Initialization failed. Check log.txt file..");
		return false;
	}

#ifdef ENABLE_CONFIG_MODULE
	static CPythonConfig m_pyConfig;
	m_pyConfig.Initialize("config.cfg");
#endif


	if (LocaleService_LoadGlobal(hInstance))
		SetDefaultCodePage(LocaleService_GetCodePage());


	CPythonApplication* app = new CPythonApplication;

	app->Initialize(hInstance);

	bool ret = false;
	{

		CPythonLauncherIxtreeme pyLauncher;
		CPythonExceptionSender pyExceptionSender;
		SetExceptionSender(&pyExceptionSender);

		if (pyLauncher.Create()) {
			ret = RunMainScript(pyLauncher, lpCmdLine);
		}

		//ProcessScanner_ReleaseQuitEvent();

		//°ÔŔÓ Áľ·á˝Ă.
		app->Clear();

		timeEndPeriod(1);
		pyLauncher.Clear();
	}


	app->Destroy();
	delete app;

	return ret;

}

HANDLE CreateMetin2GameMutex()
{
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = nullptr;
	sa.bInheritHandle = FALSE;

	return CreateMutex(&sa, FALSE, "Metin2GameMutex");
}

void DestroyMetin2GameMutex(HANDLE hMutex)
{
	if (hMutex)
	{
		ReleaseMutex(hMutex);
		hMutex = nullptr;
	}
}

//void __ErrorPythonLibraryIsNotExist()
//{
//	LogBoxf("FATAL ERROR!! Python Library file not exist!");
//}

bool __IsTimeStampOption(LPSTR lpCmdLine)
{
	const char* TIMESTAMP = "/timestamp";
	return (strncmp(lpCmdLine, TIMESTAMP, strlen(TIMESTAMP)) == 0);
}

void __PrintTimeStamp()
{
#ifdef	_DEBUG
	if (__IS_TEST_SERVER_MODE__)
		LogBoxf("METIN2 BINARY TEST DEBUG VERSION %s  ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);
	else
		LogBoxf("METIN2 BINARY DEBUG VERSION %s ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);

#else
	if (__IS_TEST_SERVER_MODE__)
		LogBoxf("METIN2 BINARY TEST VERSION %s  ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);
	else
		LogBoxf("METIN2 BINARY DISTRIBUTE VERSION %s ( MS C++ %d Compiled )", __TIMESTAMP__, _MSC_VER);
#endif
}

bool __IsLocaleOption(LPSTR lpCmdLine)
{
	return (strcmp(lpCmdLine, "--locale") == 0);
}

bool __IsLocaleVersion(LPSTR lpCmdLine)
{
	return (strcmp(lpCmdLine, "--perforce-revision") == 0);
}


#include <windows.h>
#include "../SecureLayer/LauncherGuard.h"


#ifdef BYTE
#undef BYTE
#endif


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	/*HMODULE hAC = LoadLibraryW(AY_OBFUSCATE(L"IXAC.dll"));
	if (!hAC)
	{
		MessageBoxW(nullptr, AY_OBFUSCATE(L"Failed to load IXAC.dll"),AY_OBFUSCATE(L"Error"), MB_ICONERROR);
		ExitProcess(1);
	}*/
	//CheckAndUpdatePatcher();
	//LG_EnsureLaunchedFromPatcher();

	/*if (!InitIxacIntegrityWatchdog())
	{
		MessageBoxW(nullptr,AY_OBFUSCATE(L"Integrity check FAILED."),AY_OBFUSCATE(L"IXAC Anti-Cheat"),MB_ICONERROR);
		ExitProcess(1);
	}*/
//#ifndef _DEBUG
//	abort_if_vm_detected(true);
//#endif
//
//#if defined(NEEDED_COMMAND_ARGUMENT)
//	if (strstr(lpCmdLine, AY_OBFUSCATE("KC8nRPUPHbL9rMeamQqr")) == nullptr) {
//		MessageBox(nullptr, AY_OBFUSCATE("Use the patcher!"), AY_OBFUSCATE("Auriga-Global"), MB_ICONSTOP);
//
//		return -1;
//	}
//#endif


	//Sleep(3500);






#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc( 110247 );
#endif

	ApplicationStringTable_Initialize(hInstance);
	LocaleService_LoadConfig("locale.cfg");
	SetDefaultCodePage(LocaleService_GetCodePage());

	Main(hInstance, lpCmdLine);


	CoUninitialize();


	return 0;
}


int Setup(LPSTR lpCmdLine)
{

	TIMECAPS tc;
	UINT wTimerRes;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
		return 0;

	wTimerRes = MINMAX(tc.wPeriodMin, 1, tc.wPeriodMax);
	timeBeginPeriod(wTimerRes);

	return 1;
}
