#include "StdAfx.h"
#include "Material.h"
#include "Mesh.h"
#include "../Base/Filename.h"
#include "../Render/ResourceManager.h"
#include "../Render/StateManager.h"
#include "../Render/GrpScreen.h"
#include <mutex>

CGraphicImageInstance CGrannyMaterial::ms_akSphereMapInstance[SPHEREMAP_NUM];

D3DXVECTOR3 CGrannyMaterial::ms_v3SpecularTrans(0.0f, 0.0f, 0.0f);
D3DXMATRIX CGrannyMaterial::ms_matSpecular;

D3DXCOLOR g_fSpecularColor = D3DXCOLOR(0.0f, 0.0f, 0.0f, 0.0f);

void CGrannyMaterial::TranslateSpecularMatrix(const float fAddX, const float fAddY, const float fAddZ)
{
	static float SPECULAR_TRANSLATE_MAX = 1000000.0f;

	ms_v3SpecularTrans.x += fAddX;
	ms_v3SpecularTrans.y += fAddY;
	ms_v3SpecularTrans.z += fAddZ;

	if (ms_v3SpecularTrans.x >= SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.x = 0.0f;

	if (ms_v3SpecularTrans.y >= SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.y = 0.0f;

	if (ms_v3SpecularTrans.z >= SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.z = 0.0f;

	D3DXMatrixTranslation(&ms_matSpecular,
		ms_v3SpecularTrans.x,
		ms_v3SpecularTrans.y,
		ms_v3SpecularTrans.z
	);
}

void CGrannyMaterial::ApplyRenderState()
{
	assert(m_pfnApplyRenderState != NULL && "CGrannyMaterial::SaveRenderState");
	(this->*m_pfnApplyRenderState)();
}

void CGrannyMaterial::RestoreRenderState()
{
	assert(m_pfnRestoreRenderState != NULL && "CGrannyMaterial::RestoreRenderState");
	(this->*m_pfnRestoreRenderState)();
}

void CGrannyMaterial::Copy(const CGrannyMaterial& rkMtrl)
{
	m_pgrnMaterial = rkMtrl.m_pgrnMaterial;
	m_roImage[0] = rkMtrl.m_roImage[0];
	m_roImage[1] = rkMtrl.m_roImage[1];
	m_eType = rkMtrl.m_eType;
}

CGrannyMaterial::CGrannyMaterial()
{
	m_bTwoSideRender = false;
	m_dwLastCullRenderStateForTwoSideRendering = D3DCULL_CW;

	Initialize();
}

CGrannyMaterial::~CGrannyMaterial()
= default;

CGrannyMaterial::EType CGrannyMaterial::GetType() const
{
	return m_eType;
}

void CGrannyMaterial::SetImagePointer(const int iStage, CGraphicImage* pImage)
{
	assert(iStage < 2 && "CGrannyMaterial::SetImagePointer");
	m_roImage[iStage] = pImage;
}

bool CGrannyMaterial::IsIn(const char* c_szImageName, int* piStage) const
{
	std::string strImageName = c_szImageName;
	CFileNameHelper::StringPath(strImageName);

	if (const granny_texture* pgrnDiffuseTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyDiffuseColorTexture))
	{
		std::string strDiffuseFileName = pgrnDiffuseTexture->FromFileName;
		CFileNameHelper::StringPath(strDiffuseFileName);
		if (strDiffuseFileName == strImageName)
		{
			*piStage = 0;
			return true;
		}
	}

	if (const granny_texture* pgrnOpacityTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyOpacityTexture))
	{
		std::string strOpacityFileName = pgrnOpacityTexture->FromFileName;
		CFileNameHelper::StringPath(strOpacityFileName);
		if (strOpacityFileName == strImageName)
		{
			*piStage = 1;
			return true;
		}
	}

	return false;
}

void CGrannyMaterial::SetSpecularInfo(const BOOL bFlag, const float fPower, const uint8_t uSphereMapIndex)
{
	m_fSpecularPower = fPower;
	m_bSphereMapIndex = uSphereMapIndex;
	m_bSpecularEnable = bFlag;

	if (bFlag)
	{
		m_pfnApplyRenderState = &CGrannyMaterial::__ApplySpecularRenderState;
		m_pfnRestoreRenderState = &CGrannyMaterial::__RestoreSpecularRenderState;
	}
	else
	{
		m_pfnApplyRenderState = &CGrannyMaterial::__ApplyDiffuseRenderState;
		m_pfnRestoreRenderState = &CGrannyMaterial::__RestoreDiffuseRenderState;
	}
}

bool CGrannyMaterial::IsEqual(const granny_material* pgrnMaterial) const
{
	if (m_pgrnMaterial == pgrnMaterial)
		return true;

	return false;
}


LPDIRECT3DTEXTURE9 CGrannyMaterial::GetD3DTexture(const int iStage) const
{
	const CGraphicImage::TRef& ratImage = m_roImage[iStage];

	if (ratImage.IsNull())
		return nullptr;

	CGraphicImage* pImage = ratImage.GetPointer();
	const CGraphicTexture* pTexture = pImage->GetTexturePointer();
	return pTexture->GetD3DTexture();
}

CGraphicImage* CGrannyMaterial::GetImagePointer(const int iStage) const
{
	const CGraphicImage::TRef& ratImage = m_roImage[iStage];

	if (ratImage.IsNull())
		return nullptr;

	CGraphicImage* pImage = ratImage.GetPointer();
	return pImage;
}

const CGraphicTexture* CGrannyMaterial::GetDiffuseTexture() const
{
	if (m_roImage[0].IsNull())
		return nullptr;

	return m_roImage[0].GetPointer()->GetTexturePointer();
}

const CGraphicTexture* CGrannyMaterial::GetOpacityTexture() const
{
	if (m_roImage[1].IsNull())
		return nullptr;

	return m_roImage[1].GetPointer()->GetTexturePointer();
}

BOOL CGrannyMaterial::__IsSpecularEnable() const
{
	return m_bSpecularEnable;
}

float CGrannyMaterial::__GetSpecularPower() const
{
	return m_fSpecularPower;
}

extern const std::string& GetModelLocalPath();

CGraphicImage* CGrannyMaterial::__GetImagePointer(const char* fileName)
{
	assert(*fileName != '\0');

	CResourceManager& rkResMgr = CResourceManager::Instance();

	if (const int fileName_len = static_cast<const int>(strlen(fileName)); fileName_len > 2 && fileName[1] != ':')
	{
		char localFileName[256];
		const std::string& modelLocalPath = GetModelLocalPath();

		const int localFileName_len = static_cast<const int>(modelLocalPath.length() + 1 + fileName_len);
		if (localFileName_len < sizeof(localFileName) - 1)
		{
			_snprintf(localFileName, sizeof(localFileName), "%s%s", GetModelLocalPath().c_str(), fileName);
			CResource* pResource = rkResMgr.GetResourcePointer(localFileName);
			return dynamic_cast<CGraphicImage*>(pResource);
		}
	}

	CResource* pResource = rkResMgr.GetResourcePointer(fileName);
	return dynamic_cast<CGraphicImage*>(pResource);
}

bool CGrannyMaterial::CreateFromGrannyMaterialPointer(granny_material* pgrnMaterial)
{
	m_pgrnMaterial = pgrnMaterial;

	const granny_texture* pgrnDiffuseTexture = nullptr;
	const granny_texture* pgrnOpacityTexture = nullptr;

	if (pgrnMaterial)
	{
		if (pgrnMaterial->MapCount > 1 && !_strnicmp(pgrnMaterial->Name, "Blend", 5))
		{
			pgrnDiffuseTexture = GrannyGetMaterialTextureByType(pgrnMaterial->Maps[0].Material,
				GrannyDiffuseColorTexture);
			pgrnOpacityTexture = GrannyGetMaterialTextureByType(pgrnMaterial->Maps[1].Material,
				GrannyDiffuseColorTexture);
		}
		else
		{
			pgrnDiffuseTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyDiffuseColorTexture);
			pgrnOpacityTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyOpacityTexture);
		}

		{
			granny_int32 twoSided = 0;
			constexpr granny_data_type_definition TwoSidedFieldType[] =
			{
				{GrannyInt32Member, "Two-sided"},
				{GrannyEndMember},
			};

			granny_variant twoSideResult;

			if (GrannyFindMatchingMember(pgrnMaterial->ExtendedData.Type, pgrnMaterial->ExtendedData.Object,
				"Two-sided", &twoSideResult)
				&& nullptr != twoSideResult.Type)
				GrannyConvertSingleObject(twoSideResult.Type, twoSideResult.Object, TwoSidedFieldType, &twoSided,
					nullptr);

			m_bTwoSideRender = 1 == twoSided;
		}
	}

	if (pgrnDiffuseTexture)
		m_roImage[0].SetPointer(__GetImagePointer(pgrnDiffuseTexture->FromFileName));

	if (pgrnOpacityTexture)
		m_roImage[1].SetPointer(__GetImagePointer(pgrnOpacityTexture->FromFileName));

	if (!m_roImage[1].IsNull())
		m_eType = TYPE_BLEND_PNT;
	else
		m_eType = TYPE_DIFFUSE_PNT;

	return true;
}

void CGrannyMaterial::Initialize()
{
	m_roImage[0] = nullptr;
	m_roImage[1] = nullptr;

	SetSpecularInfo(FALSE, 0.0f, 0);
}

void CGrannyMaterial::__ApplyDiffuseRenderState()
{
	STATEMANAGER.SetTexture(0, GetD3DTexture(0));

	if (m_bTwoSideRender)
	{
		m_dwLastCullRenderStateForTwoSideRendering = STATEMANAGER.GetRenderState(D3DRS_CULLMODE);
		STATEMANAGER.SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	}
}

void CGrannyMaterial::__RestoreDiffuseRenderState() const
{
	if (m_bTwoSideRender)
	{
		STATEMANAGER.SetRenderState(D3DRS_CULLMODE, m_dwLastCullRenderStateForTwoSideRendering);
	}
}

void CGrannyMaterial::__ApplySpecularRenderState()
{
	if (TRUE == STATEMANAGER.GetRenderState(D3DRS_ALPHABLENDENABLE))
	{
		__ApplyDiffuseRenderState();
		return;
	}

	const CGraphicTexture* pkTexture = ms_akSphereMapInstance[m_bSphereMapIndex].GetTexturePointer();

	STATEMANAGER.SetTexture(0, GetD3DTexture(0));

	if (pkTexture)
		STATEMANAGER.SetTexture(1, pkTexture->GetD3DTexture());
	else
		STATEMANAGER.SetTexture(1, nullptr);

	STATEMANAGER.SetRenderState(D3DRS_TEXTUREFACTOR,
		D3DXCOLOR(g_fSpecularColor.r, g_fSpecularColor.g, g_fSpecularColor.b,
			__GetSpecularPower()));
	STATEMANAGER.SaveTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR);
	STATEMANAGER.SaveTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	STATEMANAGER.SaveTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	STATEMANAGER.SaveTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	STATEMANAGER.SaveTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	STATEMANAGER.SaveTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
	STATEMANAGER.SaveTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

	STATEMANAGER.SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	STATEMANAGER.SetTransform(D3DTS_TEXTURE1, &ms_matSpecular);
	STATEMANAGER.SaveTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	STATEMANAGER.SaveSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	STATEMANAGER.SaveSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
}

void CGrannyMaterial::__RestoreSpecularRenderState() const
{
	if (TRUE == STATEMANAGER.GetRenderState(D3DRS_ALPHABLENDENABLE))
	{
		__RestoreDiffuseRenderState();
		return;
	}

	STATEMANAGER.RestoreTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS);
	STATEMANAGER.RestoreSamplerState(1, D3DSAMP_ADDRESSU);
	STATEMANAGER.RestoreSamplerState(1, D3DSAMP_ADDRESSV);

	STATEMANAGER.RestoreTextureStageState(1, D3DTSS_TEXCOORDINDEX);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	STATEMANAGER.RestoreTextureStageState(0, D3DTSS_COLORARG1);
	STATEMANAGER.RestoreTextureStageState(0, D3DTSS_COLORARG2);
	STATEMANAGER.RestoreTextureStageState(0, D3DTSS_COLOROP);
	STATEMANAGER.RestoreTextureStageState(0, D3DTSS_ALPHAARG1);
	STATEMANAGER.RestoreTextureStageState(0, D3DTSS_ALPHAARG2);
	STATEMANAGER.RestoreTextureStageState(0, D3DTSS_ALPHAOP);
}

void CGrannyMaterial::CreateSphereMap(const UINT uMapIndex, const char* c_szSphereMapImageFileName)
{
	CResourceManager& rkResMgr = CResourceManager::Instance();
	const auto pImage = dynamic_cast<CGraphicImage*>(rkResMgr.GetResourcePointer(c_szSphereMapImageFileName));
	ms_akSphereMapInstance[uMapIndex].SetImagePointer(pImage);
}

void CGrannyMaterial::DestroySphereMap()
{
	for (auto& uMapIndex : ms_akSphereMapInstance)
		uMapIndex.Destroy();
}

CGrannyMaterialPalette::CGrannyMaterialPalette()
= default;

CGrannyMaterialPalette::~CGrannyMaterialPalette()
{
	Clear();
}

void CGrannyMaterialPalette::Copy(const CGrannyMaterialPalette& rkMtrlPalSrc)
{
	m_mtrlVector = rkMtrlPalSrc.m_mtrlVector;
}



void CGrannyMaterialPalette::Clear()
{
	std::vector<CGrannyMaterial::TRef> tmp;

	{
		// EXKLUZÍV zár: vector módosítás
		std::unique_lock<std::shared_mutex> lk(m_mtrlMutex);
		tmp.swap(m_mtrlVector);
	} // itt feloldjuk a m_mtrlMutex-et

	// tmp itt (függvény végén) felszabadul -> referenciák elengedése lockon kívül
}

CGrannyMaterial::TRef CGrannyMaterialPalette::GetMaterialRef(const uint32_t mtrlIndex) const
{
	std::shared_lock lk(m_mtrlMutex);
	assert(mtrlIndex < m_mtrlVector.size());
	return m_mtrlVector[mtrlIndex];
}

void CGrannyMaterialPalette::SetMaterialImagePointer(const char* c_szImageName, CGraphicImage* pImage)
{
	std::unique_lock<std::shared_mutex> lk(m_mtrlMutex);

	const uint32_t size = static_cast<uint32_t>(m_mtrlVector.size());
	for (uint32_t i = 0; i < size; ++i)
	{
		CGrannyMaterial::TRef& roMtrl = m_mtrlVector[i];  // referencia az elemre (írni fogjuk)

		int iStage = 0;
		if (roMtrl->IsIn(c_szImageName, &iStage))
		{
			const auto pkNewMtrl = new CGrannyMaterial;
			pkNewMtrl->Copy(*roMtrl.GetPointer());
			pkNewMtrl->SetImagePointer(iStage, pImage);

			roMtrl = pkNewMtrl; // vector elem módosítás -> unique lock kell

			return;
		}
	}
}

void CGrannyMaterialPalette::SetMaterialData(const char* c_szMtrlName, const SMaterialData& c_rkMaterialData)
{
	if (c_szMtrlName)
	{
		for (auto& roMtrl : m_mtrlVector)
		{
			int iStage;
			if (roMtrl->IsIn(c_szMtrlName, &iStage))
			{
				const auto pkNewMtrl = new CGrannyMaterial;
				pkNewMtrl->Copy(*roMtrl.GetPointer());
				pkNewMtrl->SetImagePointer(iStage, c_rkMaterialData.pImage);
				pkNewMtrl->SetSpecularInfo(c_rkMaterialData.isSpecularEnable, c_rkMaterialData.fSpecularPower,
					c_rkMaterialData.bSphereMapIndex);
				roMtrl = pkNewMtrl;

				return;
			}
		}
	}
	else
	{
		for (const auto& roMtrl : m_mtrlVector)
		{
			roMtrl->SetSpecularInfo(c_rkMaterialData.isSpecularEnable, c_rkMaterialData.fSpecularPower,
				c_rkMaterialData.bSphereMapIndex);
		}
	}
}

void CGrannyMaterialPalette::SetSpecularInfo(const char* c_szMtrlName, const BOOL bEnable, const float fPower) const
{
	const uint32_t size = static_cast<uint32_t>(m_mtrlVector.size());
	uint32_t i;
	if (c_szMtrlName)
	{
		for (i = 0; i < size; ++i)
		{
			const CGrannyMaterial::TRef& roMtrl = m_mtrlVector[i];

			int iStage;
			if (roMtrl->IsIn(c_szMtrlName, &iStage))
			{
				roMtrl->SetSpecularInfo(bEnable, fPower, 0);
				return;
			}
		}
	}
	else
	{
		for (i = 0; i < size; ++i)
		{
			const CGrannyMaterial::TRef& roMtrl = m_mtrlVector[i];
			roMtrl->SetSpecularInfo(bEnable, fPower, 0);
		}
	}
}

uint32_t CGrannyMaterialPalette::RegisterMaterial(granny_material* pgrnMaterial)
{
	std::unique_lock lk(m_mtrlMutex);
	const uint32_t size = static_cast<uint32_t>(m_mtrlVector.size());
	for (uint32_t i = 0; i < size; ++i)
	{
		const CGrannyMaterial::TRef& roMtrl = m_mtrlVector[i];
		if (roMtrl->IsEqual(pgrnMaterial))
			return i;
	}

	const auto pkNewMtrl = new CGrannyMaterial;
	pkNewMtrl->CreateFromGrannyMaterialPointer(pgrnMaterial);
	m_mtrlVector.emplace_back(pkNewMtrl);
	return size;
}

uint32_t CGrannyMaterialPalette::GetMaterialCount() const
{
	return static_cast<uint32_t>(m_mtrlVector.size());
}
