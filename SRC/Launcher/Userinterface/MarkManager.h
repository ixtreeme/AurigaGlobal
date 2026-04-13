// ===========================================================================
// [CLIENT] MarkManager.h
// Guild Mark Manager - Client Side
// Handles: block sync reception, local CRC comparison, symbol cache
// ===========================================================================
#ifndef __INC_METIN_II_CLIENT_MARK_MANAGER_H__
#define __INC_METIN_II_CLIENT_MARK_MANAGER_H__

#include "MarkImage.h"

#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

class CGuildMarkManager : public CSingleton<CGuildMarkManager>
{
public:
	enum
	{
		MAX_IMAGE_COUNT = 5,
		INVALID_MARK_ID = 0xffffffff,
		MAX_SYMBOL_SIZE = 64 * 1024,  // 64 KB
	};

	// [CLIENT] Symbol cache
	struct TGuildSymbol
	{
		uint32_t crc = 0;
		std::vector<uint8_t> raw;
	};

	CGuildMarkManager();
	~CGuildMarkManager();

	// Non-copyable
	CGuildMarkManager(const CGuildMarkManager&) = delete;
	CGuildMarkManager& operator=(const CGuildMarkManager&) = delete;

	// [CLIENT] Symbol API - client caches symbols locally
	const TGuildSymbol* GetGuildSymbol(uint32_t guildID) const;
	bool				LoadSymbol(const char* filename);
	void				SaveSymbol(const char* filename);
	void				UploadSymbol(uint32_t guildID, int iSize, const uint8_t* pbyData);

	// [CLIENT] Mark path/ID management
	void		SetMarkPathPrefix(const char* prefix);
	bool		GetMarkImageFilename(uint32_t imgIdx, std::string& path) const;
	bool		AddMarkIDByGuildID(uint32_t guildID, uint32_t markID);
	uint32_t	GetMarkImageCount() const;
	uint32_t	GetMarkCount() const;
	uint32_t	GetMarkID(uint32_t guildID) const;

	// [CLIENT] Mark image loading/saving - local cache management
	void		LoadMarkImages();
	void		SaveMarkImage(uint32_t imgIdx);

	// [CLIENT] Block sync - receive compressed blocks from server
	bool		SaveBlockFromCompressedData(uint32_t imgIdx, uint32_t posBlock,
		const uint8_t* pbBlock, uint32_t dwSize);

	// [CLIENT] CRC comparison - send to server to detect differences
	bool		GetBlockCRCList(uint32_t imgIdx, uint32_t* crcList);

private:
	// [CLIENT] Internal
	CGuildMarkImage* __GetImage(uint32_t imgIdx);

	std::map<uint32_t, std::unique_ptr<CGuildMarkImage>>	m_mapIdx_Image;
	std::map<uint32_t, uint32_t>							m_mapGID_MarkID;
	std::set<uint32_t>										m_setFreeMarkID;
	std::string												m_pathPrefix;

	// [CLIENT] Symbol cache
	std::map<uint32_t, TGuildSymbol>						m_mapSymbol;
};

#endif