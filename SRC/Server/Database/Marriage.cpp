#include "stdafx.h"
#include "Marriage.h"
#include "Main.h"
#include "DBManager.h"
#include "ClientManager.h"

namespace marriage
{
	const uint32_t WEDDING_LENGTH = 60 * 60; // sec
	bool operator < (const TWedding& lhs, const TWedding& rhs)
	{
		return lhs.dwTime < rhs.dwTime;
	}

	bool operator > (const TWedding& lhs, const TWedding& rhs)
	{
		return lhs.dwTime > rhs.dwTime;
	}

	bool operator > (const TWeddingInfo &lhs, const TWeddingInfo& rhs)
	{
		return lhs.dwTime > rhs.dwTime;
	}

	using namespace std;

	CManager::CManager()
	{
	}

	CManager::~CManager()
	{
	}

	bool CManager::Initialize()
	{
		std::unique_ptr<SQLMsg> native(CDBManager::instance().DirectQuery(
			"SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='player' AND TABLE_NAME='marriage' AND COLUMN_NAME='typed_items'"));
		m_nativeMarriages = native && !native->uiSQLErrno && native->Get() && native->Get()->uiNumRows == 1;

		char szQuery[1024];

		snprintf(szQuery, sizeof(szQuery),
				"SELECT pid1, pid2, love_point, time, is_married, p1.name, p2.name FROM marriage, player%s as p1, player%s as p2 WHERE p1.id = pid1 AND p2.id = pid2",
				GetTablePostfix(), GetTablePostfix());

		std::unique_ptr<SQLMsg> cleanup(CDBManager::instance().DirectQuery(m_nativeMarriages
			? "DELETE FROM marriage WHERE is_married = 0 AND typed_items = 0"
			: "DELETE FROM marriage WHERE is_married = 0"));
		if (!cleanup || cleanup->uiSQLErrno) return false;
		unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));
		if (!pmsg || pmsg->uiSQLErrno || !pmsg->Get()) return false;

		SQLResult * pRes = pmsg->Get();
		LOG_INFO("MarriageList(size={})", pRes->uiNumRows);

		if (pRes->uiNumRows > 0)
		{
			for (uint64_t uiRow = 0; uiRow != pRes->uiNumRows; ++uiRow)
			{
				MYSQL_ROW row = pRes->FetchRow();

				uint32_t pid1 = 0; str_to_number(pid1, row[0]);
				uint32_t pid2 = 0; str_to_number(pid2, row[1]);
				int love_point = 0; str_to_number(love_point, row[2]);
				int64_t time = 0; str_to_number(time, row[3]);
				uint8_t is_married = 0; str_to_number(is_married, row[4]);
				const char* name1 = row[5];
				const char* name2 = row[6];

				TMarriage* pMarriage = new TMarriage(pid1, pid2, love_point, time, is_married, name1, name2);
				m_Marriages.insert(pMarriage);
				m_MarriageByPID.insert(make_pair(pid1, pMarriage));
				m_MarriageByPID.insert(make_pair(pid2, pMarriage));

				LOG_INFO("Marriage {}: LP:{} TM:{} ST:{} {:10}:{:16} {:10}:{:16} ", uiRow, love_point, time, is_married, pid1, name1, pid2, name2);
			}
		}
		return true;
	}

	TMarriage* CManager::Get(uint32_t dwPlayerID)
	{
		auto it = m_MarriageByPID.find(dwPlayerID);

		if (it != m_MarriageByPID.end())
			return it->second;

		return nullptr;
	}

	void Align(uint32_t& rPID1, uint32_t& rPID2)
	{
		if (rPID1 > rPID2)
			std::swap(rPID1, rPID2);
	}

	void CManager::Add(uint32_t dwPID1, uint32_t dwPID2, const char* szName1, const char* szName2)
	{
		const int64_t now = static_cast<int64_t>(CClientManager::instance().GetCurrentTime());
		if (IsMarried(dwPID1) || IsMarried(dwPID2))
		{
			LOG_ERROR("cannot marry already married character. {} - {}", dwPID1, dwPID2);
			return;
		}

        if (dwPID1 > dwPID2)
        {
            std::swap(dwPID1, dwPID2);
            std::swap(szName1, szName2);
        }
        const auto first = SqlParam::UInt(dwPID1), second = SqlParam::UInt(dwPID2);
        SqlTransaction transaction{
            {fmt::format("SELECT id FROM player{} WHERE id IN (?,?) ORDER BY id FOR UPDATE", GetTablePostfix()),
                {first, second}, {}, 2},
            {"INSERT INTO marriage(pid1,pid2,love_point,time) SELECT ?,?,0,? FROM DUAL WHERE NOT EXISTS "
                "(SELECT 1 FROM marriage WHERE pid1 IN (?,?) OR pid2 IN (?,?))",
                {first, second, SqlParam::Int(now), first, second, first, second}, 1, {}},
        };
        std::unique_ptr<SQLMsg> message(CDBManager::instance().DirectTransactionPrepared(std::move(transaction)));
        if (!message || message->uiSQLErrno || message->transactionOutcome != SqlTransactionOutcome::Committed)
        {
            LOG_ERROR("cannot insert marriage");
            return;
        }
        PublishAdd(TMarriage(dwPID1, dwPID2, 0, now, 0, szName1, szName2));
    }

    void CManager::PublishAdd(const TMarriage& marriage)
    {
        const auto first = marriage.pid1, second = marriage.pid2;
        if (Get(first) || Get(second)) return;
        LOG_INFO("MARRIAGE ADD {} {}", first, second);
        auto* record = new TMarriage(marriage);
        m_Marriages.insert(record);
        m_MarriageByPID.emplace(first, record);
        m_MarriageByPID.emplace(second, record);

        TPacketMarriageAdd packet{};
        packet.dwPID1 = first;
        packet.dwPID2 = second;
        packet.tMarryTime = marriage.time;
        strlcpy(packet.szName1, marriage.name1.c_str(), sizeof(packet.szName1));
        strlcpy(packet.szName2, marriage.name2.c_str(), sizeof(packet.szName2));
        CClientManager::instance().ForwardPacket(HEADER_DG_MARRIAGE_ADD, &packet, sizeof(packet));
        if (marriage.is_married)
        {
            TPacketMarriageUpdate state{};
            state.dwPID1 = first;
            state.dwPID2 = second;
            state.iLovePoint = marriage.love_point;
            state.byMarried = marriage.is_married;
            CClientManager::instance().ForwardPacket(HEADER_DG_MARRIAGE_UPDATE, &state, sizeof(state));
        }
    }

	void CManager::Update(uint32_t dwPID1, uint32_t dwPID2, int iLovePoint, uint8_t byMarried)
	{
		TMarriage* pMarriage = Get(dwPID1);
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			LOG_ERROR("not under marriage : {} {}", dwPID1, dwPID2);
			return;
		}

		Align(dwPID1, dwPID2);

		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "UPDATE marriage SET love_point = %d, is_married = %d WHERE pid1 = %u AND pid2 = %u",
				iLovePoint, byMarried, pMarriage->pid1, pMarriage->pid2);

		unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

		SQLResult* res = pmsg->Get();
		if (res->uiAffectedRows == 0 || res->uiAffectedRows == (uint32_t)-1)
		{
			LOG_INFO("cannot update marriage : PID:{} {}", dwPID1, dwPID2); // @warme012
			return;
		}

		LOG_INFO("MARRIAGE UPDATE PID:{} {} LP:{} ST:{}", dwPID1, dwPID2, iLovePoint, byMarried);
		pMarriage->love_point = iLovePoint;
		pMarriage->is_married = byMarried;

		TPacketMarriageUpdate p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		p.iLovePoint = pMarriage->love_point;
		p.byMarried = pMarriage->is_married;
		CClientManager::instance().ForwardPacket(HEADER_DG_MARRIAGE_UPDATE, &p, sizeof(p));
	}

	void CManager::Remove(uint32_t dwPID1, uint32_t dwPID2)
	{
		TMarriage* pMarriage = Get(dwPID1);

		if (pMarriage)
		{
			LOG_INFO("Break Marriage pid1 {} pid2 {} Other {}", dwPID1, dwPID2, pMarriage->GetOther(dwPID1));
		}
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			auto it = m_MarriageByPID.begin();

			for (; it != m_MarriageByPID.end(); ++it)
			{
				LOG_INFO("Marriage List pid1 {} pid2 {}", it->second->pid1, it->second->pid2);
			}
			LOG_ERROR("not under marriage : PID:{} {}", dwPID1, dwPID2);
			return;
		}

		Align(dwPID1, dwPID2);

		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM marriage WHERE pid1 = %u AND pid2 = %u", dwPID1, dwPID2);

		unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

		SQLResult* res = pmsg->Get();
		if (res->uiAffectedRows == 0 || res->uiAffectedRows == (uint32_t)-1)
		{
			LOG_ERROR("cannot delete marriage : PID:{} {}", dwPID1, dwPID2);
			return;
		}

        PublishRemove(dwPID1, dwPID2);
    }

    void CManager::PublishRemove(uint32_t first, uint32_t second)
    {
        if (auto* current = Get(first); current && current->GetOther(first) == second)
        {
            LOG_INFO("MARRIAGE REMOVE PID:{} {}", first, second);
            m_Marriages.erase(current);
            m_MarriageByPID.erase(first);
            m_MarriageByPID.erase(second);
            delete current;
        }
        TPacketMarriageRemove packet{};
        packet.dwPID1 = first;
        packet.dwPID2 = second;
        CClientManager::instance().ForwardPacket(HEADER_DG_MARRIAGE_REMOVE, &packet, sizeof(packet));
    }

	void CManager::EngageToMarriage(uint32_t dwPID1, uint32_t dwPID2)
	{
		TMarriage* pMarriage = Get(dwPID1);
		if (!pMarriage || pMarriage->GetOther(dwPID1) != dwPID2)
		{
			LOG_ERROR("not under marriage : PID:{} {}", dwPID1, dwPID2);
			return;
		}

		if (pMarriage->is_married)
		{
			LOG_ERROR("already married, cannot change engage to marry : PID:{} {}", dwPID1, dwPID2);
			return;
		}

		Align(dwPID1, dwPID2);

		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "UPDATE marriage SET is_married = 1 WHERE pid1 = %u AND pid2 = %u",
				pMarriage->pid1, pMarriage->pid2);

		unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

		SQLResult* res = pmsg->Get();
		if (res->uiAffectedRows == 0 || res->uiAffectedRows == (uint32_t)-1)
		{
			LOG_ERROR("cannot change engage to marriage : PID:{} {}", dwPID1, dwPID2);
			return;
		}

		LOG_INFO("MARRIAGE ENGAGE->MARRIAGE PID:{} {}", dwPID1, dwPID2);
		pMarriage->is_married = 1;

		TPacketMarriageUpdate p;
		p.dwPID1 = dwPID1;
		p.dwPID2 = dwPID2;
		p.iLovePoint = pMarriage->love_point;
		p.byMarried = pMarriage->is_married;
		CClientManager::instance().ForwardPacket(HEADER_DG_MARRIAGE_UPDATE, &p, sizeof(p));
	}

	void CManager::OnSetup(CPeer* peer)
	{
		for (auto it = m_Marriages.begin(); it != m_Marriages.end(); ++it)
		{
			TMarriage* pMarriage = *it;

			{
				TPacketMarriageAdd p;
				p.dwPID1 = pMarriage->pid1;
				p.dwPID2 = pMarriage->pid2;
				p.tMarryTime = pMarriage->time;
				strlcpy(p.szName1, pMarriage->name1.c_str(), sizeof(p.szName1));
				strlcpy(p.szName2, pMarriage->name2.c_str(), sizeof(p.szName2));
				peer->EncodeHeader(HEADER_DG_MARRIAGE_ADD, 0, sizeof(p));
				peer->Encode(&p, sizeof(p));
			}

			{
				TPacketMarriageUpdate p;
				p.dwPID1 = pMarriage->pid1;
				p.dwPID2 = pMarriage->pid2;
				p.iLovePoint = pMarriage->love_point;
				p.byMarried	= pMarriage->is_married;
				peer->EncodeHeader(HEADER_DG_MARRIAGE_UPDATE, 0, sizeof(p));
				peer->Encode(&p, sizeof(p));
			}
		}

		for (auto it = m_mapRunningWedding.begin(); it != m_mapRunningWedding.end(); ++it)
		{
			const TWedding& t = it->second;

			TPacketWeddingReady p;
			p.dwPID1 = t.dwPID1;
			p.dwPID2 = t.dwPID2;
			p.dwMapIndex = t.dwMapIndex;

			peer->EncodeHeader(HEADER_DG_WEDDING_READY, 0, sizeof(p));
			peer->Encode(&p, sizeof(p));

			TPacketWeddingStart p2;
			p2.dwPID1 = t.dwPID1;
			p2.dwPID2 = t.dwPID2;

			peer->EncodeHeader(HEADER_DG_WEDDING_START, 0, sizeof(p2));
			peer->Encode(&p2, sizeof(p2));
		}
	}

	void CManager::ReadyWedding(uint32_t dwMapIndex, uint32_t dwPID1, uint32_t dwPID2)
	{
		const uint64_t dwStartTime = CClientManager::instance().GetCurrentTime();
		m_pqWeddingStart.push(TWedding(dwStartTime + 5, dwMapIndex, dwPID1, dwPID2));
	}

	void CManager::EndWedding(uint32_t dwPID1, uint32_t dwPID2)
	{
		auto it = m_mapRunningWedding.find(make_pair(dwPID1, dwPID2));
		if (it == m_mapRunningWedding.end())
		{
			LOG_ERROR("try to end wedding {} {}", dwPID1, dwPID2);
			return;
		}

		TWedding& w = it->second;

		TPacketWeddingEnd p;
		p.dwPID1 = w.dwPID1;
		p.dwPID2 = w.dwPID2;
		CClientManager::instance().ForwardPacket(HEADER_DG_WEDDING_END, &p, sizeof(p));
		m_mapRunningWedding.erase(it);
	}

    void CManager::PublishNativeChanges()
    {
        std::unique_ptr<SQLMsg> rows(CDBManager::instance().DirectQuery(
            "SELECT e.id,e.pid1,e.pid2,e.engage,m.time,m.love_point,m.is_married,p.name,q.name "
            "FROM player.item_marriage_publication e LEFT JOIN player.marriage m ON m.pid1=e.pid1 AND m.pid2=e.pid2 "
            "LEFT JOIN player.player p ON p.id=e.pid1 LEFT JOIN player.player q ON q.id=e.pid2 "
            "ORDER BY e.id LIMIT 32"));
        if (!rows || rows->uiSQLErrno || !rows->Get()) return;
        while (auto row = rows->Get()->FetchRow())
        {
            std::uint64_t id = 0;
            std::uint32_t first = 0, second = 0;
            unsigned engage = 0;
            str_to_number(id, row[0]);
            str_to_number(first, row[1]);
            str_to_number(second, row[2]);
            str_to_number(engage, row[3]);
            if (engage && row[4] && row[7] && row[8])
            {
                std::int64_t time = 0;
                int points = 0;
                std::uint8_t married = 0;
                str_to_number(time, row[4]);
                str_to_number(points, row[5]);
                str_to_number(married, row[6]);
                PublishAdd(TMarriage(first, second, points, time, married, row[7], row[8]));
            }
            else if (!engage)
                PublishRemove(first, second);

            std::unique_ptr<SQLMsg> done(CDBManager::instance().DirectQueryPrepared(
                "DELETE FROM player.item_marriage_publication WHERE id=?", {SqlParam::UInt(id)}));
            if (!done || done->uiSQLErrno) return;
        }
    }

    void CManager::Update()
    {
        const uint64_t now = CClientManager::instance().GetCurrentTime();
        if (m_nativeMarriages) PublishNativeChanges();

		if (!m_pqWeddingEnd.empty())
		{
			while (!m_pqWeddingEnd.empty() && m_pqWeddingEnd.top().dwTime <= now)
			{
				TWeddingInfo wi = m_pqWeddingEnd.top();
				m_pqWeddingEnd.pop();

				auto it = m_mapRunningWedding.find(make_pair(wi.dwPID1, wi.dwPID2));
				if (it == m_mapRunningWedding.end())
					continue;

				const TWedding w = it->second;

				TPacketWeddingEnd p;
				p.dwPID1 = w.dwPID1;
				p.dwPID2 = w.dwPID2;
				CClientManager::instance().ForwardPacket(HEADER_DG_WEDDING_END, &p, sizeof(p));
				m_mapRunningWedding.erase(it);

				auto it_marriage = m_MarriageByPID.find(w.dwPID1);

				if (it_marriage != m_MarriageByPID.end())
				{
					TMarriage* pMarriage = it_marriage->second;
					if (!pMarriage->is_married)
					{
						Remove(pMarriage->pid1, pMarriage->pid2);
					}
				}
			}
		}
		if (!m_pqWeddingStart.empty())
		{
			while (!m_pqWeddingStart.empty() && m_pqWeddingStart.top().dwTime <= now)
			{
				TWedding w = m_pqWeddingStart.top();
				m_pqWeddingStart.pop();

				TPacketWeddingStart p;
				p.dwPID1 = w.dwPID1;
				p.dwPID2 = w.dwPID2;
				CClientManager::instance().ForwardPacket(HEADER_DG_WEDDING_START, &p, sizeof(p));

				w.dwTime += WEDDING_LENGTH;
				m_pqWeddingEnd.push(TWeddingInfo(w.dwTime, w.dwPID1, w.dwPID2));
				m_mapRunningWedding.insert(make_pair(make_pair(w.dwPID1, w.dwPID2), w));
			}
		}
	}
}
