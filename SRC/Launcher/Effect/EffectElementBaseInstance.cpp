#include "StdAfx.h"
#include "EffectElementBaseInstance.h"


void CEffectElementBaseInstance::SetTimeScale(float fTimeScale)
{
	if (fTimeScale <= 0.01f)
		fTimeScale = 0.01f;

	m_fTimeScale = fTimeScale;
}


bool CEffectElementBaseInstance::Update(float fElapsedTime)
{
	if (m_bStart)
	{
		const float fScaledElapsedTime = fElapsedTime * m_fTimeScale;

		m_fElapsedTime = fScaledElapsedTime;
		m_fLocalTime += fScaledElapsedTime;

		return OnUpdate(fScaledElapsedTime);
	}
	else
	{
		m_fRemainingTime -= fElapsedTime;
		if (m_fRemainingTime <= 0.0f)
			m_bStart = true;

		return true;
	}
}

void CEffectElementBaseInstance::Render()
{
	if (!m_bStart)
		return;

	assert(mc_pmatLocal);

	OnRender();
}

void CEffectElementBaseInstance::SetLocalMatrixPointer(const D3DXMATRIX * c_pMatrix)
{
	mc_pmatLocal = c_pMatrix;
}

void CEffectElementBaseInstance::SetDataPointer(CEffectElementBase * pElement)
{
	m_pBase = pElement;

	m_dwStartTime = CTimer::Instance().GetCurrentMillisecond();

	//////////////////////////////////////////////////////////////////////////
	//add by ipkn, start time management

	m_fRemainingTime = pElement->GetStartTime();
	if (m_fRemainingTime<=0.0f)
		m_bStart = true;
	else
		m_bStart = false;

	//////////////////////////////////////////////////////////////////////////

	OnSetDataPointer(pElement);
}

bool CEffectElementBaseInstance::isActive()
{
	return m_isActive;
}
void CEffectElementBaseInstance::SetActive()
{
	m_isActive = true;
}
void CEffectElementBaseInstance::SetDeactive()
{
	m_isActive = false;
}

void CEffectElementBaseInstance::Initialize()
{
	mc_pmatLocal = nullptr;

	m_isActive = true;

	m_fLocalTime = 0.0f;
	m_dwStartTime = 0;
	m_fElapsedTime = 0.0f;

	m_bStart = false;
	m_fRemainingTime = 0.0f;
	//m_fTimeScale = 1.0f;
	m_wikiIgnoreFrustum = false;

	OnInitialize();
}

void CEffectElementBaseInstance::Destroy()
{
	OnDestroy();
	Initialize();
}

CEffectElementBaseInstance::CEffectElementBaseInstance() : m_wikiIgnoreFrustum(false), mc_pmatLocal(nullptr),
                                                           m_isActive(false), m_fLocalTime(0),
                                                           m_dwStartTime(0),
                                                           m_fElapsedTime(0),
                                                           m_fRemainingTime(0), m_fTimeScale(1.0f),
                                                           m_bStart(false),
                                                           m_pBase(nullptr)
{
}

CEffectElementBaseInstance::~CEffectElementBaseInstance() = default;
