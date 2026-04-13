#pragma once

#include "Resource.h"
#include "Ref.h"
#include "GrpFontTexture.h"

class CGraphicText : public CResource
{
	public:
		typedef CRef<CGraphicText> TRef;

	public:
		static TType Type();

	public:
		CGraphicText(const char* c_szFileName);
		~CGraphicText() override;

		bool			CreateDeviceObjects() override;
		void			DestroyDeviceObjects() override;

		CGraphicFontTexture *	GetFontTexturePointer();

	protected:
		bool		OnLoad(int iSize, const void * c_pvBuf) override;
		void		OnClear() override;
		bool		OnIsEmpty() const override;
		bool		OnIsType(TType type) override;

	protected:
		CGraphicFontTexture m_fontTexture;
};
