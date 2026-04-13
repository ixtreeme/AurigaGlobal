#include "StdAfx.h"

#include "../Base/Stl.h"
#include "CWikiRenderTargetManager.h"

CWikiRenderTargetManager::~CWikiRenderTargetManager() { InitializeData(); }
CWikiRenderTargetManager::CWikiRenderTargetManager() { InitializeData(); }

/*----------------------------
--------PUBLIC CLASS FUNCTIONS
-----------------------------*/

std::shared_ptr<CWikiRenderTarget> CWikiRenderTargetManager::GetRenderTarget(const int module_id)
{
	const auto it = m_renderTargets.find(module_id);
	if (it != m_renderTargets.end())
		return it->second;

	return nullptr;
}

bool CWikiRenderTargetManager::CreateRenderTarget( int module_id,  int width,  int height)
{
	if (module_id < 1 /*(CPythonWikiRenderTarget::START_MODULE)*/ || GetRenderTarget(module_id))
		return false;

	m_renderTargets.emplace(module_id, std::make_shared<CWikiRenderTarget>(width, height));
	return true;
}

void CWikiRenderTargetManager::DeleteRenderTarget(const int module_id)
{
	if (!GetRenderTarget(module_id))
		return;
	
	const auto it = m_renderTargets.find(module_id);
	if (it != m_renderTargets.end())
		m_renderTargets.erase(it);
}

void CWikiRenderTargetManager::CreateRenderTargetTextures()
{
	for (const auto& elem : m_renderTargets)
		elem.second->CreateTextures();
}

void CWikiRenderTargetManager::ReleaseRenderTargetTextures()
{
	for (const auto& elem : m_renderTargets)
		elem.second->ReleaseTextures();
}

void CWikiRenderTargetManager::DeformModels()
{
	for (const auto& elem : m_renderTargets)
		elem.second->DeformModel();
}

void CWikiRenderTargetManager::UpdateModels()
{
	for (auto& elem : m_renderTargets)
		elem.second->UpdateModel();
}

void CWikiRenderTargetManager::RenderBackgrounds()
{
	for (const auto& elem : m_renderTargets)
		elem.second->RenderBackground();
}

void CWikiRenderTargetManager::RenderModels()
{
	for (const auto& elem : m_renderTargets)
		elem.second->RenderModel();
}
