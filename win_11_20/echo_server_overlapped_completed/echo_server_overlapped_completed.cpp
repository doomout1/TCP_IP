#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#define MAXLINE 1024
//소켓 정보를 저장하기 위한 구조체 readn과 writen 은 현재 읽은 바이트 수와 써야할 바이트 수를 저장한다.
//이 값은 데이터를 쓸 때 원하는 크기 만큼 모두 보냈는지를 확인하기 위해서 사용한다.
//buf는 읽거나 쓸 데이터를 저장하기 위한 메모리 공간으로 dataBuf.buf가 가리킨다.
struct SOCKETINFO
{
	WSAOVERLAPPED overlapped;
	SOCKET fd;
	WSABUF dataBuf;
	char buf[MAXLINE];
	int readn;
	int writen;
};

void CALLBACK WorkerRoutine(DWORD Error, DWORD readn, LPWSAOVERLAPPED overlapped, DWORD lnFlags);

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET listen_fd, client_fd;
	struct sockaddr_in addr;
	struct SOCKETINFO *sInfo;
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

	ZeroMemory(&addr, sizeof(struct sockaddr_in));

	addr.sin_family = PF_INET;
	addr.sin_port = htons(3600);
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		return 0;
	}
	if (listen(listen_fd, 5) == SOCKET_ERROR)
	{
		return 0;
	}


	while (1)
	{
		//accept 함수로 클라이언트 연결 요첨을 기다린다. 이 함수는 봉쇄모드로 작동한다.
		//그럼 데이터 입력을 어떻게 처리해야 할까?
		//accept 함수에서 대기하고 있는 중이더라도 데이터 입출력이 완료되면 커널이 완료 루틴을 실행시켜 준다.
		if ((client_fd = accept(listen_fd, NULL, NULL)) == INVALID_SOCKET)
		{
			return 1;
		}

		sInfo = (struct SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
		memset((void *)sInfo, 0x00, sizeof(struct SOCKETINFO));
		sInfo->fd = client_fd;
		sInfo->dataBuf.len = MAXLINE;
		sInfo->dataBuf.buf = sInfo->buf;
		flags = 0;

		//accept 함수로 가져온 연결 소켓에 대해서 중첩 소켓 입력을 지정한다.
		//연결 소켓에 읽을 데이터의 입력이 완료되면 완료 루틴인 WorkerRoutine 가 실행된다.
		if (WSARecv(sInfo->fd, &sInfo->dataBuf, 1, &readn, &flags, &(sInfo->overlapped), WorkerRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("wsarecv error %d\n", WSAGetLastError());
			}
		}
	}
	return 1;
}

void CALLBACK WorkerRoutine(DWORD Error, DWORD transfern, LPWSAOVERLAPPED overlapped, DWORD lnFlags)
{
	struct SOCKETINFO *si;

	unsigned long readn, writen;
	unsigned long flags = 0;
	//중첩 입출력 상태 정보를 가지고 있는 overlapped 매개 변수를 포인트 한다.
	si = (struct SOCKETINFO *)overlapped;

	//만약 전송된 데이터가 0이라면 소켓을 닫는다.
	if (transfern == 0)
	{
		closesocket(si->fd);
		free(si);
		return;
	}

	//readn이 0이면 이 워커 루틴은 읽기 작업을 위해서 호출 되었음을 의미한다.
	//그렇지 않다면 쓰기를 위해서 호출 되었음을 의미한다.
	if (si->readn == 0)
	{
		si->readn = transfern;
		si->writen = 0;
	}
	else
	{
		si->writen += transfern;
	}

	//쓰기까지 중첩 특성으로 처리하고 있다. 읽은 데이터를 모두 쓸 때까지 이 루틴을 실행한다.
	if (si->readn>si->writen)
	{
		memset(&(si->overlapped), 0x00, sizeof(WSAOVERLAPPED));
		si->dataBuf.buf = si->buf + si->writen;
		si->dataBuf.len = si->readn - si->writen;

		if (WSASend(si->fd, &(si->dataBuf), 1, &writen, 0, &(si->overlapped), WorkerRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("WSASend Error\n");
			}
		}
	}
	else
	{
		//중첩 입력 데이터를 읽는다. 
		si->readn = 0;
		flags = 0;
		memset(&(si->overlapped), 0x00, sizeof(WSAOVERLAPPED));
		si->dataBuf.len = MAXLINE;
		si->dataBuf.buf = si->buf;
		if (WSARecv(si->fd, &si->dataBuf, 1, &readn, &flags, &(si->overlapped), WorkerRoutine) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf("wsarecv error %d\n", WSAGetLastError());
			}
		}
	}
}