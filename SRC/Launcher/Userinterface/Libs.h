#pragma once

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "imagehlp.lib" )
#pragma comment(lib, "oldnames.lib" )
#pragma comment(lib, "dinput8.lib" )
#pragma comment(lib, "dxguid.lib" )
#pragma comment(lib, "ws2_32.lib" )
#pragma comment(lib, "strmiids.lib" )
#pragma comment(lib, "ddraw.lib" )
#pragma comment(lib, "dmoguids.lib" )
#pragma comment(lib, "version.lib" )
#pragma comment(lib, "shlwapi.lib")


#ifdef ENABLE_ANTICHEAT
#pragma comment (lib, "AntiCheat.lib")
#endif