#pragma once

#include "../Render/GrpVertexBuffer.h"
#include "../Render/GrpIndexBuffer.h"


#define PR_FLOAT_TO_INT(inreal, outint) (outint) = static_cast<int>(inreal);

#define TERRAIN_PATCHSIZE	16
#define TERRAIN_SIZE		128
#define TERRAIN_PATCHCOUNT	TERRAIN_SIZE/TERRAIN_PATCHSIZE
#define MAXTERRAINTEXTURES	256

typedef struct
{
	int32_t					Active;
	int32_t					NeedsUpdate;
	LPDIRECT3DTEXTURE9		pd3dTexture;
} TTerainSplat;

typedef struct
{
 	uint32_t			TileCount[MAXTERRAINTEXTURES];
	uint32_t			PatchTileCount[TERRAIN_PATCHCOUNT*TERRAIN_PATCHCOUNT][MAXTERRAINTEXTURES];
	TTerainSplat 	Splats[MAXTERRAINTEXTURES];
	bool			m_bNeedsUpdate;
} TTerrainSplatPatch;

typedef struct
{
	char					used;
	short					mat;

	CGraphicVertexBuffer	vb;
	CGraphicIndexBuffer		ib;
	int32_t					VertexSize;

	short					NumIndices;

	float					minx, maxx;
	float					miny, maxy;
	float					minz, maxz;
} TERRAIN_VBUFFER;

typedef struct
{
	char name[19];
	float ambi_r, ambi_g, ambi_b, ambi_a;		/* Ambient Color */
	float diff_r, diff_g, diff_b, diff_a;		/* Diffuse Color */
	float spec_r, spec_g, spec_b, spec_a;		/* Specular Color */
	float spec_power;							/* Specular power */
} PR_MATERIAL;

typedef struct
{
	/* Public Settings */
	float			PageUVLength;
	int32_t			SquaresPerTexture;              /* Heightfield squares per texture (128 texels) */
	int32_t			SplatTilesX;					/* Number of splat textures across map */
	int32_t			SplatTilesY;					/* Number of splat textures down map */
	int32_t			DisableWrapping;
	int32_t			DisableShadow;
	int32_t			ShadowMode;
	int32_t			OutsideVisible;
	D3DXVECTOR3		SunLocation;
} TTerrainGlobals;
