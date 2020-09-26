#include <iostream>
#include <WS2tcpip.h>
#include <stdlib.h>
#include "protocol.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")

constexpr int BUF_SIZE = 1024;
constexpr short PORT = 3500;

int clientPtX = 0;
int clientPtY = 0;

char	packet_buffer[BUF_SIZE] = "";
char send_buffer[BUF_SIZE + 1] = "";
char recv_buffer[BUF_SIZE + 1] = "";

WSABUF send_wsabuf;
WSABUF recv_wsabuf;

SOCKET clientSocket;

void error_disp(const char* msg, int err_no);
void ProcessPacket(char* packet);

int main()
{
	wcout.imbue(std::locale("korean"));
	WSADATA WSAdata;
	WSAStartup(MAKEWORD(2, 0), &WSAdata);
	SOCKET serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	::bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
	listen(serverSocket, SOMAXCONN);

	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	while (true) {
		SOCKADDR_IN clientAddress;
		INT a_size = sizeof(clientAddress);
		clientSocket = WSAAccept(serverSocket, (sockaddr*)&clientAddress, &a_size, NULL, NULL);
		if (SOCKET_ERROR == clientSocket)
			error_disp("WSAAccept", WSAGetLastError());
		cout << "New client accepted.\n";
		while (true) {
			DWORD num_recv;
			DWORD flag = 0;
			WSARecv(clientSocket, &recv_wsabuf, 1, &num_recv, &flag, NULL, NULL);
			if (0 == num_recv)
				break; // 클라이언트 종료 처리
			memcpy(packet_buffer, recv_wsabuf.buf, num_recv);
			cout << "Received " << num_recv << "Bytes\n";
			ProcessPacket(packet_buffer); // 패킷 처리
		}
		cout << "Client connection closed.\n";
		closesocket(clientSocket);
	}
	closesocket(serverSocket);
	WSACleanup();


}

void ProcessPacket(char* packet) {
	cs_packet_up* p = reinterpret_cast<cs_packet_up*>(packet);
	int x = clientPtX;
	int y = clientPtY;

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
	clientPtX = x;
	clientPtY = y;

	sc_packet_move_player* movePacket = reinterpret_cast<sc_packet_move_player*>(send_buffer);
	movePacket->size = sizeof(movePacket);
	movePacket->type = SC_MOVEPLAYER;
	movePacket->x = clientPtX;
	movePacket->y = clientPtY;

	send_wsabuf.buf = send_buffer;

	DWORD num_send;
	int ret = WSASend(clientSocket, &send_wsabuf, 1, &num_send, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while sending packet [%d]", error_code);
		system("pause");
		exit(-1);
	}
	else {
		cout << "Sent Type[" << SC_MOVEPLAYER << "] Player's X[" << clientPtX << "] Player's Y[" << clientPtY << "]\n";
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

