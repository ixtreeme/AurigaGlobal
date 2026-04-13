/******************************************************************************

  Copyright (C) 1999, 2000 NVIDIA Corporation

  This file is provided without support, instruction, or implied warranty of any
  kind.  NVIDIA makes no guarantee of its fitness for a particular purpose and is
  not liable under any circumstances for any damages or loss whatsoever arising
  from the use or inability to use this file or items derived from it.

	Comments:

	  A simple class to manage rendering state.  Created as a singleton.
	  Create it as a static global, or with new.  It doesn't matter as long as it is created
	  before you use the CStateManager::GetSingleton() API to get a reference to it.

	  Call it with STATEMANAGER.SetRenderState(...)
	  Call it with STATEMANAGER.SetTextureStageState(...), etc.

	  Call the 'Save' versions of the function if you want to deviate from the current state.
	  Call the 'Restore' version to retrieve the last Save.

	  There are two levels of caching:
	  - All Sets/Saves/Restores are tracked for redundancy.  This reduces the size of the batch to
	  be flushed
	  - The flush function is called before rendering, and only copies state that is
	  different from the current chip state.

  If you get an assert it is probably because an API call failed.

  See NVLink for a good example of how this class is used.

  Don't be afraid of the vector being used to track the flush batch.  It will grow as big as
  it needs to be and then stop, so it shouldn't be reallocated.

  The state manager holds a reference to the d3d device.

  - cmaughan@nvidia.com

******************************************************************************/

#pragma once

#include <directxsdk/d3d9.h>
#include <directxsdk/d3dx9.h>

#include <vector>

#include "../Base/Singleton.h"

#define CHECK_D3DAPI(a)		\
{							\
	HRESULT hr = (a);		\
							\
	if (hr != S_OK)			\
		assert(!#a);		\
}

static constexpr uint32_t STATEMANAGER_MAX_RENDERSTATES = 256;
static constexpr uint32_t STATEMANAGER_MAX_TEXTURESTATES = 128;
static constexpr uint32_t STATEMANAGER_MAX_STAGES = 8;
static constexpr uint32_t STATEMANAGER_MAX_VCONSTANTS = 96;
static constexpr uint32_t STATEMANAGER_MAX_PCONSTANTS = 8;
static constexpr uint32_t STATEMANAGER_MAX_TRANSFORMSTATES = 300;	// World1 lives way up there...
static constexpr uint32_t STATEMANAGER_MAX_STREAMS = 16;

class CStreamData
{
public:
	CStreamData(LPDIRECT3DVERTEXBUFFER9 pStreamData = nullptr, UINT Stride = 0) : m_lpStreamData(pStreamData), m_Stride(Stride)
	{
	}

	bool operator == (const CStreamData& rhs) const
	{
		return ((m_lpStreamData == rhs.m_lpStreamData) && (m_Stride == rhs.m_Stride));
	}

	LPDIRECT3DVERTEXBUFFER9	m_lpStreamData;
	UINT					m_Stride;
};

class CIndexData
{
public:
	CIndexData(LPDIRECT3DINDEXBUFFER9 pIndexData = nullptr, UINT BaseVertexIndex = 0)
		: m_lpIndexData(pIndexData),
		m_BaseVertexIndex(BaseVertexIndex)
	{
	}

	bool operator == (const CIndexData& rhs) const
	{
		return ((m_lpIndexData == rhs.m_lpIndexData) && (m_BaseVertexIndex == rhs.m_BaseVertexIndex));
	}

	LPDIRECT3DINDEXBUFFER9	m_lpIndexData;
	UINT					m_BaseVertexIndex;
};

// State types managed by the class
typedef enum eStateType
{
	STATE_MATERIAL = 0,
	STATE_RENDER,
	STATE_TEXTURE,
	STATE_TEXTURESTAGE,
	STATE_VSHADER,
	STATE_PSHADER,
	STATE_TRANSFORM,
	STATE_VCONSTANT,
	STATE_PCONSTANT,
	STATE_STREAM,
	STATE_INDEX
} eStateType;

class CStateID
{
public:
	CStateID(eStateType Type, uint32_t dwValue0 = 0, uint32_t dwValue1 = 0)
		: m_Type(Type),
		m_dwValue0(dwValue0),
		m_dwValue1(dwValue1)
	{
	}

	CStateID(eStateType Type, uint32_t dwStage, D3DTEXTURESTAGESTATETYPE StageType) : m_Type(Type), m_dwStage(dwStage), m_TextureStageStateType(StageType) {}

	CStateID(eStateType Type, D3DRENDERSTATETYPE RenderType) : m_Type(Type), m_RenderStateType(RenderType) {}

	eStateType m_Type;

	union
	{
		uint32_t					m_dwValue0;
		uint32_t					m_dwStage;
		D3DRENDERSTATETYPE		m_RenderStateType;
		D3DTRANSFORMSTATETYPE	m_TransformStateType;
	};

	union
	{
		uint32_t						m_dwValue1;
		D3DTEXTURESTAGESTATETYPE	m_TextureStageStateType;
	};
};

typedef std::vector<CStateID> TStateID;

class CStateManagerState
{
public:
	CStateManagerState()
		= default;

	void ResetState()
	{
		uint32_t i, y;

		for (i = 0; i < STATEMANAGER_MAX_RENDERSTATES; i++)
			m_RenderStates[i] = 0x7FFFFFFF;

		for (i = 0; i < STATEMANAGER_MAX_STAGES; i++)
			for (y = 0; y < STATEMANAGER_MAX_TEXTURESTATES; y++)
				m_TextureStates[i][y] = 0x7FFFFFFF;

		for (i = 0; i < STATEMANAGER_MAX_STREAMS; i++)
			m_StreamData[i] = CStreamData();

		m_IndexData = CIndexData();

		for (i = 0; i < STATEMANAGER_MAX_STAGES; i++)
			m_Textures[i] = nullptr;

		// Matrices and constants are not cached, just restored.  It's silly to check all the
		// data elements (by which time the driver could have been sent them).
		for (i = 0; i < STATEMANAGER_MAX_TRANSFORMSTATES; i++)
			D3DXMatrixIdentity(&m_Matrices[i]);

		for (i = 0; i < STATEMANAGER_MAX_VCONSTANTS; i++)
			m_VertexShaderConstants[i] = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);

		for (i = 0; i < STATEMANAGER_MAX_PCONSTANTS; i++)
			m_PixelShaderConstants[i] = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);

		m_dwPixelShader = nullptr;
		m_dwVertexShader = nullptr;
		m_dwVertexDeclaration = nullptr;
		m_dwFVF = D3DFVF_XYZ;
		m_bVertexProcessing = false;

		ZeroMemory(&m_Matrices, sizeof(D3DXMATRIX) * STATEMANAGER_MAX_TRANSFORMSTATES);
	}

	// Renderstates
	uint32_t					m_RenderStates[STATEMANAGER_MAX_RENDERSTATES];

	// Texture stage states
	uint32_t					m_TextureStates[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];
	uint32_t					m_SamplerStates[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];

	// Vertex shader constants
	D3DXVECTOR4				m_VertexShaderConstants[STATEMANAGER_MAX_VCONSTANTS];

	// Pixel shader constants
	D3DXVECTOR4				m_PixelShaderConstants[STATEMANAGER_MAX_PCONSTANTS];

	// Textures
	LPDIRECT3DBASETEXTURE9	m_Textures[STATEMANAGER_MAX_STAGES];

	// Shaders
	LPDIRECT3DPIXELSHADER9					m_dwPixelShader;
	LPDIRECT3DVERTEXSHADER9					m_dwVertexShader;
	LPDIRECT3DVERTEXDECLARATION9 m_dwVertexDeclaration;
	uint32_t					m_dwFVF;

	D3DXMATRIX				m_Matrices[STATEMANAGER_MAX_TRANSFORMSTATES];

	D3DMATERIAL9			m_D3DMaterial;

	CStreamData				m_StreamData[STATEMANAGER_MAX_STREAMS];
	CIndexData				m_IndexData;
	bool					m_bVertexProcessing;
};

class CStateManager : public CSingleton<CStateManager>
{
public:
	CStateManager(LPDIRECT3DDEVICE9 lpDevice);
	~CStateManager() override;

	void	SetDefaultState();
	void	Restore();

	bool	BeginScene();
	void	EndScene();

	// Material
	void	SaveMaterial();
	void	SaveMaterial(const D3DMATERIAL9* pMaterial);
	void	RestoreMaterial();
	void	SetMaterial(const D3DMATERIAL9* pMaterial);
	void	GetMaterial(D3DMATERIAL9* pMaterial) const;

	void	SetLight(uint32_t index, CONST D3DLIGHT9* pLight);
	void	GetLight(uint32_t index, D3DLIGHT9* pLight);

	// Renderstates
	void	SaveRenderState(D3DRENDERSTATETYPE Type, uint32_t dwValue);
	void	RestoreRenderState(D3DRENDERSTATETYPE Type);
	void	SetRenderState(D3DRENDERSTATETYPE Type, uint32_t Value);
	void	GetRenderState(D3DRENDERSTATETYPE Type, uint32_t* pdwValue) const;

	// Textures
	void	SaveTexture(uint32_t dwStage, LPDIRECT3DBASETEXTURE9 pTexture);
	void	RestoreTexture(uint32_t dwStage);
	void	SetTexture(uint32_t dwStage, LPDIRECT3DBASETEXTURE9 pTexture);
	void	GetTexture(uint32_t dwStage, LPDIRECT3DBASETEXTURE9* ppTexture) const;

	// Texture stage states
	void	SaveTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type, uint32_t dwValue);
	void	RestoreTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type);
	void	SetTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type, uint32_t dwValue);
	void	GetTextureStageState(uint32_t dwStage, D3DTEXTURESTAGESTATETYPE Type, uint32_t* pdwValue) const;
	void	SetBestFiltering(uint32_t dwStage); // if possible set anisotropy filtering, or use trilinear



	// Sampler states
	void	SaveSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type, uint32_t dwValue);
	void	RestoreSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type);
	void	SetSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type, uint32_t dwValue);
	void	GetSamplerState(uint32_t dwStage, D3DSAMPLERSTATETYPE Type, uint32_t* pdwValue) const;

	// Vertex Shader
	void	SaveVertexShader(LPDIRECT3DVERTEXSHADER9 dwShader);
	void	RestoreVertexShader();
	void	SetVertexShader(LPDIRECT3DVERTEXSHADER9 dwShader);
	void	GetVertexShader(LPDIRECT3DVERTEXSHADER9* pdwShader);

	// Vertex Declaration
	void	SaveVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 dwShader);
	void	RestoreVertexDeclaration();
	void	SetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 dwShader);
	void	GetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9* pdwShader);

	// FVF
	void	SaveFVF(uint32_t dwShader);
	void	RestoreFVF();
	void	SetFVF(uint32_t dwShader);
	void	GetFVF(uint32_t* pdwShader);

	// Pixel Shader
	void	SavePixelShader(LPDIRECT3DPIXELSHADER9 dwShader);
	void	RestorePixelShader();
	void	SetPixelShader(LPDIRECT3DPIXELSHADER9 dwShader);
	void	GetPixelShader(LPDIRECT3DPIXELSHADER9* pdwShader);

	// *** These states are cached, but not protected from multiple sends of the same value.
	// Transform
	void SaveTransform(D3DTRANSFORMSTATETYPE Transform, const D3DMATRIX* pMatrix);
	void RestoreTransform(D3DTRANSFORMSTATETYPE Transform);

	// Vertex Processing
	void SaveVertexProcessing(bool IsON);
	void RestoreVertexProcessing();

	// Don't cache-check the transform.  To much to do
	void SetTransform(D3DTRANSFORMSTATETYPE Type, const D3DMATRIX* pMatrix);
	void GetTransform(D3DTRANSFORMSTATETYPE Type, D3DMATRIX* pMatrix);

	// SetVertexShaderConstant
	void SaveVertexShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount);
	void RestoreVertexShaderConstant(uint32_t dwRegister, uint32_t dwConstantCount);
	void SetVertexShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount);

	// SetPixelShaderConstant
	void SavePixelShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount);
	void RestorePixelShaderConstant(uint32_t dwRegister, uint32_t dwConstantCount);
	void SetPixelShaderConstant(uint32_t dwRegister, CONST void* pConstantData, uint32_t dwConstantCount);

	void SaveStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER9 pStreamData, UINT Stride);
	void RestoreStreamSource(UINT StreamNumber);
	void SetStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER9 pStreamData, UINT Stride);

	void SaveIndices(LPDIRECT3DINDEXBUFFER9 pIndexData, UINT BaseVertexIndex);
	void RestoreIndices();
	void SetIndices(LPDIRECT3DINDEXBUFFER9 pIndexData, UINT BaseVertexIndex);

	HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
	HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount);
	HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);

	// Codes For Debug
	uint32_t GetRenderState(D3DRENDERSTATETYPE Type) const;

private:
	void SetDevice(LPDIRECT3DDEVICE9 lpDevice);

private:
	CStateManagerState	m_ChipState;
	CStateManagerState	m_CurrentState;
	CStateManagerState	m_CopyState;
	TStateID			m_DirtyStates;
	bool				m_bForce;
	bool				m_bScene;
	uint32_t				m_dwBestMinFilter;
	uint32_t				m_dwBestMagFilter;
	LPDIRECT3DDEVICE9	m_lpD3DDev;

#ifdef _DEBUG
	// Saving Flag
	bool				m_bRenderStateSavingFlag[STATEMANAGER_MAX_RENDERSTATES];
	bool				m_bTextureStageStateSavingFlag[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];
	bool				m_bSamplerStateSavingFlag[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];
	bool				m_bTransformSavingFlag[STATEMANAGER_MAX_TRANSFORMSTATES];
#endif _DEBUG
};

#define STATEMANAGER (CStateManager::Instance())
