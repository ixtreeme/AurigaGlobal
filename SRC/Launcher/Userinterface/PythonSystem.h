#pragma once

class CPythonSystem : public CSingleton<CPythonSystem>
{
	public:
		enum EWindow
		{
			WINDOW_STATUS,
			WINDOW_INVENTORY,
			WINDOW_ABILITY,
			WINDOW_SOCIETY,
			WINDOW_JOURNAL,
			WINDOW_COMMAND,

			WINDOW_QUICK,
			WINDOW_GAUGE,
			WINDOW_MINIMAP,
			WINDOW_CHAT,

			WINDOW_MAX_NUM,
		};

		enum
		{
			FREQUENCY_MAX_NUM  = 30,
			RESOLUTION_MAX_NUM = 100
		};

		typedef struct SResolution
		{
			uint32_t	width;
			uint32_t	height;
			uint32_t	bpp;		// bits per pixel (high-color = 16bpp, true-color = 32bpp)

			uint32_t	frequency[20];
			uint8_t	frequency_count;
		} TResolution;

		typedef struct SWindowStatus
		{
			int		isVisible;
			int		isMinimized;

			int		ixPosition;
			int		iyPosition;
			int		iHeight;
		} TWindowStatus;

		typedef struct SConfig
		{
			uint32_t width = 0;
			uint32_t height = 0;
			uint32_t bpp = 0;
			uint32_t frequency = 0;

			bool is_software_cursor = false;
			bool is_object_culling = false;
			int iDistance = 0;
			int iShadowLevel = 0;

			float music_volume = 0.0f;
			uint8_t voice_volume = 0;

			int gamma = 0;

			int isSaveID = 0;
			char SaveID[20] = { 0 };

			bool bWindowed = false;
			bool bDecompressDDS = false;
			bool bNoSoundCard = false;
			bool bUseDefaultIME = false;
			uint8_t bSoftwareTiling = 0;
			bool bViewChat = false;
			bool bAlwaysShowName = false;
			bool bShowDamage = false;
			bool bShowSalesText = false;
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
			bool bShowMobAIFlag = false;
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
			bool bShowMobLevel = false;
#endif
#ifdef ENABLE_MULTI_LANGUAGE
			std::string szLanguageFilter;
			bool bAutoTranslateWhisper = false;
#endif
#ifdef ENABLE_PERSPECTIVE_VIEW
			float fField = 0.0f;
#endif
#ifdef ENABLE_BIOLOGIST_UI
			bool biologist_alert = false;
#endif
#ifdef ENABLE_SAVECAMERA_PREFERENCES
			uint8_t bCameraType = 0;
			float fCameraHeight = 0.0f;
#endif
#ifdef OUTLINE_NAMES_TEXTLINE
			bool bNamesOutline = false;
#endif
#ifdef ENABLE_AUTO_PICKUP
			bool bPickUpType = false;
#endif
#ifdef ENABLE_NEW_CHAT
			int iChatFilter = 0;
#endif
			int iEnvironment = 0;
			bool bTimePm = false;
			bool bHide1Mode = false;
			bool bHide2Mode = false;
			bool bHide3Mode = false;
			bool bHide4Mode = false;
			bool bHide5Mode = false;
			bool bHide6Mode = false;
			bool bHide7Mode = false;
			bool bHide1Mode2 = false;
			bool bHide2Mode2 = false;
			bool bHide3Mode2 = false;
			bool bHide4Mode2 = false;


			SConfig() = default; // üres konstruktor, minden mezõ a fenti alapérték
		} TConfig;
#if defined(__BL_PICK_FILTER__)
		class CPickUpFilter final
		{
		public:
			CPickUpFilter();
			~CPickUpFilter();

			void	SetFilter(size_t sIndex, bool b);
			void	SetSize(size_t sIndex, bool b);
			void	SetRefine(uint8_t min, uint8_t max);
			void	SetLevel(long min, long max);
			void	SetModeAll(bool b);

			bool	CanPickItem(DWORD dwIID);

			bool	GetFilter(size_t sIndex) const;
			bool	GetSize(size_t sIndex) const;
			bool	IsModeAll() const;

			std::pair<uint8_t, uint8_t> GetRefine();
			std::pair<long, long> GetLevel();

		private:
			bool	CheckRefine(const CItemData* pItem) const;
			bool	CheckLevel(const CItemData* pItem) const;
			bool	CheckSize(const CItemData* pItem) const;
			bool	CheckType(const CItemData* pItem) const;

			static constexpr const char* cPickUpFilterFileName = "PickUpFilter.dat";

		public:
			enum EPICKFILTER
			{
				/*WEAPON-SUB*/
				SUB_WEAPON_SWORD,
				SUB_WEAPON_DAGGER,
				SUB_WEAPON_BOW,
				SUB_WEAPON_TWO_HANDED,
				SUB_WEAPON_BELL,
				SUB_WEAPON_FAN,
				SUB_WEAPON_ARROW,
				//SUB_WEAPON_MOUNT_SPEAR,
				/*WEAPON-SUB*/

				/*ARMOR-SUB*/
				SUB_ARMOR_BODY,
				SUB_ARMOR_HEAD,
				SUB_ARMOR_SHIELD,
				SUB_ARMOR_WRIST,
				SUB_ARMOR_FOOTS,
				SUB_ARMOR_NECK,
				SUB_ARMOR_EAR,
				/*ARMOR-SUB*/

				/*OTHER*/
				TYPE_METIN,
				TYPE_YANG,
				TYPE_SKILLBOOK,
				TYPE_GIFTBOX,
				TYPE_BELT,
				TYPE_POLY,
				TYPE_RING,
				SUB_POTION,
				TYPE_MATERIAL,
				/*OTHER*/

				EPICKFILTER_MAX
			};

			enum ESIZE
			{
				SMALL,
				MID,
				BIG,

				ESIZE_MAX
			};

		private:
			bool bPickFilter[EPICKFILTER::EPICKFILTER_MAX];
			bool bPickSize[ESIZE::ESIZE_MAX];

			bool bModeAll;

			uint8_t m_bRefineMin;
			uint8_t m_bRefineMax;

			long m_lLevelMin;
			long m_lLevelMax;
		} TPickUpFilter;
#endif
	public:
		CPythonSystem();
		virtual ~CPythonSystem();

		void Clear();
		void SetInterfaceHandler(PyObject * poHandler);
		void DestroyInterfaceHandler();

		// Config
		void							SetDefaultConfig();
		bool							LoadConfig();
		bool							SaveConfig();
		void							ApplyConfig();
		void							SetConfig(TConfig * set_config);
		TConfig *						GetConfig();
		void							ChangeSystem();

		// Interface
		bool							LoadInterfaceStatus();
		void							SaveInterfaceStatus();
		bool							isInterfaceConfig();
		const TWindowStatus &			GetWindowStatusReference(int iIndex);

		uint32_t							GetWidth();
		uint32_t							GetHeight();
		uint32_t							GetBPP();
		uint32_t							GetFrequency();
		bool							IsSoftwareCursor();
		bool							IsWindowed();
		bool							IsViewChat();
		bool							IsAlwaysShowName();
		bool							IsShowDamage();
		bool							IsShowSalesText();
		bool							IsUseDefaultIME();
		bool							IsNoSoundCard();
		bool							IsAutoTiling();
		bool							IsSoftwareTiling();
		void							SetSoftwareTiling(bool isEnable);
		void							SetViewChatFlag(int iFlag);
		void							SetAlwaysShowNameFlag(int iFlag);
		void							SetShowDamageFlag(int iFlag);
		void							SetShowSalesTextFlag(int iFlag);
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBAIFLAG)
		bool							IsShowMobAIFlag();
		void							SetShowMobAIFlagFlag(int iFlag);
#endif
#if defined(WJ_SHOW_MOB_INFO) && defined(ENABLE_SHOW_MOBLEVEL)
		bool							IsShowMobLevel();
		void							SetShowMobLevelFlag(int iFlag);
#endif

#ifdef ENABLE_MULTI_LANGUAGE
		void							SetChatFilterValue(std::string szFilter);
		std::string 					GetChatFilterValue();
		bool							IsAutoTranslateWhisper();
		void							SetAutoTranslateWhisper(int iFlag);
#endif

		// Window
		void							SaveWindowStatus(int iIndex, int iVisible, int iMinimized, int ix, int iy, int iHeight);

		// SaveID
		int								IsSaveID();
		const char *					GetSaveID();
		void							SetSaveID(int iValue, const char * c_szSaveID);

		/// Display
		void							GetDisplaySettings();

		int								GetResolutionCount();
		int								GetFrequencyCount(int index);
		bool							GetResolution(int index, OUT uint32_t *width, OUT uint32_t *height, OUT uint32_t *bpp);
		bool							GetFrequency(int index, int freq_index, OUT uint32_t *frequncy);
		int								GetResolutionIndex(uint32_t width, uint32_t height, uint32_t bpp);
		int								GetFrequencyIndex(int res_index, uint32_t frequency);
		bool							isViewCulling();

		// Sound
		float							GetMusicVolume();
		int								GetSoundVolume();
		void							SetMusicVolume(float fVolume);
		void							SetSoundVolumef(float fVolume);

		int		GetDistance();
#ifdef ENABLE_PERSPECTIVE_VIEW
		float	GetFieldPerspective();
		void	SetFieldPerspective(float fValue);
#endif
		int								GetShadowLevel();
		void							SetShadowLevel(unsigned int level);
#ifdef ENABLE_BIOLOGIST_UI
		bool	GetBiologistAlert();
		void	SetBiologistAlert(bool value);
#endif
#ifdef ENABLE_SAVECAMERA_PREFERENCES
		uint8_t	GetCameraType();
		void	SetCameraType(uint8_t value);
		float	GetCameraHeight();
		void	SetCameraHeight(float value);
#endif
#ifdef OUTLINE_NAMES_TEXTLINE
		bool	GetNamesType();
		void	SetNamesType(bool value);
#endif
#ifdef ENABLE_AUTO_PICKUP
		bool	GetPickUpMode();
		void	SetPickUpMode(int value);
#endif
#ifdef ENABLE_NEW_CHAT
		void	SetChatFilter(int value);
		int		GetChatFilter();
#endif
		int		GetEnvironment();
		void	SetEnvironment(int value);
		bool	GetTimePm();
		void	SetTimePm(bool value);
		void	SetHideModeStatus(int type, int value);
		bool	GetHideMode1Status();
		bool	GetHideMode2Status();
		bool	GetHideMode3Status();
		bool	GetHideMode4Status();
		bool	GetHideMode5Status();
		bool	GetHideMode6Status();
		bool	GetHideMode7Status();
		void	SetHideModeStatus2(int type, int value);
		bool	GetHideMode1Status2();
		bool	GetHideMode2Status2();
		bool	GetHideMode3Status2();
		bool	GetHideMode4Status2();


	protected:
		TResolution						m_ResolutionList[RESOLUTION_MAX_NUM];
		int								m_ResolutionCount;

		TConfig							m_Config;
		TConfig							m_OldConfig;

		bool							m_isInterfaceConfig;
		PyObject *						m_poInterfaceHandler;
		TWindowStatus					m_WindowStatus[WINDOW_MAX_NUM];
};