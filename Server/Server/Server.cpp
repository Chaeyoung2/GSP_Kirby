#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include "protocol.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")

constexpr int KEY_SERVER = 1000000; // 클라이언트 아이디와 헷갈리지 않게 큰 값으로

constexpr char OP_MODE_RECV = 0;
constexpr char OP_MODE_SEND = 1;
constexpr char OP_MODE_ACCEPT = 2;

constexpr int VIEW_LIMIT = 3;

int cur_playerCnt = 0;

struct OVER_EX {
	WSAOVERLAPPED wsa_over;
	char op_mode;
	WSABUF wsa_buf;
	unsigned char iocp_buf[MAX_BUFFER];
};


struct client_info {
	client_info() {}
	client_info(int _id, short _x, short _y, SOCKET _sock, bool _connected) {
		id = _id; x = _x; y = _y; sock = _sock; connected = _connected;
	}
	OVER_EX m_recv_over;
	mutex c_lock;
	int id;
	char name[MAX_ID_LEN];
	short x, y;
	SOCKET sock;
	bool connected = false;
	//char oType;
	unsigned char* m_packet_start;
	unsigned char* m_recv_start;
	mutex vl;
	unordered_set<int> view_list;
	int move_time;
};

mutex id_lock;//아이디를 넣어줄때 상호배제해줄 락킹
client_info g_clients[MAX_USER];
HANDLE h_iocp;
SOCKET g_listenSocket;
OVER_EX g_accept_over; // accept용 overlapped 구조체

void error_display(const char* msg, int err_no);
void ProcessPacket(int id);
void ProcessRecv(int id, DWORD iosize);
void ProcessMove(int id, char dir);
void WorkerThread();
void AddNewClient(SOCKET ns);
void DisconnectClient(int id);
void SendPacket(int id, void* p);
void SendLeavePacket(int to_id, int id);
void SendLoginOK(int id);
void SendEnterPacket(int to_id, int new_id);
void SendMovePacket(int to_id, int id);
bool IsNear(int p1, int p2);


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

	vector <thread> workerthreads;
	for (int i = 0; i < 6; ++i) {
		workerthreads.emplace_back(WorkerThread);
	}
	for (auto& t : workerthreads)
		t.join();

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
		SendLoginOK(id);
		for (int i = 0; i < MAX_USER; ++i) {
			if (false == g_clients[i].connected) continue;
			if (id == i) continue;
			if (false == IsNear(i, id)) continue;
			if (0 == g_clients[i].view_list.count(id)) {
				g_clients[i].view_list.insert(id);
				SendEnterPacket(i, id);
			}
			if (0 == g_clients[id].view_list.count(i)) {
				g_clients[id].view_list.insert(i);
				SendEnterPacket(id, i);
			}
		}
	}
	break;
	case CS_MOVE:
	{
		cs_packet_move* p = reinterpret_cast<cs_packet_move*>(g_clients[id].m_packet_start);
		g_clients[id].move_time = p->move_time;
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
	int left_data = next_recv_ptr - g_clients[id].m_packet_start;
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
	g_clients[id].m_recv_over.wsa_buf.len = MAX_BUFFER - (next_recv_ptr - g_clients[id].m_recv_over.iocp_buf); // 남아 있는 버퍼 용량
	g_clients[id].c_lock.lock();
	if (true == g_clients[id].connected)
		WSARecv(g_clients[id].sock, &g_clients[id].m_recv_over.wsa_buf, 1, NULL, &recv_flag, &g_clients[id].m_recv_over.wsa_over, NULL);
	g_clients[id].c_lock.unlock();
}

void ProcessMove(int id, char dir)
{
	short y = g_clients[id].y;
	short x = g_clients[id].x;

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

	// 이동 전 뷰 리스트
	unordered_set<int> old_viewlist = g_clients[id].view_list; 

	g_clients[id].x = x;
	g_clients[id].y = y;

	// 나에게 알림
	SendMovePacket(id, id);

	// a
	//for (int i = 0; i < MAX_USER; ++i) {
	//	if (false == g_clients[i].in_use) continue;
	//	if (i == id) continue;
	//	SendMovePacket(i, id);
	//}
	// ----------------------- 시야 처리 구현하면 주석 해제할 것.. 위에 a 코드 지워도 됨

	// 뷰 리스트 처리
	//// 이동 후 객체-객체가 서로 보이나 안 보이나 처리
	unordered_set<int> new_viewlist; 
	for (int i = 0; i < MAX_USER; ++i) {
		if (id == i) continue;
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i)) { // i가 id와 가깝다면
			new_viewlist.insert(i); // 뉴 뷰리스트에 i를 넣는다
		}
	}
	// 시야 처리 (3차 시도)-------------------------------
	for (int ob : new_viewlist) { // 이동 후 new viewlist (시야에 들어온) 객체에 대해 처리 - enter할 건지, move만 할 건지
		if (0 == old_viewlist.count(ob)) { // 이동 전에 시야에 없었던 경우
			g_clients[id].view_list.insert(ob); // 이동한 클라이언트(id)의 실제 뷰리스트에 넣는다
			SendEnterPacket(id, ob); // 이동한 클라이언트(id)에게 시야에 들어온 ob를 enter하게 한다
			
			if (0 == g_clients[ob].view_list.count(id)) { // 시야에 들어온 ob의 뷰리스트에 id가 없었다면
				g_clients[ob].view_list.insert(id); // 시야에 들어온 ob의 뷰리스트에 id를 넣어주고
				SendEnterPacket(ob, id); // ob에게 id를 enter하게 한다
			}
			else {// 시야에 들어온 ob의 뷰리스트에 id가 있었다면 
				SendMovePacket(ob, id); // ob에게 id를 move하게 한다
			}
		}
		else {  // 이동 전에 시야에 있었던 경우
			if (0 != g_clients[ob].view_list.count(id)) { // 시야에 들어온 ob가 id뷰리스트에 원래 있었ㄷ면
				SendMovePacket(ob, id); // ob에게 id move 패킷 보냄
			}
			else // id가 ob의 뷰리스트에 없었다면
			{ 
				g_clients[ob].view_list.insert(id); // ob의 뷰리스트에 id 넣어줌 
				SendEnterPacket(ob, id); // ob에게 id를 enter하게 한다
			}
		}
	}
	for (int ob : old_viewlist) { // 이동 후 old viewlist 안의 객체에 대해 처리 - leave 처리 할 건지, 유지할 건지
		if (0 == new_viewlist.count(ob)) { // 이동 후 old viewlist에는 있는데 new viewlist에 없다 -> 시야에서 사라졌다
			g_clients[id].view_list.erase(ob); // 이동한 id의 뷰리스트에서 ob를 삭제
			SendLeavePacket(id, ob); // id가 ob를 leave 하게끔
			if (0 != g_clients[ob].view_list.count(id)) { // ob의 뷰리스트에 id가 있었다면
				g_clients[ob].view_list.erase(id); // ob의 뷰리스트에서도 id를 삭제
				SendLeavePacket(ob, id); // ob가 id를 leave 하게끔
			}
		}
	}
}
	//// 시야 처리 (2차 시도)-----------------------------------
	//// 1. old vl에 있고 new vl에도 있는 객체 // 서로가 
	//for (auto pl : old_viewlist) {
	//	if (0 == new_viewlist.count(pl)) continue;
	//	if (0 < g_clients[pl].view_list.count(id)) {
	//		SendMovePacket(pl, id);
	//	}
	//	else {
	//		g_clients[pl].view_list.insert(id);
	//		SendEnterPacket(pl, id);
	//	}
	//}
	//// 2. old vl에 없고 new_vl에만 있는 객체
	//for (auto pl : new_viewlist) {
	//	if (0 < old_viewlist.count(pl)) continue;
	//	g_clients[id].view_list.insert(pl);
	//	SendEnterPacket(id, pl);
	//	if (0 == g_clients[pl].view_list.count(id)) {
	//		g_clients[pl].view_list.insert(id);
	//		SendEnterPacket(pl, id);
	//	}
	//	else {
	//		SendMovePacket(pl, id);
	//	}
	//}
	//// 3. old vl에 있고 new vl에는 없는 플레이어
	//for (auto pl : old_viewlist) {
	//	if (0 < new_viewlist.count(pl)) continue;
	//	g_clients[id].view_list.erase(pl);
	//	SendLeavePacket(id, pl);
	//	// SendLeavePacket(pl, id);
	//	if (0 < g_clients[pl].view_list.count(id)) {
	//		g_clients[pl].view_list.erase(id);
	//		SendLeavePacket(pl, id);
	//	}
	//}


	
	// 시야 처리 (1차 시도) --------------------------------
	////// 시야에 들어온 객체 처리
	//for (int ob : new_viewlist) { // 뉴 뷰리스트에 있는 객체 ob를 갖다가
	//	if (0 == old_viewlist.count(ob)) { // id의 올드 뷰리스트에 ob가 있었다면
	//		g_clients[id].view_list.insert(ob);
	//		SendEnterPacket(id, ob);

	//		if (0 == g_clients[ob].view_list.count(id)) { // 내가 시야에 없다면
	//			g_clients[ob].view_list.insert(id);
	//			SendEnterPacket(ob, id);
	//		}
	//		else { // 있다면
	//		   // 이미 상대방 뷰 리스트에 있으니까 무브 패킷만 보내면 됨
	//			SendMovePacket(ob, id);
	//		}

	//	}
	//	else { // 이전에 시야에 있었고, 이동 후에도 시야에 있는 객체
	//	   // 계속 시야에 있었기 때문에 내가 가진 정보는 변함이 없음
	//	   // 멀티 쓰레드라서
	//		if (0 != g_clients[ob].view_list.count(id)) {
	//			SendMovePacket(ob, id);
	//		}
	//		else {
	//			g_clients[ob].view_list.insert(id);
	//			SendEnterPacket(ob, id);
	//		}
	//	}
	//}

	//for (int ob : old_viewlist) { // 이전에는 시야에 있었는데, 이동 후에 없는
	//	if (0 == new_viewlist.count(ob)) {
	//		g_clients[id].view_list.erase(id);
	//		SendLeavePacket(id, ob);
	//		if (0 != g_clients[ob].view_list.count(id)) {
	//			g_clients[ob].view_list.erase(id);
	//			SendLeavePacket(ob, id);
	//		}
	//		else {} //다른 애가 이미 지웠네 ㄸㅋ!

	//	}
	//}

	// 나한테만 알리지 말고 다른 플레이어들도 알게! 널리널리
	//for (int i = 0; i < MAX_USER; ++i) {
	//	if (true == g_clients[i].connected) {
	//		SendMovePacket(i, id);
	//	}
	//}


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
			error_display("GQCS Error : ", WSAGetLastError());
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
			delete over_ex;
			break;
		}
	}
}

void AddNewClient(SOCKET ns)
{
	// 안 쓰는 id 찾기
	int i;
	id_lock.lock();
	for (i = 0; i < MAX_USER; ++i) {
		if (false == g_clients[i].connected) break;
	}
	id_lock.unlock();
	// id 다 차면 close
	if (MAX_USER == i) {
#ifdef DEBUG
		cout << "Max user limit exceeded\n";
#endif
		closesocket(ns);
	}
	// 공간 남아 있으면
	else {
		// g_clients 배열에 정보 넣기
		g_clients[i].c_lock.lock();
		g_clients[i].id = i;
		g_clients[i].connected = true;
		g_clients[i].sock = ns;
		g_clients[i].name[0] = 0; // null string으로 초기화 필수
		g_clients[i].c_lock.unlock();
		g_clients[i].m_packet_start = g_clients[i].m_recv_over.iocp_buf;
		g_clients[i].m_recv_over.op_mode = OP_MODE_RECV;
		g_clients[i].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(g_clients[i].m_recv_over.iocp_buf); // 실제 버퍼의 주소 가리킴
		g_clients[i].m_recv_over.wsa_buf.len = sizeof(g_clients[i].m_recv_over.iocp_buf);
		ZeroMemory(&g_clients[i].m_recv_over.wsa_over, sizeof(g_clients[i].m_recv_over.wsa_over));
		g_clients[i].m_recv_start = g_clients[i].m_recv_over.iocp_buf; // 이 위치에서 받기 시작한다.
		g_clients[i].x = rand() % WORLD_WIDTH;
		g_clients[i].y = rand() % WORLD_HEIGHT;
		// 소켓을 iocp에 등록
		DWORD flags = 0;
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(ns), h_iocp, i, 0); // 소켓을 iocp에 등록
		// Recv
		g_clients[i].c_lock.lock();
		int ret = 0;
		if (true == g_clients[i].connected) {
			ret = WSARecv(g_clients[i].sock, &g_clients[i].m_recv_over.wsa_buf, 1, NULL, &flags, &(g_clients[i].m_recv_over.wsa_over), NULL);
		}
		g_clients[i].c_lock.unlock();
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
	g_clients[new_id].c_lock.lock();
	strcpy_s(p.name, g_clients[new_id].name);
	g_clients[new_id].c_lock.unlock();
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

bool IsNear(int p1, int p2)
{
	int dist = (g_clients[p1].x - g_clients[p2].x) * (g_clients[p1].x - g_clients[p2].x);
	dist += (g_clients[p1].y - g_clients[p2].y) * (g_clients[p1].y - g_clients[p2].y);
	return dist <= VIEW_LIMIT * VIEW_LIMIT;
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

