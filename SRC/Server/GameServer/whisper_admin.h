#pragma once

extern char M2_WHISPER_LANG[512 + 1];
extern int M2_WHISPER_COLOR;

class CWhisperAdmin : public singleton<CWhisperAdmin>
{
	public:
		CWhisperAdmin();
		~CWhisperAdmin();

		bool IsEuropa(std::string lang);
		void SaveConfig(const char* c_pszLang, int color);
		bool file_is_empty(std::ifstream& file);
		
		std::string GetLang();
		int GetColor();

		void SaveLog(LPCHARACTER ch, const char* c_pszText, const char* c_pszLang, int color);
		
		void SendWhisper(const char* c_pszText);		
		int Whisper(LPDESC d, const char * c_pData, size_t uiBytes);
		void Manager(LPCHARACTER ch, const char* c_pData);	
};