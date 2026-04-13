#include "StdAfx.h"
#include "../Base/Stl.h"
#include "GrpTexture.h"
#include "StateManager.h"
#ifdef LEADERBOARD_RAZOR93
void CGraphicTexture::AttachExternalTexture(LPDIRECT3DTEXTURE9 tex)
{
	TraceError("[LB] AttachExternalTexture: old=%p new=%p", m_lpd3dTexture, tex);
	if (m_lpd3dTexture == tex) { TraceError("[LB] AttachExternalTexture: same pointer, skip"); return; }
	if (m_lpd3dTexture) m_lpd3dTexture->Release();
	m_lpd3dTexture = tex;
	if (m_lpd3dTexture) m_lpd3dTexture->AddRef();
	m_bEmpty = (m_lpd3dTexture == nullptr);
	TraceError("[LB] AttachExternalTexture: done, empty=%d", m_bEmpty ? 1 : 0);
}
#endif

void CGraphicTexture::DestroyDeviceObjects()
{
	safe_release(m_lpd3dTexture);
}

void CGraphicTexture::Destroy()
{
	DestroyDeviceObjects();

	Initialize();
}

void CGraphicTexture::Initialize()
{
	m_lpd3dTexture = nullptr;
	m_width = 0;
	m_height = 0;
	m_bEmpty = true;
}

bool CGraphicTexture::IsEmpty() const
{
	return m_bEmpty;
}

void CGraphicTexture::SetTextureStage(int stage) const
{
	assert(ms_lpd3dDevice != NULL);
	STATEMANAGER.SetTexture(stage, m_lpd3dTexture);
}

LPDIRECT3DTEXTURE9 CGraphicTexture::GetD3DTexture() const
{
	return m_lpd3dTexture;
}

int CGraphicTexture::GetWidth() const
{
	return m_width;
}

int CGraphicTexture::GetHeight() const
{
	return m_height;
}

CGraphicTexture::CGraphicTexture()
{
	Initialize();
}

CGraphicTexture::~CGraphicTexture() = default;
