import app
import localeinfo

SERVER_MODE = "LIVE"  # alapértelmezett

SRV_LIVE = {
	"name": "Auriga Global",
	"host": "37.187.254.37",
	"auth1": 30052,
	"ch1": 30054,
	"ch2": 30058,
	"ch3": 30062,
	"ch4": 30066,
	"ch5": 30070,
	"ch6": 30074,
}

SRV_TEST = {
	"name": "Auriga TEST",
	#"host": "152.53.120.221",
	"host": "127.0.0.1",

	"auth1": 30052,
	"ch1": 30054,
	"ch2": 30058,
	"ch3": 30062,
	"ch4": 30066,
	"ch5": 30070,
	"ch6": 30074,
}


def getServerData():
	global SERVER_MODE
	if SERVER_MODE == "TEST":
		return SRV_TEST
	return SRV_LIVE


# Default értékek (üresen inicializáljuk, majd frissítjük)
SERVER1_CHANNEL_DICT = {}
REGION_NAME_DICT = {}
REGION_AUTH_SERVER_DICT = {}
REGION_DICT = {}
MARKADDR_DICT = {}

STATE_NONE = "|cFFFF0000Offline"
STATE_DICT = {
	0: "|cFFFF0000Non Disponibile",
	1: "|cFF17EA17Online",
	2: "|cFFFF4500Occupato",
	3: "|cFFFF0000Pieno"
}


# ==============================================================
# 🔥 Dinamikus újratöltés minden szerverváltáskor
# ==============================================================
def UpdateServerConfig():
	global SERVER1_CHANNEL_DICT
	global REGION_NAME_DICT
	global REGION_AUTH_SERVER_DICT
	global REGION_DICT
	global MARKADDR_DICT

	SRV = getServerData()

	SERVER1_CHANNEL_DICT = {
		1: {"key": 11, "name": "Channel 1", "ip": SRV["host"], "tcp_port": SRV["ch1"], "udp_port": SRV["ch1"], "state": STATE_NONE},
		2: {"key": 12, "name": "Channel 2", "ip": SRV["host"], "tcp_port": SRV["ch2"], "udp_port": SRV["ch2"], "state": STATE_NONE},
		3: {"key": 13, "name": "Channel 3", "ip": SRV["host"], "tcp_port": SRV["ch3"], "udp_port": SRV["ch3"], "state": STATE_NONE},
		4: {"key": 14, "name": "Channel 4", "ip": SRV["host"], "tcp_port": SRV["ch4"], "udp_port": SRV["ch4"], "state": STATE_NONE},
		5: {"key": 15, "name": "Channel 5", "ip": SRV["host"], "tcp_port": SRV["ch5"], "udp_port": SRV["ch5"], "state": STATE_NONE},
		6: {"key": 16, "name": "Channel 6", "ip": SRV["host"], "tcp_port": SRV["ch6"], "udp_port": SRV["ch6"], "state": STATE_NONE},
	}

	REGION_NAME_DICT = {
		0: SRV["name"],
	}

	REGION_AUTH_SERVER_DICT = {
		0: {
			1: {"ip": SRV["host"], "port": SRV["auth1"]},
		}
	}

	REGION_DICT = {
		0: {
			1: {"name": SRV["name"], "channel": SERVER1_CHANNEL_DICT},
		}
	}

	MARKADDR_DICT = {
		10: {"ip": SRV["host"], "tcp_port": SRV["ch1"], "mark": "10.tga", "symbol_path": "10"},
	}



UpdateServerConfig()
