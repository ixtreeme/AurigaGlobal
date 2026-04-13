#pragma once
#include "lzo.h"
#include "FileBase.h"

#include <memory>
#include <cstdint>

class CMappedFile : public CFileBase
{
public:
	enum ESeekType
	{
		SEEK_TYPE_BEGIN,
		SEEK_TYPE_CURRENT,
		SEEK_TYPE_END
	};

public:
	CMappedFile();
	~CMappedFile() override;
#ifdef MEMORY_LEAK_FIX_RAZRO93
	void Link(uint32_t dwBufSize, const void* c_pvBufData, bool takeOwnership);

#else
	void		Link(uint32_t dwBufSize, const void* c_pvBufData);
#endif
	BOOL		Create(const char* filename);
	BOOL		Create(const char* filename, const void** dest, int offset, int size);
	const void*		Get();
	void		Destroy() override;
	int			Seek(uint32_t offset, int iSeekType = SEEK_TYPE_BEGIN);
	int			Map(const void **dest, int offset=0, int size=0);
	DWORD		Size() override;
	uint32_t		GetPosition() override;
	BOOL		Read(void* dest, int bytes) override;
	uint32_t		GetSeekPosition();
	void		BindLZObject(CLZObject * pLZObj);
	void		BindLZObjectWithBufferedSize(CLZObject * pLZObj);
	uint8_t*		AppendDataBlock( const void* pBlock, uint32_t dwBlockSize );

	uint8_t* GetCurrentSeekPoint();

private:
	void		Unmap(const void* data);

private:
	uint8_t*		m_pbBufLinkData;
#ifdef MEMORY_LEAK_FIX_RAZRO93
	bool		m_bOwnsLinkData;
#endif
	DWORD		m_dwBufLinkSize;

	std::unique_ptr<uint8_t[]>m_pbAppendResultDataBlock;
	uint32_t		m_dwAppendResultDataSize;

	uint32_t		m_seekPosition;
	HANDLE		m_hFM;
	uint32_t		m_dataOffset;
	uint32_t		m_mapSize;
	LPVOID		m_lpMapData;
	LPVOID		m_lpData;

	CLZObject *	m_pLZObj;
#ifdef MEMORY_LEAK_FIX_RAZRO93
	bool		m_bFreeOnDestroy;
#endif
};

