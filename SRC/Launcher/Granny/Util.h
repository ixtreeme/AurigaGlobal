#pragma once

bool GrannyMeshIsDeform(granny_mesh* pgrnMesh);

class CGraphicImage;

struct SMaterialData
{
	CGraphicImage* pImage;
	float fSpecularPower;
	BOOL isSpecularEnable;
	uint8_t bSphereMapIndex;
};
