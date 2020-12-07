#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include <queue>
#include "protocol.h"

extern "C" {
#include "include/lua.h"
#include "include/lauxlib.h"
#include "include/lualib.h"
}

using namespace std;
using namespace chrono;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")

constexpr int KEY_SERVER = 1000000; // 클라이언트 아이디와 헷갈리지 않게 큰 값으로

constexpr char OP_MODE_RECV = 0;
constexpr char OP_MODE_SEND = 1;
constexpr char OP_MODE_ACCEPT = 2;
constexpr char OP_RANDOM_MOVE = 3;
constexpr char OP_PLAYER_MOVE_NOTIFY = 4;

struct OVER_EX {
	WSAOVERLAPPED wsa_over;
	char op_mode;
	WSABUF wsa_buf;
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
	lua_State* L;
	SOCKET sock = -1;
	atomic_bool connected = false;
	atomic_bool is_active;
	//char oType;
	unsigned char* m_packet_start;
	unsigned char* m_recv_start;
	//mutex vl;
	unordered_set<int> view_list;
	int move_time;
	short sx, sy;
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

mutex id_lock;//아이디를 넣어줄때 상호배제해줄 락킹
client_info g_clients[MAX_USER+NUM_NPC];
HANDLE h_iocp;
SOCKET g_listenSocket;
OVER_EX g_accept_over; // accept용 overlapped 구조체
priority_queue<event_type> timer_queue;
mutex timer_l;
mutex sector_l;

#define S_SIZE 50
unordered_set<int> g_sector[S_SIZE][S_SIZE];

void error_display(const char* msg, int err_no);

void InitializeNPC();
void RandomMoveNPC(int id);
void WakeUpNPC(int id);

void AddTimer(int obj_id, int ev_type, system_clock::time_point t);

void ProcessPacket(int id);
void ProcessRecv(int id, DWORD iosize);
void ProcessMove(int id, char dir);

void WorkerThread();
void TimerThread();

void AddNewClient(SOCKET ns);
void DisconnectClient(int id);

void SendPacket(int id, void* p);
void SendLeavePacket(int to_id, int id);
void SendLoginOK(int id);
void SendEnterPacket(int to_id, int new_id);
void SendMovePacket(int to_id, int id);
void SendChatPacket(int to_client, int id, char* mess);

bool IsNear(int p1, int p2);
bool IsNPC(int p1);

int API_SendMessage(lua_State* L);
int API_get_y(lua_State* L);
int API_get_x(lua_State* L);

int main()
{
	for (auto& cl : g_clients) {
		cl.connected = false;
	}

	wcout.imbue(std::locale("korean"));

	WSADATA WSAdata;
	WSAStartup(MAKEWORD(2, 0), &WSAdata);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	g_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_listenSocket), h_iocp, KEY_SERVER, 0); // iocp에 리슨 소켓 등록

	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(SOCKADDR_IN));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	::bind(g_listenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
	listen(g_listenSocket, 5);

	// Accept
	SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_accept_over.op_mode = OP_MODE_ACCEPT;
	g_accept_over.wsa_buf.len = static_cast<int>(cSocket); // 같은 integer끼리 그냥.. 넣어줌.... ??
	ZeroMemory(&g_accept_over.wsa_over, sizeof(WSAOVERLAPPED));
	AcceptEx(g_listenSocket, cSocket, g_accept_over.iocp_buf, 0, 32, 32, NULL, &g_accept_over.wsa_over); // accept ex의 데이터 영역 모자라서 클라이언트가 접속 못하는 문제 생겼음 (1006)

	// NPC 정보 세팅
	InitializeNPC();

	// timer thread 생성
	thread timer_thread{ TimerThread };
	// worker thread 생성
	vector <thread> workerthreads;
	for (int i = 0; i < 6; ++i) {
		workerthreads.emplace_back(WorkerThread);
	}
	for (auto& t : workerthreads)
		t.join();
	timer_thread.join();

	closesocket(g_listenSocket);
	WSACleanup();
	return 0;
}


void ProcessPacket(int id)
{
	char p_type = g_clients[id].m_packet_start[1]; // 패킷 타입
	switch (p_type) {
	case CS_LOGIN:
	{
		cs_packet_login* p = reinterpret_cast<cs_packet_login*>(g_clients[id].m_packet_start);
		g_clients[id].c_lock.lock();
		strcpy_s(g_clients[id].name, p->name);
		g_clients[id].c_lock.unlock();
		//char cl_name[256] = "";
		//strcpy_s(cl_name, g_clients[id].name);
		//if (true == atomic_compare_exchange_strong(
		//	reinterpret_cast<volatile atomic_int*>(g_clients[id].name),
		//	reinterpret_cast<int*>(cl_name),
		//	reinterpret_cast<atomic_char>(p->name) ) {
		//}
		SendLoginOK(id);
		// Players
		for (int i = 0; i < MAX_USER; ++i) {
			if (false == g_clients[i].connected) continue;
			if (id == i) continue;
			if (false == IsNear(i, id)) continue;
			g_clients[i].c_lock.lock();
			if (0 == g_clients[i].view_list.count(id)) {
				g_clients[i].view_list.insert(id);
				g_clients[i].c_lock.unlock();
				SendEnterPacket(i, id);
			}
			else
				g_clients[i].c_lock.unlock();
			g_clients[id].c_lock.lock();
			if (0 == g_clients[id].view_list.count(i)) {
				g_clients[id].view_list.insert(i);
				g_clients[id].c_lock.unlock();
				SendEnterPacket(id, i);
			}
			else {
				g_clients[id].c_lock.unlock();
			}
		}
		// NPC
		for (int i = MAX_USER; i < MAX_USER + NUM_NPC; ++i) {
			if (false == IsNear(id, i)) continue;
			g_clients[id].c_lock.lock();
			g_clients[id].view_list.insert(i);
			g_clients[id].c_lock.unlock();
			SendEnterPacket(id, i);
			WakeUpNPC(i);
		}
	}
	break;
	case CS_MOVE:
	{
		g_clients[id].c_lock.lock();
		cs_packet_move* p = reinterpret_cast<cs_packet_move*>(g_clients[id].m_packet_start);
		g_clients[id].move_time = p->move_time;
		g_clients[id].c_lock.unlock();
		ProcessMove(id, p->direction);
	}
	break;
	default:
	{
#ifdef DEBUG
		cout << "Unkown Packet type[" << p_type << "] from Client [" << id << "]\n";
#endif
		while (true);
	}
	break;
	}

}

void ProcessRecv(int id, DWORD iosize)
{
	// 패킷 조립.
	unsigned char p_size = g_clients[id].m_packet_start[0]; // 패킷 사이즈
	unsigned char* next_recv_ptr = g_clients[id].m_recv_start + iosize; // 다음에 받을 ptr
	while (p_size <= next_recv_ptr - g_clients[id].m_packet_start) { // 조립
		ProcessPacket(id); // 패킷 처리
		g_clients[id].m_packet_start += p_size; // 패킷 처리했으니 다음 패킷 주소로
		if (g_clients[id].m_packet_start < next_recv_ptr)
			p_size = g_clients[id].m_packet_start[0]; // 다음 패킷의 사이즈를 p_size로 업데이트
		else
			break;
	}
	// 패킷 조립하고 이만큼 남았음.
	long long left_data = next_recv_ptr - g_clients[id].m_packet_start;
	// 버퍼를 다 썼으면 초기화해야 함. (앞으로 밀어버린다)
	if ((MAX_BUFFER - (next_recv_ptr - g_clients[id].m_recv_over.iocp_buf) < MIN_BUFFER)) { // 버퍼가 MIN_BUFFER 크기보다 작으면
		memcpy(g_clients[id].m_recv_over.iocp_buf, g_clients[id].m_packet_start, left_data); // 남아 있는 바이트만큼 copy 
		g_clients[id].m_packet_start = g_clients[id].m_recv_over.iocp_buf; 
		next_recv_ptr = g_clients[id].m_recv_start + left_data; // 다음에 받을 위치 가리킴
	}
	DWORD recv_flag = 0;

	// 어디서부터 다시 받을지
	g_clients[id].m_recv_start = next_recv_ptr;
	g_clients[id].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(next_recv_ptr);
	g_clients[id].m_recv_over.wsa_buf.len = MAX_BUFFER - static_cast<int>(next_recv_ptr - g_clients[id].m_recv_over.iocp_buf); // 남아 있는 버퍼 용량
	// 최적화
	bool connected = true;
	if (true == g_clients[id].connected.compare_exchange_strong(connected, true)) {
		WSARecv(g_clients[id].sock, &g_clients[id].m_recv_over.wsa_buf, 1, NULL, &recv_flag, &g_clients[id].m_recv_over.wsa_over, NULL);
	}
	// 최적화 전
	//g_clients[id].c_lock.lock();
	//if (true == g_clients[id].connected)
	//	WSARecv(g_clients[id].sock, &g_clients[id].m_recv_over.wsa_buf, 1, NULL, &recv_flag, &g_clients[id].m_recv_over.wsa_over, NULL);
	//g_clients[id].c_lock.unlock();
}

void ProcessMove(int id, char dir)
{
	// 이동 전 좌표
	short sx = g_clients[id].sx;
	short sy = g_clients[id].sy;
	short y = g_clients[id].y;
	short x = g_clients[id].x;
	// 이동 전 뷰 리스트
	unordered_set<int> old_viewlist = g_clients[id].view_list;
	// 이동
	switch (dir) {
	case MV_UP:
		if (y > 0) y--;
		break;
	case MV_DOWN:
		if (y < WORLD_HEIGHT - 1) y++;
		break;
	case MV_LEFT:
		if (x > 0) x--;
		break;
	case MV_RIGHT:
		if (x < WORLD_WIDTH - 1) x++;
		break;
	default:
#ifdef DEBUG
		cout << "Unknown Direction in CS_MOVE packet\n";
#endif
		while (true);
		break;
	}

	// 이동 후 좌표
	g_clients[id].c_lock.lock();
	int cur_x = g_clients[id].x = x;
	int cur_y = g_clients[id].y = y;
	g_clients[id].c_lock.unlock();
	int cur_sx = cur_x / S_SIZE;
	int cur_sy = cur_y / S_SIZE;
	// 이동 후 섹터
	// sx, sy와 cur_sx, cur_sy가 동일하다면 -> 그대로
	//											다르면 -> 이전 섹터에서 erase, 새로운 섹터에 insert
	if (sx != cur_sx || sy != cur_sy) {
		sector_l.lock();
		g_sector[sx][sy].erase(id);
		g_sector[cur_sx][cur_sy].insert(id);
		sector_l.unlock();
	}

	// 나에게 알림
	SendMovePacket(id, id);

	// 뷰 리스트 처리
	//// 이동 후 객체-객체가 서로 보이나 안 보이나 처리
	unordered_set<int> new_viewlist; 
	//for (int i = 0; i < MAX_USER; ++i) {
	for(auto i : g_sector[cur_sx][cur_sy]){
		if (id == i) continue;
		//g_clients[i].c_lock.lock();
		if (false == g_clients[i].connected) continue;
		//g_clients[i].c_lock.unlock();
		if (true == IsNear(id, i)) { // i가 id와 가깝다면
			new_viewlist.insert(i); // 뉴 뷰리스트에 i를 넣는다
		}
	}
//	for (int i = MAX_USER; i < MAX_USER + NUM_NPC; ++i) {
	for(auto i : g_sector[cur_sx][cur_sy]){
		if (false == IsNPC(i)) continue;
		if (true == IsNear(id, i)) {
			new_viewlist.insert(i);
			WakeUpNPC(i);
		}
	}

	// 시야 처리 (3차 시도)-------------------------------
	for (int ob : new_viewlist) { // 이동 후 new viewlist (시야에 들어온) 객체에 대해 처리 - enter할 건지, move만 할 건지
		if (0 == old_viewlist.count(ob)) { // 이동 전에 시야에 없었던 경우
			g_clients[id].c_lock.lock();
			g_clients[id].view_list.insert(ob); // 이동한 클라이언트(id)의 실제 뷰리스트에 넣는다
			g_clients[id].c_lock.unlock();
			SendEnterPacket(id, ob); // 이동한 클라이언트(id)에게 시야에 들어온 ob를 enter하게 한다

			if (false == IsNPC(ob)) {
				if (0 == g_clients[ob].view_list.count(id)) { // 시야에 들어온 ob의 뷰리스트에 id가 없었다면
					g_clients[ob].c_lock.lock();
					g_clients[ob].view_list.insert(id); // 시야에 들어온 ob의 뷰리스트에 id를 넣어주고
					g_clients[ob].c_lock.unlock();
					SendEnterPacket(ob, id); // ob에게 id를 enter하게 한다
				}
				else {// 시야에 들어온 ob의 뷰리스트에 id가 있었다면 
					//g_clients[ob].c_lock.unlock();
					SendMovePacket(ob, id); // ob에게 id를 move하게 한다
				}
			}
		}
		else {  // 이동 전에 시야에 있었던 경우
			if (false == IsNPC(ob)) {
				//g_clients[ob].c_lock.lock();
				if (0 != g_clients[ob].view_list.count(id)) { // 시야에 들어온 ob가 id뷰리스트에 원래 있었ㄷ면
					//g_clients[ob].c_lock.unlock();
					SendMovePacket(ob, id); // ob에게 id move 패킷 보냄
				}
				else // id가 ob의 뷰리스트에 없었다면
				{
					//g_clients[ob].c_lock.unlock();
					g_clients[ob].c_lock.lock();
					g_clients[ob].view_list.insert(id); // ob의 뷰리스트에 id 넣어줌 
					g_clients[ob].c_lock.unlock();
					SendEnterPacket(ob, id); // ob에게 id를 enter하게 한다
				}
			}
		}
	}
	for (int ob : old_viewlist) { // 이동 후 old viewlist 안의 객체에 대해 처리 - leave 처리 할 건지, 유지할 건지
		if (0 == new_viewlist.count(ob)) { // 이동 후 old viewlist에는 있는데 new viewlist에 없다 -> 시야에서 사라졌다
			g_clients[id].c_lock.lock();
			g_clients[id].view_list.erase(ob); // 이동한 id의 뷰리스트에서 ob를 삭제
			g_clients[id].c_lock.unlock();
			SendLeavePacket(id, ob); // id가 ob를 leave 하게끔
			if (false == IsNPC(ob)) {
				//g_clients[ob].c_lock.lock();
				if (0 != g_clients[ob].view_list.count(id)) { // ob의 뷰리스트에 id가 있었다면
					g_clients[ob].c_lock.lock();
					g_clients[ob].view_list.erase(id); // ob의 뷰리스트에서도 id를 삭제
					g_clients[ob].c_lock.unlock();
					SendLeavePacket(ob, id); // ob가 id를 leave 하게끔
				}
				//else
					//g_clients[ob].c_lock.unlock();
			}
		}
	}

	// 주위 npc에게 player event를 pqcs로 보낸다
	if (true == IsNPC(id)) { 
		for (auto& npc : new_viewlist) {
			if (false == IsNPC(npc)) continue;
			OVER_EX* ex_over = new OVER_EX;
			ex_over->object_id = id;
			ex_over->op_mode = OP_PLAYER_MOVE_NOTIFY;
			PostQueuedCompletionStatus(h_iocp, 1, npc, &ex_over->wsa_over);
		}
	}
}

void WorkerThread() {
	// 반복
	// - 쓰레드를 iocp thread pool에 등록 (GQCS)
	// - iocp가 처리를 맡긴 i/o 완료
	// - 데이터 처리 꺼내기 q
	// - 꺼낸 i/o 완료, 데이터 처리
	while (true) {
		DWORD io_size;
		ULONG_PTR iocp_key;
		int key;
		WSAOVERLAPPED* lpover;
		int ret = GetQueuedCompletionStatus(h_iocp, &io_size, &iocp_key, &lpover, INFINITE);
		key = static_cast<int>(iocp_key);
#ifdef DEBUG
		cout << "Completion Detected\n";
#endif
		if (FALSE == ret) { // max user exceed 문제 방지. // 0이 나오면 진행하지 않는다.
			int error_no = WSAGetLastError();
			if (64 == error_no) { // 
				DisconnectClient(key);
				continue;
			}
			else 
				error_display("GQCS Error : ", error_no);
		}
		OVER_EX* over_ex = reinterpret_cast<OVER_EX*>(lpover);
		switch (over_ex->op_mode) {
		case OP_MODE_ACCEPT: 
		{ // 새 클라이언트 accept
			AddNewClient(static_cast<SOCKET>(over_ex->wsa_buf.len)); 
		}
		break;
		case OP_MODE_RECV:
		{
			if (io_size == 0) { // 클라이언트 접속 종료
				DisconnectClient(key);
			}
			else { // 패킷 처리
#ifdef DEBUG
				cout << "Packet from Client [" << key << "]\n";
#endif
				ProcessRecv(key, io_size);
			}
		}
		break;
		case OP_MODE_SEND:
		{
			delete over_ex;
		}
		break;
		case OP_RANDOM_MOVE:
		{
			RandomMoveNPC(key);
		}
		break;
		case OP_PLAYER_MOVE_NOTIFY:
		{
			//	x 다름 y 다름, x 같음 y 다름, x 다름 y 같음
			g_clients[key].c_lock.lock();
			lua_getglobal(g_clients[key].L, "event_player_move");
			lua_pushnumber(g_clients[key].L, over_ex->object_id);
			lua_pcall(g_clients[key].L, 1, 1, 0);
			g_clients[key].c_lock.unlock();
			delete over_ex;
		}
		break;
		}
	}
}

void TimerThread()
{
	while (true) {
		while (true) {
			if (false == timer_queue.empty()) {
				event_type ev = timer_queue.top();
				if (ev.wakeup_time > system_clock::now()) break;
				timer_l.lock();
				timer_queue.pop();
				timer_l.unlock();

				if (ev.event_id == OP_RANDOM_MOVE) {
					//RandomMoveNPC(ev.obj_id);
					//AddTimer(ev.obj_id, OP_RANDOM_MOVE, system_clock::now() + 1s);
					OVER_EX* ex_over = new OVER_EX;
					ex_over->op_mode = OP_RANDOM_MOVE;
					PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ex_over->wsa_over);
				}
			}
			else break;
		}
		this_thread::sleep_for(1ms);
	}
}

void AddNewClient(SOCKET ns)
{
	// 안 쓰는 id 찾기
	//// 최적화 전 ----
	//int i;
	//id_lock.lock();
	//for (i = 0; i < MAX_USER; ++i) {
	//	if (false == g_clients[i].connected) 
	//		break;
	//}
	//id_lock.unlock();
	// 최적화 2 ----
	int i = -1;
	int compare_val = -1;
	for (int j = 0; j < MAX_USER; ++j) {
		if (false == g_clients[j].connected) {
			atomic_compare_exchange_strong(
				reinterpret_cast<atomic_int*>(&i), &compare_val, j);
			break;
		}
	}
	// id 다 차면 close
	if (MAX_USER == i) {
		cout << "Max user limit exceeded\n";
		closesocket(ns);
	}
	// 공간 남아 있으면
	else {
		// g_clients 배열에 정보 넣기
		// 최적화 전 ----
		//g_clients[i].c_lock.lock();
		//g_clients[i].id = i;
		//g_clients[i].connected = true;
		//g_clients[i].sock = ns;
		//g_clients[i].name[0] = 0; // null string으로 초기화 필수
		//g_clients[i].c_lock.unlock();
		// 최적화 후 ----
		int i_compare_val = -1;
		bool b_compare_val = false;
		char c_compare_val = 0;
		SOCKET u_compare_val = -1;
		atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&g_clients[i].id), &i_compare_val, i);
		atomic_compare_exchange_strong(reinterpret_cast<atomic_bool*>(&g_clients[i].connected), &b_compare_val, true);
		atomic_compare_exchange_strong(reinterpret_cast<atomic_uintptr_t*>(&g_clients[i].sock), &u_compare_val, ns);
		atomic_compare_exchange_strong(reinterpret_cast<atomic_char*>(&(g_clients[i].name[0])), &c_compare_val, 0);

		g_clients[i].m_packet_start = g_clients[i].m_recv_over.iocp_buf;
		g_clients[i].m_recv_over.op_mode = OP_MODE_RECV;
		g_clients[i].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(g_clients[i].m_recv_over.iocp_buf); // 실제 버퍼의 주소 가리킴
		g_clients[i].m_recv_over.wsa_buf.len = sizeof(g_clients[i].m_recv_over.iocp_buf);
		ZeroMemory(&g_clients[i].m_recv_over.wsa_over, sizeof(g_clients[i].m_recv_over.wsa_over));
		g_clients[i].m_recv_start = g_clients[i].m_recv_over.iocp_buf; // 이 위치에서 받기 시작한다.
		g_clients[i].c_lock.lock();
		int x = g_clients[i].x = rand() % WORLD_WIDTH;
		int y = g_clients[i].y = rand() % WORLD_HEIGHT;
		// 섹터
		int sx = g_clients[i].sx = x / S_SIZE;
		int sy = g_clients[i].sy = y / S_SIZE;
		g_clients[i].c_lock.unlock();
		// 섹터에 정보 넣기
		sector_l.lock();
		g_sector[sx][sy].insert(i);
		sector_l.unlock();
		// 소켓을 iocp에 등록
		DWORD flags = 0;
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(ns), h_iocp, i, 0); // 소켓을 iocp에 등록
		// Recv
		// 최적화 전
		g_clients[i].c_lock.lock();
		int ret = 0;
		if (true == g_clients[i].connected) {
			ret = WSARecv(g_clients[i].sock, &g_clients[i].m_recv_over.wsa_buf, 1, NULL, &flags, &(g_clients[i].m_recv_over.wsa_over), NULL);
		}
		g_clients[i].c_lock.unlock();
		// 최적화 후
		//int ret = 0;
		//bool connected = true;
		//if (true == atomic_compare_exchange_strong(&g_clients[i].connected, &connected, true)) {
		//	WSARecv(g_clients[i].sock, &g_clients[i].m_recv_over.wsa_buf, 1, NULL, 0, &(g_clients[i].m_recv_over.wsa_over), NULL);
		//}
		if (ret == SOCKET_ERROR) {
			int error_no = WSAGetLastError();
			if (error_no != ERROR_IO_PENDING) {
				error_display("WSARecv : ", error_no);
			}
		}
	}
	// 다시 accept
	SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_accept_over.op_mode = OP_MODE_ACCEPT;
	g_accept_over.wsa_buf.len = static_cast<ULONG>(cSocket); // 같은 integer끼리 그냥.. 넣어줌.... ??
	ZeroMemory(&g_accept_over.wsa_over, sizeof(g_accept_over.wsa_over));// accept overlapped 구조체 사용 전 초기화
	AcceptEx(g_listenSocket, cSocket, g_accept_over.iocp_buf, 0, 32, 32, NULL, &g_accept_over.wsa_over);
}

void DisconnectClient(int id)
{
	// 주변 클라들에게 지우라고 알림
	for (int i = 0; i < MAX_USER; ++i) {
		if (true == g_clients[i].connected) {
			if (i != id) {
				if (0 != g_clients[i].view_list.count(id)) {// 뷰리스트에 있는지 확인하고 지움
					g_clients[i].view_list.erase(id);
					SendLeavePacket(i, id);
				}
//				else { // 없으면 지울 필요도 없고 패킷 보낼 필요도 없음

//				}
			}
		}
	}

	g_clients[id].c_lock.lock();
	g_clients[id].connected = false;
	g_clients[id].view_list.clear();
	closesocket(g_clients[id].sock);
	g_clients[id].sock = 0;
	g_clients[id].c_lock.unlock();
	// 최적화
	//int i_compare_val = 0;
	//bool b_compare_val = true;
	//if (true == atomic_compare_exchange_strong(&g_clients[id].connected, &b_compare_val, false)) {
	//	g_clients[id].view_list.clear();
	//	closesocket(g_clients[id].sock);
	//	g_clients[id].sock = 0;
	//}

}

void SendPacket(int id, void* p)
{
	unsigned char* packet = reinterpret_cast<unsigned char*>(p);
	OVER_EX* send_over = new OVER_EX;
	memcpy(&send_over->iocp_buf, packet, packet[0]);
	send_over->op_mode = OP_MODE_SEND;
	send_over->wsa_buf.buf = reinterpret_cast<CHAR*>(send_over->iocp_buf);
	send_over->wsa_buf.len = packet[0];
	ZeroMemory(&send_over->wsa_over, sizeof(send_over->wsa_over));
	//클라이언트의 소켓에 접근하므로 lock
	g_clients[id].c_lock.lock();
	if (true == g_clients[id].connected) // use 중이므로 sock이 살아있다
		WSASend(g_clients[id].sock, &send_over->wsa_buf, 1, NULL, 0, &send_over->wsa_over, NULL);
	g_clients[id].c_lock.unlock();
	//// 최적화
	//bool connected = true;
	//if (true == atomic_compare_exchange_strong(&g_clients[id].connected, &connected, true)) {
	//	WSASend(g_clients[id].sock, &send_over->wsa_buf, 1, NULL, 0, &send_over->wsa_over, NULL);
	//}
}

void SendLeavePacket(int to_id, int id)
{
	sc_packet_leave packet;
	packet.id = id;
	packet.size = sizeof(packet);
	packet.type = SC_LEAVE;
	SendPacket(to_id, &packet);
}

void SendLoginOK(int id)
{
	sc_packet_login_ok p;
	p.exp = 0;
	p.hp = 100;
	p.id = id;
	p.level = 1;
	p.size = sizeof(p);
	p.type = SC_LOGIN_OK;
	p.x = g_clients[id].x;
	p.y = g_clients[id].y;
	SendPacket(id, &p); // p를 보내면 struct가 몽땅 카피돼서 보내짐
}

void SendEnterPacket(int to_id, int new_id)
{
	sc_packet_enter p;
	p.id = new_id;
	p.size = sizeof(p);
	p.type = SC_ENTER;
	p.x = g_clients[new_id].x;
	p.y = g_clients[new_id].y;
	//g_clients[new_id].c_lock.lock();
	strcpy_s(p.name, g_clients[new_id].name);
	//g_clients[new_id].c_lock.unlock();
	p.o_type = 0;
	SendPacket(to_id, &p);
}

void SendMovePacket(int to_id, int id)
{
	sc_packet_move p;
	p.id = id;
	p.size = sizeof(p);
	p.type = SC_MOVEPLAYER;
	p.x = g_clients[id].x;
	p.y = g_clients[id].y;
	p.move_time = g_clients[id].move_time;
	SendPacket(to_id, &p);
}

void SendChatPacket(int to_client, int id, char* mess)
{
	sc_packet_chat p;
	p.id = id;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	strcpy_s(p.message, mess);
	SendPacket(to_client, &p);
}

bool IsNear(int p1, int p2)
{
	int dist = (g_clients[p1].x - g_clients[p2].x) * (g_clients[p1].x - g_clients[p2].x);
	dist += (g_clients[p1].y - g_clients[p2].y) * (g_clients[p1].y - g_clients[p2].y);
	return dist <= VIEW_LIMIT * VIEW_LIMIT;
}

bool IsNPC(int p1)
{
	return p1 >= MAX_USER;
}

void error_display(const char* msg, int err_no) {
	WCHAR* h_mess;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&h_mess, 0, NULL);
	std::cout << msg;
	std::wcout << L"에러 =>" << h_mess << std::endl;
	while (true);
	LocalFree(h_mess);
}

void InitializeNPC()
{
//#ifdef _DEBUG
	cout << "Initializing NPCs\n";
//#endif
	for(int i = MAX_USER; i < MAX_USER + NUM_NPC; ++i) {
		int x = g_clients[i].x = rand() % WORLD_WIDTH;
		int y = g_clients[i].y = rand() % WORLD_HEIGHT;
		// 섹터
		int sx = g_clients[i].sx = x / S_SIZE;
		int sy = g_clients[i].sy = y / S_SIZE;
		// 섹터에 정보 넣기
		sector_l.lock();
		g_sector[sx][sy].insert(i);
		sector_l.unlock();
		char npc_name[50];
		sprintf_s(npc_name, "NPC%d", i);
		g_clients[i].c_lock.lock();
		strcpy_s(g_clients[i].name, npc_name);
		g_clients[i].is_active = false;
		g_clients[i].c_lock.unlock();
		// 가상 머신 생성
		lua_State* L = g_clients[i].L = luaL_newstate();
		luaL_openlibs(L);
		int error = luaL_loadfile(L, "monster.lua");
		error = lua_pcall(L, 0, 0, 0);
		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 1, 0);
		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
//#ifdef _DEBUG
	cout << "Initializing NPCs finishied.\n";
//#endif
}

void RandomMoveNPC(int id)
{
	// 이동 전 좌표
	int x = g_clients[id].x;
	int y = g_clients[id].y;
	int sx = x / S_SIZE;
	int sy = y / S_SIZE;
	// 이동 전 viewlist
	unordered_set<int> o_vl;
	//for (int i = 0; i < MAX_USER; ++i) {
	for (auto i : g_sector[sx][sy]) {
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i)) 
			o_vl.insert(i);
	}
	// 이동
	switch (rand() % 4) {
	case 0:
		if (x > 0)
			x--;
		break;
	case 1:
		if (x < WORLD_WIDTH - 1)
			x++;
		break;
	case 2:
		if (y > 0)
			y--;
		break;
	case 4:
		if (y < WORLD_HEIGHT - 1)
			y++;
		break;
	}
	// 이동 후 좌표
	int cur_x = g_clients[id].x = x;
	int cur_y = g_clients[id].y = y;
	int cur_sx = cur_x / S_SIZE;
	int cur_sy = cur_y / S_SIZE;

	// sector 처리 -------------------
	// sx, sy와 cur_sx, cur_sy가 동일하다면 -> 그대로
	//											다르면 -> 이전 섹터에서 erase, 새로운 섹터에 insert
	if (sx != cur_sx || sy != cur_sy) {
		sector_l.lock();
		g_sector[sx][sy].erase(id);
		g_sector[cur_sx][cur_sy].insert(id);
		sector_l.unlock();
	}
	// view list 처리 (sector O) ----------------- - 섹터 안의 뷰리스트
	// 이동 후 viewlist
	unordered_set<int> n_vl;
	for(auto i : g_sector[cur_sx][cur_sy]){
		if (id == i) continue;
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i))
			n_vl.insert(i);
	}
	// 이동 후 처리
	for (auto pl : o_vl) {
		if (0 < n_vl.count(pl)) {
			//g_clients[pl].c_lock.lock();
			if (0 < g_clients[pl].view_list.count(id)) {
				//g_clients[pl].c_lock.unlock();
				SendMovePacket(pl, id);
			}
			else {
				g_clients[pl].c_lock.lock();
				g_clients[pl].view_list.insert(id);
				g_clients[pl].c_lock.unlock();
				//g_clients[pl].c_lock.unlock();
				SendEnterPacket(pl, id);
			}
		}
		else {
			//g_clients[pl].c_lock.lock();
			if (0 < g_clients[pl].view_list.count(id)) {
				g_clients[pl].c_lock.lock();
				g_clients[pl].view_list.erase(id);
				g_clients[pl].c_lock.unlock();
				//g_clients[pl].c_lock.unlock();
				SendLeavePacket(pl, id);
			}
		}
	}
	for (auto pl : n_vl) {
		//g_clients[pl].c_lock.lock();
		if (0 == g_clients[pl].view_list.count(pl)) {
			if (0 == g_clients[pl].view_list.count(id)) {
				g_clients[pl].c_lock.lock();
				g_clients[pl].view_list.insert(id);
				g_clients[pl].c_lock.unlock();
				//g_clients[pl].c_lock.unlock();
				SendEnterPacket(pl, id);
			}
			else {
				//g_clients[pl].c_lock.unlock();
				SendMovePacket(pl, id);
			}
		}
		else {

			//g_clients[pl].c_lock.unlock();
		}
	}

	//// view list 처리 (sector X) ------------------
	//// 이동 후 viewlist
	//unordered_set<int> n_vl;
	//for (int i = 0; i < MAX_USER; ++i) {
	//	if (id == i) continue;
	//	if (false == g_clients[i].connected) continue;
	//	if (true == IsNear(id, i))
	//		n_vl.insert(i);
	//}
	//// 이동 후 처리
	//for (auto pl : o_vl) {
	//	if (0 < n_vl.count(pl)) {
	//		if (0 < g_clients[pl].view_list.count(id))
	//			SendMovePacket(pl, id);
	//		else {
	//			g_clients[pl].view_list.insert(id);
	//			SendEnterPacket(pl, id);
	//		}
	//	}
	//	else {
	//		if (0 < g_clients[pl].view_list.count(id)) {
	//			g_clients[pl].view_list.erase(id);
	//			SendLeavePacket(pl, id);
	//		}
	//	}
	//}
	//for (auto pl : n_vl) {
	//	if (0 == g_clients[pl].view_list.count(pl)) {
	//		if (0 == g_clients[pl].view_list.count(id)) {
	//			g_clients[pl].view_list.insert(id);
	//			SendEnterPacket(pl, id);
	//		}
	//		else {
	//			SendMovePacket(pl, id);
	//		}
	//	}
	//}
	//---------------------------------

	if (true == n_vl.empty()) {
		g_clients[id].is_active = false;
	}
	else{
		AddTimer(id, OP_RANDOM_MOVE, system_clock::now() + 1s);
	}

	for (auto pc : n_vl) {
		OVER_EX* over_ex = new OVER_EX;
		over_ex->object_id = pc;
		over_ex->op_mode = OP_PLAYER_MOVE_NOTIFY;
		PostQueuedCompletionStatus(h_iocp, 1, id, &over_ex->wsa_over);
	}
}

void WakeUpNPC(int id)
{
	bool b = false;
	if (true == g_clients[id].is_active.compare_exchange_strong(b, true))
	{
		AddTimer(id, OP_RANDOM_MOVE, system_clock::now() + 1s);
	}
}

void AddTimer(int obj_id, int ev_type, system_clock::time_point t) {
	event_type ev{ obj_id, t, ev_type };
	timer_l.lock();
	timer_queue.push(ev);
	timer_l.unlock();
}



int API_SendMessage(lua_State* L) {
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 3);

	SendChatPacket(user_id, my_id, mess);
	return 0;
}

int API_get_y(lua_State* L) {
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = g_clients[user_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_get_x(lua_State* L) {
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = g_clients[user_id].x;
	lua_pushnumber(L, x);
	return 1;
}

