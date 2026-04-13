#pragma once
#pragma pack(1)

typedef struct SPacketGGSetup
{
	uint8_t	bHeader;
	uint16_t	wPort;
	uint8_t	bChannel;
} TPacketGGSetup;

typedef struct SPacketGGLogin
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	uint32_t	dwPID;
	uint8_t	bEmpire;
	int32_t	lMapIndex;
	uint8_t	bChannel;
} TPacketGGLogin;

typedef struct SPacketGGLogout
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGGLogout;

typedef struct SPacketGGRelay
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lSize;
} TPacketGGRelay;

typedef struct SPacketGGNotice
{
	uint8_t	bHeader;
	int32_t	lSize;
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
	char szText[2048];
#endif
} TPacketGGNotice;

typedef struct SPacketGGShutdown
{
	uint8_t	bHeader;
} TPacketGGShutdown;

typedef struct SPacketGGGuild
{
	uint8_t	bHeader;
	uint8_t	bSubHeader;
	uint32_t	dwGuild;
} TPacketGGGuild;

typedef struct SPacketGGGuildChat
{
	uint8_t	bHeader;
	uint8_t	bSubHeader;
	uint32_t	dwGuild;
	char	szText[CHAT_MAX_LEN + 1];
} TPacketGGGuildChat;

typedef struct SPacketGGParty
{
	uint8_t	header;
	uint8_t	subheader;
	uint32_t	pid;
	uint32_t	leaderpid;
} TPacketGGParty;

typedef struct SPacketGGDisconnect
{
	uint8_t	bHeader;
	char	szLogin[LOGIN_MAX_LEN + 1];
} TPacketGGDisconnect;

typedef struct SPacketGGShout
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
	char	szText[CHAT_MAX_LEN + 1];
} TPacketGGShout;

typedef struct SPacketGGMessenger
{
	uint8_t        bHeader;
	char        szAccount[CHARACTER_NAME_MAX_LEN + 1];
	char        szCompanion[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGGMessenger;

typedef struct SPacketGGMessengerMobile
{
	uint8_t        bHeader;
	char        szName[CHARACTER_NAME_MAX_LEN + 1];
	char        szMobile[MOBILE_MAX_LEN + 1];
} TPacketGGMessengerMobile;

typedef struct SPacketGGFindPosition
{
	uint8_t header;
	uint32_t dwFromPID; // 저 위치로 워프하려는 사람
	uint32_t dwTargetPID; // 찾는 사람
} TPacketGGFindPosition;

typedef struct SPacketGGWarpCharacter
{
	uint8_t header;
	uint32_t pid;
	int32_t x;
	int32_t y;
#ifdef __CMD_WARP_IN_DUNGEON__
	int mapIndex;
#endif
} TPacketGGWarpCharacter;

typedef struct SPacketGGGuildWarMapIndex
{
	uint8_t bHeader;
	uint32_t dwGuildID1;
	uint32_t dwGuildID2;
	int32_t lMapIndex;
} TPacketGGGuildWarMapIndex;

typedef struct SPacketGGTransfer
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lX, lY;
} TPacketGGTransfer;

typedef struct SPacketGGLoginPing
{
	uint8_t	bHeader;
	char	szLogin[LOGIN_MAX_LEN + 1];
} TPacketGGLoginPing;

typedef struct SPacketGGBlockChat
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lBlockDuration;
} TPacketGGBlockChat;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
typedef struct SPacketGGWhisperSystem
{
	uint8_t	bHeader;
	int32_t	lSize;
} TPacketGGWhisperSystem;
#endif

#ifdef TEXTS_IMPROVEMENT
typedef struct SPacketGGChatNew {
	uint8_t header;
	uint8_t type;
	uint8_t empire;
	int32_t mapidx;
	uint32_t idx;
	uint16_t size;
} TPacketGGChatNew;
#endif

#pragma pack()