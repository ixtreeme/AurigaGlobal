#include "stdafx.h"
#include "json_group_loader.h"
#include "JsonFile.h"

#include <common/stl.h>

#include <nlohmann/json.hpp>

namespace
{
	void ParseTokens(const nlohmann::json& doc, CJsonGroupLoader::TGroupNode* node)
	{
		if (!doc.contains("tokens") || !doc.at("tokens").is_object())
			return;

		for (const auto& item : doc.at("tokens").items())
		{
			std::string key = item.key();
			stl_lowers(key);

			TTokenVector tokens;

			if (item.value().is_array())
			{
				for (const auto& value : item.value())
					tokens.push_back(JsonFile::ToToken(value));
			}
			else
			{
				tokens.push_back(JsonFile::ToToken(item.value()));
			}

			node->LocalTokenVectorMap.insert(std::make_pair(key, tokens));
		}
	}

	void ParseNode(const nlohmann::json& doc, CJsonGroupLoader::TGroupNode* node)
	{
		ParseTokens(doc, node);

		if (!doc.contains("groups") || !doc.at("groups").is_array())
			return;

		for (const auto& child : doc.at("groups"))
		{
			auto* childNode = new CJsonGroupLoader::TGroupNode;
			childNode->pParentNode = node;
			childNode->strGroupName = child.value("name", std::string());
			stl_lowers(childNode->strGroupName);

			ParseNode(child, childNode);

			node->ChildNodeVector.push_back(childNode);
		}
	}

	void FreeNode(CJsonGroupLoader::TGroupNode* node)
	{
		for (CJsonGroupLoader::TGroupNode* child : node->ChildNodeVector)
			FreeNode(child);

		delete node;
	}
}

CJsonGroupLoader::CJsonGroupLoader()
	: m_pcurNode(nullptr)
{
	m_globalNode.pParentNode = nullptr;
}

CJsonGroupLoader::~CJsonGroupLoader()
{
	for (TGroupNode* child : m_globalNode.ChildNodeVector)
		FreeNode(child);

	m_globalNode.ChildNodeVector.clear();
}

const char* CJsonGroupLoader::GetFileName()
{
	return m_strFileName.c_str();
}

bool CJsonGroupLoader::Load(const char* c_szFileName)
{
	m_strFileName = c_szFileName ? c_szFileName : "";

	nlohmann::json doc;

	if (!JsonFile::Load(m_strFileName, doc))
		return false;

	for (TGroupNode* child : m_globalNode.ChildNodeVector)
		FreeNode(child);

	m_globalNode.ChildNodeVector.clear();
	m_globalNode.LocalTokenVectorMap.clear();
	m_globalNode.strGroupName.clear();
	m_globalNode.pParentNode = nullptr;

	ParseNode(doc, &m_globalNode);

	SetTop();
	return true;
}

void CJsonGroupLoader::SetTop()
{
	m_pcurNode = &m_globalNode;
}

uint32_t CJsonGroupLoader::GetChildNodeCount()
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return 0;
	}

	return (uint32_t)m_pcurNode->ChildNodeVector.size();
}

bool CJsonGroupLoader::SetChildNode(const char* c_szKey)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	for (uint32_t i = 0; i < m_pcurNode->ChildNodeVector.size(); ++i)
	{
		TGroupNode* pGroupNode = m_pcurNode->ChildNodeVector[i];

		if (0 == pGroupNode->strGroupName.compare(c_szKey))
		{
			m_pcurNode = pGroupNode;
			return true;
		}
	}

	return false;
}

bool CJsonGroupLoader::SetChildNode(std::string_view c_rstrKeyHead, uint32_t dwIndex)
{
	char szKey[32];
	snprintf(szKey, sizeof(szKey), "%.*s%02u", static_cast<int>(c_rstrKeyHead.size()), c_rstrKeyHead.data(), (unsigned int)dwIndex);
	return SetChildNode(szKey);
}

bool CJsonGroupLoader::SetChildNode(uint32_t dwIndex)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	if (dwIndex >= m_pcurNode->ChildNodeVector.size())
	{
		assert(!"Node index to set is too large to access!");
		return false;
	}

	m_pcurNode = m_pcurNode->ChildNodeVector[dwIndex];

	return true;
}

bool CJsonGroupLoader::SetParentNode()
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	if (nullptr == m_pcurNode->pParentNode)
	{
		assert(!"Current group node is already top!");
		return false;
	}

	m_pcurNode = m_pcurNode->pParentNode;

	return true;
}

bool CJsonGroupLoader::GetCurrentNodeName(std::string* pstrName)
{
	if (!m_pcurNode)
		return false;

	if (nullptr == m_pcurNode->pParentNode)
		return false;

	*pstrName = m_pcurNode->strGroupName;
	return true;
}

bool CJsonGroupLoader::IsToken(std::string_view c_rstrKey)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	return m_pcurNode->LocalTokenVectorMap.end() != m_pcurNode->LocalTokenVectorMap.find(c_rstrKey);
}

bool CJsonGroupLoader::GetTokenVector(std::string_view c_rstrKey, TTokenVector** ppTokenVector)
{
	if (!m_pcurNode)
	{
		assert(!"Node to access has not set!");
		return false;
	}

	TTokenVectorMap::iterator it = m_pcurNode->LocalTokenVectorMap.find(c_rstrKey);

	if (m_pcurNode->LocalTokenVectorMap.end() == it)
		return false;

	*ppTokenVector = &it->second;
	return true;
}

bool CJsonGroupLoader::GetTokenBoolean(std::string_view c_rstrKey, bool* pData)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->empty())
		return false;

	bool out = 0;
	str_to_number(out, pTokenVector->at(0).c_str());
	*pData = out;
	return true;
}

bool CJsonGroupLoader::GetTokenByte(std::string_view c_rstrKey, uint8_t* pData)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->empty())
		return false;

	uint8_t out = 0;
	str_to_number(out, pTokenVector->at(0).c_str());
	*pData = out;
	return true;
}

bool CJsonGroupLoader::GetTokenWord(std::string_view c_rstrKey, uint16_t* pData)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->empty())
		return false;

	uint16_t out = 0;
	str_to_number(out, pTokenVector->at(0).c_str());
	*pData = out;
	return true;
}

bool CJsonGroupLoader::GetTokenInteger(std::string_view c_rstrKey, int* pData)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->empty())
		return false;

	int out = 0;
	str_to_number(out, pTokenVector->at(0).c_str());
	*pData = out;
	return true;
}

bool CJsonGroupLoader::GetTokenDoubleWord(std::string_view c_rstrKey, uint32_t* pData)
{
	return GetTokenInteger(c_rstrKey, (int*)pData);
}

bool CJsonGroupLoader::GetTokenFloat(std::string_view c_rstrKey, float* pData)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->empty())
		return false;

	try
	{
		*pData = std::stof(pTokenVector->at(0));
	}
	catch (const std::exception&)
	{
		return false;
	}

	return true;
}

bool CJsonGroupLoader::GetTokenVector2(std::string_view c_rstrKey, D3DXVECTOR2* pVector2)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 2)
		return false;

	pVector2->x = std::stof(pTokenVector->at(0));
	pVector2->y = std::stof(pTokenVector->at(1));
	return true;
}

bool CJsonGroupLoader::GetTokenVector3(std::string_view c_rstrKey, D3DXVECTOR3* pVector3)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 3)
		return false;

	pVector3->x = std::stof(pTokenVector->at(0));
	pVector3->y = std::stof(pTokenVector->at(1));
	pVector3->z = std::stof(pTokenVector->at(2));
	return true;
}

bool CJsonGroupLoader::GetTokenVector4(std::string_view c_rstrKey, D3DXVECTOR4* pVector4)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 4)
		return false;

	pVector4->x = std::stof(pTokenVector->at(0));
	pVector4->y = std::stof(pTokenVector->at(1));
	pVector4->z = std::stof(pTokenVector->at(2));
	pVector4->w = std::stof(pTokenVector->at(3));
	return true;
}

bool CJsonGroupLoader::GetTokenPosition(std::string_view c_rstrKey, D3DXVECTOR3* pVector)
{
	return GetTokenVector3(c_rstrKey, pVector);
}

bool CJsonGroupLoader::GetTokenQuaternion(std::string_view c_rstrKey, D3DXQUATERNION* pQ)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 4)
		return false;

	pQ->x = std::stof(pTokenVector->at(0));
	pQ->y = std::stof(pTokenVector->at(1));
	pQ->z = std::stof(pTokenVector->at(2));
	pQ->w = std::stof(pTokenVector->at(3));
	return true;
}

bool CJsonGroupLoader::GetTokenDirection(std::string_view c_rstrKey, D3DVECTOR* pVector)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 3)
		return false;

	pVector->x = std::stof(pTokenVector->at(0));
	pVector->y = std::stof(pTokenVector->at(1));
	pVector->z = std::stof(pTokenVector->at(2));
	return true;
}

bool CJsonGroupLoader::GetTokenColor(std::string_view c_rstrKey, D3DXCOLOR* pColor)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 4)
		return false;

	pColor->r = std::stof(pTokenVector->at(0));
	pColor->g = std::stof(pTokenVector->at(1));
	pColor->b = std::stof(pTokenVector->at(2));
	pColor->a = std::stof(pTokenVector->at(3));
	return true;
}

bool CJsonGroupLoader::GetTokenColor(std::string_view c_rstrKey, D3DCOLORVALUE* pColor)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->size() != 4)
		return false;

	pColor->r = std::stof(pTokenVector->at(0));
	pColor->g = std::stof(pTokenVector->at(1));
	pColor->b = std::stof(pTokenVector->at(2));
	pColor->a = std::stof(pTokenVector->at(3));
	return true;
}

bool CJsonGroupLoader::GetTokenString(std::string_view c_rstrKey, std::string* pString)
{
	TTokenVector* pTokenVector;

	if (!GetTokenVector(c_rstrKey, &pTokenVector) || pTokenVector->empty())
		return false;

	*pString = pTokenVector->at(0);
	return true;
}
