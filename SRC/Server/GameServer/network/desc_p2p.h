#include "desc.h"

class DESC_P2P : public DESC
{
	public:
		virtual ~DESC_P2P();

		virtual void	Destroy();
		virtual void	SetPhase(int iPhase);
		bool		Setup(auriga::net::tcp::socket&& sock, const char * host, uint16_t wPort);

		virtual void	OnInput() override;
		virtual void	OnHeartbeatSend() override;

		bool		IsAwaitingAuth() const { return m_bAwaitingAuth; }

	private:
		void		SendAuthChallenge();
		void		HandleAuthInput();

	private:
		bool		m_bAwaitingAuth { false };
		uint8_t		m_authNonce[16] {};
		uint64_t	m_authTimestamp { 0 };
};
