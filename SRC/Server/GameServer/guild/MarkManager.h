// ===========================================================================
// [SERVER] MarkManager.h
// Guild Mark Manager - Server Side
// Handles: mark allocation, index persistence, symbol storage, block sync
// ===========================================================================
#ifndef __INC_METIN_II_SERVER_MARK_MANAGER_H__
#define __INC_METIN_II_SERVER_MARK_MANAGER_H__

#include "MarkImage.h"

#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

class CGuildMarkManager : public singleton<CGuildMarkManager>
{
public:
	enum
	{
		MAX_IMAGE_COUNT = 5,
		INVALID_MARK_ID = 0xffffffff,
		MAX_SYMBOL_SIZE = 64 * 1024,  // 64 KB max per guild symbol
	};

	// [SERVER] Symbol data
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

	// [SERVER] Symbol API - server stores and serves guild symbols
	const TGuildSymbol*	GetGuildSymbol(uint32_t guildID) const;
	bool				LoadSymbol(const char* filename);
	void				SaveSymbol(const char* filename);
	void				UploadSymbol(uint32_t guildID, int iSize, const uint8_t* pbyData);

	// [SERVER] Mark path/image management
	void		SetMarkPathPrefix(const char* prefix);
	bool		GetMarkImageFilename(uint32_t imgIdx, std::string& path) const;
	void		LoadMarkImages();
	void		SaveMarkImage(uint32_t imgIdx);

	// [SERVER] Mark index persistence - server owns the index file
	bool		LoadMarkIndex();
	bool		SaveMarkIndex();

	// [SERVER] Mark ID management
	bool		AddMarkIDByGuildID(uint32_t guildID, uint32_t markID);
	uint32_t	GetMarkImageCount() const;
	uint32_t	GetMarkCount() const;
	uint32_t	GetMarkID(uint32_t guildID) const;

	// [SERVER] Mark operations - server creates/deletes marks
	void		CopyMarkIdx(char* pcBuf, size_t bufSize) const;
	uint32_t	SaveMark(uint32_t guildID, uint8_t* pbMarkImage);
	void		DeleteMark(uint32_t guildID);

	// [SERVER] Block sync - server compares CRCs and sends diffs to client
	void		GetDiffBlocks(uint32_t imgIdx, const uint32_t* crcList,
	                          std::map<uint8_t, const SGuildMarkBlock*>& mapDiffBlocks);
	bool		GetBlockCRCList(uint32_t imgIdx, uint32_t* crcList);

private:
	// [SERVER] Internal helpers
	CGuildMarkImage*	__GetImage(uint32_t imgIdx);
	uint32_t			__AllocMarkID(uint32_t guildID);

	std::map<uint32_t, std::unique_ptr<CGuildMarkImage>>	m_mapIdx_Image;
	std::map<uint32_t, uint32_t>							m_mapGID_MarkID;
	std::set<uint32_t>										m_setFreeMarkID;
	std::string												m_pathPrefix;

	// [SERVER] Symbol storage
	std::map<uint32_t, TGuildSymbol>						m_mapSymbol;
};

#endif
