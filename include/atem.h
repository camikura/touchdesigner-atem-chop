#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "udp.h"

using namespace std;

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
  thread send_thread;
  thread recv_thread;
  int nofMEs = 0;
  int nofSources = 0;
  int nofCGs = 0;
  int nofAuxs = 0;
  int nofDSKs = 0;
  int nofStingers = 0;
  int nofDVEs = 0;
  int nofSSrcs = 0;
  int nofInputs = 0;
  int nofMacros = 0;

  vector<string> chan_names;
  vector<float> chan_values;

  vector<string> topology_names{
      "atemNofMEs",    "atemNofSoures",   "atemNofCGs",  "atemNofAuxs",
      "atemNofDSKs",   "atemNofStingers", "atemNofDVEs", "atemNofSSrcs",
      "atemNofInputs", "atemNofMacros"};
  vector<float> topology_values{0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  vector<string> atem_input_names{};
  vector<string> atem_input_labels{};

  vector<string> atem_macro_names{};
  vector<int> atem_macro_run_status{};

  uint8_t conn_state;

  vector<uint8_t> start_packet{0x10, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x26, 0x00, 0x00, 0x01, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  vector<uint8_t> ack_packet{0x80, 0x0c, 0x00, 0x00, 0x00, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  vector<bool> dcut{false};
  vector<bool> daut{false};
  vector<uint16_t> cpgi{0};
  vector<uint16_t> cpvi{0};
  vector<uint16_t> caus{0};
  vector<bool> cdsl{false};
  vector<bool> ddsa{false};

  uint16_t session_id;
  uint16_t last_session_id;
  uint16_t remote_packet_id;
  uint16_t last_remote_packet_id;
  uint16_t local_packet_id;

  queue<vector<uint8_t> > que;

  string atem_ip;
  int atem_port;
  int active = -1;
  time_t last_recv_packet_t;
  time_t last_send_packet_t;

  string atem_product_id;
  string atem_warning;

  ostringstream oss;

  Udp udp;

  bool isClosed();
  bool isConnected();
  bool isConnecting();

  void init();
  void start();
  void stop();

  void sendPacket(vector<uint8_t> packet);
  void sendPacketStart();
  void sendPacketAck();

  void parsePacket(vector<unsigned char> packet);
  void parseCommand(vector<uint8_t> packet);
  void readCommand(string command, vector<uint8_t> data);
  void sendCommand(string command, vector<uint8_t> data);

  void readCommandTopology(vector<uint8_t> data);
  void readCommandProductId(vector<uint8_t> data);
  void readCommandWarning(vector<uint8_t> data);
  void readCommandInputProperty(vector<uint8_t> data);
  void readCommandMacroProperty(vector<uint8_t> data);
  void readCommandMacroRunStatus(vector<uint8_t> data);

  void readCommandProgramInput(vector<uint8_t> data);
  void readCommandPreviewInput(vector<uint8_t> data);
  void readCommandAuxSource(vector<uint8_t> data);
  void readCommandDownstreamKeyer(vector<uint8_t> data);

  void parseSessionID(vector<uint8_t> packet);
  void parseRemotePacketID(vector<uint8_t> packet);

  uint16_t word(uint8_t n1, uint8_t n2);

  void performCut(uint8_t me);
  void performAuto(uint8_t me);
  void changeProgramInput(uint8_t me, uint16_t source);
  void changePreviewInput(uint8_t me, uint16_t source);
  void changeAuxSource(uint8_t index, uint16_t source);
  void changeDownstreamKeyer(uint8_t keyer, bool onair);
  void performDownstreamKeyerAuto(uint8_t keyer);

  Atem();
  virtual ~Atem();
};
