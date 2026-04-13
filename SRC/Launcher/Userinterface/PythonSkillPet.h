#pragma once

#include "../Game/ItemData.h"
#include "../Base/Poly/Poly.h"

class CInstanceBase;

class CPythonSkillPet : public CSingleton<CPythonSkillPet>
{
	public:
		enum
		{
			SKILLPET_TYPE_NONE,
			SKILLPET_TYPE_PASSIVE,
			SKILLPET_TYPE_AUTO,			
			SKILLPET_TYPE_MAX_NUM,
		};

		enum ESkillPetDescTokenType
		{
			DESCPET_TOKEN_TYPE_VNUM,			
			DESCPET_TOKEN_TYPE_NAME,
			DESCPET_TOKEN_TYPE_ICON_NAME,
			DESCPET_TOKEN_TYPE,
			DESCPET_TOKEN_TYPE_DESCRIPTION,
			DESCPET_TOKEN_TYPE_DELAY,
			DESCPET_TOKEN_TYPE_MAX_NUM,

			CONDITIONPET_COLUMN_COUNT = 3,
			AFFECTPET_COLUMN_COUNT = 3,
			AFFECTPET_STEP_COUNT = 3,
		};
		
#ifdef ENABLE_NEW_PET_EDITS
		int petslot[4];
#else
		int petslot[3];
#endif
		
		typedef struct SSkillDataPet
		{
			// Functions
			SSkillDataPet();
			///////////////////////////////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////////////////////////////

			// Variable
			uint32_t dwSkillIndex;
			std::string strName;
			std::string strIconFileName;
			uint8_t byType;
			std::string strDescription;
			uint32_t dwskilldelay;			

			CGraphicImage * pImage;

			/////
			
			
		} TSkillDataPet;

		typedef std::map<uint32_t, TSkillDataPet> TSkillDataPetMap;
		

	public:
		CPythonSkillPet();
		virtual ~CPythonSkillPet();

		void SetSkillbySlot(int slot, int skillIndex);


		bool GetSkillIndex(int slot, int * skillIndex);
		bool GetSkillData(uint32_t dwSkillIndex, TSkillDataPet ** ppSkillData);
		void Destroy();			
		bool RegisterSkillPet(const char * c_szFileName);
		
	protected:
		//void __RegisterGradeIconImage(TSkillDataPet & rData, const char * c_szHeader, const char * c_szImageName);
		void __RegisterNormalIconImage(TSkillDataPet & rData, const char * c_szHeader, const char * c_szImageName);

	protected:
		TSkillDataPetMap m_SkillDataPetMap;
		std::map<std::string, uint32_t> m_SkillPetTypeIndexMap;
		
};