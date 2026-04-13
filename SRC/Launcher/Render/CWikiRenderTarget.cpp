#include "StdAfx.h"

#include "../Render/Camera.h"
#include "../Render/CWikiRenderTargetManager.h"
#include "../Python/PythonGraphic.h"
#include "../Base/CRC32.h"
#include "../Game/GameType.h"
#include "../Game/MapType.h"
#include "../Game/ItemData.h"
#include "../Game/ActorInstance.h"
#include "../UserInterface/InstanceBase.h"

#include "ResourceManager.h"
#include "CWikiRenderTarget.h"

CWikiRenderTarget::~CWikiRenderTarget() = default;

CWikiRenderTarget::CWikiRenderTarget(const int width, const int height) :
																		m_pModel(nullptr),
																		m_background(nullptr),
																		m_renderTargetTexture(nullptr),
																		m_modelRotation(0),
																		m_visible(false),
																		m_v3Eye(0.0f, 0.0f, 0.0f),
																		m_v3Target(0.0f, 0.0f, 0.0f),
																		m_v3Up(0.0f, 0.0f, 1.0f)
{
	auto pTex = new CGraphicWikiRenderTargetTexture;
	
	if (!pTex->Create(width, height, D3DFMT_X8R8G8B8, D3DFMT_D16)) {
		TraceError("CWikiRenderTarget::CWikiRenderTarget: Could not create CGraphicRenderTargetTexture %dx%d", width, height);
		delete pTex;
		
		throw std::runtime_error("CWikiRenderTarget::CWikiRenderTarget: Could not create CGraphicRenderTargetTexture");
	}
	
	m_renderTargetTexture = std::unique_ptr<CGraphicWikiRenderTargetTexture>(pTex);
}

/*----------------------------
--------PUBLIC CLASS FUNCTIONS
-----------------------------*/

void CWikiRenderTarget::CreateTextures() const
{
	if (!m_renderTargetTexture)
		return;
	
	m_renderTargetTexture->CreateTextures();
}

void CWikiRenderTarget::ReleaseTextures() const
{
	if (!m_renderTargetTexture)
		return;
	
	m_renderTargetTexture->ReleaseTextures();
}

void CWikiRenderTarget::RenderTexture() const
{
	if (!m_renderTargetTexture)
		return;
	
	m_renderTargetTexture->Render();
}

void CWikiRenderTarget::SetRenderingRect(RECT* rect) const
{
	if (!m_renderTargetTexture)
		return;
	
	m_renderTargetTexture->SetRenderingRect(rect);
}

void CWikiRenderTarget::SetRenderingBox(RECT* renderBox) const
{
	if (!m_renderTargetTexture)
		return;
	
	m_renderTargetTexture->SetRenderingBox(renderBox);
}

void CWikiRenderTarget::SelectModel(const unsigned int model_vnum)
{
	//CreateBackground();
	CInstanceBase::SCreateData kCreateData{};
	
	kCreateData.m_bType = CActorInstance::TYPE_PC;
	kCreateData.m_dwRace = model_vnum;
	
	auto model = std::make_unique<CInstanceBase>();
	if (!model->Create(kCreateData, true))
	{
		if (m_pModel)
			m_pModel.reset();
		return;
	}
	
	m_pModel = std::move(model);
	m_pModel->NEW_SetPixelPosition(TPixelPosition(0,0,0));
	m_pModel->GetGraphicThingInstancePtr()->ClearAttachingEffect();
	m_pModel->Refresh(CRaceMotionData::NAME_WAIT, true);
	m_pModel->SetLoopMotion(CRaceMotionData::NAME_WAIT);
	m_pModel->SetAlwaysRender(true);
	m_pModel->SetRotation(0.0f);
	
	float fTargetHeight = 110.0f;
	
	m_v3Eye.x = 0.0f;
	m_v3Eye.y = -1000.0f;
	m_v3Eye.z = 600.0f;
	
	m_v3Target.x = 0.0f;
	m_v3Target.y = 0.0f;
	m_v3Target.z = 95.0f;
	
	if (model_vnum > (NRaceData::JOB_MAX_NUM * 2))
	{
		auto ptrGraphicInstance = m_pModel->GetGraphicThingInstancePtr();
		if (ptrGraphicInstance) {
			float fRaceHeight = ptrGraphicInstance->GetHeight();
			if (model_vnum == 34082 || model_vnum == 34102) {
				fRaceHeight += 60.0f;
			}

			fTargetHeight = fRaceHeight / 2.0f;
			if (model_vnum == 34082 || 
				model_vnum == 34102) {
				m_v3Eye.y = -(fRaceHeight * 5.9f);
				m_v3Target.z = fRaceHeight / 2.0f;
			} else if (model_vnum == 34036 || 
				model_vnum == 34093 || 
				model_vnum == 34094 || 
				model_vnum == 34107
			) {
				fTargetHeight = 120.0f;
				m_v3Eye.y = -2200.0f;
				m_v3Target.z = 120.0f;
			} else {
				m_v3Eye.y = -(fRaceHeight * 8.9f);
				m_v3Target.z = fRaceHeight / 2.0f;
			}

			m_v3Eye.z = 0.0f;
		}
	} else {
		auto ptrGraphicInstance = m_pModel->GetGraphicThingInstancePtr();
		if (ptrGraphicInstance) {
			float fRaceHeight = ptrGraphicInstance->GetHeight();
			fTargetHeight = fRaceHeight / 2.0f;
			m_v3Eye.y = -(fRaceHeight * 6.9f);
			m_v3Target.z = fRaceHeight/2.0f;
		}
	}
	
	auto& camera_manager = CCameraManager::instance();
	camera_manager.SetCurrentCamera(CCameraManager::INGAME_WIKI_CAMERA);
	camera_manager.GetCurrentCamera()->SetTargetHeight(fTargetHeight);
	camera_manager.ResetToPreviousCamera();
	
	m_modelRotation = 0.0f;
}

void CWikiRenderTarget::UpdateModel()
{
	if (!m_visible || !m_pModel)
		return;
	
	if (m_modelRotation < 360.0f)
		m_modelRotation += 1.0f;
	else
		m_modelRotation = 0.0f;
	
	m_pModel->SetRotation(m_modelRotation);
	m_pModel->Transform();
	m_pModel->GetGraphicThingInstanceRef().RotationProcess();
}

void CWikiRenderTarget::DeformModel() const
{
	if (!m_visible)
		return;

	if (m_pModel)
		m_pModel->Deform();
}

void CWikiRenderTarget::RenderModel() const
{
	if (!m_visible)
		return;
	
	auto& python_graphic = CPythonGraphic::Instance();
	auto& camera_manager = CCameraManager::instance();
	auto& state_manager = CStateManager::Instance();
	auto& rectRender = *m_renderTargetTexture->GetRenderingRect();
	
	if (!m_pModel)
		return;
	
	m_renderTargetTexture->SetRenderTarget();
	if (!m_background) {
		m_renderTargetTexture->Clear();
	}
	
	python_graphic.ClearDepthBuffer();
	
	const auto fov = python_graphic.GetFOV();
	const auto aspect = python_graphic.GetAspect();
	const auto near_y = python_graphic.GetNear();
	const auto far_y = python_graphic.GetFar();
	
	const auto width = static_cast<float>(rectRender.right - rectRender.left);
	const auto height = static_cast<float>(rectRender.bottom - rectRender.top);
	
	state_manager.SetRenderState(D3DRS_FOGENABLE, FALSE);
	
	python_graphic.SetViewport(0.0f, 0.0f, width, height);
	python_graphic.PushState();
	
	camera_manager.SetCurrentCamera(CCameraManager::INGAME_WIKI_CAMERA);
	if (!camera_manager.GetCurrentCamera()->IsDraging())
		camera_manager.GetCurrentCamera()->SetViewParams(m_v3Eye, m_v3Target, m_v3Up);
	
	python_graphic.UpdateViewMatrix();
	python_graphic.SetPerspective(10.0f, width / height, 100.0f, 15000.0f);
	
	m_pModel->Render();
	m_pModel->GetGraphicThingInstanceRef().WikiRenderAllAttachingModuleEffect();
	
	camera_manager.ResetToPreviousCamera();
	
	python_graphic.RestoreViewport();
	python_graphic.PopState();
	python_graphic.SetPerspective(fov, aspect, near_y, far_y);
	
	m_renderTargetTexture->ResetRenderTarget();
	
	state_manager.SetRenderState(D3DRS_FOGENABLE, TRUE);
}

void CWikiRenderTarget::SetArmor(unsigned int vnum)
{
	if (!m_pModel)
		return;
	
	m_pModel->ChangeArmor(vnum);
}

void CWikiRenderTarget::SetWeapon(unsigned int vnum)
{
	if (!m_pModel)
		return;
	
	m_pModel->ChangeWeapon(vnum);
}

void CWikiRenderTarget::SetHair(unsigned int vnum)
{
	if (!m_pModel)
		return;
	
	m_pModel->ChangeHair(vnum);
}

void CWikiRenderTarget::SetSash(unsigned int vnum)
{
	if (!m_pModel)
		return;
	
	m_pModel->SetAcce(vnum - 85000, true);
}

void CWikiRenderTarget::SetWeaponEffect(unsigned int vnum)
{
	if (!m_pModel)
		return;
	
	m_pModel->ChangeCostumeEffect(false, vnum);
}

void CWikiRenderTarget::SetArmorEffect(unsigned int vnum)
{
	if (!m_pModel)
		return;
	
	m_pModel->ChangeCostumeEffect(true, vnum);
}

void CWikiRenderTarget::SetModelV3Eye(float x, float y, float z)
{
	m_v3Eye.x = x;
	m_v3Eye.y = y;
	m_v3Eye.z = z;
}

void CWikiRenderTarget::SetModelV3Target(float x, float y, float z)
{
	m_v3Target.x = x;
	m_v3Target.y = y;
	m_v3Target.z = z;
}

bool CWikiRenderTarget::CreateBackground(const char* imgPath, const uint32_t width, const uint32_t height)
{
	if (m_background)
		return false;

	m_background = std::make_unique<CGraphicImageInstance>();
	
	const auto graphic_image = dynamic_cast<CGraphicImage*>(CResourceManager::instance().GetResourcePointer(imgPath));
	if (!graphic_image)
	{
		m_background.reset();
		return false;
	}

	m_background->SetImagePointer(graphic_image);
	m_background->SetScale(static_cast<float>(width) / graphic_image->GetWidth(), static_cast<float>(height) / graphic_image->GetHeight());
	return true;
}

void CWikiRenderTarget::RenderBackground() const
{
	if (!m_visible)
		return;

	if (!m_background)
		return;

	auto& rectRender = *m_renderTargetTexture->GetRenderingRect();
	m_renderTargetTexture->SetRenderTarget();

	CGraphicWikiRenderTargetTexture::Clear();
	CPythonGraphic::Instance().SetInterfaceRenderState();

	m_background->Render();
	m_renderTargetTexture->ResetRenderTarget();
}
