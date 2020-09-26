
const int CS_INPUTUP = 1;
const int CS_INPUTDOWN = 2;
const int CS_INPUTRIGHT = 3;
const int CS_INPUTLEFT = 4;
const int SC_MOVEPLAYER = 1;

#define TILEMAX 7


#pragma pack (push, 1)
struct cs_packet_up {
	char size;
	char type;
};

struct cs_packet_down {
	char size;
	char type;
};

struct cs_packet_left {
	char size;
	char type;
};

struct cs_packet_right {
	char size;
	char type;
};

struct sc_packet_move_player {
	char size;
	char type;
	char x, y;
};

#pragma pack(pop)