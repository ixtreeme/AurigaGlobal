#include "StdAfx.h"

#include <io.h>
#include <print>

#include "EterPackManager.h"

#include "../Base/Debug.h"
#include "../Base/CRC32.h"

#define PATH_ABSOLUTE_YMIRWORK1	"d:/ymir work/"
#define PATH_ABSOLUTE_YMIRWORK2	"d:\\ymir work\\"


namespace FoxFS
{
	enum
	{
		ERROR_OK = 0,
		ERROR_BASE_CODE = 0,
		ERROR_FILE_WAS_NOT_FOUND = ERROR_BASE_CODE + 1,
		ERROR_CORRUPTED_FILE = ERROR_BASE_CODE + 2,
		ERROR_MISSING_KEY = ERROR_BASE_CODE + 3,
		ERROR_MISSING_IV = ERROR_BASE_CODE + 4,
		ERROR_DECRYPTION_HAS_FAILED = ERROR_BASE_CODE + 5,
		ERROR_DECOMPRESSION_FAILED = ERROR_BASE_CODE + 6,
		ERROR_ARCHIVE_NOT_FOUND = ERROR_BASE_CODE + 7,
		ERROR_ARCHIVE_NOT_READABLE = ERROR_BASE_CODE + 8,
		ERROR_ARCHIVE_INVALID = ERROR_BASE_CODE + 9,
		ERROR_ARCHIVE_ACCESS_DENIED = ERROR_BASE_CODE + 10,
		ERROR_KEYSERVER_SOCKET = ERROR_BASE_CODE + 11,
		ERROR_KEYSERVER_CONNECTION = ERROR_BASE_CODE + 12,
		ERROR_KEYSERVER_RESPONSE = ERROR_BASE_CODE + 13,
		ERROR_KEYSERVER_TIMEOUT = ERROR_BASE_CODE + 14,
		ERROR_UNKNOWN = ERROR_BASE_CODE + 15
	};
}

const char* white_file_list[] = { "logininfo.xml", "mark\10_0.tga" "mark\250_0.tga" };

bool isWhiteFile(const char* c_szFileName)
{
	for (int i = 0; i < ARRAYSIZE(white_file_list); i++)
	{
		if (stricmp(c_szFileName, white_file_list[i]) == 0)
		{
			return true;
		}
	}
	return false;
}

const char* white_file_list_ext[] = { "xml", "tga", "png", "bmp", "mp3", "jpg" };

bool isWhiteFileExt(const char* c_szFileName)
{
	for (int i = 0; i < ARRAYSIZE(white_file_list_ext); i++)
	{
		auto ixtreeme = std::string(c_szFileName);
		std::string ext = CFileNameHelper::GetExtension(ixtreeme);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		auto whiteExt = std::string(white_file_list_ext[i]);

		if (ext == whiteExt)
			return true;
	}
	return false;
}

void CEterPackManager::SetCacheMode()
{
	m_isCacheMode=false;
}

void CEterPackManager::SetRelativePathMode()
{
	m_bTryRelativePath = true;
}

bool CEterPackManager::Get(CMappedFile & rMappedFile, const char * c_szFileName, const void** pData)
{
	if (m_iSearchMode == SEARCH_FILE)
	{
		if (GetFromFile(rMappedFile, c_szFileName, pData))
		{
			return true;
		}

		return GetFromPack(rMappedFile, c_szFileName, pData);
	}
	else
	{
		if (isExistInPack(c_szFileName))
		{
			return GetFromPack(rMappedFile, c_szFileName, pData);
		}
		else if (isExist(c_szFileName))
		{
			return GetFromFile(rMappedFile, c_szFileName, pData);
		}
	}
	return false;
}

struct FinderLock
{
	FinderLock(CRITICAL_SECTION& cs) : p_cs(&cs)
	{
		EnterCriticalSection(p_cs);
	}

	~FinderLock()
	{
		LeaveCriticalSection(p_cs);
	}

	CRITICAL_SECTION* p_cs;
};

bool CEterPackManager::GetFromPack(CMappedFile & rMappedFile, const char * c_szFileName, const void** pData)
{
	assert(c_szFileName);

	FinderLock lock(m_csFinder);

	if (m_pFoxFS)
	{
		int errorCodeSize = 0;
		if ((errorCodeSize = FoxFS_ExistsA(m_pFoxFS, c_szFileName)) == FoxFS::ERROR_OK)
		{
			unsigned int dwSize = FoxFS_SizeA(m_pFoxFS, c_szFileName), dwReadSize = 0;
			uint8_t* pbData = new uint8_t[dwSize + 1];
			int errorCode = 0;
			if ((errorCode = FoxFS_GetA(m_pFoxFS, c_szFileName, pbData, dwSize, &dwReadSize)) == FoxFS::ERROR_OK)
			{
				pbData[dwReadSize] = 0;
				*pData = pbData;
#ifdef MEMORY_LEAK_FIX_RAZRO93
				rMappedFile.Link(dwReadSize, pbData, true);
#else
				rMappedFile.Link(dwReadSize, pbData);
#endif
				return true;
			}
			else {
				TraceError("Could not get file %s Error Code %d", c_szFileName, errorCode);
			}
			delete[] pbData;
		}
		else {
			TraceError("File not existing %s Error Code %d", c_szFileName, errorCodeSize);

		}
	}
	else
	{
		TraceError("FoxFS: Not initialized!");
	}

	return false;
}

bool CEterPackManager::GetFromFile(CMappedFile & rMappedFile, const char * c_szFileName, const void** pData)
{
	return rMappedFile.Create(c_szFileName, pData, 0, 0) ? true : false;
}

bool CEterPackManager::isExistInPack(const char * c_szFileName)
{
	assert(c_szFileName);

	if (m_pFoxFS)
	{
		int errorCodeSize = 0;
		if ((errorCodeSize = FoxFS_ExistsA(m_pFoxFS, c_szFileName)) == FoxFS::ERROR_OK) {
			return true;
		}
	}
	else
	{
		TraceError("FoxFS: Not initialized!");
	}

	return false;
}

bool CEterPackManager::isExist(const char * c_szFileName)
{
	if (m_iSearchMode == SEARCH_PACK)
	{
		if (isWhiteFile(c_szFileName) || isWhiteFileExt(c_szFileName))
		{
			return isExistInPack(c_szFileName) || (_access(c_szFileName, 0) == 0);
		}
		return isExistInPack(c_szFileName);
	}

	if (_access(c_szFileName, 0) == 0)
		return true;

	return isExistInPack(c_szFileName);
}


void CEterPackManager::RegisterRootPack(const char * c_szName)
{
	assert(c_szName);
	if (m_pFoxFS)
	{
		int errorCode = 0;
		if ((errorCode = FoxFS_LoadA(m_pFoxFS, c_szName)) != FoxFS::ERROR_OK)
		{
			TraceError("%s: Error Code %d", c_szName, errorCode);
		}
	}
	else
	{
		TraceError("Pack: Not initialized!");
	}
}

bool CEterPackManager::RegisterPack(const char * c_szName, const char * c_szDirectory)
{
	assert(c_szName);
	if (m_pFoxFS)
	{
		int errorCode = 0;
		if ((errorCode = FoxFS_LoadA(m_pFoxFS, c_szName)) != FoxFS::ERROR_OK)
		{
			TraceError("%s: Error Code %d", c_szName, errorCode);
		}
	}
	else
	{
		TraceError("Pack: Not initialized!");
	}

	return false;
}

void CEterPackManager::SetSearchMode(bool bPackFirst)
{
	m_iSearchMode = bPackFirst ? SEARCH_PACK_FIRST : SEARCH_FILE_FIRST;
}

int CEterPackManager::GetSearchMode()
{
	return m_iSearchMode;
}

CEterPackManager::CEterPackManager() : m_bTryRelativePath(false), m_iSearchMode(SEARCH_FILE_FIRST), m_isCacheMode(false)
{
	InitializeCriticalSection(&m_csFinder);
	m_pFoxFS = FoxFS_Create();

}

CEterPackManager::~CEterPackManager()
{
	DeleteCriticalSection(&m_csFinder);
	if (m_pFoxFS)
	{
		FoxFS_Destroy(m_pFoxFS);
	}
}
