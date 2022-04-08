#include <WS2tcpip.h>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.Lib")

#include <iostream>
#include <string>
#include <vector>

#define port 9910

struct Udp {
 private:
  WSAData wsaData;
  SOCKET sock;
  struct sockaddr_in addr;
  char recv_buf[2048];

 public:
  Udp();
  ~Udp();

  void setup(std::string ip);
  void teardown();

  void sendPacket(std::vector<uint8_t> packet);

  std::vector<uint8_t> recvPacket();
};
