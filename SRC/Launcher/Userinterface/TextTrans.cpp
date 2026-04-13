#include "StdAfx.h"
#ifdef TEXTS_IMPROVEMENT
#include <map>
#include <string>

//using namespace std;

#include "TextTrans.h"
#include "../Pack/EterPackManager.h"

int CTextTrans::Load(const char* localePath) {
	m_String.clear();

	char szStringList[256];
	_snprintf(szStringList, sizeof(szStringList), "%s/locale_string.txt", localePath);

	CMappedFile file;
	const VOID* pvData;
	if (!CEterPackManager::Instance().Get(file, szStringList, &pvData)) {
		return -2;
	}

	CMemoryTextFileLoader kMemTextFileLoader;
	kMemTextFileLoader.Bind(file.Size(), pvData);

	CTokenVector kTokenVector;
	for (uint32_t i = 0; i < kMemTextFileLoader.GetLineCount(); ++i) {
		if (!kMemTextFileLoader.SplitLineByTab(i, &kTokenVector))
			continue;

		while (kTokenVector.size() < 2) {
			kTokenVector.push_back("");
		}

		uint32_t idx = atoi(kTokenVector[0].c_str());
		const std::string& text = kTokenVector[1].c_str();
		AddStringText(idx, text);
	}
	
	return 1;
}

void CTextTrans::AddStringText(uint32_t idx, const std::string& text) {
	if (text.empty()) {
		return;
	}

	m_String.insert(std::pair<uint32_t, std::string> (idx, text));
#ifdef _DEBUG
	Tracef("CTextTrans<m_String>: %d : %s, new size(%d).\n", idx, text.c_str(), m_String.size());
#endif
}

std::string CTextTrans::GetStringText(uint32_t idx) {
	auto it = m_String.find(idx);
	if (it != m_String.end()) {
		return it->second;
	}

	return "";
}

CTextTrans::CTextTrans(void) {
}

CTextTrans::~CTextTrans(void)
{
	m_String.clear();
}
#endif
