// stdafx.cpp : source file that includes just the standard includes
//	UserInterface.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

#ifdef ENABLE_VOTE4BUFF
#include <windows.h>
#include <wininet.h>

std::string httpGet(const std::string& url) {
	HINTERNET hInternet = InternetOpen(TEXT("MyApp"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL) {
		return "empty";
	}
	HINTERNET hUrl = InternetOpenUrl(hInternet, TEXT(url.c_str()), NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if (hUrl == NULL) {
		InternetCloseHandle(hInternet);
		return "empty";
	}
	std::string contents;
	char buffer[4096];
	uint32_t bytesRead = 0;
	while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		contents.append(buffer, bytesRead);
	}
	InternetCloseHandle(hUrl);
	InternetCloseHandle(hInternet);
	return contents;
}
#endif