#include "StdAfx.h"
#include "ScreenshotManager.h"

#include "GrpBase.h"
#include "Input.h"

bool SaveDirectX9ScreenShot(const char* szFileName)
{
    IDirect3DDevice9* dev = CGraphicBase::GetD3DDevice();
    if (!dev) return false;

    IDirect3DSurface9* back = nullptr;
    HRESULT hr = dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    if (FAILED(hr) || !back) return false;

    D3DSURFACE_DESC desc{};
    back->GetDesc(&desc);

    IDirect3DSurface9* src = back;
    IDirect3DSurface9* resolved = nullptr;

    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        hr = dev->CreateRenderTarget(desc.Width, desc.Height, desc.Format,
            D3DMULTISAMPLE_NONE, 0, FALSE, &resolved, nullptr);
        if (FAILED(hr) || !resolved) { back->Release(); return false; }

        hr = dev->StretchRect(back, nullptr, resolved, nullptr, D3DTEXF_NONE);
        if (FAILED(hr)) { resolved->Release(); back->Release(); return false; }

        src = resolved;
    }

    hr = D3DXSaveSurfaceToFileA(szFileName, D3DXIFF_PNG, src, nullptr, nullptr);

    if (resolved) resolved->Release();
    back->Release();

    return SUCCEEDED(hr);
}
