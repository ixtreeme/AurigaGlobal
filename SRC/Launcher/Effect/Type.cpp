#include "StdAfx.h"
#include "Type.h"

bool GetTokenTimeEventFloat(CTextFileLoader & rTextFileLoader, const char * c_szKey, TTimeEventTableFloat * pTimeEventTableFloat)
{
	CTokenVector * pTokenVector;
	if (!rTextFileLoader.GetTokenVector(c_szKey, &pTokenVector))
		return FALSE;

	pTimeEventTableFloat->clear();
	pTimeEventTableFloat->resize(pTokenVector->size() / 2);

	uint32_t dwIndex = 0;
	for (uint32_t i = 0; i < pTokenVector->size(); i+=2, ++dwIndex)
	{
		pTimeEventTableFloat->at(dwIndex).m_fTime = std::stof(pTokenVector->at(i));
		pTimeEventTableFloat->at(dwIndex).m_Value = std::stof(pTokenVector->at(i+1));
	}

	return TRUE;
}

void InsertItemTimeEventFloat(TTimeEventTableFloat * pTable, float fTime, float fValue)
{
	auto itor = pTable->begin();
	for (; itor != pTable->end(); ++itor)
	{
		if (auto& [m_fTime, m_Value] = *itor; m_fTime > fTime)
			break;
	}

	TTimeEventTypeFloat TimeEvent;
	TimeEvent.m_fTime = fTime;
	TimeEvent.m_Value = fValue;

	pTable->insert(itor, TimeEvent);
}
