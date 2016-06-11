#include <stdio.h>
#include <winsock2.h>

#define MAXLINE 1024
//중첩 연산 관련 정보를 전달하기 위한 구조체
struct SocketInfo
{
	OVERLAPPED overlapped;
	SOCKET fd;
	char buf[MAXLINE];
	int readn;
	int writen;
	WSABUF wsabuf;
};

HANDLE g_hlocp;
DWORD WINAPI Thread_func(LPVOID parm)
{
	unsigned long readn;
	unsigned long coKey;
	unsigned long flags;
	struct SocketInfo *sInfo;
	while (1)
	{
		//입출력 완료 보고를 기다린다.
		GetQueuedCompletionStatus(g_hlocp, &readn, &coKey, (LPOVERLAPPED *)&sInfo, INFINITE);
		//만약 읽은 값이 0이면 소켓을 닫는다.
		if (readn == 0)
		{
			closesocket(sInfo->fd);
			free(sInfo);
			continue;
		}
		else
		{
			//값을 제대로 읽었다면 WSASend 함수로 데이터를 쓴다.
			//여기에서는 중첩 속성을 NULL 로 했다. 
			//데이터 전송에는 굳이 중첩 속성을 사용하지 않아도 되기 때문이다.
			WSASend(sInfo->fd, &(sInfo->wsabuf), 1, &readn, 0, NULL, NULL);
		}
		flags = 0;
		memset(sInfo->buf, 0x00, MAXLINE);
		sInfo->readn = 0;
		sInfo->writen = 0;
		WSARecv(sInfo->fd, &(sInfo->wsabuf), 1, &readn, &flags, &sInfo->overlapped, NULL);
	}
}

HANDLE g_Handle[10];
int main(int argc, char **argv)
{
	WSADATA wsaData;
	struct sockaddr_in addr;
	struct SocketInfo *sInfo;
	DWORD threadid;
	SOCKET listen_fd, client_fd;
	unsigned long readn;
	int addrlen;
	unsigned long flags;
	if (argc != 2)
	{
		printf("Usage : %s [port num]\n", argv[0]);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return 1;
	}
	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		return 1;
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		return 1;
	}
	if (listen(listen_fd, 5) == SOCKET_ERROR)
	{
		return 1;
	}
	//새로 CP를 만든다. 아직 소켓 핸들을 지정하지는 않았다.
	CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	
	//워커 스레드를 만든다.
	for (int i = 0; i < 10; i++)
	{
		g_Handle[i] = CreateThread(NULL, 0, Thread_func, 0, 0, &threadid);
	}
	while (TRUE)
	{
		addrlen = sizeof(addr);
		client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addrlen);
		if (client_fd == INVALID_SOCKET)
		{
			return 1;
		}
		sInfo = (struct SocketInfo *)malloc(sizeof(struct SocketInfo));
		sInfo->fd = client_fd;
		sInfo->readn = 0;
		sInfo->writen = 0;
		sInfo->wsabuf.buf = sInfo->buf;
		sInfo->wsabuf.len = MAXLINE;

		//accept 함수로 새로운 클라이언트 연결을 성공적으로 가져왔다면, 연결 소켓을 CP에 등록하고,
		//중첩 입력 연산을 수행한다.
		//이제 이 소켓에 데이터 입력이 완료되면 os는 cp에 대해서 입출력 완료 보고를 하게 되고
		//워커 스레드들 중 하나가 깨어나서 데이터를 처리한다.
		CreateIoCompletionPort((HANDLE)client_fd, g_hlocp, (unsigned long)sInfo, 0);
		WSARecv(sInfo->fd, &sInfo->wsabuf, 1, &readn, &flags, &sInfo->overlapped, NULL);
	}
}