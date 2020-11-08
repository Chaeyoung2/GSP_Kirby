#pragma once
#define MAX_NICKNAME 64

constexpr int SERVER_PORT = 3500;

constexpr int TILEMAX = 100;
constexpr int WORLD_WIDTH = 100;
constexpr int WORLD_HEIGHT = 100;
constexpr int MAX_BUFFER = 4096;
constexpr int MIN_BUFFER = 1024;

constexpr int MAX_ID_LEN = 10;
constexpr int MAX_USER = 10;

#pragma pack (push, 1)

constexpr char SC_LOGIN_OK = 0;
constexpr char SC_MOVEPLAYER = 1;
constexpr char SC_ENTER = 2;
constexpr char SC_LEAVE = 3;


constexpr char CS_LOGIN = 0;
constexpr char CS_MOVE = 1;

constexpr char MV_UP = 0;
constexpr char MV_DOWN = 1;
constexpr char MV_LEFT = 2;
constexpr char MV_RIGHT = 3;

struct sc_packet_login_ok {
	char size;
	char type;
	int id;
	short x, y;
	short hp;
	short level;
	int exp;
};

struct sc_packet_enter {
	char size;
	char type;
	int id;
	char name[MAX_ID_LEN];
	char o_type;
	short x, y;
};


struct sc_packet_move {
	char size;
	char type;
	int id;
	short x, y;
};

struct sc_packet_leave {
	char size;
	char type;
	int id;
};


struct cs_packet_login {
	char size;
	char type;
	char name[MAX_ID_LEN];
};

struct cs_packet_move {
	char size;
	char type;
	char direction;
};


#pragma pack(pop)