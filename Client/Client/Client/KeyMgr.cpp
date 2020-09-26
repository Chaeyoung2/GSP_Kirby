#include "KeyMgr.h"

KeyMgr* KeyMgr::m_pInstance = nullptr;

KeyMgr::KeyMgr(void)
{
}

KeyMgr::~KeyMgr(void)
{
	ZeroMemory(m_bKeyDown, 256);
	ZeroMemory(m_bKeyUp, 256);
}

bool KeyMgr::StayKeyDown(int nKey)
{
	if (GetAsyncKeyState(nKey) & 0x8000)
		return true;

	return false;
}

bool KeyMgr::OnceKeyDown(int nKey)
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

bool KeyMgr::OnceKeyUp(int nKey)
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
