#include "StdAfx.h"
#include "GrpDevice.h"
#include "../Base/Stl.h"
#include "../Base/Debug.h"


bool CPU_HAS_SSE2 = false;
bool GRAPHICS_CAPS_CAN_NOT_DRAW_LINE = false;
bool GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW = false;
bool GRAPHICS_CAPS_HALF_SIZE_IMAGE = false;
bool GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;
bool GRAPHICS_CAPS_SOFTWARE_TILING = false;

D3DPRESENT_PARAMETERS g_kD3DPP;
bool g_isBrowserMode = false;
RECT g_rcBrowser;

CGraphicDevice::CGraphicDevice()
	: m_uBackBufferCount(0)
{
	__Initialize();
}

CGraphicDevice::~CGraphicDevice()
{
	Destroy();
}

void CGraphicDevice::__Initialize()
{
	ms_lpd3d = nullptr;
	ms_lpd3dDevice = nullptr;
	ms_lpd3dMatStack = nullptr;

	ms_dwWavingEndTime = 0;
	ms_dwFlashingEndTime = 0;

	m_pStateManager = nullptr;

	__InitializeDefaultIndexBufferList();
	__InitializePDTVertexBufferList();
}

void CGraphicDevice::RegisterWarningString(UINT uiMsg, std::string  c_szString)
{
	m_kMap_strWarningMessage[uiMsg] = c_szString;
}

void CGraphicDevice::__WarningMessage(HWND hWnd, UINT uiMsg)
{
	if (m_kMap_strWarningMessage.end() == m_kMap_strWarningMessage.find(uiMsg))
		return;
	MessageBox(hWnd, m_kMap_strWarningMessage[uiMsg].c_str(), "Warning", MB_OK | MB_TOPMOST);
}

void CGraphicDevice::MoveWebBrowserRect(const RECT& c_rcWebPage)
{
	g_rcBrowser = c_rcWebPage;
}

void CGraphicDevice::EnableWebBrowserMode(const RECT& c_rcWebPage)
{
	if (!ms_lpd3dDevice)
		return;

	D3DPRESENT_PARAMETERS& rkD3DPP = ms_d3dPresentParameter;

	g_isBrowserMode = true;

	if (D3DSWAPEFFECT_COPY == rkD3DPP.SwapEffect)
		return;

	g_kD3DPP = rkD3DPP;
	g_rcBrowser = c_rcWebPage;

	//rkD3DPP.Windowed=TRUE;
	rkD3DPP.SwapEffect = D3DSWAPEFFECT_COPY;
	rkD3DPP.BackBufferCount = 1;
	rkD3DPP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	IDirect3DDevice9& rkD3DDev = *ms_lpd3dDevice;
	HRESULT hr = rkD3DDev.Reset(&rkD3DPP);
	if (FAILED(hr))
		return;

	STATEMANAGER.SetDefaultState();
}

void CGraphicDevice::DisableWebBrowserMode()
{
	if (!ms_lpd3dDevice)
		return;

	D3DPRESENT_PARAMETERS& rkD3DPP = ms_d3dPresentParameter;

	g_isBrowserMode = false;

	rkD3DPP = g_kD3DPP;

	IDirect3DDevice9& rkD3DDev = *ms_lpd3dDevice;
	HRESULT hr = rkD3DDev.Reset(&rkD3DPP);
	if (FAILED(hr))
		return;

	STATEMANAGER.SetDefaultState();
}

bool CGraphicDevice::ResizeBackBuffer(UINT uWidth, UINT uHeight)
{
	if (!ms_lpd3dDevice)
		return false;

	D3DPRESENT_PARAMETERS& rkD3DPP = ms_d3dPresentParameter;
	if (rkD3DPP.Windowed)
	{
		if (rkD3DPP.BackBufferWidth != uWidth || rkD3DPP.BackBufferHeight != uHeight)
		{
			rkD3DPP.BackBufferWidth = uWidth;
			rkD3DPP.BackBufferHeight = uHeight;

			IDirect3DDevice9& rkD3DDev = *ms_lpd3dDevice;

			HRESULT hr = rkD3DDev.Reset(&rkD3DPP);
			if (FAILED(hr))
			{
				return false;
			}

			STATEMANAGER.SetDefaultState();
		}
	}

	return true;
}

LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreatePNTStreamVertexShader()
{
	assert(ms_lpd3dDevice != NULL);

	LPDIRECT3DVERTEXDECLARATION9 dwShader = nullptr;

	if (constexpr D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
		}; ms_lpd3dDevice->CreateVertexDeclaration(pShaderDecl, &dwShader) != D3D_OK)
	{
		char szError[1024];
		sprintf(szError, "Failed to create CreatePNTStreamVertexShader");
		MessageBox(nullptr, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}


LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreatePNT2StreamVertexShader()
{
	assert(ms_lpd3dDevice != NULL);

	LPDIRECT3DVERTEXDECLARATION9 dwShader = nullptr;

	if (constexpr D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
		}; ms_lpd3dDevice->CreateVertexDeclaration(pShaderDecl, &dwShader) != D3D_OK)
	{
		char szError[1024];
		sprintf(szError, "Failed to create CreatePNT2StreamVertexShader");
		MessageBox(nullptr, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}

LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreatePTStreamVertexShader()
{
	assert(ms_lpd3dDevice != NULL);

	LPDIRECT3DVERTEXDECLARATION9 dwShader = nullptr;

	if (constexpr D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
		}; ms_lpd3dDevice->CreateVertexDeclaration(pShaderDecl, &dwShader) != D3D_OK)
	{
		char szError[1024];
		sprintf(szError, "Failed to create CreatePTStreamVertexShader");
		MessageBox(nullptr, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}


LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreateDoublePNTStreamVertexShader()
{
	assert(ms_lpd3dDevice != NULL);

	LPDIRECT3DVERTEXDECLARATION9 dwShader = nullptr;

	if (constexpr D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },

		{ 1, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
		{ 1, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 1 },
		{ 1, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
		}; ms_lpd3dDevice->CreateVertexDeclaration(pShaderDecl, &dwShader) != D3D_OK)
	{
		char szError[1024];
		sprintf(szError, "Failed to create CreateDoublePNTStreamVertexShader");
		MessageBox(nullptr, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}

CGraphicDevice::EDeviceState CGraphicDevice::GetDeviceState()
{
	if (!ms_lpd3dDevice)
		return DEVICESTATE_NULL;

	HRESULT hr;

	if (FAILED(hr = ms_lpd3dDevice->TestCooperativeLevel()))
	{
		if (D3DERR_DEVICELOST == hr)
			return DEVICESTATE_BROKEN;

		if (D3DERR_DEVICENOTRESET == hr)
			return DEVICESTATE_NEEDS_RESET;

		return DEVICESTATE_BROKEN;
	}

	return DEVICESTATE_OK;
}

bool CGraphicDevice::Reset()
{
#ifdef ENABLE_TEXT_RENEWAL
	__DestroyPDTVertexBufferList();

	HRESULT hr;
	if (FAILED(hr = ms_lpd3dDevice->Reset(&ms_d3dPresentParameter)))
	{
		TraceError("Failed to reset device");
#else
	HRESULT hr;

	if (FAILED(hr = ms_lpd3dDevice->Reset(&ms_d3dPresentParameter)))
	{
#endif
		return false;
	}

#ifdef ENABLE_TEXT_RENEWAL
	m_pStateManager->SetDefaultState();
	__CreatePDTVertexBufferList();
#endif
	return true;
	}

static LPDIRECT3DSURFACE9 s_lpStencil;
static uint32_t   s_MaxTextureWidth, s_MaxTextureHeight;

bool EL3D_ConfirmDevice(D3DCAPS9 & rkD3DCaps, UINT uBehavior)
{
	// PUREDEVICE는 GetTransform / GetViewport 등이 되지 않는다.
	if (uBehavior & D3DCREATE_PUREDEVICE)
		return FALSE;

	if (uBehavior & D3DCREATE_HARDWARE_VERTEXPROCESSING)
	{
		// DirectionalLight
		if (!(rkD3DCaps.VertexProcessingCaps & D3DVTXPCAPS_DIRECTIONALLIGHTS))
			return FALSE;

		// PositionalLight
		if (!(rkD3DCaps.VertexProcessingCaps & D3DVTXPCAPS_POSITIONALLIGHTS))
			return FALSE;

		// Software T&L Support - ATI NOT SUPPORT CLIP, USE DIRECTX SOFTWARE PROCESSING CLIPPING
		if (GRAPHICS_CAPS_SOFTWARE_TILING)
		{
			if (!(rkD3DCaps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPTLVERTS))
				return FALSE;
		}
		else
		{
			// Shadow/Terrain
			if (!(rkD3DCaps.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN))
				return FALSE;
		}
	}

	s_MaxTextureWidth = rkD3DCaps.MaxTextureWidth;
	s_MaxTextureHeight = rkD3DCaps.MaxTextureHeight;

	return TRUE;
}

uint32_t GetMaxTextureWidth()
{
	return s_MaxTextureWidth;
}

uint32_t GetMaxTextureHeight()
{
	return s_MaxTextureHeight;
}

int CGraphicDevice::Create(HWND hWnd, int iHres, int iVres, bool Windowed, int /*iBit*/, int iReflashRate)
{
	int iRet = CREATE_OK;

	Destroy();

	ms_iWidth = iHres;
	ms_iHeight = iVres;

	ms_hWnd = hWnd;
	ms_hDC = GetDC(hWnd);
	ms_lpd3d = Direct3DCreate9(D3D_SDK_VERSION);


	D3DADAPTER_IDENTIFIER9 rkD3DAdapterId;
	ms_lpd3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &rkD3DAdapterId);

	D3DDISPLAYMODE d3dDisplayMode;
	ms_lpd3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3dDisplayMode);


	if (Windowed &&
		strnicmp(rkD3DAdapterId.Driver, "3dfx", 4) == 0 &&
		D3DFMT_X8R8G8B8 == d3dDisplayMode.Format)
	{
		return CREATE_FORMAT;
	}


	D3DCAPS9 d3dCaps;
	ms_lpd3d->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps);

	bool isFormatConfirmed = FALSE;
	if (d3dCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
	{
		if (d3dCaps.DevCaps & D3DDEVCAPS_PUREDEVICE)
		{
			ms_dwD3DBehavior = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
			isFormatConfirmed = EL3D_ConfirmDevice(d3dCaps, ms_dwD3DBehavior);
		}

		if (!isFormatConfirmed)
		{
			ms_dwD3DBehavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;
			isFormatConfirmed = EL3D_ConfirmDevice(d3dCaps, ms_dwD3DBehavior);
		}

		if (!isFormatConfirmed)
		{
			ms_dwD3DBehavior = D3DCREATE_MIXED_VERTEXPROCESSING;
			isFormatConfirmed = EL3D_ConfirmDevice(d3dCaps, ms_dwD3DBehavior);
		}
	}

	if (!isFormatConfirmed)
	{
		ms_dwD3DBehavior = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		isFormatConfirmed = EL3D_ConfirmDevice(d3dCaps, ms_dwD3DBehavior);
		iRet |= CREATE_NO_TNL;
	}

	int ErrorCorrection = 0;

RETRY:
	ZeroMemory(&ms_d3dPresentParameter, sizeof(ms_d3dPresentParameter));

	ms_d3dPresentParameter.Windowed = Windowed;
	ms_d3dPresentParameter.BackBufferWidth = iHres;
	ms_d3dPresentParameter.BackBufferHeight = iVres;
	ms_d3dPresentParameter.hDeviceWindow = hWnd;
	ms_d3dPresentParameter.BackBufferFormat = Windowed ? D3DFMT_UNKNOWN : d3dDisplayMode.Format;
	ms_d3dPresentParameter.BackBufferCount = m_uBackBufferCount;
	ms_d3dPresentParameter.SwapEffect = D3DSWAPEFFECT_DISCARD;
	ms_d3dPresentParameter.MultiSampleType = D3DMULTISAMPLE_NONE;


	if (Windowed)
	{
		ms_d3dPresentParameter.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		ms_d3dPresentParameter.FullScreen_RefreshRateInHz = 0;
	}
	else
	{
		ms_d3dPresentParameter.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		ms_d3dPresentParameter.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	}

	ms_d3dPresentParameter.EnableAutoDepthStencil = TRUE;
	ms_d3dPresentParameter.AutoDepthStencilFormat = D3DFMT_D24S8;

	if (!Windowed)
	{
		D3DFORMAT msaaCheckFormat = ms_d3dPresentParameter.BackBufferFormat;
		if (SUCCEEDED(ms_lpd3d->CheckDeviceMultiSampleType(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			msaaCheckFormat,
			FALSE,
			D3DMULTISAMPLE_2_SAMPLES,
			&ms_d3dPresentParameter.MultiSampleQuality)))
		{
			ms_d3dPresentParameter.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
			ms_d3dPresentParameter.MultiSampleQuality = 0;
		}

		if (SUCCEEDED(ms_lpd3d->CheckDeviceMultiSampleType(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			msaaCheckFormat,
			FALSE,
			D3DMULTISAMPLE_4_SAMPLES,
			&ms_d3dPresentParameter.MultiSampleQuality)))
		{
			ms_d3dPresentParameter.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
			ms_d3dPresentParameter.MultiSampleQuality = 0;
		}
	}

	if (FAILED(ms_hLastResult = ms_lpd3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hWnd,
		ms_dwD3DBehavior,
		&ms_d3dPresentParameter,
		&ms_lpd3dDevice)))
	{
		switch (ms_hLastResult)
		{
		case D3DERR_INVALIDCALL:
			Tracen("IDirect3DDevice.CreateDevice - ERROR D3DERR_INVALIDCALL\nThe method call is invalid. For example, a method's parameter may have an invalid value.");
			break;
		case D3DERR_NOTAVAILABLE:
			Tracen("IDirect3DDevice.CreateDevice - ERROR D3DERR_NOTAVAILABLE\nThis device does not support the queried technique. ");
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			Tracen("IDirect3DDevice.CreateDevice - ERROR D3DERR_OUTOFVIDEOMEMORY\nDirect3D does not have enough display memory to perform the operation");
			break;
		default:
			Tracenf("IDirect3DDevice.CreateDevice - ERROR %d", ms_hLastResult);
			break;
		}

		if (ErrorCorrection)
			return CREATE_DEVICE;

		// 2004. 1. 9 myevan 큰의미 없는 코드인듯.. 에러나면 표시하고 종료하자
		iReflashRate = 0;
		++ErrorCorrection;
		iRet = CREATE_REFRESHRATE;
		goto RETRY;
	}

	// Check DXT Support Info
	if (ms_lpd3d->CheckDeviceFormat(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		ms_d3dPresentParameter.BackBufferFormat,
		0,
		D3DRTYPE_TEXTURE,
		D3DFMT_DXT1) == D3DERR_NOTAVAILABLE)
	{
		ms_bSupportDXT = false;
	}

	if (ms_lpd3d->CheckDeviceFormat(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		ms_d3dPresentParameter.BackBufferFormat,
		0,
		D3DRTYPE_TEXTURE,
		D3DFMT_DXT3) == D3DERR_NOTAVAILABLE)
	{
		ms_bSupportDXT = false;
	}

	if (ms_lpd3d->CheckDeviceFormat(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		ms_d3dPresentParameter.BackBufferFormat,
		0,
		D3DRTYPE_TEXTURE,
		D3DFMT_DXT5) == D3DERR_NOTAVAILABLE)
	{
		ms_bSupportDXT = false;
	}

	if (FAILED((ms_hLastResult = ms_lpd3dDevice->GetDeviceCaps(&ms_d3dCaps))))
	{
		Tracenf("IDirect3DDevice.GetDeviceCaps - ERROR %d", ms_hLastResult);
		return CREATE_GET_DEVICE_CAPS2;
	}

	if (!Windowed)
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, iHres, iVres, SWP_SHOWWINDOW);

	//Tracef("vertex shader version : %X\n",(uint32_t)ms_d3dCaps.VertexShaderVersion);

	ms_lpd3dDevice->GetViewport(&ms_Viewport);

	m_pStateManager = new CStateManager(ms_lpd3dDevice);

	D3DXCreateMatrixStack(0, &ms_lpd3dMatStack);
	ms_lpd3dMatStack->LoadIdentity();

	ms_ptVS = CreatePTStreamVertexShader();
	ms_pntVS = CreatePNTStreamVertexShader();
	ms_pnt2VS = CreatePNT2StreamVertexShader();

	D3DXMatrixIdentity(&ms_matIdentity);
	D3DXMatrixIdentity(&ms_matView);
	D3DXMatrixIdentity(&ms_matProj);
	D3DXMatrixIdentity(&ms_matInverseView);
	D3DXMatrixIdentity(&ms_matInverseViewYAxis);
	D3DXMatrixIdentity(&ms_matScreen0);
	D3DXMatrixIdentity(&ms_matScreen1);
	D3DXMatrixIdentity(&ms_matScreen2);

	ms_matScreen0._11 = 1;
	ms_matScreen0._22 = -1;

	ms_matScreen1._41 = 1;
	ms_matScreen1._42 = 1;

	ms_matScreen2._11 = (float)iHres / 2;
	ms_matScreen2._22 = (float)iVres / 2;

	D3DXCreateSphere(ms_lpd3dDevice, 1.0f, 32, 32, &ms_lpSphereMesh, nullptr);
	D3DXCreateCylinder(ms_lpd3dDevice, 1.0f, 1.0f, 1.0f, 8, 8, &ms_lpCylinderMesh, nullptr);

	ms_lpd3dDevice->Clear(0L, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0);

	if (!__CreateDefaultIndexBufferList())
		return false;

	if (!__CreatePDTVertexBufferList())
		return false;

	uint32_t dwTexMemSize = GetAvailableTextureMemory();

	if (dwTexMemSize < 64 * 1024 * 1024)
		ms_isLowTextureMemory = true;
	else
		ms_isLowTextureMemory = false;

	if (dwTexMemSize > 100 * 1024 * 1024)
		ms_isHighTextureMemory = true;
	else
		ms_isHighTextureMemory = false;

	if (ms_d3dCaps.TextureAddressCaps & D3DPTADDRESSCAPS_BORDER)
		GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;
	else
		GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = true;

	//D3DADAPTER_IDENTIFIER9& rkD3DAdapterId=pkD3DAdapterInfo->GetIdentifier();
	if (strnicmp(rkD3DAdapterId.Driver, "SIS", 3) == 0)
	{
		GRAPHICS_CAPS_CAN_NOT_DRAW_LINE = true;
		GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW = true;
		GRAPHICS_CAPS_HALF_SIZE_IMAGE = true;
		ms_isLowTextureMemory = true;
	}
	else if (strnicmp(rkD3DAdapterId.Driver, "3dfx", 4) == 0)
	{
		GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW = true;
		GRAPHICS_CAPS_HALF_SIZE_IMAGE = true;
		ms_isLowTextureMemory = true;
	}

	int cpuInfo[4] = { 0 };

	__cpuid(cpuInfo, 1);
	CPU_HAS_SSE2 = cpuInfo[3] & (1 << 26);

	return (iRet);
}

void CGraphicDevice::__InitializePDTVertexBufferList()
{
#ifdef ENABLE_TEXT_RENEWAL
	m_smallPdtVertexBuffer = nullptr;
	m_largePdtVertexBuffer = nullptr;
#else
	for (UINT i = 0; i < PDT_VERTEXBUFFER_NUM; ++i)
		ms_alpd3dPDTVB[i] = NULL;
#endif
}

void CGraphicDevice::__DestroyPDTVertexBufferList()
{
#ifdef ENABLE_TEXT_RENEWAL
	if (m_smallPdtVertexBuffer)
	{
		m_smallPdtVertexBuffer->Release();
		m_smallPdtVertexBuffer = nullptr;
	}

	if (m_largePdtVertexBuffer)
	{
		m_largePdtVertexBuffer->Release();
		m_largePdtVertexBuffer = nullptr;
	}
#else
	for (UINT i = 0; i < PDT_VERTEXBUFFER_NUM; ++i)
	{
		if (ms_alpd3dPDTVB[i])
		{
			ms_alpd3dPDTVB[i]->Release();
			ms_alpd3dPDTVB[i] = NULL;
		}
	}
#endif
}

bool CGraphicDevice::__CreatePDTVertexBufferList()
{
#ifdef ENABLE_TEXT_RENEWAL
	auto res = ms_lpd3dDevice->CreateVertexBuffer(sizeof(TPDTVertex) * SMALL_PDT_VERTEX_BUFFER_SIZE,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
		TPDTVertex::kFVF,
		D3DPOOL_DEFAULT,
		&m_smallPdtVertexBuffer, nullptr);


	if (FAILED(res))
		return false;

	res = ms_lpd3dDevice->CreateVertexBuffer(sizeof(TPDTVertex) * LARGE_PDT_VERTEX_BUFFER_SIZE,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
		TPDTVertex::kFVF,
		D3DPOOL_DEFAULT,
		&m_largePdtVertexBuffer, nullptr);

	if (FAILED(res))
		return false;

#else
	for (UINT i = 0; i < PDT_VERTEXBUFFER_NUM; ++i)
	{
		if (FAILED(
			ms_lpd3dDevice->CreateVertexBuffer(
				sizeof(TPDTVertex) * PDT_VERTEX_NUM,
				D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
				D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1,
				D3DPOOL_SYSTEMMEM,
				&ms_alpd3dPDTVB[i], NULL)
		))
			return false;
	}
#endif
	return true;
}

void CGraphicDevice::__InitializeDefaultIndexBufferList()
{
	for (UINT i = 0; i < DEFAULT_IB_NUM; ++i)
		ms_alpd3dDefIB[i] = nullptr;
}

void CGraphicDevice::__DestroyDefaultIndexBufferList()
{
	for (UINT i = 0; i < DEFAULT_IB_NUM; ++i)
		if (ms_alpd3dDefIB[i])
		{
			ms_alpd3dDefIB[i]->Release();
			ms_alpd3dDefIB[i] = nullptr;
		}
}

bool CGraphicDevice::__CreateDefaultIndexBuffer(UINT eDefIB, UINT uIdxCount, const WORD * c_awIndices)
{
	assert(ms_alpd3dDefIB[eDefIB] == NULL);

	if (FAILED(
		ms_lpd3dDevice->CreateIndexBuffer(
			sizeof(WORD) * uIdxCount,
			D3DUSAGE_WRITEONLY,
			D3DFMT_INDEX16,
			D3DPOOL_MANAGED,
			&ms_alpd3dDefIB[eDefIB],
			nullptr)
	)) return false;

	WORD* dstIndices;
	if (FAILED(
		ms_alpd3dDefIB[eDefIB]->Lock(0, 0, (void**)&dstIndices, 0)
	)) return false;

	memcpy(dstIndices, c_awIndices, sizeof(WORD) * uIdxCount);

	ms_alpd3dDefIB[eDefIB]->Unlock();

	return true;
}

bool CGraphicDevice::__CreateDefaultIndexBufferList()
{
	static const WORD c_awLineIndices[2] = { 0, 1, };
	static const WORD c_awLineTriIndices[6] = { 0, 1, 0, 2, 1, 2, };
	static const WORD c_awLineRectIndices[8] = { 0, 1, 0, 2, 1, 3, 2, 3, };
	static const WORD c_awLineCubeIndices[24] = {
		0, 1, 0, 2, 1, 3, 2, 3,
		0, 4, 1, 5, 2, 6, 3, 7,
		4, 5, 4, 6, 5, 7, 6, 7,
	};
	static const WORD c_awFillTriIndices[3] = { 0, 1, 2, };
	static const WORD c_awFillRectIndices[6] = { 0, 2, 1, 2, 3, 1, };
	static const WORD c_awFillCubeIndices[36] = {
		0, 1, 2, 1, 3, 2,
		2, 0, 6, 0, 4, 6,
		0, 1, 4, 1, 5, 4,
		1, 3, 5, 3, 7, 5,
		3, 2, 7, 2, 6, 7,
		4, 5, 6, 5, 7, 6,
	};

	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE, 2, c_awLineIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_TRI, 6, c_awLineTriIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_RECT, 8, c_awLineRectIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_CUBE, 24, c_awLineCubeIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_TRI, 3, c_awFillTriIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_RECT, 6, c_awFillRectIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_CUBE, 36, c_awFillCubeIndices))
		return false;

	return true;
}

void CGraphicDevice::InitBackBufferCount(UINT uBackBufferCount)
{
	m_uBackBufferCount = uBackBufferCount;
}

void CGraphicDevice::Destroy()
{

	__DestroyPDTVertexBufferList();
	__DestroyDefaultIndexBufferList();

	if (ms_hDC)
	{
		ReleaseDC(ms_hWnd, ms_hDC);
		ms_hDC = nullptr;
	}

	if (ms_ptVS)
	{
		ms_ptVS->Release();
		//ms_lpd3dDevice->DeleteVertexShader(ms_ptVS);
		ms_ptVS = nullptr;
	}

	if (ms_pntVS)
	{
		ms_pntVS->Release();
		//ms_lpd3dDevice->DeleteVertexShader(ms_pntVS);
		ms_pntVS = nullptr;
	}

	if (ms_pnt2VS)
	{
		ms_pnt2VS->Release();
		//ms_lpd3dDevice->DeleteVertexShader(ms_pnt2VS);
		ms_pnt2VS = nullptr;
	}

	safe_release(ms_lpSphereMesh);
	safe_release(ms_lpCylinderMesh);

	safe_release(ms_lpd3dMatStack);
	safe_release(ms_lpd3dDevice);
	safe_release(ms_lpd3d);

	if (m_pStateManager)
	{
		delete m_pStateManager;
		m_pStateManager = nullptr;
	}

	__Initialize();
}
