#include "StdAfx.h"

#include <cassert>
#include "Image.h"

CImage::CImage(const CImage & image)
{
	Initialize();

	int w = image.GetWidth();
	int h = image.GetHeight();

	Create(w, h);

	uint32_t * pdwDest = GetBasePointer();
	uint32_t * pdwSrc = image.GetBasePointer();

	memcpy(pdwDest, pdwSrc, w * h * sizeof(uint32_t));
}

void CImage::SetFileName(const char* c_szFileName)
{
	m_stFileName = c_szFileName;
}

const std::string& CImage::GetFileNameString()
{
	return m_stFileName;
}

void CImage::PutImage(int x, int y, const CImage* pImage) const
{
	assert(x >= 0 && x + pImage->GetWidth() <= GetWidth());
	assert(y >= 0 && y + pImage->GetHeight() <= GetHeight());

	const int len = pImage->GetWidth() * sizeof(uint32_t);

	for (int j = 0; j < pImage->GetHeight(); ++j)
	{
		uint32_t * pdwDest = GetLinePointer(y + j) + x;
		memcpy(pdwDest, pImage->GetLinePointer(j), len);
	}
}

uint32_t* CImage::GetBasePointer() const
{
	assert(m_pdwColors != NULL);
	return m_pdwColors;
}

uint32_t* CImage::GetLinePointer(int line) const
{
	assert(m_pdwColors != NULL);
	return m_pdwColors + line * m_width;
}

int CImage::GetWidth() const
{
	assert(m_pdwColors != NULL);
	return m_width;
}

int CImage::GetHeight() const
{
	assert(m_pdwColors != NULL);
	return m_height;
}

void CImage::Clear(uint32_t color) const
{
	assert(m_pdwColors != NULL);

	for (int y = 0; y < m_height; ++y)
	{
		uint32_t * colorLine = &m_pdwColors[y * m_width];

		for (int x = 0; x < m_width; ++x)
			colorLine[x] = color;
	}
}

void CImage::Create(int width, int height)
{
	Destroy();

	m_width = width;
	m_height = height;
	m_pdwColors = new uint32_t[m_width*m_height];
}

void CImage::Destroy()
{
	if (m_pdwColors)
	{
		delete [] m_pdwColors;
		m_pdwColors = nullptr;
	}
}

void CImage::Initialize()
{
	m_pdwColors = nullptr;
	m_width = 0;
	m_height = 0;
}

bool CImage::IsEmpty() const
{
	return m_pdwColors == nullptr;
}

void CImage::FlipTopToBottom() const
{
	const auto swap = new uint32_t[m_width * m_height];

	UINT width = GetWidth();
	UINT height = GetHeight();

	for (int row = 0; row < GetHeight() / 2; row++)
	{
		uint32_t* end_row = &(m_pdwColors[width * (height - row - 1)]);
		uint32_t* start_row = &(m_pdwColors[width * row]);

		memcpy(swap, end_row, width * sizeof(uint32_t));
		memcpy(end_row, start_row, width * sizeof(uint32_t));
		memcpy(start_row, swap, width * sizeof(uint32_t));
	}

	delete [] swap;
}

CImage::CImage()
{
	Initialize();
}

CImage::~CImage()
{
	Destroy();
}
