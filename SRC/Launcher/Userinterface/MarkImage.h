// ===========================================================================
// [CLIENT] MarkImage.h
// Guild Mark Image System - Client Side
// Compression: LZ4
// ===========================================================================
#ifndef __INC_METIN_II_CLIENT_MARKIMAGE_H__
#define __INC_METIN_II_CLIENT_MARKIMAGE_H__

#include <cstdint>
#include <cstring>
#include <map>

using Pixel = uint32_t;

struct SGuildMark
{
	enum
	{
		WIDTH = 16,
		HEIGHT = 12,
		SIZE = WIDTH * HEIGHT,
	};

	Pixel m_apxBuf[SIZE]{};

	void Clear();
	bool IsEmpty() const;
};

struct SGuildMarkBlock
{
	enum
	{
		MARK_PER_BLOCK_WIDTH = 4,
		MARK_PER_BLOCK_HEIGHT = 4,

		WIDTH = SGuildMark::WIDTH * MARK_PER_BLOCK_WIDTH,
		HEIGHT = SGuildMark::HEIGHT * MARK_PER_BLOCK_HEIGHT,

		SIZE = WIDTH * HEIGHT,

		// LZ4 worst-case bound: LZ4_COMPRESSBOUND(SIZE * sizeof(Pixel))
		// LZ4_COMPRESSBOUND(n) = n + n/255 + 16
		MAX_COMP_SIZE = (SIZE * sizeof(Pixel)) + ((SIZE * sizeof(Pixel)) / 255) + 16,
	};

	Pixel		m_apxBuf[SIZE]{};
	uint8_t		m_abCompBuf[MAX_COMP_SIZE]{};
	uint32_t	m_sizeCompBuf = 0;
	uint32_t	m_crc = 0;

	uint32_t	GetCRC() const;
	void		CopyFrom(const uint8_t* pbCompBuf, uint32_t dwCompSize, uint32_t crc);
	void		Compress(const Pixel* pxBuf);
};

class CGuildMarkImage
{
public:
	enum
	{
		WIDTH = 512,
		HEIGHT = 512,

		BLOCK_ROW_COUNT = HEIGHT / SGuildMarkBlock::HEIGHT,		// 10
		BLOCK_COL_COUNT = WIDTH / SGuildMarkBlock::WIDTH,		// 8

		BLOCK_TOTAL_COUNT = BLOCK_ROW_COUNT * BLOCK_COL_COUNT,	// 80

		MARK_ROW_COUNT = BLOCK_ROW_COUNT * SGuildMarkBlock::MARK_PER_BLOCK_HEIGHT,	// 40
		MARK_COL_COUNT = BLOCK_COL_COUNT * SGuildMarkBlock::MARK_PER_BLOCK_WIDTH,	// 32

		MARK_TOTAL_COUNT = MARK_ROW_COUNT * MARK_COL_COUNT,		// 1280

		INVALID_MARK_POSITION = 0xffffffff,
	};

	CGuildMarkImage();
	~CGuildMarkImage();

	// Non-copyable, movable
	CGuildMarkImage(const CGuildMarkImage&) = delete;
	CGuildMarkImage& operator=(const CGuildMarkImage&) = delete;
	CGuildMarkImage(CGuildMarkImage&& other) noexcept;
	CGuildMarkImage& operator=(CGuildMarkImage&& other) noexcept;

	// [CLIENT] Image lifecycle
	void Create();
	void Destroy();
	bool Build(const char* c_szFileName);
	bool Save(const char* c_szFileName);
	bool Load(const char* c_szFileName);

	// [CLIENT] Pixel I/O
	void PutData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);
	void GetData(uint32_t x, uint32_t y, uint32_t width, uint32_t height, void* data);

	// [CLIENT] Mark operations (local)
	bool SaveMark(uint32_t posMark, uint8_t* pbMarkImage);
	bool DeleteMark(uint32_t posMark);

	// [CLIENT] Block sync - receive compressed blocks from server
	bool SaveBlockFromCompressedData(uint32_t posBlock, const uint8_t* pbComp, uint32_t dwCompSize);

	// [CLIENT] CRC comparison for sync
	uint32_t GetEmptyPosition();
	void GetBlockCRCList(uint32_t* crcList) const;
	void GetDiffBlocks(const uint32_t* crcList, std::map<uint8_t, const SGuildMarkBlock*>& mapDiffBlocks) const;

private:
	

	void BuildAllBlocks();

	SGuildMarkBlock m_aakBlock[BLOCK_ROW_COUNT][BLOCK_COL_COUNT]{};
	Pixel m_apxImage[WIDTH * HEIGHT];
	
};

#endif
