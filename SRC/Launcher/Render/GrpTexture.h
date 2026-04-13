#pragma once

#include "GrpBase.h"
#include <d3d9.h>
class CGraphicTexture : public CGraphicBase
{
	public:
#ifdef LEADERBOARD_RAZOR93
		
		void AttachExternalTexture(LPDIRECT3DTEXTURE9 tex);

#endif
		virtual bool IsEmpty() const;

		int GetWidth() const;
		int GetHeight() const;

		void SetTextureStage(int stage) const;
		LPDIRECT3DTEXTURE9 GetD3DTexture() const;

		void DestroyDeviceObjects();

	protected:
		CGraphicTexture();
		virtual	~CGraphicTexture();

		virtual void Destroy();
		virtual void Initialize();

	protected:
		bool m_bEmpty;

		int m_width;
		int m_height;

		LPDIRECT3DTEXTURE9 m_lpd3dTexture;
};
