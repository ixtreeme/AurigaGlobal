#include "StdAfx.h"
#include "PythonSystem.h"
#include "PythonApplication.h"
#include "PythonBackground.h"

#define DEFAULT_VALUE_ALWAYS_SHOW_NAME		true

void CPythonSystem::SetInterfaceHandler(PyObject * poHandler)
{
// NOTE : 레퍼런스 카운트는 바꾸지 않는다. 레퍼런스가 남아 있어 Python에서 완전히 지워지지 않기 때문.
//        대신에 __del__때 Destroy를 호출해 Handler를 NULL로 셋팅한다. - [levites]
//	if (m_poInterfaceHandler)
//		Py_DECREF(m_poInterfaceHandler);

	m_poInterfaceHandler = poHandler;

//	if (m_poInterfaceHandler)
//		Py_INCREF(m_poInterfaceHandler);
}

void CPythonSystem::DestroyInterfaceHandler()
{
	m_poInterfaceHandler = nullptr;
}

void CPythonSystem::SaveWindowStatus(int iIndex, int iVisible, int iMinimized, int ix, int iy, int iHeight)
{
	m_WindowStatus[iIndex].isVisible	= iVisible;
	m_WindowStatus[iIndex].isMinimized	= iMinimized;
	m_WindowStatus[iIndex].ixPosition	= ix;
	m_WindowStatus[iIndex].iyPosition	= iy;
	m_WindowStatus[iIndex].iHeight		= iHeight;
}

void CPythonSystem::GetDisplaySettings()
{
	memset(m_ResolutionList, 0, sizeof(TResolution) * RESOLUTION_MAX_NUM);
	m_ResolutionCount = 0;

	LPDIRECT3D9 lpD3D = CPythonGraphic::Instance().GetD3D();

	D3DADAPTER_IDENTIFIER9 d3dAdapterIdentifier;
	D3DDISPLAYMODE d3ddmDesktop;

	lpD3D->GetAdapterIdentifier(0, 0, &d3dAdapterIdentifier);
	lpD3D->GetAdapterDisplayMode(0, &d3ddmDesktop);


	// 이 어뎁터가 가지고 있는 디스플래이 모드갯수를 나열한다..
	uint32_t dwNumAdapterModes = lpD3D->GetAdapterModeCount(0, D3DFMT_X8R8G8B8);

	for (UINT iMode = 0; iMode < dwNumAdapterModes; iMode++)
	{
		D3DDISPLAYMODE DisplayMode;
		lpD3D->EnumAdapterModes(0, D3DFMT_X8R8G8B8, iMode, &DisplayMode);
		uint32_t bpp = 0;

		// 800 600 이상만 걸러낸다.
		if (DisplayMode.Width < 800 || DisplayMode.Height < 600)
			continue;

		// 일단 16bbp 와 32bbp만 취급하자.
		// 16bbp만 처리하게끔 했음 - [levites]
		if (DisplayMode.Format == D3DFMT_R5G6B5)
			bpp = 16;
		else if (DisplayMode.Format == D3DFMT_X8R8G8B8)
			bpp = 32;
		else
			continue;

		int check_res = false;

		for (int i = 0; !check_res && i < m_ResolutionCount; ++i)
		{
			if (m_ResolutionList[i].bpp != bpp ||
				m_ResolutionList[i].width != DisplayMode.Width ||
				m_ResolutionList[i].height != DisplayMode.Height)
				continue;

			int check_fre = false;

			// 프리퀀시만 다르므로 프리퀀시만 셋팅해준다.
			for (int j = 0; j < m_ResolutionList[i].frequency_count; ++j)
			{
				if (m_ResolutionList[i].frequency[j] == DisplayMode.RefreshRate)
				{
					check_fre = true;
					break;
				}
			}

			if (!check_fre)
				if (m_ResolutionList[i].frequency_count < FREQUENCY_MAX_NUM)
					m_ResolutionList[i].frequency[m_ResolutionList[i].frequency_count++] = DisplayMode.RefreshRate;

			check_res = true;
		}

		if (!check_res)
		{
			// 새로운 거니까 추가해주자.
			if (m_ResolutionCount < RESOLUTION_MAX_NUM)
			{
				m_ResolutionList[m_ResolutionCount].width			= DisplayMode.Width;
				m_ResolutionList[m_ResolutionCount].height			= DisplayMode.Height;
				m_ResolutionList[m_ResolutionCount].bpp				= bpp;
				m_ResolutionList[m_ResolutionCount].frequency[0]	= DisplayMode.RefreshRate;
				m_ResolutionList[m_ResolutionCount].frequency_count	= 1;

				++m_ResolutionCount;
			}
		}
	}
}

int	CPythonSystem::GetResolutionCount()
{
	return m_ResolutionCount;
}

int CPythonSystem::GetFrequencyCount(int index)
{
	if (index >= m_ResolutionCount)
		return 0;

    return m_ResolutionList[index].frequency_count;
}

bool CPythonSystem::GetResolution(int index, OUT uint32_t *width, OUT uint32_t *height, OUT uint32_t *bpp)
{
	if (index >= m_ResolutionCount)
		return false;

	*width = m_ResolutionList[index].width;
	*height = m_ResolutionList[index].height;
	*bpp = m_ResolutionList[index].bpp;
	return true;
}

bool CPythonSystem::GetFrequency(int index, int freq_index, OUT uint32_t *frequncy)
{
	if (index >= m_ResolutionCount)
		return false;

	if (freq_index >= m_ResolutionList[index].frequency_count)
		return false;

	*frequncy = m_ResolutionList[index].frequency[freq_index];
	return true;
}

int	CPythonSystem::GetResolutionIndex(uint32_t width, uint32_t height, uint32_t bit)
{
	uint32_t re_width, re_height, re_bit;
	int i = 0;

	while (GetResolution(i, &re_width, &re_height, &re_bit))
	{
		if (re_width == width)
			if (re_height == height)
				if (re_bit == bit)
					return i;
		i++;
	}

	return 0;
}

int	CPythonSystem::GetFrequencyIndex(int res_index, uint32_t frequency)
{
	uint32_t re_frequency;
	int i = 0;

	while (GetFrequency(res_index, i, &re_frequency))
	{
		if (re_frequency == frequency)
			return i;

		i++;
	}

	return 0;
}

uint32_t CPythonSystem::GetWidth()
{
	return m_Config.width;
}

uint32_t CPythonSystem::GetHeight()
{
	return m_Config.height;
}
uint32_t CPythonSystem::GetBPP()
{
	return m_Config.bpp;
}
uint32_t CPythonSystem::GetFrequency()
{
	return m_Config.frequency;
}

bool CPythonSystem::IsNoSoundCard()
{
	return m_Config.bNoSoundCard;
}

bool CPythonSystem::IsSoftwareCursor()
{
	return m_Config.is_software_cursor;
}

float CPythonSystem::GetMusicVolume()
{
	return m_Config.music_volume;
}

int CPythonSystem::GetSoundVolume()
{
	return m_Config.voice_volume;
}

void CPythonSystem::SetMusicVolume(float fVolume)
{
	m_Config.music_volume = fVolume;
}

void CPythonSystem::SetSoundVolumef(float fVolume)
{
	m_Config.voice_volume = int(5 * fVolume);
}

#ifdef ENABLE_PERSPECTIVE_VIEW
float CPythonSystem::GetFieldPerspective()
{
	return m_Config.fField;
}

void CPythonSystem::SetFieldPerspective(float fValue)
{
	m_Config.fField = fValue;
}
#endif

#ifdef ENABLE_BIOLOGIST_UI
bool CPythonSystem::GetBiologistAlert()
{
	return m_Config.biologist_alert;
}

void CPythonSystem::SetBiologistAlert(bool value)
{
	m_Config.biologist_alert = value;
}
#endif

#ifdef ENABLE_SAVECAMERA_PREFERENCES
uint8_t CPythonSystem::GetCameraType() {
	return m_Config.bCameraType;
}

void CPythonSystem::SetCameraType(uint8_t value) {
	if (value < 0) {
		value = 0;
	} else if (value > 2) {
		value = 2;
	}

	m_Config.bCameraType = value;
}

float CPythonSystem::GetCameraHeight() {
	return m_Config.fCameraHeight;
}

void CPythonSystem::SetCameraHeight(float value)
{
	m_Config.fCameraHeight = value;
}
#endif

#ifdef OUTLINE_NAMES_TEXTLINE
bool CPythonSystem::GetNamesType() {
	return m_Config.bNamesOutline;
}

void CPythonSystem::SetNamesType(bool value) {
	m_Config.bNamesOutline = value;
}
#endif

#ifdef ENABLE_AUTO_PICKUP
bool CPythonSystem::GetPickUpMode() {
	return m_Config.bPickUpType;
}

void CPythonSystem::SetPickUpMode(int value) {
	m_Config.bPickUpType = value == 0 ? false : true;
}
#endif

#ifdef ENABLE_NEW_CHAT
void CPythonSystem::SetChatFilter(int value) {
	m_Config.iChatFilter = value > 9 ? 0 : value;
}

int CPythonSystem::GetChatFilter() {
	return m_Config.iChatFilter;
}
#endif

void CPythonSystem::SetEnvironment(int value) {
	m_Config.iEnvironment = value > 6 ? 0 : value;

	const TEnvironmentData * c_pEnvironmenData;
	CPythonBackground& rkBG = CPythonBackground::Instance();
	if (rkBG.GetEnvironmentData(value, &c_pEnvironmenData)) {
		rkBG.ResetEnvironmentDataPtr(c_pEnvironmenData);
	}
}

int CPythonSystem::GetEnvironment() {
	return m_Config.iEnvironment;
}

void CPythonSystem::SetTimePm(bool value) {
	m_Config.bTimePm = value;
}

bool CPythonSystem::GetTimePm() {
	return m_Config.bTimePm;
}

bool CPythonSystem::GetHideMode1Status() {
	return m_Config.bHide1Mode;
}

bool CPythonSystem::GetHideMode2Status() {
	return m_Config.bHide2Mode;
}

bool CPythonSystem::GetHideMode3Status() {
	return m_Config.bHide3Mode;
}

bool CPythonSystem::GetHideMode4Status() {
	return m_Config.bHide4Mode;
}

bool CPythonSystem::GetHideMode5Status() {
	return m_Config.bHide5Mode;
}

bool CPythonSystem::GetHideMode6Status() {
	return m_Config.bHide6Mode;
}

bool CPythonSystem::GetHideMode7Status() {
	return m_Config.bHide7Mode;
}

void CPythonSystem::SetHideModeStatus(int type, int value) {
	switch (type) {
		case 0:
			m_Config.bHide1Mode = value == 0 ? false : true;
			break;
		case 1:
			m_Config.bHide2Mode = value == 0 ? false : true;
			break;
		case 2:
			m_Config.bHide3Mode = value == 0 ? false : true;
			break;
		case 3:
			m_Config.bHide4Mode = value == 0 ? false : true;
			break;
		case 4:
			m_Config.bHide5Mode = value == 0 ? false : true;
			break;
		case 5:
			m_Config.bHide6Mode = value == 0 ? false : true;
			break;
		case 6:
			m_Config.bHide7Mode = value == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_ALL, m_Config.bHide7Mode);
			break;
		default:
			break;
	}
}

bool CPythonSystem::GetHideMode1Status2() {
	return m_Config.bHide1Mode2;
}

bool CPythonSystem::GetHideMode2Status2() {
	return m_Config.bHide2Mode2;
}

bool CPythonSystem::GetHideMode3Status2() {
	return m_Config.bHide3Mode2;
}

bool CPythonSystem::GetHideMode4Status2() {
	return m_Config.bHide4Mode2;
}

void CPythonSystem::SetHideModeStatus2(int type, int value) {
	switch (type) {
		case 0:
			m_Config.bHide1Mode2 = value == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_ARMORS_SHININGS, m_Config.bHide1Mode2);
			break;
		case 1:
			m_Config.bHide2Mode2 = value == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_WEAPONS_SHININGS, m_Config.bHide2Mode2);
			break;
		case 2:
			m_Config.bHide3Mode2 = value == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_BUFF, m_Config.bHide3Mode2);
			break;
		case 3:
			m_Config.bHide4Mode2 = value == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_SKILL, m_Config.bHide4Mode2);
			break;
		default:
			break;
	}
}

int CPythonSystem::GetDistance()
{
	return m_Config.iDistance;
}

int CPythonSystem::GetShadowLevel()
{
	return m_Config.iShadowLevel;
}

void CPythonSystem::SetShadowLevel(unsigned int level)
{
	m_Config.iShadowLevel = MIN(level, 5);
	CPythonBackground::instance().RefreshShadowLevel();
}

int CPythonSystem::IsSaveID()
{
	return m_Config.isSaveID;
}

const char * CPythonSystem::GetSaveID()
{
	return m_Config.SaveID;
}

bool CPythonSystem::isViewCulling()
{
	return m_Config.is_object_culling;
}

void CPythonSystem::SetSaveID(int iValue, const char * c_szSaveID)
{
	if (iValue != 1)
		return;

	m_Config.isSaveID = iValue;
	strncpy(m_Config.SaveID, c_szSaveID, sizeof(m_Config.SaveID) - 1);
}

CPythonSystem::TConfig * CPythonSystem::GetConfig()
{
	return &m_Config;
}

void CPythonSystem::SetConfig(TConfig * pNewConfig)
{
	m_Config = *pNewConfig;
}

void CPythonSystem::SetDefaultConfig()
{
	memset(&m_Config, 0, sizeof(m_Config));

	m_Config.width				= 1024;
	m_Config.height				= 768;
	m_Config.bpp				= 32;

#if defined( LOCALE_SERVICE_WE_JAPAN )
	m_Config.bWindowed			= true;
#else
	m_Config.bWindowed			= false;
#endif

	m_Config.is_software_cursor	= false;
	m_Config.is_object_culling	= true;
	m_Config.iDistance			= 3;

	m_Config.gamma				= 3;
	m_Config.music_volume		= 1.0f;
	m_Config.voice_volume		= 5;

	m_Config.bDecompressDDS		= 0;
	m_Config.bSoftwareTiling	= 0;
	m_Config.iShadowLevel		= 3;
	m_Config.bViewChat			= true;
	m_Config.bAlwaysShowName	= DEFAULT_VALUE_ALWAYS_SHOW_NAME;
	m_Config.bShowDamage		= true;
	m_Config.bShowSalesText		= true;
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
	m_Config.bShowMobAIFlag		= true;
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
	m_Config.bShowMobLevel		= true;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	m_Config.szLanguageFilter = "";
	m_Config.bAutoTranslateWhisper = false;
#endif
#ifdef ENABLE_PERSPECTIVE_VIEW
	m_Config.fField = 0.0f;
#endif
#ifdef ENABLE_BIOLOGIST_UI
	m_Config.biologist_alert = false;
#endif
#ifdef ENABLE_SAVECAMERA_PREFERENCES
	m_Config.bCameraType = 0;
	m_Config.fCameraHeight = 1550.0f;
#endif
#ifdef OUTLINE_NAMES_TEXTLINE
	m_Config.bNamesOutline = false;
#endif
#ifdef ENABLE_AUTO_PICKUP
	m_Config.bPickUpType = true;
#endif
#ifdef ENABLE_NEW_CHAT
	m_Config.iChatFilter = 0;
#endif
	m_Config.iEnvironment = 0;
	m_Config.bTimePm = false;
	m_Config.bHide1Mode = false;
	m_Config.bHide2Mode = false;
	m_Config.bHide3Mode = false;
	m_Config.bHide4Mode = false;
	m_Config.bHide5Mode = false;
	m_Config.bHide6Mode = false;
	m_Config.bHide7Mode = false;
	m_Config.bHide1Mode2 = false;
	m_Config.bHide2Mode2 = false;
	m_Config.bHide3Mode2 = false;
	m_Config.bHide4Mode2 = false;
}

bool CPythonSystem::IsWindowed()
{
	return m_Config.bWindowed;
}

#ifdef ENABLE_MULTI_LANGUAGE
void CPythonSystem::SetChatFilterValue(std::string szFilter)
{
	m_Config.szLanguageFilter = szFilter;
}

std::string CPythonSystem::GetChatFilterValue()
{
	return m_Config.szLanguageFilter;
}
#endif

bool CPythonSystem::IsViewChat()
{
	return m_Config.bViewChat;
}

void CPythonSystem::SetViewChatFlag(int iFlag)
{
	m_Config.bViewChat = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsAlwaysShowName()
{
	return m_Config.bAlwaysShowName;
}

void CPythonSystem::SetAlwaysShowNameFlag(int iFlag)
{
	m_Config.bAlwaysShowName = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsShowDamage()
{
	return m_Config.bShowDamage;
}

void CPythonSystem::SetShowDamageFlag(int iFlag)
{
	m_Config.bShowDamage = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsShowSalesText()
{
	return m_Config.bHide5Mode;
}

void CPythonSystem::SetShowSalesTextFlag(int iFlag)
{
	m_Config.bShowSalesText = iFlag == 1 ? true : false;
}

#ifdef ENABLE_MULTI_LANGUAGE
bool CPythonSystem::IsAutoTranslateWhisper()
{
	return m_Config.bAutoTranslateWhisper;
}

void CPythonSystem::SetAutoTranslateWhisper(int iFlag)
{
	m_Config.bAutoTranslateWhisper = iFlag == 1 ? true : false;
}
#endif

#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
bool CPythonSystem::IsShowMobAIFlag()
{
	return m_Config.bShowMobAIFlag;
}

void CPythonSystem::SetShowMobAIFlagFlag(int iFlag)
{
	m_Config.bShowMobAIFlag = iFlag == 1 ? true : false;
}
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
bool CPythonSystem::IsShowMobLevel()
{
	return m_Config.bShowMobLevel;
}

void CPythonSystem::SetShowMobLevelFlag(int iFlag)
{
	m_Config.bShowMobLevel = iFlag == 1 ? true : false;
}
#endif

bool CPythonSystem::IsAutoTiling()
{
	if (m_Config.bSoftwareTiling == 0)
		return true;

	return false;
}

void CPythonSystem::SetSoftwareTiling(bool isEnable)
{
	if (isEnable)
		m_Config.bSoftwareTiling=1;
	else
		m_Config.bSoftwareTiling=2;
}

bool CPythonSystem::IsSoftwareTiling()
{
	if (m_Config.bSoftwareTiling==1)
		return true;

	return false;
}

bool CPythonSystem::IsUseDefaultIME()
{
	return m_Config.bUseDefaultIME;
}

bool CPythonSystem::LoadConfig()
{
	FILE * fp = nullptr;

	if (nullptr == (fp = fopen("metin2.cfg", "rt")))
		return false;

	char buf[256];
	char command[256];
	char value[256];

	while (fgets(buf, 256, fp))
	{
		if (sscanf(buf, " %s %s\n", command, value) == EOF)
			break;

		if (!stricmp(command, "WIDTH"))
			m_Config.width		= atoi(value);
		else if (!stricmp(command, "HEIGHT"))
			m_Config.height	= atoi(value);
		else if (!stricmp(command, "BPP"))
			m_Config.bpp		= atoi(value);
		else if (!stricmp(command, "FREQUENCY"))
			m_Config.frequency = atoi(value);
		else if (!stricmp(command, "SOFTWARE_CURSOR"))
			m_Config.is_software_cursor = atoi(value) ? true : false;
		else if (!stricmp(command, "OBJECT_CULLING"))
			m_Config.is_object_culling = atoi(value) ? true : false;
		else if (!stricmp(command, "VISIBILITY"))
			m_Config.iDistance = atoi(value);
		else if (!stricmp(command, "MUSIC_VOLUME")) {
			if(strchr(value, '.') == nullptr) { // Old compatiability
				m_Config.music_volume = pow(10.0f, (-1.0f + (((float) atoi(value)) / 5.0f)));
				if(atoi(value) == 0)
					m_Config.music_volume = 0.0f;
			} else
				m_Config.music_volume = std::stof(value);
		} else if (!stricmp(command, "VOICE_VOLUME"))
			m_Config.voice_volume = (char) atoi(value);
		else if (!stricmp(command, "GAMMA"))
			m_Config.gamma = atoi(value);
		else if (!stricmp(command, "IS_SAVE_ID"))
			m_Config.isSaveID = atoi(value);
		else if (!stricmp(command, "SAVE_ID"))
			strncpy(m_Config.SaveID, value, 20);
#ifdef ENABLE_LOADING_DELAY
		else if (!stricmp(command, "PRE_LOADING_DELAY_TIME"))
			g_iLoadingDelayTime = atoi(value);
#endif
		else if (!stricmp(command, "WINDOWED"))
		{
			m_Config.bWindowed = atoi(value) == 1 ? true : false;
		}
		else if (!stricmp(command, "USE_DEFAULT_IME"))
			m_Config.bUseDefaultIME = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SOFTWARE_TILING"))
			m_Config.bSoftwareTiling = atoi(value);
		else if (!stricmp(command, "SHADOW_LEVEL"))
			m_Config.iShadowLevel = atoi(value);
		else if (!stricmp(command, "DECOMPRESSED_TEXTURE"))
			m_Config.bDecompressDDS = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "NO_SOUND_CARD"))
			m_Config.bNoSoundCard = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "VIEW_CHAT"))
			m_Config.bViewChat = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "ALWAYS_VIEW_NAME"))
			m_Config.bAlwaysShowName = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SHOW_DAMAGE"))
			m_Config.bShowDamage = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SHOW_SALESTEXT"))
			m_Config.bShowSalesText = atoi(value) == 1 ? true : false;
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
		else if (!stricmp(command, "SHOW_MOBAIFLAG"))
			m_Config.bShowMobAIFlag = atoi(value) == 1 ? true : false;
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
		else if (!stricmp(command, "SHOW_MOBLEVEL"))
			m_Config.bShowMobLevel = atoi(value) == 1 ? true : false;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
		else if (!stricmp(command, "LANGUAGE_FILTER"))
			m_Config.szLanguageFilter = value;
		else if (!stricmp(command, "AUTO_TRANSLATE_W"))
			m_Config.bAutoTranslateWhisper = atoi(value) == 1 ? true : false;
#endif
#ifdef ENABLE_PERSPECTIVE_VIEW
		else if (!stricmp(command, "PERSPECTIVE_FIELD")) {
			m_Config.fField = std::stof(value);
		}
#endif
#ifdef ENABLE_BIOLOGIST_UI
		else if (!stricmp(command, "BIOLOGIST_ALERT")) {
			m_Config.biologist_alert = atoi(value) == 1 ? true : false;
		}
#endif
#ifdef ENABLE_SAVECAMERA_PREFERENCES
		else if (!stricmp(command, "CAMERA_TYPE")) {
			m_Config.bCameraType = atoi(value);
		}
		else if (!stricmp(command, "CAMERA_HEIGHT")) {
			m_Config.fCameraHeight = std::stof(value);
		}
#endif
#ifdef OUTLINE_NAMES_TEXTLINE
		else if (!stricmp(command, "INLINE_NAMES")) {
			m_Config.bNamesOutline = atoi(value) == 0 ? false : true;
		}
#endif
#ifdef ENABLE_AUTO_PICKUP
		else if (!stricmp(command, "AUTO_PICKUP")) {
			m_Config.bPickUpType = atoi(value) == 0 ? false : true;
		}
#endif
#ifdef ENABLE_NEW_CHAT
		else if (!stricmp(command, "CHAT_FILTER")) {
			m_Config.iChatFilter = atoi(value) > 9 ? 0 : atoi(value);
		}
#endif
		else if (!stricmp(command, "ENVIRONMENT")) {
			m_Config.iEnvironment = atoi(value) > 5 ? 0 : atoi(value);
		}
		else if (!stricmp(command, "TIMEPM")) {
			m_Config.bTimePm = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE1_MODE")) {
			m_Config.bHide1Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE2_MODE")) {
			m_Config.bHide2Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE3_MODE")) {
			m_Config.bHide3Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE4_MODE")) {
			m_Config.bHide4Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE5_MODE")) {
			m_Config.bHide5Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE6_MODE")) {
			m_Config.bHide6Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE7_MODE")) {
			m_Config.bHide7Mode = atoi(value) == 0 ? false : true;
		}
		else if (!stricmp(command, "HIDE1_MODE2")) {
			m_Config.bHide1Mode2 = atoi(value) == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_ARMORS_SHININGS, m_Config.bHide1Mode2);
		}
		else if (!stricmp(command, "HIDE2_MODE2")) {
			m_Config.bHide2Mode2 = atoi(value) == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_WEAPONS_SHININGS, m_Config.bHide2Mode2);
		}
		else if (!stricmp(command, "HIDE3_MODE2")) {
			m_Config.bHide3Mode2 = atoi(value) == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_BUFF, m_Config.bHide3Mode2);
		}
		else if (!stricmp(command, "HIDE4_MODE2")) {
			m_Config.bHide4Mode2 = atoi(value) == 0 ? false : true;
			CEffectManager::Instance().SetEffectOption(CEffectManager::EFFECT_OPTION_SKILL, m_Config.bHide4Mode2);
		}
	}

	if (m_Config.bWindowed)
	{
		unsigned screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
		unsigned screen_height = GetSystemMetrics(SM_CYFULLSCREEN);

		if (m_Config.width >= screen_width)
		{
			m_Config.width = screen_width;
		}
		if (m_Config.height >= screen_height)
		{
			m_Config.height = screen_height;
		}
	}

	m_OldConfig = m_Config;
	fclose(fp);

//	Tracef("LoadConfig: Resolution: %dx%d %dBPP %dHZ Software Cursor: %d, Music/Voice Volume: %d/%d Gamma: %d\n",
//		m_Config.width,
//		m_Config.height,
//		m_Config.bpp,
//		m_Config.frequency,
//		m_Config.is_software_cursor,
//		m_Config.music_volume,
//		m_Config.voice_volume,
//		m_Config.gamma);

	return true;
}

bool CPythonSystem::SaveConfig()
{
	FILE *fp;

	if (nullptr == (fp = fopen("metin2.cfg", "wt")))
		return false;

	fprintf(fp, "WIDTH						%d\n"
				"HEIGHT						%d\n"
				"BPP						%d\n"
				"FREQUENCY					%d\n"
				"SOFTWARE_CURSOR			%d\n"
				"OBJECT_CULLING				%d\n"
				"VISIBILITY					%d\n"
				"MUSIC_VOLUME				%.3f\n"
				"VOICE_VOLUME				%d\n"
				"GAMMA						%d\n"
				"IS_SAVE_ID					%d\n"
				"SAVE_ID					%s\n"
#ifdef ENABLE_LOADING_DELAY
				"PRE_LOADING_DELAY_TIME		%d\n"
#endif
				"DECOMPRESSED_TEXTURE		%d\n",
				m_Config.width,
				m_Config.height,
				m_Config.bpp,
				m_Config.frequency,
				m_Config.is_software_cursor,
				m_Config.is_object_culling,
				m_Config.iDistance,
				m_Config.music_volume,
				m_Config.voice_volume,
				m_Config.gamma,
				m_Config.isSaveID,
				m_Config.SaveID,
#ifdef ENABLE_LOADING_DELAY
				g_iLoadingDelayTime,
#endif
				m_Config.bDecompressDDS);

	if (m_Config.bWindowed == 1) {
		fprintf(fp, "WINDOWED			%d\n", m_Config.bWindowed);
	}
	if (m_Config.bViewChat == 0) {
		fprintf(fp, "VIEW_CHAT			%d\n", m_Config.bViewChat);
	}
	if (m_Config.bAlwaysShowName != DEFAULT_VALUE_ALWAYS_SHOW_NAME) {
		fprintf(fp, "ALWAYS_VIEW_NAME	%d\n", m_Config.bAlwaysShowName);
	}
	if (m_Config.bShowDamage == 0) {
		fprintf(fp, "SHOW_DAMAGE		%d\n", m_Config.bShowDamage);
	}
	if (m_Config.bShowSalesText == 0) {
		fprintf(fp, "SHOW_SALESTEXT		%d\n", m_Config.bShowSalesText);
	}
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
	if (m_Config.bShowMobAIFlag == 0) {
		fprintf(fp, "SHOW_MOBAIFLAG		%d\n", m_Config.bShowMobAIFlag);
	}
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
	if (m_Config.bShowMobLevel == 0) {
		fprintf(fp, "SHOW_MOBLEVEL		%d\n", m_Config.bShowMobLevel);
	}
#endif
#ifdef ENABLE_MULTI_LANGUAGE
	fprintf(fp, "LANGUAGE_FILTER		%s\n", m_Config.szLanguageFilter.c_str());
	fprintf(fp, "AUTO_TRANSLATE_W		%d\n", m_Config.bAutoTranslateWhisper);
#endif
#ifdef ENABLE_BIOLOGIST_UI
	fprintf(fp, "BIOLOGIST_ALERT		%d\n", m_Config.biologist_alert);
#endif
#ifdef ENABLE_SAVECAMERA_PREFERENCES
	fprintf(fp, "CAMERA_TYPE			%d\n", m_Config.bCameraType);
	fprintf(fp, "CAMERA_HEIGHT			%.1f\n", m_Config.fCameraHeight);
#endif
#ifdef OUTLINE_NAMES_TEXTLINE
	fprintf(fp, "INLINE_NAMES			%d\n", m_Config.bNamesOutline);
#endif
#ifdef ENABLE_AUTO_PICKUP
	fprintf(fp, "AUTO_PICKUP			%d\n", m_Config.bPickUpType);
#endif
#ifdef ENABLE_NEW_CHAT
	fprintf(fp, "CHAT_FILTER			%d\n", m_Config.iChatFilter);
#endif
	fprintf(fp, "USE_DEFAULT_IME		%d\n", m_Config.bUseDefaultIME);
	fprintf(fp, "SOFTWARE_TILING		%d\n", m_Config.bSoftwareTiling);
	fprintf(fp, "SHADOW_LEVEL			%d\n", m_Config.iShadowLevel);
#ifdef ENABLE_PERSPECTIVE_VIEW
	fprintf(fp, "PERSPECTIVE_FIELD		%.2f\n", m_Config.fField);
#endif
	fprintf(fp, "ENVIRONMENT			%d\n", m_Config.iEnvironment);
	fprintf(fp, "TIMEPM					%d\n", m_Config.bTimePm);
	fprintf(fp, "HIDE1_MODE				%d\n", m_Config.bHide1Mode);
	fprintf(fp, "HIDE2_MODE				%d\n", m_Config.bHide2Mode);
	fprintf(fp, "HIDE3_MODE				%d\n", m_Config.bHide3Mode);
	fprintf(fp, "HIDE4_MODE				%d\n", m_Config.bHide4Mode);
	fprintf(fp, "HIDE5_MODE				%d\n", m_Config.bHide5Mode);
	fprintf(fp, "HIDE6_MODE				%d\n", m_Config.bHide6Mode);
	fprintf(fp, "HIDE7_MODE				%d\n", m_Config.bHide7Mode);
	fprintf(fp, "HIDE1_MODE2			%d\n", m_Config.bHide1Mode2);
	fprintf(fp, "HIDE2_MODE2			%d\n", m_Config.bHide2Mode2);
	fprintf(fp, "HIDE3_MODE2			%d\n", m_Config.bHide3Mode2);
	fprintf(fp, "HIDE4_MODE2			%d\n", m_Config.bHide4Mode2);

	fprintf(fp, "\n");
	fclose(fp);
	return true;
}

bool CPythonSystem::LoadInterfaceStatus()
{
	FILE * File;
	File = fopen("interface.cfg", "rb");

	if (!File)
		return false;

	fread(m_WindowStatus, 1, sizeof(TWindowStatus) * WINDOW_MAX_NUM, File);
	fclose(File);
	return true;
}

void CPythonSystem::SaveInterfaceStatus()
{
	if (!m_poInterfaceHandler)
		return;

	PyCallClassMemberFunc(m_poInterfaceHandler, "OnSaveInterfaceStatus", Py_BuildValue("()"));

	FILE * File;

	File = fopen("interface.cfg", "wb");

	if (!File)
	{
		TraceError("Cannot open interface.cfg");
		return;
	}

	fwrite(m_WindowStatus, 1, sizeof(TWindowStatus) * WINDOW_MAX_NUM, File);
	fclose(File);
}

bool CPythonSystem::isInterfaceConfig()
{
	return m_isInterfaceConfig;
}

const CPythonSystem::TWindowStatus & CPythonSystem::GetWindowStatusReference(int iIndex)
{
	return m_WindowStatus[iIndex];
}

void CPythonSystem::ApplyConfig()
{
	if (m_OldConfig.gamma != m_Config.gamma)
	{
		float val = 1.0f;

		switch (m_Config.gamma)
		{
			case 0: val = 0.4f;	break;
			case 1: val = 0.7f; break;
			case 2: val = 1.0f; break;
			case 3: val = 1.2f; break;
			case 4: val = 1.4f; break;
		}

		CPythonGraphic::Instance().SetGamma(val);
	}

	if (m_OldConfig.is_software_cursor != m_Config.is_software_cursor)
	{
		if (m_Config.is_software_cursor)
			CPythonApplication::Instance().SetCursorMode(CPythonApplication::CURSOR_MODE_SOFTWARE);
		else
			CPythonApplication::Instance().SetCursorMode(CPythonApplication::CURSOR_MODE_HARDWARE);
	}

	m_OldConfig = m_Config;

	ChangeSystem();
}

void CPythonSystem::ChangeSystem()
{
	SoundEngine& rkSndMgr = SoundEngine::Instance();
	rkSndMgr.SetMusicVolume(m_Config.music_volume);
	rkSndMgr.SetSoundVolume(m_Config.voice_volume);
}

void CPythonSystem::Clear()
{
	SetInterfaceHandler(nullptr);
}

CPythonSystem::CPythonSystem()
{
	memset(&m_Config, 0, sizeof(TConfig));

	m_poInterfaceHandler = nullptr;

	SetDefaultConfig();

	LoadConfig();

	ChangeSystem();

	if (LoadInterfaceStatus())
		m_isInterfaceConfig = true;
	else
		m_isInterfaceConfig = false;
}

CPythonSystem::~CPythonSystem()
{
	assert(m_poInterfaceHandler==NULL && "CPythonSystem MUST CLEAR!");
}

#if defined(__BL_PICK_FILTER__)
#include <fstream>

CPythonSystem::CPickUpFilter::CPickUpFilter()
{
	///// READ ////
	std::ifstream inputFile;
	inputFile.open(cPickUpFilterFileName, std::ios::binary);
	if (inputFile.is_open())
	{
		inputFile.read(reinterpret_cast<char*>(bPickFilter), sizeof(bool) * EPICKFILTER::EPICKFILTER_MAX);
		inputFile.read(reinterpret_cast<char*>(bPickSize), sizeof(bool) * ESIZE::ESIZE_MAX);
		inputFile.read(reinterpret_cast<char*>(&bModeAll), sizeof(bool));
		inputFile.read(reinterpret_cast<char*>(&m_bRefineMin), sizeof(uint8_t));
		inputFile.read(reinterpret_cast<char*>(&m_bRefineMax), sizeof(uint8_t));
		inputFile.read(reinterpret_cast<char*>(&m_lLevelMin), sizeof(long));
		inputFile.read(reinterpret_cast<char*>(&m_lLevelMax), sizeof(long));

		inputFile.close();
	}
	else
	{
		///// SET DEFAULT ////
		std::fill(std::begin(bPickFilter), std::end(bPickFilter), true);
		std::fill(std::begin(bPickSize), std::end(bPickSize), true);

		bModeAll = false;

		m_bRefineMin = 0;
		m_bRefineMax = 9;

		m_lLevelMin = 0;
		m_lLevelMax = 999;
	}
}

CPythonSystem::CPickUpFilter::~CPickUpFilter()
{
	///// SAVE ////
	std::ofstream outputFile;
	outputFile.open(cPickUpFilterFileName, std::ios::binary);
	if (outputFile.is_open())
	{
		outputFile.write(reinterpret_cast<char*>(bPickFilter), sizeof(bool) * EPICKFILTER::EPICKFILTER_MAX);
		outputFile.write(reinterpret_cast<char*>(bPickSize), sizeof(bool) * ESIZE::ESIZE_MAX);
		outputFile.write(reinterpret_cast<char*>(&bModeAll), sizeof(bool));
		outputFile.write(reinterpret_cast<char*>(&m_bRefineMin), sizeof(uint8_t));
		outputFile.write(reinterpret_cast<char*>(&m_bRefineMax), sizeof(uint8_t));
		outputFile.write(reinterpret_cast<char*>(&m_lLevelMin), sizeof(long));
		outputFile.write(reinterpret_cast<char*>(&m_lLevelMax), sizeof(long));

		outputFile.close();
	}
	else
	{
		Tracenf("CPickUpFilter::~CPickUpFilter() Cannot create a file for save settings.");
	}
}

void CPythonSystem::CPickUpFilter::SetFilter(size_t sIndex, bool b)
{
	if (sIndex >= EPICKFILTER::EPICKFILTER_MAX)
	{
		Tracenf("CPickUpFilter::SetFilter(Index=%d) : Out of range", sIndex);
		return;
	}

	bPickFilter[sIndex] = b;
}

void CPythonSystem::CPickUpFilter::SetSize(size_t sIndex, bool b)
{
	if (sIndex >= ESIZE::ESIZE_MAX)
	{
		Tracenf("CPickUpFilter::SetSize(Index=%d) : Out of range", sIndex);
		return;
	}

	if (sIndex == ESIZE::BIG && b == false)
		SetFilter(EPICKFILTER::SUB_WEAPON_TWO_HANDED, false);

	bPickSize[sIndex] = b;
}

void CPythonSystem::CPickUpFilter::SetRefine(uint8_t min, uint8_t max)
{
	m_bRefineMin = static_cast<uint8_t>(MINMAX(0, min, 9));
	m_bRefineMax = static_cast<uint8_t>(MINMAX(0, max, 9));
}

void CPythonSystem::CPickUpFilter::SetLevel(long min, long max)
{
	m_lLevelMin = MINMAX(0, min, 999);
	m_lLevelMax = MINMAX(0, max, 999);
}

void CPythonSystem::CPickUpFilter::SetModeAll(bool b)
{
	bModeAll = b;
}

bool CPythonSystem::CPickUpFilter::CanPickItem(DWORD dwIID)
{
	CItemData* pItemData;
	if (!CItemManager::Instance().GetItemDataPointer(CPythonItem::Instance().GetVirtualNumberOfGroundItem(dwIID), &pItemData))
	{
		Tracenf("CPickUpFilter::CanPickItem(dwIID=%d) : Non-exist item.", dwIID);
		return true;
	}

	if (CheckRefine(pItemData) == false)
		return false;

	if (CheckLevel(pItemData) == false)
		return false;

	if (CheckSize(pItemData) == false)
		return false;

	if (CheckType(pItemData) == false)
		return false;

	return true;
}

std::pair<uint8_t, uint8_t> CPythonSystem::CPickUpFilter::GetRefine()
{
	return std::make_pair(m_bRefineMin, m_bRefineMax);
}

std::pair<long, long> CPythonSystem::CPickUpFilter::GetLevel()
{
	return std::make_pair(m_lLevelMin, m_lLevelMax);
}

bool CPythonSystem::CPickUpFilter::GetFilter(size_t sIndex) const
{
	if (sIndex >= EPICKFILTER::EPICKFILTER_MAX)
	{
		Tracenf("CPickUpFilter::GetFilter(Index=%d) : Out of range", sIndex);
		return false;
	}

	return bPickFilter[sIndex];
}

bool CPythonSystem::CPickUpFilter::GetSize(size_t sIndex) const
{
	if (sIndex >= ESIZE::ESIZE_MAX)
	{
		Tracenf("CPickUpFilter::GetSize(Index=%d) : Out of range", sIndex);
		return false;
	}

	return bPickSize[sIndex];
}

bool CPythonSystem::CPickUpFilter::IsModeAll() const
{
	return bModeAll;
}

bool CPythonSystem::CPickUpFilter::CheckRefine(const CItemData* pItem) const
{
	const uint8_t bRefineLevel = static_cast<uint8_t>(pItem->GetRefine());
	if (bRefineLevel >= m_bRefineMin && bRefineLevel <= m_bRefineMax)
		return true;

	return false;
}

bool CPythonSystem::CPickUpFilter::CheckLevel(const CItemData* pItem) const
{
	CItemData::TItemLimit ItemLimit;
	for (uint8_t i = 0; i < CItemData::ITEM_LIMIT_MAX_NUM; i++)
	{
		if (!pItem->GetLimit(i, &ItemLimit))
			continue;

		if (ItemLimit.bType != CItemData::LIMIT_LEVEL)
			continue;

		const long lLimitLevel = ItemLimit.lValue;
		return (lLimitLevel >= m_lLevelMin && lLimitLevel <= m_lLevelMax);
	}

	return true;
}

bool CPythonSystem::CPickUpFilter::CheckSize(const CItemData* pItem) const
{
	return GetSize(pItem->GetSize() - 1);
}

bool CPythonSystem::CPickUpFilter::CheckType(const CItemData* pItem) const
{
	const uint8_t bType = pItem->GetType();
	const uint8_t bSubType = pItem->GetSubType();

	switch (bType)
	{
	case CItemData::EItemType::ITEM_TYPE_WEAPON:
		switch (bSubType)
		{
		case CItemData::EWeaponSubTypes::WEAPON_SWORD:
			return GetFilter(EPICKFILTER::SUB_WEAPON_SWORD);

		case CItemData::EWeaponSubTypes::WEAPON_DAGGER:
			return GetFilter(EPICKFILTER::SUB_WEAPON_DAGGER);

		case CItemData::EWeaponSubTypes::WEAPON_BOW:
			return GetFilter(EPICKFILTER::SUB_WEAPON_BOW);

		case CItemData::EWeaponSubTypes::WEAPON_TWO_HANDED:
			return GetFilter(EPICKFILTER::SUB_WEAPON_TWO_HANDED);

		case CItemData::EWeaponSubTypes::WEAPON_BELL:
			return GetFilter(EPICKFILTER::SUB_WEAPON_BELL);

		case CItemData::EWeaponSubTypes::WEAPON_FAN:
			return GetFilter(EPICKFILTER::SUB_WEAPON_FAN);

		case CItemData::EWeaponSubTypes::WEAPON_ARROW:
			return GetFilter(EPICKFILTER::SUB_WEAPON_ARROW);

			/*case CItemData::EWeaponSubTypes::WEAPON_MOUNT_SPEAR:
				return GetFilter(EPICKFILTER::SUB_WEAPON_MOUNT_SPEAR);*/
		}
		break;

	case CItemData::EItemType::ITEM_TYPE_ARMOR:
		switch (bSubType)
		{
		case CItemData::EArmorSubTypes::ARMOR_BODY:
			return GetFilter(EPICKFILTER::SUB_ARMOR_BODY);

		case CItemData::EArmorSubTypes::ARMOR_HEAD:
			return GetFilter(EPICKFILTER::SUB_ARMOR_HEAD);

		case CItemData::EArmorSubTypes::ARMOR_SHIELD:
			return GetFilter(EPICKFILTER::SUB_ARMOR_SHIELD);

		case CItemData::EArmorSubTypes::ARMOR_WRIST:
			return GetFilter(EPICKFILTER::SUB_ARMOR_WRIST);

		case CItemData::EArmorSubTypes::ARMOR_FOOTS:
			return GetFilter(EPICKFILTER::SUB_ARMOR_FOOTS);

		case CItemData::EArmorSubTypes::ARMOR_NECK:
			return GetFilter(EPICKFILTER::SUB_ARMOR_NECK);

		case CItemData::EArmorSubTypes::ARMOR_EAR:
			return GetFilter(EPICKFILTER::SUB_ARMOR_EAR);
		}
		break;

	case CItemData::EItemType::ITEM_TYPE_METIN:
		return GetFilter(EPICKFILTER::TYPE_METIN);

	case CItemData::EItemType::ITEM_TYPE_ELK:
		return GetFilter(EPICKFILTER::TYPE_YANG);

	case CItemData::EItemType::ITEM_TYPE_SKILLBOOK:
		return GetFilter(EPICKFILTER::TYPE_SKILLBOOK);

	case CItemData::EItemType::ITEM_TYPE_GIFTBOX:
		return GetFilter(EPICKFILTER::TYPE_GIFTBOX);

	case CItemData::EItemType::ITEM_TYPE_BELT:
		return GetFilter(EPICKFILTER::TYPE_BELT);

	case CItemData::EItemType::ITEM_TYPE_POLYMORPH:
		return GetFilter(EPICKFILTER::TYPE_POLY);

	case CItemData::EItemType::ITEM_TYPE_RING:
		return GetFilter(EPICKFILTER::TYPE_RING);

	case CItemData::EItemType::ITEM_TYPE_USE:
		switch (bSubType)
		{
		case CItemData::EUseSubTypes::USE_POTION:
		case CItemData::EUseSubTypes::USE_ABILITY_UP:
		case CItemData::EUseSubTypes::USE_POTION_NODELAY:
		case CItemData::EUseSubTypes::USE_POTION_CONTINUE:
			return GetFilter(EPICKFILTER::SUB_POTION);
		}
		break;

	case CItemData::EItemType::ITEM_TYPE_MATERIAL:
		return GetFilter(EPICKFILTER::TYPE_MATERIAL);
	}

	return true;
}
#endif
