#ifndef __LOCALE_SERVCICE__
#define __LOCALE_SERVCICE__

#include <string>

// Flat (single-language) server data layout: data_path is the root of the
// map/quest/drop/config files (default "data"), locale is the MySQL charset.
bool LocaleService_InitFlat(const std::string& dataPath, const std::string& locale);
void LocaleService_LoadEmpireTextConvertTables();
void LocaleService_TransferDefaultSetting();
const std::string& LocaleService_GetBasePath();
const std::string& LocaleService_GetMapPath();
const std::string& LocaleService_GetQuestPath();

#endif