
struct OVER_EX {
	// ## overlapped io pointer Ȯ��.
// overlapped io ����ü ��ü�� �� ���� ������ ����.
// ���� �������� �� �߰��� �ʿ䰡 �ִ�.
// - �ڿ� �߰��ϸ� iocp�� �𸣰� ������ ���� �ʴ´�. (pointer�� �Դٰ���)
// �� �ʿ��� ����
// - ���� �� i/o�� send���� recv����
// - i/o buffer�� ��ġ (send�� �� ���۵� ���� ����)
	WSAOVERLAPPED wsa_over;
	char op_mode;
	WSABUF wsa_buf;
	unsigned char iocp_buf[MAX_BUFFER]; // iocp send/recv ����
	int object_id;
};
struct client_info {
	// ## Ŭ���̾�Ʈ ��ü
// ������ Ŭ���̾�Ʈ ����(id, ��Ʈ��ũ ���� ����, ����, ���� ����)�� ���� ��ü �ʿ�.
// - �ִ� ������ ���� ���� �ʿ�
// GetQueuedCompletionStatus�� �޾��� �� Ŭ���̾�Ʈ ��ü�� ã�� �� �־�� �Ѵ�.
// - iocp���� ���� ���� ������ completion_key, overlapped i/o pointer, number of byte ��
// - completion_key�� Ŭ�� ��ü�� �����ͳ� id�� index�� �Ѵ�.
	client_info() {}
	client_info(int _id, short _x, short _y, SOCKET _sock, bool _connected) {
		id = _id; x = _x; y = _y; sock = _sock; connected = _connected;
	}
	// ## overlapped ����ü
	// ��� send, recv�� overlapped ����ü�� �ʿ��ϴ�.
	// �ϳ��� ����ü�� ���ÿ� ���� ȣ�⿡�� ����ϴ� ���� �Ұ���
	// ���� �� recv ȣ���� ������ �� ��, send ȣ���� ���ÿ� ������ ����
	// - recv ȣ��� overlapped ����ü �� ���� ��� �����ϴ� ���� �ٶ���
	// - send ���۵� ���� ���� �ʿ�
	// - send ���� ������ �����Ƿ� new/delete ���
	// - ������ ���ؼ��� ���� pool�� ����� ����
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
	// atomic���� ���� ���� ������ ��� �����忡�� ��Ű�� �Ѵ�.
	atomic_bool connected = false;
	atomic_bool is_active;
	unsigned char* m_packet_start; // ��Ŷ ���� ��ġ
	unsigned char* m_recv_start; // recv ���� ��ġ
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