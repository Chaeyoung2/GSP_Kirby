// header.h: 표준 시스템 포함 파일
// 또는 프로젝트 특정 포함 파일이 들어 있는 포함 파일입니다.
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
// Windows 헤더 파일
#include <windows.h>
// C 런타임 헤더 파일입니다.
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <iostream>
#include <WS2tcpip.h>
#include <thread>
#include <wchar.h>
#include <mutex>
#include <chrono>
#include <list>
// 자체 헤더 파일
#include "../../../Server/Server/protocol.h"
using namespace std;
using namespace chrono;

// 라이브러리 추가
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "WS2_32.LIB")
#pragma warning(disable:4996)
#pragma warning(disable:4703)

// define
#define WINCX 800
#define WINCY 800
#define PLAYERCX 30
#define PLAYERCY 30
#define MON1CX 30
#define MON1CY 30
#define TILESIZE 40
#define WM_SOCKET (WM_USER+1)
#define SERVER_IP "127.0.0.1"

constexpr int TILEMAX = 2000;
constexpr int TILESCREENMAX = 20;
constexpr int KEY_UP = 0;
constexpr int KEY_DOWN = 1;
constexpr int KEY_LEFT = 2;
constexpr int KEY_RIGHT = 3;
constexpr int KEY_ATTACK = 4;

// extern
extern HWND g_hwnd;

// struct
typedef struct tagObject {
	int id = -1;
	bool connected = false;
	//short x;
	//short y;
	short ptX;
	short ptY;
	char *name;
	short hp;
	short level;
	int exp;
	//RECT rect;
	//HBITMAP bitmap;
	TCHAR chat_buf[MAX_STR_LEN] = L"";
	high_resolution_clock::time_point timeout;
}OBJ;

typedef struct tagBullet {
	tagBullet() {}
	tagBullet(short _x, short _y, short _t)
		: x(_x), y(_y), timeout(_t) { }
	short x;
	short y;
	short timeout;
	short cur_time = 0;
}BULLET;

// const
constexpr int BUF_SIZE = 200;
constexpr short PORT = 3500;



