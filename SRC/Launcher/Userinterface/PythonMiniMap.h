#pragma once

#include "PythonBackground.h"

class CPythonMiniMap : public CScreen, public CSingleton<CPythonMiniMap>
{
	public:
		enum
		{
			EMPIRE_NUM = 4,

			MINI_WAYPOINT_IMAGE_COUNT = 12,
			WAYPOINT_IMAGE_COUNT = 15,
			TARGET_MARK_IMAGE_COUNT = 2,
		};
		enum
		{
			TYPE_OPC,
			TYPE_OPCPVP,
			TYPE_OPCPVPSELF,
			TYPE_NPC,
			TYPE_MONSTER,
			TYPE_WARP,
			TYPE_WAYPOINT,
			TYPE_PARTY,
			TYPE_EMPIRE,
			TYPE_EMPIRE_END = TYPE_EMPIRE + EMPIRE_NUM,
			TYPE_TARGET,
#ifdef ENABLE_ATLAS_BOSS
			TYPE_BOSS,
#endif
			TYPE_COUNT,
		};

	public:
		CPythonMiniMap();
		virtual ~CPythonMiniMap();

		void Destroy();
		bool Create();

		bool IsAtlas();
		bool CanShow();
		bool CanShowAtlas();

		void SetMiniMapSize(float fWidth, float fHeight);
		void SetScale(float fScale);
		void ScaleUp();
		void ScaleDown();

		void SetCenterPosition(float fCenterX, float fCenterY);

		void Update(float fCenterX, float fCenterY);
		void Render(float fScreenX, float fScreenY);

		void Show();
		void Hide();

		bool GetPickedInstanceInfo(float fScreenX, float fScreenY, std::string & rReturnName, float * pReturnPosX, float * pReturnPosY, uint32_t * pdwTextColor);

		// Atlas
		bool LoadAtlas();
		void UpdateAtlas();
		void RenderAtlas(float fScreenX, float fScreenY);
		void ShowAtlas();
		void HideAtlas();
#ifdef ENABLE_ATLAS_BOSS
		bool GetAtlasInfo(float fScreenX, float fScreenY, std::string & rReturnString, float * pReturnPosX, float * pReturnPosY, uint32_t * pdwTextColor, uint32_t * pdwGuildID, long * lTime);
#else
		bool GetAtlasInfo(float fScreenX, float fScreenY, std::string & rReturnString, float * pReturnPosX, float * pReturnPosY, uint32_t * pdwTextColor, uint32_t * pdwGuildID);
#endif
		bool GetAtlasSize(float * pfSizeX, float * pfSizeY);

		void AddObserver(uint32_t dwVID, float fSrcX, float fSrcY);
		void MoveObserver(uint32_t dwVID, float fDstX, float fDstY);
		void RemoveObserver(uint32_t dwVID);

		// WayPoint
		void AddWayPoint(uint8_t byType, uint32_t dwID, float fX, float fY, std::string strText, uint32_t dwChrVID=0);
		void RemoveWayPoint(uint32_t dwID);

		// SignalPoint
		void AddSignalPoint(float fX, float fY);
		void ClearAllSignalPoint();

		void RegisterAtlasWindow(PyObject* poHandler);
		void UnregisterAtlasWindow();
		void OpenAtlasWindow();
		void SetAtlasCenterPosition(int x, int y);

#ifdef ENABLE_ATLAS_BOSS
		void ClearAtlasMarkInfoBoss();
		void RegisterAtlasMarkBoss(uint8_t byType, const char * c_szName, int32_t lx, int32_t ly, int32_t lTime);
#endif

		// NPC List
		void ClearAtlasMarkInfo();
		void RegisterAtlasMark(uint8_t byType, const char * c_szName, int32_t lx, int32_t ly);
		// Guild
		void ClearGuildArea();
		void RegisterGuildArea(uint32_t dwID, uint32_t dwGuildID, int32_t x, int32_t y, int32_t width, int32_t height);
		uint32_t GetGuildAreaID(uint32_t x, uint32_t y);

		// Target
		void CreateTarget(int iID, const char * c_szName);
		void CreateTarget(int iID, const char * c_szName, uint32_t dwVID);
		void UpdateTarget(int iID, int ix, int iy);
		void DeleteTarget(int iID);

	protected:
		void __Initialize();
		void __SetPosition();
		void __LoadAtlasMarkInfo();

		void __RenderWayPointMark(int ixCenter, int iyCenter);
		void __RenderMiniWayPointMark(int ixCenter, int iyCenter);
		void __RenderTargetMark(int ixCenter, int iyCenter);

		void __GlobalPositionToAtlasPosition(long lx, long ly, float * pfx, float * pfy);

	protected:
		// Atlas
		struct TAtlasMarkInfo
		{
			uint8_t		m_byType;
			uint32_t		m_dwID;
			float		m_fX;
			float		m_fY;
			float		m_fScreenX;
			float		m_fScreenY;
			float		m_fMiniMapX;
			float		m_fMiniMapY;
			uint32_t		m_dwChrVID;
			std::string	m_strText;
#ifdef ENABLE_ATLAS_BOSS
			int32_t		lTime;
#endif

			TAtlasMarkInfo() :
				m_byType(0), m_dwID(0), m_fX(0.0f), m_fY(0.0f),
				m_fScreenX(0.0f), m_fScreenY(0.0f), m_fMiniMapX(0.0f), m_fMiniMapY(0.0f),
				m_dwChrVID(0),
#ifdef ENABLE_ATLAS_BOSS
				lTime(0),
#endif
				m_strText("")
			{}
		};

		// GuildArea
		typedef struct
		{
			uint32_t dwGuildID;
			int32_t lx, ly;
			int32_t lwidth, lheight;

			float fsxRender, fsyRender;
			float fexRender, feyRender;
		} TGuildAreaInfo;

		struct SObserver
		{
			float fCurX;
			float fCurY;
			float fSrcX;
			float fSrcY;
			float fDstX;
			float fDstY;

			uint32_t dwSrcTime;
			uint32_t dwDstTime;
		};

		// 캐릭터 리스트
		typedef struct
		{
			float	m_fX;
			float	m_fY;
			UINT	m_eNameColor;
		} TMarkPosition;

		typedef std::vector<TMarkPosition>				TInstanceMarkPositionVector;
		typedef TInstanceMarkPositionVector::iterator	TInstancePositionVectorIterator;

	protected:
		bool __GetWayPoint(uint32_t dwID, TAtlasMarkInfo ** ppkInfo);
		void __UpdateWayPoint(TAtlasMarkInfo * pkInfo, int ix, int iy);

	protected:
		float							m_fWidth;
		float							m_fHeight;

		float							m_fScale;

		float							m_fCenterX;
		float							m_fCenterY;

		float							m_fCenterCellX;
		float							m_fCenterCellY;

		float							m_fScreenX;
		float							m_fScreenY;

		float							m_fMiniMapRadius;

		// 맵 그림...
		LPDIRECT3DTEXTURE9				m_lpMiniMapTexture[AROUND_AREA_NUM];

		// 미니맵 커버
		CGraphicImageInstance			m_MiniMapFilterGraphicImageInstance;
		CGraphicExpandedImageInstance	m_MiniMapCameraraphicImageInstance;

		// 캐릭터 마크
		CGraphicExpandedImageInstance	m_PlayerMark;
		CGraphicImageInstance			m_WhiteMark;

		TInstanceMarkPositionVector		m_PartyPCPositionVector;
		TInstanceMarkPositionVector		m_OtherPCPositionVector;
		TInstanceMarkPositionVector		m_NPCPositionVector;
		TInstanceMarkPositionVector		m_MonsterPositionVector;
		TInstanceMarkPositionVector		m_StonePositionVector;
		TInstanceMarkPositionVector		m_BossPositionVector;
		TInstanceMarkPositionVector		m_WarpPositionVector;
		std::map<uint32_t, SObserver>		m_kMap_dwVID_kObserver;

		bool							m_bAtlas;
		bool							m_bShow;

		CGraphicVertexBuffer			m_VertexBuffer;
		CGraphicIndexBuffer				m_IndexBuffer;

		D3DXMATRIX						m_matIdentity;
		D3DXMATRIX						m_matWorld;
		D3DXMATRIX						m_matMiniMapCover;

		bool							m_bShowAtlas;
		CGraphicImageInstance			m_AtlasImageInstance;
		D3DXMATRIX						m_matWorldAtlas;
		CGraphicExpandedImageInstance	m_AtlasPlayerMark;

		float							m_fAtlasScreenX;
		float							m_fAtlasScreenY;

		uint32_t							m_dwAtlasBaseX;
		uint32_t							m_dwAtlasBaseY;

		float							m_fAtlasMaxX;
		float							m_fAtlasMaxY;

		float							m_fAtlasImageSizeX;
		float							m_fAtlasImageSizeY;

		typedef std::vector<TAtlasMarkInfo>		TAtlasMarkInfoVector;
		typedef TAtlasMarkInfoVector::iterator	TAtlasMarkInfoVectorIterator;
		typedef std::vector<TGuildAreaInfo>		TGuildAreaInfoVector;
		typedef TGuildAreaInfoVector::iterator	TGuildAreaInfoVectorIterator;
		TAtlasMarkInfoVectorIterator			m_AtlasMarkInfoVectorIterator;
		TAtlasMarkInfoVector					m_AtlasNPCInfoVector;
		TAtlasMarkInfoVector					m_AtlasWarpInfoVector;
#ifdef ENABLE_ATLAS_BOSS
		TAtlasMarkInfoVector					m_AtlasBossInfoVector;
		CGraphicImageInstance					m_BossMark;
#endif

		// WayPoint
		CGraphicExpandedImageInstance			m_MiniWayPointGraphicImageInstances[MINI_WAYPOINT_IMAGE_COUNT];
		CGraphicExpandedImageInstance			m_WayPointGraphicImageInstances[WAYPOINT_IMAGE_COUNT];
		CGraphicExpandedImageInstance			m_TargetMarkGraphicImageInstances[TARGET_MARK_IMAGE_COUNT];
		CGraphicImageInstance					m_GuildAreaFlagImageInstance;
		TAtlasMarkInfoVector					m_AtlasWayPointInfoVector;
		TGuildAreaInfoVector					m_GuildAreaInfoVector;

		// SignalPoint
		struct TSignalPoint
		{
			D3DXVECTOR2 v2Pos;
			unsigned int id;

			TSignalPoint() : id(0) {}
		};
		std::vector<TSignalPoint>				m_SignalPointVector;

		PyObject*							m_poHandler;
};