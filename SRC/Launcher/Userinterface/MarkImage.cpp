// ===========================================================================
// [CLIENT] MarkImage.cpp
// Guild Mark Image System - Client Side
// Compression: LZ4
// ===========================================================================
#include "stdafx.h"
#include "MarkImage.h"

#include <lz4.h>
#include <vector>

#define sys_err  TraceError
#define sys_log  // (n, format, ...) Tracenf(format, __VA_ARGS__)


// ---------------------------------------------------------------------------
// [CLIENT] CGuildMarkImage - Construction / Destruction
// ---------------------------------------------------------------------------
CGuildMarkImage::CGuildMarkImage()
	: m_aakBlock{}, m_apxImage{}
{
}

CGuildMarkImage::~CGuildMarkImage()
{
	Destroy();
}


// ---------------------------------------------------------------------------
// [CLIENT] DevIL image lifecycle
// ---------------------------------------------------------------------------
void CGuildMarkImage::Destroy()
{
	memset(m_apxImage, 0, sizeof(m_apxImage));
}

void CGuildMarkImage::Create()
{
	memset(m_apxImage, 0, sizeof(m_apxImage));
}

namespace
{
	bool SaveGuildImageBufferToFile(const char* c_szFileName, const Pixel* pPixels)
	{
		LPDIRECT3DDEVICE9 pkDevice = CGraphicBase::GetD3DDevice();
		if (!pkDevice)
			return false;

		IDirect3DTexture9* pkTexture = nullptr;
		if (FAILED(pkDevice->CreateTexture(CGuildMarkImage::WIDTH, CGuildMarkImage::HEIGHT, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pkTexture, nullptr)))
			return false;

		D3DLOCKED_RECT kLockedRect{};
		if (FAILED(pkTexture->LockRect(0, &kLockedRect, nullptr, 0)))
		{
			pkTexture->Release();
			return false;
		}

		const uint32_t dwRowBytes = CGuildMarkImage::WIDTH * sizeof(Pixel);
		const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(pPixels);
		uint8_t* pDst = reinterpret_cast<uint8_t*>(kLockedRect.pBits);

		for (uint32_t y = 0; y < CGuildMarkImage::HEIGHT; ++y)
		{
			memcpy(pDst + (y * kLockedRect.Pitch), pSrc + (y * dwRowBytes), dwRowBytes);
		}

		pkTexture->UnlockRect(0);
		const bool bSaved = SUCCEEDED(D3DXSaveTextureToFileA(c_szFileName, D3DXIFF_TGA, pkTexture, nullptr));
		pkTexture->Release();
		return bSaved;

	}
}

bool LoadGuildImageBufferFromFile(const char* c_szFileName, Pixel* pPixels)
{
	LPDIRECT3DDEVICE9 pkDevice = CGraphicBase::GetD3DDevice();
	if (!pkDevice)
		return false;

	D3DXIMAGE_INFO kImageInfo{};
	if (FAILED(D3DXGetImageInfoFromFileA(c_szFileName, &kImageInfo)))
		return false;

	IDirect3DTexture9* pkTexture = nullptr;
	if (FAILED(D3DXCreateTextureFromFileExA(
		pkDevice,
		c_szFileName,
		kImageInfo.Width,
		kImageInfo.Height,
		1,
		0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_SYSTEMMEM,
		D3DX_FILTER_NONE,
		D3DX_FILTER_NONE,
		0,
		&kImageInfo,
		nullptr,
		&pkTexture)))
	{
		return true;
	}

	D3DLOCKED_RECT kLockedRect{};
	if (FAILED(pkTexture->LockRect(0, &kLockedRect, nullptr, D3DLOCK_READONLY)))
	{
		pkTexture->Release();
		return false;
	}

	const uint32_t dwRowBytes = CGuildMarkImage::WIDTH * sizeof(Pixel);
	const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(kLockedRect.pBits);
	uint8_t* pDst = reinterpret_cast<uint8_t*>(pPixels);

	for (uint32_t y = 0; y < CGuildMarkImage::HEIGHT; ++y)
	{
		memcpy(pDst + (y * dwRowBytes), pSrc + (y * kLockedRect.Pitch), dwRowBytes);
	}

	pkTexture->UnlockRect(0);
	pkTexture->Release();
	return true;
}

bool CGuildMarkImage::Save(const char* c_szFileName)
{
	return SaveGuildImageBufferToFile(c_szFileName, m_apxImage);
}

bool CGuildMarkImage::Build(const char* c_szFileName)
{
	Destroy();
	Create();

	if (!Save(c_szFileName))
	{
		sys_err("CGuildMarkImage: cannot initialize image");
		return false;
	}

	return true;
}


bool CGuildMarkImage::Load(const char* c_szFileName)
{
	Destroy();
	Create();


	if (!LoadGuildImageBufferFromFile(c_szFileName, m_apxImage))
	{
		if (!Build(c_szFileName) || !LoadGuildImageBufferFromFile(c_szFileName, m_apxImage))
		{
			sys_err("CGuildMarkImage::Load: cannot open %s", c_szFileName);
			return false;
		}
	}

	BuildAllBlocks();
	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] Pixel I/O
// ---------------------------------------------------------------------------
void CGuildMarkImage::PutData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data)
{
	if (x + width > WIDTH || y + height > HEIGHT)
		return;

	const uint32_t dwRowBytes = width * sizeof(Pixel);
	const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(data);

	for (uint32_t row = 0; row < height; ++row)
	{
		Pixel* pDstRow = m_apxImage + ((y + row) * WIDTH) + x;
		memcpy(pDstRow, pSrc + (row * dwRowBytes), dwRowBytes);
	}
}

void CGuildMarkImage::GetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data)
{
	if (x + width > WIDTH || y + height > HEIGHT)
		return;

	const uint32_t dwRowBytes = width * sizeof(Pixel);
	uint8_t* pDst = reinterpret_cast<uint8_t*>(data);

	for (uint32_t row = 0; row < height; ++row)
	{
		const Pixel* pSrcRow = m_apxImage + ((y + row) * WIDTH) + x;
		memcpy(pDst + (row * dwRowBytes), pSrcRow, dwRowBytes);
	}
}

// ---------------------------------------------------------------------------
// [CLIENT] SaveMark - write a single mark into the local image
// ---------------------------------------------------------------------------
bool CGuildMarkImage::SaveMark(uint32_t posMark, uint8_t* pbImage)
{
	if (posMark >= MARK_TOTAL_COUNT)
	{
		sys_err("CGuildMarkImage::SaveMark: invalid position %u (max %u)", posMark, MARK_TOTAL_COUNT);
		return false;
	}

	const uint32_t colMark = posMark % MARK_COL_COUNT;
	const uint32_t rowMark = posMark / MARK_COL_COUNT;

	PutData(colMark * SGuildMark::WIDTH, rowMark * SGuildMark::HEIGHT,
	        SGuildMark::WIDTH, SGuildMark::HEIGHT, pbImage);

	const uint32_t rowBlock = rowMark / SGuildMarkBlock::MARK_PER_BLOCK_HEIGHT;
	const uint32_t colBlock = colMark / SGuildMarkBlock::MARK_PER_BLOCK_WIDTH;

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	GetData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT,
	        SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
	m_aakBlock[rowBlock][colBlock].Compress(apxBuf);

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] DeleteMark - clear a mark locally
// ---------------------------------------------------------------------------
bool CGuildMarkImage::DeleteMark(uint32_t posMark)
{
	Pixel image[SGuildMark::SIZE]{};
	return SaveMark(posMark, reinterpret_cast<uint8_t*>(image));
}

// ---------------------------------------------------------------------------
// [CLIENT] SaveBlockFromCompressedData - receive LZ4 block from server
// ---------------------------------------------------------------------------
bool CGuildMarkImage::SaveBlockFromCompressedData(uint32_t posBlock, const uint8_t* pbComp, uint32_t dwCompSize)
{
	if (posBlock >= BLOCK_TOTAL_COUNT)
	{
		sys_err("CGuildMarkImage::SaveBlockFromCompressedData: invalid block %u", posBlock);
		return false;
	}

	Pixel apxBuf[SGuildMarkBlock::SIZE];
	const int expectedSize = static_cast<int>(sizeof(apxBuf));

	const int decompressedSize = LZ4_decompress_safe(
		reinterpret_cast<const char*>(pbComp),
		reinterpret_cast<char*>(apxBuf),
		static_cast<int>(dwCompSize),
		expectedSize);

	if (decompressedSize < 0)
	{
		sys_err("CGuildMarkImage::SaveBlockFromCompressedData: LZ4 decompression failed (compSize=%u)", dwCompSize);
		return false;
	}

	if (decompressedSize != expectedSize)
	{
		sys_err("CGuildMarkImage::SaveBlockFromCompressedData: corrupt data (got %d, expected %d)",
		        decompressedSize, expectedSize);
		return false;
	}

	const uint32_t rowBlock = posBlock / BLOCK_COL_COUNT;
	const uint32_t colBlock = posBlock % BLOCK_COL_COUNT;

	PutData(colBlock * SGuildMarkBlock::WIDTH, rowBlock * SGuildMarkBlock::HEIGHT,
	        SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);

	m_aakBlock[rowBlock][colBlock].CopyFrom(
		pbComp, dwCompSize,
		GetCRC32(reinterpret_cast<const char*>(apxBuf), sizeof(Pixel) * SGuildMarkBlock::SIZE));

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] BuildAllBlocks - compress all blocks from the loaded image
// ---------------------------------------------------------------------------
void CGuildMarkImage::BuildAllBlocks()
{
	Pixel apxBuf[SGuildMarkBlock::SIZE];

	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
	{
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMarkBlock::WIDTH, row * SGuildMarkBlock::HEIGHT,
			        SGuildMarkBlock::WIDTH, SGuildMarkBlock::HEIGHT, apxBuf);
			m_aakBlock[row][col].Compress(apxBuf);
		}
	}
}

// ---------------------------------------------------------------------------
// [CLIENT] GetEmptyPosition - find first unused mark slot locally
// ---------------------------------------------------------------------------
uint32_t CGuildMarkImage::GetEmptyPosition()
{
	SGuildMark kMark;

	for (uint32_t row = 0; row < MARK_ROW_COUNT; ++row)
	{
		for (uint32_t col = 0; col < MARK_COL_COUNT; ++col)
		{
			GetData(col * SGuildMark::WIDTH, row * SGuildMark::HEIGHT,
			        SGuildMark::WIDTH, SGuildMark::HEIGHT, kMark.m_apxBuf);

			if (kMark.IsEmpty())
				return (row * MARK_COL_COUNT + col);
		}
	}

	return INVALID_MARK_POSITION;
}

// ---------------------------------------------------------------------------
// [CLIENT] GetDiffBlocks - compare local CRCs with server's list
// ---------------------------------------------------------------------------
void CGuildMarkImage::GetDiffBlocks(const uint32_t* crcList,
                                     std::map<uint8_t, const SGuildMarkBlock*>& mapDiffBlocks) const
{
	uint8_t posBlock = 0;

	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
	{
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
		{
			if (m_aakBlock[row][col].m_crc != *crcList)
				mapDiffBlocks.emplace(posBlock, &m_aakBlock[row][col]);

			++crcList;
			++posBlock;
		}
	}
}

// ---------------------------------------------------------------------------
// [CLIENT] GetBlockCRCList - export CRCs to send to server for comparison
// ---------------------------------------------------------------------------
void CGuildMarkImage::GetBlockCRCList(uint32_t* crcList) const
{
	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
			*(crcList++) = m_aakBlock[row][col].GetCRC();
}

// ---------------------------------------------------------------------------
// [CLIENT] SGuildMark
// ---------------------------------------------------------------------------
void SGuildMark::Clear()
{
	for (uint32_t i = 0; i < SIZE; ++i)
		m_apxBuf[i] = 0xff000000;
}

bool SGuildMark::IsEmpty() const
{
	for (uint32_t i = 0; i < SIZE; ++i)
	{
		if (m_apxBuf[i] != 0x00000000)
			return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] SGuildMarkBlock - LZ4 compression
// ---------------------------------------------------------------------------
uint32_t SGuildMarkBlock::GetCRC() const
{
	return m_crc;
}

void SGuildMarkBlock::CopyFrom(const uint8_t* pbCompBuf, uint32_t dwCompSize, uint32_t crc)
{
	if (dwCompSize > MAX_COMP_SIZE)
	{
		sys_err("SGuildMarkBlock::CopyFrom: data too large (%u > %u)", dwCompSize, MAX_COMP_SIZE);
		return;
	}

	m_sizeCompBuf = dwCompSize;
	std::memcpy(m_abCompBuf, pbCompBuf, dwCompSize);
	m_crc = crc;
}

void SGuildMarkBlock::Compress(const Pixel* pxBuf)
{
	const int srcSize = static_cast<int>(sizeof(Pixel) * SIZE);
	const int maxDstSize = static_cast<int>(MAX_COMP_SIZE);

	const int compressedSize = LZ4_compress_default(
		reinterpret_cast<const char*>(pxBuf),
		reinterpret_cast<char*>(m_abCompBuf),
		srcSize,
		maxDstSize);

	if (compressedSize <= 0)
	{
		sys_err("SGuildMarkBlock::Compress: LZ4 compression failed");
		m_sizeCompBuf = 0;
		m_crc = 0;
		return;
	}

	m_sizeCompBuf = static_cast<uint32_t>(compressedSize);
	m_crc = GetCRC32(reinterpret_cast<const char*>(pxBuf), srcSize);
}
