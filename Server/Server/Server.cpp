#include <iostream>
#include <WS2tcpip.h>
#include <stdlib.h>
#include "protocol.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")

constexpr int KEY_SERVER = 1000000; // 클라이언트 아이디와 헷갈리지 않게 큰 값으로

int cur_playerCnt = 0;



struct client_info {
	client_info() {}
	client_info(int _id, short _x, short _y, SOCKET _sock, bool _connected) {
		id = _id; x = _x; y = _y; sock = _sock; connected = _connected;
	}
	WSAOVERLAPPED overlapped;
	int id;
	char name[MAX_ID_LEN];
	short x, y;
	SOCKET sock;
	bool connected = false;
	char oType;
	WSABUF send_wsabuf;
	WSABUF recv_wsabuf;
	char send_buffer[MAX_BUFFER] = "";
	char recv_buffer[MAX_BUFFER] = "";
};

client_info g_clients[MAX_USER];

void error_disp(const char* msg, int err_no);
void ProcessPacket(char* packet, LPWSAOVERLAPPED over, DWORD bytes);

void CALLBACK recv_complete(DWORD err, DWORD bytes, LPWSAOVERLAPPED over, DWORD flags);
void CALLBACK send_complete(DWORD err, DWORD bytes, LPWSAOVERLAPPED over, DWORD flags);

int main()
{
	wcout.imbue(std::locale("korean"));
	WSADATA WSAdata;
	WSAStartup(MAKEWORD(2, 0), &WSAdata);
	SOCKET serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	::bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
	listen(serverSocket, MAX_USER);


	SOCKADDR_IN clientAddress;

	while (true) {
		int a_size = sizeof(clientAddress);
		SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &a_size);
		if (SOCKET_ERROR == clientSocket)
			error_disp("accept", WSAGetLastError());
		cout << "New client accepted. [" << cur_playerCnt << "]\n";
		// 플레이어 배열에 새 플레이어 추가
		client_info c_info= client_info{cur_playerCnt, 3, 0, clientSocket, true };
		c_info.overlapped.hEvent = (HANDLE)clientSocket; // 여기서 소켓 정보를 미리 받아놓고 나중에 recvCallback에서 사용할 것임
		g_clients[cur_playerCnt++] = c_info;
		// 접속한 플레이어에게 아이디, 초기 위치 부여 패킷 send + 최초 recv
		sc_packet_login_ok* loginPacket = reinterpret_cast<sc_packet_login_ok*>(g_clients[c_info.id].send_buffer);
		loginPacket->id = c_info.id;
		loginPacket->x = c_info.x;
		loginPacket->y = c_info.y;
		loginPacket->size = sizeof(sc_packet_login_ok);
		loginPacket->type = SC_LOGIN_OK;
		g_clients[c_info.id].send_wsabuf.buf = g_clients[c_info.id].send_buffer;
		g_clients[c_info.id].send_wsabuf.len = sizeof(sc_packet_login_ok);
		ZeroMemory(&(g_clients[c_info.id].overlapped), sizeof(WSAOVERLAPPED));
		int ret = WSASend(g_clients[c_info.id].sock, &(g_clients[c_info.id].send_wsabuf), 1, NULL, NULL, &(g_clients[c_info.id].overlapped), send_complete);
		if (ret) {
			int error_code = WSAGetLastError();
			printf("Error while sending packet [%d]", error_code);
			system("pause");
			exit(-1);
		}
		else {
			cout << "Sent sc_packet_login_ok To players[" << c_info.id << "]\n";
		}
		DWORD flags = 0;
		g_clients[c_info.id].recv_wsabuf.buf = g_clients[c_info.id].recv_buffer;
		g_clients[c_info.id].recv_wsabuf.len = MAX_BUFFER;
		ret = WSARecv(g_clients[c_info.id].sock, &(g_clients[c_info.id].recv_wsabuf), 1, NULL, &flags, &(g_clients[c_info.id].overlapped), recv_complete);
		if (ret) {
			int error_code = WSAGetLastError();
			if (error_code != 997)
				printf("Error while receving packet [%d]", error_code);
			//system("pause");
			//exit(-1);
		}
		else {
			cout << "Recv players[" << c_info.id << "]\n";
		}
		//다른 플레이어에게 접속한 플레이어 정보 send 
		for (int i = 0; i < MAX_USER; ++i) {
			if (g_clients[i].connected == false) continue;
			if (i == c_info.id) continue;
			sc_packet_enter* enterPacket = reinterpret_cast<sc_packet_enter*>(g_clients[i].send_buffer);
			enterPacket->size = sizeof(sc_packet_enter);
			enterPacket->id = g_clients[c_info.id].id;
			memcpy(&(enterPacket->name), &(g_clients[c_info.id].name), strlen(g_clients[c_info.id].name));
			enterPacket->o_type = g_clients[c_info.id].oType;
			enterPacket->type = SC_ENTER;
			enterPacket->x = g_clients[c_info.id].x;
			enterPacket->y = g_clients[c_info.id].y;

			g_clients[i].send_wsabuf.len = sizeof(sc_packet_enter);
			int ret = WSASend(g_clients[i].sock, &(g_clients[i].send_wsabuf), 1, NULL, NULL, &(g_clients[i].overlapped), send_complete);
			if (ret) {
				int error_code = WSAGetLastError();
				printf("Error while sending packet [%d]\n", error_code);
				system("pause");
				exit(-1);
			}
			else {
				cout << "Sent sc_packet_enter To players[" << i << "], this packet's id is " << c_info.id << "]\n";
			}
		}
		// 접속한 플레이어에게 다른 플레이어 정보 send
		for (int i = 0; i < MAX_USER; ++i) {
			if (g_clients[i].connected == false) continue;
			if (i == c_info.id) continue;
			sc_packet_enter* enterPacket = reinterpret_cast<sc_packet_enter*>(g_clients[c_info.id].send_buffer);
			enterPacket->size = sizeof(sc_packet_enter);
			enterPacket->id = g_clients[i].id;
			memcpy(&(enterPacket->name), &(g_clients[i].name), strlen(g_clients[i].name));
			enterPacket->o_type = g_clients[i].oType;
			enterPacket->type = SC_ENTER;
			enterPacket->x = g_clients[i].x;
			enterPacket->y = g_clients[i].y;

			g_clients[c_info.id].send_wsabuf.len = sizeof(sc_packet_enter);
			int ret = WSASend(g_clients[c_info.id].sock, &(g_clients[c_info.id].send_wsabuf), 1, NULL, NULL, &(g_clients[c_info.id].overlapped), send_complete);
			if (ret) {
				int error_code = WSAGetLastError();
				printf("Error while sending packet [%d]", error_code);
				system("pause");
				exit(-1);
			}
			else {
				cout << "Sent sc_packet_enter To players[" << c_info.id << "], this packet's id is " << i << "]\n";
			}
		}

		g_clients[c_info.id].recv_wsabuf.buf = g_clients[c_info.id].recv_buffer;
		g_clients[c_info.id].recv_wsabuf.len = MAX_BUFFER;
		flags = 0;
		int recvBytes = WSARecv(clientSocket, &(g_clients[c_info.id].recv_wsabuf), 1, NULL, &flags, &(g_clients[c_info.id].overlapped), recv_complete);
	}
	closesocket(serverSocket);
	WSACleanup();


}

void ProcessPacket(char* packet, LPWSAOVERLAPPED over, DWORD bytes) {
	cs_packet_up* p = reinterpret_cast<cs_packet_up*>(packet);
	int id = p->id;
	int x = g_clients[id].x;
	int y = g_clients[id].y;

	switch (p->type) {
	case CS_INPUTRIGHT:
		if (x < TILEMAX)
			x++;
		break;
	case CS_INPUTLEFT:
		if (x > 0)
			x--;
		break;
	case CS_INPUTUP:
		if (y > 0)
			y--;
		break;
	case CS_INPUTDOWN:
		if (y < TILEMAX)
			y++;
		break;
	}
	g_clients[id].x = x;
	g_clients[id].y = y;


	// 움직인 플레이어 정보 보내기 (나를 포함한 모든 플레이어들에게)
	for (int i = 0; i < MAX_USER; ++i) {
		if (g_clients[i].connected == false) continue;
		sc_packet_move_player* movePacket = reinterpret_cast<sc_packet_move_player*>(g_clients[i].send_buffer);
		movePacket->size = sizeof(sc_packet_move_player);
		movePacket->type = SC_MOVEPLAYER;
		movePacket->x = g_clients[id].x;
		movePacket->y = g_clients[id].y;
		movePacket->id = id;
		g_clients[i].send_wsabuf.len = sizeof(sc_packet_move_player);
		ZeroMemory(&(g_clients[i].overlapped), sizeof(g_clients[i].overlapped));
		int ret = WSASend(g_clients[i].sock, &(g_clients[i].send_wsabuf), 1, NULL, NULL, &(g_clients[i].overlapped), send_complete);
		if (ret) {
			int error_code = WSAGetLastError();
			printf("Error while sending packet [%d]", error_code);
			system("pause");
			exit(-1);
		}
		else {
			cout << "Sent sc_packet_move_player : Player[" << id << "] 's X(" << g_clients[id].x << "), Y(" << g_clients[id].y << ")\n";
		}
	}
}

void CALLBACK recv_complete(DWORD err, DWORD bytes, LPWSAOVERLAPPED over, DWORD flags) {
	client_info* clientinfo = (struct client_info*)over;
	SOCKET socket = reinterpret_cast<int>(over->hEvent); // 아래에서 over가 지워진다ㅠㅠ
	volatile int id = clientinfo->id;
	if (bytes > 0) {
		ProcessPacket(g_clients[id].recv_buffer, over, bytes);
		g_clients[id].recv_buffer[bytes] = 0;
		// cout << "TRACE - Receive message : " << recv_buffer << "(" << bytes << " bytes)\n";
		// 다시 recv
		//ZeroMemory(over, sizeof(*over));
		if (WSARecv(g_clients[id].sock, &(g_clients[id].recv_wsabuf), 1, NULL, &flags, over, recv_complete)) {
			int error_code = WSAGetLastError();
			if(error_code != 997)
				printf("Error while sending packet [%d]", error_code);
		}
	}
	else {
		// 클라이언트에서 접속을 끊었다
		closesocket(g_clients[id].sock);
		return;
	}

}


void CALLBACK send_complete(DWORD err, DWORD bytes, LPWSAOVERLAPPED over, DWORD flags) {
	client_info* clientinfo = (struct client_info*)over;
	SOCKET socket = reinterpret_cast<int>(over->hEvent);
	if (bytes > 0) {
		//printf("TRACE - Send message : %s (%d bytes)\n", send_buffer, bytes);
	}
	else {
		closesocket(socket);
		return;
	}

}


void error_disp(const char* msg, int err_no) {
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

