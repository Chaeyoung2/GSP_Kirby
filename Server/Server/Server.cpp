#include "Server.h"

HANDLE h_iocp; // ������ Ŀ�� ��ü�� �ڵ��� �޾Ƽ� ����Ѵ�.
SOCKET g_listenSocket;
client_info g_clients[MAX_USER + NUM_NPC + NUM_OBSTACLE + NUM_ITEM];
OVER_EX g_accept_over; // accept�� overlapped ����ü
mutex timer_l;
mutex sector_l;
priority_queue<event_info> event_queue;
unordered_set<int> g_sector[S_CNT][S_CNT];
SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt = 0;

int PLAYER_ATTACKDAMAGE = 20;

int main()
{
	wcout.imbue(std::locale("korean"));

	for (auto& cl : g_clients) {
		cl.connected = false;
	}

	InitializeDB();
	InitializeNetwork();
	InitializeNPC();
	InitializeObstacle();
	InitializeItem();

	thread timer_thread{ TimerThread };
	vector <thread> workerthreads;
	for (int i = 0; i < 6; ++i) {
		workerthreads.emplace_back(WorkerThread);
	}
	for (auto& t : workerthreads)
		t.join();
	timer_thread.join();

	closesocket(g_listenSocket);
	WSACleanup();
	CloseDB();

	return 0;
}

// ��Ŷ ó�� ��ƾ
void ProcessPacket(int id)
{
	char p_type = g_clients[id].m_packet_start[1];
	switch (p_type) {
	case CS_LOGIN: 
	{
		cs_packet_login* p = reinterpret_cast<cs_packet_login*>(g_clients[id].m_packet_start);
		g_clients[id].c_lock.lock();
		strcpy_s(g_clients[id].name, p->name);
		g_clients[id].c_lock.unlock();
		LoadDB(string(p->name), id);
		SendLoginOK(id);
		ProcessLogin(id);
	}
	break;
	case CS_MOVE:
	{
		g_clients[id].c_lock.lock();
		cs_packet_move* p = reinterpret_cast<cs_packet_move*>(g_clients[id].m_packet_start);
		g_clients[id].move_time = p->move_time;
		g_clients[id].c_lock.unlock();
		ProcessMove(id, p->direction);
	}
	break;
	case CS_ATTACK:
	{
		ProcessAttack(id, 0);
	}
	break;
	case CS_ATTACKS:
	{
		ProcessAttack(id, 1);
	}
	break;
	default:
	{
		cout << "Unkown Packet type[" << p_type << "] from Client [" << id << "]\n";
		while (true);
	}
	break;
	}

}

// ## iocp ���� - recv
// 1 ���� : 1 recv ȣ�� �̹Ƿ�, �ϳ��� ���۸� ��� ����� �� �ִ�.
// ��Ŷ���� �߰��� �߸� ä�� ������ �� �ִ�.
// - ��� �ξ��ٰ� ������ �� �����Ϳ� �ٿ� �ش�.
// - ���� �����͸� ������ �δ� ���� �Ǵ� ring buffer �ʿ�
// ���� ��Ŷ�� �Ѳ����� ������ �� �ִ�.
// - �߶� ó��
void ProcessRecv(int id, DWORD iosize)
{
	// ��Ŷ ����.
	unsigned char p_size = g_clients[id].m_packet_start[0]; // ��Ŷ ������
	unsigned char* next_recv_ptr = g_clients[id].m_recv_start + iosize; // ������ ���� ptr
	while (p_size <= next_recv_ptr - g_clients[id].m_packet_start) { // ����
		ProcessPacket(id); // ��Ŷ ó��
		g_clients[id].m_packet_start += p_size; // ��Ŷ ó�������� ���� ��Ŷ �ּҷ�
		if (g_clients[id].m_packet_start < next_recv_ptr)
			p_size = g_clients[id].m_packet_start[0]; // ���� ��Ŷ�� ����� p_size�� ������Ʈ
		else
			break;
	}
	// ��Ŷ �����ϰ� �̸�ŭ ������.
	long long left_data = next_recv_ptr - g_clients[id].m_packet_start;
	// ���۸� �� ������ �ʱ�ȭ�ؾ� ��. (������ �о������)
	if ((MAX_BUFFER - (next_recv_ptr - g_clients[id].m_recv_over.iocp_buf) < MIN_BUFFER)) { // ���۰� MIN_BUFFER ũ�⺸�� ������
		memcpy(g_clients[id].m_recv_over.iocp_buf, g_clients[id].m_packet_start, left_data); // ���� �ִ� ����Ʈ��ŭ copy 
		g_clients[id].m_packet_start = g_clients[id].m_recv_over.iocp_buf; 
		next_recv_ptr = g_clients[id].m_recv_start + left_data; // ������ ���� ��ġ ����Ŵ
	}
	DWORD recv_flag = 0;

	// ��𼭺��� �ٽ� ������
	g_clients[id].m_recv_start = next_recv_ptr;
	g_clients[id].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(next_recv_ptr);
	g_clients[id].m_recv_over.wsa_buf.len = MAX_BUFFER - static_cast<int>(next_recv_ptr - g_clients[id].m_recv_over.iocp_buf); // ���� �ִ� ���� �뷮
	// ����ȭ
	bool connected = true;
	if (true == g_clients[id].connected.compare_exchange_strong(connected, true)) {
		WSARecv(g_clients[id].sock, &g_clients[id].m_recv_over.wsa_buf, 1, NULL, &recv_flag, &g_clients[id].m_recv_over.wsa_over, NULL);
	}
}

void ProcessMove(int id, char dir)
{
	// �̵� ��
	//// ��ǥ
	short sx = g_clients[id].sx;
	short sy = g_clients[id].sy;
	short y = g_clients[id].y;
	short x = g_clients[id].x;
	// �ӽ� ��ǥ1 �̵�
	switch (dir) {
	case MV_UP:
		if (y > 0) y--;
		break;
	case MV_DOWN:
		if (y < WORLD_HEIGHT - 1) y++;
		break;
	case MV_LEFT:
		if (x > 0) x--;
		break;
	case MV_RIGHT:
		if (x < WORLD_WIDTH - 1) x++;
		break;
	default:
		while (true);
		break;
	}

	// �̵� ��
	//// �ӽ� ��ǥ2 ó��
	int cur_x = x;
	int cur_y = y;
	int cur_sx = cur_x / S_SIZE;
	int cur_sy = cur_y / S_SIZE;


	if (sx != cur_sx || sy != cur_sy) {	
		sector_l.lock();
		g_sector[sx][sy].erase(id);
		g_sector[cur_sx][cur_sy].insert(id);
		sector_l.unlock();
		sx = cur_x;
		sy = cur_y;
	}
	////��ǥ�� ��ֹ��� ��ġ�� �ʴ���? -> ��ġ�� ���⼭ move fail
	bool isCollide = false;
	for (auto i : g_sector[cur_sx][cur_sy]) {
		if (i == id) continue;
		if (false == IsObstacle(i)) continue;
		if (x == g_clients[i].x && y == g_clients[i].y) { // �浹�ߴٸ�
			isCollide = true;
			return;
		}
	}
	// ���� ��ǥ �̵� 
	if (false == isCollide)  { // ��ֹ��� �浹 �� �ߴٸ�
		g_clients[id].c_lock.lock();
		g_clients[id].x = cur_x;
		g_clients[id].y = cur_y;
		g_clients[id].c_lock.unlock();
		// ������ �˸�
		SendMovePacket(id, id);
	}

	// �� ����Ʈ ó��
	unordered_set<int> old_viewlist = g_clients[id].view_list;
	unordered_set<int> new_viewlist; 
	for(auto i : g_sector[cur_sx][cur_sy]){
		if (id == i) continue;
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i)) { 
			new_viewlist.insert(i); 
		}
	}
	for(auto i : g_sector[cur_sx][cur_sy]){
		if (true == IsNear(id, i)) {
			new_viewlist.insert(i);
			if(true == IsNPC(i))
				WakeUpNPC(i);
		}
	}

	// �þ� ó��
	for (int ob : new_viewlist) {
		// �̵� �� new viewlist�� ���� ob�� ���� ó��
		if (0 == old_viewlist.count(ob)) { // �̵� ���� �þ߿� ������ ���
			g_clients[id].c_lock.lock();
			g_clients[id].view_list.insert(ob); // �̵��� Ŭ���̾�Ʈ(id)�� ���� �丮��Ʈ�� �ִ´�
			g_clients[id].c_lock.unlock();
			SendEnterPacket(id, ob); // �̵��� Ŭ���̾�Ʈ(id)���� �þ߿� ���� ob�� enter�ϰ� �Ѵ�

			// �� ����Ʈ�� �ִ� ob�� �丮��Ʈ ó��.
			if (true == IsPlayer(ob)) {
				if (0 == g_clients[ob].view_list.count(id)) { // �þ߿� ���� ob�� �丮��Ʈ�� id�� �����ٸ�
					g_clients[ob].c_lock.lock();
					g_clients[ob].view_list.insert(id); // �þ߿� ���� ob�� �丮��Ʈ�� id�� �־��ְ�
					g_clients[ob].c_lock.unlock();
					SendEnterPacket(ob, id); // ob���� id�� enter�ϰ� �Ѵ�
				}
				else {// �þ߿� ���� ob�� �丮��Ʈ�� id�� �־��ٸ� 
					SendMovePacket(ob, id); // ob���� id�� move�ϰ� �Ѵ�
				}
			}
		}
		else {  // �̵� ���� �þ߿� �־��� ���
			if (true == IsPlayer(ob)) {
				if (0 != g_clients[ob].view_list.count(id)) { // �þ߿� ���� ob�� id�丮��Ʈ�� ���� �־�����
					SendMovePacket(ob, id); // ob���� id move ��Ŷ ����
				}
				else{ // id�� ob�� �丮��Ʈ�� �����ٸ� 
					g_clients[ob].c_lock.lock();
					g_clients[ob].view_list.insert(id); // ob�� �丮��Ʈ�� id �־��� 
					g_clients[ob].c_lock.unlock();
					SendEnterPacket(ob, id); // ob���� id�� enter�ϰ� �Ѵ�
				}
			}
		}
	}
	for (int ob : old_viewlist) { // �̵� �� old viewlist ���� ��ü�� ���� ó�� - leave ó�� �� ����, ������ ����
		if (0 == new_viewlist.count(ob)) { // �̵� �� old viewlist���� �ִµ� new viewlist�� ���� -> �þ߿��� �������
			g_clients[id].c_lock.lock();
			g_clients[id].view_list.erase(ob); // �̵��� id�� �丮��Ʈ���� ob�� ����
			g_clients[id].c_lock.unlock();
			SendLeavePacket(id, ob);
			if (true == IsPlayer(ob)) {
				if (0 != g_clients[ob].view_list.count(id)) { // ob�� �丮��Ʈ�� id�� �־��ٸ�
					g_clients[ob].c_lock.lock();
					g_clients[ob].view_list.erase(id); // ob�� �丮��Ʈ������ id�� ����
					g_clients[ob].c_lock.unlock();
					SendLeavePacket(ob, id); // ob�� id�� leave �ϰԲ�
				}
			}
		}
	}

	// ���Ϳ� �浹�ϴ��� (hp ó��)
	// �����۰� �浹�ϴ���
	if (false == IsInvincible(id)) { // ������ ���� �浹 ó�� X
		for (auto ob : new_viewlist) {
			if (IsNPC(ob)) {
				if (IsCollide(id, ob)) {
					StatChange_MonsterCollide(id, ob);
				}
			}
			else if (IsItem(ob)) {
				if (IsCollide(id, ob)) {
					StatChange_ItemCollide(id, ob);
				}
			}
		}
	}


	// ���� npc���� player event�� pqcs�� ������
	for (auto& npc : new_viewlist) {
		if (false == IsNPC(npc)) continue;
		OVER_EX* ex_over = new OVER_EX;
		ex_over->object_id = id;
		ex_over->op_mode = OP_PLAYER_MOVE_NOTIFY;
		// ## �̺�Ʈ �߰� �Լ�, Ŀ���� queue�� �̺�Ʈ�� �߰��Ѵ�.
		// ������� Ŀ�� object, ���۵� ������ ��, �̸� ���� ���� ID, overlapped ����ü
		PostQueuedCompletionStatus(h_iocp, 1, npc, &ex_over->wsa_over);
	}
}

void ProcessAttack(int id, int type) {

	int* x = nullptr, * y = nullptr;
	int size = 0;
	short id_x = g_clients[id].x, id_y = g_clients[id].y;

	switch (type) {
	case 0:
	{
		size = 4; // 0 up, 1 down, 2 left, 3 right

		x = new int[size];
		y = new int[size];

		for (int dir = 0; dir < size; ++dir) {
			if (dir == 0) {
				x[dir] = id_x;
				y[dir] = id_y - 1;
			}
			else if (dir == 1) {
				x[dir] = id_x;
				y[dir] = id_y + 1;
			}
			else if (dir == 2) {
				x[dir] = id_x - 1;
				y[dir] = id_y;
			}
			else if (dir == 3) {
				x[dir] = id_x + 1;
				y[dir] = id_y;
			}
		}
	}
		break;
	case 1:
	{
		size = 25;

		x = new int[size];
		y = new int[size];

		for (int dir = 0; dir < 25; ++dir) {
			if (dir == 0) {
				x[dir] = id_x;
				y[dir] = id_y;
			}
			else if (dir == 1) {
				x[dir] = id_x;
				y[dir] = id_y - 3;
			}
			else if (dir == 2) {
				x[dir] = id_x - 1;
				y[dir] = id_y - 2;
			}
			else if (dir == 3) {
				x[dir] = id_x;
				y[dir] = id_y - 2;
			}
			else if (dir == 4) {
				x[dir] = id_x + 1;
				y[dir] = id_y - 2;
			}
			else if (dir == 5) {
				x[dir] = id_x - 2;
				y[dir] = id_y - 1;
			}
			else if (dir == 6) {
				x[dir] = id_x - 1;
				y[dir] = id_y - 1;
			}
			else if (dir == 7) {
				x[dir] = id_x;
				y[dir] = id_y - 1;
			}
			else if (dir == 8) {
				x[dir] = id_x + 1;
				y[dir] = id_y - 1;
			}
			else if (dir == 9) {
				x[dir] = id_x + 2;
				y[dir] = id_y - 1;
			}
			else if (dir == 10) {
				x[dir] = id_x - 3;
				y[dir] = id_y;
			}
			else if (dir == 11) {
				x[dir] = id_x - 2;
				y[dir] = id_y;
			}
			else if (dir == 12) {
				x[dir] = id_x - 1;
				y[dir] = id_y;
			}
			else if (dir == 13) {
				x[dir] = id_x + 1;
				y[dir] = id_y;
			}
			else if (dir == 14) {
				x[dir] = id_x + 2;
				y[dir] = id_y;
			}
			else if (dir == 15) {
				x[dir] = id_x + 3;
				y[dir] = id_y;
			}
			else if (dir == 16) {
				x[dir] = id_x - 2;
				y[dir] = id_y + 1;
			}
			else if (dir == 17) {
				x[dir] = id_x - 1;
				y[dir] = id_y + 1;
			}
			else if (dir == 18) {
				x[dir] = id_x;
				y[dir] = id_y + 1;
			}
			else if (dir == 19) {
				x[dir] = id_x + 1;
				y[dir] = id_y + 1;
			}
			else if (dir == 20) {
				x[dir] = id_x + 2;
				y[dir] = id_y + 1;
			}
			else if (dir == 21) {
				x[dir] = id_x - 1;
				y[dir] = id_y + 2;
			}
			else if (dir == 22) {
				x[dir] = id_x;
				y[dir] = id_y + 2;
			}
			else if (dir == 23) {
				x[dir] = id_x + 1;
				y[dir] = id_y + 2;
			}
			else if (dir == 24) {
				x[dir] = id_x;
				y[dir] = id_y + 3;
			}
		}
	}
		break;
	}



	unordered_set<int> vl = g_clients[id].view_list;
	// �÷��̾� ���ݰ� npc�� �浹 �˻�
	for (auto i : vl) {
		if (false == IsNPC(i)) continue;
		for (int dir = 0; dir < size; ++dir) {
			if (g_clients[i].x == x[dir] && g_clients[i].y == y[dir]) {
				if (false == IsInvincible(i)) {
					// �浹
					g_clients[i].c_lock.lock();
					g_clients[i].invincible_timeout = high_resolution_clock::now() + 2s;
					g_clients[i].hp -= PLAYER_ATTACKDAMAGE;
					g_clients[i].attackme_id = id;
					g_clients[i].c_lock.unlock();
					char mess[MAX_STR_LEN];
					sprintf_s(mess, "�÷��̾� %d�� ���� %d�� ������ %d�� �������� �������ϴ�.", id, i, PLAYER_ATTACKDAMAGE);
					SendChatPacket(id, -1, mess); // ��ê
				}
			}
		}
	}

}

void ProcessLogin(int id)
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (false == g_clients[i].connected) continue;
		if (id == i) continue;
		if (false == IsNear(i, id)) continue;
		g_clients[i].c_lock.lock();
		if (0 == g_clients[i].view_list.count(id)) {
			g_clients[i].view_list.insert(id);
			g_clients[i].c_lock.unlock();
			SendEnterPacket(i, id);
		}
		else {
			g_clients[i].c_lock.unlock();
		}
		g_clients[id].c_lock.lock();
		if (0 == g_clients[id].view_list.count(i)) {
			g_clients[id].view_list.insert(i);
			g_clients[id].c_lock.unlock();
			SendEnterPacket(id, i);
		}
		else {
			g_clients[id].c_lock.unlock();
		}
	}
	// NPC
	for (int i = MAX_USER; i < MAX_USER + NUM_NPC; ++i) {
		if (false == IsNear(id, i)) continue;
		g_clients[id].c_lock.lock();
		g_clients[id].view_list.insert(i);
		g_clients[id].c_lock.unlock();
		SendEnterPacket(id, i);
		WakeUpNPC(i);
	}
	// Obstacle, Item
	for (int i = MAX_USER + NUM_NPC; i < MAX_USER + NUM_NPC + NUM_OBSTACLE + NUM_ITEM; ++i) {
		if (false == IsNear(id, i)) continue;
		g_clients[id].c_lock.lock();
		g_clients[id].view_list.insert(i);
		g_clients[id].c_lock.unlock();
		SendEnterPacket(id, i);
	}
}


void StatChange_MonsterDead(int id, int mon_id) {
	int mon_type = g_clients[mon_id].m_type;
	int exp = g_clients[id].level * g_clients[id].level * 2;
	if (mon_type == 1 || mon_type == 2)
		exp *= 2;
	g_clients[id].c_lock.lock();
	g_clients[id].exp += exp;
	switch (g_clients[id].level) {
	case 1:
		if (g_clients[id].exp >= 100) {
			g_clients[id].exp = 0;
			g_clients[id].level++;
		}
		break;
	case 2:
		if (g_clients[id].exp >= 200) {
			g_clients[id].exp = 0;
			g_clients[id].level++;
		}
		break;
	case 3:
		if (g_clients[id].exp >= 400) {
			g_clients[id].exp = 0;
			g_clients[id].level++;
		}
		break;
	case 4:
		if (g_clients[id].exp >= 800) {
			g_clients[id].exp = 0;
			g_clients[id].level++;
		}
		break;
	default:
		if (g_clients[id].exp >= 1000 + g_clients[id].level * 100) {
			g_clients[id].exp = 0;
			g_clients[id].level++;
		}
	}
	g_clients[id].c_lock.unlock();
	SendStatChangePacket(id); // ���� �ٲ� ��Ŷ

	cout << "���� " << mon_id << "(��)�� " << g_clients[id].attackme_id << "�� ���� ����߽��ϴ�." << endl;
	char mess[MAX_STR_LEN];
	sprintf_s(mess, "�÷��̾� %d�� ���� %d�� ���񷯼� %d�� ����ġ�� ������ϴ�.", id, mon_id, exp);
	SendChatPacket(id, -1, mess); // ��ê ��Ŷ
}

void StatChange_MonsterCollide(int id, int mon_id) {
	int damage = MONSTER_ATTACKDAMAGE;
	g_clients[id].c_lock.lock();
	g_clients[id].hp -= damage;
	if (g_clients[id].hp <= 0) { // �÷��̾� ��� ó��
		g_clients[id].hp = MAX_PLAYERHP;
		g_clients[id].exp /= 2; // ����ġ ���� ����
		g_clients[id].x = rand() % WORLD_WIDTH;
		g_clients[id].y = rand() % WORLD_HEIGHT;
		g_clients[id].invincible_timeout = high_resolution_clock::now() + 7s;
		g_clients[id].c_lock.unlock();
		SendGameOverPacket(id);
		SendStatChangePacket(id); // ���� �ٲ� ��Ŷ

		// �ٵ� ���⼭..
		// id���� id�� �丮��Ʈ�� �ִ� �÷��̾�����׵� �˷��� �ǰŵ�???... �ϴ� �ð� �����ϱ� �������� ��������
	}
	else {
		g_clients[id].invincible_timeout = high_resolution_clock::now() + 2s;
		g_clients[id].c_lock.unlock();
#ifdef _DEBUG
		cout << "���� " << mon_id << "�� �������� �÷��̾� " << id << "�� " << damage << "�� �������� �Խ��ϴ�." << endl;
#endif
		char mess[MAX_STR_LEN];
		sprintf_s(mess, "���� %d�� ���� �÷��̾� %d�� %d�� �������� �Խ��ϴ�.", mon_id, id, damage);
		SendChatPacket(id, -1, mess); // ��ê
		SendStatChangePacket(id); // ���� �ٲ� ��Ŷ
	}
}

void StatChange_ItemCollide(int id, int item_id) {
	// ������ ����
	g_clients[item_id].c_lock.lock();
	g_clients[item_id].is_active = false;
	g_clients[item_id].connected = false;
	g_clients[item_id].c_lock.unlock();
	int sx = g_clients[item_id].sx = g_clients[item_id].x / S_SIZE;
	int sy = g_clients[item_id].sy = g_clients[item_id].y / S_SIZE;
	// ���Ϳ��� ����
	sector_l.lock();
	g_sector[sx][sy].erase(item_id);
	sector_l.unlock();
	// �޽���, �÷��̾� hp ó��
	char mess[MAX_STR_LEN];
	if (g_clients[item_id].m_type == OTYPE_ITEM_HP) { // hp ������ ���
		int plusHP = PLUS_ITEMHP;
		g_clients[item_id].c_lock.lock();
		g_clients[id].hp += plusHP;
		g_clients[id].invincible_timeout = high_resolution_clock::now() + 2s;
		if (g_clients[id].hp > MAX_PLAYERHP) {
			g_clients[id].hp = MAX_PLAYERHP;
		}
		g_clients[item_id].c_lock.unlock();
#ifdef _DEBUG
		cout << "�÷��̾� " << id << "(��)�� HP ������ �Ծ�" << plusHP << "�� HP�� ����ϴ�." << endl;
#endif
		sprintf_s(mess, "�÷��̾� %d�� HP ������ �Ծ� %d�� HP�� ����ϴ�.", id, plusHP);
	}
	else {
		PLAYER_ATTACKDAMAGE = 40;
#ifdef _DEBUG
		cout << "�÷��̾� " << id << "(��)�� ���� ������ �Ծ� 3�ʰ� ���ݷ� 2�� ������ ����ϴ�." << endl;
#endif
		sprintf_s(mess, "�÷��̾� %d�� ���� ������ �Ծ� 3�ʰ� ���ݷ� 2�� ������ ����ϴ�.", id);		
		// �÷��̾� ü�� timer
		AddEventToTimer(id, OP_PLAYER_BUF, system_clock::now() + 3s);
	}
	SendChatPacket(id, -1, mess); // ��ê

	SendStatChangePacket(id); // ���� �ٲ� ��Ŷ

}

void WorkerThread() {
	while (true) {
		DWORD io_size;
		ULONG_PTR iocp_key;
		int key;
		WSAOVERLAPPED* lpover;
		int ret = GetQueuedCompletionStatus(h_iocp, &io_size, &iocp_key, &lpover, INFINITE);
		key = static_cast<int>(iocp_key); 
		if (FALSE == ret) { 
			int error_no = WSAGetLastError();
			if (64 == error_no) { 
				SaveDB(g_clients[key].name, 
					g_clients[key].level, g_clients[key].x, g_clients[key].y, g_clients[key].exp);
				DisconnectClient(key);
				continue;
			}
			else 
				ErrorDisplay("GQCS Error : ", error_no);
		}
		OVER_EX* over_ex = reinterpret_cast<OVER_EX*>(lpover);
		switch (over_ex->op_mode) {
		case OP_MODE_ACCEPT: 
		{ 
			AddNewClient(static_cast<SOCKET>(over_ex->wsa_buf.len)); 
		}
		break;
		case OP_MODE_RECV:
		{
			if (io_size == 0) { 
				SaveDB(g_clients[key].name, 
					g_clients[key].level, g_clients[key].x, 
					g_clients[key].y, g_clients[key].exp);
				DisconnectClient(key);
			}
			else { 
				ProcessRecv(key, io_size);
			}
		}
		break;
		case OP_MODE_SEND:
		{
			delete over_ex;
		}
		break;
		case OP_RANDOM_MOVE:
		{
			RandomMoveNPC(key);
		}
		break;
		case OP_PLAYER_MOVE_NOTIFY:
		{
			g_clients[key].c_lock.lock();
			lua_getglobal(g_clients[key].L, "event_player_move");
			lua_pushnumber(g_clients[key].L, over_ex->object_id);
			lua_pcall(g_clients[key].L, 1, 1, 0);
			g_clients[key].c_lock.unlock();
			delete over_ex;
		}
		break;
		case OP_PLAYER_HP_PLUS:
		{
			PlayerHPPlus(key);
		}
		break;
		case OP_PLAYER_BUF:
		{
			PLAYER_ATTACKDAMAGE = 20;
		}
		break;
		}
	}
}

void TimerThread()
{
	while (true) {
		while (true) {
			if (false == event_queue.empty()) {
				event_info ev = event_queue.top();
				if (ev.wakeup_time > system_clock::now()) break;
				timer_l.lock();
				event_queue.pop();
				timer_l.unlock();

				if (ev.event_id == OP_RANDOM_MOVE) {
					OVER_EX* ex_over = new OVER_EX;
					ex_over->op_mode = OP_RANDOM_MOVE;
					PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ex_over->wsa_over);
				}
				if (ev.event_id == OP_PLAYER_HP_PLUS) {
					OVER_EX* ex_over = new OVER_EX;
					ex_over->op_mode = OP_PLAYER_HP_PLUS;
					PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ex_over->wsa_over);
				}
			}
			else break;
		}
		this_thread::sleep_for(1ms);
	}
}

void AddNewClient(SOCKET ns)
{
	int i = -1;
	int compare_val = -1;
	for (int j = 0; j < MAX_USER; ++j) {
		if (false == g_clients[j].connected) {
			atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&i), &compare_val, j);
			break;
		}
	}

	if (MAX_USER == i) {
		cout << "Max user limit exceeded\n";
		closesocket(ns);
	}

	else {
		int i_compare_val = -1;
		bool b_compare_val = false;
		char c_compare_val = 0;
		SOCKET u_compare_val = -1;
		atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&g_clients[i].id), &i_compare_val, i);
		atomic_compare_exchange_strong(reinterpret_cast<atomic_bool*>(&g_clients[i].connected), &b_compare_val, true);
		atomic_compare_exchange_strong(reinterpret_cast<atomic_uintptr_t*>(&g_clients[i].sock), &u_compare_val, ns);
		atomic_compare_exchange_strong(reinterpret_cast<atomic_char*>(&(g_clients[i].name[0])), &c_compare_val, 0);


		g_clients[i].m_packet_start = g_clients[i].m_recv_over.iocp_buf;
		g_clients[i].m_recv_over.op_mode = OP_MODE_RECV;
		g_clients[i].m_recv_over.wsa_buf.buf = reinterpret_cast<CHAR*>(g_clients[i].m_recv_over.iocp_buf); 
		g_clients[i].m_recv_over.wsa_buf.len = sizeof(g_clients[i].m_recv_over.iocp_buf);
		ZeroMemory(&g_clients[i].m_recv_over.wsa_over, sizeof(g_clients[i].m_recv_over.wsa_over));
		g_clients[i].m_recv_start = g_clients[i].m_recv_over.iocp_buf; 


		DWORD flags = 0;
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(ns), h_iocp, i, 0); 

		g_clients[i].c_lock.lock();
		int ret = 0;
		if (true == g_clients[i].connected) {
			ret = WSARecv(g_clients[i].sock, &g_clients[i].m_recv_over.wsa_buf, 1, NULL, &flags, &(g_clients[i].m_recv_over.wsa_over), NULL);
		}
		g_clients[i].c_lock.unlock();

		if (ret == SOCKET_ERROR) {
			int error_no = WSAGetLastError();
			if (error_no != ERROR_IO_PENDING) {
				ErrorDisplay("WSARecv : ", error_no);
			}
		}

		AddEventToTimer(i, OP_PLAYER_HP_PLUS, system_clock::now() + 5s);
	}

	SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_accept_over.op_mode = OP_MODE_ACCEPT;
	g_accept_over.wsa_buf.len = static_cast<ULONG>(cSocket);
	ZeroMemory(&g_accept_over.wsa_over, sizeof(g_accept_over.wsa_over));

	AcceptEx(g_listenSocket, cSocket, g_accept_over.iocp_buf, 0, 32, 32, NULL, &g_accept_over.wsa_over);
}

void DisconnectClient(int id)
{
	// �ֺ� Ŭ��鿡�� ������ �˸�
	for (int i = 0; i < MAX_USER; ++i) {
		if (true == g_clients[i].connected) {
			if (i != id) {
				if (0 != g_clients[i].view_list.count(id)) {// �丮��Ʈ�� �ִ��� Ȯ���ϰ� ����
					g_clients[i].c_lock.lock();
					g_clients[i].view_list.erase(id);
					g_clients[i].c_lock.unlock();
					SendLeavePacket(i, id);
				}
//				else { // ������ ���� �ʿ䵵 ���� ��Ŷ ���� �ʿ䵵 ����

//				}
			}
		}
	}

	g_clients[id].c_lock.lock();
	g_clients[id].connected = false;
	g_clients[id].view_list.clear();
	closesocket(g_clients[id].sock);
	g_clients[id].sock = 0;
	g_clients[id].c_lock.unlock();
	// ����ȭ
	//int i_compare_val = 0;
	//bool b_compare_val = true;
	//if (true == atomic_compare_exchange_strong(&g_clients[id].connected, &b_compare_val, false)) {
	//	g_clients[id].view_list.clear();
	//	closesocket(g_clients[id].sock);
	//	g_clients[id].sock = 0;
	//}

}

// ## iocp ���� - ���� ���� - send
// 1 socket : ���� send ����
// - ���� ���� BroadCasting
// - overlapped ����ü, WSABUF�� �ߺ� ��� �Ұ�
// windows�� send ���� ������� ���� ���ۿ� �ְ� ����
// ���� ���۰� ���� send�� �߰��� �߷ȴٸ�?
// - �������� �ٽ� ����
// �ٽ� ������ �� �ٸ� �������� send�� �������ٸ�?
// - ��Ƽ� �������� ������. send ������ ���� �� ��Ŷ ���� ���۸� ���� �д�.
// - �Ǵ� �̷� ���� ������ ������ ���� ������.


void SendPacket(int id, void* p)
{
	unsigned char* packet = reinterpret_cast<unsigned char*>(p);
	OVER_EX* send_over = new OVER_EX;
	memcpy(&send_over->iocp_buf, packet, packet[0]);
	send_over->op_mode = OP_MODE_SEND;
	send_over->wsa_buf.buf = reinterpret_cast<CHAR*>(send_over->iocp_buf);
	send_over->wsa_buf.len = packet[0];
	ZeroMemory(&send_over->wsa_over, sizeof(send_over->wsa_over));
	//Ŭ���̾�Ʈ�� ���Ͽ� �����ϹǷ� lock
	//g_clients[id].c_lock.lock();
	//	WSASend(g_clients[id].sock, &send_over->wsa_buf, 1, NULL, 0, &send_over->wsa_over, NULL);
	//g_clients[id].c_lock.unlock();
	// ����ȭ
	bool connected = true;
	if (true == atomic_compare_exchange_strong(&g_clients[id].connected, &connected, true)) {
		WSASend(g_clients[id].sock, &send_over->wsa_buf, 1, NULL, 0, &send_over->wsa_over, NULL);
	}
}

void SendLeavePacket(int to_id, int id)
{
	sc_packet_leave packet;
	packet.id = id;
	packet.size = sizeof(packet);
	packet.type = SC_PACKET_LEAVE;
	SendPacket(to_id, &packet);
}

void SendLoginOK(int id)
{
	sc_packet_login_ok p;
	p.exp = g_clients[id].exp;
	p.hp = MAX_PLAYERHP;
	p.id = id;
	p.level = g_clients[id].level;
	p.size = sizeof(p);
	p.type = SC_PACKET_LOGIN_OK;
	p.x = g_clients[id].x;
	p.y = g_clients[id].y;
	SendPacket(id, &p); // p�� ������ struct�� ���� ī�ǵż� ������
}

void SendEnterPacket(int to_id, int new_id)
{
	sc_packet_enter p;
	p.id = new_id;
	p.size = sizeof(p);
	p.type = SC_PACKET_ENTER;
	p.x = g_clients[new_id].x;
	p.y = g_clients[new_id].y;
	strcpy_s(p.name, g_clients[new_id].name);
	p.o_type = static_cast<char>(g_clients[new_id].m_type);
	SendPacket(to_id, &p);
}


void SendMovePacket(int to_id, int id)
{
	sc_packet_move p;
	p.id = id;
	p.size = sizeof(p);
	p.type = SC_PACKET_MOVE;
	p.x = g_clients[id].x;
	p.y = g_clients[id].y;
	p.move_time = g_clients[id].move_time;
	SendPacket(to_id, &p);
}

void SendChatPacket(int to_client, int id, char* mess)
{
	sc_packet_chat p;
	p.id = id;
	p.size = sizeof(p);
	p.type = SC_PACKET_CHAT;
	strcpy_s(p.message, mess);
	SendPacket(to_client, &p);
}

void SendStatChangePacket(int id) {
	sc_packet_stat_change p;
	p.exp = g_clients[id].exp;
	p.hp = g_clients[id].hp;
	p.id = id;
	p.level = g_clients[id].level;
	p.size = sizeof(p);
	p.type = SC_PACKET_STAT_CHANGE;
	SendPacket(id, &p);
}

void SendGameOverPacket(int id)
{
}


int calcDist(int p1, int p2) {
	int dist = (g_clients[p1].x - g_clients[p2].x) * (g_clients[p1].x - g_clients[p2].x);
	dist += (g_clients[p1].y - g_clients[p2].y) * (g_clients[p1].y - g_clients[p2].y);
	return (int)(sqrt(dist));
}

int calcDistX(int p1, int p2) {
	return abs(g_clients[p1].x - g_clients[p2].x);
}

int calcDistY(int p1, int p2) {
	return abs(g_clients[p1].y - g_clients[p2].y);
}

bool IsNear(int p1, int p2)
{
	int dist = (g_clients[p1].x - g_clients[p2].x) * (g_clients[p1].x - g_clients[p2].x);
	dist += (g_clients[p1].y - g_clients[p2].y) * (g_clients[p1].y - g_clients[p2].y);
	return dist <= VIEW_LIMIT * VIEW_LIMIT;
}

bool IsCollide(int p1, int p2) {
	return (g_clients[p1].x == g_clients[p2].x && g_clients[p1].y == g_clients[p2].y);
}

bool IsPlayer(int p1) {
	return p1 < MAX_USER;
}

bool IsNPC(int p1)
{
	return p1 >= MAX_USER && p1 < NUM_NPC + MAX_USER;
}

bool IsObstacle(int p1) {
	return p1 >= MAX_USER + NUM_NPC && p1 < NUM_NPC + MAX_USER + NUM_OBSTACLE;
}

bool IsItem(int p1) {
	return p1 >= MAX_USER + NUM_NPC + NUM_OBSTACLE &&  p1 < NUM_NPC + MAX_USER + NUM_OBSTACLE + NUM_ITEM;
}

bool IsInvincible(int p1) {
	if (g_clients[p1].invincible_timeout < high_resolution_clock::now()) {
		return false;
	}
	return true;
}

void ErrorDisplay(const char* msg, int err_no) {
	WCHAR* h_mess;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&h_mess, 0, NULL);
	cout << msg;
	wcout << L"���� =>" << h_mess << std::endl;
	while (true);
	LocalFree(h_mess);
}

void InitializeNPC()
{
	cout << "Initializing NPCs\n";
	for(int i = MAX_USER; i < MAX_USER + NUM_NPC; ++i) {
		int x = g_clients[i].x = rand() % WORLD_WIDTH;
		int y = g_clients[i].y = rand() % WORLD_HEIGHT;
		int sx = g_clients[i].sx = x / S_SIZE; // sector
		int sy = g_clients[i].sy = y / S_SIZE;
		sector_l.lock();
		g_sector[sx][sy].insert(i);
		sector_l.unlock();
		char npc_name[50];
		sprintf_s(npc_name, "NPC%d", i);
		g_clients[i].c_lock.lock();
		strcpy_s(g_clients[i].name, npc_name);
		g_clients[i].is_active = false;
		g_clients[i].hp = MAX_MONSTERHP;
		g_clients[i].m_type = rand() % 3;
		g_clients[i].c_lock.unlock();
		// ���� �ӽ� ����
		lua_State* L = g_clients[i].L = luaL_newstate();
		luaL_openlibs(L);
		int error = luaL_loadfile(L, "monster.lua");
		error = lua_pcall(L, 0, 0, 0);
		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 1, 0);
		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		lua_register(L, "API_RandomMove", API_RandomMove);
	}
	cout << "Initializing NPCs finishied.\n";
}

void InitializeNetwork()
{
	WSADATA WSAdata;
	int ret = WSAStartup(MAKEWORD(2, 0), &WSAdata);
	if (ret != 0) 
		ErrorDisplay("WSAStartup()", 0);

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	g_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_listenSocket), h_iocp, KEY_SERVER, 0); 

	SOCKADDR_IN serverAddress;
	memset(&serverAddress, 0, sizeof(SOCKADDR_IN));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	::bind(g_listenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
	listen(g_listenSocket, SOMAXCONN);

	SOCKET cSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_accept_over.op_mode = OP_MODE_ACCEPT;
	g_accept_over.wsa_buf.len = static_cast<int>(cSocket); 
	ZeroMemory(&g_accept_over.wsa_over, sizeof(WSAOVERLAPPED));
	AcceptEx(g_listenSocket, cSocket, g_accept_over.iocp_buf, 0, 32, 32, NULL, &g_accept_over.wsa_over);

}

void InitializeObstacle() {
	cout << "Initializing Obstacles\n";

	for (int i = MAX_USER + NUM_NPC; i < MAX_USER + NUM_NPC + NUM_OBSTACLE; ++i) {
		g_clients[i].x = rand() % WORLD_WIDTH;
		g_clients[i].y = rand() % WORLD_HEIGHT;
		g_clients[i].id = i;
		int sx = g_clients[i].x / S_SIZE;
		int sy = g_clients[i].y / S_SIZE;
		sector_l.lock();
		g_sector[sx][sy].insert(i);
		sector_l.unlock();
	}

	cout << "Initializing Obstacles finishied.\n";
}

void InitializeItem() {
	cout << "Initializing Items\n";
	for (int i = MAX_USER + NUM_NPC + NUM_OBSTACLE; i < MAX_USER + NUM_NPC + NUM_OBSTACLE+NUM_ITEM; ++i) {
		g_clients[i].x = rand() % WORLD_WIDTH;
		g_clients[i].y = rand() % WORLD_HEIGHT;
		g_clients[i].m_type = rand() % 2 + OTYPE_ITEM_HP;
		g_clients[i].id = i;
		int sx = g_clients[i].x / S_SIZE;
		int sy = g_clients[i].y / S_SIZE;
		sector_l.lock();
		g_sector[sx][sy].insert(i);
		sector_l.unlock();
	}
	cout << "Initializing Items finishied.\n";
}


void RandomMoveNPC(int id)
{
	int x = g_clients[id].x;
	int y = g_clients[id].y;
	int sx = x / S_SIZE;
	int sy = y / S_SIZE;

	if (g_clients[id].hp <= 0) {
		g_clients[id].is_active = false;
		g_clients[id].connected = false;
		int sx = g_clients[id].sx = x / S_SIZE;
		int sy = g_clients[id].sy = y / S_SIZE;
		sector_l.lock();
		g_sector[sx][sy].erase(id);
		sector_l.unlock();
		StatChange_MonsterDead(g_clients[id].attackme_id, id);
		for (auto i : g_sector[sx][sy]) {
			if (IsPlayer(i)) {
				SendLeavePacket(i, id); // player���� npc�� leave �ϰԲ�
			}
		}
		return;
	}


	// �̵� �� viewlist
	unordered_set<int> o_vl;
	for (auto i : g_sector[sx][sy]) {
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i))
			o_vl.insert(i);
	}
	int type = g_clients[id].m_type;
	if (type == 1) // �ι�
	{
		// �̵�
		switch (rand() % 4) {
		case 0:
			if (x > 0)
				x--;
			break;
		case 1:
			if (x < WORLD_WIDTH - 1)
				x++;
			break;
		case 2:
			if (y > 0)
				y--;
			break;
		case 4:
			if (y < WORLD_HEIGHT - 1)
				y++;
			break;
		}
	}
	else if (type == 2) { // ��׷�
		// �̵�
		int dist = 0, nearPl = 0, nearPlX = 0, nearPlY;
		for (auto i : g_sector[sx][sy]) { // ���� ����� �÷��̾� ã��
			if (dist < calcDist(i, id)) {
				nearPl = i;
			}
		}
		// ����� �÷��̾ ��/�Ʒ�?��/��?�� �ִ��� �˻�
		nearPlX = g_clients[nearPl].x;
		nearPlY = g_clients[nearPl].y;
		if (nearPlX <= x) {// ���ʿ� ����
			if (nearPlY <= y) { // ���� ����
				if (abs(nearPlX - x) < abs(nearPlY - y)) // x�Ÿ��� �� ������
					y--;
				else
					x--;
			}
			if (nearPlY > y) { // �Ʒ��� ����
				if (abs(nearPlX - x) < abs(nearPlY - y)) // x�Ÿ��� �� ������
					y++;
				else
					x--;
			}
		}
		else if(nearPlX > x) {// �����ʿ� ����
			if (nearPlY <= y) { // ���� ����
				if (abs(nearPlX - x) < abs(nearPlY - y)) // x�Ÿ��� �� ������
					y--;
				else
					x++;
			}
			if(nearPlY > y) { // �Ʒ��� ����
				if (abs(nearPlX - x) < abs(nearPlY - y)) // x�Ÿ��� �� ������
					y++;
				else
					x++;
			}
		}
	}
	// �̵� �� ��ǥ
	int cur_x = g_clients[id].x = x;
	int cur_y = g_clients[id].y = y;
	int cur_sx = cur_x / S_SIZE;
	int cur_sy = cur_y / S_SIZE;

	// sector ó�� -------------------
	// sx, sy�� cur_sx, cur_sy�� �����ϴٸ� -> �״��
	// �ٸ��� -> ���� ���Ϳ��� erase, ���ο� ���Ϳ� insert
	if (sx != cur_sx || sy != cur_sy) {
		sector_l.lock();
		g_sector[sx][sy].erase(id);
		g_sector[cur_sx][cur_sy].insert(id);
		sector_l.unlock();
	}
	// �̵� �� viewlist
	unordered_set<int> n_vl;
	for (auto i : g_sector[cur_sx][cur_sy]) {
		if (id == i) continue;
		if (false == g_clients[i].connected) continue;
		if (true == IsNear(id, i))
			n_vl.insert(i);
	}
	// �̵� �� ó��
	for (auto pl : o_vl) {
		if (0 < n_vl.count(pl)) {
			if (0 < g_clients[pl].view_list.count(id)) {
				SendMovePacket(pl, id);
			}
			else {
				g_clients[pl].c_lock.lock();
				g_clients[pl].view_list.insert(id);
				g_clients[pl].c_lock.unlock();
				SendEnterPacket(pl, id);
			}
		}
		else {
			if (0 < g_clients[pl].view_list.count(id)) {
				g_clients[pl].c_lock.lock();
				g_clients[pl].view_list.erase(id);
				g_clients[pl].c_lock.unlock();
				SendLeavePacket(pl, id);
			}
		}
	}
	for (auto pl : n_vl) {
		if (0 == g_clients[pl].view_list.count(pl)) {
			if (0 == g_clients[pl].view_list.count(id)) {
				g_clients[pl].c_lock.lock();
				g_clients[pl].view_list.insert(id);
				g_clients[pl].c_lock.unlock();
				SendEnterPacket(pl, id);
			}
			else {
				SendMovePacket(pl, id);
			}
		}
	}
	// �̵� �� �÷��̾�� �浹�ϴ���
	for (auto pl : n_vl) {
		if (IsCollide(pl, id)) {
			StatChange_MonsterCollide(pl, id);
		}
	}

	// �ι� ai
	// AI Script�� ���� randommove�� ��� : 3ĭ �̵� ��� �������� bye ä�� �޽��� ����ϰԲ� 
	if (g_clients[id].m_type == 1) {
		if (g_clients[id].is_AIrandommove) {
			g_clients[id].c_lock.lock();
			g_clients[id].cnt_randommove++;
			g_clients[id].c_lock.unlock();
			if (g_clients[id].cnt_randommove >= 3) {
				char mess[5] = "BYE";
				SendChatPacket(g_clients[id].encountered_id, id, mess);
				g_clients[id].c_lock.lock();
				g_clients[id].is_AIrandommove = false;
				g_clients[id].cnt_randommove = 0;
				g_clients[id].c_lock.unlock();
			}
		}
	}

	if (true == n_vl.empty()) {
		g_clients[id].is_active = false;
	}
	else {
		AddEventToTimer(id, OP_RANDOM_MOVE, system_clock::now() + 1s);
	}

	for (auto pc : n_vl) {
		OVER_EX* over_ex = new OVER_EX;
		over_ex->object_id = pc;
		over_ex->op_mode = OP_PLAYER_MOVE_NOTIFY;
		PostQueuedCompletionStatus(h_iocp, 1, id, &over_ex->wsa_over);
	}


}

void WakeUpNPC(int id)
{
	bool b = false;
	if (true == g_clients[id].is_active.compare_exchange_strong(b, true))
	{
		AddEventToTimer(id, OP_RANDOM_MOVE, system_clock::now() + 1s);
	}
}

void AddEventToTimer(int obj_id, int ev_type, system_clock::time_point t) {
	event_info ev{ obj_id, t, ev_type };
	timer_l.lock();
	event_queue.push(ev);
	timer_l.unlock();
}

void PlayerHPPlus(int id) {
	if (g_clients[id].hp < MAX_PLAYERHP) {
		g_clients[id].hp += int((float)MAX_PLAYERHP * 0.1f);
		SendStatChangePacket(id);
	}
	// �ٽ� 
	AddEventToTimer(id, OP_PLAYER_HP_PLUS, system_clock::now() + 5s);
}

int API_SendMessage(lua_State* L) {
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 3);

	SendChatPacket(user_id, my_id, mess);
	return 0;
}

int API_get_y(lua_State* L) {
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = g_clients[user_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_get_x(lua_State* L) {
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = g_clients[user_id].x;
	lua_pushnumber(L, x);
	return 1;
}

int API_RandomMove(lua_State* L) {
	int monster_id = (int)lua_tointeger(L, -2);
	int user_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	g_clients[monster_id].cnt_randommove = 0;
	g_clients[monster_id].is_AIrandommove = true;
	g_clients[monster_id].encountered_id = user_id;
	return 0;
}

void InitializeDB()
{
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				retcode = SQLConnect(hdbc, (SQLWCHAR*)L"kirby_db", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				cout << "DB ���� �Ϸ�" << endl;
			}
		}

	}

}

void LoadDB(const string name, int id) {
	wstring w_name;
	w_name.assign(name.begin(), name.end());
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	retcode = SQLExecDirect(hstmt, (SQLWCHAR*)((L"SELECT USER_ID, USER_LEVEL, USER_X, USER_Y, USER_EXP FROM Table_User WHERE USER_ID = '" + w_name + L"'").c_str()), SQL_NTS);


	SQLINTEGER LEVEL = 0, X = 0, Y = 0, EXP = 0;
	SQLWCHAR USERID[10] = L"";
	SQLLEN cbNAME = 0, cbLEVEL = 0, cbX = 0, cbY = 0, cbEXP = 0;


	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

		retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, USERID, 10, &cbNAME);
		retcode = SQLBindCol(hstmt, 2, SQL_C_LONG, &LEVEL, 100, &cbLEVEL);
		retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &X, 100, &cbX);
		retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &Y, 100, &cbY);
		retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &EXP, 100, &cbEXP);

		retcode = SQLFetch(hstmt);

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			cout << "DB���� ��ġ�ϴ� ID ������ �ҷ��ɴϴ�." << name << endl;
			std::wstring str = USERID;
			g_clients[id].c_lock.lock();
			copy(str.begin(), str.end(), g_clients[id].name);
			g_clients[id].level = LEVEL;
			g_clients[id].exp = EXP;
			g_clients[id].x = X;
			g_clients[id].y = Y;

			// ����
			int sx = g_clients[id].sx = X / S_SIZE;
			int sy = g_clients[id].sy = Y / S_SIZE;
			sector_l.lock();
			g_sector[sx][sy].insert(id);
			sector_l.unlock();
			g_clients[id].hp = 100;
			g_clients[id].c_lock.unlock();
		}
		else {

			cout << "DB�� ��ġ�ϴ� ID�� �����ϴ�. ���ο� ���̵� ����ϴ�." << name << endl;

			g_clients[id].c_lock.lock();
			copy(name.begin(), name.end(), g_clients[id].name);
			int x = g_clients[id].x = rand() % WORLD_WIDTH;
			int y = g_clients[id].y = rand() % WORLD_HEIGHT;
			g_clients[id].hp = MAX_PLAYERHP;
			g_clients[id].exp = 0;
			g_clients[id].level = 1;
			g_clients[id].c_lock.unlock();
			// ����
			int sx = g_clients[id].sx = x / S_SIZE;
			int sy = g_clients[id].sy = y / S_SIZE;
			sector_l.lock();
			g_sector[sx][sy].insert(id);
			sector_l.unlock();

			DB_ERROR(hstmt, SQL_HANDLE_STMT, retcode);
		}
		SQLFreeStmt(hstmt, SQL_DROP);
	}


}

void SaveDB(string name, int lv, int x, int y, int exp) {
	wstring w_name;
	w_name.assign(name.begin(), name.end());
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
	retcode = SQLExecDirect(hstmt, (SQLWCHAR*)(L"Exec saveDB " +
		w_name + L","
		+ std::to_wstring(lv) + L","
		+ std::to_wstring(x) + L","
		+ std::to_wstring(y) + L","
		+ std::to_wstring(exp)).c_str(), SQL_NTS);

	if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)DB_ERROR(hstmt, SQL_HANDLE_STMT, retcode);
	SQLFreeStmt(hstmt, SQL_DROP);
	cout << "SaveDB " << name << endl;
}

void CloseDB() {
	SQLDisconnect(hdbc);
	SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	SQLFreeHandle(SQL_HANDLE_ENV, henv);
}

void DB_ERROR(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
	if (RetCode == SQL_INVALID_HANDLE) {
		//fwprintf(stderr, L"Invalid handle!\n");
		wcout << L"invalid handle" << endl;
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			wcout << L"[" << wszState << L"]" << wszMessage << "(" << iError << ")" << endl;
		}
	}
}
