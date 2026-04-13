// ===========================================================================
// [SERVER] MarkImage.cpp
// Guild Mark Image System - Server Side
// Compression: LZ4 | Image I/O: stb_image / stb_image_write (no DevIL)
// ===========================================================================
#include "stdafx.h"
#include "MarkImage.h"

#include <lz4.h>
#include "crc32.h"

// stb_image - header-only, single TU definition
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_TGA
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_WRITE_IMPLEMENTATION

#ifdef _WIN64
#include <stb_image.h>
#include <stb_image_write.h>
#else
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#endif
// ---------------------------------------------------------------------------
// Internal helpers: RGBA <-> BGRA swap
// stb_image loads as RGBA; the guild mark system uses BGRA internally
// ---------------------------------------------------------------------------
static void RGBAtoBGRA(uint8_t* data, int pixelCount)
{
	for (int i = 0; i < pixelCount; ++i)
	{
		uint8_t* p = data + i * 4;
		std::swap(p[0], p[2]); // R <-> B
	}
}

// stb_image_write TGA callback — writes into a std::vector<uint8_t>
static void StbWriteCallback(void* context, void* data, int size)
{
	auto* vec = static_cast<std::vector<uint8_t>*>(context);
	const uint8_t* bytes = static_cast<const uint8_t*>(data);
	vec->insert(vec->end(), bytes, bytes + size);
}

// ---------------------------------------------------------------------------
// [SERVER] CGuildMarkImage - Construction
// ---------------------------------------------------------------------------
CGuildMarkImage::CGuildMarkImage()
	: m_buffer(WIDTH* HEIGHT, 0u)
{
}

// ---------------------------------------------------------------------------
// [SERVER] Build - create a blank image and save to disk
// ---------------------------------------------------------------------------
bool CGuildMarkImage::Build(const char* c_szFileName)
{
	sys_log(0, "GuildMarkImage: creating new file %s", c_szFileName);

	// Reset buffer to transparent black
	std::fill(m_buffer.begin(), m_buffer.end(), 0u);

	return Save(c_szFileName);
}

// ---------------------------------------------------------------------------
// [SERVER] Save - write current buffer as TGA
// stb_image_write expects RGBA; we swap BGRA -> RGBA for the write, then swap back
// ---------------------------------------------------------------------------
bool CGuildMarkImage::Save(const char* c_szFileName)
{
	// Work on a copy so we don't permanently alter the in-memory BGRA buffer
	std::vector<uint8_t> rgba(WIDTH * HEIGHT * 4);
	memcpy(rgba.data(), m_buffer.data(), rgba.size());
	RGBAtoBGRA(rgba.data(), WIDTH * HEIGHT); // BGRA -> RGBA for stb

	if (!stbi_write_tga(c_szFileName, WIDTH, HEIGHT, 4, rgba.data()))
	{
		sys_err("GuildMarkImage::Save: stbi_write_tga failed for %s", c_szFileName);
		return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// [SERVER] Load - read TGA/PNG/BMP from disk into internal BGRA buffer
// ---------------------------------------------------------------------------
bool CGuildMarkImage::Load(const char* c_szFileName)
{
	int w = 0, h = 0, channels = 0;

	// Force 4 channels (RGBA)
	uint8_t* raw = stbi_load(c_szFileName, &w, &h, &channels, 4);
	if (!raw)
	{
		sys_err("GuildMarkImage::Load: %s cannot open file (%s)", c_szFileName, stbi_failure_reason());
		return false;
	}

	if (w != WIDTH)
	{
		sys_err("GuildMarkImage::Load: %s width must be %u (got %d)", c_szFileName, WIDTH, w);
		stbi_image_free(raw);
		return false;
	}

	if (h != HEIGHT)
	{
		sys_err("GuildMarkImage::Load: %s height must be %u (got %d)", c_szFileName, HEIGHT, h);
		stbi_image_free(raw);
		return false;
	}

	// Convert RGBA -> BGRA in-place before copying into our buffer
	RGBAtoBGRA(raw, WIDTH * HEIGHT);
	memcpy(m_buffer.data(), raw, WIDTH * HEIGHT * 4);
	stbi_image_free(raw);

	BuildAllBlocks();
	return true;
}

// ---------------------------------------------------------------------------
// [SERVER] Pixel I/O - direct sub-rect read/write on the flat buffer
// ---------------------------------------------------------------------------
void CGuildMarkImage::PutData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data)
{
	const Pixel* src = static_cast<const Pixel*>(data);
	for (uint32_t row = 0; row < height; ++row)
	{
		Pixel* dst = m_buffer.data() + (y + row) * WIDTH + x;
		memcpy(dst, src + row * width, width * sizeof(Pixel));
	}
}

void CGuildMarkImage::GetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data) const
{
	Pixel* dst = static_cast<Pixel*>(data);
	for (uint32_t row = 0; row < height; ++row)
	{
		const Pixel* src = m_buffer.data() + (y + row) * WIDTH + x;
		memcpy(dst + row * width, src, width * sizeof(Pixel));
	}
}

// ---------------------------------------------------------------------------
// [SERVER] SaveMark - write a single guild mark into the image
// ---------------------------------------------------------------------------
bool CGuildMarkImage::SaveMark(uint32_t posMark, uint8_t* pbImage)
{
	if (posMark >= MARK_TOTAL_COUNT)
	{
		sys_err("GuildMarkImage::SaveMark: invalid mark position %u (max %u)", posMark, MARK_TOTAL_COUNT);
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

	sys_log(0, "GuildMarkImage::SaveMark: pos=%u block[%u,%u] compSize=%u",
		posMark, rowBlock, colBlock, m_aakBlock[rowBlock][colBlock].m_sizeCompBuf);

	return true;
}

// ---------------------------------------------------------------------------
// [SERVER] DeleteMark - clear a guild mark (fill with transparent black)
// ---------------------------------------------------------------------------
bool CGuildMarkImage::DeleteMark(uint32_t posMark)
{
	Pixel image[SGuildMark::SIZE]{};
	return SaveMark(posMark, reinterpret_cast<uint8_t*>(image));
}

// ---------------------------------------------------------------------------
// [SERVER] BuildAllBlocks - compress all blocks from the loaded buffer
// ---------------------------------------------------------------------------
void CGuildMarkImage::BuildAllBlocks()
{
	Pixel apxBuf[SGuildMarkBlock::SIZE];
	sys_log(0, "GuildMarkImage::BuildAllBlocks");

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
// [SERVER] GetEmptyPosition - find first unused mark slot
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
// [SERVER] GetDiffBlocks
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
// [SERVER] GetBlockCRCList
// ---------------------------------------------------------------------------
void CGuildMarkImage::GetBlockCRCList(uint32_t* crcList) const
{
	for (uint32_t row = 0; row < BLOCK_ROW_COUNT; ++row)
		for (uint32_t col = 0; col < BLOCK_COL_COUNT; ++col)
			*(crcList++) = m_aakBlock[row][col].GetCRC();
}

// ---------------------------------------------------------------------------
// [SERVER] SGuildMark
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
// [SERVER] SGuildMarkBlock - LZ4 compression
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
	memcpy(m_abCompBuf, pbCompBuf, dwCompSize);
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