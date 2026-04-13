#pragma once

#include <filesystem>
#include <unordered_map>
#include <cassert>

#include <granny.h>


class CModel
{
protected:
	friend class CModelManager;

	CModel();
	virtual ~CModel();

	bool Load(const std::filesystem::path& path);
	void Destroy();

public:
	static bool IsGrannyFile(const std::filesystem::path& path);
	static bool	IsGrannyModelFile(const std::filesystem::path& path);

public:
	granny_model*		GetModel() const { return m_model; }

private:
	granny_model*			m_model;
	granny_file*			m_file;

};

class CModelManager
{
protected:
	CModelManager();

public:
	typedef	std::unordered_map<std::string, CModel*>	TModelCache;	

public:
	virtual ~CModelManager();
	static CModelManager& Instance()
	{
		static CModelManager instance;
		return instance;
	}

	CModel*		RegisterModel(const std::filesystem::path& path);
	CModel*		GetModel(const std::filesystem::path& path);

	CModel*		AutoRegisterAndGetModel(const std::filesystem::path& path, int findDepth = 1);
	
	void		Destroy();

private:
	TModelCache		m_modelMap;

};
