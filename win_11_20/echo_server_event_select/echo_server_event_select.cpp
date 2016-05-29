#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#define MAXLINE 1024
#define PORTNUM 3600
//소켓 정보를 저장하기 위한 구조체
struct SOCKETINFO
{
	SOCKET fd;
	char buf[MAXLINE];
	int	readn;
	int	writen;
};
//이벤트 객체를 저장하기 위한 배열
WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];

//이벤트 객체를 저장한 배열의 크기를 저장하기 위한 변수
int	EventTotal = 0;
struct SOCKETINFO *socketArray[WSA_MAXIMUM_WAIT_EVENTS];

//매개변수로 넘어온 소켓 지정번호 s와 묶어주기 위한 이벤트 객체를 만들고,
//소켓 정보를 소켓 정보 구조체 socketArray에 저장한다.
int	CreateSocketInfo(SOCKET s)
{
	struct SOCKETINFO *sInfo;
	if ((EventArray[EventTotal] = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		printf("Event Failure\n");
		return -1;
	}
	sInfo = (SOCKETINFO *)malloc(sizeof(struct SOCKETINFO));
	memset((void *)sInfo, 0x00, sizeof(struct SOCKETINFO));
	sInfo->fd = s;
	sInfo->readn = 0;
	sInfo->writen = 0;
	socketArray[EventTotal] = sInfo;
	EventTotal++;
	return 1;
}
//소켓이 종료되면 소켓을 닫아주고 이벤트 객체 배열과 소켓 정보 구조체도 정리한다.
//여기에서는 배열의 원소를 일일이 복사하여 관리한다.
//간단하지만 비 효율적인 방법이다.
void freeSocketInfo(int eventIndex)
{
	struct SOCKETINFO *si = socketArray[eventIndex];
	int i;
	closesocket(si->fd);
	free((void *)si);
	if (WSACloseEvent(EventArray[eventIndex]) == TRUE)
	{
		printf("Event Close OK\n");
	}
	else
	{
		printf("Event Close Failure\n");
	}
	for (i = eventIndex; i <EventTotal; i++)
	{
		EventArray[i] = EventArray[i + 1];
		socketArray[i] = socketArray[i + 1];
	}
	EventTotal--;
}

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET sockfd, clientFd;
	WSANETWORKEVENTS networkEvents;
	char buf[MAXLINE];
	int eventIndex;
	int flags;
	struct SOCKETINFO *socketInfo;
	struct sockaddr_in sockaddr;

	//socket() -> listen() -> bind()의 과정을 거친다.
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == INVALID_SOCKET)
		return 1;

	ZeroMemory(&sockaddr, sizeof(struct sockaddr_in));

	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons(3600);
	sockaddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		return 0;
	}
	if (listen(sockfd, 5) == SOCKET_ERROR)
	{
		return 0;
	}

	//socket 함수로 만든 듣기 소켓을 위한 이벤트 객체와 소켓 정보를 만든다.
	if (CreateSocketInfo(sockfd) == -1)
	{
		return 1;
	}

	//WSAEventSelect 함수로 듣기 소켓을 이벤트 객체와 묶어준다. 
	//이벤트 객체는 FD_ACCEPT와 FD_CLOSE 이벤트를 받으면 신호 상태가 된다.
	if (WSAEventSelect(sockfd, EventArray[EventTotal - 1], FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
	{
		return 1;
	}
	while (1)
	{
		//WSAWaitForMultipleEvents 함수로 EventArray에 있는 이벤트 객체의 신호상태를 기다린다.
		eventIndex = WSAWaitForMultipleEvents(EventTotal, EventArray, FALSE, WSA_INFINITE, FALSE);
		if (eventIndex == WSA_WAIT_FAILED)
		{
			printf("Event Wait Failed\n");
		}

		//WSAEnumNetworkEvents 함수로 신호 상태의 이벤트 객체의 정보를 가져온다.
		if (WSAEnumNetworkEvents
			(socketArray[eventIndex - WSA_WAIT_EVENT_0]->fd,
				EventArray[eventIndex - WSA_WAIT_EVENT_0], &networkEvents) == SOCKET_ERROR)
		{
			printf("Event Type Error\n");
		}
		//발생한 이벤트가 클라이언트 요청(FD_ACCEP)이라면, accpet 함수를 호출해서 연결 소켓을 만든다.
		//연결 소켓은 CreateSockInfo 함수와 WSAEventSelect 함수로 이벤트 객체와 묶어준다.
		if (networkEvents.lNetworkEvents& FD_ACCEPT)
		{
			if (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
			{
				break;
			}
			if ((clientFd = accept(socketArray[eventIndex - WSA_WAIT_EVENT_0]->fd, NULL, NULL)) == INVALID_SOCKET)
			{
				break;
			}
			if (EventTotal> WSA_MAXIMUM_WAIT_EVENTS)
			{
				printf("Too many connections\n");
				closesocket(clientFd);
			}
			CreateSocketInfo(clientFd);

			if ((WSAEventSelect(clientFd, EventArray[EventTotal - 1], FD_READ | FD_CLOSE) == SOCKET_ERROR))
			{
				return 1;
			}
		}

		//읽이 이벤트라면 데이터를 읽어서 클라이언트에 쓴다.
		if (networkEvents.lNetworkEvents& FD_READ)
		{
			flags = 0;
			memset(buf, 0x00, MAXLINE);
			socketInfo = socketArray[eventIndex - WSA_WAIT_EVENT_0];
			socketInfo->readn = recv(socketInfo->fd, socketInfo->buf, MAXLINE, 0);
			send(socketInfo->fd, socketInfo->buf, socketInfo->readn, 0);
		}

		//종료 이벤트라면 소켓을 정리한다.
		if (networkEvents.lNetworkEvents& FD_CLOSE)
		{
			printf("Socket Close\n");
			freeSocketInfo(eventIndex - WSA_WAIT_EVENT_0);
		}
	}
}