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
	bool isNear(int x1, int y1, int x2, int y2, int viewlimit);
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
	HBITMAP bitmaps[13];
private:
	wchar_t mynickname[128] = L"";
	OBJ players[MAX_USER+NUM_NPC+NUM_OBSTACLE+NUM_ITEM] = {};
	list<BULLET> bulletList;
/// ////////////////////////////////////////////////////////////
public:
	void InitNetwork();
	void SendPacket(void* packet);
	bool getGameStart() {
		return gamestart;
	}
	void setGameStart(bool b) {
		gamestart = b;
	}
public:
	thread recvThread;
	bool threading = true;
private:
	bool gamestart = false;
};

