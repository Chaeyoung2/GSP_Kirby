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
// 자체 헤더 파일
#include "../../../Server/Server/protocol.h"
using namespace std;

// 라이브러리 추가
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "WS2_32.LIB")
#pragma warning(disable:4996)

// define
#define WINCX 800
#define WINCY 800
#define PLAYERCX 50
#define PLAYERCY 50
#define TILESIZE 100
#define WM_SOCKET (WM_USER+1)


// extern
extern HWND g_hwnd;

// struct
typedef struct tagObject {
	float x;
	float y;
	short ptX;
	short ptY;
	RECT rect;
	HBITMAP bitmap;
}OBJ;

// const
constexpr int BUF_SIZE = 1024;
constexpr short PORT = 3500;
constexpr char SERVER_ADDR[] = "127.0.0.1";


