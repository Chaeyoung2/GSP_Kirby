#pragma once

class KeyMgr;
class MainGame
{
public:
	MainGame();
	~MainGame();
public:
	void Start();
	void Update();
	void Render();
	void Release();
public:
	void InputKeyState(int key);
	void setObjectPoint(int id, float ptX, float ptY);
	void setObjectRect(int id);
public:
	void LoadBitmaps();
public:
	OBJ* getPlayers() {
		if (players != nullptr) return players;
	}
	void setMyid(int id) {
		myid = id;
	}
	HBITMAP* getBitmaps() {
		if (bitmaps != nullptr) return bitmaps;
	}
private:
	HDC hdc;
	HBITMAP bitmaps[3];
private:
	int myid=-1;
	OBJ players[MAX_USER] = {};
/// ////////////////////////////////////////////////////////////
public:
	void InitNetwork();
public:
	const SOCKET* getServerSocket() {
		if (&serverSocket != nullptr) return &serverSocket;
	}
	 WSABUF* getRecvWsabuf() {
		if (&recv_wsabuf != nullptr) return &recv_wsabuf;
	}
private:
	SOCKET serverSocket;
	WSABUF send_wsabuf;
	WSABUF recv_wsabuf;
	char	packet_buffer[BUF_SIZE];
	char 	send_buffer[BUF_SIZE];
	char	recv_buffer[BUF_SIZE];
	thread recvThread;
};

