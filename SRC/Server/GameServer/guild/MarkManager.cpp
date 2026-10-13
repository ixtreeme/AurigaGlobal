// ===========================================================================
// [SERVER] MarkManager.cpp
// Guild Mark Manager - Server Side
// Handles: mark allocation, index persistence, symbol storage, block sync
// ===========================================================================
#include "stdafx.h"
#include <Core/Logging.hpp>
#include "MarkManager.h"

#include "crc32.h"

#include <fmt/format.h>
#include <fstream>
#include <sstream>

#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#endif

// ---------------------------------------------------------------------------
// [SERVER] Construction / Destruction
// ---------------------------------------------------------------------------
CGuildMarkManager::CGuildMarkManager()
{
#ifdef _WIN32
	_mkdir("mark");
#else
	mkdir("mark", S_IRWXU);
#endif

	const uint32_t totalMarks = MAX_IMAGE_COUNT * CGuildMarkImage::MARK_TOTAL_COUNT;
	for (uint32_t i = 0; i < totalMarks; ++i)
		m_setFreeMarkID.insert(i);
}

CGuildMarkManager::~CGuildMarkManager()
{
	m_mapIdx_Image.clear();
}

// ---------------------------------------------------------------------------
// [SERVER] Path management
// ---------------------------------------------------------------------------
bool CGuildMarkManager::GetMarkImageFilename(uint32_t imgIdx, std::string& path) const
{
	if (imgIdx >= MAX_IMAGE_COUNT)
		return false;

	char buf[64];
	snprintf(buf, sizeof(buf), "mark/%s_%u.tga", m_pathPrefix.c_str(), imgIdx);
	path = buf;
	return true;
}

void CGuildMarkManager::SetMarkPathPrefix(const char* prefix)
{
	m_pathPrefix = prefix;
}

// ---------------------------------------------------------------------------
// [SERVER] Index persistence - server owns the mark index file
// ---------------------------------------------------------------------------
bool CGuildMarkManager::LoadMarkIndex()
{
	const std::string path = fmt::format("mark/{}_index", m_pathPrefix);

	std::ifstream file(path);

	if (!file)
		return false;

	std::string line;

	while (std::getline(file, line))
	{
		std::istringstream ins(line);

		uint32_t guildID = 0, markID = 0;

		if (ins >> guildID >> markID)
			AddMarkIDByGuildID(guildID, markID);
	}

	LoadMarkImages();
	return true;
}

bool CGuildMarkManager::SaveMarkIndex()
{
	const std::string path = fmt::format("mark/{}_index", m_pathPrefix);

	std::ofstream file(path);

	if (!file)
	{
		LOG_ERROR("MarkManager::SaveMarkIndex: cannot open index file {}", path);
		return false;
	}

	for (const auto& [guildID, markID] : m_mapGID_MarkID)
		file << fmt::format("{} {}\n", guildID, markID);

	LOG_INFO("MarkManager::SaveMarkIndex: {} entries saved", m_mapGID_MarkID.size());
	return true;
}

// ---------------------------------------------------------------------------
// [SERVER] Image loading
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

void CGuildMarkManager::SaveMarkImage(uint32_t imgIdx)
{
	std::string path;
	if (GetMarkImageFilename(imgIdx, path))
	{
		CGuildMarkImage* pkImage = __GetImage(imgIdx);
		if (pkImage && !pkImage->Save(path.c_str()))
			LOG_ERROR("MarkManager::SaveMarkImage: {} save failed", path.c_str());
	}
}

// ---------------------------------------------------------------------------
// [SERVER] __GetImage - load or create image on demand
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

	if (!pkImage->Load(imagePath.c_str()))
	{
		if (!pkImage->Build(imagePath.c_str()) || !pkImage->Load(imagePath.c_str()))
		{
			LOG_ERROR("MarkManager::__GetImage: failed to create/load image {} ({})", imgIdx, imagePath.c_str());
			return nullptr;
		}
	}

	CGuildMarkImage* rawPtr = pkImage.get();
	m_mapIdx_Image.emplace(imgIdx, std::move(pkImage));
	return rawPtr;
}

// ---------------------------------------------------------------------------
// [SERVER] Mark ID management
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

// ---------------------------------------------------------------------------
// [SERVER] __AllocMarkID - allocate a new mark slot for a guild
// ---------------------------------------------------------------------------
uint32_t CGuildMarkManager::__AllocMarkID(uint32_t guildID)
{
	auto it = m_setFreeMarkID.begin();
	if (it == m_setFreeMarkID.end())
		return INVALID_MARK_ID;

	const uint32_t markID = *it;
	const uint32_t imgIdx = markID / CGuildMarkImage::MARK_TOTAL_COUNT;

	CGuildMarkImage* pkImage = __GetImage(imgIdx);
	if (pkImage && AddMarkIDByGuildID(guildID, markID))
		return markID;

	return INVALID_MARK_ID;
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
// [SERVER] CopyMarkIdx - serialize mark index to network buffer
// ---------------------------------------------------------------------------
void CGuildMarkManager::CopyMarkIdx(char* pcBuf, size_t bufSize) const
{
	const size_t requiredSize = m_mapGID_MarkID.size() * sizeof(uint16_t) * 2;

	if (bufSize < requiredSize)
	{
		LOG_ERROR("MarkManager::CopyMarkIdx: buffer too small ({} < {})", bufSize, requiredSize);
		return;
	}

	uint16_t* pwBuf = reinterpret_cast<uint16_t*>(pcBuf);

	for (const auto& [guildID, markID] : m_mapGID_MarkID)
	{
		if (guildID > 0xFFFF || markID > 0xFFFF)
		{
			LOG_ERROR("MarkManager::CopyMarkIdx: value overflow (guildID={}, markID={})", guildID, markID);
			continue;
		}

		*(pwBuf++) = static_cast<uint16_t>(guildID);
		*(pwBuf++) = static_cast<uint16_t>(markID);
	}
}

// ---------------------------------------------------------------------------
// [SERVER] SaveMark - save a guild's mark image data
// ---------------------------------------------------------------------------
uint32_t CGuildMarkManager::SaveMark(uint32_t guildID, uint8_t* pbMarkImage)
{
	uint32_t idMark = GetMarkID(guildID);

	if (idMark == INVALID_MARK_ID)
	{
		idMark = __AllocMarkID(guildID);
		if (idMark == INVALID_MARK_ID)
		{
			LOG_ERROR("MarkManager::SaveMark: cannot alloc mark for guild {}", guildID);
			return INVALID_MARK_ID;
		}
		LOG_INFO("MarkManager::SaveMark: allocated mark {} for guild {}", idMark, guildID);
	}
	else
	{
		LOG_INFO("MarkManager::SaveMark: found existing mark {} for guild {}", idMark, guildID);
	}

	const uint32_t imgIdx = idMark / CGuildMarkImage::MARK_TOTAL_COUNT;
	CGuildMarkImage* pkImage = __GetImage(imgIdx);

	if (pkImage)
	{
		pkImage->SaveMark(idMark % CGuildMarkImage::MARK_TOTAL_COUNT, pbMarkImage);
		SaveMarkImage(imgIdx);
		SaveMarkIndex();
	}

	return idMark;
}

// ---------------------------------------------------------------------------
// [SERVER] DeleteMark - remove a guild's mark
// ---------------------------------------------------------------------------
void CGuildMarkManager::DeleteMark(uint32_t guildID)
{
	auto it = m_mapGID_MarkID.find(guildID);
	if (it == m_mapGID_MarkID.end())
		return;

	const uint32_t markID = it->second;

	CGuildMarkImage* pkImage = __GetImage(markID / CGuildMarkImage::MARK_TOTAL_COUNT);
	if (pkImage)
		pkImage->DeleteMark(markID % CGuildMarkImage::MARK_TOTAL_COUNT);

	m_mapGID_MarkID.erase(it);
	m_setFreeMarkID.insert(markID);

	SaveMarkIndex();
}

// ---------------------------------------------------------------------------
// [SERVER] GetDiffBlocks - find blocks that differ from client's CRC list
// ---------------------------------------------------------------------------
void CGuildMarkManager::GetDiffBlocks(uint32_t imgIdx, const uint32_t* crcList,
                                       std::map<uint8_t, const SGuildMarkBlock*>& mapDiffBlocks)
{
	mapDiffBlocks.clear();

	if (m_mapIdx_Image.find(imgIdx) == m_mapIdx_Image.end())
	{
		LOG_ERROR("MarkManager::GetDiffBlocks: invalid image index {}", imgIdx);
		return;
	}

	CGuildMarkImage* p = __GetImage(imgIdx);
	if (p)
		p->GetDiffBlocks(crcList, mapDiffBlocks);
}

// ---------------------------------------------------------------------------
// [SERVER] GetBlockCRCList - get CRC list for an image (used in sync protocol)
// ---------------------------------------------------------------------------
bool CGuildMarkManager::GetBlockCRCList(uint32_t imgIdx, uint32_t* crcList)
{
	if (m_mapIdx_Image.find(imgIdx) == m_mapIdx_Image.end())
	{
		LOG_ERROR("MarkManager::GetBlockCRCList: invalid image index {}", imgIdx);
		return false;
	}

	CGuildMarkImage* p = __GetImage(imgIdx);
	if (p)
		p->GetBlockCRCList(crcList);

	return true;
}

// ---------------------------------------------------------------------------
// [SERVER] Symbol management - server stores guild symbols
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
	std::ifstream file(filename, std::ios::binary);

	if (!file)
		return true;

	uint32_t symbolCount = 0;

	if (!file.read(reinterpret_cast<char*>(&symbolCount), sizeof(symbolCount)))
	{
		LOG_ERROR("MarkManager::LoadSymbol: failed to read symbol count from {}", filename);
		return false;
	}

	if (symbolCount > 100000)
	{
		LOG_ERROR("MarkManager::LoadSymbol: suspicious symbol count {} in {}", symbolCount, filename);
		return false;
	}

	for (uint32_t i = 0; i < symbolCount; ++i)
	{
		uint32_t guildID = 0;
		uint32_t dwSize = 0;

		if (!file.read(reinterpret_cast<char*>(&guildID), sizeof(guildID)) ||
		    !file.read(reinterpret_cast<char*>(&dwSize), sizeof(dwSize)))
		{
			LOG_ERROR("MarkManager::LoadSymbol: truncated file at entry {}/{}", i, symbolCount);
			break;
		}

		if (dwSize > MAX_SYMBOL_SIZE)
		{
			LOG_ERROR("MarkManager::LoadSymbol: symbol {} too large ({} > {}), skipping", guildID, dwSize, MAX_SYMBOL_SIZE);
			file.seekg(dwSize, std::ios::cur);
			continue;
		}

		TGuildSymbol gs;
		gs.raw.resize(dwSize);

		if (!file.read(reinterpret_cast<char*>(gs.raw.data()), dwSize))
		{
			LOG_ERROR("MarkManager::LoadSymbol: incomplete read for guild {}", guildID);
			break;
		}

		gs.crc = GetCRC32(reinterpret_cast<const char*>(gs.raw.data()), dwSize);
		m_mapSymbol.emplace(guildID, std::move(gs));
	}

	return true;
}

void CGuildMarkManager::SaveSymbol(const char* filename)
{
	std::ofstream file(filename, std::ios::binary);

	if (!file)
	{
		LOG_ERROR("MarkManager::SaveSymbol: cannot open {} for writing", filename);
		return;
	}

	const uint32_t symbolCount = static_cast<uint32_t>(m_mapSymbol.size());
	file.write(reinterpret_cast<const char*>(&symbolCount), sizeof(symbolCount));

	for (const auto& [guildID, symbol] : m_mapSymbol)
	{
		const uint32_t id = guildID;
		const uint32_t dwSize = static_cast<uint32_t>(symbol.raw.size());

		file.write(reinterpret_cast<const char*>(&id), sizeof(id));
		file.write(reinterpret_cast<const char*>(&dwSize), sizeof(dwSize));

		if (!symbol.raw.empty())
			file.write(reinterpret_cast<const char*>(symbol.raw.data()), dwSize);
	}
}

// ---------------------------------------------------------------------------
// [SERVER] UploadSymbol - receive symbol data from client
// ---------------------------------------------------------------------------
void CGuildMarkManager::UploadSymbol(uint32_t guildID, int iSize, const uint8_t* pbyData)
{
	if (iSize < 0)
	{
		LOG_ERROR("MarkManager::UploadSymbol: negative size {} for guild {}", iSize, guildID);
		return;
	}

	if (static_cast<uint32_t>(iSize) > MAX_SYMBOL_SIZE)
	{
		LOG_ERROR("MarkManager::UploadSymbol: size {} exceeds limit {} for guild {}", iSize, MAX_SYMBOL_SIZE, guildID);
		return;
	}

	LOG_INFO("MarkManager::UploadSymbol: guild={} size={}", guildID, iSize);

	auto [it, inserted] = m_mapSymbol.try_emplace(guildID, TGuildSymbol{});
	TGuildSymbol& rSymbol = it->second;
	rSymbol.raw.clear();

	if (iSize > 0 && pbyData)
	{
		rSymbol.raw.assign(pbyData, pbyData + iSize);
		rSymbol.crc = GetCRC32(reinterpret_cast<const char*>(pbyData), iSize);
	}
}
