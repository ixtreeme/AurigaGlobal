#pragma once

#include <common/d3dtype.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

typedef std::vector<std::string> TTokenVector;
typedef std::map<std::string, TTokenVector, std::less<>> TTokenVectorMap;

// Group/token tree loaded from JSON (replaces CTextFileLoader). JSON shape:
//
//   {
//     "tokens": { "key": "value" | ["v1", "v2"] },
//     "groups": [
//       { "name": "grp", "tokens": { ... }, "groups": [ ... ] }
//     ]
//   }
//
// Keys and group names are lowercased on load, values are kept verbatim
// (same behaviour as the old txt loader). The generated files come from
// SRC/Tools/txt2json/txt2json_group.py.
class CJsonGroupLoader
{
public:
	typedef struct TGroupNode
	{
		std::string strGroupName;

		TTokenVectorMap LocalTokenVectorMap;

		TGroupNode* pParentNode;
		std::vector<TGroupNode*> ChildNodeVector;
	} TGroupNode;

public:
	CJsonGroupLoader();
	virtual ~CJsonGroupLoader();

	CJsonGroupLoader(const CJsonGroupLoader&) = delete;
	CJsonGroupLoader& operator=(const CJsonGroupLoader&) = delete;

	bool Load(const char* c_szFileName);
	const char* GetFileName();

	void SetTop();
	uint32_t GetChildNodeCount();

	bool SetChildNode(const char* c_szKey);
	bool SetChildNode(std::string_view c_rstrKeyHead, uint32_t dwIndex);
	bool SetChildNode(uint32_t dwIndex);
	bool SetParentNode();
	bool GetCurrentNodeName(std::string* pstrName);

	bool IsToken(std::string_view c_rstrKey);
	bool GetTokenVector(std::string_view c_rstrKey, TTokenVector** ppTokenVector);
	bool GetTokenBoolean(std::string_view c_rstrKey, bool* pData);
	bool GetTokenByte(std::string_view c_rstrKey, uint8_t* pData);
	bool GetTokenWord(std::string_view c_rstrKey, uint16_t* pData);
	bool GetTokenInteger(std::string_view c_rstrKey, int* pData);
	bool GetTokenDoubleWord(std::string_view c_rstrKey, uint32_t* pData);
	bool GetTokenFloat(std::string_view c_rstrKey, float* pData);

	bool GetTokenVector2(std::string_view c_rstrKey, D3DXVECTOR2* pVector2);
	bool GetTokenVector3(std::string_view c_rstrKey, D3DXVECTOR3* pVector3);
	bool GetTokenVector4(std::string_view c_rstrKey, D3DXVECTOR4* pVector4);

	bool GetTokenPosition(std::string_view c_rstrKey, D3DXVECTOR3* pVector);
	bool GetTokenQuaternion(std::string_view c_rstrKey, D3DXQUATERNION* pQ);
	bool GetTokenDirection(std::string_view c_rstrKey, D3DVECTOR* pVector);
	bool GetTokenColor(std::string_view c_rstrKey, D3DXCOLOR* pColor);
	bool GetTokenColor(std::string_view c_rstrKey, D3DCOLORVALUE* pColor);
	bool GetTokenString(std::string_view c_rstrKey, std::string* pString);

protected:
	std::string	m_strFileName;

	TGroupNode	m_globalNode;
	TGroupNode*	m_pcurNode;
};
