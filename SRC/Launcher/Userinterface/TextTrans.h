#pragma once

#ifdef TEXTS_IMPROVEMENT
class CTextTrans
{
	public:
		CTextTrans(void);
		virtual ~CTextTrans(void);

		int Load(const char* localePath);

		void AddStringText(uint32_t idx, const std::string& text);

		std::string GetStringText(uint32_t idx);

	private:
		std::map<uint32_t, std::string> m_Quest, m_String;
};
#endif
