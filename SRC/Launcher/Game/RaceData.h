#pragma once

#include "../Granny/Thing.h"

class CRaceMotionData;
class CAttributeData;

#define COMBO_KEY									uint32_t
#define MAKE_COMBO_KEY(motion_mode, combo_type)		(	(uint32_t(motion_mode) << 16) | (uint32_t(combo_type))	)
#define COMBO_KEY_GET_MOTION_MODE(key)				(	WORD(uint32_t(key) >> 16 & 0xFFFF)						)
#define COMBO_KEY_GET_COMBO_TYPE(key)				(	WORD(uint32_t(key) & 0xFFFF)							)

class CRaceData
{
	public:
		enum EParts
		{
			// Share index with server
			// ECharacterEquipmentPart도 수정해주세요.
			//패킷 크기가 변합니다 서버와 상의후 추가해주세요.
			PART_MAIN,
			PART_WEAPON,
			PART_HEAD,
			PART_WEAPON_LEFT,
			PART_HAIR,
#ifdef ENABLE_ACCE_SYSTEM
			PART_ACCE,
#endif
#ifdef ENABLE_COSTUME_EFFECT
			PART_EFFECT_BODY,
			PART_EFFECT_WEAPON,
#endif
#ifdef ENABLE_RUNE_SYSTEM
			PART_RUNE,
#endif
			PART_MAX_NUM,
		};

		enum
		{
			SMOKE_NUM = 4,
		};

		/////////////////////////////////////////////////////////////////////////////////
		// Graphic Resource

		// Model
		typedef std::map<WORD, CGraphicThing*> TGraphicThingMap;
		typedef std::map<uint32_t, std::string> TAttachingBoneNameMap;

		// Motion
		typedef struct SMotion
		{
			uint8_t byPercentage;
			CGraphicThing * pMotion;
			CRaceMotionData * pMotionData;
		} TMotion;
		typedef std::vector<TMotion> TMotionVector;
		typedef std::map<WORD, TMotionVector> TMotionVectorMap;

		typedef struct SMotionModeData
		{
			WORD wMotionModeIndex;

			TMotionVectorMap MotionVectorMap;

			SMotionModeData() : wMotionModeIndex(0) {}
			virtual ~SMotionModeData() {}
		} TMotionModeData;
		typedef std::map<WORD, TMotionModeData*> TMotionModeDataMap;
		typedef TMotionModeDataMap::iterator TMotionModeDataIterator;

		/////////////////////////////////////////////////////////////////////////////////
		// Model Data
		typedef struct SModelData
		{
			NRaceData::TAttachingDataVector AttachingDataVector;
		} TModelData;
		typedef std::map<uint32_t, TModelData> TModelDataMap;
		typedef TModelDataMap::iterator TModelDataMapIterator;

		/////////////////////////////////////////////////////////////////////////////////
		// Motion Data
		typedef std::map<uint32_t, CRaceMotionData*> TMotionDataMap;

		/////////////////////////////////////////////////////////////////////////////////
		// Combo Data
		typedef std::vector<uint32_t> TComboIndexVector;
		typedef struct SComboAttackData
		{
			TComboIndexVector ComboIndexVector;
		} TComboData;
		typedef std::map<uint32_t, uint32_t> TNormalAttackIndexMap;
		typedef std::map<COMBO_KEY, TComboData> TComboAttackDataMap;
		typedef TComboAttackDataMap::iterator TComboAttackDataIterator;

		struct SSkin
		{
			int m_ePart;

			std::string m_stSrcFileName;
			std::string m_stDstFileName;

			SSkin()
			{
				m_ePart=0;
			}
			SSkin(const SSkin& c_rkSkin)
			{
				Copy(c_rkSkin);
			}
			void operator=(const SSkin& c_rkSkin)
			{
				Copy(c_rkSkin);
			}
			void Copy(const SSkin& c_rkSkin)
			{
				m_ePart=c_rkSkin.m_ePart;
				m_stSrcFileName=c_rkSkin.m_stSrcFileName;
				m_stDstFileName=c_rkSkin.m_stDstFileName;
			}
		};

		struct SHair
		{
			std::string m_stModelFileName;
			std::vector<SSkin> m_kVct_kSkin;
		};

		struct SShape
		{
			std::string m_stModelFileName;
			std::vector<SSkin> m_kVct_kSkin;
		};

	public:
		static CRaceData* New();
		static void Delete(CRaceData* pkRaceData);
		static void CreateSystem(UINT uCapacity, UINT uMotModeCapacity);
		static void DestroySystem();

	public:
		CRaceData();
		virtual ~CRaceData();

		void Destroy();

		// Codes For Client
		const char* GetBaseModelFileName() const;
		const char* GetAttributeFileName() const;
		const char* GetMotionListFileName() const;
		CGraphicThing * GetBaseModelThing();
		CGraphicThing * GetLODModelThing();
		CAttributeData * GetAttributeDataPtr();
		bool GetAttachingBoneName(uint32_t dwPartIndex, const char ** c_pszBoneName);
		bool CreateMotionModeIterator(TMotionModeDataIterator & itor);
		bool NextMotionModeIterator(TMotionModeDataIterator & itor);

		bool GetMotionKey(WORD wMotionModeIndex, WORD wMotionIndex, MOTION_KEY * pMotionKey);
#ifdef ENABLE_CLIENT_OPTIMIZATION
		uint32_t GetRace() const { return m_dwRaceIndex; };
#endif
		bool GetMotionModeDataPointer(WORD wMotionMode, TMotionModeData ** ppMotionModeData);
		bool GetModelDataPointer(uint32_t dwModelIndex, const TModelData ** c_ppModelData);
		bool GetMotionVectorPointer(WORD wMotionMode, WORD wMotionIndex, const TMotionVector ** c_ppMotionVector);
		bool GetMotionDataPointer(WORD wMotionMode, WORD wMotionIndex, WORD wMotionSubIndex, CRaceMotionData** ppMotionData);
		bool GetMotionDataPointer(uint32_t dwMotionKey, CRaceMotionData ** ppMotionData);

		uint32_t GetAttachingDataCount();
		bool GetAttachingDataPointer(uint32_t dwIndex, const NRaceData::TAttachingData ** c_ppAttachingData);
		bool GetCollisionDataPointer(uint32_t dwIndex, const NRaceData::TAttachingData ** c_ppAttachingData);
		bool GetBodyCollisionDataPointer(const NRaceData::TAttachingData ** c_ppAttachingData);

		bool IsTree();
		const char * GetTreeFileName();

		///////////////////////////////////////////////////////////////////
		// Setup by Script
		bool LoadRaceData(const char * c_szFileName);

		CGraphicThing* RegisterMotionData(WORD wMotionMode, WORD wMotionIndex, const char * c_szFileName, uint8_t byPercentage = 100);

		///////////////////////////////////////////////////////////////////
		// Setup by Python
		void SetRace(uint32_t dwRaceIndex);
		void RegisterAttachingBoneName(uint32_t dwPartIndex, const char * c_szBoneName);

		void RegisterMotionMode(WORD wMotionModeIndex);
		void SetMotionModeParent(WORD wParentMotionModeIndex, WORD wMotionModeIndex);
		void OLD_RegisterMotion(WORD wMotionModeIndex, WORD wMotionIndex, const char * c_szFileName, uint8_t byPercentage = 100);
		CGraphicThing* NEW_RegisterMotion(CRaceMotionData* pkMotionData, WORD wMotionModeIndex, WORD wMotionIndex, const char * c_szFileName, uint8_t byPercentage = 100);
		bool SetMotionRandomWeight(WORD wMotionModeIndex, WORD wMotionIndex, WORD wMotionSubIndex, uint8_t byPercentage);

		void RegisterNormalAttack(WORD wMotionModeIndex, WORD wMotionIndex);
		bool GetNormalAttackIndex(WORD wMotionModeIndex, WORD * pwMotionIndex);

		void ReserveComboAttack(WORD wMotionModeIndex, WORD wComboType, uint32_t dwComboCount);
		void RegisterComboAttack(WORD wMotionModeIndex, WORD wComboType, uint32_t dwComboIndex, WORD wMotionIndex);
		bool GetComboDataPointer(WORD wMotionModeIndex, WORD wComboType, TComboData ** ppComboData);

		void SetShapeModel(UINT eShape, const char* c_szModelFileName);
		void AppendShapeSkin(UINT eShape, UINT ePart, const char* c_szSrcFileName, const char* c_szDstFileName);

		void SetHairSkin(UINT eHair, UINT ePart, const char* c_szModelFileName, const char* c_szSrcFileName, const char* c_szDstFileName);

		/////

		uint32_t GetSmokeEffectID(UINT eSmoke);

		const std::string& GetSmokeBone();

		SHair* FindHair(UINT eHair);
		SShape* FindShape(UINT eShape);

	protected:
		void __Initialize();

		void __OLD_RegisterMotion(WORD wMotionMode, WORD wMotionIndex, const TMotion & rMotion);

		bool GetMotionVectorPointer(WORD wMotionMode, WORD wMotionIndex, TMotionVector ** ppMotionVector);

	protected:
		uint32_t m_dwRaceIndex;
		uint32_t m_adwSmokeEffectID[SMOKE_NUM];

		CGraphicThing * m_pBaseModelThing;
		CGraphicThing * m_pLODModelThing;

		std::string m_strBaseModelFileName;
		std::string m_strTreeFileName;
		std::string m_strAttributeFileName;
		std::string m_strMotionListFileName;
		std::string m_strSmokeBoneName;

		TModelDataMap m_ModelDataMap;
		TMotionModeDataMap m_pMotionModeDataMap;
		TAttachingBoneNameMap m_AttachingBoneNameMap;
		TComboAttackDataMap m_ComboAttackDataMap;
		TNormalAttackIndexMap m_NormalAttackIndexMap;

		std::map<uint32_t, SHair> m_kMap_dwHairKey_kHair;
		std::map<uint32_t, SShape> m_kMap_dwShapeKey_kShape;

		NRaceData::TAttachingDataVector m_AttachingDataVector;

	protected:
		static CDynamicPool<TMotionModeData>	ms_MotionModeDataPool;
		static CDynamicPool<CRaceData>			ms_kPool;
};
