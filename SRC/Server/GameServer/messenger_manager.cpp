#include "stdafx.h"
#include "constants.h"
#include "gm.h"
#include "messenger_manager.h"
#include "buffer_manager.h"
#include "desc_client.h"
#include "log.h"
#include "config.h"
#include "p2p.h"
#include "crc32.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "questmanager.h"

// @fixme142 BEGIN
static char	__account[CHARACTER_NAME_MAX_LEN*2+1];
static char	__companion[CHARACTER_NAME_MAX_LEN*2+1];
// @fixme142 END

MessengerManager::MessengerManager()
{
}

MessengerManager::~MessengerManager()
{
}

void MessengerManager::Initialize()
{
}

void MessengerManager::Destroy()
{
}

void MessengerManager::P2PLogin(MessengerManager::keyA account)
{
	Login(account);
}

void MessengerManager::P2PLogout(MessengerManager::keyA account)
{
	Logout(account);
}

void MessengerManager::Login(MessengerManager::keyA account)
{
	if (m_set_loginAccount.find(account) != m_set_loginAccount.end())
		return;

	// @fixme142 BEGIN
	DBManager::instance().EscapeString(__account, sizeof(__account), account.c_str(), account.size());
	if (account.compare(__account))
		return;
	// @fixme142 END

	DBManager::instance().FuncQuery(std::bind(&MessengerManager::LoadList, this,std::placeholders::_1), "SELECT account, companion FROM messenger_list%s WHERE account='%s'", get_table_postfix(), __account);
#ifdef ENABLE_MESSENGER_TEAM
	DBManager::instance().FuncQuery(std::bind(&MessengerManager::LoadTeamList, this, std::placeholders::_1), "SELECT '%s',mName FROM common.gmlist WHERE mName!='%s'", account.c_str(), account.c_str());
#endif
#ifdef ENABLE_MESSENGER_HELPER
	DBManager::instance().FuncQuery(std::bind(&MessengerManager::LoadHelperList, this, std::placeholders::_1), "SELECT '%s',mName FROM common.helperlist WHERE mName!='%s'", account.c_str(), account.c_str());
#endif
	m_set_loginAccount.insert(account);
}

void MessengerManager::LoadList(SQLMsg * msg)
{
	if (nullptr == msg)
		return;

	if (nullptr == msg->Get())
		return;

	if (msg->Get()->uiNumRows == 0)
		return;

	std::string account;

	sys_log(1, "Messenger::LoadList");

	for (uint64_t i = 0; i < msg->Get()->uiNumRows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		if (row[0] && row[1])
		{
			if (account.length() == 0)
				account = row[0];

			m_Relation[row[0]].insert(row[1]);
			m_InverseRelation[row[1]].insert(row[0]);
		}
	}

	SendList(account);

	std::set<MessengerManager::keyT>::iterator it;

	for (it = m_InverseRelation[account].begin(); it != m_InverseRelation[account].end(); ++it)
		SendLogin(*it, account);
}

void MessengerManager::Logout(MessengerManager::keyA account)
{
	if (m_set_loginAccount.find(account) == m_set_loginAccount.end())
		return;

	m_set_loginAccount.erase(account);

	std::set<MessengerManager::keyT>::iterator it;

	for (it = m_InverseRelation[account].begin(); it != m_InverseRelation[account].end(); ++it)
	{
		SendLogout(*it, account);
	}

	std::map<keyT, std::set<keyT> >::iterator it2 = m_Relation.begin();

	while (it2 != m_Relation.end())
	{
		it2->second.erase(account);
		++it2;
	}

	m_Relation.erase(account);
#ifdef ENABLE_MESSENGER_TEAM
	std::set<MessengerManager::keyT>::iterator it5;
	
	for (it5 = m_InverseTeamRelation[account].begin(); it5 != m_InverseTeamRelation[account].end(); ++it5)
	{
		SendTeamLogout(*it5, account);
	}

	std::map<keyT, std::set<keyT> >::iterator it6 = m_TeamRelation.begin();

	while (it6 != m_TeamRelation.end())
	{
		it6->second.erase(account);
		++it6;
	}
	m_TeamRelation.erase(account);
#endif

#ifdef ENABLE_MESSENGER_HELPER
	std::set<MessengerManager::keyT>::iterator ithelpersinv;
	for (ithelpersinv = m_InverseHelperRelation[account].begin(); ithelpersinv != m_InverseHelperRelation[account].end(); ++ithelpersinv)
	{
		SendHelperLogout(*ithelpersinv, account);
	}

	std::map<keyT, std::set<keyT> >::iterator ithelpersrel = m_HelperRelation.begin();
	while (ithelpersrel != m_HelperRelation.end())
	{
		ithelpersrel->second.erase(account);
		++ithelpersrel;
	}

	m_HelperRelation.erase(account);
#endif
}

void MessengerManager::RequestToAdd(LPCHARACTER ch, LPCHARACTER target)
{
	if (!ecs::IsPC(ch) || !target->IsPC())
		return;

	if (quest::CQuestManager::instance().GetPCForce(ecs::GetPlayerID(ch))->IsRunning() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 607, "");
#endif
		return;
	}

	if (quest::CQuestManager::instance().GetPCForce(target->GetPlayerID())->IsRunning() == true)
		return;

	uint32_t dw1 = GetCRC32(ecs::GetName(ch), strlen(ecs::GetName(ch)));
	uint32_t dw2 = GetCRC32(target->GetName(), strlen(target->GetName()));

	char buf[64];
	snprintf(buf, sizeof(buf), "%u:%u", dw1, dw2);
	uint32_t dwComplex = GetCRC32(buf, strlen(buf));

	m_set_requestToAdd.insert(dwComplex);

	ecs::ChatSystem::Send(AIHelpers::EcsOf(target), CHAT_TYPE_COMMAND, "messenger_auth %s", ecs::GetName(ch));
}

// @fixme130 void -> bool
bool MessengerManager::AuthToAdd(MessengerManager::keyA account, MessengerManager::keyA companion, bool bDeny)
{
	uint32_t dw1 = GetCRC32(companion.c_str(), companion.length());
	uint32_t dw2 = GetCRC32(account.c_str(), account.length());

	char buf[64];
	snprintf(buf, sizeof(buf), "%u:%u", dw1, dw2);
	uint32_t dwComplex = GetCRC32(buf, strlen(buf));

	if (m_set_requestToAdd.find(dwComplex) == m_set_requestToAdd.end())
	{
		sys_log(0, "MessengerManager::AuthToAdd : request not exist %s -> %s", companion.c_str(), account.c_str());
		return false;
	}

	m_set_requestToAdd.erase(dwComplex);

	if (!bDeny)
	{
		AddToList(companion, account);
		AddToList(account, companion);
	}
	return true;
}

void MessengerManager::__AddToList(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	m_Relation[account].insert(companion);
	m_InverseRelation[companion].insert(account);

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
#ifdef TEXTS_IMPROVEMENT
	if (ch) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 183, "%s", companion.c_str());
	}
#endif
	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(companion.c_str());

	if (tch)
		SendLogin(account, companion);
	else
		SendLogout(account, companion);
}

void MessengerManager::AddToList(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	if (companion.size() == 0)
		return;

	if (m_Relation[account].find(companion) != m_Relation[account].end())
		return;

	// @fixme142 BEGIN
	DBManager::instance().EscapeString(__account, sizeof(__account), account.c_str(), account.size());
	DBManager::instance().EscapeString(__companion, sizeof(__companion), companion.c_str(), companion.size());
	if (account.compare(__account) || companion.compare(__companion))
		return;
	// @fixme142 END

	sys_log(0, "Messenger Add %s %s", account.c_str(), companion.c_str());
	DBManager::instance().Query("INSERT INTO messenger_list%s VALUES ('%s', '%s')",
			get_table_postfix(), __account, __companion);

	__AddToList(account, companion);

	TPacketGGMessenger p2ppck;

	p2ppck.bHeader = HEADER_GG_MESSENGER_ADD;
	strlcpy(p2ppck.szAccount, account.c_str(), sizeof(p2ppck.szAccount));
	strlcpy(p2ppck.szCompanion, companion.c_str(), sizeof(p2ppck.szCompanion));
	P2P_MANAGER::instance().Send(&p2ppck, sizeof(TPacketGGMessenger));
}

#ifdef ENABLE_MESSENGER_TEAM
void MessengerManager::LoadTeamList(SQLMsg * msg)
{
	if (nullptr == msg || nullptr == msg->Get() ||msg->Get()->uiNumRows == 0)
		return;
	
	std::string account;

	for (uint64_t i = 0; i < msg->Get()->uiNumRows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		if (row[0] && row[1])
		{
			if (account.length() == 0)
				account = row[0];

			m_TeamRelation[row[0]].insert(row[1]);
			m_InverseTeamRelation[row[1]].insert(row[0]);
		}
	}

	SendTeamList(account);

	std::set<MessengerManager::keyT>::iterator it;

	for (it = m_InverseTeamRelation[account].begin(); it != m_InverseTeamRelation[account].end(); ++it)
		SendTeamLogin(*it, account);
}

void MessengerManager::SendTeamList(MessengerManager::keyA account)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());

	if (!ch)
		return;

	LPDESC d = ch->GetDesc();

	if (!d)
		return;

	TPacketGCMessenger pack;

	pack.header		= HEADER_GC_MESSENGER;
	pack.subheader	= MESSENGER_SUBHEADER_GC_TEAM_LIST;
	pack.size		= sizeof(TPacketGCMessenger);

	TPacketGCMessengerTeamListOffline pack_offline;
	TPacketGCMessengerTeamListOnline pack_online;

	TEMP_BUFFER buf(128 * 1024);

	auto it = m_TeamRelation[account].begin(), eit = m_TeamRelation[account].end();

	while (it != eit)
	{
		if (m_set_loginAccount.contains(*it))
		{
			pack_online.connected = 1;
			int iSize = it->size();
#ifdef ENABLE_MULTI_LANGUAGE
			bool bAuto = false;
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s'", it->c_str()));
			if (pMsg->Get()->uiNumRows != 0) {
				MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				int id = atoi(row[0]);
				std::unique_ptr<SQLMsg> pMsg2(DBManager::instance().DirectQuery("SELECT language FROM account.account WHERE id=%d", id));
				if (pMsg2->Get()->uiNumRows != 0) {
					row = mysql_fetch_row(pMsg2->Get()->pSQLResult);
					strncpy(pack_online.language, row[0], sizeof(pack_online.language));
					bAuto = true;
				}
			}
			
			if (!bAuto)
				strncpy(pack_online.language, "en", sizeof(pack_online.language));
			
			iSize += sizeof(pack_online.language);
#endif
			pack_online.length = iSize;

			buf.write(&pack_online, sizeof(TPacketGCMessengerTeamListOnline));
			buf.write(it->c_str(), iSize);
		}
		else
		{
			pack_offline.connected = 0;
			int iSize = it->size();
#ifdef ENABLE_MULTI_LANGUAGE
			bool bAuto = false;
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s'", it->c_str()));
			if (pMsg->Get()->uiNumRows != 0) {
				MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				int id = atoi(row[0]);
				std::unique_ptr<SQLMsg> pMsg2(DBManager::instance().DirectQuery("SELECT language FROM account.account WHERE id=%d", id));
				if (pMsg2->Get()->uiNumRows != 0) {
					row = mysql_fetch_row(pMsg2->Get()->pSQLResult);
					strncpy(pack_offline.language, row[0], sizeof(pack_offline.language));
					bAuto = true;
				}
			}
			
			if (!bAuto)
				strncpy(pack_offline.language, "en", sizeof(pack_offline.language));
			
			iSize += sizeof(pack_offline.language);
#endif
			pack_offline.length = iSize;

			buf.write(&pack_offline, sizeof(TPacketGCMessengerTeamListOffline));
			buf.write(it->c_str(), iSize);
		}

		++it;
	}

	pack.size += buf.size();

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->Packet(buf.read_peek(), buf.size());
}

void MessengerManager::SendTeamLogin(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	LPDESC d = ch ? ch->GetDesc() : nullptr;

	if (!d)
		return;

	if (!d->GetCharacter())
		return;

	uint8_t bLen = companion.size();

	TPacketGCMessenger pack;

	pack.header			= HEADER_GC_MESSENGER;
	pack.subheader		= MESSENGER_SUBHEADER_GC_TEAM_LOGIN;
	pack.size = sizeof(TPacketGCMessenger) + sizeof(uint8_t) + bLen;
	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->BufferedPacket(&bLen, sizeof(uint8_t));
	d->Packet(companion.c_str(), companion.size());
}

void MessengerManager::SendTeamLogout(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	if (!companion.size())
		return;

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	LPDESC d = ch ? ch->GetDesc() : nullptr;

	if (!d)
		return;

	uint8_t bLen = companion.size();

	TPacketGCMessenger pack;

	pack.header		= HEADER_GC_MESSENGER;
	pack.subheader	= MESSENGER_SUBHEADER_GC_TEAM_LOGOUT;
	pack.size		= sizeof(TPacketGCMessenger) + sizeof(uint8_t) + bLen;
	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->BufferedPacket(&bLen, sizeof(uint8_t));
	d->Packet(companion.c_str(), companion.size());
}
#endif

#ifdef ENABLE_MESSENGER_HELPER
void MessengerManager::LoadHelperList(SQLMsg * msg)
{
	if (nullptr == msg || nullptr == msg->Get() || msg->Get()->uiNumRows == 0)
		return;

	std::string account;
	for (uint32_t i = 0; i < msg->Get()->uiNumRows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
		if (row[0] && row[1])
		{
			if (account.length() == 0)
			{
				account = row[0];
			}

			m_HelperRelation[row[0]].insert(row[1]);
			m_InverseHelperRelation[row[1]].insert(row[0]);
		}
	}

	SendHelperList(account);

	std::set<MessengerManager::keyT>::iterator it;
	for (it = m_InverseHelperRelation[account].begin(); it != m_InverseHelperRelation[account].end(); ++it)
	{
		SendHelperLogin(*it, account);
	}
}

void MessengerManager::SendHelperList(MessengerManager::keyA account)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	if (!ch)
		return;

	LPDESC d = ch->GetDesc();
	if (!d)
		return;

	TPacketGCMessenger pack;
	pack.header = HEADER_GC_MESSENGER;
	pack.subheader = MESSENGER_SUBHEADER_GC_HELPER_LIST;
	pack.size = sizeof(TPacketGCMessenger);

	TPacketGCMessengerTeamListOffline pack_offline;
	TPacketGCMessengerTeamListOnline pack_online;

	TEMP_BUFFER buf(128 * 1024);

	auto it = m_HelperRelation[account].begin(), eit = m_HelperRelation[account].end();
	while (it != eit)
	{
		if (m_set_loginAccount.find(*it) != m_set_loginAccount.end())
		{
			pack_online.connected = 1;
			int iSize = it->size();
#ifdef ENABLE_MULTI_LANGUAGE
			bool bAuto = false;
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s'", it->c_str()));
			if (pMsg->Get()->uiNumRows != 0) {
				MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				int id = atoi(row[0]);
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT language FROM account.account WHERE id=%d", id));
				if (msg->Get()->uiNumRows != 0) {
					row = mysql_fetch_row(msg->Get()->pSQLResult);
					strncpy(pack_online.language, row[0], sizeof(pack_online.language));
					bAuto = true;
				}
			}

			if (!bAuto)
			{
				strncpy(pack_online.language, "en", sizeof(pack_online.language));
			}

			iSize += sizeof(pack_online.language);
#endif
			pack_online.length = iSize;

			buf.write(&pack_online, sizeof(TPacketGCMessengerTeamListOnline));
			buf.write(it->c_str(), iSize);
		}
		else
		{
			pack_offline.connected = 0;
			int iSize = it->size();
#ifdef ENABLE_MULTI_LANGUAGE
			bool bAuto = false;
			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("SELECT account_id FROM player.player WHERE name='%s'", it->c_str()));
			if (pMsg->Get()->uiNumRows != 0) {
				MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				int id = atoi(row[0]);
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT language FROM account.account WHERE id=%d", id));
				if (msg->Get()->uiNumRows != 0) {
					row = mysql_fetch_row(msg->Get()->pSQLResult);
					strncpy(pack_offline.language, row[0], sizeof(pack_offline.language));
					bAuto = true;
				}
			}

			if (!bAuto)
			{
				strncpy(pack_offline.language, "en", sizeof(pack_offline.language));
			}

			iSize += sizeof(pack_offline.language);
#endif
			pack_offline.length = iSize;

			buf.write(&pack_offline, sizeof(TPacketGCMessengerTeamListOffline));
			buf.write(it->c_str(), iSize);
		}

		++it;
	}

	pack.size += buf.size();

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->Packet(buf.read_peek(), buf.size());
}

void MessengerManager::SendHelperLogin(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	LPDESC d = ch ? ch->GetDesc() : nullptr;
	if (!d)
	{
		return;
	}

	if (!d->GetCharacter())
	{
		return;
	}

	uint8_t bLen = companion.size();

	TPacketGCMessenger pack;
	pack.header = HEADER_GC_MESSENGER;
	pack.subheader = MESSENGER_SUBHEADER_GC_HELPER_LOGIN;
	pack.size = sizeof(TPacketGCMessenger) + sizeof(uint8_t) + bLen;

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->BufferedPacket(&bLen, sizeof(uint8_t));
	d->Packet(companion.c_str(), companion.size());
}

void MessengerManager::SendHelperLogout(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	if (!companion.size())
	{
		return;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	LPDESC d = ch ? ch->GetDesc() : nullptr;

	if (!d)
	{
		return;
	}

	uint8_t bLen = companion.size();

	TPacketGCMessenger pack;
	pack.header		= HEADER_GC_MESSENGER;
	pack.subheader	= MESSENGER_SUBHEADER_GC_HELPER_LOGOUT;
	pack.size		= sizeof(TPacketGCMessenger) + sizeof(uint8_t) + bLen;

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->BufferedPacket(&bLen, sizeof(uint8_t));
	d->Packet(companion.c_str(), companion.size());
}
#endif

void MessengerManager::__RemoveFromList(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	m_Relation[account].erase(companion);
	m_InverseRelation[companion].erase(account);

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
#ifdef TEXTS_IMPROVEMENT
	if (ch) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 182, "%s", companion.c_str());
	}
#endif
}

void MessengerManager::RemoveFromList(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	if (companion.size() == 0)
		return;

	// @fixme142 BEGIN
	DBManager::instance().EscapeString(__account, sizeof(__account), account.c_str(), account.size());
	DBManager::instance().EscapeString(__companion, sizeof(__companion), companion.c_str(), companion.size());
	if (account.compare(__account) || companion.compare(__companion))
		return;
	// @fixme142 END

	sys_log(1, "Messenger Remove %s %s", account.c_str(), companion.c_str());
	DBManager::instance().Query("DELETE FROM messenger_list%s WHERE account='%s' AND companion = '%s'",
			get_table_postfix(), __account, __companion);

	__RemoveFromList(account, companion);

	TPacketGGMessenger p2ppck;

	p2ppck.bHeader = HEADER_GG_MESSENGER_REMOVE;
	strlcpy(p2ppck.szAccount, account.c_str(), sizeof(p2ppck.szAccount));
	strlcpy(p2ppck.szCompanion, companion.c_str(), sizeof(p2ppck.szCompanion));
	P2P_MANAGER::instance().Send(&p2ppck, sizeof(TPacketGGMessenger));
}

void MessengerManager::RemoveAllList(keyA account)
{
	std::set<keyT>	company(m_Relation[account]);

	// @fixme142 BEGIN
	DBManager::instance().EscapeString(__account, sizeof(__account), account.c_str(), account.size());
	if (account.compare(__account))
		return;
	// @fixme142 END

	/* SQL Data 삭제 */
	DBManager::instance().Query("DELETE FROM messenger_list%s WHERE account='%s' OR companion='%s'",
			get_table_postfix(), __account, __account);

	/* 내가 가지고있는 리스트 삭제 */
	for (std::set<keyT>::iterator iter = company.begin();
			iter != company.end();
			iter++ )
	{
		this->RemoveFromList(account, *iter);
	}

	/* 복사한 데이타 삭제 */
	for (std::set<keyT>::iterator iter = company.begin();
			iter != company.end();
			)
	{
		company.erase(iter++);
	}

	company.clear();
}


void MessengerManager::SendList(MessengerManager::keyA account)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());

	if (!ch)
		return;

	LPDESC d = ch->GetDesc();

	if (!d)
		return;

	if (m_Relation.find(account) == m_Relation.end())
		return;

	if (m_Relation[account].empty())
		return;

	TPacketGCMessenger pack;

	pack.header		= HEADER_GC_MESSENGER;
	pack.subheader	= MESSENGER_SUBHEADER_GC_LIST;
	pack.size		= sizeof(TPacketGCMessenger);

	TPacketGCMessengerListOffline pack_offline;
	TPacketGCMessengerListOnline pack_online;

	TEMP_BUFFER buf(128 * 1024); // 128k

	auto it = m_Relation[account].begin(), eit = m_Relation[account].end();

	while (it != eit)
	{
		if (m_set_loginAccount.find(*it) != m_set_loginAccount.end())
		{
			pack_online.connected = 1;
			int iSize = it->size();
			// Online
			pack_online.length = iSize;

			buf.write(&pack_online, sizeof(TPacketGCMessengerListOnline));
			buf.write(it->c_str(), iSize);
		}
		else
		{
			pack_offline.connected = 0;
			int iSize = it->size();
			// Offline
			pack_offline.length = iSize;

			buf.write(&pack_offline, sizeof(TPacketGCMessengerListOffline));
			buf.write(it->c_str(), iSize);
		}

		++it;
	}

	pack.size += buf.size();

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->Packet(buf.read_peek(), buf.size());
}

void MessengerManager::SendLogin(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	LPDESC d = ch ? ch->GetDesc() : nullptr;

	if (!d)
		return;

	if (!d->GetCharacter())
		return;

	if (ch->GetGMLevel() == GM_PLAYER && gm_get_level(companion.c_str()) != GM_PLAYER)
		return;

	uint8_t bLen = companion.size();

	TPacketGCMessenger pack;

	pack.header			= HEADER_GC_MESSENGER;
	pack.subheader		= MESSENGER_SUBHEADER_GC_LOGIN;
	pack.size			= sizeof(TPacketGCMessenger) + sizeof(uint8_t) + bLen;

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->BufferedPacket(&bLen, sizeof(uint8_t));
	d->Packet(companion.c_str(), companion.size());
}

void MessengerManager::SendLogout(MessengerManager::keyA account, MessengerManager::keyA companion)
{
	if (!companion.size())
		return;

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindPC(account.c_str());
	LPDESC d = ch ? ch->GetDesc() : nullptr;

	if (!d)
		return;

	uint8_t bLen = companion.size();

	TPacketGCMessenger pack;

	pack.header		= HEADER_GC_MESSENGER;
	pack.subheader	= MESSENGER_SUBHEADER_GC_LOGOUT;
	pack.size		= sizeof(TPacketGCMessenger) + sizeof(uint8_t) + bLen;

	d->BufferedPacket(&pack, sizeof(TPacketGCMessenger));
	d->BufferedPacket(&bLen, sizeof(uint8_t));
	d->Packet(companion.c_str(), companion.size());
}



