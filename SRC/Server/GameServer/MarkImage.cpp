// ===========================================================================
// [SERVER] MarkImage.cpp
// Guild Mark Image System - Server Side
// Compression: LZ4
// ===========================================================================
#include "stdafx.h"
#include "MarkImage.h"

#include <lz4.h>
#include "crc32.h"

// ---------------------------------------------------------------------------
// [SERVER] CGuildMarkImage - Construction / Destruction
// ---------------------------------------------------------------------------
CGuildMarkImage::CGuildMarkImage()
	: m_aakBlock{}, m_uImg(INVALID_HANDLE)
{
}

CGuildMarkImage::~CGuildMarkImage()
{
	Destroy();
}

CGuildMarkImage::CGuildMarkImage(CGuildMarkImage&& other) noexcept
	: m_uImg(other.m_uImg)
{
	memcpy(m_aakBlock, other.m_aakBlock, sizeof(m_aakBlock));
	other.m_uImg = INVALID_HANDLE;
}

CGuildMarkImage& CGuildMarkImage::operator=(CGuildMarkImage&& other) noexcept
{
	if (this != &other)
	{
		Destroy();
		m_uImg = other.m_uImg;
		memcpy(m_aakBlock, other.m_aakBlock, sizeof(m_aakBlock));
		other.m_uImg = INVALID_HANDLE;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// [SERVER] DevIL image lifecycle
// ---------------------------------------------------------------------------
void CGuildMarkImage::Destroy()
{
	if (INVALID_HANDLE == m_uImg)
		return;

	ilDeleteImages(1, &m_uImg);
	m_uImg = INVALID_HANDLE;
}

void CGuildMarkImage::Create()
{
	if (INVALID_HANDLE != m_uImg)
		return;

	ilGenImages(1, &m_uImg);
}

bool CGuildMarkImage::Save(const char* c_szFileName)
{
	if (INVALID_HANDLE == m_uImg)
	{
		sys_err("GuildMarkImage::Save: no image loaded");
		return false;
	}

	ilEnable(IL_FILE_OVERWRITE);
	ilBindImage(m_uImg);

	if (!ilSave(IL_TGA, (const ILstring)c_szFileName))
	{
		sys_err("GuildMarkImage::Save: ilSave failed for %s", c_szFileName);
		return false;
	}

	return true;
}

bool CGuildMarkImage::Build(const char* c_szFileName)
{
	sys_log(0, "GuildMarkImage: creating new file %s", c_szFileName);

	Destroy();
	Create();

	ilBindImage(m_uImg);
	ilEnable(IL_ORIGIN_SET);
	ilOriginFunc(IL_ORIGIN_UPPER_LEFT);

	std::vector<uint8_t> data(sizeof(Pixel) * WIDTH * HEIGHT, 0);

	if (!ilTexImage(WIDTH, HEIGHT, 1, 4, IL_BGRA, IL_UNSIGNED_BYTE, data.data()))
	{
		sys_err("GuildMarkImage::Build: cannot initialize image");
		return false;
	}

	ilEnable(IL_FILE_OVERWRITE);

	if (!ilSave(IL_TGA, (const ILstring)c_szFileName))
	{
		sys_err("GuildMarkImage::Build: cannot save %s", c_szFileName);
		return false;
	}

	return true;
}

bool CGuildMarkImage::Load(const char* c_szFileName)
{
	Destroy();
	Create();

	ilBindImage(m_uImg);
	ilEnable(IL_ORIGIN_SET);
	ilOriginFunc(IL_ORIGIN_UPPER_LEFT);

	if (!ilLoad(IL_TYPE_UNKNOWN, (const ILstring)c_szFileName))
	{
		sys_err("GuildMarkImage::Load: %s cannot open file.", c_szFileName);
		return false;
	}

	if (ilGetInteger(IL_IMAGE_WIDTH) != WIDTH)
	{
		sys_err("GuildMarkImage::Load: %s width must be %u (got %d)", c_szFileName, WIDTH, ilGetInteger(IL_IMAGE_WIDTH));
		return false;
	}

	if (ilGetInteger(IL_IMAGE_HEIGHT) != HEIGHT)
	{
		sys_err("GuildMarkImage::Load: %s height must be %u (got %d)", c_szFileName, HEIGHT, ilGetInteger(IL_IMAGE_HEIGHT));
		return false;
	}

	ilConvertImage(IL_BGRA, IL_UNSIGNED_BYTE);

	BuildAllBlocks();
	return true;
}

// ---------------------------------------------------------------------------
// [SERVER] Pixel I/O
// ---------------------------------------------------------------------------
void CGuildMarkImage::PutData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data)
{
	ilBindImage(m_uImg);
	ilSetPixels(x, y, 0, width, height, 1, IL_BGRA, IL_UNSIGNED_BYTE, data);
}

void CGuildMarkImage::GetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data)
{
	ilBindImage(m_uImg);
	ilCopyPixels(x, y, 0, width, height, 1, IL_BGRA, IL_UNSIGNED_BYTE, data);
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
// [SERVER] BuildAllBlocks - compress all blocks from the loaded image
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
// [SERVER] GetDiffBlocks - find blocks where server CRC differs from client
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
// [SERVER] GetBlockCRCList - export all block CRCs for sync comparison
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
