#include "StdAfx.h"
#include "StateManager.h"

//#define StateManager_Assert(a) if (!(a)) puts("assert"#a)
#define StateManager_Assert(a) assert(a)

struct SLightData
{
	enum
	{
		LIGHT_NUM = 8,
	};
	D3DLIGHT9 m_akD3DLight[LIGHT_NUM];
} m_kLightData;



void CStateManager::SetLight(uint32_t index, CONST D3DLIGHT9* pLight)
{
	assert(index < SLightData::LIGHT_NUM);
	m_kLightData.m_akD3DLight[index] = *pLight;

	m_lpD3DDev->SetLight(index, pLight);
}

void CStateManager::GetLight(uint32_t index, D3DLIGHT9* pLight)
{
	assert(index < 8);
	*pLight = m_kLightData.m_akD3DLight[index];
}

bool CStateManager::BeginScene()
{
	m_bScene = true;

	D3DXMATRIX m4Proj;
	D3DXMATRIX m4View;
	D3DXMATRIX m4World;
	GetTransform(D3DTS_WORLD, &m4World);
	GetTransform(D3DTS_PROJECTION, &m4Proj);
	GetTransform(D3DTS_VIEW, &m4View);
	SetTransform(D3DTS_WORLD, &m4World);
	SetTransform(D3DTS_PROJECTION, &m4Proj);
	SetTransform(D3DTS_VIEW, &m4View);

	if (FAILED(m_lpD3DDev->BeginScene()))
		return false;
	return true;
}

void CStateManager::EndScene()
{
	m_lpD3DDev->EndScene();
	m_bScene = false;
}

CStateManager::CStateManager(LPDIRECT3DDEVICE9 lpDevice) : m_lpD3DDev(nullptr)
{
	m_bScene = false;
	m_dwBestMinFilter = D3DTEXF_LINEAR;
	m_dwBestMagFilter = D3DTEXF_LINEAR;
	SetDevice(lpDevice);
}

CStateManager::~CStateManager()
{
	if (m_lpD3DDev)
	{
		m_lpD3DDev->Release();
		m_lpD3DDev = nullptr;
	}
}

void CStateManager::SetDevice(LPDIRECT3DDEVICE9 lpDevice)
{
	StateManager_Assert(lpDevice);
	lpDevice->AddRef();

	if (m_lpD3DDev)
	{
		m_lpD3DDev->Release();
		m_lpD3DDev = nullptr;
	}

	m_lpD3DDev = lpDevice;

	D3DCAPS9 d3dCaps;
	m_lpD3DDev->GetDeviceCaps(&d3dCaps);

	if (d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC)
		m_dwBestMagFilter = D3DTEXF_ANISOTROPIC;
	else
		m_dwBestMagFilter = D3DTEXF_LINEAR;

	if (d3dCaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC)
		m_dwBestMinFilter = D3DTEXF_ANISOTROPIC;
	else
		m_dwBestMinFilter = D3DTEXF_LINEAR;

	uint32_t dwMax = d3dCaps.MaxAnisotropy;
	dwMax = dwMax < 4 ? dwMax : 4;

	for (int i = 0; i < 8; ++i)
		m_lpD3DDev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, dwMax);

	SetDefaultState();
}

void CStateManager::SetBestFiltering(uint32_t dwStage)
{
	SetSamplerState(dwStage, D3DSAMP_MINFILTER, m_dwBestMinFilter);
	SetSamplerState(dwStage, D3DSAMP_MAGFILTER, m_dwBestMagFilter);
	SetSamplerState(dwStage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
}

void CStateManager::Restore()
{
	int i, j;

	m_bForce = true;

	for (i = 0; i < STATEMANAGER_MAX_RENDERSTATES; ++i)
		SetRenderState(D3DRENDERSTATETYPE(i), m_CurrentState.m_RenderStates[i]);

	for (i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
		for (j = 0; j < STATEMANAGER_MAX_TEXTURESTATES; ++j)
			SetTextureStageState(i, D3DTEXTURESTAGESTATETYPE(j), m_CurrentState.m_TextureStates[i][j]);

	for (i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
		SetTexture(i, m_CurrentState.m_Textures[i]);

	m_bForce = false;
}

void CStateManager::SetDefaultState()
{
	m_CurrentState.ResetState();
	m_CopyState.ResetState();
	m_ChipState.ResetState();

	m_bScene = false;
	m_bForce = true;

	D3DXMATRIX Identity;
	D3DXMatrixIdentity(&Identity);

	SetTransform(D3DTS_WORLD, &Identity);
	SetTransform(D3DTS_VIEW, &Identity);
	SetTransform(D3DTS_PROJECTION, &Identity);

	D3DMATERIAL9 DefaultMat;
	ZeroMemory(&DefaultMat, sizeof(D3DMATERIAL9));

	DefaultMat.Diffuse.r = 1.0f;
	DefaultMat.Diffuse.g = 1.0f;
	DefaultMat.Diffuse.b = 1.0f;
	DefaultMat.Diffuse.a = 1.0f;
	DefaultMat.Ambient.r = 1.0f;
	DefaultMat.Ambient.g = 1.0f;
	DefaultMat.Ambient.b = 1.0f;
	DefaultMat.Ambient.a = 1.0f;
	DefaultMat.Emissive.r = 0.0f;
	DefaultMat.Emissive.g = 0.0f;
	DefaultMat.Emissive.b = 0.0f;
	DefaultMat.Emissive.a = 0.0f;
	DefaultMat.Specular.r = 0.0f;
	DefaultMat.Specular.g = 0.0f;
	DefaultMat.Specular.b = 0.0f;
	DefaultMat.Specular.a = 0.0f;
	DefaultMat.Power = 0.0f;

	SetMaterial(&DefaultMat);

	SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);

	//SetRenderState(D3DRS_LINEPATTERN, 0xFFFFFFFF);
	SetRenderState(D3DRS_LASTPIXEL, false);
	SetRenderState(D3DRS_ALPHAREF, 1);
	SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	//	SetRenderState(D3DRS_ZVISIBLE, false);
	SetRenderState(D3DRS_FOGSTART, 0);
	SetRenderState(D3DRS_FOGEND, 0);
	SetRenderState(D3DRS_FOGDENSITY, 0);
	//SetRenderState(D3DRS_EDGEANTIALIAS, false);
//	SetRenderState(D3DRS_ZBIAS, 0);
	SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	SetRenderState(D3DRS_AMBIENT, 0x00000000);
	SetRenderState(D3DRS_LOCALVIEWER, false);
	SetRenderState(D3DRS_NORMALIZENORMALS, false);
	SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	//SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING, false);
	SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
	SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
	SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, false);
	SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
	SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	SetRenderState(D3DRS_FOGENABLE, false);
	SetRenderState(D3DRS_FOGCOLOR, 0xFF000000);
	SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	SetRenderState(D3DRS_RANGEFOGENABLE, false);
	SetRenderState(D3DRS_ZENABLE, true);
	SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	SetRenderState(D3DRS_ZWRITEENABLE, true);
	SetRenderState(D3DRS_DITHERENABLE, true);
	SetRenderState(D3DRS_STENCILENABLE, false);
	SetRenderState(D3DRS_ALPHATESTENABLE, false);
	SetRenderState(D3DRS_CLIPPING, true);
	SetRenderState(D3DRS_LIGHTING, false);
	SetRenderState(D3DRS_SPECULARENABLE, false);
	SetRenderState(D3DRS_COLORVERTEX, false);
	SetRenderState(D3DRS_WRAP0, 0);
	SetRenderState(D3DRS_WRAP1, 0);
	SetRenderState(D3DRS_WRAP2, 0);
	SetRenderState(D3DRS_WRAP3, 0);
	SetRenderState(D3DRS_WRAP4, 0);
	SetRenderState(D3DRS_WRAP5, 0);
	SetRenderState(D3DRS_WRAP6, 0);
	SetRenderState(D3DRS_WRAP7, 0);

	SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
	SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(2, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(2, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(3, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(3, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(3, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(3, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(3, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(4, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(4, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(4, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(4, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(4, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(4, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(5, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(5, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(5, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(5, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(5, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(5, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(6, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(6, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(6, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(6, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(6, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(6, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(7, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(7, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(7, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(7, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(7, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(7, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
	SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2);
	SetTextureStageState(3, D3DTSS_TEXCOORDINDEX, 3);
	SetTextureStageState(4, D3DTSS_TEXCOORDINDEX, 4);
	SetTextureStageState(5, D3DTSS_TEXCOORDINDEX, 5);
	SetTextureStageState(6, D3DTSS_TEXCOORDINDEX, 6);
	SetTextureStageState(7, D3DTSS_TEXCOORDINDEX, 7);

	SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(4, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(5, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(6, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(6, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(6, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(7, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(7, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
	SetSamplerState(7, D3DSAMP_MIPFILTER, D3DTEXF_ANISOTROPIC);

	SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(5, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(5, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(6, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(6, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(7, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(7, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(3, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(4, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(5, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(6, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(7, D3DTSS_TEXTURETRANSFORMFLAGS, 0);

	SetTexture(0, nullptr);
	SetTexture(1, nullptr);
	SetTexture(2, nullptr);
	SetTexture(3, nullptr);
	SetTexture(4, nullptr);
	SetTexture(5, nullptr);
	SetTexture(6, nullptr);
	SetTexture(7, nullptr);

	SetPixelShader(nullptr);
	SetFVF(D3DFVF_XYZ);

	D3DXVECTOR4 av4Null[STATEMANAGER_MAX_VCONSTANTS];
	memset(av4Null, 0, sizeof(av4Null));
	SetVertexShaderConstant(0, av4Null, STATEMANAGER_MAX_VCONSTANTS);
	SetPixelShaderConstant(0, av4Null, STATEMANAGER_MAX_PCONSTANTS);

	m_bForce = false;

	//#ifdef _DEBUG
	//	int i, j;
	//	for (i = 0; i < STATEMANAGER_MAX_RENDERSTATES; i++)
	//		m_bRenderStateSavingFlag[i] = false;
	//
	//	for (j = 0; j < STATEMANAGER_MAX_TRANSFORMSTATES; j++)
	//		m_bTransformSavingFlag[j] = false;
	//
	//	for (j = 0; j < STATEMANAGER_MAX_STAGES; ++j)
	//		for (i = 0; i < STATEMANAGER_MAX_TEXTURESTATES; ++i)
	//			m_bTextureStageStateSavingFlag[j][i] = false;
	//#endif _DEBUG
}

// Material
void CStateManager::SaveMaterial()
{
	m_CopyState.m_D3DMaterial = m_CurrentState.m_D3DMaterial;
}

void CStateManager::SaveMaterial(const D3DMATERIAL9* pMaterial)
{
	// Check that we have set this up before, if not, the default is this.
	m_CopyState.m_D3DMaterial = m_CurrentState.m_D3DMaterial;
	SetMaterial(pMaterial);
}

void CStateManager::RestoreMaterial()
{
	SetMaterial(&m_CopyState.m_D3DMaterial);
}

void CStateManager::SetMaterial(const D3DMATERIAL9* pMaterial)
{
	m_lpD3DDev->SetMaterial(pMaterial);
	m_CurrentState.m_D3DMaterial = *pMaterial;
}

void CStateManager::GetMaterial(D3DMATERIAL9* pMaterial) const
{
	// Set the renderstate and remember it.
	*pMaterial = m_CurrentState.m_D3DMaterial;
}

// Renderstates
uint32_t CStateManager::GetRenderState(D3DRENDERSTATETYPE Type) const
{
	return m_CurrentState.m_RenderStates[Type];
}

void CStateManager::SaveRenderState(D3DRENDERSTATETYPE Type, uint32_t dwValue)
{
	//#ifdef _DEBUG
	//	if (m_bRenderStateSavingFlag[Type])
	//	{
	//		Tracef(" CStateManager::SaveRenderState - This render state is already saved [%d, %d]\n", Type, dwValue);
	//		StateManager_Assert(!" This render state is already saved!");
	//	}
	//	m_bRenderStateSavingFlag[Type] = true;
	//#endif _DEBUG

		// Check that we have set this up before, if not, the default is this.
	m_CopyState.m_RenderStates[Type] = m_CurrentState.m_RenderStates[Type];
	SetRenderState(Type, dwValue);
}

void CStateManager::RestoreRenderState(D3DRENDERSTATETYPE Type)
{
	//#ifdef _DEBUG
	//	if (!m_bRenderStateSavingFlag[Type])
	//	{
	//		Tracef(" CStateManager::SaveRenderState - This render state was not saved [%d, %d]\n", Type);
	//		StateManager_Assert(!" This render state was not saved!");
	//	}
	//	m_bRenderStateSavingFlag[Type] = false;
	//#endif _DEBUG

	SetRenderState(Type, m_CopyState.m_RenderStates[Type]);
}

void CStateManager::SetRenderState(D3DRENDERSTATETYPE Type, uint32_t Value)
{
	if (!m_bForce && m_CurrentState.m_RenderStates[Type] == Value)
		return;

	m_lpD3DDev->SetRenderState(Type, Value);
	m_CurrentState.m_RenderStates[Type] = Value;
}

void CStateManager::GetRenderState(D3DRENDERSTATETYPE Type, uint32_t* pdwValue) const
{
	*pdwValue = m_CurrentState.m_RenderStates[Type];
}

// Textures
void CStateManager::SaveTexture(uint32_t dwStage, LPDIRECT3DBASETEXTURE9 pTexture)
{
	// Check that we have set this up before, if not, the default is this.
	if (m_CopyState.m_Textures[dwStage])
	{
		m_CopyState.m_Textures[dwStage]->Release();
		m_CopyState.m_Textures[dwStage] = nullptr;
	}

	// Check that we have set this up before, if not, the default is this.
	m_CopyState.m_Textures[dwStage] = m_CurrentState.m_Textures[dwStage];
	if (m_CopyState.m_Textures[dwStage])
		m_CopyState.m_Textures[dwStage]->AddRef();
	SetTexture(dwStage, pTexture);
}

void CStateManager::RestoreTexture(uint32_t dwStage)
{
	SetTexture(dwStage, m_CopyState.m_Textures[dwStage]);
	if (m_CopyState.m_Textures[dwStage])
	{
		m_CopyState.m_Textures[dwStage]->Release();
		m_CopyState.m_Textures[dwStage] = nullptr;
	}
}

void CStateManager::SetTexture(uint32_t dwStage, LPDIRECT3DBASETEXTURE9 pTexture)
{
	if (m_CurrentState.m_Textures[dwStage] == pTexture)
		return;

	if (pTexture)
		pTexture->AddRef();

	if (m_CurrentState.m_Textures[dwStage])
		m_CurrentState.m_Textures[dwStage]->Release();

	m_lpD3DDev->SetTexture(dwStage, pTexture);
	m_CurrentState.m_Textures[dwStage] = pTexture;
}

void CStateManager::GetTexture(uint32_t dwStage, LPDIRECT3DBASETEXTURE9* ppTexture) const
{
	*ppTexture = m_CurrentState.m_Textures[dwStage];
}

// Texture stage states
void CStateManager::SaveTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type, uint32_t dwValue)
{
	// Check that we have set this up before, if not, the default is this.
//#ifdef _DEBUG
//	if (m_bTextureStageStateSavingFlag[dwStage][Type])
//	{
//		Tracef(" CStateManager::SaveTextureStageState - This texture stage state is already saved [%d, %d]\n", dwStage, Type);
//		StateManager_Assert(!" This texture stage state is already saved!");
//	}
//	m_bTextureStageStateSavingFlag[dwStage][Type] = true;
//#endif _DEBUG
	m_CopyState.m_TextureStates[dwStage][Type] = m_CurrentState.m_TextureStates[dwStage][Type];
	SetTextureStageState(dwStage, Type, dwValue);
}

void CStateManager::RestoreTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type)
{
	//#ifdef _DEBUG
	//	if (!m_bTextureStageStateSavingFlag[dwStage][Type])
	//	{
	//		Tracef(" CStateManager::RestoreTextureStageState - This texture stage state was not saved [%d, %d]\n", dwStage, Type);
	//		StateManager_Assert(!" This texture stage state was not saved!");
	//	}
	//	m_bTextureStageStateSavingFlag[dwStage][Type] = false;
	//#endif _DEBUG
	SetTextureStageState(dwStage, Type, m_CopyState.m_TextureStates[dwStage][Type]);
}

void CStateManager::SetTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type, uint32_t dwValue)
{
	if (m_CurrentState.m_TextureStates[dwStage][Type] == dwValue)
		return;

	m_lpD3DDev->SetTextureStageState(dwStage, Type, dwValue);
	m_CurrentState.m_TextureStates[dwStage][Type] = dwValue;
}

void CStateManager::GetTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type, uint32_t* pdwValue) const
{
	*pdwValue = m_CurrentState.m_TextureStates[dwStage][Type];
}


// Sampler states
void CStateManager::SaveSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type, uint32_t dwValue)
{
	//#ifdef _DEBUG
	//	if (m_bSamplerStateSavingFlag[dwStage][Type])
	//	{
	//		Tracef(" CStateManager::SaveTextureStageState - This texture stage state is already saved [%d, %d]\n", dwStage, Type);
	//		StateManager_Assert(!" This texture stage state is already saved!");
	//	}
	//	m_bSamplerStateSavingFlag[dwStage][Type] = true;
	//#endif _DEBUG
	m_CopyState.m_SamplerStates[dwStage][Type] = m_CurrentState.m_SamplerStates[dwStage][Type];
	SetSamplerState(dwStage, Type, dwValue);
}
void CStateManager::RestoreSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type)
{
	//#ifdef _DEBUG
	//	if (!m_bSamplerStateSavingFlag[dwStage][Type])
	//	{
	//		Tracef(" CStateManager::RestoreTextureStageState - This texture stage state was not saved [%d, %d]\n", dwStage, Type);
	//		StateManager_Assert(!" This texture stage state was not saved!");
	//	}
	//	m_bSamplerStateSavingFlag[dwStage][Type] = false;
	//#endif _DEBUG
	SetSamplerState(dwStage, Type, m_CopyState.m_SamplerStates[dwStage][Type]);
}
void CStateManager::SetSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type, uint32_t dwValue)
{
	if (m_CurrentState.m_SamplerStates[dwStage][Type] == dwValue)
		return;
	m_lpD3DDev->SetSamplerState(dwStage, Type, dwValue);
	m_CurrentState.m_SamplerStates[dwStage][Type] = dwValue;
}
void CStateManager::GetSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type, uint32_t* pdwValue) const
{
	*pdwValue = m_CurrentState.m_SamplerStates[dwStage][Type];
}


// Vertex Shader
void CStateManager::SaveVertexShader(LPDIRECT3DVERTEXSHADER9 dwShader)
{
	m_CopyState.m_dwVertexShader = m_CurrentState.m_dwVertexShader;
	SetVertexShader(dwShader);
}

void CStateManager::RestoreVertexShader()
{
	SetVertexShader(m_CopyState.m_dwVertexShader);
}

void CStateManager::SetVertexShader(LPDIRECT3DVERTEXSHADER9 dwShader)
{
	//if (m_CurrentState.m_dwVertexShader == dwShader)
	//	return;

	m_lpD3DDev->SetVertexShader(dwShader);
	m_CurrentState.m_dwVertexShader = dwShader;
}

void CStateManager::GetVertexShader(LPDIRECT3DVERTEXSHADER9* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwVertexShader;
}

// Vertex Processing
void CStateManager::SaveVertexProcessing(bool IsON)
{
	if (m_CurrentState.m_bVertexProcessing == IsON)
		return;
	m_CopyState.m_bVertexProcessing = m_CurrentState.m_bVertexProcessing;
	m_lpD3DDev->SetSoftwareVertexProcessing(IsON);
	m_CurrentState.m_bVertexProcessing = IsON;
}
void CStateManager::RestoreVertexProcessing()
{
	if (m_CopyState.m_bVertexProcessing == m_CurrentState.m_bVertexProcessing)
		return;
	m_lpD3DDev->SetSoftwareVertexProcessing(m_CopyState.m_bVertexProcessing);
}
// Vertex Declaration
void CStateManager::SaveVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 dwShader)
{
	m_CopyState.m_dwVertexDeclaration = m_CurrentState.m_dwVertexDeclaration;
	SetVertexDeclaration(dwShader);
}
void CStateManager::RestoreVertexDeclaration()
{
	SetVertexDeclaration(m_CopyState.m_dwVertexDeclaration);
}
void CStateManager::SetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 dwShader)
{
	//if (m_CurrentState.m_dwVertexDeclaration == dwShader)
	//	return;
	m_lpD3DDev->SetVertexDeclaration(dwShader);
	m_CurrentState.m_dwVertexDeclaration = dwShader;
}
void CStateManager::GetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwVertexDeclaration;
}
// FVF
void CStateManager::SaveFVF(uint32_t dwShader)
{
	m_CopyState.m_dwFVF = m_CurrentState.m_dwFVF;
	SetFVF(dwShader);
}
void CStateManager::RestoreFVF()
{
	SetFVF(m_CopyState.m_dwFVF);
}
void CStateManager::SetFVF(uint32_t dwShader)
{
	//if (m_CurrentState.m_dwFVF == dwShader)
	//	return;
	m_lpD3DDev->SetFVF(dwShader);
	m_CurrentState.m_dwFVF = dwShader;
}
void CStateManager::GetFVF(uint32_t* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwFVF;
}
// Pixel Shader
void CStateManager::SavePixelShader(LPDIRECT3DPIXELSHADER9 dwShader)
{
	m_CopyState.m_dwPixelShader = m_CurrentState.m_dwPixelShader;
	SetPixelShader(dwShader);
}

void CStateManager::RestorePixelShader()
{
	SetPixelShader(m_CopyState.m_dwPixelShader);
}

void CStateManager::SetPixelShader(LPDIRECT3DPIXELSHADER9 dwShader)
{
	if (m_CurrentState.m_dwPixelShader == dwShader)
		return;

	m_lpD3DDev->SetPixelShader(dwShader);
	m_CurrentState.m_dwPixelShader = dwShader;
}

void CStateManager::GetPixelShader(LPDIRECT3DPIXELSHADER9* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwPixelShader;
}

// *** These states are cached, but not protected from multiple sends of the same value.
// Transform
void CStateManager::SaveTransform(D3DTRANSFORMSTATETYPE Type, const D3DMATRIX* pMatrix)
{
	//#ifdef _DEBUG
	//	if (m_bTransformSavingFlag[Type])
	//	{
	//		Tracef(" CStateManager::SaveTransform - This transform is already saved [%d]\n", Type);
	//		StateManager_Assert(!" This trasform is already saved!");
	//	}
	//	m_bTransformSavingFlag[Type] = true;
	//#endif _DEBUG

	m_CopyState.m_Matrices[Type] = m_CurrentState.m_Matrices[Type];
	SetTransform(Type, (D3DXMATRIX*)pMatrix);
}

void CStateManager::RestoreTransform(D3DTRANSFORMSTATETYPE Type)
{
	//#ifdef _DEBUG
	//	if (!m_bTransformSavingFlag[Type])
	//	{
	//		Tracef(" CStateManager::RestoreTransform - This transform was not saved [%d]\n", Type);
	//		StateManager_Assert(!" This render state was not saved!");
	//	}
	//	m_bTransformSavingFlag[Type] = false;
	//#endif _DEBUG

	SetTransform(Type, &m_CopyState.m_Matrices[Type]);
}

// Don't cache-check the transform.  To much to do
void CStateManager::SetTransform(D3DTRANSFORMSTATETYPE Type, const D3DMATRIX* pMatrix)
{
	if (m_bScene)
	{
		m_lpD3DDev->SetTransform(Type, pMatrix);
	}
	else
	{
		assert(D3DTS_VIEW == Type || D3DTS_PROJECTION == Type || D3DTS_WORLD == Type);
	}

	m_CurrentState.m_Matrices[Type] = *pMatrix;
}

void CStateManager::GetTransform(D3DTRANSFORMSTATETYPE Type, D3DMATRIX* pMatrix)
{
	*pMatrix = m_CurrentState.m_Matrices[Type];
}

// SetVertexShaderConstant
void CStateManager::SaveVertexShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount)
{
	uint32_t i;

	for (i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_VCONSTANTS);
		m_CopyState.m_VertexShaderConstants[dwRegister + i] = m_CurrentState.m_VertexShaderConstants[dwRegister + i];
	}

	SetVertexShaderConstant(dwRegister, pConstantData, dwConstantCount);
}

void CStateManager::RestoreVertexShaderConstant(uint32_t dwRegister, uint32_t dwConstantCount)
{
	SetVertexShaderConstant(dwRegister, &m_CopyState.m_VertexShaderConstants[dwRegister], dwConstantCount);
}

void CStateManager::SetVertexShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount)
{
	m_lpD3DDev->SetVertexShaderConstantF(dwRegister, static_cast<const float*>(pConstantData), dwConstantCount);

	// Set the renderstate and remember it.
	for (uint32_t i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_VCONSTANTS);
		m_CurrentState.m_VertexShaderConstants[dwRegister + i] = *(((D3DXVECTOR4*)pConstantData) + i);
	}
}

// SetPixelShaderConstant
void CStateManager::SavePixelShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount)
{
	uint32_t i;

	for (i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_VCONSTANTS);
		m_CopyState.m_PixelShaderConstants[dwRegister + i] = *(((D3DXVECTOR4*)pConstantData) + i);
	}

	SetPixelShaderConstant(dwRegister, pConstantData, dwConstantCount);
}

void CStateManager::RestorePixelShaderConstant(uint32_t dwRegister, uint32_t dwConstantCount)
{
	SetPixelShaderConstant(dwRegister, &m_CopyState.m_PixelShaderConstants[dwRegister], dwConstantCount);
}

void CStateManager::SetPixelShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount)
{
	m_lpD3DDev->SetPixelShaderConstantF(dwRegister, static_cast<const float*>(pConstantData), dwConstantCount);

	// Set the renderstate and remember it.
	for (uint32_t i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_VCONSTANTS);
		m_CurrentState.m_PixelShaderConstants[dwRegister + i] = *(((D3DXVECTOR4*)pConstantData) + i);
	}
}

void CStateManager::SaveStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER9 pStreamData, UINT Stride)
{
	// Check that we have set this up before, if not, the default is this.
	m_CopyState.m_StreamData[StreamNumber] = m_CurrentState.m_StreamData[StreamNumber];
	SetStreamSource(StreamNumber, pStreamData, Stride);
}

void CStateManager::RestoreStreamSource(UINT StreamNumber)
{
	SetStreamSource(StreamNumber,
		m_CopyState.m_StreamData[StreamNumber].m_lpStreamData,
		m_CopyState.m_StreamData[StreamNumber].m_Stride);
}

void CStateManager::SetStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER9 pStreamData, UINT Stride)
{
	CStreamData kStreamData(pStreamData, Stride);
	if (m_CurrentState.m_StreamData[StreamNumber] == kStreamData)
		return;

	m_lpD3DDev->SetStreamSource(StreamNumber, pStreamData, 0, Stride);
	m_CurrentState.m_StreamData[StreamNumber] = kStreamData;
}

void CStateManager::SaveIndices(LPDIRECT3DINDEXBUFFER9 pIndexData, UINT BaseVertexIndex)
{
	m_CopyState.m_IndexData = m_CurrentState.m_IndexData;
	SetIndices(pIndexData, BaseVertexIndex);
}

void CStateManager::RestoreIndices()
{
	SetIndices(m_CopyState.m_IndexData.m_lpIndexData, m_CopyState.m_IndexData.m_BaseVertexIndex);
}

void CStateManager::SetIndices(LPDIRECT3DINDEXBUFFER9 pIndexData, UINT BaseVertexIndex)
{
	CIndexData kIndexData(pIndexData, BaseVertexIndex);

	if (m_CurrentState.m_IndexData == kIndexData)
		return;

	m_lpD3DDev->SetIndices(pIndexData);
	m_CurrentState.m_IndexData = kIndexData;
}

HRESULT CStateManager::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	return (m_lpD3DDev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount));
}

HRESULT CStateManager::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	m_CurrentState.m_StreamData[0] = nullptr;
	return (m_lpD3DDev->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride));
}

HRESULT CStateManager::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
	return (m_lpD3DDev->DrawIndexedPrimitive(PrimitiveType, static_cast<INT>(minIndex), minIndex, NumVertices, startIndex, primCount));
}

HRESULT CStateManager::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	m_CurrentState.m_IndexData = nullptr;
	m_CurrentState.m_StreamData[0] = nullptr;
	return (m_lpD3DDev->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride));
}
