#pragma once
#define MAX_NICKNAME 64


constexpr int SERVER_PORT = 3500;

constexpr int TILESCREENMAX = 20;
constexpr int TILEMAX = 2000;
constexpr int WORLD_WIDTH = 2000;
constexpr int WORLD_HEIGHT = 2000;
constexpr int MAX_BUFFER = 4096;
constexpr int MIN_BUFFER = 1024;

constexpr int MAX_ID_LEN = 10;
constexpr int MAX_USER = 10000;
constexpr int MAX_STR_LEN = 32;

constexpr int VIEW_LIMIT = 8;
constexpr int NUM_NPC = 200000;

#pragma pack (push, 1)

constexpr char SC_LOGIN_OK = 0;
constexpr char SC_MOVEPLAYER = 1;
constexpr char SC_ENTER = 2;
constexpr char SC_LEAVE = 3;
constexpr char SC_CHAT = 4;


constexpr char CS_LOGIN = 1;
constexpr char CS_MOVE = 2;

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
	int move_time;
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
	int	  move_time;
};

struct sc_packet_chat {
	char  size; 
	char  type;
	int id; 
	char  message[MAX_STR_LEN];
};


#pragma pack(pop)