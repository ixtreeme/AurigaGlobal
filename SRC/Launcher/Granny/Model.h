#pragma once

#include <memory>

#include "../Render/GrpVertexBuffer.h"
#include "../Render/GrpIndexBuffer.h"

#include "Mesh.h"

class CGrannyModel final : public CReferenceObject
{
public:
	using TMeshNode = struct SMeshNode
	{
		int iMesh;
		const CGrannyMesh* pMesh;
		SMeshNode* pNextMeshNode;
	};

public:
	CGrannyModel();
	~CGrannyModel() override;

	bool IsEmpty() const;
	bool CreateFromGrannyModelPointer(granny_model* pgrnModel);
	bool CreateDeviceObjects();
	void DestroyDeviceObjects();
	void Destroy();

	int GetRigidVertexCount() const;
	int GetDeformVertexCount() const;
	int GetVertexCount() const;

	bool CanDeformPNTVertices() const;
	void DeformPNTVertices(void* dstBaseVertices, D3DXMATRIX* boneMatrices,
	                       const std::vector<granny_mesh_binding*>& c_rvct_pgrnMeshBinding) const;

	int GetIdxCount() const;
	int GetMeshCount() const;
	CGrannyMesh* GetMeshPointer(int iMesh);
	granny_model* GetGrannyModelPointer() const;
	const CGrannyMesh* GetMeshPointer(int iMesh) const;

	LPDIRECT3DVERTEXBUFFER9 GetPNTD3DVertexBuffer() const;
	LPDIRECT3DINDEXBUFFER9 GetD3DIndexBuffer() const;

	const TMeshNode* GetMeshNodeList(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType) const;

	bool LockVertices(void** indicies, void** vertices) const;
	void UnlockVertices() const;

	const CGrannyMaterialPalette& GetMaterialPalette() const;

protected:
	bool LoadMeshs();
	bool LoadPNTVertices();
	bool LoadIndices();
	void Initialize();

	BOOL CheckMeshIndex(int iIndex) const;
	void AppendMeshNode(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType, int iMesh);

protected:
	granny_model* m_pgrnModel;

	std::unique_ptr<CGrannyMesh[]>m_meshs;
	std::unique_ptr<TMeshNode[]>m_meshNodes;

	CGraphicVertexBuffer m_pntVtxBuf;
	CGraphicIndexBuffer m_idxBuf;


	TMeshNode* m_meshNodeLists[CGrannyMesh::TYPE_MAX_NUM][CGrannyMaterial::TYPE_MAX_NUM];

	int m_deformVtxCount;
	int m_rigidVtxCount;
	int m_vtxCount;
	int m_idxCount;

	int m_meshNodeSize;
	int m_meshNodeCapacity;

	bool m_canDeformPNVertices;

	CGrannyMaterialPalette m_kMtrlPal;

private:
	bool m_bHaveBlendThing;

public:
	bool HaveBlendThing() const { return m_bHaveBlendThing; }

protected:
	bool __LoadVertices();

protected:
	uint32_t m_dwFvF;
};
