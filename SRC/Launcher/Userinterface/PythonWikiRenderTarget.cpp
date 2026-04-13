#include "StdAfx.h"

#include "PythonApplication.h"

CPythonWikiRenderTarget::~CPythonWikiRenderTarget() = default;

CPythonWikiRenderTarget::CPythonWikiRenderTarget()
{
	_bCanRenderModules = false;
	_RenderWikiModules.clear();
}

/*----------------------------
--------PUBLIC CLASS FUNCTIONS
-----------------------------*/

int CPythonWikiRenderTarget::GetFreeID()
{
	const size_t render_wiki_size = _RenderWikiModules.size();
	int new_module = START_MODULE;
	
	if (!render_wiki_size)
		return new_module;

	for (const auto& elem : _RenderWikiModules) {
		int i = 0;
		std::tie(i, std::ignore) = elem;
		if (i != new_module)
			break;
		
		++new_module;
	}

	for (const auto &[module_id, module_wnd] : _RenderWikiModules) {
		if (module_id != new_module)
			break;
		
		++new_module;
	}

	return new_module;
}

void CPythonWikiRenderTarget::RegisterRenderModule(int module_id, UI::CWindow* module_wnd)
{
	if (module_id == DELETE_PARM)
	{
		const auto it = std::ranges::find_if(_RenderWikiModules,
		                                     [=](const std::tuple<int, std::shared_ptr<UI::CUiWikiRenderTarget>>& _s)
		                                     {
			                                     return std::get<0>(_s) == module_id;
		                                     });

		if (it != _RenderWikiModules.end())
			_RenderWikiModules.erase(it);

		CWikiRenderTargetManager::Instance().DeleteRenderTarget(module_id);
	}
	else
	{
		if (!module_wnd)
			return;

		UI::CUiWikiRenderTarget* _hwn = (UI::CUiWikiRenderTarget*)module_wnd;
		_RenderWikiModules.emplace_back(std::make_tuple(module_id, _hwn));

		if (!_InitializeWindow(module_id, _hwn))
		{
			TraceError("RegisterRenderModule Cant Regist Module ID : %d", module_id);
			return;
		}
	}
}

void CPythonWikiRenderTarget::ManageModelViewVisibility(int module_id, bool flag)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetVisibility(flag);
}

bool CPythonWikiRenderTarget::CanRenderWikiModules() const
{
	//return true;
	bool _canRender = false;
	if (_bCanRenderModules)
	{
		const auto hWnd = CPythonApplication::Instance().GetWindowHandle();
		const auto isMinimized = static_cast<bool>(IsIconic(hWnd));
		_canRender = isMinimized ? false : true;
	}
	
	return _canRender;
}

void CPythonWikiRenderTarget::SetModelViewModel(int module_id, int module_vnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SelectModel(module_vnum);
}

void CPythonWikiRenderTarget::SetWeaponModel(int module_id, int weapon_vnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetWeapon(weapon_vnum);
}

void CPythonWikiRenderTarget::SetModelForm(int module_id, int main_vnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetArmor(main_vnum);
}

void CPythonWikiRenderTarget::SetModelHair(int module_id, int hair_vnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetHair(hair_vnum);
}

void CPythonWikiRenderTarget::SetModelSash(int module_id, int sashVnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetSash(sashVnum);
}

void CPythonWikiRenderTarget::SetModelWeaponEffect(int module_id, int weaponeffectVnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetWeaponEffect(weaponeffectVnum);
}

void CPythonWikiRenderTarget::SetModelArmorEffect(int module_id, int armoreffectVnum)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetArmorEffect(armoreffectVnum);
}

void CPythonWikiRenderTarget::SetModelV3Eye(int module_id, float x, float y, float z)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetModelV3Eye(x, y, z);
}

void CPythonWikiRenderTarget::SetModelV3Target(int module_id, float x, float y, float z)
{
	auto _Wrt = _GetRenderTargetHandle(module_id);
	if (!_Wrt)
		return;
	
	_Wrt->SetModelV3Target(x, y, z);
}

void* CPythonWikiRenderTarget::CreateBackground(int moduleID, const char* szPathName, int iWidth, int iHeight) {
	auto _Wrt = _GetRenderTargetHandle(moduleID);
	if (_Wrt)
		_Wrt->CreateBackground(szPathName, iWidth, iHeight);
	return nullptr;
}

/*----------------------------
------PROTECTED CLASS FUNCTIONS
-----------------------------*/

bool CPythonWikiRenderTarget::_InitializeWindow(int module_id, UI::CUiWikiRenderTarget* handle_window)
{
	if (!handle_window)
		return false;
	
	return handle_window->SetWikiRenderTargetModule(module_id);
}

std::shared_ptr<CWikiRenderTarget> CPythonWikiRenderTarget::_GetRenderTargetHandle(int module_id)
{
	auto it = std::find_if(_RenderWikiModules.begin(), _RenderWikiModules.end(),
			[=](const std::tuple<int, std::shared_ptr<UI::CUiWikiRenderTarget>>& _s)
			{
				return std::get<0>(_s) == module_id;
			} );
	
	if (it == _RenderWikiModules.end())
		return nullptr;
	
	return CWikiRenderTargetManager::Instance().GetRenderTarget(module_id);
}
