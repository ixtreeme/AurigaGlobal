// ===========================================================================
// [CLIENT] MarkManager.cpp
// Guild Mark Manager - Client Side
// Handles: block sync reception, local CRC comparison, symbol cache
// ===========================================================================
#include "stdafx.h"
#include "MarkManager.h"

#include <direct.h>
#include <cstdio>

#define sys_err  TraceError
#define sys_log  // (n, format, ...) Tracenf(format, __VA_ARGS__)



// ---------------------------------------------------------------------------
// [CLIENT] Construction / Destruction
// ---------------------------------------------------------------------------
CGuildMarkManager::CGuildMarkManager()
{
	_mkdir("mark");

	const uint32_t totalMarks = MAX_IMAGE_COUNT * CGuildMarkImage::MARK_TOTAL_COUNT;
	for (uint32_t i = 0; i < totalMarks; ++i)
		m_setFreeMarkID.insert(i);
}

CGuildMarkManager::~CGuildMarkManager()
{
	m_mapIdx_Image.clear();
}

// ---------------------------------------------------------------------------
// [CLIENT] Path management
// ---------------------------------------------------------------------------
bool CGuildMarkManager::GetMarkImageFilename(uint32_t imgIdx, std::string& path) const
{
	if (imgIdx >= MAX_IMAGE_COUNT)
		return false;

	char buf[64];
	_snprintf(buf, sizeof(buf), "mark/%s_%u.tga", m_pathPrefix.c_str(), imgIdx);
	path = buf;
	return true;
}

void CGuildMarkManager::SetMarkPathPrefix(const char* prefix)
{
	m_pathPrefix = prefix;
}

// ---------------------------------------------------------------------------
// [CLIENT] __GetImage - load local image on demand
// ---------------------------------------------------------------------------
CGuildMarkImage* CGuildMarkManager::__GetImage(uint32_t imgIdx)
{
	auto it = m_mapIdx_Image.find(imgIdx);
	if (it != m_mapIdx_Image.end())
		return it->second.get();

	std::string imagePath;
	if (!GetMarkImageFilename(imgIdx, imagePath))
		return nullptr;

	auto pkImage = std::make_unique<CGuildMarkImage>();
	pkImage->Load(imagePath.c_str());

	CGuildMarkImage* rawPtr = pkImage.get();
	m_mapIdx_Image.emplace(imgIdx, std::move(pkImage));
	return rawPtr;
}

// ---------------------------------------------------------------------------
// [CLIENT] LoadMarkImages - load all mark images that have assigned marks
// ---------------------------------------------------------------------------
void CGuildMarkManager::LoadMarkImages()
{
	bool isMarkExists[MAX_IMAGE_COUNT]{};

	for (const auto& [guildID, markID] : m_mapGID_MarkID)
	{
		if (markID < MAX_IMAGE_COUNT * CGuildMarkImage::MARK_TOTAL_COUNT)
			isMarkExists[markID / CGuildMarkImage::MARK_TOTAL_COUNT] = true;
	}

	for (uint32_t i = 0; i < MAX_IMAGE_COUNT; ++i)
	{
		if (isMarkExists[i])
			__GetImage(i);
	}
}

// ---------------------------------------------------------------------------
// [CLIENT] SaveMarkImage - save a specific mark image to disk
// ---------------------------------------------------------------------------
void CGuildMarkManager::SaveMarkImage(uint32_t imgIdx)
{
	std::string path;
	if (GetMarkImageFilename(imgIdx, path))
	{
		CGuildMarkImage* pkImage = __GetImage(imgIdx);
		if (pkImage && !pkImage->Save(path.c_str()))
			sys_err("CGuildMarkManager::SaveMarkImage: %s save failed", path.c_str());
	}
}

// ---------------------------------------------------------------------------
// [CLIENT] Mark ID management - populated from server data
// ---------------------------------------------------------------------------
bool CGuildMarkManager::AddMarkIDByGuildID(uint32_t guildID, uint32_t markID)
{
	if (markID >= MAX_IMAGE_COUNT * CGuildMarkImage::MARK_TOTAL_COUNT)
		return false;

	m_mapGID_MarkID.emplace(guildID, markID);
	m_setFreeMarkID.erase(markID);
	return true;
}

uint32_t CGuildMarkManager::GetMarkID(uint32_t guildID) const
{
	auto it = m_mapGID_MarkID.find(guildID);
	if (it == m_mapGID_MarkID.end())
		return INVALID_MARK_ID;
	return it->second;
}

uint32_t CGuildMarkManager::GetMarkImageCount() const
{
	return static_cast<uint32_t>(m_mapIdx_Image.size());
}

uint32_t CGuildMarkManager::GetMarkCount() const
{
	return static_cast<uint32_t>(m_mapGID_MarkID.size());
}

// ---------------------------------------------------------------------------
// [CLIENT] SaveBlockFromCompressedData - receive LZ4 block from server
// ---------------------------------------------------------------------------
bool CGuildMarkManager::SaveBlockFromCompressedData(uint32_t imgIdx, uint32_t posBlock,
	const uint8_t* pbBlock, uint32_t dwSize)
{
	CGuildMarkImage* pkImage = __GetImage(imgIdx);
	if (pkImage)
		return pkImage->SaveBlockFromCompressedData(posBlock, pbBlock, dwSize);
	return false;
}

// ---------------------------------------------------------------------------
// [CLIENT] GetBlockCRCList - build CRC list to send to server for comparison
// ---------------------------------------------------------------------------
bool CGuildMarkManager::GetBlockCRCList(uint32_t imgIdx, uint32_t* crcList)
{
	if (m_mapIdx_Image.find(imgIdx) == m_mapIdx_Image.end())
	{
		sys_log(0, "CGuildMarkManager::GetBlockCRCList: invalid idx %u", imgIdx);
		return false;
	}

	CGuildMarkImage* p = __GetImage(imgIdx);
	if (p)
		p->GetBlockCRCList(crcList);

	return true;
}

// ---------------------------------------------------------------------------
// [CLIENT] Symbol cache - client stores symbols locally
// ---------------------------------------------------------------------------
const CGuildMarkManager::TGuildSymbol* CGuildMarkManager::GetGuildSymbol(uint32_t guildID) const
{
	auto it = m_mapSymbol.find(guildID);
	if (it == m_mapSymbol.end())
		return nullptr;
	return &it->second;
}

bool CGuildMarkManager::LoadSymbol(const char* filename)
{
	FILE* fp = fopen(filename, "rb");
	if (!fp)
		return true;

	uint32_t symbolCount = 0;
	if (fread(&symbolCount, sizeof(symbolCount), 1, fp) != 1)
	{
		sys_err("CGuildMarkManager::LoadSymbol: failed to read count from %s", filename);
		fclose(fp);
		return false;
	}

	if (symbolCount > 100000)
	{
		sys_err("CGuildMarkManager::LoadSymbol: suspicious count %u in %s", symbolCount, filename);
		fclose(fp);
		return false;
	}

	for (uint32_t i = 0; i < symbolCount; ++i)
	{
		uint32_t guildID = 0;
		uint32_t dwSize = 0;

		if (fread(&guildID, sizeof(guildID), 1, fp) != 1 ||
			fread(&dwSize, sizeof(dwSize), 1, fp) != 1)
		{
			sys_err("CGuildMarkManager::LoadSymbol: truncated at entry %u/%u", i, symbolCount);
			break;
		}

		if (dwSize > MAX_SYMBOL_SIZE)
		{
			sys_err("CGuildMarkManager::LoadSymbol: symbol %u too large (%u), skipping", guildID, dwSize);
			fseek(fp, dwSize, SEEK_CUR);
			continue;
		}

		TGuildSymbol gs;
		gs.raw.resize(dwSize);

		if (fread(gs.raw.data(), 1, dwSize, fp) != dwSize)
		{
			sys_err("CGuildMarkManager::LoadSymbol: incomplete read for guild %u", guildID);
			break;
		}

		gs.crc = GetCRC32(reinterpret_cast<const char*>(gs.raw.data()), dwSize);
		m_mapSymbol.emplace(guildID, std::move(gs));
	}

	fclose(fp);
	return true;
}

void CGuildMarkManager::SaveSymbol(const char* filename)
{
	FILE* fp = fopen(filename, "wb");
	if (!fp)
	{
		sys_err("CGuildMarkManager::SaveSymbol: cannot open %s", filename);
		return;
	}

	uint32_t symbolCount = static_cast<uint32_t>(m_mapSymbol.size());
	fwrite(&symbolCount, sizeof(symbolCount), 1, fp);

	for (const auto& [guildID, symbol] : m_mapSymbol)
	{
		uint32_t id = guildID;
		uint32_t dwSize = static_cast<uint32_t>(symbol.raw.size());
		fwrite(&id, sizeof(id), 1, fp);
		fwrite(&dwSize, sizeof(dwSize), 1, fp);

		if (!symbol.raw.empty())
			fwrite(symbol.raw.data(), 1, dwSize, fp);
	}

	fclose(fp);
}

// ---------------------------------------------------------------------------
// [CLIENT] UploadSymbol - prepare symbol data to send to server
// ---------------------------------------------------------------------------
void CGuildMarkManager::UploadSymbol(uint32_t guildID, int iSize, const uint8_t* pbyData)
{
	if (iSize < 0)
	{
		sys_err("CGuildMarkManager::UploadSymbol: negative size %d for guild %u", iSize, guildID);
		return;
	}

	if (static_cast<uint32_t>(iSize) > MAX_SYMBOL_SIZE)
	{
		sys_err("CGuildMarkManager::UploadSymbol: size %d exceeds limit for guild %u", iSize, guildID);
		return;
	}

	sys_log(0, "GuildSymbolUpload guild=%u size=%d", guildID, iSize);

	auto [it, inserted] = m_mapSymbol.try_emplace(guildID, TGuildSymbol{});
	TGuildSymbol& rSymbol = it->second;
	rSymbol.raw.clear();

	if (iSize > 0 && pbyData)
	{
		rSymbol.raw.assign(pbyData, pbyData + iSize);
		rSymbol.crc = GetCRC32(reinterpret_cast<const char*>(pbyData), iSize);
	}
}