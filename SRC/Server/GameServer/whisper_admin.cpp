#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "desc.h"
#include "utils.h"
#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "affect.h"
#include "start_position.h"
#include "p2p.h"
#include "db.h"
#include "dungeon.h"
#include <string>
#include "whisper_admin.h"
#include <boost/algorithm/string/replace.hpp>
#include "desc_manager.h"
#include "buffer_manager.h"
#include "config.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include <fstream>
#include <algorithm>
#include <iostream>

CWhisperAdmin::CWhisperAdmin()
{}
CWhisperAdmin::~CWhisperAdmin()
{}




bool CWhisperAdmin::file_is_empty(std::ifstream& file)
{
	if(file.peek() == file.eof() ||!file||!file.is_open())
		return true;
	return false;
	//file.peek()
	//if(!file || !file.is_open())
		//return true;
	//else
		//return false;
}

std::string CWhisperAdmin::GetLang()
{
	std::string lang;
	char szFileName[256];
	snprintf(szFileName, sizeof(szFileName), "%s/whisper.txt", LocaleService_GetBasePath().c_str());

	std::ifstream file(szFileName);

	if(CWhisperAdmin::instance().file_is_empty(file))
		return "";

	file >> lang;
	file.close();

	return lang.c_str();
}

int CWhisperAdmin::GetColor()
{
	std::string f;
	int color;

	char szFileName[256];
	snprintf(szFileName, sizeof(szFileName), "%s/whisper.txt", LocaleService_GetBasePath().c_str());

	std::ifstream file(szFileName);

	if(CWhisperAdmin::instance().file_is_empty(file))
		return false;

	file >> f;
	file >> color;
	file.close();

	return color;
}

/*
void CWhisperAdmin::SaveConfig(const char* c_pszLang, int color)
{
	char szFileName[256];
	snprintf(szFileName, sizeof(szFileName), "%s/whisper.txt", LocaleService_GetBasePath().c_str());

	FILE *fp = 0;
	fp = fopen(szFileName, "a");

	if (0 == fp)
		return;

	std::ifstream fFile(szFileName);
	if (!fFile.is_open())
		return;

	FILE *f = 0;
	f = fopen(szFileName, "w");

	fprintf(f, "");
	fclose(f);

	fprintf(fp, "%s\n", c_pszLang);
	fprintf(fp, "%d", color);

	fclose(fp);
	fFile.close();
}
*/

void CWhisperAdmin::SaveConfig(const char* c_pszLang, int color)
{
	char szFileName[256];
	snprintf(szFileName, sizeof(szFileName), "%s/whisper.txt", LocaleService_GetBasePath().c_str());

	FILE *fp = nullptr;
	fp = fopen(szFileName, "w");

	if (nullptr == fp)
		return;

	fprintf(fp, "%s\n", c_pszLang);
	fprintf(fp, "%d", color);

	fclose(fp);
}

struct whisper_packet_func
{
	const char * c_pszText;

	whisper_packet_func(const char * text) : c_pszText(text)
	{}

	void operator () (LPDESC d)
	{
		if (!d->GetCharacter())
			return;

		if (!d->GetCharacter()->GetLang().compare(CWhisperAdmin::instance().GetLang()) || CWhisperAdmin::instance().IsEuropa(CWhisperAdmin::instance().GetLang()))
		{
			ecs::ChatSystem::Send(AIHelpers::EcsOf(d->GetCharacter()), CHAT_TYPE_COMMAND, "OnRecvWhisperAdminSystem [SYSTEM] %s %d", c_pszText, CWhisperAdmin::instance().GetColor());
		}
	}
};

bool CWhisperAdmin::IsEuropa(std::string lang)
{
	return (!lang.compare("gl")) ? true : false;
}

void CWhisperAdmin::SendWhisper(const char * c_pszText)
{
	const DESC_MANAGER::DESC_SET & f = DESC_MANAGER::instance().GetClientSet();
	std::for_each(f.begin(), f.end(), whisper_packet_func(c_pszText));
}

int CWhisperAdmin::Whisper(LPDESC d, const char * c_pData, size_t uiBytes)
{
	TPacketGGWhisperSystem * p = (TPacketGGWhisperSystem *) c_pData;

	if (uiBytes < sizeof(TPacketGGWhisperSystem) + p->lSize)
		return -1;

	if (p->lSize < 0)
	{
		LOG_ERROR("invalid packet length {}", p->lSize);
		d->SetPhase(PHASE_CLOSE);
		return -1;
	}

	char szBuf[CHAT_MAX_LEN + 1];
	strlcpy(szBuf, c_pData + sizeof(TPacketGGWhisperSystem), MIN(p->lSize + 1, sizeof(szBuf)));
	CWhisperAdmin::instance().SendWhisper(szBuf);
	return (p->lSize);
}

void CWhisperAdmin::SaveLog(LPCHARACTER ch, const char* c_pszText, const char* c_pszLang, int color)
{
	char szQuery[QUERY_MAX_LEN + 1];
	snprintf(szQuery, sizeof(szQuery), "INSERT INTO whisper_system_message (who, date, text, lang, color) VALUES('%s', NOW(), '%s', '%s', '%d')", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), c_pszText, c_pszLang, color);
	std::unique_ptr<SQLMsg> msg(DBManager::Instance().DirectQuery(szQuery));
}

void CWhisperAdmin::Manager(LPCHARACTER ch, const char* c_pData)
{
	TPacketCGWhisperAdmin * f = (TPacketCGWhisperAdmin *)c_pData;

	if (!ch)
		return;

	if (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) != GM_IMPLEMENTOR)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 774, "");
#endif
		return;
	}

	if (strlen(f->szText) <= 0 || strlen(f->szLang) <= 0 || f->color < 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 775, "");
#endif
		return;
	}

	char szEscaped[CHAT_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szEscaped, sizeof(szEscaped), f->szText, strlen(f->szText));

	std::string textLine = szEscaped;
	boost::algorithm::replace_all(textLine, "#", " ");

	CWhisperAdmin::instance().SaveConfig(f->szLang, f->color);
	CWhisperAdmin::instance().SaveLog(ch, textLine.c_str(), f->szLang, f->color);

	TPacketGGWhisperSystem p;
	p.bHeader = HEADER_GG_WHISPER_SYSTEM;
	p.lSize = strlen(f->szText) + 1;

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(p));
	buf.write(f->szText, p.lSize);

	P2P_MANAGER::instance().Send(buf.read_peek(), buf.size());
	CWhisperAdmin::instance().SendWhisper(f->szText);
}


