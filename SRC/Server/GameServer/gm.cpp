#include "stdafx.h"
#include <Core/Logging.hpp>
#include "constants.h"
#include "gm.h"
#include "locale_service.h"
#include "config.h"

//ADMIN_MANAGER
std::set<std::string> g_set_Host;
std::map<std::string, tGM> g_map_GM;

void gm_new_clear()
{
	g_set_Host.clear();
	g_map_GM.clear();
}

void gm_new_insert( const tAdminInfo &rAdminInfo )
{
	LOG_INFO("InsertGMList(account:{}, player:{}, contact_ip:{}, server_ip:{}, auth:{})", rAdminInfo.m_szAccount, rAdminInfo.m_szName, rAdminInfo.m_szContactIP, rAdminInfo.m_szServerIP, rAdminInfo.m_Authority);

	tGM t;

	if ( strlen( rAdminInfo.m_szContactIP ) == 0 )
	{
		t.pset_Host = &g_set_Host;
		LOG_INFO("GM Use ContactIP");
	}
	else
	{
		t.pset_Host = nullptr;
		LOG_INFO("GM Use Default Host List");
	}

	memcpy ( &t.Info, &rAdminInfo, sizeof ( rAdminInfo ) );

	g_map_GM[rAdminInfo.m_szName] = t;

}

void gm_new_host_inert( const char * host )
{
	g_set_Host.insert( host );
	LOG_INFO("InsertGMHost(ip:{})", host);
}

uint8_t gm_new_get_level( const char * name, const char * host, const char* account)
{
	if ( test_server ) return GM_IMPLEMENTOR;

	std::map<std::string, tGM >::iterator it = g_map_GM.find(name);

	if (g_map_GM.end() == it)
		return GM_PLAYER;

	// GERMAN_GM_NOT_CHECK_HOST
	// 독일 버전은 호스트 체크를 하지 않는다.
#ifdef ENABLE_NEWSTUFF
	if (!g_bGMHostCheck)
#else
	if (true)
#endif
	{
	    if (account)
	    {
		if ( strcmp ( it->second.Info.m_szAccount, account  ) != 0 )
		{
		    LOG_INFO("GM_NEW_GET_LEVEL : BAD ACCOUNT [ACCOUNT:{}/{}", it->second.Info.m_szAccount, account);
		    return GM_PLAYER;
		}
	    }
	    LOG_INFO("GM_NEW_GET_LEVEL : FIND ACCOUNT");
	    return it->second.Info.m_Authority;
	}
	// END_OF_GERMAN_GM_NOT_CHECK_HOST
	else
	{

	    if ( host )
	    {
		if ( it->second.pset_Host )
		{
		    if ( it->second.pset_Host->end() == it->second.pset_Host->find( host ) )
		    {
			LOG_INFO("GM_NEW_GET_LEVEL : BAD HOST IN HOST_LIST");
			return GM_PLAYER;
		    }
		}
		else
		{
		    if ( strcmp ( it->second.Info.m_szContactIP, host  ) != 0 )
		    {
			LOG_INFO("GM_NEW_GET_LEVEL : BAD HOST IN GMLIST");
			return GM_PLAYER;
		    }
		}
	    }
	    LOG_INFO("GM_NEW_GET_LEVEL : FIND HOST");

	    return it->second.Info.m_Authority;
	}
	return GM_PLAYER;
}

//END_ADMIN_MANAGER
uint8_t gm_get_level(const char * name, const char * host, const char* account)
{
	return gm_new_get_level( name, host, account );
}

