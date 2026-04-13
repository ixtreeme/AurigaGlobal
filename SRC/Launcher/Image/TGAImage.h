#ifndef __INC_ETERIMAGELIB_TGAIMAGE_H__
#define __INC_ETERIMAGELIB_TGAIMAGE_H__

#include "Image.h"

class CTGAImage : public CImage
{
	public:
		enum ETGAImageFlags
		{
			FLAG_RLE_COMPRESS = (1 << 0)
		};

		CTGAImage();
		CTGAImage(CImage &image);
		virtual ~CTGAImage();

		virtual void	Create(int width, int height);
		virtual bool	LoadFromMemory(int iSize, const uint8_t* c_pbMem);
		virtual bool	LoadFromDiskFile(const char * c_szFileName);
		virtual bool	SaveToDiskFile(const char* c_szFileName);

		void			SetCompressed(bool isCompress = true);
		void			SetAlphaChannel(bool isExist = true);

		TGA_HEADER &	GetHeader();
	protected:
		int			GetRawPixelCount(const uint32_t * data);
		int			GetRLEPixelCount(const uint32_t * data);

	protected:
		TGA_HEADER	m_Header;
		uint32_t		m_dwFlag;
		uint32_t *		m_pdwEndPtr;
};

#endif
