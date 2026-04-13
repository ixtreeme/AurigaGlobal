#include "StdAfx.h"
#include "../Base/Debug.h"
#include "Thing.h"
#include "ThingInstance.h"

CGraphicThing::CGraphicThing(const char* c_szFileName) : CResource(c_szFileName)
{
	Initialize();
}

CGraphicThing::~CGraphicThing()
{
	Clear();
}

void CGraphicThing::Initialize()
{
	m_pgrnFile = nullptr;
	m_pgrnFileInfo = nullptr;
	m_pgrnAni = nullptr;

	m_models = nullptr;
	m_motions.clear();
	
}

void CGraphicThing::OnClear()
{
	/*if (m_motions)
		delete [] m_motions;*/

	/*if (m_models)
		delete [] m_models;*/

	if (m_pgrnFile)
		GrannyFreeFile(m_pgrnFile);

	Initialize();
}

CGraphicThing::TType CGraphicThing::Type()
{
	static TType s_type = StringToType("CGraphicThing");
	return s_type;
}

bool CGraphicThing::OnIsEmpty() const
{
	return m_pgrnFile ? false : true;
}

bool CGraphicThing::OnIsType(const TType type)
{
	if (Type() == type)
		return true;

	return CResource::OnIsType(type);
}

bool CGraphicThing::CreateDeviceObjects()
{
	if (!m_pgrnFileInfo)
		return true;

	for (int m = 0; m < m_pgrnFileInfo->ModelCount; ++m)
	{
		CGrannyModel& rModel = m_models[m];
		rModel.CreateDeviceObjects();
	}

	return true;
}

void CGraphicThing::DestroyDeviceObjects()
{
	if (!m_pgrnFileInfo)
		return;

	for (int m = 0; m < m_pgrnFileInfo->ModelCount; ++m)
	{
		CGrannyModel& rModel = m_models[m];
		rModel.DestroyDeviceObjects();
	}
}

bool CGraphicThing::CheckModelIndex(const int iModel) const
{
	if (!m_pgrnFileInfo)
	{
		Tracef("m_pgrnFileInfo == NULL: %s\n", GetFileName());
		return false;
	}

	assert(m_pgrnFileInfo != NULL);

	if (iModel < 0)
		return false;

	if (iModel >= m_pgrnFileInfo->ModelCount)
		return false;

	return true;
}

bool CGraphicThing::CheckMotionIndex(const int iMotion) const
{
	if (!m_pgrnFileInfo)
		return false;

	assert(m_pgrnFileInfo != NULL);

	if (iMotion < 0)
		return false;

	if (iMotion >= m_pgrnFileInfo->AnimationCount)
		return false;

	return true;
}

CGrannyModel* CGraphicThing::GetModelPointer(const int iModel) const
{
	assert(CheckModelIndex(iModel));
	assert(m_models != NULL);
	return m_models.get() + iModel;
}

std::shared_ptr<CGrannyMotion> CGraphicThing::GetMotionPointer(const int iMotion)
{
	if (!CheckMotionIndex(iMotion))
		return nullptr;

	if (iMotion >= static_cast<int>(m_motions.size()))
		return nullptr;
	return m_motions[iMotion];
}

#ifdef ENABLE_3D_MODELS_TEXTURE_FIX
int CGraphicThing::GetTextureCount() const
{
	if (!m_pgrnFileInfo)
		return 0;

	if (m_pgrnFileInfo->TextureCount <= 0)
		return 0;

	return (m_pgrnFileInfo->TextureCount);
}

const char* CGraphicThing::GetTexturePath(int iTexture)
{
	if (iTexture >= GetTextureCount())
		return "";

	return m_pgrnFileInfo->Textures[iTexture]->FromFileName;
}
#endif

int CGraphicThing::GetModelCount() const
{
	if (!m_pgrnFileInfo)
		return 0;

	return (m_pgrnFileInfo->ModelCount);
}

int CGraphicThing::GetMotionCount() const
{
	if (!m_pgrnFileInfo)
		return 0;

	return (m_pgrnFileInfo->AnimationCount);
}

bool CGraphicThing::OnLoad(const int iSize, const void* c_pvBuf)
{
	if (!c_pvBuf)
		return false;

	m_pgrnFile = GrannyReadEntireFileFromMemory(iSize, c_pvBuf);

	if (!m_pgrnFile)
		return false;

	m_pgrnFileInfo = GrannyGetFileInfo(m_pgrnFile);

	if (!m_pgrnFileInfo)
		return false;

	LoadModels();
	return LoadMotions();
	return true;
}

std::string gs_modelLocalPath;

const std::string& GetModelLocalPath()
{
	return gs_modelLocalPath;
}

bool CGraphicThing::LoadModels()
{
	assert(m_pgrnFile != NULL);
	assert(m_models == NULL);

	if (m_pgrnFileInfo->ModelCount <= 0)
		return false;

	const std::string& fileName = GetFileNameString();

	if (fileName.length() > 2 && fileName[1] != ':')
	{
		const int sepPos = static_cast<const int>(fileName.rfind('\\'));
		gs_modelLocalPath.assign(fileName, 0, sepPos + 1);
	}

	const int modelCount = m_pgrnFileInfo->ModelCount;

	//m_models = new CGrannyModel[modelCount];

	m_models = std::make_unique<CGrannyModel[]>(modelCount);
	for (int m = 0; m < modelCount; ++m)
	{
		CGrannyModel& rModel = m_models[m];
		granny_model* pgrnModel = m_pgrnFileInfo->Models[m];
		if (!rModel.CreateFromGrannyModelPointer(pgrnModel))
			return false;

	}


	GrannyFreeFileSection(m_pgrnFile, GrannyStandardRigidVertexSection);
	GrannyFreeFileSection(m_pgrnFile, GrannyStandardRigidIndexSection);
	GrannyFreeFileSection(m_pgrnFile, GrannyStandardDeformableIndexSection);
	GrannyFreeFileSection(m_pgrnFile, GrannyStandardTextureSection);
	return true;
}

bool CGraphicThing::LoadMotions()
{
	assert(m_pgrnFile != NULL);
	assert(m_motions.empty());

	if (m_pgrnFileInfo->AnimationCount <= 0)
		return false;

	const int motionCount = m_pgrnFileInfo->AnimationCount;



	for (int m = 0; m < motionCount; ++m) {

		auto motion = std::make_shared<CGrannyMotion>(); 

		if (!motion->BindGrannyAnimation(m_pgrnFileInfo->Animations[m]))
			return false;

		m_motions.push_back(motion);
	}
	return true;
}
