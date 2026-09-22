# C++ IOCP 멀티플레이 게임 서버

> 게임 서버 프로그래밍 팀 프로젝트 「Kirby Chess」

IOCP 기반으로 다수의 클라이언트를 처리하는 멀티플레이 게임 서버를
구현한 프로젝트입니다.

네트워크 처리뿐만 아니라 멀티스레드 동시성 제어, 패킷 재조립,
Sector/View List 기반 시야 처리, Timer 기반 NPC 이벤트,
Lua Script 기반 NPC AI, MSSQL 데이터 저장/로드 등을 구현했습니다.

## 프로젝트 개요

| 항목 | 내용 |
|---|---|
| 개발 기간 | 2020.09.26 ~ 2020.12.20 |
| 개발 환경 | Visual Studio 2019 |
| 언어 | C/C++, Lua |
| 네트워크 | Winsock, IOCP |
| 데이터베이스 | MSSQL 2019, ODBC |

## 주요 구현 내용

- IOCP 기반 비동기 게임 서버
- Worker Thread 기반 I/O 완료 처리
- Timer Thread와 PQCS를 이용한 게임 이벤트 처리
- TCP 패킷 재조립
- CAS / Mutex를 이용한 멀티스레드 동시성 제어
- Sector / View List 기반 관심 영역 관리
- Lua Script 기반 NPC AI
- MSSQL 기반 플레이어 데이터 저장 및 로드

---

## 1. IOCP 기반 서버 구조

초기에는 Overlapped I/O 모델을 사용했으나,
다수 클라이언트를 여러 스레드에서 효율적으로 처리하기 위해
IOCP 기반 구조로 변경했습니다.

서버에서는 여러 Worker Thread가
`GetQueuedCompletionStatus()`를 호출하여 완료된 I/O를 처리합니다.

별도의 Timer Thread에서는 등록된 게임 이벤트를 관리하고,
이벤트 실행 시점이 되면 `PostQueuedCompletionStatus()`를 통해
Worker Thread에 실제 작업을 전달하도록 구성했습니다.

### 주요 스레드

- **Worker Thread**
  - IOCP 완료 통지 처리
  - SEND / RECV 처리
  - Timer Thread에서 전달된 게임 이벤트 처리

- **Timer Thread**
  - 예약된 게임 이벤트 관리
  - 이벤트 실행 시 PQCS를 통해 Worker Thread로 작업 전달

---

## 2. Overlapped I/O 관리

기본 `WSAOVERLAPPED` 구조체를 확장하여
I/O 처리에 필요한 정보를 함께 관리했습니다.

- SEND / RECV 등 I/O 종류
- `WSABUF`
- I/O Buffer
- 대상 Object ID

SEND 작업은 여러 스레드에서 동시에 발생할 수 있으므로
각 SEND 작업마다 별도의 Overlapped 구조체를 동적으로 생성하고,
완료 통지를 받은 이후 해제하도록 구현했습니다.

---

## 3. TCP 패킷 재조립

TCP에서는 하나의 패킷이 여러 번의 `recv`로 나뉘어 수신되거나
여러 패킷이 하나의 버퍼에 함께 들어올 수 있습니다.

이를 처리하기 위해 클라이언트별로

- 패킷 시작 위치
- 다음 recv 시작 위치

를 관리하고, 수신된 데이터에서 완성된 패킷을 순차적으로
분리하여 처리하도록 구현했습니다.

처리 후 남은 데이터는 버퍼 앞으로 이동시키고
다음 `WSARecv()` 위치를 재설정하여 불완전한 패킷을
다음 수신 데이터와 이어서 처리했습니다.

---

## 4. 멀티스레드 동시성 제어

여러 Worker Thread가 동시에 공유 데이터에 접근하기 때문에
데이터 특성에 따라 CAS와 Mutex를 사용했습니다.

### CAS

클라이언트의 접속 상태 등 단순 공유 상태 변경에는
C++11 CAS를 사용하여 Atomic하게 처리했습니다.

### Mutex

Sector처럼 여러 오브젝트의 삽입/삭제가 이루어지는 자료구조는
Mutex로 보호했습니다.

Lock 범위는 실제 자료구조를 변경하는 구간으로 제한하여
동기화로 인한 성능 저하를 줄이도록 구성했습니다.

---

## 5. Sector / View List 기반 시야 처리

전체 월드를 Sector 단위로 나누어 오브젝트를 관리했습니다.

각 Sector에는 현재 위치한 오브젝트의 ID를 저장하고,
오브젝트가 이동하면 기존 Sector에서 제거한 뒤
새로운 Sector에 등록합니다.

각 플레이어는 별도의 **View List**를 가지고 있으며,
이동할 때마다 이전 View List와 새로운 시야 범위를 비교하여

- 새롭게 시야에 들어온 오브젝트 추가
- 시야에서 벗어난 오브젝트 제거

를 수행했습니다.

이를 통해 모든 오브젝트의 정보를 전송하지 않고
주변 오브젝트의 정보만 전달하도록 구성했습니다.

---

## 6. Timer 기반 NPC 이벤트

NPC의 주기적인 행동은 별도의 Timer Thread에서 관리했습니다.

플레이어 이동으로 NPC가 활성화되면
`RANDOM_MOVE` 이벤트를 Timer Queue에 등록합니다.

실행 시간이 된 이벤트는 Timer Thread에서 직접 게임 로직을
수행하지 않고 `PostQueuedCompletionStatus()`를 이용해
Worker Thread로 전달합니다.

이를 통해 Timer Thread가 이벤트 실행으로 장시간 점유되는 것을
방지하고 실제 게임 로직은 Worker Thread에서 수행하도록 구성했습니다.

---

## 7. Lua 기반 NPC AI

NPC의 행동 로직 일부를 Lua Script로 분리했습니다.

플레이어가 이동하면 주변 NPC의 Lua 함수를 호출하고,
Lua Script에서 정의한 행동에 따라 서버 API를 호출합니다.

예를 들어 플레이어와 NPC가 마주친 경우 Lua Script에서

- 메시지 전송
- Random Move

등의 행동을 실행할 수 있도록 구성했습니다.

NPC별로 Lua VM을 하나씩 소유하도록 구성하여
각 NPC의 Script 실행 상태를 분리했습니다.

---

## 8. MSSQL 데이터 저장 / 로드

MSSQL과 ODBC를 이용하여 플레이어 데이터를 저장했습니다.

저장 데이터:

- ID
- Level
- Position X / Y
- EXP

로그인 시 DB에서 플레이어 데이터를 조회하여 게임 상태를 복원하고,
접속 종료 시 Stored Procedure를 호출하여 현재 데이터를 저장했습니다.
