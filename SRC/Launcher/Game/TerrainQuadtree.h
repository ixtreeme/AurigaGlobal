// TerrainQuadtreeNode.h: interface for the CTerrainQuadtreeNode class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

class CTerrainQuadtreeNode
{
public:
	CTerrainQuadtreeNode();
	virtual ~CTerrainQuadtreeNode();

public:
	int32_t					x0, y0, x1, y1;
	CTerrainQuadtreeNode *	NW_Node;
	CTerrainQuadtreeNode *	NE_Node;
	CTerrainQuadtreeNode *	SW_Node;
	CTerrainQuadtreeNode *	SE_Node;
	int32_t					Size;
	int32_t					PatchNum;
	D3DXVECTOR3				center;
	float					radius;
	uint8_t					m_byLODLevel;
};
