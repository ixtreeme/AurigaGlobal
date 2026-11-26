#ifndef __MOB_CSV_READER_H__
#define __MOB_CSV_READER_H__

#include <iostream>
#include <map>

#include "CsvReader.h"




int get_Mob_Rank_Value(std::string inputString);
int get_Mob_Type_Value(std::string inputString);
int get_Mob_BattleType_Value(std::string inputString);

int get_Mob_Size_Value(std::string inputString);
int get_Mob_AIFlag_Value(std::string inputString);
int get_Mob_RaceFlag_Value(std::string inputString);
int get_Mob_ImmuneFlag_Value(std::string inputString);

//
bool Set_Proto_Mob_Table(TMobTable *mobTable, cCsvTable &csvTable, std::map<int,const char*> &nameMap);
#endif
