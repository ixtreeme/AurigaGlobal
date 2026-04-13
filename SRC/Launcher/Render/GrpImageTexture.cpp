#include "StdAfx.h"
#include "../Base/MappedFile.h"
#include "../Pack/EterPackManager.h"
#include "GrpImageTexture.h"

bool CGraphicImageTexture::Lock(int* pRetPitch, void** ppRetPixels, int level)
{
	D3DLOCKED_RECT lockedRect;
	if (FAILED(m_lpd3dTexture->LockRect(level, &lockedRect, NULL, 0)))
		return false;

	*pRetPitch = lockedRect.Pitch;
	*ppRetPixels = (void*)lockedRect.pBits;
	return true;
}

void CGraphicImageTexture::Unlock(int level)
{
	assert(m_lpd3dTexture != NULL);
	m_lpd3dTexture->UnlockRect(level);
}

void CGraphicImageTexture::Initialize()
{
	CGraphicTexture::Initialize();

	m_stFileName = "";

	m_d3dFmt = D3DFMT_UNKNOWN;
	m_dwFilter = 0;
}

void CGraphicImageTexture::Destroy()
{
	CGraphicTexture::Destroy();

	Initialize();
}

bool CGraphicImageTexture::CreateDeviceObjects()
{
	assert(ms_lpd3dDevice != NULL);
	assert(m_lpd3dTexture == NULL);

	if (m_stFileName.empty())
	{
		// 폰트 텍스쳐
		if (FAILED(ms_lpd3dDevice->CreateTexture(m_width, m_height, 1, 0, m_d3dFmt, D3DPOOL_MANAGED, &m_lpd3dTexture, nullptr)))
			return false;
	}
	else
	{
		CMappedFile	mappedFile;
		const void* c_pvMap;

		if (!CEterPackManager::Instance().Get(mappedFile, m_stFileName.c_str(), &c_pvMap))
			return false;

		//@fixme002
		if (!CreateFromMemoryFile(mappedFile.Size(), c_pvMap, m_d3dFmt, m_dwFilter))
		{
			TraceError("CGraphicImageTexture::CreateDeviceObjects: CreateFromMemoryFile: texture not found(%s)", m_stFileName.c_str());
			return false;
		}
		return true;
		// return CreateFromMemoryFile(mappedFile.Size(), c_pvMap, m_d3dFmt, m_dwFilter);
	}

	m_bEmpty = false;
	return true;
}

bool CGraphicImageTexture::Create(UINT width, UINT height, D3DFORMAT d3dFmt, uint32_t dwFilter)
{
	assert(ms_lpd3dDevice != NULL);
	Destroy();

	m_width = width;
	m_height = height;
	m_d3dFmt = d3dFmt;
	m_dwFilter = dwFilter;

	return CreateDeviceObjects();
}

void CGraphicImageTexture::CreateFromTexturePointer(const CGraphicTexture* c_pSrcTexture)
{
	if (m_lpd3dTexture)
		m_lpd3dTexture->Release();

	m_width = c_pSrcTexture->GetWidth();
	m_height = c_pSrcTexture->GetHeight();
	m_lpd3dTexture = c_pSrcTexture->GetD3DTexture();

	if (m_lpd3dTexture)
		m_lpd3dTexture->AddRef();

	m_bEmpty = false;
}


bool CGraphicImageTexture::CreateFromMemoryFile(UINT bufSize, const void* c_pvBuf, D3DFORMAT d3dFmt, uint32_t dwFilter)
{
	assert(ms_lpd3dDevice != nullptr);
	assert(m_lpd3dTexture == nullptr);

	D3DXIMAGE_INFO info;
	if (FAILED(D3DXCreateTextureFromFileInMemoryEx(
		ms_lpd3dDevice,
		c_pvBuf,
		bufSize,
		D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
		0, d3dFmt, D3DPOOL_MANAGED,
		dwFilter, dwFilter,
		0xffff00ff,
		&info,
		nullptr,
		&m_lpd3dTexture)))
	{
		TraceError("CreateFromMemoryFile: Cannot create texture");
		return false;
	}

	m_width = info.Width;
	m_height = info.Height;

	D3DFORMAT optimalFormat = info.Format;
	switch (optimalFormat)
	{
	case D3DFMT_A8R8G8B8: optimalFormat = D3DFMT_A4R4G4B4; break;
	case D3DFMT_X8R8G8B8:
	case D3DFMT_R8G8B8:   optimalFormat = D3DFMT_A1R5G5B5; break;
	}

	bool GRAPHICS_CAPS_HALF_SIZE_IMAGE = false;

	UINT texBias = (GRAPHICS_CAPS_HALF_SIZE_IMAGE ? 1 : 0);

	if (IsLowTextureMemory() && (texBias || optimalFormat != info.Format))
	{
		IDirect3DTexture9* srcTex = m_lpd3dTexture;
		IDirect3DTexture9* dstTex = nullptr;

		if (SUCCEEDED(D3DXCreateTexture(
			ms_lpd3dDevice,
			info.Width >> texBias,
			info.Height >> texBias,
			info.MipLevels,
			0,
			optimalFormat,
			D3DPOOL_MANAGED,
			&dstTex)))
		{
			m_lpd3dTexture = dstTex;

			for (UINT i = 0; i < info.MipLevels; ++i)
			{
				IDirect3DSurface9* srcSurf = nullptr;
				IDirect3DSurface9* dstSurf = nullptr;

				if (SUCCEEDED(srcTex->GetSurfaceLevel(i, &srcSurf)) &&
					SUCCEEDED(dstTex->GetSurfaceLevel(i, &dstSurf)))
				{
					D3DXLoadSurfaceFromSurface(dstSurf, nullptr, nullptr, srcSurf, nullptr, nullptr, D3DX_FILTER_POINT, 0);
				}

				if (dstSurf) dstSurf->Release();
				if (srcSurf) srcSurf->Release();
			}

			srcTex->Release();
		}
	}

	m_bEmpty = false;
	return true;
}

void CGraphicImageTexture::SetFileName(const char* c_szFileName)
{
	m_stFileName = c_szFileName;
}

bool CGraphicImageTexture::CreateFromDiskFile(const char* c_szFileName, D3DFORMAT d3dFmt, uint32_t dwFilter)
{
	Destroy();

	SetFileName(c_szFileName);

	m_d3dFmt = d3dFmt;
	m_dwFilter = dwFilter;
	return CreateDeviceObjects();
}

CGraphicImageTexture::CGraphicImageTexture()
{
	CGraphicImageTexture::Initialize();
}

CGraphicImageTexture::~CGraphicImageTexture()
{
	CGraphicImageTexture::Destroy();
}
