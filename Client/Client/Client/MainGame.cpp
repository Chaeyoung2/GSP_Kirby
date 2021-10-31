#include "framework.h"
#include "MainGame.h"

mutex pl;
int scrollX = 0;
int scrollY = 0;
int myid = -1;
bool gameover = false;
high_resolution_clock::time_point gameover_timeout;

SOCKET serverSocket;
WSABUF send_wsabuf;
WSABUF recv_wsabuf;
char	packet_buffer[BUF_SIZE];
char 	send_buffer[BUF_SIZE] = "";
char	recv_buffer[BUF_SIZE] = "";
DWORD		in_packet_size = 0;
int		saved_packet_size = 0;
DWORD send_last_time = 0; // ���������� send�ϰ� �󸶳� ��������
list<ALLCHAT> allchatList;

char userID_buffer[MAX_ID_LEN]= "";

void ProcessPacket(char* ptr, MainGame* maingame)
{
	int packet_size = ptr[0];
	int packet_type = ptr[1];
	OBJ* pPlayers = maingame->getPlayers();
	switch (packet_type) {
	case SC_PACKET_LOGIN_OK:
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
		pPlayers[packet_id].exp = p->exp;
		pPlayers[packet_id].hp = p->hp;
		pPlayers[packet_id].level = p->level;
		//HBITMAP* hBitmaps = maingame->getBitmaps();
		//pPlayers[packet_id].bitmap = hBitmaps[2];
		maingame->setObjectPoint(packet_id, (float)ptX, (float)ptY);
		//maingame->setObjectRect(packet_id);
		// �α��� ok ���� ������
		//int t_id = GetCurrentProcessId();
		//char tempBuffer[MAX_ID_LEN] = "";
		//sprintf_s(tempBuffer, "P%03d", t_id % 1000);
		pPlayers[packet_id].name = new char[MAX_ID_LEN];
		ZeroMemory(pPlayers[packet_id].name, MAX_ID_LEN);
		//memcpy(pPlayers[packet_id].name, tempBuffer, strlen(tempBuffer));
		sprintf(pPlayers[packet_id].name, "%ws", g_nicknamebuf);
		pl.unlock();
		// ��ǥ�� ���� ��ũ�� ����
		maingame->setScroll(ptX, ptY);
	}
	break;
	case SC_PACKET_MOVE:
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
		//maingame->setObjectRect(packet_id);
		if(myid == packet_id)
			maingame->setScroll(ptX, ptY); // ��ǥ�� ���� ��ũ�� ����

		// cout << "Recv Type[" << SC_MOVEPLAYER << "] Player's X[" << ptX << "] Player's Y[" << ptY << "]\n";
	}
	break;
	case SC_PACKET_ENTER:
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
			pPlayers[packet_id].name = new char[MAX_ID_LEN];
			pPlayers[packet_id].m_type = packet->o_type;
			ZeroMemory(pPlayers[packet_id].name, MAX_ID_LEN);
			memcpy(pPlayers[packet_id].name, packet->name, strlen(packet->name));
			pl.unlock();
			maingame->setObjectPoint(packet_id, packet->x, packet->y);
		// maingame->setObjectRect(packet_id);
	}
	break;
	case SC_PACKET_LEAVE:
	{
		sc_packet_leave* my_packet = reinterpret_cast<sc_packet_leave*>(ptr);
		int packet_id = my_packet->id;
		if (packet_id == myid) { // ���� ����
			gameover = true;
			gameover_timeout = high_resolution_clock::now() + 5s;
		}
		else {
			pl.lock();
			delete pPlayers[packet_id].name; // Enter �� �� new �������
			ZeroMemory(&(pPlayers[packet_id]), sizeof(pPlayers[packet_id]));
			pPlayers[packet_id].connected = false;
			pl.unlock();
		}
	}
	break;
	case SC_PACKET_CHAT:
	{
		sc_packet_chat* p = reinterpret_cast<sc_packet_chat*>(ptr);
		int id = p->id;
		if (id != -1) {
			WCHAR* wmess;
			int nChars = MultiByteToWideChar(CP_ACP, 0, p->message, -1, NULL, 0);
			wmess = new WCHAR[nChars];
			MultiByteToWideChar(CP_ACP, 0, p->message, -1, (LPWSTR)wmess, nChars);
			wcscpy_s(pPlayers[id].chat_buf, wmess);
			pPlayers[id].timeout = high_resolution_clock::now() + 1s;
		}
		else { // ��ä ä��
			WCHAR* wmess;
			int nChars = MultiByteToWideChar(CP_ACP, 0, p->message, -1, NULL, 0);
			wmess = new WCHAR[nChars];
			MultiByteToWideChar(CP_ACP, 0, p->message, -1, (LPWSTR)wmess, nChars);
			ALLCHAT allchat = {};
			wcscpy_s(allchat.chat_buf, wmess);
			allchat.timeout = high_resolution_clock::now() + 5s;
			allchatList.push_back(allchat);
		}
	}
	break;
	case SC_PACKET_STAT_CHANGE:
	{
		sc_packet_stat_change* p = reinterpret_cast<sc_packet_stat_change*>(ptr);
		int id = p->id;
		pPlayers[id].hp = p->hp;
		pPlayers[id].exp = p->exp;
		pPlayers[id].level = p->level;
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

	while (maingame->threading) {
		int ret = WSARecv(serverSocket, &recv_wsabuf, 1, &num_recv, &flag, NULL, NULL);
		if (ret) {
			int err_code = WSAGetLastError();
			printf("Recv Error [%d]\n", err_code);
			break;
		}
		char* ptr = reinterpret_cast<char*>(recv_buffer);
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
	//setObjectRect(0);

	//InitNetwork();

}

void MainGame::Update()
{
	send_last_time += 1;
}

void MainGame::Render()
{
	HDC hdcMain = GetDC(g_hwnd);
	HDC hdcBuffer = CreateCompatibleDC(hdcMain);
	HDC hdcBackGround = CreateCompatibleDC(hdcMain);
	HDC hdcBullet = CreateCompatibleDC(hdcMain);
	HDC hdcBulletS = CreateCompatibleDC(hdcMain);
	HDC hdcHPPortion = CreateCompatibleDC(hdcMain);
	HDC hdcBUFPortion = CreateCompatibleDC(hdcMain);
	HDC hdcPlayer = CreateCompatibleDC(hdcMain);
	HDC hdcNPC = CreateCompatibleDC(hdcMain);
	HDC hdcObstacle = CreateCompatibleDC(hdcMain);
	HDC hdcGameover = CreateCompatibleDC(hdcMain);
	HDC hdcAgro = CreateCompatibleDC(hdcMain);
	HDC hdcPeace = CreateCompatibleDC(hdcMain);
	HDC hdcMenu = CreateCompatibleDC(hdcMain);

	SelectObject(hdcBuffer, bitmaps[0]);
	SelectObject(hdcBackGround, bitmaps[1]);
	SelectObject(hdcHPPortion, bitmaps[9]);
	SelectObject(hdcBUFPortion, bitmaps[10]);
	SelectObject(hdcBullet, bitmaps[5]);
	SelectObject(hdcBulletS, bitmaps[11]);
	SelectObject(hdcPlayer, bitmaps[2]);
	SelectObject(hdcNPC, bitmaps[3]);
	SelectObject(hdcObstacle, bitmaps[4]);
	SelectObject(hdcGameover, bitmaps[6]);
	SelectObject(hdcAgro, bitmaps[7]);
	SelectObject(hdcPeace, bitmaps[8]);
	SelectObject(hdcMenu, bitmaps[12]);

	int playerptX = 0, playerptY = 0;
	// render BackGround at BackBuffer
	BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcBackGround, 0, 0, SRCCOPY);
	if (gamestart == false) {
		BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcMenu, 0, 0, SRCCOPY);
		// �Է� id ���.
		RECT rc = { 80, WINCY - 130, 180, WINCY - 100 };
		DrawText(hdcBuffer, g_nicknamebuf, wcslen(g_nicknamebuf), &rc, DT_SINGLELINE);
	}
	else {
		if (gameover == false) {
			// render Line
			for (int i = 0; i < TILESCREENMAX + 1; i++) {
				MoveToEx(hdcBuffer, TILESIZE * (i), 0, NULL);
				LineTo(hdcBuffer, TILESIZE * (i), TILESIZE * (TILESCREENMAX));
				MoveToEx(hdcBuffer, 0, TILESIZE * (i), NULL);
				LineTo(hdcBuffer, TILESIZE * (TILESCREENMAX), TILESIZE * (i));
			}
			// render Bullets
			{
				for (auto iter = bulletList.begin(); iter != bulletList.end();) {
					float x = (iter->x) * TILESIZE + TILESIZE * 0.5f;
					float y = (iter->y) * TILESIZE + TILESIZE * 0.5f;
					RECT rc;
					if (iter->type == 0) {
						rc.left = long(x - 30 * 0.5f);
						rc.right = long(x + 30 * 0.5f);
						rc.top = long(y - 30 * 0.5f);
						rc.bottom = long(y + 30 * 0.5f);
						TransparentBlt(hdcBuffer, rc.left + scrollX, rc.top + scrollY, 30, 30, hdcBullet, 0, 0, 30, 30, RGB(0, 255, 255));
					}
					else {
						rc.left = long(x - 240 * 0.5f);
						rc.right = long(x + 240 * 0.5f);
						rc.top = long(y - 240 * 0.5f);
						rc.bottom = long(y + 240 * 0.5f);
						TransparentBlt(hdcBuffer, rc.left + scrollX, rc.top + scrollY, 240, 240, hdcBulletS, 0, 0, 30, 30, RGB(0, 255, 255));
					}
					if (++(iter->cur_time) > iter->timeout) {
						iter = bulletList.erase(iter);
					}
					else
						++iter;
				}
			}
			// render Items
			for (int i = MAX_USER + NUM_NPC + NUM_OBSTACLE; i < MAX_USER + NUM_NPC + NUM_OBSTACLE + NUM_ITEM; ++i) {
				if (players[i].connected == false) continue;
				OBJ obj = players[i];
				short x = players[i].ptX * TILESIZE + TILESIZE * 0.5f;
				short y = players[i].ptY * TILESIZE + TILESIZE * 0.5f;
				RECT rc;
				rc.left = long(x - PLAYERCX * 0.5f);
				rc.right = long(x + PLAYERCX * 0.5f);
				rc.top = long(y - PLAYERCY * 0.5f);
				rc.bottom = long(y + PLAYERCY * 0.5f);
				if (players[i].m_type == OTYPE_ITEM_HP) {
					TransparentBlt(hdcBuffer, /// �̹��� ����� ��ġ �ڵ�
						rc.left + scrollX, rc.top + scrollY, /// �̹����� ����� ��ġ x,y
						30, 30, /// ����� �̹����� �ʺ�, ����
						hdcHPPortion, /// �̹��� �ڵ�
						0, 0, /// ������ �̹����� �������� x,y 
						30, 30, /// ���� �̹����κ��� �߶� �̹����� �ʺ�,����
						RGB(0, 255, 255) /// �����ϰ� �� ����
					);
				}
				else {
					TransparentBlt(hdcBuffer, /// �̹��� ����� ��ġ �ڵ�
						rc.left + scrollX, rc.top + scrollY, /// �̹����� ����� ��ġ x,y
						30, 30, /// ����� �̹����� �ʺ�, ����
						hdcBUFPortion, /// �̹��� �ڵ�
						0, 0, /// ������ �̹����� �������� x,y 
						30, 30, /// ���� �̹����κ��� �߶� �̹����� �ʺ�,����
						RGB(0, 255, 255) /// �����ϰ� �� ����
					);
				}
			}
			// render Players
			for (int i = 0; i < MAX_USER + NUM_NPC + NUM_OBSTACLE + NUM_ITEM; ++i) {
				if (players[i].connected == false) continue;
				OBJ obj = players[i];
				short x = players[i].ptX * TILESIZE + TILESIZE * 0.5f;
				short y = players[i].ptY * TILESIZE + TILESIZE * 0.5f;
				RECT rc;
				rc.left = long(x - PLAYERCX * 0.5f);
				rc.right = long(x + PLAYERCX * 0.5f);
				rc.top = long(y - PLAYERCY * 0.5f);
				rc.bottom = long(y + PLAYERCY * 0.5f);
				if (i < MAX_USER) { // Player
					TransparentBlt(hdcBuffer, rc.left + scrollX, rc.top + scrollY, PLAYERCX, PLAYERCY, hdcPlayer, 0, 0, 30, 30, RGB(0, 255, 255));
					// render Text(info)
					if (i == myid) {
						// �� ĳ���� ����
						//TextOut(hdcBuffer, players[i].rect.left + scrollX, players[i].y + 3 + scrollY, L"It's me!", lstrlen(L"It's me"));
						playerptX = obj.ptX; playerptY = obj.ptY; // �迭 ��� �д°� ����
					}
					// ���� ���
					//TCHAR lpOut[128]; // ��ǥ
					//wsprintf(lpOut, TEXT("(%d, %d)"), (int)(players[i].ptX), (int)(players[i].ptY));
					//TextOut(hdcBuffer, players[i].rect.left + scrollX, players[i].y - 40 + scrollY, lpOut, lstrlen(lpOut));
					if (players[i].name != nullptr) {
						TCHAR lpNickname[MAX_ID_LEN];	// �г���
						size_t convertedChars = 0;
						size_t newsize = strlen(obj.name) + 1;
						mbstowcs_s(&convertedChars, lpNickname, newsize, obj.name, _TRUNCATE);
						TextOut(hdcBuffer, rc.left + scrollX, y - 25 + scrollY, (wchar_t*)lpNickname, lstrlen(lpNickname));
					}
				}
				else if (MAX_USER <= i && i < MAX_USER + NUM_NPC) { // NPC
					switch (players[i].m_type) {
					case 0:
						TransparentBlt(hdcBuffer, rc.left + scrollX, rc.top + scrollY, MON1CX, MON1CY, hdcPeace, 0, 0, 50, 50, RGB(0, 255, 255));
						break;
					case 1:
						TransparentBlt(hdcBuffer, rc.left + scrollX, rc.top + scrollY, MON1CX, MON1CY, hdcNPC, 0, 0, 20, 30, RGB(0, 255, 255));
						break;
					case 2:
						TransparentBlt(hdcBuffer, rc.left + scrollX, rc.top + scrollY, MON1CX, MON1CY, hdcAgro, 0, 0, 30, 30, RGB(0, 255, 255));
						break;
					}
					// ��ǥ ���� ���
					//TCHAR lpOut[128]; 
					//wsprintf(lpOut, TEXT("(%d, %d)"), (int)(players[i].ptX), (int)(players[i].ptY));
					//TextOut(hdcBuffer, players[i].rect.left + 10 + scrollX, players[i].y - 40 + scrollY, lpOut, lstrlen(lpOut));
					if (obj.name != nullptr) {
						TCHAR lpNickname[MAX_ID_LEN];	// �г���
						size_t convertedChars = 0;
						size_t newsize = strlen(obj.name) + 1;
						mbstowcs_s(&convertedChars, lpNickname, newsize, obj.name, _TRUNCATE);
						TextOut(hdcBuffer, rc.left + 5 + scrollX, y - 25 + scrollY, (wchar_t*)lpNickname, lstrlen(lpNickname));
					}
				}
				else if (MAX_USER + NUM_NPC <= i && i < MAX_USER + NUM_NPC + NUM_OBSTACLE) {
					TransparentBlt(hdcBuffer, /// �̹��� ����� ��ġ �ڵ�
						rc.left + scrollX, rc.top + scrollY, /// �̹����� ����� ��ġ x,y
						30, 30, /// ����� �̹����� �ʺ�, ����
						hdcObstacle, /// �̹��� �ڵ�
						0, 0, /// ������ �̹����� �������� x,y 
						30, 30, /// ���� �̹����κ��� �߶� �̹����� �ʺ�,����
						RGB(0, 255, 255) /// �����ϰ� �� ����
					);
				}

				// chat message
				if (high_resolution_clock::now() < obj.timeout) {
					TextOut(hdcBuffer, rc.left + scrollX, y + scrollY, obj.chat_buf, lstrlen(obj.chat_buf));
				}
			}
			// render Coordinates
			int pointsCount = WORLD_WIDTH / 8; // 0, 8, 16, 24, .. 
			for (int i = 0; i < pointsCount; ++i) {
				for (int j = 0; j < pointsCount; ++j) {
					int ptX = j * 8;
					int ptY = i * 8;
					int posX = TILESIZE * ptX /*+ TILESIZE * 0.5*/;
					int posY = TILESIZE * ptY + TILESIZE * 0.25;
					if ((ptX >= playerptX - TILESCREENMAX * 0.5 && ptX <= playerptX + TILESCREENMAX * 0.5)
						&& (ptY >= playerptY - TILESCREENMAX * 0.5 && ptY <= playerptY + TILESCREENMAX * 0.5)) {
						TCHAR lpOut[128]; // ��ǥ
						wsprintf(lpOut, TEXT("(%d, %d)"), ptX, ptY);
						TextOut(hdcBuffer,
							posX + scrollX, posY + scrollY, lpOut, lstrlen(lpOut));
					}
				}
			}


			// render UI
			{
				RECT rc; rc.left = 0; rc.right = WINCX; rc.bottom = 50; rc.top = 0;
				TCHAR lpUI[256];
				wsprintfW(lpUI, TEXT("��LEVEL : %d                 ��HP : %d                 ��EXP : %d                 ��Position : (%d,%d)        "),
					players[myid].level, players[myid].hp, players[myid].exp, players[myid].ptX, players[myid].ptY);
				TextOut(hdcBuffer, rc.left, rc.bottom, (wchar_t*)lpUI, lstrlen(lpUI));
			}

			// reder chat
			{
				int chatY = 20;
				RECT cRect; cRect.left = WINCX - 600; cRect.right = WINCX; cRect.bottom = WINCY - 50; cRect.top = WINCY - 100;
				int num = 0;
				for (auto iter = allchatList.begin(); iter != allchatList.end();) {
					TextOut(hdcBuffer, cRect.left, cRect.top + chatY * num, iter->chat_buf, lstrlen(iter->chat_buf));
					num++;
					if (iter->timeout < high_resolution_clock::now()) {
						iter = allchatList.erase(iter);
					}
					else
						++iter;
				}
			}
		}
		// ���� ���� ȭ��
		else {
			BitBlt(hdcBuffer, 0, 0, WINCX, WINCY, hdcGameover, 0, 0, SRCCOPY);
			if (gameover_timeout < high_resolution_clock::now()) {
				gameover = false;
			}
		}
		// render BackBuffer at MainDC (Double Buffering)
	}
	BitBlt(hdcMain, 0, 0, WINCX, WINCY, hdcBuffer, 0, 0, SRCCOPY);


	DeleteDC(hdcMenu);
	DeleteDC(hdcGameover);
	DeleteDC(hdcAgro);
	DeleteDC(hdcPeace);
	DeleteDC(hdcBulletS);
	DeleteDC(hdcBullet);
	DeleteDC(hdcBUFPortion);
	DeleteDC(hdcHPPortion);
	DeleteDC(hdcObstacle);
	DeleteDC(hdcNPC);
	DeleteDC(hdcPlayer);
	DeleteDC(hdcBackGround);
	DeleteDC(hdcBuffer);

	ReleaseDC(g_hwnd, hdcMain);
}

void MainGame::InputKeyState(int key)
{
	if (send_last_time < 10) return;
	send_last_time = 0;

	int ret = 0;
	if (key < KEY_ATTACK) {
		int x = 0, y = 0;
		// 0: up, 1:down, 2:left, 3:right, 4:attack
		switch (key) {
		case KEY_UP:
			y -= 1;
			break;
		case KEY_DOWN:
			y += 1;
			break;
		case KEY_LEFT:
			x -= 1;
			break;
		case KEY_RIGHT:
			x += 1;
			break;
		}
		// Send
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

		ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	}
	else if(key >= KEY_ATTACK) {
		if (key == 4) { // attack
			// Send
			cs_packet_attack* p = reinterpret_cast<cs_packet_attack*>(send_buffer);
			p->size = sizeof(p);
			send_wsabuf.len = sizeof(p);
			DWORD iobyte;
			p->type = CS_ATTACK;
			ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			// bullet list�� 4�� �߰�
			BULLET b0(players[myid].ptX, players[myid].ptY - 1, 30);
			BULLET b1(players[myid].ptX, players[myid].ptY + 1, 30);
			BULLET b2(players[myid].ptX - 1, players[myid].ptY, 30);
			BULLET b3(players[myid].ptX + 1, players[myid].ptY, 30);
			b0.type = 0; b1.type = 0; b2.type = 0; b3.type = 0;
			bulletList.push_back(b0);
			bulletList.push_back(b1);
			bulletList.push_back(b2);
			bulletList.push_back(b3);
		}
		else if (key == 10) { // attack S
			// Send
			cs_packet_attack* p = reinterpret_cast<cs_packet_attack*>(send_buffer);
			p->size = sizeof(p);
			send_wsabuf.len = sizeof(p);
			DWORD iobyte;
			p->type = CS_ATTACKS;
			ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
			// bullet list�� �߰�
			BULLET b0(players[myid].ptX, players[myid].ptY, 30);
			b0.type = 1;
			bulletList.push_back(b0);
		}
		else if (key == 5) { // 1��ĭ �Һ�

		}
	}

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
	// ���� ��Ƽ�÷��̾�ϱ� �迭�� ���� !
	//players[id].ptX = ptX;
	//players[id].ptY = ptY;
	//players[id].x = ptX * TILESIZE + TILESIZE * 0.5f;
	//players[id].y = ptY * TILESIZE + TILESIZE * 0.5f;
	//// ü���ǿ� �°� // point�� ���� ��ġ ����
	//player.x = player.ptX * TILESIZE + TILESIZE * 0.5f;
	//player.y = player.ptY * TILESIZE + TILESIZE * 0.5f;
}

void MainGame::setObjectRect(int id)
{
	//// ���� ��Ƽ�÷��̾�ϱ� �迭�� ���� !
	//players[id].rect.left = long(players[id].x - PLAYERCX * 0.5f);
	//players[id].rect.right = long(players[id].x + PLAYERCX * 0.5f);
	//players[id].rect.top = long(players[id].y - PLAYERCY * 0.5f);
	//players[id].rect.bottom = long(players[id].y + PLAYERCY * 0.5f);
}

void MainGame::setScroll(int ptX, int ptY)
{
	scrollX = -(ptX * TILESIZE) + (WINCX * 0.5);
	scrollY = -(ptY * TILESIZE) + (WINCY * 0.5);
	// ��ũ�� ó��
	if (scrollX < WINCX - (TILESIZE * TILEMAX) - (WINCX * 0.5))
		scrollX = WINCX - (TILESIZE * TILEMAX) - (WINCX * 0.5);
	if (scrollX > WINCX * 0.5)
		scrollX = WINCX * 0.5;
	if (scrollY < WINCY - (TILESIZE * TILEMAX) - (WINCY * 0.5))
		scrollY = WINCY - (TILESIZE * TILEMAX) - (WINCY * 0.5);
	if (scrollY > WINCY * 0.5)
		scrollY = WINCY * 0.5;
	//cout << "scrollX : " << scrollX << endl;
	//cout << "scrollY : " << scrollY << endl;
}


bool MainGame::isNear(int x1, int y1, int x2, int y2, int viewlimit)
{
	int dist = (x1 - x2) * (x1 - x2);
	dist += (y1 - y2) * (y1 - y2);
	return dist <= viewlimit * viewlimit;
}



void MainGame::LoadBitmaps()
{
	// BackBuffer
	HBITMAP tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackBuffer.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // ��Ʈ�� �ε� ���2
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/BackBuffer.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[0] = tempBitmap;

	// BackGround
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/BackGround.bmp", IMAGE_BITMAP, WINCX, WINCY, LR_LOADFROMFILE | LR_CREATEDIBSECTION); // ��Ʈ�� �ε� ���2
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

	// Waddle
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Waddle.bmp", IMAGE_BITMAP, 150, 90, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Waddle.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[3] = tempBitmap;

	// Obstacle
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Obstacle.bmp", IMAGE_BITMAP, 30, 30, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Obstacle.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[4] = tempBitmap;

	// Bullet
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Bullet.bmp", IMAGE_BITMAP, 30, 30, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Bullet.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[5] = tempBitmap;

	// Dead Screen
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/gameover.bmp", IMAGE_BITMAP, 800, 800, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/gameover.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[6] = tempBitmap;

	// Agro
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Agro.bmp", IMAGE_BITMAP, 30, 30, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Agro.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[7] = tempBitmap;


	// Peace
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/Peace.bmp", IMAGE_BITMAP, 50, 50, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/Peace.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[8] = tempBitmap;


	// HPPortion
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/hpportion.bmp", IMAGE_BITMAP, 30, 30, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/hpportion.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[9] = tempBitmap;


	// BufPortion
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/bufportion.bmp", IMAGE_BITMAP, 30, 30, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/bufportion.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[10] = tempBitmap;

	// Attack S
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/AttackS.bmp", IMAGE_BITMAP, 30, 30, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/AttackS.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[11] = tempBitmap;

	// Menu
	tempBitmap = (HBITMAP)LoadImage(NULL, L"Image/menu.bmp", IMAGE_BITMAP, 800, 800, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	if (NULL == tempBitmap) {
		MessageBox(g_hwnd, L"Image/menu.bmp", L"Failed (LoadImage)", MB_OK);
		return;
	}
	else
		bitmaps[12] = tempBitmap;
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

	//WSAAsyncSelect(serverSocket, g_hwnd, WM_SOCKET, FD_CLOSE | FD_READ); // ������ �޽����ε� ���о������.. ����/..
	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = BUF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = BUF_SIZE;

	recvThread = thread{ RecvPacket, this };
	// ReadPacket(); -> ���� �����忡�� �ޱ��

	// �ʱ⿡ login �õ� ��Ŷ ����
	//cs_packet_login login_packet;
	//login_packet.size = sizeof(login_packet);
	//login_packet.type = CS_LOGIN;
	int t_id = GetCurrentProcessId();
	//sprintf_s(login_packet.name, "P%03d", t_id % 1000);

	// �̶� �� �ѹ� ������ ����.
	cs_packet_login* p = reinterpret_cast<cs_packet_login*>(send_buffer);
	p->size = sizeof(p);
	send_wsabuf.len = sizeof(p);
	DWORD iobyte;
	p->type = CS_LOGIN;
	//// ���̵�
	// sprintf_s(p->name, "P%03d", t_id % 1000);
	char* pStr;
	int strSize = WideCharToMultiByte(CP_ACP, 0, g_nicknamebuf, -1, NULL, 0, NULL, NULL);
	pStr = new char[strSize];
	WideCharToMultiByte(CP_ACP, 0, g_nicknamebuf, -1, pStr, strSize, 0, 0);
	memcpy(&(p->name), (pStr), strSize);

	int ret = WSASend(serverSocket, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while send packet [%d]", error_code);
	}
	//---
	//SendPacket(&login_packet);

	// ������� mynickname�� �г��� ���� (char to wchar_t)
	char tempBuffer[MAX_ID_LEN] = "";
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
	for (int i = 0; i < 13; ++i) {
		DeleteObject(bitmaps[i]);
	}
	
	closesocket(serverSocket);
	WSACleanup();

	
	// recvThread.~thread();
}
