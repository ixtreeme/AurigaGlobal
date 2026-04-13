#include "StdAfx.h"


#include "../Base/Stl.h"
#include "../Base/Utils.h"

#include "StateManager.h"
#include "GrpWikiRenderTargetTexture.h"

CGraphicWikiRenderTargetTexture::~CGraphicWikiRenderTargetTexture() { Reset(); }
CGraphicWikiRenderTargetTexture::CGraphicWikiRenderTargetTexture() :
															m_d3dFormat{D3DFMT_UNKNOWN},
															m_depthStencilFormat{D3DFMT_UNKNOWN}
{
	Initialize();
	
	memset(&m_renderRect, 0, sizeof(m_renderRect));
	memset(&m_renderBox, 0, sizeof(m_renderBox));
}

/*----------------------------
--------PUBLIC CLASS FUNCTIONS
-----------------------------*/

bool CGraphicWikiRenderTargetTexture::Create(const int width, const int height, const D3DFORMAT texFormat, const D3DFORMAT depthFormat)
{
	Reset();
	
	m_height = height;
	m_width = width;
	
	if (!CreateRenderTexture(width, height, texFormat))
		return false;
	
	if (!CreateRenderDepthStencil(width, height, depthFormat))
		return false;
	
	return true;
}

int CGraphicWikiRenderTargetTexture::GetWidth() {
	return m_width;
}

int CGraphicWikiRenderTargetTexture::GetHeight() {
	return m_height;
}

void CGraphicWikiRenderTargetTexture::CreateTextures()
{
	if (CreateRenderTexture(m_width, m_height, m_d3dFormat))
		CreateRenderDepthStencil(m_width, m_height, m_depthStencilFormat);
}

bool CGraphicWikiRenderTargetTexture::CreateRenderTexture(const int width, const int height, const D3DFORMAT format)
{
	m_d3dFormat = format;
	
	if (FAILED(ms_lpd3dDevice->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, m_d3dFormat, D3DPOOL_DEFAULT, &m_lpd3dRenderTexture, nullptr)))
		return false;
	
	if (FAILED(m_lpd3dRenderTexture->GetSurfaceLevel(0, &m_lpd3dRenderTargetSurface)))
		return false;
	
	return true;
}

void CGraphicWikiRenderTargetTexture::ReleaseTextures()
{
	SAFE_RELEASE(m_lpd3dRenderTexture);
	SAFE_RELEASE(m_lpd3dRenderTargetSurface);
	SAFE_RELEASE(m_lpd3dDepthSurface);
	SAFE_RELEASE(m_lpd3dDepthSurface);
	SAFE_RELEASE(m_lpd3dOriginalRenderTarget);
	SAFE_RELEASE(m_lpd3dOldDepthBufferSurface);
	
	memset(&m_renderRect, 0, sizeof(m_renderRect));
	memset(&m_renderBox, 0, sizeof(m_renderBox));
}

LPDIRECT3DTEXTURE9 CGraphicWikiRenderTargetTexture::GetD3DRenderTargetTexture() const
{
	return m_lpd3dRenderTexture;
}

bool CGraphicWikiRenderTargetTexture::CreateRenderDepthStencil(const int width, const int height, const D3DFORMAT format)
{
	m_depthStencilFormat = format;
	
	return (ms_lpd3dDevice->CreateDepthStencilSurface(width, height, m_depthStencilFormat, D3DMULTISAMPLE_NONE, 0, false, &m_lpd3dDepthSurface, nullptr)) == D3D_OK;
}

void CGraphicWikiRenderTargetTexture::SetRenderTarget()
{
	ms_lpd3dDevice->GetRenderTarget(0, &m_lpd3dOriginalRenderTarget);
	ms_lpd3dDevice->GetDepthStencilSurface(&m_lpd3dOldDepthBufferSurface);
	ms_lpd3dDevice->SetRenderTarget(0, m_lpd3dRenderTargetSurface);
	ms_lpd3dDevice->SetDepthStencilSurface(m_lpd3dDepthSurface);
}

void CGraphicWikiRenderTargetTexture::ResetRenderTarget()
{
	ms_lpd3dDevice->SetRenderTarget(0, m_lpd3dOriginalRenderTarget);
	ms_lpd3dDevice->SetDepthStencilSurface(m_lpd3dOldDepthBufferSurface);
	
	SAFE_RELEASE(m_lpd3dOriginalRenderTarget);
	SAFE_RELEASE(m_lpd3dOldDepthBufferSurface);
}

void CGraphicWikiRenderTargetTexture::SetRenderingRect(RECT* rect)
{
	m_renderRect = *rect;
}

RECT* CGraphicWikiRenderTargetTexture::GetRenderingRect()
{
	return &m_renderRect;
}

void CGraphicWikiRenderTargetTexture::SetRenderingBox(RECT* renderBox)
{
	m_renderBox = *renderBox;
}

RECT* CGraphicWikiRenderTargetTexture::GetRenderingBox()
{
	return &m_renderBox;
}

void CGraphicWikiRenderTargetTexture::Clear()
{
	ms_lpd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
}

void CGraphicWikiRenderTargetTexture::Render() const
{
	const float sx = static_cast<float>(m_renderRect.left) - 0.5f + static_cast<float>(m_renderBox.left);
	const float sy = static_cast<float>(m_renderRect.top) - 0.5f + static_cast<float>(m_renderBox.top);
	const float ex = static_cast<float>(m_renderRect.left) + (static_cast<float>(m_renderRect.right) - static_cast<float>(m_renderRect.left)) - 0.5f - static_cast<float>(m_renderBox.right);
	const float ey = static_cast<float>(m_renderRect.top) + (static_cast<float>(m_renderRect.bottom) - static_cast<float>(m_renderRect.top)) - 0.5f - static_cast<float>(m_renderBox.bottom);
	
	const float texReverseWidth = 1.0f / (static_cast<float>(m_renderRect.right) - static_cast<float>(m_renderRect.left));
	const float texReverseHeight = 1.0f / (static_cast<float>(m_renderRect.bottom) - static_cast<float>(m_renderRect.top));
	
	const float su = m_renderBox.left * texReverseWidth;
	const float sv = m_renderBox.top * texReverseHeight;
	const float eu = ((m_renderRect.right - m_renderRect.left) - m_renderBox.right) * texReverseWidth;
	const float ev = ((m_renderRect.bottom - m_renderRect.top) - m_renderBox.bottom) * texReverseHeight;
	
	TPDTVertex pVertices[4];
	pVertices[0].position = TPosition(sx, sy, 0.0f);
	pVertices[0].texCoord = TTextureCoordinate(su, sv);
	pVertices[0].diffuse = 0xFFFFFFFF;
	
	pVertices[1].position = TPosition(ex, sy, 0.0f);
	pVertices[1].texCoord = TTextureCoordinate(eu, sv);
	pVertices[1].diffuse = 0xFFFFFFFF;
	
	pVertices[2].position = TPosition(sx, ey, 0.0f);
	pVertices[2].texCoord = TTextureCoordinate(su, ev);
	pVertices[2].diffuse = 0xFFFFFFFF;
	
	pVertices[3].position = TPosition(ex, ey, 0.0f);
	pVertices[3].texCoord = TTextureCoordinate(eu, ev);
	pVertices[3].diffuse = 0xFFFFFFFF;
	
	if (SetPDTStream(pVertices, 4))
	{
		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);
		
		STATEMANAGER.SetTexture(0, GetD3DRenderTargetTexture());
		STATEMANAGER.SetTexture(1, nullptr);
		STATEMANAGER.SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
		STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);
	}
}

/*----------------------------
------PROTECTED CLASS FUNCTIONS
-----------------------------*/

void CGraphicWikiRenderTargetTexture::Reset()
{
	Destroy();
	ReleaseTextures();
	
	m_d3dFormat = D3DFMT_UNKNOWN;
	m_depthStencilFormat = D3DFMT_UNKNOWN;
}
