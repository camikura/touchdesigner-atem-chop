#pragma once

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#include <vector>

#define port 9910

using namespace std;

class Udp {
private:
	WSAData wsaData;
	SOCKET sock;
	struct sockaddr_in addr;
	string ip;
	char recv_buf[2048];

public:
	Udp();
	~Udp();

	void setup(string ip);
	void teardown();

	string getAddress();
	void setAddress(string ip);
	void sendPacket(vector<uint8_t> packet);

	vector<uint8_t> recvPacket();
};

Udp::Udp()
{
};

Udp::~Udp()
{
	teardown();
}

void Udp::setup(string ip)
{
	WSAStartup(MAKEWORD(2, 0), &wsaData);
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	u_long blocking = 1;
	ioctlsocket(sock, FIONBIO, &blocking);

	memset(recv_buf, 0, sizeof(recv_buf));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.S_un.S_addr = inet_addr(ip.c_str());
}

void Udp::teardown()
{
	closesocket(sock);
	WSACleanup();
}

string Udp::getAddress()
{
	return this->ip;
}

void Udp::setAddress(string ip)
{
	this->ip = ip;

}

void Udp::sendPacket(vector<uint8_t> packet)
{
	int addr_len = sizeof(addr);
	//int buf_len = int(packet.size());
	//cout << buf_len << endl;
	//const char* buf = string(begin(packet), end(packet)).c_str();
	sendto(sock, string(begin(packet), end(packet)).c_str(), packet.size(), 0, (struct sockaddr*) & addr, addr_len);
	//sendto(sock, buf, buf_len, 0, (struct sockaddr*) & addr, addr_len);
}

vector<uint8_t> Udp::recvPacket()
{
	int addr_len = sizeof(addr);
	int buf_len = sizeof(recv_buf);
	int size = recvfrom(sock, recv_buf, buf_len, 0, (struct sockaddr*) &addr, &addr_len);

	vector<uint8_t> packet;
	if (size > 0) {
		packet.assign(recv_buf, recv_buf + size);
	}
	return packet;
}
