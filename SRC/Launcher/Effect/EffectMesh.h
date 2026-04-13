#pragma once

#include <directxsdk/d3dx9.h>

#include "../Render/GrpScreen.h"
#include "../Render/Resource.h"
#include "../Render/GrpImageInstance.h"
#include "../Render/TextFileLoader.h"
#include "../Render/GrpBase.h"

#include "Type.h"
#include "EffectElementBase.h"

#define D3DFVF_TPTVERTEX (D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE)

class CEffectMesh : public CResource
{
	public:
		using TEffectFrameData = struct SEffectFrameData
		{
			uint8_t byChangedFrame = 0;
			float fVisibility = 0.0f;
			uint32_t dwVertexCount = 0;
			uint32_t dwTextureVertexCount = 0;
			uint32_t dwIndexCount = 0;

			LPDIRECT3DVERTEXBUFFER9 pVB = nullptr;
			LPDIRECT3DINDEXBUFFER9 pIB = nullptr;


			std::vector<uint16_t> IndexVector = {};

			std::vector<TPTVertex> PDTVertexVector;

			SEffectFrameData() = default;
		};

		using TEffectMeshData = struct SEffectMeshData
		{
			char szObjectName[32] = {};
			char szDiffuseMapFileName[128] = {};

			std::vector<TEffectFrameData> EffectFrameDataVector = {};
			std::vector<CGraphicImage*> pImageVector = {};

			static SEffectMeshData* New();
			static void Delete(SEffectMeshData* pkData);

			static void DestroySystem();

			static CDynamicPool<SEffectMeshData> ms_kPool;

			SEffectMeshData()
			{
				memset(szObjectName, 0, sizeof(szObjectName));
				memset(szDiffuseMapFileName, 0, sizeof(szDiffuseMapFileName));
			}
		};

	// About Resource Code
	public:
		typedef CRef<CEffectMesh> TRef;

	public:
		static TType Type();

	public:
		explicit CEffectMesh(const char * c_szFileName);
		~CEffectMesh() override;

		uint32_t GetFrameCount() const;
		uint32_t GetMeshCount() const;
		TEffectMeshData * GetMeshDataPointer(uint32_t dwMeshIndex) const;

		std::vector<CGraphicImage*>* GetTextureVectorPointer(uint32_t dwMeshIndex) const;
		std::vector<CGraphicImage*>& GetTextureVectorReference(uint32_t dwMeshIndex) const;

		// Exceptional function for tool
		bool GetMeshElementPointer(uint32_t dwMeshIndex, TEffectMeshData ** ppMeshData) const;

	protected:
		bool OnLoad(int iSize, const void * c_pvBuf);

		void OnClear();
		bool OnIsEmpty() const;
		bool OnIsType(TType type);

		bool __LoadData_Ver001(int iSize, const uint8_t* c_pbBuf);
		bool __LoadData_Ver002(int iSize, const uint8_t* c_pbBuf);

	protected:
		int								m_iGeomCount;
		int								m_iFrameCount;
		std::vector<TEffectMeshData *>	m_pEffectMeshDataVector;

		bool							m_isData;
};

class CEffectMeshScript : public CEffectElementBase
{
	public:
		typedef struct SMeshData
		{
			uint8_t byBillboardType;

			bool bBlendingEnable;
			uint8_t byBlendingSrcType;
			uint8_t byBlendingDestType;
			bool bTextureAlphaEnable;

			uint8_t byColorOperationType;
			D3DXCOLOR ColorFactor;

			bool bTextureAnimationLoopEnable;
			float fTextureAnimationFrameDelay;

			uint32_t dwTextureAnimationStartFrame;

			TTimeEventTableFloat TimeEventAlpha;

			SMeshData()
			{
				byBillboardType = 0;

				bBlendingEnable = false;
				byBlendingSrcType = 0;
				byBlendingDestType = 0;
				bTextureAlphaEnable = false;

				byColorOperationType = 0;
				ColorFactor = {};

				bTextureAnimationLoopEnable = false;
				fTextureAnimationFrameDelay = 0;

				dwTextureAnimationStartFrame = 0;
				TimeEventAlpha.clear();
			}
		} TMeshData;
		typedef std::vector<TMeshData> TMeshDataVector;

	public:
		CEffectMeshScript();
		virtual ~CEffectMeshScript();

		const char * GetMeshFileName() const;

		void ReserveMeshData(uint32_t dwMeshCount);
		bool CheckMeshIndex(uint32_t dwMeshIndex) const;
		bool GetMeshDataPointer(uint32_t dwMeshIndex, TMeshData ** ppMeshData);
		int GetMeshDataCount() const;

		int GetBillboardType(uint32_t dwMeshIndex) const;
		bool isBlendingEnable(uint32_t dwMeshIndex) const;
		uint8_t GetBlendingSrcType(uint32_t dwMeshIndex) const;
		uint8_t GetBlendingDestType(uint32_t dwMeshIndex) const;
		bool isTextureAlphaEnable(uint32_t dwMeshIndex) const;
		bool GetColorOperationType(uint32_t dwMeshIndex, uint8_t* pbyType) const;
		bool GetColorFactor(uint32_t dwMeshIndex, D3DXCOLOR * pColor) const;
		bool GetTimeTableAlphaPointer(uint32_t dwMeshIndex, TTimeEventTableFloat ** pTimeEventAlpha);

		bool isMeshAnimationLoop() const;
		int GetMeshAnimationLoopCount() const;
		float GetMeshAnimationFrameDelay() const;
		bool isTextureAnimationLoop(uint32_t dwMeshIndex) const;
		float GetTextureAnimationFrameDelay(uint32_t dwMeshIndex) const;
		uint32_t GetTextureAnimationStartFrame(uint32_t dwMeshIndex) const;

	protected:
		void OnClear();
		bool OnIsData();
		bool OnLoadScript(CTextFileLoader & rTextFileLoader);

	protected:
		bool m_isMeshAnimationLoop;
		int m_iMeshAnimationLoopCount;
		float m_fMeshAnimationFrameDelay;
		TMeshDataVector m_MeshDataVector;

		std::string m_strMeshFileName;

	public:
		static void DestroySystem();

		static CEffectMeshScript* New();
		static void Delete(CEffectMeshScript* pkData);

		static CDynamicPool<CEffectMeshScript> ms_kPool;
};