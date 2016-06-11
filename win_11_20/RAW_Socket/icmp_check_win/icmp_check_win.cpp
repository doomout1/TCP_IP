#include <winsock2.h>
#include <stdio.h>

#define PACKET_LENGTH 65536
#define ICMP_ECHO 8

//윈도우는 IP와 ICMP 혹은 TCP의 패킷 헤더 정보를 저장하기 위한 구조체를 제공하지 않는다.
//그러므로 개발자가 필요에 따라서 자체 개발하여 사용해야 한다.
//여기에서 제공하는 헤더 구조체를 다른 프로그램에도 그대로 가져다 사용할 수 있다.
//구조체의 멤버 변수는 헤더의 필드와 일대일로 대응한다.
struct icmp_hdr
{
	u_char icmp_type;
	u_char icmp_code;
	u_short icmp_cksum;
	u_short icmp_id;
	u_short icmp_seq;
	char icmp_data[1];
};

struct ip_hdr {
	u_char ip_hl;
	u_char ip_v;
	u_char ip_tos;
	short ip_len;
	u_short ip_id;
	short ip_off;
	u_char ip_ttl;
	u_char ip_p;
	u_short ip_cksum;
	struct in_addr ip_src;
	struct in_addr ip_dst;
};
int in_cksum(u_short *p, int n);

int main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET raw_socket;
	struct sockaddr_in addr, from;
	struct icmp_hdr *p, *rp;
	struct ip_hdr *ip;
	char buffer[1024];
	int ret;
	int sl;
	int hlen;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return 1;
	}
	//로우 소켓을 만든다.
	if ((raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == INVALID_SOCKET)
	{
		return 1;
	}
	memset(buffer, 0x00, 1024);

	//전송할 ICMP 요청 데이터를 만든다.
	p = (struct icmp_hdr *)buffer;
	p->icmp_type = ICMP_ECHO;
	p->icmp_code = 0;
	p->icmp_cksum = 0;
	p->icmp_seq = 1;
	p->icmp_id = 1;
	p->icmp_cksum = in_cksum((unsigned short *)p, sizeof(struct icmp_hdr));
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_family = AF_INET;

	//sendto 함수로 ICMP 데이터를 전송한다.
	ret = sendto(raw_socket, (char *)p, sizeof(struct icmp_hdr), 0, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		printf("ICMP Request Error\n");
		return 1;
	}
	sl = sizeof(from);

	//ICMP 응답 데이터를 읽는다.
	ret = recvfrom(raw_socket, buffer, 1024, 0, (struct sockaddr *)&from, &sl);
	if (ret < 0)
	{
		printf("ICMP Error\n");
		return 1;
	}
	ip = (struct ip_hdr *)buffer;
	hlen = ip->ip_hl * 4;
	rp = (struct icmp_hdr *)(buffer + hlen);

	//ICMP 데이터를 출력한다.
	printf("reply from %s\n", inet_ntoa(from.sin_addr));
	printf("Type : %d \n", rp->icmp_type);
	printf("Code : %d \n", rp->icmp_code);
	printf("Seq : %d \n", rp->icmp_seq);
	printf("Iden : %d \n", rp->icmp_id);
	closesocket(raw_socket);
	WSACleanup();
	return 0;
}

int in_cksum(u_short *p, int n)
{
	register u_short answer;
	register long sum = 0;
	u_short odd_byte = 0;
	while (n > 1)
	{
		sum += *p++;
		n -= 2;
	}
	if (n == 1)
	{
		*(u_char*)(&odd_byte) = *(u_char*)p;
		sum += odd_byte;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
}