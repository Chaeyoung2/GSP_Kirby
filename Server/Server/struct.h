
struct OVER_EX {
	WSAOVERLAPPED wsa_over;
	WSABUF wsa_buf;
	char op_mode;
	unsigned char iocp_buf[MAX_BUFFER]; 
	int object_id;
};
struct client_info {
	client_info() {}
	client_info(int _id, short _x, short _y, SOCKET _sock, bool _connected) {
		id = _id; x = _x; y = _y; sock = _sock; connected = _connected;
	}
	OVER_EX m_recv_over;
	mutex c_lock;
	int id = -1;
	char name[MAX_ID_LEN];
	short x, y;
	short hp;
	short level;
	int exp;
	lua_State* L;
	SOCKET sock = -1;
	atomic_bool connected = false;
	atomic_bool is_active;
	unsigned char* m_packet_start; // 패킷 시작 위치
	unsigned char* m_recv_start; // recv 시작 위치
	unordered_set<int> view_list;
	int move_time;
	short sx, sy;
	bool is_AIrandommove = false;
	short cnt_randommove = 0;
	short encountered_id = 0;
	short attackme_id = 0;
	high_resolution_clock::time_point invincible_timeout;
	short m_type;
};
struct event_info {
	int obj_id;
	system_clock::time_point wakeup_time;
	int event_id;
	int target_id;
	constexpr bool operator < (const event_info& _Left) const {
		return (wakeup_time > _Left.wakeup_time);
	}
};