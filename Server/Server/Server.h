#include <iostream>
#include "Include.h"

void ErrorDisplay(const char* msg, int err_no);

void PlayerHPPlus(int id);

void InitializeObstacle();
void InitializeItem();
void InitializeNPC();
void InitializeNetwork();

void RandomMoveNPC(int id);
void WakeUpNPC(int id);

void AddTimer(int obj_id, int ev_type, system_clock::time_point t);

void ProcessPacket(int id);
void ProcessRecv(int id, DWORD iosize);
void ProcessMove(int id, char dir);
void ProcessAttack(int id, int type);
void ProcessLogin(int id);

void StatChange_MonsterDead(int id, int mon_id);
void StatChange_MonsterCollide(int id, int mon_id);
void StatChange_ItemCollide(int id, int item_id);

void WorkerThread();
void TimerThread();

void AddNewClient(SOCKET ns);
void DisconnectClient(int id);

void SendPacket(int id, void* p);
void SendLeavePacket(int to_id, int id);
void SendLoginOK(int id);
void SendEnterPacket(int to_id, int new_id);
void SendMovePacket(int to_id, int id);
void SendChatPacket(int to_client, int id, char* mess);
void SendStatChangePacket(int id);

int calcDist(int p1, int p2);

bool IsCollide(int p1, int p2);

bool IsNear(int p1, int p2);
bool IsPlayer(int p1);
bool IsNPC(int p1);
bool IsObstacle(int p1);
bool IsItem(int p1);
bool IsInvincible(int p1);

int API_SendMessage(lua_State* L);
int API_get_y(lua_State* L);
int API_get_x(lua_State* L);
int API_RandomMove(lua_State* L);


void InitializeDB();
void LoadDB(const string name, int id);
void SaveDB(string name, int lv, int x, int y, int exp);
void CloseDB();
void DB_ERROR(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);
