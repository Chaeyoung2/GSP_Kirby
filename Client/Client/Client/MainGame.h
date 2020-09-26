#pragma once

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
private:
	HDC hdc;
	HBITMAP hBitmapBackGround;
	HBITMAP hBitmapBackBuffer;
private:
	OBJ player;
/// ////////////////////////////////////////////////////////////
public:
	void InitNetwork();
	void ReadPacket();
private:
	SOCKET serverSocket;
	WSABUF send_wsabuf;
	WSABUF recv_wsabuf;
	char	packet_buffer[BUF_SIZE];
	char 	send_buffer[BUF_SIZE];
	char	recv_buffer[BUF_SIZE];
};

