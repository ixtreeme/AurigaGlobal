#pragma once

#include "../Render/GrpImageInstance.h"

typedef struct STerrainTexture
{
	STerrainTexture() : pd3dTexture(nullptr),
		UScale(4.0f),
		VScale(4.0f),
		UOffset(0.0f),
		VOffset(0.0f),
		bSplat(true),
		Begin(0),
		End(0)
	{
	}

	~STerrainTexture() = default;

	std::string					stFilename;
	LPDIRECT3DTEXTURE9			pd3dTexture;
	CGraphicImageInstance 		ImageInstance;
	float						UScale;
	float						VScale;
	float						UOffset;
	float						VOffset;
	bool						bSplat;
	uint16_t				Begin, End;	// 0 ~ 65535 의 16bit heightfield 높이값.
	D3DXMATRIX					m_matTransform;
} TTerrainTexture;

class CTextureSet
{
public:
	typedef std::vector<TTerrainTexture> TTextureVector;

	CTextureSet();
	virtual ~CTextureSet();

	static void			Initialize();
	void			Clear();

	void			Create();

	bool			Load(const char* c_pszFileName, float fTerrainTexCoordBase);
	bool			Save(const char* c_pszFileName);

	unsigned long GetTextureCount() const;

	TTerrainTexture& GetTexture(unsigned long ulIndex);
	bool			RemoveTexture(unsigned long ulIndex);

	bool			SetTexture(unsigned long ulIndex,const char* c_szFileName,float fuScale,float fvScale,float fuOffset,float fvOffset,bool bSplat,uint16_t usBegin, uint16_t usEnd,float fTerrainTexCoordBase);

	void			Reload(float fTerrainTexCoordBase);

	bool			AddTexture(const char* c_szFileName, float fuScale, float fvScale, float fuOffset, float fvOffset, bool bSplat, uint16_t usBegin, uint16_t usEnd, float fTerrainTexCoordBase);

	const char* GetFileName() const { return m_stFileName.c_str(); }

protected:
	void			AddEmptyTexture();

protected:
	TTextureVector			m_Textures;
	TTerrainTexture			m_ErrorTexture;
	std::string				m_stFileName;
};
