#include <winsock2.h>
#include <stdio.h>
#include <windows.h>

#define MAXLINE 1024
//소켓 정보를 저장하기 위한 구조체다. buf는 데이터를 저장하기 위한 공간으로 dataBuf.buf가 가리킨다.
struct SOCKETINFO
{
	WSAOVERLAPPED overlapped;
	SOCKET fd;
	WSABUF dataBuf;
	char buf[MAXLINE];
	int readn;
	int writen;
};
//소켓 정보를 저장하기 위한 배열을 가리키는 포인터
struct SOCKETINFO *socketArray[WSA_MAXIMUM_WAIT_EVENTS];
//이벤트의 총 개수를 저장한다.
int EventTotal = 0;
//이벤트 정보를 저장하기 위한 배열
WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];
DWORD WINAPI ThreadProc(LPVOID argv);
CRITICAL_SECTION CriticalSection;

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET listen_fd, client_fd;
	struct sockaddr_in server_addr;
	struct SOCKETINFO *sInfo;
	unsigned long ThreadId;
	unsigned long flags;
	unsigned long readn;

	if (argc != 2)
	{
		printf("Usage : %s [port number]\n", argv[0]);
		return 1;
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return 1;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == INVALID_SOCKET)
		return 1;

	ZeroMemory(&server_addr, sizeof(struct sockaddr_in));

	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(3600);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		return 0;
	}
	if (listen(listen_fd, 5) == SOCKET_ERROR)
	{
		return 0;
	}

	//워커 스레드를 만든다.
	if (CreateThread(NULL, 0, ThreadProc, NULL, 0, &ThreadId) == NULL)
	{
		return 1;
	}
	EventTotal = 1;
	while (1)
	{
		//accept 함수로 연결 소켓을 가져온다.
		if ((client_fd = accept(listen_fd, NULL, NULL)) == INVALID_SOCKET)
		{
			return 1;
		}

		EnterCriticalSection(&CriticalSection);
		sInfo = (struct SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
		memset((void *)sInfo, 0x00, sizeof(struct SOCKETINFO));

		socketArray[EventTotal] = sInfo;
		sInfo->fd = client_fd;
		sInfo->dataBuf.len = MAXLINE;
		sInfo->dataBuf.buf = socketArray[EventTotal]->buf;

		//WSACreateEvent() 함수를 이용해서 이벤트 객체를 생성하고, 연결 소켓과 연결한다.
		EventArray[EventTotal] = WSACreateEvent();
		sInfo->overlapped.hEvent = EventArray[EventTotal];

		flags = 0;
		//WSARecv함수를 호출한다. 데이터 완료 이벤트가 발생하면 이벤트 객체로 신호가 전달된다.
		//이벤트 객체가 신호 상태가 된다.
		WSARecv(socketArray[EventTotal]->fd, &socketArray[EventTotal]->dataBuf, 1, &readn, &flags,
			&(socketArray[EventTotal]->overlapped), NULL);
		EventTotal++;
		LeaveCriticalSection(&CriticalSection);

	}
}

DWORD WINAPI ThreadProc(LPVOID argv)
{
	unsigned long readn;
	unsigned long index;
	unsigned long flags;
	int i;
	struct SOCKETINFO *si;

	while (1)
	{
		//이벤트 객체르 ㄹ기다린다.
		if ((index = WSAWaitForMultipleEvents(EventTotal, EventArray, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
		{
			return 1;
		}
		si = socketArray[index - WSA_WAIT_EVENT_0];
		WSAResetEvent(EventArray[index - WSA_WAIT_EVENT_0]);
		
		//이벤트가 발생한 소켓에 에러가 발생했다면, 소켓을 닫고 이벤트 정보 배열과 소켓정보 배열을 정리한다.
		//배열 값을 변경하는 영역은 임계 영역으로 보호한다.
		if (WSAGetOverlappedResult(si->fd, &(si->overlapped), &readn, FALSE, &flags) == FALSE || readn == 0)
		{
			closesocket(si->fd);
			free(si);
			WSACloseEvent(EventArray[index - WSA_WAIT_EVENT_0]);

			EnterCriticalSection(&CriticalSection);
			if (index - WSA_WAIT_EVENT_0 + 1 != EventTotal)
			{
				for (i = index - WSA_WAIT_EVENT_0; i <EventTotal; i++)
				{
					EventArray[i] = EventArray[i + 1];
					socketArray[i] = socketArray[i + 1];
				}
			}
			EventTotal--;
			LeaveCriticalSection(&CriticalSection);
			continue;
		}

		memset((void *)&si->overlapped, 0x00, sizeof(WSAOVERLAPPED));
		si->overlapped.hEvent = EventArray[index - WSA_WAIT_EVENT_0];

		si->dataBuf.len = MAXLINE;
		si->dataBuf.buf = si->buf;
		send(si->fd, si->buf, strlen(si->buf), 0);

		//이벤트가 발생한 소켓으로부터 데이터를 읽어서 전송한다.
		if (WSARecv(si->fd, &(si->dataBuf), 1, &readn, &flags, &(si->overlapped), NULL) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != ERROR_IO_PENDING)
			{
			}
		}

	}
}