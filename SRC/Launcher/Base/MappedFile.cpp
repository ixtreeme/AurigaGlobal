#include "StdAfx.h"
#include "MappedFile.h"
#include "Debug.h"

CMappedFile::CMappedFile() :
	m_pbBufLinkData(nullptr),
	m_dwBufLinkSize(0), m_pbAppendResultDataBlock(nullptr),
	m_dwAppendResultDataSize(0),
	m_seekPosition(0),
	m_hFM(nullptr),
	m_dataOffset(0),
	m_mapSize(0),
	m_lpMapData(nullptr),
	m_lpData(nullptr),
	m_pLZObj(nullptr),
#ifdef MEMORY_LEAK_FIX_RAZRO93
	m_bOwnsLinkData(false)
	//m_bFreeOnDestroy(0) Valamiért el crashel tõle az audio...
#endif
{
}

CMappedFile::~CMappedFile()
{
	CMappedFile::Destroy();
}

BOOL CMappedFile::Create(const char * filename)
{
	Destroy();
	return CFileBase::Create(filename, FILEMODE_READ);
}

BOOL CMappedFile::Create(const char * filename, const void** dest, int offset, int size)
{
	if (!CMappedFile::Create(filename))
		return NULL;

	int ret = Map(dest, offset, size);
	return (ret) > 0;
}

const void* CMappedFile::Get()
{
	return m_lpData;
}
#ifdef MEMORY_LEAK_FIX_RAZRO93

void CMappedFile::Link(uint32_t dwBufSize, const void* c_pvBufData, bool takeOwnership)
{
	if (m_bOwnsLinkData && m_pbBufLinkData)
	{
		delete[] m_pbBufLinkData;
		m_pbBufLinkData = nullptr;
	}

	m_dwBufLinkSize = dwBufSize;
	m_pbBufLinkData = (uint8_t*)c_pvBufData;
	m_bOwnsLinkData = takeOwnership;
}


#else
void CMappedFile::Link(uint32_t dwBufSize, const void* c_pvBufData)
{
	m_dwBufLinkSize=dwBufSize;
	m_pbBufLinkData=(uint8_t*)c_pvBufData;
}
#endif
void CMappedFile::BindLZObject(CLZObject* pLZObj)
{
	assert(m_pLZObj == NULL);
	m_pLZObj = pLZObj;

#ifdef MEMORY_LEAK_FIX_RAZRO93
	Link(m_pLZObj->GetSize(), m_pLZObj->GetBuffer(), false);
#else
	Link(m_pLZObj->GetSize(), m_pLZObj->GetBuffer());
#endif
}


void CMappedFile::BindLZObjectWithBufferedSize(CLZObject* pLZObj)
{
	assert(m_pLZObj == NULL);
	m_pLZObj = pLZObj;

#ifdef MEMORY_LEAK_FIX_RAZRO93
	Link(m_pLZObj->GetBufferSize(), m_pLZObj->GetBuffer(), false);
#else
	Link(m_pLZObj->GetBufferSize(), m_pLZObj->GetBuffer());
#endif
}


uint8_t* CMappedFile::AppendDataBlock(const void* pBlock, uint32_t dwBlockSize)
{

	m_dwAppendResultDataSize = m_dwBufLinkSize + dwBlockSize;
	m_pbAppendResultDataBlock = std::make_unique<uint8_t[]>(m_dwAppendResultDataSize);

	memcpy(m_pbAppendResultDataBlock.get(), m_pbBufLinkData, m_dwBufLinkSize);
	memcpy(m_pbAppendResultDataBlock.get() + m_dwBufLinkSize, pBlock, dwBlockSize);

	Link(m_dwAppendResultDataSize, m_pbAppendResultDataBlock.get(), true);

	return m_pbAppendResultDataBlock.get();
}


void CMappedFile::Destroy()
{
	if (m_pLZObj)
	{
		delete m_pLZObj;
		m_pLZObj = nullptr;
	}

	if (nullptr != m_lpMapData)
	{
		Unmap(m_lpMapData);
		m_lpMapData = nullptr;
	}

	if (nullptr != m_hFM)
	{
		CloseHandle(m_hFM);
		m_hFM = nullptr;
	}

	m_dwAppendResultDataSize = 0;

#ifdef MEMORY_LEAK_FIX_RAZRO93
	if (m_bOwnsLinkData && m_pbBufLinkData)
	{
		delete[] m_pbBufLinkData;
		m_pbBufLinkData = nullptr;
		m_dwBufLinkSize = 0;
		m_bOwnsLinkData = false;
	}
	else
	{
		m_pbBufLinkData = nullptr;
		m_dwBufLinkSize = 0;
	}
#else
	m_pbBufLinkData = nullptr;
	m_dwBufLinkSize = 0;
#endif


	m_seekPosition = 0;
	m_dataOffset = 0;
	m_mapSize = 0;

	CFileBase::Destroy();
}

int CMappedFile::Seek(uint32_t offset, int iSeekType)
{
	switch (iSeekType)
	{
		case SEEK_TYPE_BEGIN:
			if (offset > m_dwSize)
				offset = m_dwSize;

			m_seekPosition = offset;
			break;

		case SEEK_TYPE_CURRENT:
			m_seekPosition = min(m_seekPosition + offset, Size());
			break;

		case SEEK_TYPE_END:
			m_seekPosition = max(0, Size() - offset);
			break;
	}

	return m_seekPosition;
}

// 2004.09.16.myevan.MemoryMappedFile 98/ME °³¼ö Á¦ÇÑ ¹®Á¦ Ã¼Å©
//uint32_t g_dwCount=0;

int CMappedFile::Map(const void** dest, int offset, int size)
{
	m_dataOffset = offset;

	if (size == 0)
		m_mapSize = m_dwSize;
	else
		m_mapSize = size;

	if (m_dataOffset + m_mapSize > m_dwSize)
		return NULL;


	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);
	uint32_t dwSysGran = SysInfo.dwAllocationGranularity;
	uint32_t dwFileMapStart = (m_dataOffset / dwSysGran) * dwSysGran;
	uint32_t dwMapViewSize = (m_dataOffset % dwSysGran) + m_mapSize;
	INT iViewDelta = m_dataOffset - dwFileMapStart;

	m_hFM = CreateFileMapping(
		m_hFile,
		nullptr,
		PAGE_READONLY,
		0,
		m_dataOffset + m_mapSize,
		nullptr);

	if (!m_hFM)
	{
		OutputDebugString("CMappedFile::Map !m_hFM\n");
		return NULL;
	}

	m_lpMapData = MapViewOfFile(
		m_hFM,
		FILE_MAP_READ,
		0,
		dwFileMapStart,
		dwMapViewSize);

	if (!m_lpMapData)
	{
		TraceError("CMappedFile::Map !m_lpMapData %lu", GetLastError());
		return 0;
	}

	m_lpData = (char*)m_lpMapData + iViewDelta;
	*dest = (char*)m_lpData;
	m_seekPosition = 0;

#ifdef MEMORY_LEAK_FIX_RAZRO93
	Link(m_mapSize, m_lpData, false);
#else
	Link(m_mapSize, m_lpData);
#endif

	return m_mapSize;
}


uint8_t* CMappedFile::GetCurrentSeekPoint()
{
	return m_pbBufLinkData+m_seekPosition;
	//return m_pLZObj ? m_pLZObj->GetBuffer() + m_seekPosition : (uint8_t *) m_lpData + m_seekPosition;
}


DWORD CMappedFile::Size()
{
	return m_dwBufLinkSize;
	/*
	if (m_pLZObj)
		return m_pLZObj->GetSize();

	return (m_mapSize);
	*/
}

uint32_t CMappedFile::GetPosition()
{
	return m_dataOffset;
}

BOOL CMappedFile::Read(void * dest, int bytes)
{
	if (m_seekPosition + bytes > Size())
		return FALSE;

	memcpy(dest, GetCurrentSeekPoint(), bytes);
	m_seekPosition += bytes;
	return TRUE;
}

uint32_t CMappedFile::GetSeekPosition(void)
{
	return m_seekPosition;
}

void CMappedFile::Unmap(const void* data)
{
	if (UnmapViewOfFile(data))
	{
		// 2004.09.16.myevan.MemoryMappedFile 98/ME °³¼ö Á¦ÇÑ ¹®Á¦ Ã¼Å©
		//g_dwCount--;
		//Tracenf("UNMAPFILE %d", g_dwCount);
	}
	else
	{
		TraceError("CMappedFile::Unmap - Error");
	}
	m_lpData = nullptr;
}
