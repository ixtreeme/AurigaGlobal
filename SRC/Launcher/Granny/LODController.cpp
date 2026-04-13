#include "StdAfx.h"
#include "LODController.h"

float LODHEIGHT_ACTOR = 500.0f;
float LODDISTANCE_ACTOR = 5000.0f;
float LODDISTANCE_BUILDING = 25000.0f;

static constexpr float c_fNearLodScale = 3.0f;
static constexpr float c_fFarLodScale = 25.0f;
static constexpr float LOD_APPLY_MAX = 2000.0f;
static constexpr float LOD_APPLY_MIN = 500.0f;

bool ms_isMinLODModeEnable = false;

enum
{
	SHARED_VB_500 = 0,
	SHARED_VB_1000 = 1,
	SHARED_VB_1500 = 2,
	SHARED_VB_2000 = 3,
	SHARED_VB_2500 = 4,
	SHARED_VB_3000 = 5,
	SHARED_VB_3500 = 6,
	SHARED_VB_4000 = 7,
	SHARED_VB_NUM = 9,
};

std::vector<CGraphicVertexBuffer*> gs_vbs[SHARED_VB_NUM];

CGraphicVertexBuffer gs_emptyVB;

#include <ctime>

static CGraphicVertexBuffer* __AllocDeformVertexBuffer(const unsigned deformableVertexCount)
{
	if (deformableVertexCount == 0)
		return &gs_emptyVB;

	const unsigned capacity = (((deformableVertexCount - 1) / 500) + 1) * 500;
	if (const unsigned index = (deformableVertexCount - 1) / 500; index < SHARED_VB_NUM)
	{
		std::vector<CGraphicVertexBuffer*>& vbs = gs_vbs[index];
		if (!vbs.empty())
		{
			CGraphicVertexBuffer* pkRetVB = vbs.back();
			vbs.pop_back();
			return pkRetVB;
		}
	}

	static uint64_t base = time(nullptr);

	const auto pkNewVB = new CGraphicVertexBuffer;

	if (!pkNewVB->Create(
		capacity,
		D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
		D3DUSAGE_WRITEONLY,
		D3DPOOL_MANAGED))
	{
		TraceError("NEW_ERROR %8d: %d(%d)", time(nullptr) - base, capacity, deformableVertexCount);
	}

	return pkNewVB;
}

void __FreeDeformVertexBuffer(CGraphicVertexBuffer* pkDelVB)
{
	if (pkDelVB)
	{
		if (pkDelVB == &gs_emptyVB)
			return;

		const unsigned index = (pkDelVB->GetVertexCount() - 1) / 500;
		if (index < SHARED_VB_NUM)
		{
			gs_vbs[index].emplace_back(pkDelVB);
		}
		else
		{
			pkDelVB->Destroy();
			delete pkDelVB;
		}
	}
}

void __ReserveSharedVertexBuffers(unsigned index, unsigned count)
{

	if (index >= SHARED_VB_NUM)
		return;

	unsigned capacity = (index + 1) * 500;

	for (unsigned i = 0; i != count; ++i)
	{
		auto pkNewVB = new CGraphicVertexBuffer;
		pkNewVB->Create(
			capacity,
			D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
			D3DUSAGE_WRITEONLY,
			D3DPOOL_MANAGED);
		gs_vbs[index].emplace_back(pkNewVB);
	}

}

void GrannyCreateSharedDeformBuffer()
{
	__ReserveSharedVertexBuffers(SHARED_VB_500, 40);
	__ReserveSharedVertexBuffers(SHARED_VB_1000, 20);
	__ReserveSharedVertexBuffers(SHARED_VB_1500, 20);
	__ReserveSharedVertexBuffers(SHARED_VB_2000, 40);
	__ReserveSharedVertexBuffers(SHARED_VB_3000, 20);
}

void GrannyDestroySharedDeformBuffer()
{
#ifdef _DEBUG
	TraceError("granny_shared_vbs:");
#endif
	for (auto& vbs : gs_vbs)
	{
#ifdef _DEBUG
		TraceError("\t%d: %d", vbs.size());
#endif

		//	std::vector<CGraphicVertexBuffer*>::iterator v;
		for (const auto pkEachVB : vbs)
		{
			pkEachVB->Destroy();
			delete pkEachVB;
		}
		vbs.clear();
	}
}

void CGrannyLODController::SetMinLODMode(const bool isEnable)
{
	ms_isMinLODModeEnable = isEnable;
}

void CGrannyLODController::SetMaterialImagePointer(const char* c_szImageName, CGraphicImage* pImage) const
{
	for (const auto pkModelInst : m_que_pkModelInst)
	{
		pkModelInst->SetMaterialImagePointer(c_szImageName, pImage);
	}
}

void CGrannyLODController::SetMaterialData(const char* c_szImageName, const SMaterialData& c_rkMaterialData) const
{
	for (const auto pkModelInst : m_que_pkModelInst)
	{
		pkModelInst->SetMaterialData(c_szImageName, c_rkMaterialData);
	}
}

void CGrannyLODController::SetSpecularInfo(const char* c_szMtrlName, const BOOL bEnable, const float fPower) const
{
	for (const auto pkModelInst : m_que_pkModelInst)
	{
		pkModelInst->SetSpecularInfo(c_szMtrlName, bEnable, fPower);
	}
}

CGrannyLODController::CGrannyLODController() :
	m_fLODDistance(0.0f),
	m_dwLODAniFPS(CGrannyModelInstance::ANIFPS_MAX),
	m_pAttachedParentModel(nullptr),
	m_bLODLevel(0),
	m_pCurrentModelInstance(nullptr),
	m_pkSharedDeformableVertexBuffer(nullptr)
{
}

CGrannyLODController::~CGrannyLODController()
{
	__FreeDeformVertexBuffer(m_pkSharedDeformableVertexBuffer);

	Clear();
}

void CGrannyLODController::Clear()
{
	if (m_pAttachedParentModel)
	{
		m_pAttachedParentModel->DetachModelInstance(this);
	}

	m_pCurrentModelInstance = nullptr;
	m_pAttachedParentModel = nullptr;

	for (auto& modelInstance : m_que_pkModelInst)
	{
		CGrannyModelInstance::Delete(modelInstance);
	}
	m_que_pkModelInst.clear();

	for (auto& rData : m_AttachedModelDataVector)
	{
		rData.pkLODController->m_pAttachedParentModel = nullptr;
	}
	m_AttachedModelDataVector.clear();
}

void CGrannyLODController::AddModel(CGraphicThing* pThing, const int iSrcModel, CGrannyLODController* pSkelLODController)
{
	if (!pThing)
		return;

	if (pSkelLODController && pSkelLODController->m_que_pkModelInst.empty())
	{
		//	assert(!"EMPTY SKELETON(CANNON LINK)");
		return;
	}

	assert(pThing->GetReferenceCount() >= 1);

	pThing->AddReference();

	if (pThing->GetModelCount() <= iSrcModel)
	{
		pThing->Release();
		return;
	}
	CGrannyModel* pModel = pThing->GetModelPointer(iSrcModel);
	if (!pModel)
	{
		pThing->Release();
		return;
	}

	CGrannyModelInstance* pModelInstance = CGrannyModelInstance::New();

	__ReserveSharedDeformableVertexBuffer(pModel->GetDeformVertexCount());

	if (pSkelLODController)
	{
		pModelInstance->SetLinkedModelPointer(pModel, m_pkSharedDeformableVertexBuffer,
			&pSkelLODController->m_pCurrentModelInstance);
	}
	else
	{
		pModelInstance->SetLinkedModelPointer(pModel, m_pkSharedDeformableVertexBuffer, nullptr);
	}

	if (!m_pCurrentModelInstance)
	{
		m_pCurrentModelInstance = pModelInstance;
		pModelInstance->DeformNoSkin(&ms_matIdentity);

		D3DXVECTOR3 vtMin, vtMax;
		pModelInstance->GetBoundBox(&vtMin, &vtMax);

		float fSize = 0.0f;
		fSize = fMAX(fSize, fabs(vtMin.x - vtMax.x));
		fSize = fMAX(fSize, fabs(vtMin.y - vtMax.y));
		fSize = fMAX(fSize, fabs(vtMin.z - vtMax.z));

		if (fSize < LODHEIGHT_ACTOR)
			SetLODLimits(0.0f, LODDISTANCE_ACTOR);
		else
			SetLODLimits(0.0f, LODDISTANCE_BUILDING);
	}
	else
	{
		pModelInstance->DeformNoSkin(&ms_matIdentity);
	}

	pThing->Release();

	m_que_pkModelInst.push_front(pModelInstance);
}

void CGrannyLODController::__ReserveSharedDeformableVertexBuffer(const uint32_t deformableVertexCount)
{
	if (m_pkSharedDeformableVertexBuffer &&
		m_pkSharedDeformableVertexBuffer->GetVertexCount() >= deformableVertexCount)
		return;

	__FreeDeformVertexBuffer(m_pkSharedDeformableVertexBuffer);

	m_pkSharedDeformableVertexBuffer = __AllocDeformVertexBuffer(deformableVertexCount);
}

void CGrannyLODController::AttachModelInstance(CGrannyLODController* pSrcLODController, const char* c_szBoneName)
{
	CGrannyModelInstance* pSrcInstance = pSrcLODController->GetModelInstance();
	if (!pSrcInstance)
		return;

	if (const CGrannyModelInstance* pDestInstance = GetModelInstance())
	{
		pSrcInstance->SetParentModelInstance(pDestInstance, c_szBoneName);
	}

	if (!pSrcLODController->GetModelInstance())
		return;

	pSrcLODController->m_pAttachedParentModel = this;

	for (auto itor = m_AttachedModelDataVector.begin(); m_AttachedModelDataVector.end() != itor;)
	{
		if (const auto& [pkLODController, strBoneName] = *itor; pSrcLODController == pkLODController)
		{
			itor = m_AttachedModelDataVector.erase(itor);
		}
		else
		{
			++itor;
		}
	}

	TAttachingModelData AttachingModelData;
	AttachingModelData.pkLODController = pSrcLODController;
	AttachingModelData.strBoneName = c_szBoneName;
	m_AttachedModelDataVector.emplace_back(AttachingModelData);
}

void CGrannyLODController::DetachModelInstance(CGrannyLODController* pSrcLODController)
{
	CGrannyModelInstance* pSrcInstance = pSrcLODController->GetModelInstance();
	if (!pSrcInstance)
		return;

	if (GetModelInstance())
	{
		pSrcInstance->SetParentModelInstance(nullptr, 0);
	}

	for (auto itor = m_AttachedModelDataVector.begin(); m_AttachedModelDataVector.end() != itor;)
	{
		const TAttachingModelData& rData = *itor;
		if (pSrcLODController == rData.pkLODController)
		{
			itor = m_AttachedModelDataVector.erase(itor);
		}
		else
		{
			++itor;
		}
	}
	pSrcLODController->m_pAttachedParentModel = nullptr;
}

void CGrannyLODController::SetLODLimits(float, const float fFarLOD)
{
	m_fLODDistance = fFarLOD;
}

void CGrannyLODController::SetLODLevel(const uint64_t bLodLevel)
{
	assert(m_que_pkModelInst.size() > 0);

	if (!m_que_pkModelInst.empty())
		m_bLODLevel = static_cast<int>(MIN(m_que_pkModelInst.size() - 1, bLodLevel));
}

void CGrannyLODController::CreateDeviceObjects()
{
	std::ranges::for_each(m_que_pkModelInst, CGrannyModelInstance::FCreateDeviceObjects());
}

void CGrannyLODController::DestroyDeviceObjects()
{
	std::ranges::for_each(m_que_pkModelInst, CGrannyModelInstance::FDestroyDeviceObjects());
}

void CGrannyLODController::RenderWithOneTexture() const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->RenderWithOneTexture();
}

void CGrannyLODController::BlendRenderWithOneTexture() const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->BlendRenderWithOneTexture();
}

void CGrannyLODController::RenderWithTwoTexture() const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->RenderWithTwoTexture();
}

void CGrannyLODController::BlendRenderWithTwoTexture() const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->BlendRenderWithTwoTexture();
}

void CGrannyLODController::Update(const float fElapsedTime, const float fDistanceFromCenter, const float fDistanceFromCamera)
{
	UpdateLODLevel(fDistanceFromCenter, fDistanceFromCamera);
	UpdateTime(fElapsedTime);
}

void CGrannyLODController::UpdateLODLevel(const float fDistanceFromCenter, const float fDistanceFromCamera)
{
	if (m_que_pkModelInst.size() <= 1)
		return;

	assert(m_pCurrentModelInstance != NULL);

	if (fDistanceFromCenter > LOD_APPLY_MIN)
	{
		const float fLODFactor = fMINMAX(0.0f, (m_fLODDistance - fDistanceFromCamera), m_fLODDistance);

		if (m_fLODDistance > 0.0f)
			m_dwLODAniFPS = static_cast<uint32_t>((CGrannyModelInstance::ANIFPS_MAX - CGrannyModelInstance::ANIFPS_MIN) *
				static_cast<uint32_t>(fLODFactor) /
				static_cast<uint32_t>(m_fLODDistance) +
				CGrannyModelInstance::ANIFPS_MIN);
		else
			m_dwLODAniFPS = CGrannyModelInstance::ANIFPS_MIN;

		assert(m_dwLODAniFPS > 0);
		m_dwLODAniFPS /= 10;
		m_dwLODAniFPS *= 10;

		const float fLODStep = m_fLODDistance / m_que_pkModelInst.size();
		auto bLODLevel = static_cast<uint8_t>(fLODFactor / fLODStep);

		if (m_fLODDistance <= 5000.0f)
		{
			if (fDistanceFromCamera < 500.0f)
			{
				bLODLevel = 0;
			}
			else if (fDistanceFromCamera < 1500.0f)
			{
				bLODLevel = 1;
			}
			else if (fDistanceFromCamera < 2500.0f)
			{
				bLODLevel = 2;
			}
			else
			{
				bLODLevel = 3;
			}

			bLODLevel = static_cast<uint8_t>(m_que_pkModelInst.size() - min(bLODLevel, m_que_pkModelInst.size()) - 1);
		}

		if (ms_isMinLODModeEnable)
			bLODLevel = 0;

		SetLODLevel(bLODLevel);

		if (m_pCurrentModelInstance != m_que_pkModelInst[m_bLODLevel])
		{
			SetCurrentModelInstance(m_que_pkModelInst[m_bLODLevel]);
		}
	}
	else
	{
		m_dwLODAniFPS = CGrannyModelInstance::ANIFPS_MAX;

		if (!m_que_pkModelInst.empty())
		{
			if (m_pCurrentModelInstance != m_que_pkModelInst.back())
			{
				SetCurrentModelInstance(m_que_pkModelInst.back());
			}
		}
	}
}

void CGrannyLODController::UpdateTime(const float fElapsedTime) const
{
	assert(m_pCurrentModelInstance != NULL);

	m_pCurrentModelInstance->Update(m_dwLODAniFPS);
	m_pCurrentModelInstance->UpdateLocalTime(fElapsedTime);
}

void CGrannyLODController::SetCurrentModelInstance(CGrannyModelInstance* pgrnModelInstance)
{
	pgrnModelInstance->CopyMotion(m_pCurrentModelInstance, true);
	m_pCurrentModelInstance = pgrnModelInstance;
	RefreshAttachedModelInstance();

	if (m_pAttachedParentModel)
	{
		m_pAttachedParentModel->RefreshAttachedModelInstance();
	}
}

void CGrannyLODController::RefreshAttachedModelInstance()
{
	if (!m_pCurrentModelInstance)
		return;

	for (uint32_t i = 0; i < m_AttachedModelDataVector.size(); ++i)
	{
		TAttachingModelData& rModelData = m_AttachedModelDataVector[i];

		CGrannyModelInstance* pSrcInstance = rModelData.pkLODController->GetModelInstance();
		if (!pSrcInstance)
		{
			Tracenf(
				"CGrannyLODController::RefreshAttachedModelInstance : m_AttachedModelDataVector[%d]->pkLODController->GetModelIntance()==NULL",
				i);
			continue;
		}

		pSrcInstance->SetParentModelInstance(m_pCurrentModelInstance, rModelData.strBoneName.c_str());
	}
}

void CGrannyLODController::UpdateSkeleton(const D3DXMATRIX* c_pWorldMatrix, const float fElapsedTime) const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->UpdateSkeleton(c_pWorldMatrix, fElapsedTime);
}

void CGrannyLODController::DeformAll(const D3DXMATRIX* c_pWorldMatrix) const
{
	for (const auto pkModelInst : m_que_pkModelInst)
	{
		pkModelInst->Deform(c_pWorldMatrix);
	}
}

void CGrannyLODController::DeformNoSkin(const D3DXMATRIX* c_pWorldMatrix) const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->DeformNoSkin(c_pWorldMatrix);
}

void CGrannyLODController::Deform(const D3DXMATRIX* c_pWorldMatrix) const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->Deform(c_pWorldMatrix);
}

void CGrannyLODController::RenderToShadowMap() const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->RenderWithoutTexture();
}

void CGrannyLODController::RenderShadow() const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->RenderWithOneTexture();
}

void CGrannyLODController::ReloadTexture() const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->ReloadTexture();
}

void CGrannyLODController::GetBoundBox(D3DXVECTOR3* vtMin, D3DXVECTOR3* vtMax) const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->GetBoundBox(vtMin, vtMax);
}

bool CGrannyLODController::Intersect(const D3DXMATRIX* c_pMatrix, float* u, float* v, float* t) const
{
	if (!m_pCurrentModelInstance)
		return false;
	return m_pCurrentModelInstance->Intersect(c_pMatrix, u, v, t);
}

void CGrannyLODController::SetLocalTime(const float fLocalTime) const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->SetLocalTime(fLocalTime);
}

void CGrannyLODController::ResetLocalTime() const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->ResetLocalTime();
}

void CGrannyLODController::SetMotionPointer(const std::shared_ptr<CGrannyMotion> c_pMotion, const float fBlendTime, const int iLoopCount,
	const float speedRatio) const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->SetMotionPointer(c_pMotion, fBlendTime, iLoopCount, speedRatio);
}

void CGrannyLODController::ChangeMotionPointer(const std::shared_ptr<CGrannyMotion> c_pMotion, const int iLoopCount, const float speedRatio) const
{
	assert(m_pCurrentModelInstance != NULL);
	m_pCurrentModelInstance->ChangeMotionPointer(c_pMotion, iLoopCount, speedRatio);
}

void CGrannyLODController::SetMotionAtEnd() const
{
	if (m_pCurrentModelInstance)
		m_pCurrentModelInstance->SetMotionAtEnd();
}

BOOL CGrannyLODController::isModelInstance() const
{
	if (!m_pCurrentModelInstance)
		return FALSE;

	return TRUE;
}

CGrannyModelInstance* CGrannyLODController::GetModelInstance()
{
	return m_pCurrentModelInstance;
}
