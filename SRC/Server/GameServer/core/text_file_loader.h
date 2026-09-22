#pragma once

#include <common/d3dtype.h>
#include <common/pool.h>
#include "file_loader.h"
#include <string_view>

typedef std::map<std::string, TTokenVector, std::less<>> TTokenVectorMap;

class CTextFileLoader
{
public:
    typedef struct SGroupNode
    {
        std::string strGroupName;

        TTokenVectorMap LocalTokenVectorMap;

        SGroupNode* pParentNode;
        std::vector<SGroupNode*> ChildNodeVector;
    } TGroupNode;

    typedef std::vector<TGroupNode*> TGroupNodeVector;

public:
    static void DestroySystem();

public:
    CTextFileLoader();
    virtual ~CTextFileLoader();

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
    bool LoadGroup(TGroupNode* pGroupNode);

protected:
    std::string                 m_strFileName;
    uint32_t                    m_dwcurLineIndex;
    const void* mc_pData;

    CMemoryTextFileLoader       m_fileLoader;

    TGroupNode                  m_globalNode;
    TGroupNode* m_pcurNode;

private:
    static CDynamicPool<TGroupNode> ms_groupNodePool;
};
