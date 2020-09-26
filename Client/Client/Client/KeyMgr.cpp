#include "KeyMgr.h"


KeyMgr::KeyMgr(void)
{
	ZeroMemory(m_bKeyDown, 256);
	ZeroMemory(m_bKeyUp, 256);
}

KeyMgr::~KeyMgr(void)
{
}

bool KeyMgr::StayKeyDown(INT nKey)
{
	if (GetAsyncKeyState(nKey) & 0x8000)
		return true;

	return false;
}

bool KeyMgr::OnceKeyDown(INT nKey)
{
	if (GetAsyncKeyState(nKey) & 0x8000)
	{
		if (m_bKeyDown[nKey] == false)
		{
			m_bKeyDown[nKey] = true;
			return true;
		}
	}
	else
	{
		m_bKeyDown[nKey] = false;
	}
	return false;
}

bool KeyMgr::OnceKeyUp(INT nKey)
{
	if (GetAsyncKeyState(nKey) & 0x8000)
		m_bKeyUp[nKey] = true;
	else
	{
		if (m_bKeyUp[nKey] == true)
		{
			m_bKeyUp[nKey] = false;

			return true;
		}
	}

	return false;
}

bool KeyMgr::IsToggleKey(INT nKey)
{
	//GetKeyState의 0x0001은 이전에 눌렸는지를 체크한다.
	if (GetKeyState(nKey) & 0x0001)
		return true;

	return false;
}
