// Client.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "framework.h"
#include "Client.h"
#include "MainGame.h"

#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.
MainGame mainGame;

// extern
HWND g_hwnd;

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{

    // api 콘솔창
#ifdef _DEBUG
    FILE* pFile = nullptr;
     	if(AllocConsole())
     		freopen_s(&pFile, "CONOUT$", "w", stdout);
#endif

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIENT));

    MSG msg;
    msg.message = WM_NULL; // 초기화

    // MainGame 생성
    mainGame.Start();

    // 기본 메시지 루프입니다: // 변경 -> FPS 제한 둘 것임
    //while (GetMessage(&msg, nullptr, 0, 0))
    //{
    //    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
    //    {
    //        TranslateMessage(&msg);
    //        DispatchMessage(&msg);
    //    }
    //}
    DWORD dwOldTime = GetTickCount(); // 운체가 시작됐을 때부터 흘러온 시간을 1/1000초 단위의 정수형으로 반환함
    DWORD dwCurTime = 0;

    TCHAR szFPS[32] = __T("");
    DWORD dwFPSOldTime = GetTickCount();
    DWORD dwFPSCurTime = 0;
    int   iFps = 0;

    while (WM_QUIT != msg.message)
    {
        dwFPSCurTime = GetTickCount();

        if (dwFPSOldTime + 1000 < dwFPSCurTime)
        {
            dwFPSOldTime = dwFPSCurTime;
            swprintf_s(szFPS, __T("FPS: %d"), iFps);
            SetWindowText(g_hwnd, szFPS);
            iFps = 0;
        }

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            dwCurTime = GetTickCount();
            if (dwOldTime + 10 < dwCurTime)
            {
                dwOldTime = dwCurTime;
                mainGame.Update();
                mainGame.Render();
                iFps++;
            }
        }
    }

    return (int) msg.wParam;
}



//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLIENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName = NULL; // MAKEINTRESOURCEW(IDC_CLIENT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

   // 타이틀바가 윈도우 잘라먹음
   int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
   RECT rc{ 0,0,WINCX + titleBarHeight, WINCY + titleBarHeight * 2 + 30 };

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      100, 100, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);


   if (!hWnd)
   {
      return FALSE;
   }

   // 전역 핸들이 갖고 있도록
   g_hwnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static char str[1024];
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CHAR: 
    {
        int len = strlen(str);
        str[len] = (TCHAR)wParam;
        str[len + 1] = 0;
        if (wParam == 'a' || wParam == 'A') {
            mainGame.InputKeyState(4); // attack
        } else if (wParam == '1') {
                mainGame.InputKeyState(5); // 1번 아이템 소비
            }
        else if (wParam == '2') {
            mainGame.InputKeyState(6); // 2번 아이템 소비
        }
        else if (wParam == '3') {
            mainGame.InputKeyState(7); //  3번 아이템 소비
        }
        else if (wParam == '4') {
            mainGame.InputKeyState(8); //  4번 아이템 소비
        }
        else if (wParam == '5') {
            mainGame.InputKeyState(9); //  5번 아이템 소비
        }
    }
    break;
    case WM_KEYDOWN:
    {
        switch (wParam) {
        case VK_UP:
            mainGame.InputKeyState(0);
            break;
        case VK_DOWN:
            mainGame.InputKeyState(1);
            break;
        case VK_LEFT:
            mainGame.InputKeyState(2);
            break;
        case VK_RIGHT:
            mainGame.InputKeyState(3);
            break;
        }
    }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
