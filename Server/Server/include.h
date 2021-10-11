#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include <queue>
#include <windows.h>  
#include <sqlext.h>  
#include <string>
#include "protocol.h"

extern "C" {
#include "include/lua.h"
#include "include/lauxlib.h"
#include "include/lualib.h"
}

using namespace std;
using namespace chrono;

#pragma comment(lib,"odbc32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")

#include "const.h"
#include "struct.h"
