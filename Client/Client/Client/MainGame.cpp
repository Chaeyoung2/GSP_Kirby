#include "framework.h"
#include "MainGame.h"
#include "KeyMgr.h"


MainGame::MainGame()
{
}

MainGame::~MainGame()
{
}

void MainGame::Start()
{
	hdc = GetDC(g_hwnd);
	LoadBitmaps();

	setObjectPoint();
	setObjectRect();

	InitNetwork();

	pKeyMgr = new KeyMgr;
}

void MainGame::Update()
{
	InputKeyState();
}

void MainGame::Render()
{
	HDC hdcMain = GetDC(g_hwnd);
	HDC hdcBuffer = CreateCompatibleDC(hdcMain);
	HDC hdcBackGround = CreateCompatibleDC(hdcMain);
	HDC hdcPlayer = CreateCompatibleDC(hdcMain);

	SelectObject(hdcBuffer, hBitmapBackBuffer);
	SelectObject(hdcBackGround, hBitmapBackGround);
	SelectObject(hdcPlayer, player.bitmap);

	// render BackGround at BackBuffer
	BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcBackGround, 0, 0, SRCCOPY);
	// render Line
	for (int i = 0; i < 8; i++) {
		MoveToEx(hdcBuffer, TILESIZE * (i+1), 0, NULL);
		LineTo(hdcBuffer, TILESIZE * (i+1), WINCX);
		MoveToEx(hdcBuffer, 0, TILESIZE * (i+1), NULL);
		LineTo(hdcBuffer, WINCX, TILESIZE*(i+1));
	}
	// render Player
	TransparentBlt(hdcBuffer, /// 이미지 출력할 위치 핸들
		player.rect.left, player.rect.top, /// 이미지를 출력할 위치 x,y
		PLAYERCX, PLAYERCY, /// 출력할 이미지의 너비, 높이
		hdcPlayer, /// 이미지 핸들
		0, 0, /// 가져올 이미지의 시작지점 x,y 
		30, 30, /// 원본 이미지로부터 잘라낼 이미지의 너비,높이
		RGB(0, 255, 255) /// 투명하게 할 색상
	);
	// render BackBuffer at MainDC (Double Buffering)
	BitBlt(hdcMain, 0, 0, WINCX, WINCY, hdcBuffer, 0, 0, SRCCOPY);

	DeleteDC(hdcPlayer);
	DeleteDC(hdcBackGround);
	DeleteDC(hdcBuffer);

	ReleaseDC(g_hwnd, hdcMain);
}


void MainGame::InputKeyState()
{
	int x = 0, y = 0;
	if (pKeyMgr->OnceKeyUp(VK_UP)) {
		y -= 1;
	}
	else if (pKeyMgr->OnceKeyUp(VK_DOWN)) {
		y += 1;
	}
	else if (pKeyMgr->OnceKeyUp(VK_LEFT)) {
		x -= 1;
	}
	else if (pKeyMgr->OnceKeyUp(VK_RIGHT)) {
		x += 1;
	}

	// 이때 최초 한번 서버에 보냄.
	cs_packet_up *myPacket = reinterpret_cast<cs_packet_up*>(send_buffer);
	myPacket->size = sizeof(myPacket);
	send_wsabuf.len = sizeof(myPacket);

	if (0 != x) {
		if (1 == x) myPacket->type = CS_INPUTRIGHT;
		else myPacket->type = CS_INPUTLEFT;
		DWORD num_sent;
		int ret = WSASend(serverSocket, &send_wsabuf, 1, &num_sent, 0, NULL, NULL);
		cout << "Sent " << send_wsabuf.len << "Bytes\n";
		if (ret) {
			int error_code = WSAGetLastError();
			printf("Error while sending packet [%d]", error_code);
		}
		ReadPacket();
	}
	if (0 != y) {
		if (1 == y) myPacket->type = CS_INPUTDOWN;
		else myPacket->type = CS_INPUTUP;
		DWORD num_sent;
		int ret = WSASend(serverSocket, &send_wsabuf, 1, &num_sent, 0, NULL, NULL);
		cout << "Sent " << send_wsabuf.len << "Bytes\n";
		if (ret) {
			int error_code = WSAGetLastError();
			printf("Error while sending packet [%d]", error_code);
		}
		ReadPacket();
	}

}

void MainGame::setObjectPoint()
{
	// 체스판에 맞게 // point에 따라서 위치 설정
	player.x = player.ptX * TILESIZE + TILESIZE * 0.5f;
	player.y = player.ptY * TILESIZE + TILESIZE * 0.5f;
}

void MainGame::setObjectRect()
{
	// rect 설정해 두면 TranasparentBlt 할 때 유용함
	player.rect.left = long(player.x - PLAYERCX * 0.5f);
	player.rect.right = long(player.x + PLAYERCX * 0.5f);
	player.rect.top = long(player.y - PLAYERCY * 0.5f);
	player.rect.bottom = long(player.y + PLAYERCY * 0.5f);
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
		hBitmapBackBuffer = tempBitmap;

	// BackGround
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackGround.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // 비트맵 로드 방법2
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/BackGround.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		hBitmapBackGround = tempBitmap;

	// Player
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Kirby.bmp", IMAGE_BITMAP, 150, 90, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Kirby.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		player.bitmap = tempBitmap;
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
	}

	// WSAAsyncSelect(serverSocket, g_hwnd, WM_SOCKET, FD_CLOSE | FD_READ); // 윈도우 메시지로도 안읽어와지네.. 왜지/..
	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;
}

void MainGame::ReadPacket()
{
	DWORD num_recv;
	DWORD flag = 0;

	//while (true) {
		int ret = WSARecv(serverSocket, &recv_wsabuf, 1, &num_recv, &flag, NULL, NULL);
		if (ret) {
			int err_code = WSAGetLastError();
			printf("Recv Error [%d]\n", err_code);
		}
//		cout << "Received " << num_recv << "Bytes [ " << recv_wsabuf.buf << "]\n";
	//}
		else {
			BYTE* ptr = reinterpret_cast<BYTE*>(recv_wsabuf.buf);
			int packet_size = ptr[0];
			int packet_type = ptr[1];
			if (packet_type == SC_MOVEPLAYER) {
				int ptX = ptr[2];
				int ptY = ptr[3];

				player.ptX = ptX;
				player.ptY = ptY;
				setObjectPoint();
				setObjectRect();

				cout << "Recv Type[" << SC_MOVEPLAYER << "] Player's X[" << ptX << "] Player's Y[" << ptY << "]\n";

			}
		}
}


void MainGame::Release()
{
	DeleteObject(hBitmapBackGround);
	DeleteObject(hBitmapBackBuffer);
	DeleteObject(player.bitmap);

	delete pKeyMgr;
	
	closesocket(serverSocket);
	WSACleanup();
}
