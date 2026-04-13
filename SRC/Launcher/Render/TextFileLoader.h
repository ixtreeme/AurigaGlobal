#pragma once

#include "../Base/FileLoader.h"
#include "../Base/MappedFile.h"
#include "../Render/Util.h"
#include "../Render/Pool.h"

class CTextFileLoader
{
public:
	typedef struct SGroupNode
	{
		static uint32_t GenNameKey(const char* c_szGroupName, UINT uGroupNameLen);

		void SetGroupName(const std::string& c_rstGroupName);
		bool IsGroupNameKey(uint32_t dwGroupNameKey);

		const std::string& GetGroupName();

		CTokenVector* GetTokenVector(const std::string& c_rstGroupName);
		bool IsExistTokenVector(const std::string& c_rstGroupName);
		void InsertTokenVector(const std::string& c_rstGroupName, const CTokenVector& c_rkVct_stToken);

		uint32_t m_dwGroupNameKey;
		std::string m_strGroupName;

		std::map<uint32_t, CTokenVector> m_kMap_dwKey_kVct_stToken;

		SGroupNode* pParentNode;
		std::vector<SGroupNode*> ChildNodeVector;

		static SGroupNode* New();
		static void Delete(SGroupNode* pkNode);

		static void DestroySystem();
		static CDynamicPool<SGroupNode>	ms_kPool;
	} TGroupNode;

	typedef std::vector<TGroupNode*> TGroupNodeVector;

	class CGotoChild
	{
	public:
		CGotoChild(CTextFileLoader* pOwner, const char* c_szKey) : m_pOwner(pOwner)
		{
			m_pOwner->SetChildNode(c_szKey);
		}
		CGotoChild(CTextFileLoader* pOwner, uint32_t dwIndex) : m_pOwner(pOwner)
		{
			m_pOwner->SetChildNode(dwIndex);
		}
		~CGotoChild()
		{
			m_pOwner->SetParentNode();
		}

		CTextFileLoader* m_pOwner;
	};

public:
	static void DestroySystem();

	static void SetCacheMode();

	static CTextFileLoader* Cache(const char* c_szFileName);

public:
	CTextFileLoader();
	virtual ~CTextFileLoader();

	void Destroy();

	bool Load(const char* c_szFileName);
	const char* GetFileName();

	bool IsEmpty();

	void SetTop();
	uint32_t GetChildNodeCount();
	bool SetChildNode(const char* c_szKey);
	bool SetChildNode(const std::string& c_rstrKeyHead, uint32_t dwIndex);
	bool SetChildNode(uint32_t dwIndex);
	bool SetParentNode();
	bool GetCurrentNodeName(std::string* pstrName);

	bool IsToken(const std::string& c_rstrKey);
	bool GetTokenVector(const std::string& c_rstrKey, CTokenVector** ppTokenVector);
	bool GetTokenBoolean(const std::string& c_rstrKey, bool* pData);
	bool GetTokenByte(const std::string& c_rstrKey, uint8_t* pData);
	bool GetTokenWord(const std::string& c_rstrKey, WORD* pData);
	bool GetTokenInteger(const std::string& c_rstrKey, int* pData);
	bool GetTokenDoubleWord(const std::string& c_rstrKey, uint32_t* pData);
	bool GetTokenFloat(const std::string& c_rstrKey, float* pData);
	bool GetTokenVector2(const std::string& c_rstrKey, D3DXVECTOR2* pVector2);
	bool GetTokenVector3(const std::string& c_rstrKey, D3DXVECTOR3* pVector3);
	bool GetTokenVector4(const std::string& c_rstrKey, D3DXVECTOR4* pVector4);

	bool GetTokenPosition(const std::string& c_rstrKey, D3DXVECTOR3* pVector);
	bool GetTokenQuaternion(const std::string& c_rstrKey, D3DXQUATERNION* pQ);
	bool GetTokenDirection(const std::string& c_rstrKey, D3DVECTOR* pVector);
	bool GetTokenColor(const std::string& c_rstrKey, D3DXCOLOR* pColor);
	bool GetTokenColor(const std::string& c_rstrKey, D3DCOLORVALUE* pColor);
	bool GetTokenString(const std::string& c_rstrKey, std::string* pString);

protected:
	void __DestroyGroupNodeVector();

	bool LoadGroup(TGroupNode* pGroupNode);

protected:
	std::string					m_strFileName;

	std::unique_ptr<char[]>m_acBufData;
	uint32_t						m_dwBufSize;
	uint32_t						m_dwBufCapacity;

	uint32_t						m_dwcurLineIndex;

	CMemoryTextFileLoader		m_textFileLoader;

	TGroupNode					m_GlobalNode;
	TGroupNode* m_pcurNode;

	std::vector<SGroupNode*>	m_kVct_pkNode;

protected:
	static std::map<DWORD, CTextFileLoader*> ms_kMap_dwNameKey_pkTextFileLoader;
	static bool ms_isCacheMode;
};
