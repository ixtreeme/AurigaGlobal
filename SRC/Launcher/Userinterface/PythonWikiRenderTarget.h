#pragma once

#include "../Game/in_game_wiki.h"
#include "../Python/PythonWindow.h"
#include "../Render/CWikiRenderTargetManager.h"
#include "../Render/GrpWikiRenderTargetTexture.h"

class CPythonWikiRenderTarget final : public CSingleton<CPythonWikiRenderTarget>
{
	public:
		CPythonWikiRenderTarget();
		virtual ~CPythonWikiRenderTarget();
	
	public:
		const static	int DELETE_PARM = -1;
		const static	int START_MODULE = 1;
		
		typedef std::vector<std::tuple<int, std::shared_ptr<UI::CUiWikiRenderTarget>>> TWikiRenderTargetModules;
		
		int		GetFreeID();
		void		RegisterRenderModule(int module_id, UI::CWindow* module_wnd);
		
		void		ManageModelViewVisibility(int module_id, bool flag);
		
		void		ShowModelViewManager(bool flag) { _bCanRenderModules = flag; }
		bool		CanRenderWikiModules() const;
		
		void		SetModelViewModel(int module_id, int module_vnum);
		void		SetWeaponModel(int module_id, int weapon_vnum);
		void		SetModelForm(int module_id, int main_vnum);
		void		SetModelHair(int module_id, int hair_vnum);
		void		SetModelSash(int module_id, int sashVnum);
		void		SetModelWeaponEffect(int module_id, int weaponeffectVnum);
		void		SetModelArmorEffect(int module_id, int armoreffectVnum);
		void		SetModelV3Eye(int module_id, float x, float y, float z);
		void		SetModelV3Target(int module_id, float x, float y, float z);
		void* CreateBackground(int moduleID, const char* szPathName, int iWidth, int iHeight);

	protected:
		bool									_InitializeWindow(int module_id, UI::CUiWikiRenderTarget* handle_window);
		std::shared_ptr<CWikiRenderTarget>		_GetRenderTargetHandle(int module_id);
	
	private:
		TWikiRenderTargetModules				_RenderWikiModules;
		bool									_bCanRenderModules;
};
