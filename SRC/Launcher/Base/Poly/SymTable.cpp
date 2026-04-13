#include "../StdAfx.h"
#include "SymTable.h"

//using namespace std;

CSymTable::CSymTable(int aTok, std::string aStr) : dVal(0), token(aTok), strlex(aStr)
{
}

CSymTable::~CSymTable()
{
}

