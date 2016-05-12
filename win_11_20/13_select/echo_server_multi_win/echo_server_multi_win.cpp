#include <WinSock2.h>
#include <stdio.h>

#define MAX_PACKETLEN 1024
#define nPort 3600

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET listen_s, client_s;
	struct sockaddr_in server_addr, client_addr;
	char szReceiveBuffer[MAX_PACKETLEN];
	int readn, writen;
	int addr_len;
	int fd_num;
	SOCKET sockfd;
	unsigned int i = 0;

	fd_set readfds, allfds;
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
	{
		printf("WSA Error\n");
		return 1;
	}

	listen_s = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_s == INVALID_SOCKET)
	{
		printf("socket error\n");
		return 1;
	}
	ZeroMemory(&server_addr, sizeof(struct sockaddr_in));

	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(nPort);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(listen_s, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		printf("bind error\n");
		return 1;
	}
	if (SOCKET_ERROR == listen(listen_s, 5))
	{
		printf("listen error\n");
		return 1;
	}
	//readfds 를 초기화 한다.
	FD_ZERO(&readfds);

	//듣기 소켓을 readfds에 추가한다.
	FD_SET(listen_s, &readfds);

	while (1)
	{
		//fd_set를 복사한다.
		allfds = readfds;

		//select 함수로 allfds에 읽기 데이터가 있는 소켓이 있는지를 검사한다.
		fd_num = select(0, &allfds, NULL, NULL, NULL);

		//듣기 소켓에 읽을 데이터가 있는지 검사한다. allfds복사본을 쓰니 원본은 살아있다.
		if (FD_ISSET(listen_s, &allfds))
		{
			addr_len = sizeof(struct sockaddr_in);
			client_s = accept(listen_s, (struct sockaddr*)&client_addr, &addr_len);
			if (client_s == INVALID_SOCKET)
			{
				printf("accept Error\n");
				continue;
			}
			FD_SET(client_s, &readfds);
			continue;
		}

		//연결 소켓에서 데이터를 읽는다. 있으면 recv함수로 데이터를 읽고 읽은 데이터를 send 함수로 클라이언트에 전송한다.
		for ( i = 0; i < allfds.fd_count; i++)
		{
			if (allfds.fd_array[i] == listen_s) continue;
			sockfd = allfds.fd_array[i];
			memset(szReceiveBuffer, 0x00, MAX_PACKETLEN);
			readn = recv(sockfd, szReceiveBuffer, MAX_PACKETLEN, 0);
			if (readn <= 0)
			{
				closesocket(client_s);
				FD_CLR(sockfd, &readfds);
			}
			else
			{
				writen = send(sockfd, szReceiveBuffer, readn, 0);
			}
		}
	}
	closesocket(listen_s);
	WSACleanup();
	return 0;
}

