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
	void setScroll(int ptX, int ptY);
public:
	void LoadBitmaps();
public:
	OBJ* getPlayers() {
		if (players != nullptr) return players;
	}
	HBITMAP* getBitmaps() {
		if (bitmaps != nullptr) return bitmaps;
	}
private:
	HDC hdc;
	HBITMAP bitmaps[3];
private:
	wchar_t mynickname[128] = L"";
	OBJ players[MAX_USER] = {};
/// ////////////////////////////////////////////////////////////
public:
	void InitNetwork();
	void SendPacket(void* packet);
private:
	thread recvThread;
};

