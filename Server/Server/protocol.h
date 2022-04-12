#pragma once
#pragma once

constexpr int SERVER_PORT = 3500;
constexpr int MAX_ID_LEN = 10;
constexpr int MAX_USER = 10000;
constexpr int WORLD_WIDTH = 800;
constexpr int WORLD_HEIGHT = 800;
constexpr int MAX_STR_LEN = 100;
constexpr int VIEW_LIMIT = 10; // 시야 반지름, 상대방과 사이에 6개의 타일이 있어도 보여야 함.

constexpr int NUM_NPC = 10000;

constexpr int NUM_OBSTACLE = 5000; // 장애물 개수
constexpr int NUM_ITEM = 5000; // 아이템 개수

constexpr int MAX_MONSTERHP = 40; // 몬스터 최대 체력
constexpr int MAX_PLAYERHP = 100; // 플레이어 최대 체력
constexpr int MONSTER_ATTACKDAMAGE = 10; // 플레이어 공격력

constexpr int PLUS_ITEMHP = 50; // HP 아이템 소모 시 체력 증가되는 수

// 오브젝트 타입 정의
constexpr int OTYPE_NPC_PEACE = 0; // peace 몬스터 
constexpr int OTYPE_NPC_ROAMING = 1; // roaming 몬스터
constexpr int OTYPE_NPC_AGRO = 2; // agro 몬스터
constexpr int OTYPE_ITEM_HP = 3; // HP 아이템
constexpr int OTYPE_ITEM_BUF = 4; // 버프 아이템


constexpr char SC_PACKET_LOGIN_OK	= 0;
constexpr char SC_PACKET_MOVE		= 1;
constexpr char SC_PACKET_ENTER		= 2;
constexpr char SC_PACKET_LEAVE		= 3;
constexpr char SC_PACKET_CHAT		= 4;
constexpr char SC_PACKET_LOGIN_FAIL	= 5;
constexpr char SC_PACKET_STAT_CHANGE	= 6;
constexpr char SC_PACKET_GAMEOVER = 7;

constexpr char CS_LOGIN		= 0;
constexpr char CS_MOVE		= 1;
constexpr char CS_ATTACK	= 2;
constexpr char CS_CHAT		= 3;
constexpr char CS_LOGOUT	= 4;
constexpr char CS_TELEORT	= 5;				// 부하 테스트용 동접 테스트를 위해 텔러포트로 Hot Spot 해소
constexpr char CS_PORTION = 6;
constexpr char CS_ATTACKS = 7;


constexpr char MV_UP = 0;
constexpr char MV_DOWN = 1;
constexpr char MV_LEFT = 2;
constexpr char MV_RIGHT = 3;

#pragma pack (push, 1)

struct sc_packet_login_ok {
	char size;
	char type;
	int  id;
	short x, y;
	short hp;
	short level;
	int   exp;
};

struct sc_packet_move {
	char size;
	char type;
	int id;
	short x, y;
	int move_time;
};

struct sc_packet_enter {
	char size;
	char type;
	int  id;
	char name[MAX_ID_LEN];
	char o_type;
	short x, y;
};

struct sc_packet_leave {
	char size;
	char type;
	int  id;
};

struct sc_packet_chat {
	char  size;
	char  type;
	int	  id;			
	char  message[MAX_STR_LEN];
};

struct sc_packet_login_fail {
	char  size;
	char  type;
	int	  id;			
	char  message[MAX_STR_LEN];
};

struct sc_packet_stat_change {
	char size;
	char type;
	int  id;
	short hp;
	short level;
	int   exp;
};

struct sc_packet_gameover {
	int id;
};

struct cs_packet_login {
	char  size;
	char  type;
	char  name[MAX_ID_LEN];
};


struct cs_packet_move {
	char  size;
	char  type;
	char  direction;
	int	  move_time;
};

struct cs_packet_attack {
	char	size;
	char	type;
};

struct cs_packet_chat {
	char	size;
	char	type;
	char	message[MAX_STR_LEN];
};

struct cs_packet_logout {
	char	size;
	char	type;
};



#pragma pack (pop)

