#include "stdafx.h"
#include "json_group_tree.h"
#include "JsonFile.h"

#include <common/stl.h>

#include <nlohmann/json.hpp>

#include <functional>



CJsonGroupTreeLoader::CJsonGroupTreeLoader()
	: m_pRootGroupNode(nullptr)
{
}

CJsonGroupTreeLoader::~CJsonGroupTreeLoader()
{
	delete m_pRootGroupNode;
}

const char* CJsonGroupTreeLoader::GetFileName()
{
	return m_strFileName.c_str();
}

bool CJsonGroupTreeLoader::Load(const char* c_szFileName)
{
	m_strFileName = c_szFileName ? c_szFileName : "";

	nlohmann::json doc;

	if (!JsonFile::Load(m_strFileName, doc))
		return false;

	delete m_pRootGroupNode;
	m_pRootGroupNode = new CJsonGroupNode();

	std::function<void(const nlohmann::json&, CJsonGroupNode*)> parseNode;

	parseNode = [&parseNode](const nlohmann::json& doc, CJsonGroupNode* node)
	{
		if (doc.contains("columns") && doc.at("columns").is_array())
		{
			int index = 0;

			for (const auto& column : doc.at("columns"))
			{
				std::string name = JsonFile::ToToken(column);
				stl_lowers(name);
				node->m_map_columnNameToIndex.insert(TMapNameToIndex::value_type(name, index));
				++index;
			}
		}

		if (doc.contains("tokens") && doc.at("tokens").is_object())
		{
			for (const auto& item : doc.at("tokens").items())
			{
				std::string key = item.key();
				stl_lowers(key);

				TTokenVector values;

				if (item.value().is_array())
				{
					for (const auto& value : item.value())
						values.push_back(JsonFile::ToToken(value));
				}
				else
				{
					values.push_back(JsonFile::ToToken(item.value()));
				}

				node->m_map_rows.insert(std::make_pair(key, CJsonGroupNode::CJsonGroupRow(node, values)));
			}
		}

		if (!doc.contains("groups") || !doc.at("groups").is_array())
			return;

		for (const auto& child : doc.at("groups"))
		{
			auto* childNode = new CJsonGroupNode;
			childNode->strGroupName = child.value("name", std::string());
			stl_lowers(childNode->strGroupName);

			parseNode(child, childNode);

			// The old loader kept the first group on a duplicate name.
			if (!node->SetChildNode(childNode->strGroupName.c_str(), childNode))
				delete childNode;
		}
	};

	parseNode(doc, m_pRootGroupNode);

	return true;
}

CJsonGroupNode* CJsonGroupTreeLoader::GetGroup(const char* c_szGroupName)
{
	if (nullptr == m_pRootGroupNode)
		return nullptr;

	return m_pRootGroupNode->GetChildNode(c_szGroupName);
}

CJsonGroupNode::CJsonGroupNode()
{
}

CJsonGroupNode::~CJsonGroupNode()
{
	for (TMapGroup::iterator it = m_mapChildNodes.begin(); it != m_mapChildNodes.end(); it++)
	{
		delete it->second;
	}
}

uint32_t CJsonGroupNode::GetChildNodeCount()
{
	return m_mapChildNodes.size();
}

bool CJsonGroupNode::SetChildNode(const char* c_szKey, CJsonGroupNode* pChildGroup)
{
	if (nullptr == pChildGroup)
	{
		m_mapChildNodes.erase(c_szKey);
		return true;
	}

	TMapGroup::iterator it = m_mapChildNodes.find(c_szKey);

	if (it != m_mapChildNodes.end())
		return false;

	m_mapChildNodes.insert(TMapGroup::value_type(c_szKey, pChildGroup));

	return true;
}

CJsonGroupNode* CJsonGroupNode::GetChildNode(std::string_view c_rstrKey) const
{
	TMapGroup::const_iterator it = m_mapChildNodes.find(c_rstrKey);

	if (it != m_mapChildNodes.end())
		return it->second;

	return nullptr;
}

std::string CJsonGroupNode::GetNodeName() const
{
	return strGroupName;
}

bool CJsonGroupNode::IsToken(std::string_view c_rstrKey) const
{
	return m_map_rows.end() != m_map_rows.find(c_rstrKey);
}

int CJsonGroupNode::GetRowCount()
{
	return m_map_rows.size();
}

bool CJsonGroupNode::GetRow(std::string_view c_rstrRowKey, OUT const CJsonGroupNode::CJsonGroupRow ** ppRow) const
{
	TMapRow::const_iterator row_it = m_map_rows.find(c_rstrRowKey);

	if (m_map_rows.end() == row_it)
		return false;

	*ppRow = &row_it->second;

	return true;
}

bool CJsonGroupNode::GetRow(int idx, OUT const CJsonGroupNode::CJsonGroupRow ** ppRow) const
{
	if ((TMapRow::size_type)idx >= m_map_rows.size())
		return false;

	TMapRow::const_iterator row_it = m_map_rows.begin();

	std::advance(row_it, idx);

	*ppRow = &row_it->second;

	return true;
}

bool CJsonGroupNode::GetGroupRow(std::string_view stGroupName, std::string_view stRow, OUT const CJsonGroupNode::CJsonGroupRow ** ppRow) const
{
	CJsonGroupNode* pChildGroup = GetChildNode(stGroupName);

	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetRow(stRow, ppRow))
			return true;
	}

	pChildGroup = GetChildNode("default");

	if (nullptr != pChildGroup)
	{
		if (pChildGroup->GetRow(stRow, ppRow))
			return true;
	}

	return false;
}

int	CJsonGroupNode::GetColumnIndexFromName(std::string_view stName) const
{
	TMapNameToIndex::const_iterator it = m_map_columnNameToIndex.find(stName);

	if (m_map_columnNameToIndex.end() == it)
		return -1;

	return it->second;
}

CJsonGroupNode::CJsonGroupRow::CJsonGroupRow(CJsonGroupNode* pGroupNode, TTokenVector& vec_values)
{
	m_pOwnerGroupNode = pGroupNode;
	m_vec_values.clear();
	std::copy(vec_values.begin(), vec_values.end(), std::back_inserter(m_vec_values));
}

CJsonGroupNode::CJsonGroupRow::~CJsonGroupRow()
{
}

int CJsonGroupNode::CJsonGroupRow::GetSize() const
{
	return m_vec_values.size();
}
