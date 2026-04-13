#pragma once

#include "EffectElementBase.h"

class CEffectElementBaseInstance
{
	public:
		CEffectElementBaseInstance();
		virtual ~CEffectElementBaseInstance();

		void SetDataPointer(CEffectElementBase * pElement);

		void Initialize();
		void Destroy();

		void SetLocalMatrixPointer(const D3DXMATRIX * c_pMatrix);
		void SetTimeScale(float fTimeScale);
		bool Update(float fElapsedTime);
		void Render();

		[[nodiscard]] bool isActive();
		void SetActive();
		void SetDeactive();

	public:
		void SetWikiIgnoreFrustum(bool flag) { m_wikiIgnoreFrustum = flag; }
	
	protected:
		bool m_wikiIgnoreFrustum;

	protected:
		virtual void OnSetDataPointer(CEffectElementBase * pElement) = 0;

		virtual void OnInitialize() = 0;
		virtual void OnDestroy() = 0;

		virtual bool OnUpdate(float fElapsedTime) = 0;
		virtual void OnRender() = 0;

	protected:
		const D3DXMATRIX *						mc_pmatLocal;

		bool									m_isActive;

		float									m_fLocalTime;
		uint32_t									m_dwStartTime;
		float									m_fElapsedTime;
		float									m_fRemainingTime;

		float									m_fTimeScale;
		bool									m_bStart;

	private:
		CEffectElementBase *					m_pBase;
};