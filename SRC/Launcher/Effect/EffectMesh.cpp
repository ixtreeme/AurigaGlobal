#include "StdAfx.h"
#include "../Render/StateManager.h"
#include "../Render/ResourceManager.h"
#include "../Pack/EterPackManager.h"
#include "EffectMesh.h"


CDynamicPool<CEffectMesh::SEffectMeshData> CEffectMesh::SEffectMeshData::ms_kPool;

CEffectMesh::SEffectMeshData* CEffectMesh::SEffectMeshData::New()
{
	return ms_kPool.Alloc();
}

void CEffectMesh::SEffectMeshData::Delete(SEffectMeshData* pkData)
{
	pkData->EffectFrameDataVector.clear();
	pkData->pImageVector.clear();

	ms_kPool.Free(pkData);
}

void CEffectMesh::SEffectMeshData::DestroySystem()
{
	ms_kPool.Destroy();
}


uint32_t CEffectMesh::GetFrameCount() const
{
	return m_iFrameCount;
}

uint32_t CEffectMesh::GetMeshCount() const
{
	return static_cast<uint32_t>(m_pEffectMeshDataVector.size());
}

CEffectMesh::TEffectMeshData * CEffectMesh::GetMeshDataPointer(uint32_t dwMeshIndex) const
{
	assert(dwMeshIndex < m_pEffectMeshDataVector.size());
	return m_pEffectMeshDataVector[dwMeshIndex];
}

std::vector<CGraphicImage*>* CEffectMesh::GetTextureVectorPointer(uint32_t dwMeshIndex) const
{
	if (dwMeshIndex>=m_pEffectMeshDataVector.size())
		return nullptr;

	return &m_pEffectMeshDataVector[dwMeshIndex]->pImageVector;
}

std::vector<CGraphicImage*> & CEffectMesh::GetTextureVectorReference(uint32_t dwMeshIndex) const
{
	return m_pEffectMeshDataVector[dwMeshIndex]->pImageVector;
}

CEffectMesh::TType CEffectMesh::Type()
{
	static TType s_type = StringToType("CEffectMesh");
	return s_type;
}

bool CEffectMesh::OnIsType(TType type)
{
	if (CEffectMesh::Type() == type)
		return true;

	return CResource::OnIsType(type);
}

bool CEffectMesh::OnLoad(int iSize, const void * c_pvBuf)
{
	if (!c_pvBuf)
		return false;

	auto c_pbBuf = static_cast<const uint8_t*> (c_pvBuf);

	char szHeader[10+1];
	memcpy(szHeader, c_pbBuf, 10+1);
	c_pbBuf += 10+1;

	if (0 == strcmp("EffectData", szHeader))
	{
		if (!__LoadData_Ver001(iSize, c_pbBuf))
			return false;
	}
	else if (0 == strcmp("MDEData002", szHeader))
	{
		if (!__LoadData_Ver002(iSize, c_pbBuf))
			return false;
	}
	else
	{
		return false;
	}

	m_isData = true;
	return true;
}

bool CEffectMesh::__LoadData_Ver002(int iSize, const uint8_t* c_pbBuf)
{
	std::vector<D3DXVECTOR3> v3VertexVector;
	std::vector<int> iIndexVector;
	std::vector<D3DXVECTOR2> v3TextureVertexVector;
	std::vector<int> iTextureIndexVector;

	m_iGeomCount = *(int *)c_pbBuf;
	c_pbBuf += 4;
	m_iFrameCount = *(int *)c_pbBuf;
	c_pbBuf += 4;

	m_pEffectMeshDataVector.clear();
	m_pEffectMeshDataVector.resize(m_iGeomCount);

	for (short n = 0; n < m_iGeomCount; ++n)
	{
		SEffectMeshData * pMeshData = SEffectMeshData::New();

		memcpy(pMeshData->szObjectName, c_pbBuf, 32);
		c_pbBuf += 32;
		memcpy(pMeshData->szDiffuseMapFileName, c_pbBuf, 128);
		c_pbBuf += 128;

		pMeshData->EffectFrameDataVector.clear();
		pMeshData->EffectFrameDataVector.resize(m_iFrameCount);

		for(int i = 0; i < m_iFrameCount; ++i)
		{
			TEffectFrameData & rFrameData = pMeshData->EffectFrameDataVector[i];

			memcpy(&rFrameData.byChangedFrame, c_pbBuf, sizeof(uint8_t));
			c_pbBuf += sizeof(uint8_t);

			memcpy(&rFrameData.fVisibility, c_pbBuf, sizeof(float));
			c_pbBuf += sizeof(float);

			memcpy(&rFrameData.dwVertexCount, c_pbBuf, sizeof(uint32_t));
			c_pbBuf += sizeof(uint32_t);

			memcpy(&rFrameData.dwIndexCount, c_pbBuf, sizeof(uint32_t));
			c_pbBuf += sizeof(uint32_t);

			memcpy(&rFrameData.dwTextureVertexCount, c_pbBuf, sizeof(uint32_t));
			c_pbBuf += sizeof(uint32_t);

			v3VertexVector.clear();
			v3VertexVector.resize(rFrameData.dwVertexCount);
			iIndexVector.clear();
			iIndexVector.resize(rFrameData.dwIndexCount);
			v3TextureVertexVector.clear();
			v3TextureVertexVector.resize(rFrameData.dwTextureVertexCount);
			iTextureIndexVector.clear();
			iTextureIndexVector.resize(rFrameData.dwIndexCount);

			memcpy(v3VertexVector.data(), c_pbBuf, rFrameData.dwVertexCount*sizeof(D3DXVECTOR3));
			c_pbBuf += rFrameData.dwVertexCount*sizeof(D3DXVECTOR3);
			memcpy(iIndexVector.data(), c_pbBuf, rFrameData.dwIndexCount*sizeof(int));
			c_pbBuf += rFrameData.dwIndexCount*sizeof(int);
			memcpy(v3TextureVertexVector.data(), c_pbBuf, rFrameData.dwTextureVertexCount*sizeof(D3DXVECTOR2));
			c_pbBuf += rFrameData.dwTextureVertexCount*sizeof(D3DXVECTOR2);
			memcpy(iTextureIndexVector.data(), c_pbBuf, rFrameData.dwIndexCount*sizeof(int));
			c_pbBuf += rFrameData.dwIndexCount*sizeof(int);

			///////////////////////////////

			rFrameData.PDTVertexVector.clear();
			rFrameData.PDTVertexVector.resize(rFrameData.dwIndexCount);
			for (uint32_t j = 0; j < rFrameData.dwIndexCount; ++j)
			{
				TPTVertex & rVertex = rFrameData.PDTVertexVector[j];

				uint32_t dwIndex = iIndexVector[j];
				uint32_t dwTextureIndex = iTextureIndexVector[j];

				assert(dwIndex < v3VertexVector.size());
				assert(dwTextureIndex < v3TextureVertexVector.size());

				rVertex.position = v3VertexVector[dwIndex];
				rVertex.texCoord = v3TextureVertexVector[dwTextureIndex];
				rVertex.texCoord.y *= -1;
			}
		}

		////////////////////////////////////

		pMeshData->pImageVector.clear();

		std::string strExtension;
		GetFileExtension(pMeshData->szDiffuseMapFileName, strlen(pMeshData->szDiffuseMapFileName), &strExtension);
		stl_lowers(strExtension);

		if ("ifl" == strExtension)
		{
			const void* pMotionData;
			CMappedFile File;

			if (CEterPackManager::Instance().Get(File, pMeshData->szDiffuseMapFileName, &pMotionData))
			{
				CMemoryTextFileLoader textFileLoader;
				std::vector<std::string> stTokenVector;

				textFileLoader.Bind(File.Size(), pMotionData);

				std::string strPathName;
				GetOnlyPathName(pMeshData->szDiffuseMapFileName, strPathName);

				std::string strTextureFileName;
				for (uint32_t i = 0; i < textFileLoader.GetLineCount(); ++i)
				{
					const std::string & c_rstrFileName = textFileLoader.GetLineString(i);

					if (c_rstrFileName.empty())
						continue;

					strTextureFileName = strPathName;
					strTextureFileName += c_rstrFileName;

					auto pImage = dynamic_cast<CGraphicImage*>(CResourceManager::Instance().GetResourcePointer(strTextureFileName.c_str()));

					pMeshData->pImageVector.push_back(pImage);
				}
			}
		}
		else
		{
			auto pImage = dynamic_cast<CGraphicImage*>(CResourceManager::Instance().GetResourcePointer(pMeshData->szDiffuseMapFileName));

			pMeshData->pImageVector.push_back(pImage);
		}

		////////////////////////////////////

		m_pEffectMeshDataVector[n] = pMeshData;
	}

	return TRUE;
}

bool CEffectMesh::__LoadData_Ver001(int iSize, const uint8_t* c_pbBuf)
{
	std::vector<D3DXVECTOR3> v3VertexVector;
	std::vector<int> iIndexVector;
	std::vector<D3DXVECTOR2> v3TextureVertexVector;
	std::vector<int> iTextureIndexVector;

	m_iGeomCount = *(int *)c_pbBuf;
	c_pbBuf += 4;
	m_iFrameCount = *(int *)c_pbBuf;
	c_pbBuf += 4;

	m_pEffectMeshDataVector.clear();
	m_pEffectMeshDataVector.resize(m_iGeomCount);

	for (int n = 0; n < m_iGeomCount; ++n)
	{
		SEffectMeshData * pMeshData = SEffectMeshData::New();

		memcpy(pMeshData->szObjectName, c_pbBuf, 32);
		c_pbBuf += 32;
		memcpy(pMeshData->szDiffuseMapFileName, c_pbBuf, 128);
		c_pbBuf += 128;

		//

		uint32_t dwVertexCount;
		uint32_t dwIndexCount;
		uint32_t dwTextureVertexCount;

		memcpy(&dwVertexCount, c_pbBuf, sizeof(uint32_t));
		c_pbBuf += sizeof(uint32_t);

		memcpy(&dwIndexCount, c_pbBuf, sizeof(uint32_t));
		c_pbBuf += sizeof(uint32_t);

		memcpy(&dwTextureVertexCount, c_pbBuf, sizeof(uint32_t));
		c_pbBuf += sizeof(uint32_t);

		pMeshData->EffectFrameDataVector.clear();
		pMeshData->EffectFrameDataVector.resize(m_iFrameCount);

		for(int i = 0; i < m_iFrameCount; ++i)
		{
			TEffectFrameData & rFrameData = pMeshData->EffectFrameDataVector[i];

			rFrameData.dwVertexCount = dwVertexCount;
			rFrameData.dwIndexCount = dwIndexCount;
			rFrameData.dwTextureVertexCount = dwTextureVertexCount;

			v3VertexVector.clear();
			v3VertexVector.resize(rFrameData.dwVertexCount);
			iIndexVector.clear();
			iIndexVector.resize(rFrameData.dwIndexCount);
			v3TextureVertexVector.clear();
			v3TextureVertexVector.resize(rFrameData.dwTextureVertexCount);
			iTextureIndexVector.clear();
			iTextureIndexVector.resize(rFrameData.dwIndexCount);

			memcpy(&rFrameData.fVisibility, c_pbBuf, sizeof(float));
			c_pbBuf += sizeof(float);
			memcpy(v3VertexVector.data(), c_pbBuf, rFrameData.dwVertexCount*sizeof(D3DXVECTOR3));
			c_pbBuf += rFrameData.dwVertexCount*sizeof(D3DXVECTOR3);
			memcpy(iIndexVector.data(), c_pbBuf, rFrameData.dwIndexCount*sizeof(int));
			c_pbBuf += rFrameData.dwIndexCount*sizeof(int);
			memcpy(v3TextureVertexVector.data(), c_pbBuf, rFrameData.dwTextureVertexCount*sizeof(D3DXVECTOR2));
			c_pbBuf += rFrameData.dwTextureVertexCount*sizeof(D3DXVECTOR2);
			memcpy(iTextureIndexVector.data(), c_pbBuf, rFrameData.dwIndexCount*sizeof(int));
			c_pbBuf += rFrameData.dwIndexCount*sizeof(int);

			///////////////////////////////

			rFrameData.PDTVertexVector.clear();
			rFrameData.PDTVertexVector.resize(rFrameData.dwIndexCount);
			for (uint32_t j = 0; j < rFrameData.dwIndexCount; ++j)
			{
				TPTVertex & rVertex = rFrameData.PDTVertexVector[j];

				uint32_t dwIndex = iIndexVector[j];
				uint32_t dwTextureIndex = iTextureIndexVector[j];

				assert(dwIndex < v3VertexVector.size());
				assert(dwTextureIndex < v3TextureVertexVector.size());

				rVertex.position = v3VertexVector[dwIndex];
				rVertex.texCoord = v3TextureVertexVector[dwTextureIndex];
				rVertex.texCoord.y *= -1;
			}
		}

		////////////////////////////////////

		pMeshData->pImageVector.clear();

		std::string strExtension;
		GetFileExtension(pMeshData->szDiffuseMapFileName, strlen(pMeshData->szDiffuseMapFileName), &strExtension);
		stl_lowers(strExtension);

		if (0 == strExtension.compare("ifl"))
		{
			const void* pMotionData;
			CMappedFile File;

			if (CEterPackManager::Instance().Get(File, pMeshData->szDiffuseMapFileName, &pMotionData))
			{
				CMemoryTextFileLoader textFileLoader;
				std::vector<std::string> stTokenVector;

				textFileLoader.Bind((int)File.Size(), pMotionData);

				std::string strPathName;
				GetOnlyPathName(pMeshData->szDiffuseMapFileName, strPathName);

				std::string strTextureFileName;
				for (uint32_t i = 0; i < textFileLoader.GetLineCount(); ++i)
				{
					const std::string & c_rstrFileName = textFileLoader.GetLineString(i);

					if (c_rstrFileName.empty())
						continue;

					strTextureFileName = strPathName;
					strTextureFileName += c_rstrFileName;

					auto pImage = dynamic_cast<CGraphicImage*>(CResourceManager::Instance().GetResourcePointer(strTextureFileName.c_str()));

					pMeshData->pImageVector.push_back(pImage);
				}
			}
		}
		else
		{
			auto pImage = dynamic_cast<CGraphicImage*>(CResourceManager::Instance().GetResourcePointer(pMeshData->szDiffuseMapFileName));

			pMeshData->pImageVector.push_back(pImage);
		}

		////////////////////////////////////

		m_pEffectMeshDataVector[n] = pMeshData;
	}

	return TRUE;
}

bool CEffectMesh::GetMeshElementPointer(uint32_t dwMeshIndex, TEffectMeshData ** ppMeshData) const
{
	if (dwMeshIndex >= m_pEffectMeshDataVector.size())
		return FALSE;

	*ppMeshData = m_pEffectMeshDataVector[dwMeshIndex];

	return TRUE;
}

void CEffectMesh::OnClear()
{
	if (!m_isData)
		return;

	for (const auto& i : m_pEffectMeshDataVector)
	{
		i->pImageVector.clear();
		i->EffectFrameDataVector.clear();

		SEffectMeshData::Delete(i);
	}
	m_pEffectMeshDataVector.clear();

	m_isData = false;
}

bool CEffectMesh::OnIsEmpty() const
{
	return !m_isData;
}

CEffectMesh::CEffectMesh(const char * c_szFileName) : CResource(c_szFileName)
{
	m_iGeomCount = 0;
	m_iFrameCount = 0;
	m_isData = false;
}

CEffectMesh::~CEffectMesh()
= default;

///////////////////////////////////////////////////////////////////////////////////////////////////////

CDynamicPool<CEffectMeshScript> CEffectMeshScript::ms_kPool;

void CEffectMeshScript::DestroySystem()
{
	ms_kPool.Destroy();
}

CEffectMeshScript* CEffectMeshScript::New()
{
	return ms_kPool.Alloc();
}

void CEffectMeshScript::Delete(CEffectMeshScript* pkData)
{
	pkData->Clear();
	ms_kPool.Free(pkData);
}

void CEffectMeshScript::ReserveMeshData(uint32_t dwMeshCount)
{
	if (m_MeshDataVector.size() == dwMeshCount)
		return;

	m_MeshDataVector.clear();
	m_MeshDataVector.resize(dwMeshCount);

	for (auto& rMeshData : m_MeshDataVector)
	{
		rMeshData.byBillboardType = MESH_BILLBOARD_TYPE_NONE;
		rMeshData.bBlendingEnable = TRUE;
		rMeshData.byBlendingSrcType = D3DBLEND_SRCCOLOR;
		rMeshData.byBlendingDestType = D3DBLEND_ONE;
		rMeshData.bTextureAlphaEnable = FALSE;

		rMeshData.byColorOperationType = D3DTOP_MODULATE;
		rMeshData.ColorFactor = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);

		rMeshData.bTextureAnimationLoopEnable = true;
		rMeshData.fTextureAnimationFrameDelay = 0.02f;
		rMeshData.dwTextureAnimationStartFrame = 0;
	}
}

const char * CEffectMeshScript::GetMeshFileName() const
{
	return m_strMeshFileName.c_str();
}

bool CEffectMeshScript::CheckMeshIndex(uint32_t dwMeshIndex) const
{
	if (dwMeshIndex >= m_MeshDataVector.size())
		return false;

	return true;
}

bool CEffectMeshScript::GetMeshDataPointer(uint32_t dwMeshIndex, TMeshData ** ppMeshData)
{
	if (!CheckMeshIndex(dwMeshIndex))
		return false;

	*ppMeshData = &m_MeshDataVector[dwMeshIndex];

	return true;
}

int CEffectMeshScript::GetMeshDataCount() const
{
	return static_cast<int>(m_MeshDataVector.size());
}

int CEffectMeshScript::GetBillboardType(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return 0;

	return m_MeshDataVector[dwMeshIndex].byBillboardType;
}
bool CEffectMeshScript::isBlendingEnable(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return FALSE;

	return m_MeshDataVector[dwMeshIndex].bBlendingEnable;
}
uint8_t CEffectMeshScript::GetBlendingSrcType(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return false;

	return m_MeshDataVector[dwMeshIndex].byBlendingSrcType;
}
uint8_t CEffectMeshScript::GetBlendingDestType(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return false;

	return m_MeshDataVector[dwMeshIndex].byBlendingDestType;
}
bool CEffectMeshScript::isTextureAlphaEnable(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return false;

	return m_MeshDataVector[dwMeshIndex].bTextureAlphaEnable;
}

bool CEffectMeshScript::GetColorOperationType(uint32_t dwMeshIndex, uint8_t* pbyType) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return FALSE;

	*pbyType = m_MeshDataVector[dwMeshIndex].byColorOperationType;

	return TRUE;
}
bool CEffectMeshScript::GetColorFactor(uint32_t dwMeshIndex, D3DXCOLOR * pColor) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return FALSE;

	*pColor = m_MeshDataVector[dwMeshIndex].ColorFactor;

	return TRUE;
}

bool CEffectMeshScript::GetTimeTableAlphaPointer(uint32_t dwMeshIndex, TTimeEventTableFloat ** pTimeEventAlpha)
{
	if (!CheckMeshIndex(dwMeshIndex))
		return FALSE;

	*pTimeEventAlpha = &m_MeshDataVector[dwMeshIndex].TimeEventAlpha;

	return TRUE;
}


bool CEffectMeshScript::isMeshAnimationLoop() const
{
	return m_isMeshAnimationLoop;
}
int CEffectMeshScript::GetMeshAnimationLoopCount() const
{
	return m_iMeshAnimationLoopCount;
}
float CEffectMeshScript::GetMeshAnimationFrameDelay() const
{
	return m_fMeshAnimationFrameDelay;
}

bool CEffectMeshScript::isTextureAnimationLoop(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return 0.0f;

	return m_MeshDataVector[dwMeshIndex].bTextureAnimationLoopEnable;
}
float CEffectMeshScript::GetTextureAnimationFrameDelay(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return 0.0f;

	return m_MeshDataVector[dwMeshIndex].fTextureAnimationFrameDelay;
}

uint32_t CEffectMeshScript::GetTextureAnimationStartFrame(uint32_t dwMeshIndex) const
{
	if (!CheckMeshIndex(dwMeshIndex))
		return 0;

	return m_MeshDataVector[dwMeshIndex].dwTextureAnimationStartFrame;
}

bool CEffectMeshScript::OnLoadScript(CTextFileLoader & rTextFileLoader)
{
	if (rTextFileLoader.GetTokenString("meshfilename", &m_strMeshFileName))
	{
		if (!IsGlobalFileName(m_strMeshFileName.c_str()))
		{
			m_strMeshFileName = GetOnlyPathName(rTextFileLoader.GetFileName()) + m_strMeshFileName;
		}
	}
	else
	{
		return FALSE;
	}

	if (!rTextFileLoader.GetTokenBoolean("meshanimationloopenable", &m_isMeshAnimationLoop))
		return FALSE;
	if (!rTextFileLoader.GetTokenInteger("meshanimationloopcount", &m_iMeshAnimationLoopCount))
	{
		m_iMeshAnimationLoopCount = 0;
	}
	if (!rTextFileLoader.GetTokenFloat("meshanimationframedelay", &m_fMeshAnimationFrameDelay))
		return FALSE;

	uint32_t dwMeshElementCount;
	if (!rTextFileLoader.GetTokenDoubleWord("meshelementcount", &dwMeshElementCount))
		return FALSE;

	m_MeshDataVector.clear();
	m_MeshDataVector.resize(dwMeshElementCount);
	for (DWORD i = 0; i < m_MeshDataVector.size(); ++i)
	{
		CTextFileLoader::CGotoChild GotoChild(&rTextFileLoader, i);

		TMeshData & rMeshData = m_MeshDataVector[i];

		if (!rTextFileLoader.GetTokenByte("billboardtype", &rMeshData.byBillboardType))
			return FALSE;
		if (!rTextFileLoader.GetTokenBoolean("blendingenable", &rMeshData.bBlendingEnable))
			return FALSE;
		if (!rTextFileLoader.GetTokenByte("blendingsrctype", &rMeshData.byBlendingSrcType))
			return FALSE;
		if (!rTextFileLoader.GetTokenByte("blendingdesttype", &rMeshData.byBlendingDestType))
			return FALSE;

		if (!rTextFileLoader.GetTokenBoolean("textureanimationloopenable", &rMeshData.bTextureAnimationLoopEnable))
			return FALSE;
		if (!rTextFileLoader.GetTokenFloat("textureanimationframedelay", &rMeshData.fTextureAnimationFrameDelay))
			return FALSE;
		if (!rTextFileLoader.GetTokenDoubleWord("textureanimationstartframe", &rMeshData.dwTextureAnimationStartFrame))
		{
			rMeshData.dwTextureAnimationStartFrame = 0;
		}

		if (!rTextFileLoader.GetTokenByte("coloroperationtype", &rMeshData.byColorOperationType))
		{
			rMeshData.byColorOperationType = D3DTOP_MODULATE;
		}
		if (!rTextFileLoader.GetTokenColor("colorfactor", &rMeshData.ColorFactor))
		{
			rMeshData.ColorFactor = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
		}

		if (!GetTokenTimeEventFloat(rTextFileLoader, "timeeventalpha", &rMeshData.TimeEventAlpha))
		{
			rMeshData.TimeEventAlpha.clear();
		}
	}

	return true;
}

bool CEffectMeshScript::OnIsData()
{
	if (m_strMeshFileName.empty())
		return false;

	return true;
}

void CEffectMeshScript::OnClear()
{
	m_isMeshAnimationLoop = false;
	m_iMeshAnimationLoopCount = 0;
	m_fMeshAnimationFrameDelay = 0.02f;

	m_MeshDataVector.clear();
	m_strMeshFileName = "";
}

CEffectMeshScript::CEffectMeshScript()
= default;

CEffectMeshScript::~CEffectMeshScript()
= default;
