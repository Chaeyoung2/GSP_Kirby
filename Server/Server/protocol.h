#pragma once

constexpr int SERVER_PORT = 3500;

constexpr int TILEMAX = 7;
constexpr int MAX_BUFFER = 4096;

constexpr int MAX_ID_LEN = 10;
constexpr int MAX_USER = 10;

#pragma pack (push, 1)

constexpr char SC_LOGIN_OK = 0;
constexpr char SC_MOVEPLAYER = 1;
constexpr char SC_ENTER = 2;
constexpr char SC_LEAVE = 3;

constexpr char CS_INPUTUP = 1;
constexpr char CS_INPUTDOWN = 2;
constexpr char CS_INPUTRIGHT = 3;
constexpr char CS_INPUTLEFT = 4;

struct sc_packet_login_ok {
	char size;
	char type;
	int id;
	short x, y;
};

struct cs_packet_up {
	char size;
	char type;
	int id;
};

struct cs_packet_down {
	char size;
	char type;
	int id;
};

struct cs_packet_left {
	char size;
	char type;
	int id;
};

struct cs_packet_right {
	char size;
	char type;
	int id;
};

struct sc_packet_move_player {
	char size;
	char type;
	char x, y;
	int id;
};

#pragma pack(pop)