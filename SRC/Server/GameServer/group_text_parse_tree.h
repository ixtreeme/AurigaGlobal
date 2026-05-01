#pragma once

#include <common/d3dtype.h>
#include <common/pool.h>
#include "file_loader.h"
#include <sstream>
#include <string_view>
typedef std::map<std::string, TTokenVector, std::less<>>	TTokenVectorMap;
typedef std::map<std::string, int, std::less<>> TMapNameToIndex;

class CGroupNode
{
public:
	class CGroupNodeRow
	{
	public:
		CGroupNodeRow(CGroupNode* pGroupNode, TTokenVector& vec_values);
		virtual ~CGroupNodeRow();

		template <typename T>
		bool GetValue(std::string_view stColKey, OUT T& value) const;
		template <typename T>
		bool GetValue(int idx, OUT T& value) const;

		int GetSize() const;

	private:
		CGroupNode*		m_pOwnerGroupNode;
		TTokenVector	m_vec_values;
	};
public:
	CGroupNode();
	virtual ~CGroupNode();

	bool Load(const char * c_szFileName);
	const char * GetFileName();

	uint32_t GetChildNodeCount();
	bool SetChildNode(const char * c_szKey, CGroupNode* pChildNode);
	CGroupNode* GetChildNode(std::string_view c_rstrKey) const;
	std::string GetNodeName() const;

	bool IsToken(std::string_view c_rstrKey) const;

	int GetRowCount();

	template <typename T>
	bool GetValue(size_t i, std::string_view c_rstrColKey, T& tValue) const;	// n번째(map에 들어있는 순서일 뿐, txt의 순서와는 관계 없음) row의 특정 컬럼의 값을 반환하는 함수.
																				// 이질적이긴 하지만, 편의를 위한 함수.
	template <typename T>
	bool GetValue(std::string_view c_rstrRowKey, std::string_view c_rstrColKey, T& tValue) const;
	template <typename T>
	bool GetValue(std::string_view c_rstrRowKey, int index, T& tValue) const;

	bool GetRow(std::string_view c_rstrKey, OUT const CGroupNodeRow ** ppRow) const;
	// 참고로, idx랑 txt에 쓰여진 순서랑 관계 없음.
	bool GetRow(int idx, OUT const CGroupNodeRow ** ppRow) const;
	bool GetGroupRow(std::string_view stGroupName, std::string_view stRow, OUT const CGroupNode::CGroupNodeRow ** ppRow) const;

	template <typename T>
	bool GetGroupValue(std::string_view stGroupName, std::string_view stRow, int iCol, OUT T& tValue) const;
	template <typename T>
	bool GetGroupValue(std::string_view stGroupName, std::string_view stRow, std::string_view stCol, OUT T& tValue) const;

	int	GetColumnIndexFromName(std::string_view stName) const;

private:
	typedef std::map <std::string, CGroupNode*, std::less<>> TMapGroup;
	typedef std::map <std::string, CGroupNode::CGroupNodeRow, std::less<>> TMapRow;
	TMapGroup				m_mapChildNodes;
	std::string strGroupName;

	TMapNameToIndex			m_map_columnNameToIndex;
	TMapRow					m_map_rows;
	friend class CGroupTextParseTreeLoader;
};

class CGroupTextParseTreeLoader
{
public:
	CGroupTextParseTreeLoader();
	virtual ~CGroupTextParseTreeLoader();

	bool Load(const char * c_szFileName);
	const char * GetFileName();

	CGroupNode*	GetGroup(const char * c_szGroupName);
private:
	bool LoadGroup(CGroupNode * pGroupNode);

	CGroupNode *				m_pRootGroupNode;
	std::string					m_strFileName;
	uint32_t						m_dwcurLineIndex;

	CMemoryTextFileLoader		m_fileLoader;
};

template <typename T>
bool from_string(OUT T& t, IN std::string_view s)
{
	std::istringstream iss(std::string{s});
	return !(iss >> t).fail();
}

template <>
inline bool from_string <uint8_t>(OUT uint8_t& t, IN std::string_view s)
{
	std::istringstream iss(std::string{s});
	int temp;
	bool b = !(iss >> temp).fail();
	t = (uint8_t)temp;
	return b;
}

template <typename T>
bool CGroupNode::GetValue(size_t i, std::string_view c_rstrColKey, T& tValue) const
{
	if (i > m_map_rows.size())
		return false;

	TMapRow::const_iterator row_it = m_map_rows.begin();
	std::advance(row_it, i);

	auto col_idx_it = m_map_columnNameToIndex.find(c_rstrColKey);
	if (m_map_columnNameToIndex.end() == col_idx_it)
	{
		return false;
	}

	int index = col_idx_it->second;
	if (row_it->second.GetSize() <= index)
	{
		return false;
	}

	return row_it->second.GetValue(index, tValue);
}

template <typename T>
bool CGroupNode::GetValue(std::string_view c_rstrRowKey, std::string_view c_rstrColKey, T& tValue) const
{
	TMapRow::const_iterator row_it = m_map_rows.find(c_rstrRowKey);
	if (m_map_rows.end() == row_it)
	{
		return false;
	}
	auto col_idx_it = m_map_columnNameToIndex.find(c_rstrColKey);
	if (m_map_columnNameToIndex.end() == col_idx_it)
	{
		return false;
	}

	int index = col_idx_it->second;
	if (row_it->second.GetSize() <= index)
	{
		return false;
	}

	return row_it->second.GetValue(index, tValue);
}

template <typename T>
bool CGroupNode::GetValue(std::string_view c_rstrRowKey, int index, T& tValue) const
{
	TMapRow::const_iterator row_it = m_map_rows.find(c_rstrRowKey);
	if (m_map_rows.end() == row_it)
	{
		return false;
	}

	if (row_it->second.GetSize() <= index)
	{
		return false;
	}
	return row_it->second.GetValue(index, tValue);
}

template <typename T>
bool CGroupNode::GetGroupValue(std::string_view stGroupName, std::string_view stRow, int iCol, OUT T& tValue) const
{
	CGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, iCol, tValue))
			return true;
	}
	// default group을 살펴봄.
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, iCol, tValue))
			return true;
	}
	return false;
}

template <typename T>
bool CGroupNode::GetGroupValue(std::string_view stGroupName, std::string_view stRow, std::string_view stCol, OUT T& tValue) const
{
	CGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, stCol, tValue))
			return true;
	}
	// default group을 살펴봄.
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, stCol, tValue))
			return true;
	}
	return false;
}

template <typename T>
bool CGroupNode::CGroupNodeRow::GetValue(std::string_view stColKey, OUT T& value) const
{
	int idx = m_pOwnerGroupNode->GetColumnIndexFromName(stColKey);
	if (idx < 0 || (TTokenVectorMap::size_type)idx >= m_vec_values.size())
		return false;
	return from_string(value, m_vec_values[idx]);
}

template <typename T>
bool CGroupNode::CGroupNodeRow::GetValue(int idx, OUT T& value) const
{
	if (idx < 0 || (TTokenVectorMap::size_type)idx >= m_vec_values.size())
		return false;
	return from_string(value, m_vec_values[idx]);
}
