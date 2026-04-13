#include "StdAfx.h"
#include "ModelInstance.h"
#include "Model.h"

void CGrannyModelInstance::Clear()
{
	m_kMtrlPal.Clear();

	DestroyDeviceObjects();
	__DestroyMeshBindingVector();
	__DestroyMeshMatrices();
	__DestroyModelInstance();
	__DestroyWorldPose();

	__Initialize();
}

void CGrannyModelInstance::SetMainModelPointer(CGrannyModel* pModel,
	CGraphicVertexBuffer* pkSharedDeformableVertexBuffer)
{
	SetLinkedModelPointer(pModel, pkSharedDeformableVertexBuffer, nullptr);
}

void CGrannyModelInstance::SetLinkedModelPointer(CGrannyModel* pkModel,
	CGraphicVertexBuffer* pkSharedDeformableVertexBuffer,
	CGrannyModelInstance** ppkSkeletonInst)
{
	Clear();

	if (m_pModel)
		m_pModel->Release();

	m_pModel = pkModel;

	m_pModel->AddReference();

	if (pkSharedDeformableVertexBuffer)
		__SetSharedDeformableVertexBuffer(pkSharedDeformableVertexBuffer);
	else
		__CreateDynamicVertexBuffer();

	__CreateModelInstance();

	if (ppkSkeletonInst && *ppkSkeletonInst)
	{
		m_ppkSkeletonInst = ppkSkeletonInst;
		__CreateWorldPose(*ppkSkeletonInst);
		__CreateMeshBindingVector(*ppkSkeletonInst);
	}
	else
	{
		__CreateWorldPose(nullptr);
		__CreateMeshBindingVector(nullptr);
	}

	__CreateMeshMatrices();

	ResetLocalTime();

	m_kMtrlPal.Copy(pkModel->GetMaterialPalette());
}

granny_world_pose* CGrannyModelInstance::__GetWorldPosePtr() const
{
	if (m_pgrnWorldPoseReal)
		return m_pgrnWorldPoseReal;

	if (m_ppkSkeletonInst && *m_ppkSkeletonInst)
		return (*m_ppkSkeletonInst)->m_pgrnWorldPoseReal;

	assert(m_ppkSkeletonInst != NULL && "__GetWorldPosePtr - NO HAVE SKELETON");
	return nullptr;
}

int* CGrannyModelInstance::__GetMeshBoneIndices(const unsigned int iMeshBinding) const
{
	assert(iMeshBinding < m_vct_pgrnMeshBinding.size());
	return (int*)GrannyGetMeshBindingToBoneIndices(m_vct_pgrnMeshBinding[iMeshBinding]);
}

bool CGrannyModelInstance::__CreateMeshBindingVector(const CGrannyModelInstance* pkDstModelInst)
{
	assert(m_vct_pgrnMeshBinding.empty());

	if (!m_pModel)
		return false;

	const granny_model* pgrnModel = m_pModel->GetGrannyModelPointer();
	if (!pgrnModel)
		return false;

	const granny_skeleton* pgrnDstSkeleton = pgrnModel->Skeleton;
	if (pkDstModelInst && pkDstModelInst->m_pModel && pkDstModelInst->m_pModel->GetGrannyModelPointer())
		pgrnDstSkeleton = pkDstModelInst->m_pModel->GetGrannyModelPointer()->Skeleton;

	m_vct_pgrnMeshBinding.reserve(pgrnModel->MeshBindingCount);

	for (granny_int32 iMeshBinding = 0; iMeshBinding != pgrnModel->MeshBindingCount; ++iMeshBinding)
		m_vct_pgrnMeshBinding.emplace_back(
			GrannyNewMeshBinding(pgrnModel->MeshBindings[iMeshBinding].Mesh, pgrnModel->Skeleton, pgrnDstSkeleton));

	return true;
}

void CGrannyModelInstance::__DestroyMeshBindingVector()
{
	for (auto& meshBinding : m_vct_pgrnMeshBinding)
	{
		GrannyFreeMeshBinding(meshBinding);
	}
	m_vct_pgrnMeshBinding.clear();
}

void CGrannyModelInstance::__CreateWorldPose(const CGrannyModelInstance* pkSkeletonInst)
{
	assert(m_pgrnModelInstance != NULL);
	assert(m_pgrnWorldPoseReal == NULL);

	if (pkSkeletonInst)
		return;

	const granny_skeleton* pgrnSkeleton = GrannyGetSourceSkeleton(m_pgrnModelInstance);

	m_pgrnWorldPoseReal = GrannyNewWorldPose(pgrnSkeleton->BoneCount);
}

void CGrannyModelInstance::__DestroyWorldPose()
{
	if (!m_pgrnWorldPoseReal)
		return;

	GrannyFreeWorldPose(m_pgrnWorldPoseReal);
	m_pgrnWorldPoseReal = nullptr;
}

void CGrannyModelInstance::__CreateModelInstance()
{
	assert(m_pModel != NULL);
	assert(m_pgrnModelInstance == NULL);

	const granny_model* pgrnModel = m_pModel->GetGrannyModelPointer();
	m_pgrnModelInstance = GrannyInstantiateModel(pgrnModel);
}

void CGrannyModelInstance::__DestroyModelInstance()
{
	if (!m_pgrnModelInstance)
		return;

	GrannyFreeModelInstance(m_pgrnModelInstance);
	m_pgrnModelInstance = nullptr;
}

void CGrannyModelInstance::__CreateMeshMatrices()
{
	assert(m_pModel != NULL);

	if (m_pModel->GetMeshCount() <= 0)
		return;

	const int meshCount = m_pModel->GetMeshCount();
	//m_meshMatrices = new D3DXMATRIX[meshCount];

	m_meshMatrices = std::make_unique<D3DXMATRIX[]>(meshCount);
}

void CGrannyModelInstance::__DestroyMeshMatrices()
{
	if (!m_meshMatrices)
		return;

	/*delete [] m_meshMatrices;
	m_meshMatrices = nullptr;*/
}

uint32_t CGrannyModelInstance::GetDeformableVertexCount() const
{
	if (!m_pModel)
		return 0;

	return m_pModel->GetDeformVertexCount();
}

uint32_t CGrannyModelInstance::GetVertexCount()
{
	if (!m_pModel)
		return 0;

	return m_pModel->GetVertexCount();
}

void CGrannyModelInstance::__SetSharedDeformableVertexBuffer(CGraphicVertexBuffer* pkSharedDeformableVertexBuffer)
{
	m_pkSharedDeformableVertexBuffer = pkSharedDeformableVertexBuffer;
}

bool CGrannyModelInstance::__IsDeformableVertexBuffer() const
{
	if (m_pkSharedDeformableVertexBuffer)
		return true;

	return m_kLocalDeformableVertexBuffer.IsEmpty();
}

IDirect3DVertexBuffer9* CGrannyModelInstance::__GetDeformableD3DVertexBufferPtr()
{
	return __GetDeformableVertexBufferRef().GetD3DVertexBuffer();
}

CGraphicVertexBuffer& CGrannyModelInstance::__GetDeformableVertexBufferRef()
{
	if (m_pkSharedDeformableVertexBuffer)
		return *m_pkSharedDeformableVertexBuffer;

	return m_kLocalDeformableVertexBuffer;
}

void CGrannyModelInstance::__CreateDynamicVertexBuffer()
{
	assert(m_pModel != NULL);
	assert(m_kLocalDeformableVertexBuffer.IsEmpty());

	const int vtxCount = m_pModel->GetDeformVertexCount();

	if (0 != vtxCount)
	{
		if (!m_kLocalDeformableVertexBuffer.Create(vtxCount,
			D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1,
			D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED
		))
			return;
	}
}

void CGrannyModelInstance::__DestroyDynamicVertexBuffer()
{
	m_kLocalDeformableVertexBuffer.Destroy();
	m_pkSharedDeformableVertexBuffer = nullptr;
}

bool CGrannyModelInstance::GetBoneIndexByName(const char* c_szBoneName, int* pBoneIndex) const
{
	assert(m_pgrnModelInstance != NULL);

	const granny_skeleton* pgrnSkeleton = GrannyGetSourceSkeleton(m_pgrnModelInstance);

	if (!GrannyFindBoneByName(pgrnSkeleton, c_szBoneName, pBoneIndex))
		return false;

	return true;
}

const float* CGrannyModelInstance::GetBoneMatrixPointer(const int iBone) const
{
	const float* bones = GrannyGetWorldPose4x4(__GetWorldPosePtr(), iBone);
	if (!bones)
	{
		granny_model* pModel = m_pModel->GetGrannyModelPointer();
		return nullptr;
	}
	return bones;
}

const float* CGrannyModelInstance::GetCompositeBoneMatrixPointer(const int iBone) const
{
	return GrannyGetWorldPoseComposite4x4(__GetWorldPosePtr(), iBone);
}

void CGrannyModelInstance::ReloadTexture()
{
	assert("현재 사용하지 않음 - CGrannyModelInstance::ReloadTexture()");
}
