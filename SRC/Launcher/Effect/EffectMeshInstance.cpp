#include "StdAfx.h"
#include "../Render/StateManager.h"
#include "../Render/ResourceManager.h"
#include "EffectMeshInstance.h"
#include "../Render/GrpMath.h"

CDynamicPool<CEffectMeshInstance>		CEffectMeshInstance::ms_kPool;

void CEffectMeshInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CEffectMeshInstance* CEffectMeshInstance::New()
{
	return ms_kPool.Alloc();
}

void CEffectMeshInstance::Delete(CEffectMeshInstance* pkMeshInstance)
{
	pkMeshInstance->Destroy();
	ms_kPool.Free(pkMeshInstance);
}

bool CEffectMeshInstance::isActive()
{
	if (!CEffectElementBaseInstance::isActive())
		return FALSE;

	if (!m_MeshFrameController.isActive())
		return FALSE;

	for (uint32_t j = 0; j < m_TextureInstanceVector.size(); ++j)
	{
		int iCurrentFrame = m_MeshFrameController.GetCurrentFrame();
		if (m_TextureInstanceVector[j].TextureFrameController.isActive(iCurrentFrame))
			return TRUE;
	}

	return FALSE;
}

bool CEffectMeshInstance::OnUpdate(float fElapsedTime)
{
	if (!isActive())
		return false;

	if (m_MeshFrameController.isActive())
		m_MeshFrameController.Update(fElapsedTime);

	for (uint32_t j = 0; j < m_TextureInstanceVector.size(); ++j)
	{
		int iCurrentFrame = m_MeshFrameController.GetCurrentFrame();
		if (m_TextureInstanceVector[j].TextureFrameController.isActive(iCurrentFrame))
			m_TextureInstanceVector[j].TextureFrameController.Update(fElapsedTime);
	}

	return true;
}
bool CreateVertexBuffers(CEffectMesh::TEffectFrameData& frameData)
{
	// Ha már létezik mindkettõ, nem hozunk létre újat
	if (frameData.pVB && frameData.pIB)
		return false;

	LPDIRECT3DDEVICE9 device = CGraphicBase::GetD3DDevice();
	if (!device)
		return false;

	const size_t vtxCount = frameData.PDTVertexVector.size();
	if (vtxCount == 0)
		return false;

	// Vertex buffer
	HRESULT hr = device->CreateVertexBuffer(
		static_cast<UINT>(sizeof(TPTVertex) * vtxCount),
		0,
		D3DFVF_XYZ | D3DFVF_TEX1,
		D3DPOOL_MANAGED,
		&frameData.pVB,
		nullptr
	);
	if (FAILED(hr) || !frameData.pVB)
		return false;

	void* pVertices = nullptr;
	hr = frameData.pVB->Lock(0, 0, &pVertices, 0);
	if (FAILED(hr) || !pVertices)
		return false;

	memcpy(pVertices, frameData.PDTVertexVector.data(), sizeof(TPTVertex) * vtxCount);
	frameData.pVB->Unlock();

	frameData.dwVertexCount = static_cast<DWORD>(vtxCount);

	// Indexek, ha még nem voltak (minimum: vertex count)
	if (frameData.IndexVector.empty())
	{
		frameData.dwIndexCount = frameData.dwVertexCount;
		frameData.IndexVector.resize(frameData.dwIndexCount);

		for (DWORD i = 0; i < frameData.dwIndexCount; ++i)
			frameData.IndexVector[i] = static_cast<WORD>(i);
	}

	const size_t idxCount = frameData.IndexVector.size();
	if (idxCount == 0)
		return false;

	// Index buffer
	hr = device->CreateIndexBuffer(
		static_cast<UINT>(sizeof(WORD) * idxCount),
		0,
		D3DFMT_INDEX16,
		D3DPOOL_MANAGED,
		&frameData.pIB,
		nullptr
	);
	if (FAILED(hr) || !frameData.pIB)
		return false;

	void* pIndices = nullptr;
	hr = frameData.pIB->Lock(0, 0, &pIndices, 0);
	if (FAILED(hr) || !pIndices)
		return false;

	memcpy(pIndices, frameData.IndexVector.data(), sizeof(WORD) * idxCount);
	frameData.pIB->Unlock();

	frameData.dwIndexCount = static_cast<DWORD>(idxCount);

	return true;
}



void CEffectMeshInstance::OnRender()
{
	if (!isActive())
		return;


	CEffectMesh* pEffectMesh = m_roMesh.GetPointer();

	for (uint32_t i = 0; i < pEffectMesh->GetMeshCount(); ++i)
	{
		assert(i < m_TextureInstanceVector.size());

		CFrameController& rTextureFrameController = m_TextureInstanceVector[i].TextureFrameController;
		if (!rTextureFrameController.isActive(m_MeshFrameController.GetCurrentFrame()))
			continue;

		int iBillboardType = m_pMeshScript->GetBillboardType(i);

		XMMATRIX m_matWorld;
		m_matWorld = XMMatrixIdentity();

		switch (iBillboardType)
		{
		case MESH_BILLBOARD_TYPE_ALL:
		{
			XMMATRIX matTemp;
			matTemp = XMMatrixRotationX(XMConvertToRadians(90.0f));
			auto detMat = XMMatrixDeterminant(matTemp);
			m_matWorld = XMMatrixInverse(&detMat, (XMMATRIX)CScreen::GetViewMatrix());

			m_matWorld = matTemp * m_matWorld;
		}
		break;

		case MESH_BILLBOARD_TYPE_Y:
		{
			auto view = (XMMATRIX)CScreen::GetViewMatrix();
			auto detMat = XMMatrixDeterminant(view);
			XMMATRIX matTemp = XMMatrixInverse(&detMat, view);


			m_matWorld.r[0].m128_f32[0] = matTemp.r[0].m128_f32[0];
			m_matWorld.r[0].m128_f32[1] = matTemp.r[0].m128_f32[1];
			m_matWorld.r[1].m128_f32[0] = matTemp.r[1].m128_f32[0];
			m_matWorld.r[1].m128_f32[1] = matTemp.r[1].m128_f32[1];
		}
		break;

		case MESH_BILLBOARD_TYPE_MOVE:
		{
			D3DXVECTOR3 Position;
			m_pMeshScript->GetPosition(m_fLocalTime, Position);
			D3DXVECTOR3 LastPosition;
			m_pMeshScript->GetPosition(m_fLocalTime - CTimer::Instance().GetElapsedSecond(), LastPosition);
			Position -= LastPosition;
			if (D3DXVec3LengthSq(&Position) > 0.001f)
			{
				D3DXVec3Normalize(&Position, &Position);
				D3DXQUATERNION q = SafeRotationNormalizedArc(D3DXVECTOR3(0.0f, -1.0f, 0.0f), Position);
				D3DXMatrixRotationQuaternion((D3DXMATRIX*)&m_matWorld, &q);
			}
		}
		break;
		}

		if (!m_pMeshScript->isBlendingEnable(i))
		{
			STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
		else
		{
			int iBlendingSrcType = m_pMeshScript->GetBlendingSrcType(i);
			int iBlendingDestType = m_pMeshScript->GetBlendingDestType(i);
			STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			STATEMANAGER.SetRenderState(D3DRS_SRCBLEND, iBlendingSrcType);
			STATEMANAGER.SetRenderState(D3DRS_DESTBLEND, iBlendingDestType);
		}

		D3DXVECTOR3 Position;
		m_pMeshScript->GetPosition(m_fLocalTime, Position);
		m_matWorld.r[3].m128_f32[0] = Position.x;
		m_matWorld.r[3].m128_f32[1] = Position.y;
		m_matWorld.r[3].m128_f32[2] = Position.z;
		m_matWorld = m_matWorld * (XMMATRIX)*mc_pmatLocal;
		STATEMANAGER.SetTransform(D3DTS_WORLD, (D3DXMATRIX*)&m_matWorld);

		uint8_t byType;
		D3DXCOLOR Color(1.0f, 1.0f, 1.0f, 1.0f);
		if (m_pMeshScript->GetColorOperationType(i, &byType))
			STATEMANAGER.SetTextureStageState(0, D3DTSS_COLOROP, byType);
		m_pMeshScript->GetColorFactor(i, &Color);

		TTimeEventTableFloat* TableAlpha;

		float fAlpha = 1.0f;
		if (m_pMeshScript->GetTimeTableAlphaPointer(i, &TableAlpha) && !TableAlpha->empty())
			fAlpha = GetTimeEventBlendValue(m_fLocalTime, *TableAlpha);

		// Render //
		CEffectMesh::TEffectMeshData* pMeshData = pEffectMesh->GetMeshDataPointer(i);

		assert(m_MeshFrameController.GetCurrentFrame() < pMeshData->EffectFrameDataVector.size());
		CEffectMesh::TEffectFrameData& rFrameData = pMeshData->EffectFrameDataVector[m_MeshFrameController.GetCurrentFrame()];
		CreateVertexBuffers(rFrameData);
		uint32_t dwcurTextureFrame = rTextureFrameController.GetCurrentFrame();
		if (dwcurTextureFrame < m_TextureInstanceVector[i].TextureInstanceVector.size())
		{
			CGraphicImageInstance* pImageInstance = m_TextureInstanceVector[i].TextureInstanceVector[dwcurTextureFrame];
			STATEMANAGER.SetTexture(0, pImageInstance->GetTexturePointer()->GetD3DTexture());
		}

		Color.a = fAlpha * rFrameData.fVisibility;

		STATEMANAGER.SetRenderState(D3DRS_TEXTUREFACTOR, uint32_t(Color));// brush textura bug fix @Razor93 2025.05.31
		STATEMANAGER.SetStreamSource(0, rFrameData.pVB, sizeof(TPTVertex));
		STATEMANAGER.SetIndices(rFrameData.pIB, 0);
		STATEMANAGER.SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);


		STATEMANAGER.DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST, 0, rFrameData.dwVertexCount,
			0, rFrameData.dwIndexCount / 3);
		//STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLELIST,rFrameData.dwIndexCount/3, rFrameData.PDTVertexVector.data(),sizeof(TPTVertex));
		// Render //
	}
}

void CEffectMeshInstance::OnSetDataPointer(CEffectElementBase* pElement)
{
	auto pMesh = static_cast<CEffectMeshScript*>(pElement);
	m_pMeshScript = pMesh;

	const char* c_szMeshFileName = pMesh->GetMeshFileName();

	m_pEffectMesh = static_cast<CEffectMesh*>(CResourceManager::Instance().GetResourcePointer(c_szMeshFileName));

	if (!m_pEffectMesh)
		return;

	m_roMesh.SetPointer(m_pEffectMesh);

	m_MeshFrameController.Clear();
	m_MeshFrameController.SetMaxFrame(m_roMesh.GetPointer()->GetFrameCount());
	m_MeshFrameController.SetFrameTime(pMesh->GetMeshAnimationFrameDelay());
	m_MeshFrameController.SetLoopFlag(pMesh->isMeshAnimationLoop());
	m_MeshFrameController.SetLoopCount(pMesh->GetMeshAnimationLoopCount());
	m_MeshFrameController.SetStartFrame(0);

	m_TextureInstanceVector.clear();
	m_TextureInstanceVector.resize(m_pEffectMesh->GetMeshCount());
	for (uint32_t j = 0; j < m_TextureInstanceVector.size(); ++j)
	{
		CEffectMeshScript::TMeshData* pMeshData;
		if (!m_pMeshScript->GetMeshDataPointer(j, &pMeshData))
			continue;

		CEffectMesh* pkEftMesh = m_roMesh.GetPointer();

		if (!pkEftMesh)
			continue;

		std::vector<CGraphicImage*>* pTextureVector = pkEftMesh->GetTextureVectorPointer(j);
		if (!pTextureVector)
			continue;

		std::vector<CGraphicImage*>& rTextureVector = *pTextureVector;

		CFrameController& rFrameController = m_TextureInstanceVector[j].TextureFrameController;
		rFrameController.Clear();
		rFrameController.SetMaxFrame(rTextureVector.size());
		rFrameController.SetFrameTime(pMeshData->fTextureAnimationFrameDelay);
		rFrameController.SetLoopFlag(pMeshData->bTextureAnimationLoopEnable);
		rFrameController.SetStartFrame(pMeshData->dwTextureAnimationStartFrame);

		std::vector<CGraphicImageInstance*>& rImageInstanceVector = m_TextureInstanceVector[j].TextureInstanceVector;
		rImageInstanceVector.clear();
		rImageInstanceVector.reserve(rTextureVector.size());
		for (const auto pImage : rTextureVector)
		{
			CGraphicImageInstance* pImageInstance = CGraphicImageInstance::ms_kPool.Alloc();
			pImageInstance->SetImagePointer(pImage);
			rImageInstanceVector.emplace_back(pImageInstance); //push_back
		}
	}
}

void CEffectMeshInstance_DeleteImageInstance(CGraphicImageInstance* pkInstance)
{
	CGraphicImageInstance::ms_kPool.Free(pkInstance);
}

void CEffectMeshInstance_DeleteTextureInstance(CEffectMeshInstance::TTextureInstance& rkInstance)
{
	std::vector<CGraphicImageInstance*>& rVector = rkInstance.TextureInstanceVector;
	for (auto& imageInstance : rVector)
	{
		CEffectMeshInstance_DeleteImageInstance(imageInstance);
	}
	rVector.clear();
}

void CEffectMeshInstance::OnInitialize()
{
}

void CEffectMeshInstance::OnDestroy()
{
	for (auto& textureInstance : m_TextureInstanceVector)
	{
		CEffectMeshInstance_DeleteTextureInstance(textureInstance);
	}
	m_TextureInstanceVector.clear();
	m_roMesh.SetPointer(nullptr);
}

CEffectMeshInstance::CEffectMeshInstance() : m_pEffectMesh(nullptr), m_pMeshScript(nullptr)
{
	Initialize();
}

CEffectMeshInstance::~CEffectMeshInstance()
{
	Destroy();
}