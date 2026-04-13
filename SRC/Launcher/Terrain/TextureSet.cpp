#include "StdAfx.h"
#include "TextureSet.h"

#include <ostream>

CTextureSet::CTextureSet()
{
	Initialize();
}

CTextureSet::~CTextureSet()
{
	Clear();
}

void CTextureSet::Initialize()
{
}

void CTextureSet::Create()
{
	CResource* pResource = CResourceManager::Instance().GetResourcePointer("d:/ymir work/special/error.tga");
	m_ErrorTexture.ImageInstance.SetImagePointer(dynamic_cast<CGraphicImage*> (pResource));
	AddEmptyTexture();
}

bool CTextureSet::Load(const char* c_szTextureSetFileName, float fTerrainTexCoordBase)
{

	Clear();

	CTokenVectorMap stTokenVectorMap;

	if (!LoadMultipleTextData(c_szTextureSetFileName, stTokenVectorMap))
	{
		TraceError("TextureSet::Load : cannot load %s", c_szTextureSetFileName);
		return false;
	}

	if (!stTokenVectorMap.contains("textureset"))
	{
		TraceError("TextureSet::Load : syntax error, TextureSet (filename: %s)", c_szTextureSetFileName);
		return false;
	}

	if (!stTokenVectorMap.contains("texturecount"))
	{
		TraceError("TextureSet::Load : syntax error, TextureCount (filename: %s)", c_szTextureSetFileName);
		return false;
	}

	Create();

	const std::string& c_rstrCount = stTokenVectorMap["texturecount"][0];

	int32_t lCount = std::stol(c_rstrCount);

	m_Textures.resize(lCount + 1);

	for (int32_t i = 0; i < lCount; ++i)
	{
		char szTextureName[32 + 1];
		_snprintf(szTextureName, sizeof(szTextureName), "texture%03d", i + 1);

		if (!stTokenVectorMap.contains(szTextureName))
			continue;

		const CTokenVector& rVector = stTokenVectorMap[szTextureName];

		const std::string& c_rstrFileName = rVector[0];
		const std::string& c_rstrUScale = rVector[1];
		const std::string& c_rstrVScale = rVector[2];
		const std::string& c_rstrUOffset = rVector[3];
		const std::string& c_rstrVOffset = rVector[4];
		const std::string& c_rstrbSplat = rVector[5];
		const std::string& c_rstrBegin = rVector[6];
		const std::string& c_rstrEnd = rVector[7];

		float fuScale = std::stof(c_rstrUScale);
		float fvScale = std::stof(c_rstrVScale);
		float fuOffset = std::stof(c_rstrUOffset);
		float fvOffset = std::stof(c_rstrVOffset);
		bool bSplat = 0 != atoi(c_rstrbSplat.c_str());
		uint16_t usBegin = static_cast<uint16_t>(atoi(c_rstrBegin.c_str()));
		uint16_t usEnd = static_cast<uint16_t>(atoi(c_rstrEnd.c_str()));

		if (!SetTexture(i + 1, c_rstrFileName.c_str(), fuScale, fvScale, fuOffset, fvOffset, bSplat, usBegin, usEnd, fTerrainTexCoordBase))
			TraceError("CTextureSet::Load : SetTexture failed : Filename: %s", c_rstrFileName.c_str());
	}

	m_stFileName.assign(c_szTextureSetFileName);

	return true;
}

void CTextureSet::Clear()
{
	m_ErrorTexture.ImageInstance.Destroy();
	m_Textures.clear();
	Initialize();
}

void CTextureSet::AddEmptyTexture()
{
	TTerrainTexture eraser;
	m_Textures.push_back(eraser);
}

unsigned long CTextureSet::GetTextureCount() const
{
	return (unsigned long)m_Textures.size();
}

TTerrainTexture& CTextureSet::GetTexture(unsigned long ulIndex)
{
	if (GetTextureCount() <= ulIndex)
		return m_ErrorTexture;

	return m_Textures[ulIndex];
}

bool CTextureSet::SetTexture(unsigned long ulIndex, const char* c_szFileName, float fuScale, float fvScale, float fuOffset, float fvOffset, bool bSplat, uint16_t usBegin, uint16_t usEnd, float fTerrainTexCoordBase)
{

	if (ulIndex >= m_Textures.size())
	{
		TraceError("CTextureSet::SetTexture : Index Error : Index(%d) is Larger than TextureSet Size(%d)", ulIndex, m_Textures.size());
		return false;
	}

	CResource* pResource = CResourceManager::Instance().GetResourcePointer(c_szFileName);
#ifdef ENABLE_NEW_BUGFIXES
	if (!pResource) {
		return false;
	}
#endif

	if (!pResource->IsType(CGraphicImage::Type()))
	{
		TraceError("CTerrainImpl::GenerateTexture : %s is NOT Image File", pResource->GetFileName());
		return false;
	}

	TTerrainTexture& tex = m_Textures[ulIndex];

	tex.stFilename = c_szFileName;
	tex.UScale = fuScale;
	tex.VScale = fvScale;
	tex.UOffset = fuOffset;
	tex.VOffset = fvOffset;
	tex.bSplat = bSplat;
	tex.Begin = usBegin;
	tex.End = usEnd;
	tex.ImageInstance.SetImagePointer(dynamic_cast<CGraphicImage*>(pResource));
	tex.pd3dTexture = tex.ImageInstance.GetTexturePointer()->GetD3DTexture();


	D3DXMatrixScaling(&tex.m_matTransform, fTerrainTexCoordBase * tex.UScale, -fTerrainTexCoordBase * tex.VScale, 0.0f);
	tex.m_matTransform._41 = tex.UOffset;
	tex.m_matTransform._42 = -tex.VOffset;
	return true;
}

void CTextureSet::Reload(float fTerrainTexCoordBase)
{
	for (uint32_t dwIndex = 1; dwIndex < GetTextureCount(); ++dwIndex)
	{
		TTerrainTexture& tex = m_Textures[dwIndex];

		tex.ImageInstance.ReloadImagePointer(dynamic_cast<CGraphicImage*>(CResourceManager::Instance().GetResourcePointer(tex.stFilename.c_str())));
		tex.pd3dTexture = tex.ImageInstance.GetTexturePointer()->GetD3DTexture();

		D3DXMatrixScaling(&tex.m_matTransform, fTerrainTexCoordBase * tex.UScale, -fTerrainTexCoordBase * tex.VScale, 0.0f);
		tex.m_matTransform._41 = tex.UOffset;
		tex.m_matTransform._42 = -tex.VOffset;
	}
}

bool CTextureSet::AddTexture(const char* c_szFileName, float fuScale, float fvScale, float fuOffset, float fvOffset, bool bSplat, uint16_t usBegin, uint16_t usEnd, float fTerrainTexCoordBase)
{
	if (GetTextureCount() >= 256)
	{
		LogBox("You cannot add more than 255 texture.");
		return false;
	}

	for (unsigned long i = 1; i < GetTextureCount(); ++i)
	{
		if (c_szFileName == m_Textures[i].stFilename)
		{
			LogBox("Texture of the same name already exists.", "Duplicate");
			return false;
		}
	}

	CResource* pResource = CResourceManager::Instance().GetResourcePointer(c_szFileName);

	if (!pResource->IsType(CGraphicImage::Type()))
	{
		LogBox("CTerrainImpl::GenerateTexture : It's not an image file. %s", pResource->GetFileName());
		return false;
	}

	m_Textures.reserve(m_Textures.size() + 1);

	// @fixme003
	AddEmptyTexture();
	SetTexture(m_Textures.size() - 1,
		c_szFileName,
		fuScale,
		fvScale,
		fuOffset,
		fvOffset,
		bSplat,
		usBegin,
		usEnd,
		fTerrainTexCoordBase);

	return true;
}

bool CTextureSet::RemoveTexture(unsigned long ulIndex)
{
	if (GetTextureCount() <= ulIndex)
		return false;

	const auto itor = m_Textures.begin() + ulIndex;
	m_Textures.erase(itor);
	return true;
}

bool CTextureSet::Save(const char* c_pszFileName)
{
	FILE* pFile = fopen(c_pszFileName, "w");

	//std::ofstream(pFile);

	if (!pFile)
		return false;

	fprintf(pFile, "TextureSet\n");
	fprintf(pFile, "\n");

	// @fixme004
	fprintf(pFile, "TextureCount %lud\n", GetTextureCount() ? (GetTextureCount() - 1) : 0);
	fprintf(pFile, "\n");

	for (uint32_t i = 1; i < GetTextureCount(); ++i)
	{
		TTerrainTexture& rTex = m_Textures[i];

		fprintf(pFile, "Start Texture{:03}", i);
		fprintf(pFile, "    \"%s\"\n", rTex.stFilename.c_str());
		fprintf(pFile, "    %f\n", rTex.UScale);
		fprintf(pFile, "    %f\n", rTex.VScale);
		fprintf(pFile, "    %f\n", rTex.UOffset);
		fprintf(pFile, "    %f\n", rTex.VOffset);
		fprintf(pFile, "    %d\n", rTex.bSplat);
		fprintf(pFile, "    %hu\n", rTex.Begin);
		fprintf(pFile, "    %hu\n", rTex.End);
		fprintf(pFile, "End Texture%03d\n", i);
	}

	fclose(pFile);
	return true;
}
