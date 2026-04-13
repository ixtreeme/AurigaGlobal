#pragma once

class CPropertyManager : public CSingleton<CPropertyManager>
{
	public:
		CPropertyManager();
		virtual ~CPropertyManager();

		void			Clear();
		bool			BuildPack();

		bool			LoadReservedCRC(const char * c_pszFileName);
		void			ReserveCRC(uint32_t dwCRC);
		uint32_t			GetUniqueCRC(const char * c_szSeed);

		bool			Initialize(const char * c_pszPackFileName = nullptr);
		bool			Register(const char * c_pszFileName, CProperty ** ppProperty = nullptr);

		bool			Get(uint32_t dwCRC, CProperty ** ppProperty);
		bool			Get(const char * c_pszFileName, CProperty ** ppProperty);

#ifndef ENABLE_LOAD_PROPERTY_XML
		bool			Put(const char * c_pszFileName, const char * c_pszSourceFileName);
		bool			Erase(uint32_t dwCRC);
		bool			Erase(const char * c_pszFileName);
#endif
	protected:
		typedef std::map<uint32_t, CProperty *>		TPropertyCRCMap;
		typedef std::set<uint32_t>						TCRCSet;

		bool										m_isFileMode;
		TPropertyCRCMap								m_PropertyByCRCMap;
		TCRCSet										m_ReservedCRCSet;
#ifndef ENABLE_LOAD_PROPERTY_XML
		CEterPack									m_pack;
		CEterFileDict								m_fileDict;
#endif
};
