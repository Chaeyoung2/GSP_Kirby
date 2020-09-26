#include <iostream>
#include <WS2tcpip.h>
#include <stdlib.h>
#include "protocol.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")

constexpr short PORT = 3500;

int clientPtX = 0;
int clientPtY = 0;

char send_buffer[MAX_BUFFER]= "";
char recv_buffer[MAX_BUFFER] = "";

WSABUF send_wsabuf;
WSABUF recv_wsabuf;

SOCKET clientSocket;

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
	serverAddress.sin_port = htons(PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	::bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
	listen(serverSocket, 10);

	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = MAX_BUFFER;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = MAX_BUFFER;

	SOCKADDR_IN clientAddress;
	WSAOVERLAPPED overlapped;

	while (true) {
		INT a_size = sizeof(clientAddress);
		clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &a_size);
		if (SOCKET_ERROR == clientSocket)
			error_disp("accept", WSAGetLastError());
		cout << "New client accepted.\n";
		recv_wsabuf.buf = recv_buffer;
		recv_wsabuf.len = MAX_BUFFER;
		DWORD flags = 0;
		ZeroMemory(&overlapped, sizeof(overlapped));
		int recvBytes = WSARecv(clientSocket, &recv_wsabuf, 1, NULL, &flags, &overlapped, recv_complete);
	}
	closesocket(serverSocket);
	WSACleanup();


}

void ProcessPacket(char* packet, LPWSAOVERLAPPED over, DWORD bytes) {
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

	// 받은 만큼 다시 보내기
	send_wsabuf.len = bytes;
	ZeroMemory(over, sizeof(*over));
	int ret = WSASend(clientSocket, &send_wsabuf, 1, NULL, NULL, over, send_complete);
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

void CALLBACK recv_complete(DWORD err, DWORD bytes, LPWSAOVERLAPPED over, DWORD flags) {
	if (bytes > 0) {
		ProcessPacket(recv_buffer, over, bytes);
		recv_buffer[bytes] = 0;
		// cout << "TRACE - Receive message : " << recv_buffer << "(" << bytes << " bytes)\n";
	}
	else {
		// 클라이언트에서 접속을 끊었다
		closesocket(clientSocket);
		return;
	}
}


void CALLBACK send_complete(DWORD err, DWORD bytes, LPWSAOVERLAPPED over, DWORD flags) {
	if (bytes > 0) {
		//printf("TRACE - Send message : %s (%d bytes)\n", send_buffer, bytes);
	}
	else {
		closesocket(clientSocket);
		return;
	}

	// 다시 recv
	recv_wsabuf.len = MAX_BUFFER; // 다시 얼마나 받을지 모르므로
	ZeroMemory(over, sizeof(*over));
	int ret = WSARecv(clientSocket, &recv_wsabuf, 1, NULL, &flags, over, recv_complete);
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

