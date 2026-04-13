#pragma once

#include "Material.h"

extern granny_data_type_definition GrannyPNT3322VertexType[5];

struct granny_pnt3322_vertex
{
	granny_real32 Position[3];
	granny_real32 Normal[3];
	granny_real32 UV0[2];
	granny_real32 UV1[2];
};

class CGrannyMesh
{
public:
	enum EType
	{
		TYPE_RIGID,
		TYPE_DEFORM,
		TYPE_MAX_NUM
	};

	using TTriGroupNode = struct STriGroupNode
	{
		STriGroupNode* pNextTriGroupNode;
		int idxPos;
		int triCount;
		uint32_t mtrlIndex;
	};

public:
	CGrannyMesh();
	virtual ~CGrannyMesh();

	bool IsEmpty() const;
	bool CreateFromGrannyMeshPointer(const granny_skeleton* pgrnSkeleton, granny_mesh* pgrnMesh, int vtxBasePos,
	                                 int idxBasePos, CGrannyMaterialPalette& rkMtrlPal);
	void LoadIndices(void* dstBaseIndices) const;
	void LoadPNTVertices(void* dstBaseVertices) const;
	void NEW_LoadVertices(void* dstBaseVertices) const;
	void Destroy();

	void SetPNT2Mesh();

	void DeformPNTVertices(void* dstBaseVertices, D3DXMATRIX* boneMatrices, const granny_mesh_binding* pgrnMeshBinding) const;
	bool CanDeformPNTVertices() const;
	bool IsTwoSide() const;

	int GetVertexCount() const;
	int* GetDefaultBoneIndices() const;
	int GetVertexBasePosition() const;
	int GetIndexBasePosition() const;

	const granny_mesh* GetGrannyMeshPointer() const;
	const TTriGroupNode* GetTriGroupNodeList(CGrannyMaterial::EType eMtrlType) const;

	static void RebuildTriGroupNodeList();
	void ReloadMaterials();

protected:
	void Initialize();

	bool LoadMaterials(CGrannyMaterialPalette& rkMtrlPal);
	bool LoadTriGroupNodeList(const CGrannyMaterialPalette& rkMtrlPal);

protected:
	granny_data_type_definition* m_pgrnMeshType;
	granny_mesh* m_pgrnMesh;
	granny_mesh_binding* m_pgrnMeshBindingTemp;
	granny_mesh_deformer* m_pgrnMeshDeformer;
	std::vector<uint32_t> m_mtrlIndexVector;
//	TTriGroupNode* m_triGroupNodes;

	std::unique_ptr<TTriGroupNode[]> m_triGroupNodes;

	TTriGroupNode* m_triGroupNodeLists[CGrannyMaterial::TYPE_MAX_NUM];

	int m_vtxBasePos;
	int m_idxBasePos;

	bool m_canDeformPNTVertex;
	bool m_isTwoSide;

private:
	bool m_bHaveBlendThing;

public:
	bool HaveBlendThing() const { return m_bHaveBlendThing; }
};
