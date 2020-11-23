#include "framework.h"
#include "MainGame.h"

mutex pl;
int scrollX = 0;
int scrollY = 0;
int myid = -1;

SOCKET serverSocket;
WSABUF send_wsabuf;
WSABUF recv_wsabuf;
char	packet_buffer[BUF_SIZE];
char 	send_buffer[BUF_SIZE] = "";
char	recv_buffer[BUF_SIZE] = "";
DWORD		in_packet_size = 0;
int		saved_packet_size = 0;
DWORD send_last_time = 0; // 마지막으로 send하고 얼마나 지났는지

void ProcessPacket(char* ptr, MainGame* maingame)
{
	int packet_size = ptr[0];
	int packet_type = ptr[1];
	OBJ* pPlayers = maingame->getPlayers();
	switch (packet_type) {
	case SC_LOGIN_OK:
	{
		sc_packet_login_ok* p = reinterpret_cast<sc_packet_login_ok*>(ptr);
		int packet_id = p->id;
		if (pPlayers[packet_id].connected == true) break;
		short ptX = p->x;
		short ptY = p->y;
		myid = packet_id;
		pl.lock();
		pPlayers[packet_id].id = packet_id;
		pPlayers[packet_id].ptX = ptX;
		pPlayers[packet_id].ptY = ptY;
		pPlayers[packet_id].connected = true;
		HBITMAP* hBitmaps = maingame->getBitmaps();
		pPlayers[packet_id].bitmap = hBitmaps[2];
		maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
		maingame->setObjectRect(packet_id);
		// 로그인 ok 사인 받으면
		int t_id = GetCurrentProcessId();
		char tempBuffer[MAX_NICKNAME] = "";
		sprintf_s(tempBuffer, "P%03d", t_id % 1000);
		pPlayers[packet_id].name = new char[MAX_NICKNAME];
		ZeroMemory(pPlayers[packet_id].name, MAX_NICKNAME);
		memcpy(pPlayers[packet_id].name, tempBuffer, strlen(tempBuffer));
		pl.unlock();
		// 좌표에 따라 스크롤 설정
		maingame->setScroll(ptX, ptY);
	}
	break;
	case SC_MOVEPLAYER:
	{
		sc_packet_move* p = reinterpret_cast<sc_packet_move*>(ptr);
		int packet_id = p->id;
		short ptX = p->x;
		short ptY = p->y;
		pl.lock();
		pPlayers[packet_id].ptX = ptX;
		pPlayers[packet_id].ptY = ptY;
		pl.unlock();
		maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
		maingame->setObjectRect(packet_id);
		if(myid == packet_id)
			maingame->setScroll(ptX, ptY); // 좌표에 따라 스크롤 설정

		// cout << "Recv Type[" << SC_MOVEPLAYER << "] Player's X[" << ptX << "] Player's Y[" << ptY << "]\n";
	}
	break;
	case SC_ENTER:
	{
		sc_packet_enter* packet = reinterpret_cast<sc_packet_enter*>(ptr);
		int packet_id = packet->id;
		pl.lock();
		pPlayers[packet_id].connected = true;
		//pPlayers[packet_id].o_type = packet->o_type;
		//memcpy(pPlayers[packet_id].name, packet->name, strlen(packet->name));
		pPlayers[packet_id].id = packet_id;
		pPlayers[packet_id].ptX = packet->x;
		pPlayers[packet_id].ptY = packet->y;
		HBITMAP* hBitmaps = maingame->getBitmaps();
		pPlayers[packet_id].bitmap = hBitmaps[2];
		pPlayers[packet_id].name = new char[MAX_NICKNAME];
		ZeroMemory(pPlayers[packet_id].name, MAX_NICKNAME);
		memcpy(pPlayers[packet_id].name, packet->name, strlen(packet->name));
		pl.unlock();
		maingame->setObjectPoint(packet_id, packet->x, packet->y);
		maingame->setObjectRect(packet_id);
	}
	break;
	case SC_LEAVE:
	{
		sc_packet_leave* my_packet = reinterpret_cast<sc_packet_leave*>(ptr);
		int packet_id = my_packet->id;
		pl.lock();
		delete pPlayers[packet_id].name; // Enter 할 때 new 해줬었음
		ZeroMemory(&(pPlayers[packet_id]), sizeof(pPlayers[packet_id]));
		pPlayers[packet_id].connected = false;
		pl.unlock();
	}
	break;
	default:
	{
		printf("Unknown Packet\n");
	}
	break;
	}

}
void RecvPacket(LPVOID classPtr) {
	MainGame* maingame = (MainGame*)classPtr;

	DWORD num_recv;
	DWORD flag = 0;

	while (true) {
		int ret = WSARecv(serverSocket, &recv_wsabuf, 1, &num_recv, &flag, NULL, NULL);
		if (ret) {
			int err_code = WSAGetLastError();
			printf("Recv Error [%d]\n", err_code);
			break;
		}
		BYTE* ptr = reinterpret_cast<BYTE*>(recv_buffer);
		while (0 != num_recv) {
			if (0 == in_packet_size)
				in_packet_size = ptr[0];
			if (num_recv + saved_packet_size >= in_packet_size) {
				memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
				ProcessPacket(packet_buffer, maingame);
				ptr += in_packet_size - saved_packet_size;
				num_recv -= in_packet_size - saved_packet_size;
				in_packet_size = 0;
				saved_packet_size = 0;
			}
			else {
				memcpy(packet_buffer + saved_packet_size, ptr, num_recv);
				saved_packet_size += num_recv;
				num_recv = 0;
			}
		}

	}
}


MainGame::MainGame()
{
}

MainGame::~MainGame()
{
	Release();
}

void MainGame::Start()
{
	hdc = GetDC(g_hwnd);
	LoadBitmaps();

	setObjectPoint(0, 0, 0);
	setObjectRect(0);

	InitNetwork();

}

void MainGame::Update()
{
	//InputKeyState();
	send_last_time += 1;
}

void MainGame::Render()
{
	HDC hdcMain = GetDC(g_hwnd);
	HDC hdcBuffer = CreateCompatibleDC(hdcMain);
	HDC hdcBackGround = CreateCompatibleDC(hdcMain);
	HDC hdcPlayer = CreateCompatibleDC(hdcMain);

	SelectObject(hdcBuffer, bitmaps[0]);
	SelectObject(hdcBackGround, bitmaps[1]);
	SelectObject(hdcPlayer, players[myid].bitmap);

	// render BackGround at BackBuffer
	BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcBackGround, 0, 0, SRCCOPY);
	// render Line
	for (int i = 0; i < TILEMAX + 1; i++) {
		MoveToEx(hdcBuffer, TILESIZE * (i) + scrollX, +scrollY, NULL);
		LineTo(hdcBuffer, TILESIZE * (i) + scrollX, TILESIZE * (TILEMAX)+scrollY);
		MoveToEx(hdcBuffer, 0 + scrollX, TILESIZE * (i) + scrollY, NULL);
		LineTo(hdcBuffer, TILESIZE * (TILEMAX)+scrollX, TILESIZE*(i) + scrollY);
	}
	// render Players
	for (int i = 0; i < MAX_USER; ++i) {
		if (players[i].connected == false) continue;
		TransparentBlt(hdcBuffer, /// 이미지 출력할 위치 핸들
			players[i].rect.left + scrollX, players[i].rect.top + scrollY, /// 이미지를 출력할 위치 x,y
			PLAYERCX, PLAYERCY, /// 출력할 이미지의 너비, 높이
			hdcPlayer, /// 이미지 핸들
			0, 0, /// 가져올 이미지의 시작지점 x,y 
			30, 30, /// 원본 이미지로부터 잘라낼 이미지의 너비,높이
			RGB(0, 255, 255) /// 투명하게 할 색상
		);
		// render Text(info)
		if (i == myid) {
			// 내 캐릭터 구분
			TextOut(hdcBuffer, players[i].rect.left + 5 + scrollX, players[i].y +2 + scrollY, L"It's me!", lstrlen(L"It's me"));
		}
		// 플레이어 정보 출력
		TCHAR lpOut[128]; // 좌표
		wsprintf(lpOut, TEXT("(%d, %d)"), (int)(players[i].ptX), (int)(players[i].ptY)); 
		TextOut(hdcBuffer, players[i].rect.left + 10 + scrollX, players[i].y - 25 + scrollY, lpOut, lstrlen(lpOut));
		if (players[i].name != nullptr) {
			TCHAR lpNickname[MAX_NICKNAME];	// 닉네임
			size_t convertedChars = 0;
			size_t newsize = strlen(players[i].name) + 1;
			mbstowcs_s(&convertedChars, lpNickname, newsize, players[i].name, _TRUNCATE);
			TextOut(hdcBuffer, players[i].rect.left + 5 + scrollX, players[i].y - 40 + scrollY, (wchar_t*)lpNickname, lstrlen(lpNickname));
		}
	}
	// render BackBuffer at MainDC (Double Buffering)
	BitBlt(hdcMain, 0, 0, WINCX, WINCY, hdcBuffer, 0, 0, SRCCOPY);

	DeleteDC(hdcPlayer);
	DeleteDC(hdcBackGround);
	DeleteDC(hdcBuffer);

	ReleaseDC(g_hwnd, hdcMain);
}


void MainGame::InputKeyState(int key)
{
	if (send_last_time < 10) return;
	send_last_time = 0;

	int x = 0, y = 0;

	// 0: up, 1:down, 2:left, 3:right
	switch (key) {
	case 0: 
		y -= 1;
		//if (players[myid].ptY >= 0 && players[myid].ptY <= TILEMAX - 1) 
		//	scrollY += 70;
		//else g_scrollY = TILESIZE * players[myid].ptY;
		break;
	case 1: 
		y += 1;
		//if (players[myid].ptY >= 0 && players[myid].ptY <= TILEMAX - 1) 
		//	scrollY -= 70;
		//else g_scrollY = TILESIZE * players[myid].ptY;
		break;
	case 2: 
		x -= 1;
		//if (players[myid].ptX >= 0 && players[myid].ptX <= TILEMAX - 1) 
		//	scrollX += 70;
		//else g_scrollX = TILESIZE * players[myid].ptX;
		break;
	case 3: 
		x += 1;
		//if (players[myid].ptX >= 0 && players[myid].ptX <= TILEMAX - 1) 
		//	scrollX -= 70;
		//else g_scrollX = TILESIZE * players[myid].ptX;
		break;
	}


	// 이때 딱 한번 서버에 보냄.
	cs_packet_move* p = reinterpret_cast<cs_packet_move*>(send_buffer);
	p->size = sizeof(p);
	send_wsabuf.len = sizeof(p);
	DWORD iobyte;
	p->type = CS_MOVE;
	if (0 != x) {
		if (1 == x)
			p->direction = MV_RIGHT;
		else
			p->direction = MV_LEFT;
	}
	else if (0 != y) {
		if (1 == y)
			p->direction = MV_DOWN;
		else
			p->direction = MV_UP;
	}
	int ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while send packet [%d]", error_code);
	}


	//-----------
	//SendPacket(&p);

	//InvalidateRect(g_hwnd, NULL, TRUE);

}


void MainGame::setObjectPoint(int id, float ptX, float ptY)
{
	// 이젠 멀티플레이어니까 배열에 접근 !
	players[id].ptX = ptX;
	players[id].ptY = ptY;
	players[id].x = ptX * TILESIZE + TILESIZE * 0.5f;
	players[id].y = ptY * TILESIZE + TILESIZE * 0.5f;
	//// 체스판에 맞게 // point에 따라서 위치 설정
	//player.x = player.ptX * TILESIZE + TILESIZE * 0.5f;
	//player.y = player.ptY * TILESIZE + TILESIZE * 0.5f;
}

void MainGame::setObjectRect(int id)
{
	// 이젠 멀티플레이어니까 배열에 접근 !
	players[id].rect.left = long(players[id].x - PLAYERCX * 0.5f);
	players[id].rect.right = long(players[id].x + PLAYERCX * 0.5f);
	players[id].rect.top = long(players[id].y - PLAYERCY * 0.5f);
	players[id].rect.bottom = long(players[id].y + PLAYERCY * 0.5f);
	//// rect 설정해 두면 TranasparentBlt 할 때 유용함
	//player.rect.left = long(player.x - PLAYERCX * 0.5f);
	//player.rect.right = long(player.x + PLAYERCX * 0.5f);
	//player.rect.top = long(player.y - PLAYERCY * 0.5f);
	//player.rect.bottom = long(player.y + PLAYERCY * 0.5f);
}

void MainGame::setScroll(int ptX, int ptY)
{

	scrollX = -(ptX * TILESIZE) + (WINCX * 0.5);
	scrollY = -(ptY * TILESIZE) + (WINCY * 0.5);


	// 스크롤 처리
	if (scrollX < WINCX - (TILESIZE * TILEMAX) - (WINCX * 0.5))
		scrollX = WINCX - (TILESIZE * TILEMAX) - (WINCX * 0.5);
	if (scrollX > WINCX * 0.5)
		scrollX = WINCX * 0.5;
	if (scrollY < WINCY - (TILESIZE * TILEMAX) - (WINCY * 0.5))
		scrollY = WINCY - (TILESIZE * TILEMAX) - (WINCY * 0.5);
	if (scrollY > WINCY * 0.5)
		scrollY = WINCY * 0.5;


	cout << "scrollX : " << scrollX << endl;
	cout << "scrollY : " << scrollY << endl;
}



void MainGame::LoadBitmaps()
{
	// BackBuffer
	HBITMAP tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackBuffer.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 비트맵 로드 방법2
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/BackBuffer.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[0] = tempBitmap;

	// BackGround
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackGround.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 비트맵 로드 방법2
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/BackGround.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[1] = tempBitmap;

	// Player
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Kirby.bmp", IMAGE_BITMAP, 150, 90, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Kirby.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[2] = tempBitmap;
}

void MainGame::InitNetwork()
{
	WSADATA WSAdata;
	WSAStartup(MAKEWORD(2, 0), &WSAdata);
	serverSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(SOCKADDR_IN));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, SERVER_IP, &serverAddress.sin_addr);
	if (connect(serverSocket, (sockaddr*)&serverAddress, sizeof(SOCKADDR_IN))) {
		MessageBox(g_hwnd, L"WSAConnect", L"Failed", MB_OK);
		return;
	}

	//WSAAsyncSelect(serverSocket, g_hwnd, WM_SOCKET, FD_CLOSE | FD_READ); // 윈도우 메시지로도 안읽어와지네.. 왜지/..
	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	recvThread = thread{ RecvPacket, this };
	// ReadPacket(); -> 별도 쓰레드에서 받기로

	// 초기에 login 시도 패킷 보냄
	//cs_packet_login login_packet;
	//login_packet.size = sizeof(login_packet);
	//login_packet.type = CS_LOGIN;
	int t_id = GetCurrentProcessId();
	//sprintf_s(login_packet.name, "P%03d", t_id % 1000);

	// 이때 딱 한번 서버에 보냄.
	cs_packet_login* p = reinterpret_cast<cs_packet_login*>(send_buffer);
	p->size = sizeof(p);
	send_wsabuf.len = sizeof(p);
	DWORD iobyte;
	p->type = CS_LOGIN;
	sprintf_s(p->name, "P%03d", t_id % 1000);

	int ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while send packet [%d]", error_code);
	}
	//---
	//SendPacket(&login_packet);

	// 멤버변수 mynickname에 닉네임 설정 (char to wchar_t)
	char tempBuffer[MAX_NICKNAME] = "";
	strcpy_s(tempBuffer, p->name);
	size_t convertedChars = 0;
	size_t newsize = strlen(tempBuffer) + 1;
	mbstowcs_s(&convertedChars, mynickname, newsize, tempBuffer, _TRUNCATE);

	//strcpy_s(mynickname, login_packet.name);
	//mynickname = new wchar_t;
}

void MainGame::SendPacket(void* packet)
{
	char* p = reinterpret_cast<char*>(packet);
	send_wsabuf.buf = p;
	send_wsabuf.len = p[0];
	DWORD num_sent;
	int ret = WSASend(serverSocket, &send_wsabuf, 1, &num_sent, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		//printf("Error while sending packet [%d]", error_code);
	}
}


void MainGame::Release()
{
	DeleteObject(bitmaps[0]);
	DeleteObject(bitmaps[1]);
	DeleteObject(bitmaps[2]);

	
	closesocket(serverSocket);
	WSACleanup();

	recvThread.join();
}
