#include "ModelManager.h"


CModel::CModel() : m_model(nullptr), m_file(nullptr){}

CModel::~CModel()
{
	Destroy();
}

bool CModel::Load(const std::filesystem::path& path)
{
	assert(std::filesystem::is_regular_file(path));

	m_file = GrannyReadEntireFile(path.string().c_str());
	granny_file_info* m_fileInfo = GrannyGetFileInfo(m_file);
	granny_file_info* info = m_fileInfo;

	assert(1 == info->ModelCount);

	m_model = info->Models[0];

	GrannyFreeFileSection(m_file, GrannyStandardRigidVertexSection);
	GrannyFreeFileSection(m_file, GrannyStandardRigidIndexSection);
	GrannyFreeFileSection(m_file, GrannyStandardDeformableIndexSection);
	GrannyFreeFileSection(m_file, GrannyStandardTextureSection);

	return true;
}

void CModel::Destroy()
{
	if (m_file)
	{
		GrannyFreeFile(m_file);
		m_file = nullptr;
	}
}

bool CModel::IsGrannyFile(const std::filesystem::path& path)
{
	if (false == std::filesystem::is_regular_file(path))
		return false;
	auto extS = path.extension().string();
	std::ranges::transform(extS, extS.begin(), [](const unsigned char c) {return std::tolower(c); });


	return extS == ".gr2";
}

bool CModel::IsGrannyModelFile(const std::filesystem::path& path)
{
	assert(std::filesystem::is_regular_file(path) && "File not found!");

	granny_file* file = GrannyReadEntireFile(path.string().c_str());
	assert(file && "Failed to open granny file");

	granny_file_info* info = GrannyGetFileInfo(file);
	assert(file && "Failed to read granny file info");

	const bool bResult = 0 < info->ModelCount;

	GrannyFreeFile(file);

	return bResult;
}

CModelManager::CModelManager()
= default;

CModelManager::~CModelManager()
{
	Destroy();
}

CModel* CModelManager::AutoRegisterAndGetModel(const std::filesystem::path& initPath, int depth)
{
	auto curPath = initPath;

	while (depth--)
	{
		curPath = curPath.parent_path();

		for (std::filesystem::directory_iterator endIter, iter(curPath); iter != endIter; ++iter)
		{
			if (const auto & path = iter->path(); CModel::IsGrannyFile(path) && CModel::IsGrannyModelFile(path))
			{
				return this->RegisterModel(path);
			}			
		}
	}

	return nullptr;
}

CModel* CModelManager::RegisterModel(const std::filesystem::path& path)
{	
	if (false == std::filesystem::is_regular_file(path))
		return nullptr;

	const std::string key = path.parent_path().string();

	CModel* model = GetModel(key);

	if (nullptr != model)
		return model;
	
	model = new CModel();

	if (model->Load(path))
	{
		m_modelMap.insert(std::make_pair(key, model));
		return model;
	}

	delete model;

	return nullptr;
}

void CModelManager::Destroy()
{
	for (const auto& iter : m_modelMap)
	{
		CModel* model = iter.second;

		model->Destroy();
		delete model;
	}

	m_modelMap.clear();

}

CModel* CModelManager::GetModel(const std::filesystem::path& path)
{
	const std::string key = path.parent_path().string();

	const auto iter = m_modelMap.find(key);

	if (m_modelMap.end() == iter)
		return nullptr;

	return iter->second;
}