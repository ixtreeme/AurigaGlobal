#pragma once

#pragma warning(disable:4786)

#include <deque>
#include "Thing.h"
#include "ModelInstance.h"

class CGrannyLODController final : public CGraphicBase
{
public:
	static void SetMinLODMode(bool isEnable);

public:
	struct FSetLocalTime
	{
		float fLocalTime;

		void operator()(const CGrannyLODController* pController) const
		{
			pController->SetLocalTime(fLocalTime);
		}
	};

	struct FUpdateTime
	{
		float fElapsedTime;

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->UpdateTime(fElapsedTime);
		}
	};

	struct FUpdateLODLevel
	{
		float fDistanceFromCenter;
		float fDistanceFromCamera;

		void operator()(CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->UpdateLODLevel(fDistanceFromCenter, fDistanceFromCamera);
		}
	};

	struct FRenderWithOneTexture
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->RenderWithOneTexture();
		}
	};

	struct FBlendRenderWithOneTexture
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->BlendRenderWithOneTexture();
		}
	};

	struct FRenderWithTwoTexture
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->RenderWithTwoTexture();
		}
	};

	struct FBlendRenderWithTwoTexture
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->BlendRenderWithTwoTexture();
		}
	};

	struct FRenderToShadowMap
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->RenderToShadowMap();
		}
	};

	struct FRenderShadow
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->RenderShadow();
		}
	};

	struct FDeform
	{
		const D3DXMATRIX* mc_pWorldMatrix;

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->Deform(mc_pWorldMatrix);
		}
	};

	struct FDeformNoSkin
	{
		const D3DXMATRIX* mc_pWorldMatrix;

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->DeformNoSkin(mc_pWorldMatrix);
		}
	};

	struct FDeformAll
	{
		const D3DXMATRIX* mc_pWorldMatrix;

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->DeformAll(mc_pWorldMatrix);
		}
	};

	struct FCreateDeviceObjects
	{
		void operator()(CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->CreateDeviceObjects();
		}
	};

	struct FDestroyDeviceObjects
	{
		void operator()(CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->DestroyDeviceObjects();
		}
	};

	struct FBoundBox
	{
		D3DXVECTOR3* m_vtMin;
		D3DXVECTOR3* m_vtMax;

		FBoundBox(D3DXVECTOR3* vtMin, D3DXVECTOR3* vtMax)
		{
			m_vtMin = vtMin;
			m_vtMax = vtMax;
		}

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->GetBoundBox(m_vtMin, m_vtMax);
		}
	};

	struct FResetLocalTime
	{
		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->ResetLocalTime();
		}
	};

	struct FReloadTexture
	{
		void operator ()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->ReloadTexture();
		}
	};

	struct FSetMotionPointer
	{
		std::shared_ptr<CGrannyMotion> m_pMotion;
		float m_speedRatio;
		float m_blendTime;
		int m_loopCount;

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->SetMotionPointer(m_pMotion, m_blendTime, m_loopCount, m_speedRatio);
		}
	};

	struct FChangeMotionPointer
	{
		std::shared_ptr<CGrannyMotion> m_pMotion;
		float m_speedRatio;
		int m_loopCount;

		void operator()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->ChangeMotionPointer(m_pMotion, m_loopCount, m_speedRatio);
		}
	};

	struct FEndStopMotionPointer
	{
		std::shared_ptr<CGrannyMotion> m_pMotion;

		void operator ()(const CGrannyLODController* pController) const
		{
			if (pController->isModelInstance())
				pController->SetMotionAtEnd();
		}
	};

	CGrannyLODController();
	~CGrannyLODController() override;

	void Clear();

	void CreateDeviceObjects();
	void DestroyDeviceObjects();

	void AddModel(CGraphicThing* pThing, int iSrcModel, CGrannyLODController* pSkelLODController = nullptr);
	void AttachModelInstance(CGrannyLODController* pSrcLODController, const char* c_szBoneName);
	void DetachModelInstance(CGrannyLODController* pSrcLODController);
	void SetLODLimits(float fNearLOD, float fFarLOD);
	void SetLODLevel(uint64_t bLODLevel);
	uint64_t GetLODLevel() const { return m_bLODLevel; }
	void SetMaterialImagePointer(const char* c_szImageName, CGraphicImage* pImage) const;
	void SetMaterialData(const char* c_szImageName, const SMaterialData& c_rkMaterialData) const;
	void SetSpecularInfo(const char* c_szMtrlName, BOOL bEnable, float fPower) const;

	void RenderWithOneTexture() const;
	void RenderWithTwoTexture() const;
	void BlendRenderWithOneTexture() const;
	void BlendRenderWithTwoTexture() const;

	void Update(float fElapsedTime, float fDistanceFromCenter, float fDistanceFromCamera);
	void UpdateLODLevel(float fDistanceFromCenter, float fDistanceFromCamera);
	void UpdateTime(float fElapsedTime) const;

	void UpdateSkeleton(const D3DXMATRIX* c_pWorldMatrix, float fElapsedTime) const;
	void Deform(const D3DXMATRIX* c_pWorldMatrix) const;
	void DeformNoSkin(const D3DXMATRIX* c_pWorldMatrix) const;
	void DeformAll(const D3DXMATRIX* c_pWorldMatrix) const;

	void RenderToShadowMap() const;
	void RenderShadow() const;
	void ReloadTexture() const;

	void GetBoundBox(D3DXVECTOR3* vtMin, D3DXVECTOR3* vtMax) const;
	bool Intersect(const D3DXMATRIX* c_pMatrix, float* u, float* v, float* t) const;

	void SetLocalTime(float fLocalTime) const;
	void ResetLocalTime() const;

	void SetMotionPointer(const std::shared_ptr<CGrannyMotion> c_pMotion, float fBlendTime, int iLoopCount, float speedRatio) const;
	void ChangeMotionPointer(const std::shared_ptr<CGrannyMotion> c_pMotion, int iLoopCount, float speedRatio) const;
	void SetMotionAtEnd() const;

	BOOL isModelInstance() const;
	CGrannyModelInstance* GetModelInstance();
	bool HaveBlendThing() { return nullptr != GetModelInstance() ? GetModelInstance()->HaveBlendThing() : false; }

protected:
	void SetCurrentModelInstance(CGrannyModelInstance* pgrnModelInstance);
	void RefreshAttachedModelInstance();

	void __ReserveSharedDeformableVertexBuffer(uint32_t deformableVertexCount);

protected:
	float m_fLODDistance;
	uint32_t m_dwLODAniFPS;

	using TAttachingModelData = struct SAttachingModelData
	{
		CGrannyLODController* pkLODController;
		std::string strBoneName;
	};

	std::vector<TAttachingModelData> m_AttachedModelDataVector;
	CGrannyLODController* m_pAttachedParentModel;

	uint8_t m_bLODLevel;
	CGrannyModelInstance* m_pCurrentModelInstance;
	std::deque<CGrannyModelInstance*> m_que_pkModelInst;

	CGraphicVertexBuffer* m_pkSharedDeformableVertexBuffer;
};
