#include "StdAfx.h"
#include "FileLoader.h"
#include <assert.h>

CMemoryTextFileLoader::CMemoryTextFileLoader()
{
}

CMemoryTextFileLoader::~CMemoryTextFileLoader()
{
}

bool CMemoryTextFileLoader::SplitLineByTab(uint32_t dwLine, CTokenVector* pstTokenVector)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	const std::string & c_rstLine = GetLineString(dwLine);
	const int c_iLineLength = c_rstLine.length();

	if (0 == c_iLineLength)
		return false;

	int basePos = 0;

	do
	{
		int beginPos = c_rstLine.find_first_of("\t", basePos);

		pstTokenVector->push_back(c_rstLine.substr(basePos, beginPos-basePos));

		basePos = beginPos+1;
	} while (basePos < c_iLineLength && basePos > 0);

	return true;
}

int CMemoryTextFileLoader::SplitLine2(uint32_t dwLine, CTokenVector* pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	uint32_t basePos = 0;

	do
	{
		int beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);

		if (beginPos < 0)
			return -1;

		int endPos;

		if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of("\"", beginPos);

			if (endPos < 0)
				return -2;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->push_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// 추가 코드. 맨뒤에 탭이 있는 경우를 체크한다. - [levites]
		if (int(c_rstLine.find_first_not_of(c_szDelimeter, basePos)) < 0)
			break;
	} while (basePos < c_rstLine.length());

	return 0;
}

bool CMemoryTextFileLoader::SplitLine(uint32_t dwLine, CTokenVector* pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	uint32_t basePos = 0;

	do
	{
		int beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);
		if (beginPos < 0)
			return false;

		int endPos;

		if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of("\"", beginPos);

			if (endPos < 0)
				return false;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->push_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// 추가 코드. 맨뒤에 탭이 있는 경우를 체크한다. - [levites]
		if (int(c_rstLine.find_first_not_of(c_szDelimeter, basePos)) < 0)
			break;
	} while (basePos < c_rstLine.length());

	return true;
}

uint32_t CMemoryTextFileLoader::GetLineCount()
{
	return m_stLineVector.size();
}

bool CMemoryTextFileLoader::CheckLineIndex(uint32_t dwLine)
{
	if (dwLine >= m_stLineVector.size())
		return false;

	return true;
}

const std::string & CMemoryTextFileLoader::GetLineString(uint32_t dwLine)
{
	assert(CheckLineIndex(dwLine));
	return m_stLineVector[dwLine];
}

void CMemoryTextFileLoader::Bind(int bufSize, const void* c_pvBuf)
{
	m_stLineVector.clear();
	m_stLineVector.reserve(128);

	std::string_view buffer(static_cast<const char*>(c_pvBuf), bufSize);

	std::string line;
	line.reserve(128);

	size_t i = 0;
	while (i < buffer.size()) {
		char c = buffer[i];

		if (c == '\r' || c == '\n') {
			m_stLineVector.emplace_back(std::move(line));
			line.clear();

			if (i + 1 < buffer.size()) {
				char next = buffer[i + 1];
				if ((c == '\r' && next == '\n') || (c == '\n' && next == '\r'))
					++i;
			}
		}
		else if (static_cast<unsigned char>(c) >= 0x80 && i + 1 < buffer.size()) {
			line += buffer.substr(i, 2);
			++i;
		}
		else {
			line += c;
		}

		++i;
	}

	if (!line.empty())
		m_stLineVector.emplace_back(std::move(line));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
int CMemoryFileLoader::GetSize()
{
	return m_size;
}

int CMemoryFileLoader::GetPosition()
{
	return m_pos;
}

bool CMemoryFileLoader::IsReadableSize(int size)
{
	if (m_pos + size > m_size)
		return false;

	return true;
}

bool CMemoryFileLoader::Read(int size, void* pvDst)
{
	if (!IsReadableSize(size))
		return false;

	memcpy(pvDst, GetCurrentPositionPointer(), size);
	m_pos += size;
	return true;
}

const char* CMemoryFileLoader::GetCurrentPositionPointer()
{
	assert(m_pcBase != NULL);
	return (m_pcBase + m_pos);
}

CMemoryFileLoader::CMemoryFileLoader(int size, const void* c_pvMemoryFile)
{
	assert(c_pvMemoryFile != NULL);

	m_pos = 0;
	m_size = size;
	m_pcBase = (const char *) c_pvMemoryFile;
}

CMemoryFileLoader::~CMemoryFileLoader()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
int CDiskFileLoader::GetSize()
{
	return m_size;
}

bool CDiskFileLoader::Read(int size, void* pvDst)
{
	assert(m_fp != NULL);

	int ret = fread(pvDst, size, 1, m_fp);

	if (ret <= 0)
		return false;

	return true;
}

bool CDiskFileLoader::Open(const char* c_szFileName)
{
	Close();

	if (!c_szFileName[0])
		return false;

	m_fp = fopen(c_szFileName, "rb");

	if (!m_fp)
		return false;

	fseek(m_fp, 0, SEEK_END);
	m_size = ftell(m_fp);
	fseek(m_fp, 0, SEEK_SET);
	return true;
}

void CDiskFileLoader::Close()
{
	if (m_fp)
		fclose(m_fp);

	Initialize();
}

void CDiskFileLoader::Initialize()
{
	m_fp = nullptr;
	m_size = 0;
}

CDiskFileLoader::CDiskFileLoader()
{
	Initialize();
}

CDiskFileLoader::~CDiskFileLoader()
{
	Close();
}
