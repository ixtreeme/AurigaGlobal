#include "StdAfx.h"
#include "EffectElementBase.h"


void CEffectElementBase::GetPosition(float fTime, D3DXVECTOR3 & rPosition)
{
	rPosition = GetTimeEventBlendValue(fTime, m_TimeEventTablePosition);
}

bool CEffectElementBase::isData()
{
	return OnIsData();
}

void CEffectElementBase::Clear()
{
	m_fStartTime = 0.0f;

	OnClear();
}

bool CEffectElementBase::LoadScript(CTextFileLoader & rTextFileLoader)
{
	CTokenVector * pTokenVector;
	if (!rTextFileLoader.GetTokenFloat("starttime",&m_fStartTime))
	{
		m_fStartTime = 0.0f;
	}
	if (rTextFileLoader.GetTokenVector("timeeventposition", &pTokenVector))
	{
		m_TimeEventTablePosition.clear();

		uint32_t dwIndex = 0;
		for (uint32_t i = 0; i < pTokenVector->size(); ++dwIndex)
		{
			TEffectPosition EffectPosition;
			EffectPosition.m_fTime = std::stof(pTokenVector->at(i++));
			if (pTokenVector->at(i)=="MOVING_TYPE_BEZIER_CURVE")
			{
				i++;

				EffectPosition.m_iMovingType = MOVING_TYPE_BEZIER_CURVE;

				EffectPosition.m_Value.x = std::stof(pTokenVector->at(i++));
				EffectPosition.m_Value.y = std::stof(pTokenVector->at(i++));
				EffectPosition.m_Value.z = std::stof(pTokenVector->at(i++));

				EffectPosition.m_vControlPoint.x = std::stof(pTokenVector->at(i++));
				EffectPosition.m_vControlPoint.y = std::stof(pTokenVector->at(i++));
				EffectPosition.m_vControlPoint.z = std::stof(pTokenVector->at(i++));
			}
			else if (pTokenVector->at(i) == "MOVING_TYPE_DIRECT")
			{
				i++;

				EffectPosition.m_iMovingType = MOVING_TYPE_DIRECT;

				EffectPosition.m_Value.x = std::stof(pTokenVector->at(i++));
				EffectPosition.m_Value.y = std::stof(pTokenVector->at(i++));
				EffectPosition.m_Value.z = std::stof(pTokenVector->at(i++));

				EffectPosition.m_vControlPoint = D3DXVECTOR3(0.0f,0.0f,0.0f);
			}
			else
			{
				return FALSE;
			}

			m_TimeEventTablePosition.push_back(EffectPosition);
		}
	}

	return OnLoadScript(rTextFileLoader);
}

float CEffectElementBase::GetStartTime()
{
	return m_fStartTime;
}

CEffectElementBase::CEffectElementBase()
{
	m_fStartTime = 0.0f;
}
CEffectElementBase::~CEffectElementBase() = default;