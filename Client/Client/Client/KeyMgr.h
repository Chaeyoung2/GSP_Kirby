#pragma once

#include "framework.h"

#define KEYMGR KeyMgr::GetInstance()

class KeyMgr
{
	// 싱글톤화
private:
	static KeyMgr* m_pInstance;
public:
	static KeyMgr* GetInstance() {
		if (nullptr == m_pInstance) {
			m_pInstance = new KeyMgr;
			return m_pInstance;
		}
	}
	void DestroyInstance() {
		if (m_pInstance) {
			delete m_pInstance;
			m_pInstance = nullptr;
		}
	}

public:
	bool	m_bKeyDown[256];	// 키가 눌렸는지 체크할 배열
	bool	m_bKeyUp[256];	// 키가 떼졌는지 체크할 배열

private:
	KeyMgr(void);
	~KeyMgr(void);

public:
	bool StayKeyDown(int nKey); // 키가 눌리고 있는지 체크
	bool OnceKeyDown(int nKey); // 키가 한번 눌렸는지 체크
	bool OnceKeyUp(int nKey);	// 키가 한번 눌렸다 떼졌는지 체크
};