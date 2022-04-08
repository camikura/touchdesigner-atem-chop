#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "udp.h"

#define flagAckRequest 0x01
#define flagHelloPacket 0x02
#define flagResend 0x04
#define flagRequestNextAfter 0x08
#define flagAck 0x10

#define connStateClosed 0x01
#define connStateConnecting 0x02
#define connStateConnected 0x03

#define checkCloseTerm 3
#define checkReconnTerm 3

struct Atem {
 public:
  Atem();
  virtual ~Atem();

  // accessors
  uint16_t session_id() const;
  uint16_t last_session_id() const;
  uint16_t remote_packet_id() const;
  uint16_t last_remote_packet_id() const;
  uint16_t local_packet_id() const;

  std::string atem_ip() const;
  int atem_port() const;
  int active() const;
  time_t last_recv_packet_t() const;
  time_t last_send_packet_t() const;

  std::string atem_product_id() const;
  std::string atem_warning() const;

  int n_of_mes() const;
  int n_of_auxs() const;
  int n_of_dsks() const;
  int n_of_inputs() const;
  int n_of_macros() const;

  std::vector<std::string> chan_names() const;
  std::vector<float> chan_values() const;

  std::vector<std::string> topology_names() const;
  std::vector<float> topology_values() const;

  std::vector<std::string> input_names() const;
  std::vector<std::string> input_labels() const;

  std::vector<std::string> macro_names() const;
  std::vector<int> macro_run_status() const;

  bool dcut(uint8_t me) const;
  bool daut(uint8_t me) const;
  uint16_t cpgi(uint8_t me) const;
  uint16_t cpvi(uint8_t me) const;
  uint16_t caus(uint8_t index) const;
  bool cdsl(uint8_t keyer) const;
  bool ddsa(uint8_t keyer) const;

  // mutators
  void atem_ip(std::string ip);

  void dcut(uint8_t me, bool flag);
  void daut(uint8_t me, bool flag);
  void cpgi(uint8_t me, uint16_t source);
  void cpvi(uint8_t me, uint16_t source);
  void caus(uint8_t index, uint16_t source);
  void cdsl(uint8_t keyer, bool onair);
  void ddsa(uint8_t keyer, bool flag);

  bool isClosed();
  bool isConnected();
  bool isConnecting();

  void init();
  void start();
  void stop();

  void sendPacket(std::vector<uint8_t> packet);
  void sendPacketStart();
  void sendPacketAck();

  void parsePacket(std::vector<unsigned char> packet);
  void parseCommand(std::vector<uint8_t> packet);
  void readCommand(std::string command, std::vector<uint8_t> data);
  void sendCommand(std::string command, std::vector<uint8_t> data);

  void readCommandTopology(std::vector<uint8_t> data);
  void readCommandProductId(std::vector<uint8_t> data);
  void readCommandWarning(std::vector<uint8_t> data);
  void readCommandInputProperty(std::vector<uint8_t> data);
  void readCommandMacroProperty(std::vector<uint8_t> data);
  void readCommandMacroRunStatus(std::vector<uint8_t> data);

  void readCommandProgramInput(std::vector<uint8_t> data);
  void readCommandPreviewInput(std::vector<uint8_t> data);
  void readCommandAuxSource(std::vector<uint8_t> data);
  void readCommandDownstreamKeyer(std::vector<uint8_t> data);

  void parseSessionID(std::vector<uint8_t> packet);
  void parseRemotePacketID(std::vector<uint8_t> packet);

  uint16_t word(uint8_t n1, uint8_t n2);

  void performCut(uint8_t me);
  void performAuto(uint8_t me);
  void changeProgramInput(uint8_t me, uint16_t source);
  void changePreviewInput(uint8_t me, uint16_t source);
  void changeAuxSource(uint8_t index, uint16_t source);
  void changeDownstreamKeyer(uint8_t keyer, bool onair);
  void performDownstreamKeyerAuto(uint8_t keyer);

 private:
  uint16_t session_id_;
  uint16_t last_session_id_;
  uint16_t remote_packet_id_;
  uint16_t last_remote_packet_id_;
  uint16_t local_packet_id_;

  std::queue<std::vector<uint8_t> > sender_que_;

  std::string atem_ip_;
  int atem_port_;
  int active_ = -1;
  time_t last_recv_packet_t_;
  time_t last_send_packet_t_;

  std::string atem_product_id_;
  std::string atem_warning_;

  std::ostringstream oss_;

  Udp udp_;

  int n_of_mes_ = 0;
  int n_of_sources_ = 0;
  int n_of_cgs_ = 0;
  int n_of_auxs_ = 0;
  int n_of_dsks_ = 0;
  int n_of_stingers_ = 0;
  int n_of_dves_ = 0;
  int n_of_ssrcs_ = 0;
  int n_of_inputs_ = 0;
  int n_of_macros_ = 0;

  std::vector<std::string> chan_names_;
  std::vector<float> chan_values_;

  std::vector<std::string> topology_names_{
      "atemNofMEs",    "atemNofSoures",   "atemNofCGs",  "atemNofAuxs",
      "atemNofDSKs",   "atemNofStingers", "atemNofDVEs", "atemNofSSrcs",
      "atemNofInputs", "atemNofMacros"};

  std::vector<float> topology_values_{0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  std::vector<std::string> input_names_{};
  std::vector<std::string> input_labels_{};

  std::vector<std::string> macro_names_{};
  std::vector<int> macro_run_status_{};

  uint8_t conn_state_;

  std::vector<uint8_t> start_packet_{0x10, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x26, 0x00, 0x00, 0x01, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  std::vector<uint8_t> ack_packet_{0x80, 0x0c, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  std::vector<bool> dcut_{false};
  std::vector<bool> daut_{false};
  std::vector<uint16_t> cpgi_{0};
  std::vector<uint16_t> cpvi_{0};
  std::vector<uint16_t> caus_{0};
  std::vector<bool> cdsl_{false};
  std::vector<bool> ddsa_{false};

  std::thread send_thread_;
  std::thread recv_thread_;
};
