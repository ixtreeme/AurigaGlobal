#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "desc.h"

class CLIENT_DESC : public DESC
{
	public:
		CLIENT_DESC();
		virtual ~CLIENT_DESC();

		virtual uint8_t	GetType() { return DESC_TYPE_CONNECTOR; }
		virtual void	Destroy();
		virtual void	SetPhase(int phase);

		bool 		Connect(int iPhaseWhenSucceed = 0);
		void		Setup(const char * _host, uint16_t _port);

		virtual void	OnInput() override;
		virtual void	OnHeartbeatSend() override;

		void		SetRetryWhenClosed(bool);

		void		DBPacketHeader(uint8_t bHeader, uint32_t dwHandle, uint32_t dwSize);
		void		DBPacket(uint8_t bHeader, uint32_t dwHandle, const void * c_pvData, uint32_t dwSize);
		void		Packet(const void * c_pvData, int iSize);
		bool		IsRetryWhenClosed();

		void		Update(uint32_t t);
		void		UpdateChannelStatus(uint32_t t, bool fForce);

		// Non-destructive close for reuse
		void Reset();
		const auriga::player_session::Token &PlayerSessionLink() const { return m_playerSessionLink; }
		bool PlayerSessionsReady() const { return m_playerSessionsReady; }
		bool AcceptPlayerSessionHello(auriga::player_session::Token link)
		{
			if (!auriga::player_session::Nonzero(link) || link != m_playerSessionLink) return false;
			m_playerSessionsReady = true;
			return true;
		}

	private:
		void InitializeBuffers();
		void StartConnect();
		void SendAuthResponse(const uint8_t nonce[16], uint64_t timestamp);
		void HandleAuthInput();

	protected:
		int			m_iPhaseWhenSucceed;
		bool		m_bRetryWhenClosed;
		bool		m_connectPending;
		bool		m_bAwaitingAuth;
		bool		m_bAuthResponseSent;
		auriga::player_session::Token m_playerSessionLink{};
		bool m_playerSessionsReady = false;
		uint64_t		m_LastTryToConnectTime;
		uint64_t		m_tLastChannelStatusUpdateTime;

		CInputDB 	m_inputDB;
		CInputP2P 	m_inputP2P;

};


extern LPCLIENT_DESC db_clientdesc;
extern LPCLIENT_DESC g_NetmarbleDBDesc;


#endif
