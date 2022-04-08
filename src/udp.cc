#include <udp.h>

Udp::Udp(){};

Udp::~Udp() { teardown(); }

void Udp::setup(std::string ip) {
  int err;

  err = WSAStartup(MAKEWORD(2, 0), &wsaData);
  if (err != 0) {
    switch (err) {
      case WSASYSNOTREADY:
        std::cout << "WSASYSNOTREADY" << std::endl;
        break;
      case WSAVERNOTSUPPORTED:
        std::cout << "WSAVERNOTSUPPORTED" << std::endl;
        break;
      case WSAEINPROGRESS:
        std::cout << "WSAEINPROGRESS" << std::endl;
        break;
      case WSAEPROCLIM:
        std::cout << "WSAEPROCLIM" << std::endl;
        break;
      case WSAEFAULT:
        std::cout << "WSAEFAULT" << std::endl;
        break;
    }
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);

  u_long blocking = 1;
  ioctlsocket(sock, FIONBIO, &blocking);

  memset(recv_buf, 0, sizeof(recv_buf));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &addr.sin_addr.S_un.S_addr);
}

void Udp::teardown() {
  closesocket(sock);
  WSACleanup();
}

void Udp::sendPacket(std::vector<uint8_t> packet) {
  sendto(sock, std::string(std::begin(packet), std::end(packet)).c_str(),
         (int)packet.size(), 0, (struct sockaddr*)&addr, (int)sizeof(addr));
}

std::vector<uint8_t> Udp::recvPacket() {
  int addr_len = sizeof(addr);
  int buf_len = sizeof(recv_buf);
  int size =
      recvfrom(sock, recv_buf, buf_len, 0, (struct sockaddr*)&addr, &addr_len);

  std::vector<uint8_t> packet;
  if (size > 0) {
    packet.assign(recv_buf, recv_buf + size);
  }
  return packet;
}
