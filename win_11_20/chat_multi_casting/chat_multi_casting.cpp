#include <winsock2.h>
#include <stdio.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#define MAXLINE 1024

char *msg_exit = "exit\n";

DWORD WINAPI ThreadFunction(LPVOID lpParm);

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET read_fd;
	SOCKET send_fd;
	int flag;
	int addrlen;

	struct sockaddr_in mcast_group;
	struct ip_mreq mreq;

	char name[16];
	char message[MAXLINE];

	if (argc != 4)
	{
		printf("Usage : %s \n", argv[0]);
		return 1;
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup error\n");
		return 1;
	}

	sprintf(name, "%s", argv[3]);
	memset(&mcast_group, 0x00, sizeof(mcast_group));

	mcast_group.sin_family = AF_INET;
	mcast_group.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &mcast_group.sin_addr);

	if (read_fd = socket(AF_INET, SOCK_DGRAM, 0) < 0)
	{
		return 1;
	}

	mreq.imr_multiaddr = mcast_group.sin_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	if (setsockopt(read_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) < 0)
	{
		printf("error : add group\n");
		return 1;
	}
	flag = 1;
	if (setsockopt(read_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)) < 0)
	{
		printf("Socket Reuseaddr Error\n");
		return 1;
	}

	if (bind(read_fd, (struct sockaddr *)&mcast_group, sizeof(mcast_group)) < 0)
	{
		return 1;
	}

	if ((send_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		return 1;
	}

	CreateThread(
		NULL,
		0,
		ThreadFunction,
		&read_fd,
		0,
		NULL
	);
	while (1)
	{
		memset(message, 0x00, MAXLINE);
		fgets(message, MAXLINE, stdin);
		sprintf(message, "%s : %s", name, message);
		if (sendto(send_fd, message, strlen(message), 0, (struct sockaddr *)&mcast_group,
			sizeof(mcast_group)) < strlen(message))
		{
			return 1;
		}
	}
	WSACleanup();
	return 0;
}
DWORD WINAPI ThreadFunction(LPVOID lpParm)
{
	int addrlen;
	char message[MAXLINE];
	struct sockaddr_in from;
	SOCKET read_fd;
	read_fd = *(int *)lpParm;
	addrlen = sizeof(from);
	memset(message, 0x00, MAXLINE);
	if (recvfrom(read_fd, message, MAXLINE, 0, (struct sockaddr*)&from, &addrlen))
	{
		printf("error : recvfrom\n");
		return 1;
	}
	printf("%s", message);
}