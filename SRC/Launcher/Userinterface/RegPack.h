#pragma once

#include "StdAfx.h"

inline bool PackInitialize(const char* c_pszFolder)
{

	//struct stat st;
	//if (stat("D:\\ymir work", &st) == 0) {
	//	MessageBox(nullptr, "Please remove the folder D:\\Ymir Work!", "BwMt2", MB_ICONSTOP);
	//	return false;
	//}

	if (_access(c_pszFolder, 0) != 0)
		return false;

	std::string stFolder(c_pszFolder);
	stFolder += "/";

	CTextFileLoader::SetCacheMode();

	CEterPackManager::Instance().SetCacheMode();
	CEterPackManager::Instance().SetSearchMode(CEterPackManager::SEARCH_PACK);

	//PackMode::Instance().SetPackMode();
	//CEterPackManager::Instance().RegisterPack("pack/bgm.AG", ""); Nem innen olvassuk már.
	CEterPackManager::Instance().RegisterPack("pack/building.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/effect.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/etc.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/guild.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/icon.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/item.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/locale.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/maps.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/monster.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/monster2.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/npc.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/npc2.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch1.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch2.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch3.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch4.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch5.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch6.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch7.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch8.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch9.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch10.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch11.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/patch_halloween_2021.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/pc.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/pc2.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/property.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/seasons.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/sound.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/terrain.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/textureset.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/tree.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/zone.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/zone2.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/zone3.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/zone4.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/zone5.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/razor93.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/razor93_2.AG", "");
	CEterPackManager::Instance().RegisterPack("pack/razor93_3.AG", "");

#ifdef _DEBUG
	CEterPackManager::Instance().RegisterRootPack((stFolder + std::string("root.AG")).c_str());
#else
	CEterPackManager::Instance().RegisterRootPack((stFolder + std::string("root.AG")).c_str());
#endif

	return true;
}