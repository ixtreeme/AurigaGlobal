#include <string.h>
#include <stdio.h>
#include <algorithm>
#include <common/stl.h>
#include "grid.h"
#include <common/service.h>
#ifdef ENABLE_GIRD_BUG_FIX
#include <cstddef>
#include <limits>

// Windows header-ek sokszor definialnak min/max macro-t
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

static inline int GridClampNonNeg(int v)
{
	return (v < 0) ? 0 : v;
}

static inline size_t GridCellCountSafe(int w, int h)
{
	// w/h mar clampelve van >=0-ra
	if (w == 0 || h == 0)
		return 0;
	return static_cast<size_t>(w) * static_cast<size_t>(h);
}
#endif

CGrid::CGrid(int w, int h) : m_iWidth(w), m_iHeight(h)
{
#ifdef ENABLE_GIRD_BUG_FIX
	m_iWidth = GridClampNonNeg(m_iWidth);
	m_iHeight = GridClampNonNeg(m_iHeight);

	const size_t cells = GridCellCountSafe(m_iWidth, m_iHeight);

	if (!cells)
	{
		m_pGrid = new char[1];
		m_pGrid[0] = 0;
		return;
	}

	m_pGrid = new char[cells];
	memset(m_pGrid, 0, cells);
#else
	m_pGrid = new char[m_iWidth * m_iHeight];
	memset(m_pGrid, 0, sizeof(char) * m_iWidth * m_iHeight);
#endif
}

CGrid::CGrid(CGrid* pkGrid, int w, int h) : m_iWidth(w), m_iHeight(h)
{
#ifdef ENABLE_GIRD_BUG_FIX
	m_iWidth = GridClampNonNeg(m_iWidth);
	m_iHeight = GridClampNonNeg(m_iHeight);

	const size_t dstCells = GridCellCountSafe(m_iWidth, m_iHeight);

	if (!dstCells)
	{
		m_pGrid = new char[1];
		m_pGrid[0] = 0;
		return;
	}

	m_pGrid = new char[dstCells];

	size_t srcCells = 0;
	if (pkGrid)
	{
		const int sw = GridClampNonNeg(pkGrid->m_iWidth);
		const int sh = GridClampNonNeg(pkGrid->m_iHeight);
		srcCells = GridCellCountSafe(sw, sh);
	}

	const size_t copySize = (srcCells < dstCells) ? srcCells : dstCells;

	if (copySize && pkGrid && pkGrid->m_pGrid)
		memcpy(m_pGrid, pkGrid->m_pGrid, copySize);

	if (dstCells > copySize)
		memset(m_pGrid + copySize, 0, dstCells - copySize);
#else
	m_pGrid = new char[m_iWidth * m_iHeight];
	int iSize = std::min(w * h, pkGrid->m_iWidth * pkGrid->m_iHeight);
	memcpy(m_pGrid, pkGrid->m_pGrid, sizeof(char) * iSize);
#endif
}

CGrid::~CGrid()
{
	delete[] m_pGrid;
}

void CGrid::Clear()
{
#ifdef ENABLE_GIRD_BUG_FIX
	m_iWidth = GridClampNonNeg(m_iWidth);
	m_iHeight = GridClampNonNeg(m_iHeight);

	const size_t cells = GridCellCountSafe(m_iWidth, m_iHeight);
	if (!cells)
	{
		if (m_pGrid)
			m_pGrid[0] = 0;
		return;
	}
	memset(m_pGrid, 0, cells);
#else
	memset(m_pGrid, 0, sizeof(char) * m_iWidth * m_iHeight);
#endif
}

int CGrid::FindBlank(int w, int h)
{
#ifdef ENABLE_GIRD_BUG_FIX
	if (m_iWidth <= 0 || m_iHeight <= 0)
		return -1;
	if (w <= 0 || h <= 0)
		return -1;
#endif

	if (w > m_iWidth || h > m_iHeight)
		return -1;

	for (int iRow = 0; iRow < m_iHeight; ++iRow)
	{
		for (int iCol = 0; iCol < m_iWidth; ++iCol)
		{
			int iIndex = iRow * m_iWidth + iCol;
			if (IsEmpty(iIndex, w, h))
				return iIndex;
		}
	}

	return -1;
}

bool CGrid::Put(int iPos, int w, int h)
{
	if (!IsEmpty(iPos, w, h))
		return false;

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			int index = iPos + x + y * m_iWidth;
			m_pGrid[index] = true;
		}
	}

	return true;
}

void CGrid::Get(int iPos, int w, int h)
{
#ifdef ENABLE_GIRD_BUG_FIX
	if (m_iWidth <= 0 || m_iHeight <= 0)
		return;
	if (w <= 0 || h <= 0)
		return;

	const size_t cells = GridCellCountSafe(m_iWidth, m_iHeight);
	if (iPos < 0 || static_cast<size_t>(iPos) >= cells)
		return;
#else
	if (iPos < 0 || iPos >= m_iWidth * m_iHeight)
		return;
#endif

	for (int y = 0; y < h; ++y)
	{
		int iStart = iPos + (y * m_iWidth);
		m_pGrid[iStart] = false;

		int x = 1;
		while (x < w)
			m_pGrid[iStart + x++] = false;
	}
}

bool CGrid::IsEmpty(int iPos, int w, int h)
{
#ifdef ENABLE_GIRD_BUG_FIX
	if (m_iWidth <= 0 || m_iHeight <= 0)
		return false;
	if (w <= 0 || h <= 0)
		return false;

	const size_t cells = GridCellCountSafe(m_iWidth, m_iHeight);
	if (iPos < 0 || static_cast<size_t>(iPos) >= cells)
		return false;
#else
	if (iPos < 0)
		return false;
#endif

	int iRow = iPos / m_iWidth;
	int iCol = iPos % m_iWidth;

	if (iRow + h > m_iHeight)
		return false;

	if (iCol + w > m_iWidth)
		return false;

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			int index = iPos + x + y * m_iWidth;
			if (m_pGrid[index])
				return false;
		}
	}

	return true;
}

void CGrid::Print()
{
	printf("Grid %p\n", this);

	for (int y = 0; y < m_iHeight; ++y)
	{
		for (int x = 0; x < m_iWidth; ++x)
			printf("%d", m_pGrid[y * m_iWidth + x]);

		printf("\n");
	}
}

uint32_t CGrid::GetSize()
{
#ifdef ENABLE_GIRD_BUG_FIX
	const size_t cells = GridCellCountSafe(GridClampNonNeg(m_iWidth), GridClampNonNeg(m_iHeight));
	const size_t maxu32 = static_cast<size_t>(std::numeric_limits<uint32_t>::max());
	return static_cast<uint32_t>((cells < maxu32) ? cells : maxu32);
#else
	return m_iWidth * m_iHeight;
#endif
}
