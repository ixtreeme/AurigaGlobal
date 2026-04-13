#pragma once

#include <windows.h>
#include "../Base/Singleton.h"
#include "../Base/Stl.h"
#include "../Base/MappedFile.h"


#include "FoxFS.h"


class CEterPackManager : public CSingleton<CEterPackManager>
{
	public:
		enum ESearchModes
		{
			SEARCH_FILE,
			SEARCH_PACK
		};
	
		enum ESearchModes2
		{
			SEARCH_FILE_FIRST,
			SEARCH_PACK_FIRST
		};

		CEterPackManager();
		virtual ~CEterPackManager();

		void SetCacheMode();
		void SetRelativePathMode();

		void SetSearchMode(bool bPackFirst);
		int GetSearchMode();

		bool Get(CMappedFile & rMappedFile, const char * c_szFileName, const void** pData);
		bool GetFromPack(CMappedFile & rMappedFile, const char * c_szFileName, const void** pData);
		bool GetFromFile(CMappedFile & rMappedFile, const char * c_szFileName, const void** pData);

		bool isExist(const char * c_szFileName);
		bool isExistInPack(const char * c_szFileName);

		bool RegisterPack(const char * c_szName, const char * c_szDirectory);
		void RegisterRootPack(const char * c_szName);

	protected:
		bool					m_bTryRelativePath;
		bool					m_isCacheMode;
		int						m_iSearchMode;

		CRITICAL_SECTION		m_csFinder;
		PFoxFS					m_pFoxFS;
};
