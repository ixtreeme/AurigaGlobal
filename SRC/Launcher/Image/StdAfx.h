#pragma once

#pragma warning(disable:4786)
#pragma warning(default:4018)


#include <windows.h>
#include <assert.h>
#include <print>

#pragma warning(push, 3)
#include <string>
#include <vector>
#pragma warning(pop)

void _TraceForImage(const char* c_szFormat, auto&&... args)
{

	std::print(c_szFormat, std::forward<decltype(args)>(args)...);
#ifdef _DEBUG
	std::string output = std::format(c_szFormat, std::forward<decltype(args)>(args)...);
	OutputDebugString(output.c_str());
#endif

}


