#pragma once

#include "GrpImage.h"
#include "GrpIndexBuffer.h"
#include "GrpVertexBufferDynamic.h"
#include "Pool.h"
#include "../UserInterface/Locale_inc.h"

class CGraphicImageInstance
{
	public:
		static uint32_t Type();
		bool IsType(uint32_t dwType);

	public:
		CGraphicImageInstance();
		virtual ~CGraphicImageInstance();

		virtual void Destroy();

		void Render();

		void SetDiffuseColor(float fr, float fg, float fb, float fa);
		void SetPosition(float fx, float fy);

		void SetImagePointer(CGraphicImage* pImage);
		void ReloadImagePointer(CGraphicImage* pImage);
		bool IsEmpty() const;

		int GetWidth();
		int GetHeight();

		CGraphicTexture * GetTexturePointer();
		const CGraphicTexture &	GetTextureReference() const;
		CGraphicImage * GetGraphicImagePointer();

		bool operator == (const CGraphicImageInstance & rhs) const;

	protected:
		virtual void Initialize();

		virtual void OnRender();
		virtual void OnSetImagePointer();

		virtual bool OnIsType(uint32_t dwType);

	protected:
		D3DXCOLOR m_DiffuseColor;
		D3DXVECTOR2 m_v2Position;
#ifdef NEW_PET_SYSTEM
		float m_vScale;
		float m_vScaleY;
#endif
		CGraphicImage::TRef m_roImage;

	public:
		static void CreateSystem(UINT uCapacity);
		static void DestroySystem();

		static CGraphicImageInstance* New();
		static void Delete(CGraphicImageInstance* pkImgInst);

		static CDynamicPool<CGraphicImageInstance>		ms_kPool;

#ifdef ENABLE_UI_EXTRA
	public:
		void SetScale(float fx, float fy);
		void SetScale(D3DXVECTOR2 v2Scale);
		const D3DXVECTOR2 & GetScale() const;
		void SetScalePercent(uint8_t byPercent);
		void SetScalePivotCenter(bool bScalePivotCenter);

	protected:
		D3DXVECTOR2 m_v2Scale;
		bool m_bScalePivotCenter;
#endif
};
