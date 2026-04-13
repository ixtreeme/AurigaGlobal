#pragma once

#include <unordered_map>
#include <cstdint>
#include <memory>

#include "../Base/Singleton.h"
#include "../Game/in_game_wiki.h"

#include "CWikiRenderTarget.h"

class CWikiRenderTargetManager : public CSingleton<CWikiRenderTargetManager>
{
	public:
		CWikiRenderTargetManager();
		virtual ~CWikiRenderTargetManager();
		void InitializeData() { m_renderTargets.clear(); }
	
	public:
		std::shared_ptr<CWikiRenderTarget>	GetRenderTarget(int module_id);
		bool								CreateRenderTarget(int module_id, int width, int height);
		void								DeleteRenderTarget(int module_id);
		
		void								CreateRenderTargetTextures();
		void								ReleaseRenderTargetTextures();
		
		void								DeformModels();
		void								UpdateModels();
		void								RenderBackgrounds();
		void								RenderModels();
	
	protected:
		std::unordered_map<int, std::shared_ptr<CWikiRenderTarget>>	m_renderTargets;
};
