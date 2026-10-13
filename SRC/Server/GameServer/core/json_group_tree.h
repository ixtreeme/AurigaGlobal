#pragma once

#include <common/d3dtype.h>

#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

typedef std::vector<std::string> TTokenVector;
typedef std::map<std::string, TTokenVector, std::less<>> TTokenVectorMap;

typedef std::map<std::string, int, std::less<>> TMapNameToIndex;

// Rows/columns tree loaded from JSON (replaces CGroupTextParseTreeLoader).
// JSON shape:
//
//   {
//     "columns": ["col1", "col2"],          // from the old "#--#" line
//     "tokens": { "rowkey": ["v1", "v2"] },
//     "groups": [
//       { "name": "grp", "columns": [...], "tokens": { ... }, "groups": [...] }
//     ]
//   }
//
// Names, keys and column names are lowercased on load; values are kept
// verbatim. Files come from SRC/Tools/txt2json/txt2json_group.py (rows mode).
class CJsonGroupNode
{
public:
	class CJsonGroupRow
	{
	public:
		CJsonGroupRow(CJsonGroupNode* pGroupNode, TTokenVector& vec_values);
		virtual ~CJsonGroupRow();

		template <typename T>
		bool GetValue(std::string_view stColKey, OUT T& value) const;
		template <typename T>
		bool GetValue(int idx, OUT T& value) const;

		int GetSize() const;

	private:
		CJsonGroupNode*	m_pOwnerGroupNode;
		TTokenVector	m_vec_values;
	};
public:
	CJsonGroupNode();
	virtual ~CJsonGroupNode();

	uint32_t GetChildNodeCount();
	bool SetChildNode(const char* c_szKey, CJsonGroupNode* pChildNode);
	CJsonGroupNode* GetChildNode(std::string_view c_rstrKey) const;
	std::string GetNodeName() const;

	bool IsToken(std::string_view c_rstrKey) const;

	int GetRowCount();

	template <typename T>
	bool GetValue(size_t i, std::string_view c_rstrColKey, T& tValue) const;
	template <typename T>
	bool GetValue(std::string_view c_rstrRowKey, std::string_view c_rstrColKey, T& tValue) const;
	template <typename T>
	bool GetValue(std::string_view c_rstrRowKey, int index, T& tValue) const;

	bool GetRow(std::string_view c_rstrKey, OUT const CJsonGroupNode::CJsonGroupRow ** ppRow) const;
	bool GetRow(int idx, OUT const CJsonGroupNode::CJsonGroupRow ** ppRow) const;
	bool GetGroupRow(std::string_view stGroupName, std::string_view stRow, OUT const CJsonGroupNode::CJsonGroupRow ** ppRow) const;

	template <typename T>
	bool GetGroupValue(std::string_view stGroupName, std::string_view stRow, int iCol, OUT T& tValue) const;
	template <typename T>
	bool GetGroupValue(std::string_view stGroupName, std::string_view stRow, std::string_view stCol, OUT T& tValue) const;

	int	GetColumnIndexFromName(std::string_view stName) const;

private:
	typedef std::map <std::string, CJsonGroupNode*, std::less<>> TMapGroup;
	typedef std::map <std::string, CJsonGroupNode::CJsonGroupRow, std::less<>> TMapRow;
	TMapGroup				m_mapChildNodes;
	std::string strGroupName;

	TMapNameToIndex			m_map_columnNameToIndex;
	TMapRow					m_map_rows;
	friend class CJsonGroupTreeLoader;
};

class CJsonGroupTreeLoader
{
public:
	CJsonGroupTreeLoader();
	virtual ~CJsonGroupTreeLoader();

	CJsonGroupTreeLoader(const CJsonGroupTreeLoader&) = delete;
	CJsonGroupTreeLoader& operator=(const CJsonGroupTreeLoader&) = delete;

	bool Load(const char* c_szFileName);
	const char* GetFileName();

	CJsonGroupNode*	GetGroup(const char* c_szGroupName);
private:
	CJsonGroupNode *	m_pRootGroupNode;
	std::string		m_strFileName;
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
bool CJsonGroupNode::GetValue(size_t i, std::string_view c_rstrColKey, T& tValue) const
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
bool CJsonGroupNode::GetValue(std::string_view c_rstrRowKey, std::string_view c_rstrColKey, T& tValue) const
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
bool CJsonGroupNode::GetValue(std::string_view c_rstrRowKey, int index, T& tValue) const
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
bool CJsonGroupNode::GetGroupValue(std::string_view stGroupName, std::string_view stRow, int iCol, OUT T& tValue) const
{
	CJsonGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, iCol, tValue))
			return true;
	}
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, iCol, tValue))
			return true;
	}
	return false;
}

template <typename T>
bool CJsonGroupNode::GetGroupValue(std::string_view stGroupName, std::string_view stRow, std::string_view stCol, OUT T& tValue) const
{
	CJsonGroupNode* pChildGroup = GetChildNode(stGroupName);
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, stCol, tValue))
			return true;
	}
	pChildGroup = GetChildNode("default");
	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetValue(stRow, stCol, tValue))
			return true;
	}
	return false;
}

template <typename T>
bool CJsonGroupNode::CJsonGroupRow::GetValue(std::string_view stColKey, OUT T& value) const
{
	int idx = m_pOwnerGroupNode->GetColumnIndexFromName(stColKey);
	if (idx < 0 || (TTokenVectorMap::size_type)idx >= m_vec_values.size())
		return false;
	return from_string(value, m_vec_values[idx]);
}

template <typename T>
bool CJsonGroupNode::CJsonGroupRow::GetValue(int idx, OUT T& value) const
{
	if (idx < 0 || (TTokenVectorMap::size_type)idx >= m_vec_values.size())
		return false;
	return from_string(value, m_vec_values[idx]);
}
