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
	void InputKeyState();
	void setObjectPoint();
	void setObjectRect();
public:
	void LoadBitmaps();
public:
	OBJ* getPlayer() {
		if (&player != nullptr) return &player;
	}
private:
	HDC hdc;
	HBITMAP hBitmapBackGround;
	HBITMAP hBitmapBackBuffer;
	KeyMgr * pKeyMgr;
private:
	OBJ player;
/// ////////////////////////////////////////////////////////////
public:
	void InitNetwork();
	void ReadPacket();
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

