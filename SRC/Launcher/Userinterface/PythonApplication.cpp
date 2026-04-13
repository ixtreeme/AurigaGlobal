#ifdef _DEBUG
//#include <vld.h>
#endif // DEBUG


#include "StdAfx.h"
#include "../Base/Error.h"
#include "../Render/Camera.h"
#include "../Render/AttributeInstance.h"
#include "../Game/AreaTerrain.h"
#include "../Granny/Material.h"

#include "../Pack/EterPackManager.h"


#include "resource.h"
#include "PythonApplication.h"
#include "PythonCharacterManager.h"
#include "../SecureLayer/obfuscate.h"

#include "../SecureLayer/ProcessScanner.h"
#ifdef ENABLE_SWITCHBOT
#include "PythonSwitchbot.h"
#endif
#ifdef LEADERBOARD_RAZOR93
#include "PythonLeaderboard.h"
#endif

extern void GrannyCreateSharedDeformBuffer();
extern void GrannyDestroySharedDeformBuffer();

float MIN_FOG = 2400.0f;
double g_specularSpd = 0.007f;

CPythonApplication* CPythonApplication::ms_pInstance;

float c_fDefaultCameraRotateSpeed = 1.5f;
float c_fDefaultCameraPitchSpeed = 1.5f;
float c_fDefaultCameraZoomSpeed = 0.05f;

CPythonApplication::CPythonApplication() : m_pGraphBuilder(nullptr),
										   m_pFilterSG(nullptr),
										   m_pMediaCtrl(nullptr),
										   m_pMediaEvent(nullptr),
										   m_pVideoWnd(nullptr),
										   m_pBasicVideo(nullptr),
										   m_pCaptureBuffer(nullptr),
										   m_lBufferSize(0),
										   m_pLogoTex(nullptr),
										   m_bLogoError(false),
										   m_bLogoPlay(false),
										   m_nLeft(0),
										   m_nRight(0),
										   m_nTop(0),
										   m_nBottom(0),
										   m_poMouseHandler(nullptr),
										   m_fAveRenderTime(0.0f),
										   m_dwCurRenderTime(0.0f),
										   m_dwCurUpdateTime(0),
										   m_dwLoad(0),
										   m_dwLastIdleTime(0),
										   m_fGlobalTime(0.0f),
										   m_fGlobalElapsedTime(0.0f),
										   m_dwUpdateFPS(0),
										   m_dwRenderFPS(0),
										   m_dwFaceCount(0),
										   m_dwLButtonDownTime(0),
										   m_dwLButtonUpTime(0),
										   m_hCurrentCursor(nullptr),
										   m_bCursorVisible(TRUE),
										   m_bLiarCursorOn(false),
										   m_iCursorMode(CURSOR_MODE_HARDWARE),
										   m_isWindowed(false),
										   m_isFrameSkipDisable(false),
										   m_dwStickyKeysFlag(0),
										   m_dwBufSleepSkipTime(0.0f),
										   m_IsMovingMainWindow(false)
{
#ifdef _DEBUG
	SetEterExceptionHandler();
#endif
	m_InitialMouseMovingPoint = {};
	CTimer::Instance().UseCustomTime();
	m_dwWidth = 800;
	m_dwHeight = 600;

	ms_pInstance = this;
	m_isWindowFullScreenEnable = FALSE;

	m_v3CenterPosition = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	m_dwStartLocalTime = ELTimer_GetMSec();
	m_tServerTime = 0;
	m_tLocalStartTime = 0;

	m_iPort = 0;
	m_iFPS = 60;

	m_isActivateWnd = false;
	m_isMinimizedWnd = true;

	m_fRotationSpeed = 0.0f;
	m_fPitchSpeed = 0.0f;
	m_fZoomSpeed = 0.0f;

	m_fFaceSpd = 0.0f;

	m_dwFaceAccCount = 0;
	m_dwFaceAccTime = 0;

	m_dwFaceSpdSum = 0;
	m_dwFaceSpdCount = 0;

	m_FlyingManager.SetMapManagerPtr(&m_pyBackground);

	m_iCursorNum = CURSOR_SHAPE_NORMAL;
	m_iContinuousCursorNum = CURSOR_SHAPE_NORMAL;

	m_isSpecialCameraMode = FALSE;
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed;

	m_iCameraMode = CAMERA_MODE_NORMAL;
	m_fBlendCameraStartTime = 0.0f;
	m_fBlendCameraBlendTime = 0.0f;

	m_iForceSightRange = -1;

	CCameraManager::Instance().AddCamera(EVENT_CAMERA_NUMBER);
}

CPythonApplication::~CPythonApplication()
{
}

void CPythonApplication::GetMousePosition(POINT* ppt)
{
	CMSApplication::GetMousePosition(ppt);
}

void CPythonApplication::SetMinFog(float fMinFog)
{
	MIN_FOG = fMinFog;
}

void CPythonApplication::SetFrameSkip(bool isEnable)
{
	if (isEnable)
		m_isFrameSkipDisable = false;
	else
		m_isFrameSkipDisable = true;
}

void CPythonApplication::GetInfo(UINT eInfo, std::string* pstInfo)
{
	switch (eInfo)
	{
	case INFO_ACTOR:
		m_kChrMgr.GetInfo(pstInfo);
		break;
	case INFO_EFFECT:
		m_kEftMgr.GetInfo(pstInfo);
		break;
	case INFO_ITEM:
		m_pyItem.GetInfo(pstInfo);
		break;
	case INFO_TEXTTAIL:
		m_pyTextTail.GetInfo(pstInfo);
		break;
	}
}

void CPythonApplication::Abort()
{
	TraceError("============================================================================================================");
	TraceError("Abort!!!!\n\n");

	PostQuitMessage(0);
}

void CPythonApplication::Exit()
{
	PostQuitMessage(0);
}

bool PERF_CHECKER_RENDER_GAME = false;

void CPythonApplication::RenderGame()
{
	if (!PERF_CHECKER_RENDER_GAME)
	{
		if (CPythonWikiRenderTarget::instance().CanRenderWikiModules()) {
			m_pyWikiModelViewManager.RenderBackgrounds();
		}

		float fAspect = m_kWndMgr.GetAspect();
		float fFarClip = m_pyBackground.GetFarClip();
#ifdef ENABLE_PERSPECTIVE_VIEW
		m_pyGraphic.SetPerspective(((m_pySystem.GetFieldPerspective() / 100.0f) * 55.0f) + 30.0f, fAspect, 100.0f, fFarClip);
#else
		m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0f, fFarClip);
#endif
		CCullingManager::Instance().Process();
		// ForceShowMainAfterCulling: main player must never be hidden by frustum culling
		{
			CInstanceBase* pMain = CPythonCharacterManager::Instance().GetMainInstancePtr();
			if (pMain)
				pMain->GetGraphicThingInstanceRef().Show();
		}

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		if (!CPythonSystem::instance().GetHideMode4Status()) {
			m_pyOfflineshop.DeformEntities();
		}
#endif
		m_kChrMgr.Deform();
		////#ifndef ENABLE_BUGFIXES
		m_kEftMgr.Update();
		////#endif

		if (CPythonWikiRenderTarget::instance().CanRenderWikiModules()) {
			m_pyWikiModelViewManager.DeformModels();
		}

		m_pyBackground.RenderCharacterShadowToTexture();

		m_pyGraphic.SetGameRenderState();
		m_pyGraphic.PushState();

		{
			int32_t lx, ly;
			m_kWndMgr.GetMousePosition(lx, ly);
			m_pyGraphic.SetCursorPosition(lx, ly);
		}

		m_pyBackground.RenderSky();

		m_pyBackground.RenderBeforeLensFlare();

		m_pyBackground.RenderCloud();

		m_pyBackground.BeginEnvironment();
		m_pyBackground.Render();

		m_pyBackground.SetCharacterDirLight();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		if (!CPythonSystem::instance().GetHideMode4Status()) {
			m_pyOfflineshop.RenderEntities();
		}
#endif
		m_kChrMgr.Render();

		if (CPythonWikiRenderTarget::instance().CanRenderWikiModules()) {
			m_pyWikiModelViewManager.RenderModels();
		}

		m_pyBackground.SetBackgroundDirLight();
		m_pyBackground.RenderWater();
		m_pyBackground.RenderSnow();
		m_pyBackground.RenderEffect();

		m_pyBackground.EndEnvironment();

		m_kEftMgr.Render();
		m_pyItem.Render();
		m_FlyingManager.Render();

		m_pyBackground.BeginEnvironment();
		m_pyBackground.RenderPCBlocker();
		m_pyBackground.EndEnvironment();

		m_pyBackground.RenderAfterLensFlare();

		return;
	}

	//if (GetAsyncKeyState(VK_Z))
	//	STATEMANAGER.SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	uint32_t t1 = ELTimer_GetMSec();
	m_kChrMgr.Deform();
	uint32_t t2 = ELTimer_GetMSec();
	////#ifndef ENABLE_BUGFIXES
	m_kEftMgr.Update();
	////##endif
	uint32_t t3 = ELTimer_GetMSec();
	m_pyBackground.RenderCharacterShadowToTexture();
	uint32_t t4 = ELTimer_GetMSec();

	m_pyGraphic.SetGameRenderState();
	m_pyGraphic.PushState();

	float fAspect = m_kWndMgr.GetAspect();
	float fFarClip = m_pyBackground.GetFarClip();
#ifdef ENABLE_PERSPECTIVE_VIEW
	m_pyGraphic.SetPerspective(((m_pySystem.GetFieldPerspective() / 100.0f) * 55.0f) + 30.0f, fAspect, 100.0f, fFarClip);
#else
	m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0f, fFarClip);
#endif

	uint32_t t5 = ELTimer_GetMSec();

	CCullingManager::Instance().Process();

	uint32_t t6 = ELTimer_GetMSec();

	{
		int32_t lx, ly;
		m_kWndMgr.GetMousePosition(lx, ly);
		m_pyGraphic.SetCursorPosition(lx, ly);
	}

	m_pyBackground.RenderSky();
	uint32_t t7 = ELTimer_GetMSec();
	m_pyBackground.RenderBeforeLensFlare();
	uint32_t t8 = ELTimer_GetMSec();
	m_pyBackground.RenderCloud();
	uint32_t t9 = ELTimer_GetMSec();
	m_pyBackground.BeginEnvironment();
	m_pyBackground.Render();

	m_pyBackground.SetCharacterDirLight();
	uint32_t t10 = ELTimer_GetMSec();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
	if (!CPythonSystem::instance().GetHideMode4Status()) {
		m_pyOfflineshop.RenderEntities();
	}
#endif
	m_kChrMgr.Render();
	uint32_t t11 = ELTimer_GetMSec();

	m_pyBackground.SetBackgroundDirLight();
	m_pyBackground.RenderWater();
	uint32_t t12 = ELTimer_GetMSec();
	m_pyBackground.RenderEffect();
	uint32_t t13 = ELTimer_GetMSec();
	m_pyBackground.EndEnvironment();
	m_kEftMgr.Render();
	uint32_t t14 = ELTimer_GetMSec();
	m_pyItem.Render();
	uint32_t t15 = ELTimer_GetMSec();
	m_FlyingManager.Render();
	uint32_t t16 = ELTimer_GetMSec();
	m_pyBackground.BeginEnvironment();
	m_pyBackground.RenderPCBlocker();
	m_pyBackground.EndEnvironment();
	uint32_t t17 = ELTimer_GetMSec();
	m_pyBackground.RenderAfterLensFlare();
	uint32_t t18 = ELTimer_GetMSec();
	uint32_t tEnd = ELTimer_GetMSec();

	if (GetAsyncKeyState(VK_Z))
		STATEMANAGER.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

	if (tEnd - t1 < 3)
		return;

	static FILE* fp = fopen("perf_game_render.txt", "w");

	fprintf(fp, "GR.Total %d (Time %d)\n", tEnd - t1, ELTimer_GetMSec());
	fprintf(fp, "GR.DFM %d\n", t2 - t1);
	fprintf(fp, "GR.EFT.UP %d\n", t3 - t2);
	fprintf(fp, "GR.SHW %d\n", t4 - t3);
	fprintf(fp, "GR.STT %d\n", t5 - t4);
	fprintf(fp, "GR.CLL %d\n", t6 - t5);
	fprintf(fp, "GR.BG.SKY %d\n", t7 - t6);
	fprintf(fp, "GR.BG.LEN %d\n", t8 - t7);
	fprintf(fp, "GR.BG.CLD %d\n", t9 - t8);
	fprintf(fp, "GR.BG.MAIN %d\n", t10 - t9);
	fprintf(fp, "GR.CHR %d\n", t11 - t10);
	fprintf(fp, "GR.BG.WTR %d\n", t12 - t11);
	fprintf(fp, "GR.BG.EFT %d\n", t13 - t12);
	fprintf(fp, "GR.EFT %d\n", t14 - t13);
	fprintf(fp, "GR.ITM %d\n", t15 - t14);
	fprintf(fp, "GR.FLY %d\n", t16 - t15);
	fprintf(fp, "GR.BG.BLK %d\n", t17 - t16);
	fprintf(fp, "GR.BG.LEN %d\n", t18 - t17);


	fflush(fp);
}

void CPythonApplication::UpdateGame()
{
	uint32_t t1 = ELTimer_GetMSec();
	POINT ptMouse;
	GetMousePosition(&ptMouse);

	CGraphicTextInstance::Hyperlink_UpdateMousePos(ptMouse.x, ptMouse.y);

	uint32_t t2 = ELTimer_GetMSec();

	//!@# Alt+Tab 중 SetTransfor 에서 튕김 현상 해결을 위해 - [levites]
	//if (m_isActivateWnd)
	{
		CScreen s;
		float fAspect = UI::CWindowManager::Instance().GetAspect();
		float fFarClip = CPythonBackground::Instance().GetFarClip();
#ifdef ENABLE_PERSPECTIVE_VIEW
		s.SetPerspective(((m_pySystem.GetFieldPerspective() / 100.0f) * 55.0f) + 30.0f, fAspect, 100.0f, fFarClip);
#else
		s.SetPerspective(30.0f, fAspect, 100.0f, fFarClip);
#endif
		s.BuildViewFrustum();
	}

	if (CPythonWikiRenderTarget::instance().CanRenderWikiModules()) {
		m_pyWikiModelViewManager.UpdateModels();
	}

	uint32_t t3 = ELTimer_GetMSec();
	TPixelPosition kPPosMainActor;
	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);

	uint32_t t4 = ELTimer_GetMSec();
	m_pyBackground.Update(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);

	uint32_t t5 = ELTimer_GetMSec();
	m_GameEventManager.SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	m_GameEventManager.Update();

	uint32_t t6 = ELTimer_GetMSec();
	m_kChrMgr.Update();
#ifdef ENABLE_NEW_SHOP_IN_CITIES
#ifdef __ENABLE_NEW_OFFLINESHOP__
	if (!CPythonSystem::instance().GetHideMode4Status()) {
		m_pyOfflineshop.UpdateEntities();
	}
#endif
#endif
	uint32_t t7 = ELTimer_GetMSec();
	////#ifdef ENABLE_BUGFIXES
	////	m_kEftMgr.Update();
	////#endif
	m_kEftMgr.UpdateSound();
	uint32_t t8 = ELTimer_GetMSec();
	m_FlyingManager.Update();
	uint32_t t9 = ELTimer_GetMSec();
	m_pyItem.Update(ptMouse);
	uint32_t t10 = ELTimer_GetMSec();
	m_pyPlayer.Update();
	uint32_t t11 = ELTimer_GetMSec();

	// NOTE : Update 동안 위치 값이 바뀌므로 다시 얻어 옵니다 - [levites]
	//        이 부분 때문에 메인 케릭터의 Sound가 이전 위치에서 플레이 되는 현상이 있었음.
	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);
	SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	uint32_t t12 = ELTimer_GetMSec();

	if (PERF_CHECKER_RENDER_GAME)
	{
		if (t12 - t1 > 5)
		{
			static FILE* fp = fopen("perf_game_update.txt", "w");

			fprintf(fp, "GU.Total %d (Time %d)\n", t12 - t1, ELTimer_GetMSec());
			fprintf(fp, "GU.GMP %d\n", t2 - t1);
			fprintf(fp, "GU.SCR %d\n", t3 - t2);
			fprintf(fp, "GU.MPS %d\n", t4 - t3);
			fprintf(fp, "GU.BG %d\n", t5 - t4);
			fprintf(fp, "GU.GEM %d\n", t6 - t5);
			fprintf(fp, "GU.CHR %d\n", t7 - t6);
			fprintf(fp, "GU.EFT %d\n", t8 - t7);
			fprintf(fp, "GU.FLY %d\n", t9 - t8);
			fprintf(fp, "GU.ITM %d\n", t10 - t9);
			fprintf(fp, "GU.PLR %d\n", t11 - t10);
			fprintf(fp, "GU.POS %d\n", t12 - t11);
			fflush(fp);
		}
	}
}

void CPythonApplication::SkipRenderBuffering(uint32_t dwSleepMSec)
{
	m_dwBufSleepSkipTime = ELTimer_GetMSec() + dwSleepMSec;
}

bool CPythonApplication::Process()
{
	//Ez az eventes fos valami systemhez tartozik?
	Event::Flush();
	ELTimer_SetFrameMSec();

	uint32_t dwStart = ELTimer_GetMSec();

	///////////////////////////////////////////////////////////////////////////////////////////////////
	static uint32_t	s_dwUpdateFrameCount = 0;
	static uint32_t	s_dwRenderFrameCount = 0;
	static uint32_t	s_dwFaceCount = 0;
	static UINT		s_uiLoad = 0;
	static uint32_t	s_dwCheckTime = ELTimer_GetMSec();

	if (ELTimer_GetMSec() - s_dwCheckTime > 1000)
	{
		m_dwUpdateFPS = s_dwUpdateFrameCount;
		m_dwRenderFPS = s_dwRenderFrameCount;
		m_dwLoad = s_uiLoad;

		m_dwFaceCount = s_dwFaceCount / max(1, s_dwRenderFrameCount);

		s_dwCheckTime = ELTimer_GetMSec();

		s_uiLoad = s_dwFaceCount = s_dwUpdateFrameCount = s_dwRenderFrameCount = 0;
	}

	// Update Time
	static bool s_bFrameSkip = false;
	static UINT s_uiNextFrameTime = ELTimer_GetMSec();

	CTimer& rkTimer = CTimer::Instance();
	rkTimer.Advance();

	m_fGlobalTime = rkTimer.GetCurrentSecond();
	m_fGlobalElapsedTime = rkTimer.GetElapsedSecond();

	UINT uiFrameTime = rkTimer.GetElapsedMilliecond();
	s_uiNextFrameTime += uiFrameTime;

	uint32_t updatestart = ELTimer_GetMSec();
	m_pyNetworkStream.Process();

	m_kGuildMarkUploader.Process();

	m_kGuildMarkDownloader.Process();
	m_kAccountConnector.Process();

	UpdateKeyboard();

	POINT Point;
	if (GetCursorPos(&Point))
	{
		ScreenToClient(m_hWnd, &Point);
		OnMouseMove(Point.x, Point.y);
	}
	__UpdateCamera();
	CResourceManager::Instance().Update();
	OnCameraUpdate();
	OnMouseUpdate();
	OnUIUpdate();
	CCullingManager::Instance().Update();

	m_dwCurUpdateTime = ELTimer_GetMSec() - updatestart;

	uint32_t dwCurrentTime = ELTimer_GetMSec();
	bool  bCurrentLateUpdate = false;

	s_bFrameSkip = false;

	if (dwCurrentTime > s_uiNextFrameTime)
	{
		int dt = dwCurrentTime - s_uiNextFrameTime;
		int nAdjustTime = (static_cast<float>(dt) / static_cast<float>(uiFrameTime)) * uiFrameTime;

		if (dt >= 500)
		{
			s_uiNextFrameTime += nAdjustTime;
			printf("FrameSkip Adjusting... %d\n", nAdjustTime);
			CTimer::Instance().Adjust(nAdjustTime);
		}

		s_bFrameSkip = true;
		bCurrentLateUpdate = TRUE;
	}

	{
		static char i = 0;
		if (m_isMinimizedWnd && 0 == i++)
			CEffectManager::Instance().Update();
	}

	if (m_isFrameSkipDisable && !m_isMinimizedWnd)
		s_bFrameSkip = false;

	if (!s_bFrameSkip)
	{
		CGrannyMaterial::TranslateSpecularMatrix(g_specularSpd, g_specularSpd, 0.0f);

		uint32_t dwRenderStartTime = ELTimer_GetMSec();

		bool canRender = true;

		if (m_isMinimizedWnd)
		{
			canRender = false;
		}
		else
		{
#ifdef ENABLE_TEXT_RENEWAL
			if (DEVICE_STATE_OK != CheckDeviceState())
				canRender = false;
#else
			if (m_pyGraphic.IsLostDevice())
			{
				CPythonBackground& rkBG = CPythonBackground::Instance();
				rkBG.ReleaseCharacterShadowTexture();

				if (CPythonWikiRenderTarget::instance().CanRenderWikiModules()) {
					m_pyWikiModelViewManager.ReleaseRenderTargetTextures();
				}

				if (m_pyGraphic.RestoreDevice())
				{

					if (CPythonWikiRenderTarget::instance().CanRenderWikiModules()) {
						m_pyWikiModelViewManager.CreateRenderTargetTextures();
					}

					rkBG.CreateCharacterShadowTexture();
				}
				else
					canRender = false;

			}
#endif
		}

		if (!IsActive())
		{
			SkipRenderBuffering(3000);
		}

		if (!canRender)
		{
			SkipRenderBuffering(3000);
		}
		else
		{
			if (m_pyGraphic.Begin())
			{

				m_pyGraphic.ClearDepthBuffer();

#ifdef _DEBUG
				m_pyGraphic.SetClearColor(0.3f, 0.3f, 0.3f);
				m_pyGraphic.Clear();
#endif
				m_pyGraphic.SetInterfaceRenderState();

				OnUIRender();
				OnMouseRender();

				m_pyGraphic.End();
				m_pyGraphic.Show();

				uint32_t dwRenderEndTime = ELTimer_GetMSec();

				static uint32_t s_dwRenderCheckTime = dwRenderEndTime;
				static uint32_t s_dwRenderRangeTime = 0;
				static uint32_t s_dwRenderRangeFrame = 0;

				m_dwCurRenderTime = dwRenderEndTime - dwRenderStartTime;
				s_dwRenderRangeTime += m_dwCurRenderTime;
				++s_dwRenderRangeFrame;

				if (dwRenderEndTime - s_dwRenderCheckTime > 1000)
				{
					m_fAveRenderTime = static_cast<float>(double(s_dwRenderRangeTime) / double(s_dwRenderRangeFrame));

					s_dwRenderCheckTime = ELTimer_GetMSec();
					s_dwRenderRangeTime = 0;
					s_dwRenderRangeFrame = 0;
				}

				uint32_t dwCurFaceCount = m_pyGraphic.GetFaceCount();
				m_pyGraphic.ResetFaceCount();
				s_dwFaceCount += dwCurFaceCount;

				if (dwCurFaceCount > 5000)
				{
					if (dwRenderEndTime > m_dwBufSleepSkipTime)
					{
						static float s_fBufRenderTime = 0.0f;

						float fCurRenderTime = m_dwCurRenderTime;

						if (fCurRenderTime > s_fBufRenderTime)
						{
							float fRatio = fMAX(0.5f, (fCurRenderTime - s_fBufRenderTime) / 30.0f);
							s_fBufRenderTime = (s_fBufRenderTime * (100.0f - fRatio) + (fCurRenderTime + 5) * fRatio) / 100.0f;
						}
						else
						{
							float fRatio = 0.5f;
							s_fBufRenderTime = (s_fBufRenderTime * (100.0f - fRatio) + fCurRenderTime * fRatio) / 100.0f;
						}

						if (s_fBufRenderTime > 100.0f)
							s_fBufRenderTime = 100.0f;

						uint32_t dwBufRenderTime = s_fBufRenderTime;

						if (m_isWindowed)
						{
							if (dwBufRenderTime > 58)
								dwBufRenderTime = 64;
							else if (dwBufRenderTime > 42)
								dwBufRenderTime = 48;
							else if (dwBufRenderTime > 26)
								dwBufRenderTime = 32;
							else if (dwBufRenderTime > 10)
								dwBufRenderTime = 16;
							else
								dwBufRenderTime = 8;
						}

						m_fAveRenderTime = s_fBufRenderTime;
					}

					m_dwFaceAccCount += dwCurFaceCount;
					m_dwFaceAccTime += m_dwCurRenderTime;

					m_fFaceSpd = (m_dwFaceAccCount / m_dwFaceAccTime);

					if (-1 == m_iForceSightRange)
					{
						static float s_fAveRenderTime = 16.0f;
						float fRatio = 0.3f;
						s_fAveRenderTime = (s_fAveRenderTime * (100.0f - fRatio) + max(16.0f, m_dwCurRenderTime) * fRatio) / 100.0f;


						float fFar = 25600.0f;
						float fNear = MIN_FOG;
						double dbAvePow = static_cast<double>(1000.0f / s_fAveRenderTime);
						double dbMaxPow = 60.0;
						float fDistance = max(fNear + (fFar - fNear) * (dbAvePow) / dbMaxPow, fNear);
						m_pyBackground.SetViewDistanceSet(0, fDistance);
					}
					else
					{
						m_pyBackground.SetViewDistanceSet(0, static_cast<float>(m_iForceSightRange));
					}
				}
				else
				{
					m_pyBackground.SetViewDistanceSet(0, 25600.0f);
				}

				++s_dwRenderFrameCount;
			}
		}
	}

	int rest = s_uiNextFrameTime - ELTimer_GetMSec();

	if (rest > 0 && !bCurrentLateUpdate)
	{
		s_uiLoad -= rest;
		Sleep(rest);
	}

	++s_dwUpdateFrameCount;

	s_uiLoad += ELTimer_GetMSec() - dwStart;
	return true;
}

void CPythonApplication::UpdateClientRect()
{
	RECT rcApp;
	GetClientRect(&rcApp);
	OnSizeChange(rcApp.right - rcApp.left, rcApp.bottom - rcApp.top);
}

void CPythonApplication::SetMouseHandler(PyObject* poMouseHandler)
{
	m_poMouseHandler = poMouseHandler;
}

int CPythonApplication::CheckDeviceState()
{
	CGraphicDevice::EDeviceState e_deviceState = m_grpDevice.GetDeviceState();
	static CGraphicDevice::EDeviceState s_prevDeviceState = CGraphicDevice::DEVICESTATE_OK;
	static uint32_t s_nextResetAttemptTime = 0;

#ifdef ENABLE_TEXT_RENEWAL
	if (e_deviceState != s_prevDeviceState)
	{
		TraceError("DeviceState changed: %d -> %d", s_prevDeviceState, e_deviceState);
		s_prevDeviceState = e_deviceState;
	}
#endif

	switch (e_deviceState)
	{
	case CGraphicDevice::DEVICESTATE_NULL:
		return DEVICE_STATE_FALSE;

	case CGraphicDevice::DEVICESTATE_BROKEN:
		return DEVICE_STATE_SKIP;

	case CGraphicDevice::DEVICESTATE_NEEDS_RESET:
	{
		const bool isForegroundWindow = (GetForegroundWindow() == m_hWnd);
		const bool isIconicWindow = (FALSE != IsIconic(m_hWnd));
		const uint32_t now = ELTimer_GetMSec();

		if (!m_isActivateWnd || m_isMinimizedWnd || isIconicWindow || !isForegroundWindow)
			return DEVICE_STATE_SKIP;

		if (now < s_nextResetAttemptTime)
			return DEVICE_STATE_SKIP;


		m_pyBackground.ReleaseCharacterShadowTexture();
		Trace("DEVICESTATE_NEEDS_RESET - attempting");




		if (!m_grpDevice.Reset())
		{
			s_nextResetAttemptTime = now + 1000;
			m_pyBackground.CreateCharacterShadowTexture();
			return DEVICE_STATE_SKIP;
		}
		s_nextResetAttemptTime = 0;

		m_pyBackground.CreateCharacterShadowTexture();
		break;
	}
	case CGraphicDevice::DEVICESTATE_OK:
	default:
		break;

	}

	return DEVICE_STATE_OK;
}

bool CPythonApplication::CreateDevice(int width, int height, int Windowed, int bit /* = 32*/, int frequency /* = 0*/)
{
	int iRet;

	m_grpDevice.InitBackBufferCount(2);
	m_grpDevice.RegisterWarningString(CGraphicDevice::CREATE_BAD_DRIVER, ApplicationStringTable_GetStringz(IDS_WARN_BAD_DRIVER, "WARN_BAD_DRIVER"));
	m_grpDevice.RegisterWarningString(CGraphicDevice::CREATE_NO_TNL, ApplicationStringTable_GetStringz(IDS_WARN_NO_TNL, "WARN_NO_TNL"));

	iRet = m_grpDevice.Create(GetWindowHandle(), width, height, Windowed ? true : false, bit, frequency);

	switch (iRet)
	{
	case CGraphicDevice::CREATE_OK:
		return true;

	case CGraphicDevice::CREATE_REFRESHRATE:
		return true;

	case CGraphicDevice::CREATE_ENUM:
	case CGraphicDevice::CREATE_DETECT:
		SET_EXCEPTION(CREATE_NO_APPROPRIATE_DEVICE);
		TraceError("CreateDevice: Enum & Detect failed");
		return false;

	case CGraphicDevice::CREATE_NO_DIRECTX:
		//PyErr_SetString(PyExc_RuntimeError, "DirectX 8.1 or greater required to run game");
		SET_EXCEPTION(CREATE_NO_DIRECTX);
		TraceError("CreateDevice: DirectX 8.1 or greater required to run game");
		return false;

	case CGraphicDevice::CREATE_DEVICE:
		//PyErr_SetString(PyExc_RuntimeError, "GraphicDevice create failed");
		SET_EXCEPTION(CREATE_DEVICE);
		TraceError("CreateDevice: GraphicDevice create failed");
		return false;

	case CGraphicDevice::CREATE_FORMAT:
		SET_EXCEPTION(CREATE_FORMAT);
		TraceError("CreateDevice: Change the screen format");
		return false;

		/*case CGraphicDevice::CREATE_GET_ADAPTER_DISPLAY_MODE:
		//PyErr_SetString(PyExc_RuntimeError, "GetAdapterDisplayMode failed");
		SET_EXCEPTION(CREATE_GET_ADAPTER_DISPLAY_MODE);
		TraceError("CreateDevice: GetAdapterDisplayMode failed");
		return false;*/

	case CGraphicDevice::CREATE_GET_DEVICE_CAPS:
		PyErr_SetString(PyExc_RuntimeError, "GetDevCaps failed");
		TraceError("CreateDevice: GetDevCaps failed");
		return false;

	case CGraphicDevice::CREATE_GET_DEVICE_CAPS2:
		PyErr_SetString(PyExc_RuntimeError, "GetDevCaps2 failed");
		TraceError("CreateDevice: GetDevCaps2 failed");
		return false;

	default:
		if (iRet & CGraphicDevice::CREATE_OK)
		{
			//if (iRet & CGraphicDevice::CREATE_BAD_DRIVER)
			//{
			//	LogBox(ApplicationStringTable_GetStringz(IDS_WARN_BAD_DRIVER), NULL, GetWindowHandle());
			//}
			if (iRet & CGraphicDevice::CREATE_NO_TNL)
			{
				CGrannyLODController::SetMinLODMode(true);
				//LogBox(ApplicationStringTable_GetStringz(IDS_WARN_NO_TNL), NULL, GetWindowHandle());
			}
			return true;
		}

		//PyErr_SetString(PyExc_RuntimeError, "Unknown Error!");
		SET_EXCEPTION(UNKNOWN_ERROR);
		TraceError("CreateDevice: Unknown Error!");
		return false;
	}
}

void CPythonApplication::SetUserMovingMainWindow(bool flag)
{
	if (flag && !GetCursorPos(&m_InitialMouseMovingPoint))
		return;

	m_IsMovingMainWindow = flag;
}
bool CPythonApplication::IsUserMovingMainWindow() const
{
	return m_IsMovingMainWindow;
}
void CPythonApplication::UpdateMainWindowPosition()
{
	POINT finalPoint{};
	if (GetCursorPos(&finalPoint))
	{
		long xDiff = finalPoint.x - m_InitialMouseMovingPoint.x;
		long yDiff = finalPoint.y - m_InitialMouseMovingPoint.y;

		RECT r{};
		GetWindowRect(&r);

		SetPosition(r.left + xDiff, r.top + yDiff);
		m_InitialMouseMovingPoint = finalPoint;
	}
}


void CPythonApplication::Loop()
{
	while (true)
	{
		if (IsUserMovingMainWindow())
			UpdateMainWindowPosition();

		if (IsMessage())
		{
			if (!MessageProcess())
				break;
		}
		else
		{
			if (!Process())
				break;

			m_dwLastIdleTime = ELTimer_GetMSec();
		}
	}
}

#ifdef TEXTS_IMPROVEMENT
int CPythonApplication::LoadTextTrans(const char* localePath) {
	return m_CTExtTrans.Load(localePath);
}

std::string CPythonApplication::GetStringText(uint32_t idx) {
	return m_CTExtTrans.GetStringText(idx);
}
#endif

bool LoadLocaleData(const char* localePath)
{

	CPythonNonPlayer& rkNPCMgr = CPythonNonPlayer::Instance();
	CItemManager& rkItemMgr = CItemManager::Instance();
	CPythonSkill& rkSkillMgr = CPythonSkill::Instance();
#ifdef NEW_PET_SYSTEM
	CPythonSkillPet& rkSkillPetMgr = CPythonSkillPet::Instance();
#endif
	CPythonNetworkStream& rkNetStream = CPythonNetworkStream::Instance();

	char szItemProto[256];
	char szItemDesc[256];
	char szMobProto[256];
	char szSkillDescFileName[256];
	#ifdef ENABLE_SHINING_SYSTEM
	char szShiningTable[256];
#endif
#ifdef NEW_PET_SYSTEM
	char szSkillPetFileName[256];
#endif
	char szInsultList[256];
	snprintf(szItemProto, sizeof(szItemProto), "data/items/protos/%s", LocaleService_GetLocaleName());
	snprintf(szItemDesc, sizeof(szItemDesc), "data/items/descs/%s.txt", LocaleService_GetLocaleName());
	snprintf(szMobProto, sizeof(szMobProto), "data/monsters/protos/%s", LocaleService_GetLocaleName());
#ifdef ENABLE_SHINING_SYSTEM
	snprintf(szShiningTable, sizeof(szShiningTable), "data/items/shiningtable.txt");
#endif
	snprintf(szSkillDescFileName, sizeof(szSkillDescFileName), "%s/skilldesc.txt", localePath);
#ifdef NEW_PET_SYSTEM
	snprintf(szSkillPetFileName, sizeof(szSkillPetFileName), "%s/pet_skill.txt", localePath);
#endif
	snprintf(szInsultList, sizeof(szInsultList), "%s/insult.txt", localePath);

	rkNPCMgr.Destroy();
	rkItemMgr.Destroy();
	rkSkillMgr.Destroy();
#ifdef NEW_PET_SYSTEM
	rkSkillPetMgr.Destroy();
#endif
	if (!rkItemMgr.LoadItemList("data/items/item_list.txt")) {
		TraceError("Error while loading item_list.txt");
	}

	if (!rkItemMgr.LoadItemTable(szItemProto)) {
		TraceError("Error while loading items protos.");
		return false;
	}

	if (!rkItemMgr.LoadItemDesc(szItemDesc)) {
		TraceError("Error while loading item_desc.txt.");
	}
#ifdef ENABLE_SHINING_SYSTEM
	if (!rkItemMgr.LoadShiningTable(szShiningTable))
	{
		Tracenf("LoadLocaleData - LoadShiningTable(%s) Error", szShiningTable);
	}
#endif
#ifdef ENABLE_ITEM_EXTRA_PROTO
	if (!rkItemMgr.LoadItemExtraProto("data/items/item_extra_proto.txt")) {
		TraceError("Error while loading item_extra_proto.txt.");
	}
#endif

	if (!rkNPCMgr.LoadNonPlayerData(szMobProto)) {
		TraceError("Error while loading monsters protos.");
		return false;
	}

	if (!rkSkillMgr.RegisterSkillDesc(szSkillDescFileName))
	{
		TraceError("Error while loading skilldesc.txt");
		return false;
	}

	if (!rkSkillMgr.RegisterSkillTable("data/common/skilltable.txt"))
	{
		TraceError("Error while loading skilltable.txt.");
		return false;
	}

#ifdef NEW_PET_SYSTEM
	if (!rkSkillPetMgr.RegisterSkillPet(szSkillPetFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillPet(%s) Error", szSkillPetFileName);
		return false;
	}
#endif

	if (!rkNetStream.LoadInsultList(szInsultList)) {
		TraceError("Error while loading insult.txt.");
	}

#ifdef ENABLE_ACCE_SYSTEM
	char szItemScale[256]{};
	snprintf(szItemScale, sizeof(szItemScale), "data/items/item_scale.txt", localePath);

	if (!rkItemMgr.LoadItemScale(szItemScale))
	{
		TraceError("Error while loading item_scale.txt.");
		return false;
	}
#endif

#ifdef TEXTS_IMPROVEMENT
	int ret = CPythonApplication::Instance().LoadTextTrans(localePath);
	if (ret != 1) {
		if (ret == -2)
			TraceError("Error while loading locale_string.txt.");

		return false;
	}
#endif

	return true;
}
// END_OF_SUPPORT_NEW_KOREA_SERVER

unsigned __GetWindowMode(bool windowed)
{
	if (windowed)
		return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	return WS_POPUP;
}

bool CPythonApplication::Create(PyObject* poSelf, const char* c_szName, int width, int height, int Windowed)
{
	Windowed = CPythonSystem::Instance().IsWindowed() ? 1 : 0;

	bool bAnotherWindow = false;

	if (FindWindow(nullptr, c_szName))
		bAnotherWindow = true;

	m_dwWidth = width;
	m_dwHeight = height;

	// Window
	UINT WindowMode = __GetWindowMode(Windowed ? true : false);

	if (!CMSWindow::Create(c_szName, 4, 0, WindowMode, ::LoadIcon(GetInstance(), MAKEINTRESOURCE(IDI_METIN2)), IDC_CURSOR_NORMAL))
	{
		//PyErr_SetString(PyExc_RuntimeError, "CMSWindow::Create failed");
		TraceError("CMSWindow::Create failed");
		SET_EXCEPTION(CREATE_WINDOW);
		return false;
	}
#ifdef LEADERBOARD_RAZOR93
	(void)CPythonLeaderboard::Instance(); // 

	if (CPythonLeaderboard::HasInstance())
	{
		if (auto dev = CGraphicBase::GetD3DDevice())
		{
			CPythonLeaderboard::Instance().OnDeviceCreate(dev);
			CPythonLeaderboard::Instance().OnDeviceReset(dev);
			TraceError("[LB] App.Create: device present -> OnDeviceCreate/OnDeviceReset sent");
		}
		else
		{
			TraceError(AY_OBFUSCATE("[LB] App.Create: device not yet present (OK) - hooks will arrive from GrpDevice later"));
		}
	}
#endif

	if (m_pySystem.IsUseDefaultIME())
	{
		CPythonIME::Instance().UseDefaultIME();
	}

	m_pyNetworkStream.Discord_Start();

	if (!m_pySystem.IsWindowed() && (m_pySystem.IsUseDefaultIME() || LocaleService_IsEUROPE()))
	{
		m_isWindowed = false;
		m_isWindowFullScreenEnable = TRUE;
		__SetFullScreenWindow(GetWindowHandle(), width, height, m_pySystem.GetBPP());

		Windowed = true;
	}
	else
	{
		AdjustSize(m_pySystem.GetWidth(), m_pySystem.GetHeight());

		if (Windowed)
		{
			m_isWindowed = true;

#ifdef ENABLE_BUGFIXES
			RECT workArea;
			SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

			const UINT workAreaWidth = (workArea.right - workArea.left);
			const UINT workAreaHeight = (workArea.bottom - workArea.top);

			const UINT windowWidth = m_pySystem.GetWidth() + GetSystemMetrics(SM_CXBORDER) * 2 + GetSystemMetrics(SM_CXDLGFRAME) * 2 + GetSystemMetrics(SM_CXFRAME) * 2;
			const UINT windowHeight = m_pySystem.GetHeight() + GetSystemMetrics(SM_CYBORDER) * 2 + GetSystemMetrics(SM_CYDLGFRAME) * 2 + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION);

			const UINT x = workAreaWidth / 2 - windowWidth / 2;
			const UINT y = workAreaHeight / 2 - windowHeight / 2;

			SetPosition(x, y);
#else
			if (bAnotherWindow)
			{
				RECT rc;

				GetClientRect(&rc);

				int windowWidth = rc.right - rc.left;
				int windowHeight = (rc.bottom - rc.top);

				CMSApplication::SetPosition(GetScreenWidth() - windowWidth, GetScreenHeight() - 60 - windowHeight);
			}
#endif
		}
		else
		{
			m_isWindowed = false;
			SetPosition(0, 0);
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Cursor
	if (!CreateCursors())
	{
		//PyErr_SetString(PyExc_RuntimeError, "CMSWindow::Cursors Create Error");
		TraceError("CMSWindow::Cursors Create Error");
		SET_EXCEPTION("CREATE_CURSOR");
		return false;
	}

	if (!m_pySystem.IsNoSoundCard())
	{
		if (!m_SoundEngine.Initialize())
		{
			TraceError("Failed to initialize sound manager!");
			return false; // Is this important enough to stop the client?
		}
	}

	extern bool GRAPHICS_CAPS_SOFTWARE_TILING;

	if (!m_pySystem.IsAutoTiling())
		GRAPHICS_CAPS_SOFTWARE_TILING = m_pySystem.IsSoftwareTiling();

	// Device
	if (!CreateDevice(m_pySystem.GetWidth(), m_pySystem.GetHeight(), Windowed, m_pySystem.GetBPP(), m_pySystem.GetFrequency()))
		return false;

	GrannyCreateSharedDeformBuffer();

	if (m_pySystem.IsAutoTiling())
	{
		if (m_grpDevice.IsFastTNL())
		{
			m_pyBackground.ReserveSoftwareTilingEnable(false);
		}
		else
		{
			m_pyBackground.ReserveSoftwareTilingEnable(true);
		}
	}
	else
	{
		m_pyBackground.ReserveSoftwareTilingEnable(m_pySystem.IsSoftwareTiling());
	}

	SetVisibleMode(true);

	if (m_isWindowFullScreenEnable) //m_pySystem.IsUseDefaultIME() && !m_pySystem.IsWindowed())
	{
		SetWindowPos(GetWindowHandle(), HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW);
	}

	if (!InitializeKeyboard(GetWindowHandle()))
		return false;

	m_pySystem.GetDisplaySettings();

	// Mouse
	if (m_pySystem.IsSoftwareCursor())
		SetCursorMode(CURSOR_MODE_SOFTWARE);
	else
		SetCursorMode(CURSOR_MODE_HARDWARE);

	// Network
	if (!m_netDevice.Create())
	{
		//PyErr_SetString(PyExc_RuntimeError, "NetDevice::Create failed");
		TraceError("NetDevice::Create failed");
		SET_EXCEPTION("CREATE_NETWORK");
		return false;
	}

	if (!m_grpDevice.IsFastTNL())
		CGrannyLODController::SetMinLODMode(true);

	m_pyItem.Create();

	// Other Modules
	DefaultFont_Startup();

	CPythonIME::Instance().Create(GetWindowHandle());
	CPythonIME::Instance().SetText("", 0);
	CPythonTextTail::Instance().Initialize();

	// Light Manager
	m_LightManager.Initialize();

	CGraphicImageInstance::CreateSystem(32);

	// 백업
	STICKYKEYS sStickKeys;
	memset(&sStickKeys, 0, sizeof(sStickKeys));
	sStickKeys.cbSize = sizeof(sStickKeys);
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0);
	m_dwStickyKeysFlag = sStickKeys.dwFlags;

	// 설정
	sStickKeys.dwFlags &= ~(SKF_AVAILABLE | SKF_HOTKEYACTIVE);
	SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0);

	// SphereMap
	CGrannyMaterial::CreateSphereMap(0, "d:/ymir work/special/spheremap.jpg");
	CGrannyMaterial::CreateSphereMap(1, "d:/ymir work/special/spheremap01.jpg");
	return true;
}

void CPythonApplication::SetGlobalCenterPosition(int32_t x, int32_t y)
{
	CPythonBackground& rkBG = CPythonBackground::Instance();
	rkBG.GlobalPositionToLocalPosition(x, y);

	float z = CPythonBackground::Instance().GetHeight(x, y);

	CPythonApplication::Instance().SetCenterPosition(x, y, z);
}

void CPythonApplication::SetCenterPosition(float fx, float fy, float fz)
{
	m_v3CenterPosition.x = +fx;
	m_v3CenterPosition.y = -fy;
	m_v3CenterPosition.z = +fz;
}

void CPythonApplication::GetCenterPosition(TPixelPosition* pPixelPosition)
{
	pPixelPosition->x = +m_v3CenterPosition.x;
	pPixelPosition->y = -m_v3CenterPosition.y;
	pPixelPosition->z = +m_v3CenterPosition.z;
}


void CPythonApplication::SetServerTime(time_t tTime)
{
	m_dwStartLocalTime = ELTimer_GetMSec();
	m_tServerTime = tTime;
	m_tLocalStartTime = time(nullptr);
}

time_t CPythonApplication::GetServerTime()
{
	return (ELTimer_GetMSec() - m_dwStartLocalTime) + m_tServerTime;
}

// 2005.03.28 - MALL 아이템에 들어있는 시간의 단위가 서버에서 time(0) 으로 만들어지는
//              값이기 때문에 단위를 맞추기 위해 시간 관련 처리를 별도로 추가
time_t CPythonApplication::GetServerTimeStamp()
{
	return (time(nullptr) - m_tLocalStartTime) + m_tServerTime;
}

float CPythonApplication::GetGlobalTime()
{
	return m_fGlobalTime;
}

float CPythonApplication::GetGlobalElapsedTime()
{
	return m_fGlobalElapsedTime;
}

void CPythonApplication::SetFPS(int iFPS)
{
	m_iFPS = iFPS;
}

int CPythonApplication::GetWidth()
{
	return m_dwWidth;
}

int CPythonApplication::GetHeight()
{
	return m_dwHeight;
}

void CPythonApplication::SetConnectData(const char* c_szIP, int iPort)
{
	m_strIP = c_szIP;
	m_iPort = iPort;
}

void CPythonApplication::GetConnectData(std::string& rstIP, int& riPort)
{
	rstIP = m_strIP;
	riPort = m_iPort;
}

void CPythonApplication::EnableSpecialCameraMode()
{
	m_isSpecialCameraMode = TRUE;
}

void CPythonApplication::SetCameraSpeed(int iPercentage)
{
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed * float(iPercentage) / 100.0f;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed * float(iPercentage) / 100.0f;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed * float(iPercentage) / 100.0f;
}

void CPythonApplication::SetForceSightRange(int iRange)
{
	m_iForceSightRange = iRange;
}

void CPythonApplication::Clear()
{
	m_pySystem.Clear();
}

void CPythonApplication::Destroy()
{


	// SphereMap
	CGrannyMaterial::DestroySphereMap();

	m_kWndMgr.Destroy();

	CPythonSystem::Instance().SaveConfig();

	m_pyWikiModelViewManager.InitializeData();


	DestroyCollisionInstanceSystem();

	m_pySystem.SaveInterfaceStatus();

	m_pyEventManager.Destroy();
	m_FlyingManager.Destroy();

	m_pyMiniMap.Destroy();

	m_pyTextTail.Destroy();
	m_pyChat.Destroy();
	m_kChrMgr.Destroy();
	m_RaceManager.Destroy();

	m_pyItem.Destroy();
	m_kItemMgr.Destroy();

	m_pyBackground.Destroy();

	m_kEftMgr.Destroy();
	m_LightManager.Destroy();

	// DEFAULT_FONT
	DefaultFont_Cleanup();
	// END_OF_DEFAULT_FONT

	GrannyDestroySharedDeformBuffer();

	m_pyGraphic.Destroy();
	m_pyNetworkStream.Discord_Close();

	//m_pyNetworkDatagram.Destroy();

	m_pyRes.Destroy();

	m_kGuildMarkDownloader.Disconnect();

	CGrannyModelInstance::DestroySystem();
	CGraphicImageInstance::DestroySystem();


	//m_SoundManager.Destroy();
	m_grpDevice.Destroy();

	// FIXME : 만들어져 있지 않음 - [levites]
	//CSpeedTreeForestDirectX8::Instance().Clear();

	CAttributeInstance::DestroySystem();
	CTextFileLoader::DestroySystem();
	DestroyCursors();

	CMSApplication::Destroy();

	STICKYKEYS sStickKeys;
	memset(&sStickKeys, 0, sizeof(sStickKeys));
	sStickKeys.cbSize = sizeof(sStickKeys);
	sStickKeys.dwFlags = m_dwStickyKeysFlag;
	SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(sStickKeys), &sStickKeys, 0);
}
