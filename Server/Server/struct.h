
struct OVER_EX {
	// ## overlapped io pointer 확장.
// overlapped io 구조체 자체는 쓸 만한 정보가 없다.
// 따라서 정보들을 더 추가할 필요가 있다.
// - 뒤에 추가하면 iocp가 모르고 에러도 나지 않는다. (pointer만 왔다갔다)
// 꼭 필요한 정보
// - 현재 이 i/o가 send인지 recv인지
// - i/o buffer의 위치 (send할 때 버퍼도 같이 생성)
	WSAOVERLAPPED wsa_over;
	char op_mode;
	WSABUF wsa_buf;
	unsigned char iocp_buf[MAX_BUFFER]; // iocp send/recv 버퍼
	int object_id;
};
struct client_info {
	// ## 클라이언트 객체
// 서버는 클라이언트 정보(id, 네트워크 접속 정보, 상태, 게임 정보)를 가진 객체 필요.
// - 최대 동접과 같은 개수 필요
// GetQueuedCompletionStatus를 받았을 때 클라이언트 객체를 찾을 수 있어야 한다.
// - iocp에서 오고 가는 것을은 completion_key, overlapped i/o pointer, number of byte 뿐
// - completion_key를 클라 객체의 포인터나 id나 index로 한다.
	client_info() {}
	client_info(int _id, short _x, short _y, SOCKET _sock, bool _connected) {
		id = _id; x = _x; y = _y; sock = _sock; connected = _connected;
	}
	// ## overlapped 구조체
	// 모든 send, recv는 overlapped 구조체가 필요하다.
	// 하나의 구조체를 동시에 여러 호출에서 사용하는 것은 불가능
	// 소켓 당 recv 호출은 무조건 한 개, send 호출은 동시에 여러개 가능
	// - recv 호출용 overlapped 구조체 한 개를 계속 재사용하는 것이 바람직
	// - send 버퍼도 같은 개수 필요
	// - send 개수 제한이 없으므로 new/delete 사용
	// - 성능을 위해서는 공유 pool을 만들어 관리
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
	// atomic으로 접근 절대 순서를 모든 쓰레드에서 지키게 한다.
	atomic_bool connected = false;
	atomic_bool is_active;
	unsigned char* m_packet_start; // 패킷 시작 위치
	unsigned char* m_recv_start; // recv 시작 위치
	//mutex vl;
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
struct event_type {
	int obj_id;
	system_clock::time_point wakeup_time;
	int event_id;
	int target_id;
	constexpr bool operator < (const event_type& _Left) const {
		return (wakeup_time > _Left.wakeup_time);
	}
};