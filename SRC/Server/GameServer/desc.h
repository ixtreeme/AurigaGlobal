#ifndef __INC_METIN_II_GAME_DESC_H__
#define __INC_METIN_II_GAME_DESC_H__

#include <entt/entt.hpp>

#include "constants.h"
#include "input.h"
#ifdef _IMPROVED_PACKET_ENCRYPTION_
#include "cipher.h"
#endif

#define MAX_ALLOW_USER                  4096
//#define MAX_INPUT_LEN			2048
//#define MAX_INPUT_LEN			65536
#define MAX_INPUT_LEN (300*1024)

#define HANDSHAKE_RETRY_LIMIT		32

#if defined(__IMPROVED_HANDSHAKE_PROCESS__)
#	define INTRUSIVE_HANDSHAKE_LOG false /* (choice)
	* Enable or disable intrusive handshake logging.
	*/
#	define INTRUSIVE_HANDSHAKE_PULSE 1 /* (seconds)
	* This is the handshake pulse limit in seconds.
	* Do not change this value if you don't know
	* what you're doing.
	*/
#	define INTRUSIVE_HANDSHAKE_NEXT_PULSE 60 * 10 /* (seconds)
	* This pulse is set after blocking a socket.
	* In other words, the value is a cooldown after
	* a socket (host) can connect again to the server.
	*/
#	define INTRUSIVE_HANDSHAKE_LIMIT 10 /* (handshakes)
	* This handshake limit is compared with INTRUSIVE_HANDSHAKE_PULSE
	* It will verify if INTRUSIVE_HANDSHAKE_LIMIT has been sent
	* below INTRUSIVE_HANDSHAKE_PULSE (seconds).
	*/
#	define INTRUSIVE_HANDSHAKE_BAN false /* (choice)
	* When this is enabled (TRUE) the host doesn't get
	* temporarily blocked, instead, the host is permanently
	* blocked and dumped into the BANIP file in the core directory.
	*/
#endif

class CInputProcessor;

enum EDescType
{
	DESC_TYPE_ACCEPTOR,
	DESC_TYPE_CONNECTOR
};

class CLoginKey
{
	public:
		CLoginKey(uint32_t dwKey, LPDESC pkDesc) : m_dwKey(dwKey), m_pkDesc(pkDesc)
		{
			m_dwExpireTime = 0;
		}

		void Expire()
		{
			m_dwExpireTime = get_dword_time();
			m_pkDesc = nullptr;
		}

		operator uint32_t() const
		{
			return m_dwKey;
		}

		uint32_t   m_dwKey;
		uint32_t   m_dwExpireTime;
		LPDESC  m_pkDesc;
};


// sequence 버그 찾기용 데이타
struct seq_t
{
	uint8_t	hdr;
	uint8_t	seq;
};
typedef std::vector<seq_t>	seq_vector_t;
// sequence 버그 찾기용 데이타

class DESC
{
	public:
		EVENTINFO(desc_event_info)
		{
			LPDESC desc;

			desc_event_info()
			: desc(nullptr)
			{
			}
		};

	public:
		DESC();
		virtual ~DESC();

		virtual uint8_t		GetType() { return DESC_TYPE_ACCEPTOR; }
		virtual void		Destroy();
		virtual void		SetPhase(int _phase);

		void			FlushOutput();

		bool			Setup(LPFDWATCH _fdw, socket_t _fd, const struct sockaddr_in & c_rSockAddr, uint32_t _handle, uint32_t _handshake);

		socket_t		GetSocket() const	{ return m_sock; }
		const char *	GetHostName()		{ return m_stHost.c_str(); }
		uint16_t			GetPort()	{ return m_wPort; }

		void			SetP2P(const char * h, uint16_t w, uint8_t b) { m_stP2PHost = h; m_wP2PPort = w; m_bP2PChannel = b; }
		const char *	GetP2PHost()		{ return m_stP2PHost.c_str();	}
		uint16_t			GetP2PPort() const		{ return m_wP2PPort; }
		uint8_t			GetP2PChannel() const	{ return m_bP2PChannel;	}

		void			BufferedPacket(const void * c_pvData, int iSize);
		void			Packet(const void * c_pvData, int iSize);
		void			LargePacket(const void * c_pvData, int iSize);

		int			ProcessInput();		// returns -1 if error
		int			ProcessOutput();	// returns -1 if error

		CInputProcessor	*	GetInputProcessor()	{ return m_pInputProcessor; }

		uint32_t			GetHandle() const	{ return m_dwHandle; }
		LPBUFFER		GetOutputBuffer()	{ return m_lpOutputBuffer; }

		void			BindAccountTable(TAccountTable * pTable);
		TAccountTable &		GetAccountTable()	{ return m_accountTable; }

		void			BindCharacter(LPCHARACTER ch);
		LPCHARACTER		GetCharacter()		{ return m_lpCharacter; }
        entt::entity        GetEntity() const      { return m_entity; }
        void                SetEntity(entt::entity e) { m_entity = e; }

		bool			IsPhase(int phase) const	{ return m_iPhase == phase ? true : false; }

		const struct sockaddr_in & GetAddr()		{ return m_SockAddr;	}

		void			   UDPGrant(const struct sockaddr_in & c_rSockAddr);
		const struct sockaddr_in & GetUDPAddr()		{ return m_UDPSockAddr; }

		void			Log(const char * format, ...);

		// 핸드쉐이크 (시간 동기화)
		void			StartHandshake(uint32_t _dw);
		void			SendHandshake(uint32_t dwCurTime, int32_t lNewDelta);
		bool			HandshakeProcess(uint32_t dwTime, int32_t lDelta, bool bInfiniteRetry=false);
		bool			IsHandshaking();

		uint32_t			GetHandshake() const	{ return m_dwHandshake; }
		uint32_t			GetClientTime();

#ifdef _IMPROVED_PACKET_ENCRYPTION_
		void SendKeyAgreement();
		void SendKeyAgreementCompleted();
		bool FinishHandshake(size_t agreed_length, const void* buffer, size_t length);
		bool IsCipherPrepared();
#else
		// Obsolete encryption stuff here
		void			SetSecurityKey(const uint32_t * c_pdwKey);
		const uint32_t *	GetEncryptionKey() const { return &m_adwEncryptionKey[0]; }
		const uint32_t *	GetDecryptionKey() const { return &m_adwDecryptionKey[0]; }
#endif

		// 제국
		uint8_t			GetEmpire();

#ifdef ENABLE_MULTI_LANGUAGE
		void 			SetLanguage(uint8_t bLanguage) { m_accountTable.bLanguage = bLanguage; sys_log(0, "FINDME : language set to %u", bLanguage);}
		uint8_t 			GetLanguage() { return m_accountTable.bLanguage;}
#endif

		// for p2p
		void			SetRelay(const char * c_pszName);
		bool			DelayedDisconnect(int iSec);
		void			DisconnectOfSameLogin();

		void			SetAdminMode();
		bool			IsAdminMode();		// Handshake 에서 어드민 명령을 쓸수있나?

		void			SetPong(bool b);
		bool			IsPong();

		void			SendLoginSuccessPacket();
		//void			SendServerStatePacket(int nIndex);

		void			SetLoginKey(uint32_t dwKey);
		void			SetLoginKey(CLoginKey * pkKey);
		uint32_t			GetLoginKey();

		void			AssembleCRCMagicCube(uint8_t bProcPiece, uint8_t bFilePiece);

		void			SetBillingExpireSecond(uint32_t dwSec);
		uint32_t			GetBillingExpireSecond();

		void			SetClientVersion(const char * c_pszTimestamp) { m_stClientVersion = c_pszTimestamp; }
		const char *		GetClientVersion() { return m_stClientVersion.c_str(); }

		bool			isChannelStatusRequested() const { return m_bChannelStatusRequested; }
		void			SetChannelStatusRequested(bool bChannelStatusRequested) { m_bChannelStatusRequested = bChannelStatusRequested; }
#ifdef ENABLE_HWID
		const char * GetHwid() { return m_accountTable.hwid; };
#endif

	protected:
		void			Initialize();

	protected:
		CInputProcessor *	m_pInputProcessor;
		CInputClose		m_inputClose;
		CInputHandshake	m_inputHandshake;
		CInputLogin		m_inputLogin;
		CInputMain		m_inputMain;
		CInputDead		m_inputDead;
		CInputAuth		m_inputAuth;


		LPFDWATCH		m_lpFdw;
		socket_t		m_sock;
		int				m_iPhase;
		uint32_t			m_dwHandle;

		std::string		m_stHost;
		uint16_t			m_wPort;
		uint64_t			m_LastTryToConnectTime;

		LPBUFFER		m_lpInputBuffer;
		int				m_iMinInputBufferLen;

		uint32_t			m_dwHandshake;
		uint32_t			m_dwHandshakeSentTime;
		int				m_iHandshakeRetry;
		uint32_t			m_dwClientTime;
		bool			m_bHandshaking;

		LPBUFFER		m_lpBufferedOutputBuffer;
		LPBUFFER		m_lpOutputBuffer;

		LPEVENT			m_pkPingEvent;
		LPCHARACTER		m_lpCharacter;
        entt::entity        m_entity { entt::null };
		TAccountTable		m_accountTable;

		struct sockaddr_in	m_SockAddr;
		struct sockaddr_in 	m_UDPSockAddr;

		FILE *			m_pLogFile;
		std::string		m_stRelayName;

		std::string		m_stP2PHost;
		uint16_t			m_wP2PPort;
		uint8_t			m_bP2PChannel;

		bool			m_bAdminMode; // Handshake 에서 어드민 명령을 쓸수있나?
		bool			m_bPong;

		int			m_iCurrentSequence;

		CLoginKey *		m_pkLoginKey;
		uint32_t			m_dwLoginKey;

		uint8_t                    m_bCRCMagicCubeIdx;
		uint32_t                   m_dwProcCRC;
		uint32_t                   m_dwFileCRC;
		bool			m_bHackCRCQuery;

		uint32_t			m_dwBillingExpireSecond;
		std::string		m_stClientVersion;

		std::string		m_Login;
		int				m_outtime;
		int				m_playtime;
		int				m_offtime;

		bool			m_bDestroyed;
		bool			m_bChannelStatusRequested;

#ifdef _IMPROVED_PACKET_ENCRYPTION_
		Cipher cipher_;
#else
		// Obsolete encryption stuff here
		bool			m_bEncrypted;
		uint32_t			m_adwDecryptionKey[4];
		uint32_t			m_adwEncryptionKey[4];
#endif

	public:
		LPEVENT			m_pkDisconnectEvent;

	public:
		void SetLogin( const std::string & login ) { m_Login = login; }
		void SetLogin( const char * login ) { m_Login = login; }
		const std::string& GetLogin() { return m_Login; }

		void SetOutTime( int outtime ) { m_outtime = outtime; }
		void SetOffTime( int offtime ) { m_offtime = offtime; }
		void SetPlayTime( int playtime ) { m_playtime = playtime; }

		void RawPacket(const void * c_pvData, int iSize);
		void ChatPacket(uint8_t type, const char * format, ...);

		/* 시퀀스 버그 찾기용 코드 */
	public:
		seq_vector_t	m_seq_vector;
		void			push_seq (uint8_t hdr, uint8_t seq);
#ifdef TEXTS_IMPROVEMENT
	public:
		void ChatPacketNew(uint8_t type, uint32_t idx, const char * format, ...);
#endif
};

#endif
