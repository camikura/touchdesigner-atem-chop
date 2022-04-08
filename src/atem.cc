#include "atem.h"

Atem::Atem() {}
Atem::~Atem() {}

uint16_t Atem::session_id() const { return session_id_; }
uint16_t Atem::last_session_id() const { return last_session_id_; }
uint16_t Atem::remote_packet_id() const { return remote_packet_id_; }
uint16_t Atem::last_remote_packet_id() const { return last_remote_packet_id_; }
uint16_t Atem::local_packet_id() const { return local_packet_id_; }

std::string Atem::atem_ip() const { return atem_ip_; }
int Atem::atem_port() const { return atem_port_; }
int Atem::active() const { return active_; }
time_t Atem::last_recv_packet_t() const { return last_recv_packet_t_; }
time_t Atem::last_send_packet_t() const { return last_send_packet_t_; }

std::string Atem::atem_product_id() const { return atem_product_id_; }
std::string Atem::atem_warning() const { return atem_warning_; }

int Atem::n_of_mes() const { return n_of_mes_; }
int Atem::n_of_auxs() const { return n_of_auxs_; }
int Atem::n_of_dsks() const { return n_of_dsks_; }
int Atem::n_of_inputs() const { return n_of_inputs_; }
int Atem::n_of_macros() const { return n_of_macros_; }

std::vector<std::string> Atem::chan_names() const { return chan_names_; }
std::vector<float> Atem::chan_values() const { return chan_values_; }

std::vector<std::string> Atem::topology_names() const {
  return topology_names_;
}
std::vector<float> Atem::topology_values() const { return topology_values_; }

std::vector<std::string> Atem::input_names() const { return input_names_; }
std::vector<std::string> Atem::input_labels() const { return input_labels_; }

std::vector<std::string> Atem::macro_names() const { return macro_names_; }
std::vector<int> Atem::macro_run_status() const { return macro_run_status_; }

bool Atem::dcut(uint8_t me) const { return dcut_[me]; }
bool Atem::daut(uint8_t me) const { return dcut_[me]; }
uint16_t Atem::cpgi(uint8_t me) const { return cpgi_[me]; }
uint16_t Atem::cpvi(uint8_t me) const { return cpvi_[me]; }
uint16_t Atem::caus(uint8_t index) const { return caus_[index]; }
bool Atem::cdsl(uint8_t keyer) const { return cdsl_[keyer]; }
bool Atem::ddsa(uint8_t keyer) const { return ddsa_[keyer]; }

// mutators
void Atem::atem_ip(std::string ip) { atem_ip_ = ip; }

void Atem::dcut(uint8_t me, bool flag) { dcut_[me] = flag; }
void Atem::daut(uint8_t me, bool flag) { dcut_[me] = flag; }
void Atem::cpgi(uint8_t me, uint16_t source) { cpgi_[me] = source; }
void Atem::cpvi(uint8_t me, uint16_t source) { cpvi_[me] = source; }
void Atem::caus(uint8_t index, uint16_t source) { caus_[index] = source; }
void Atem::cdsl(uint8_t keyer, bool onair) { cdsl_[keyer] = onair; }
void Atem::ddsa(uint8_t keyer, bool flag) { ddsa_[keyer] = flag; }

bool Atem::isClosed() { return conn_state_ == connStateClosed; }
bool Atem::isConnected() { return conn_state_ == connStateConnected; }
bool Atem::isConnecting() { return conn_state_ == connStateConnecting; }

void Atem::init() {
  session_id_ = 0;
  last_session_id_ = 0;
  remote_packet_id_ = 0;
  last_remote_packet_id_ = 0;
  local_packet_id_ = 0;

  conn_state_ = connStateClosed;

  chan_values_.assign(chan_values_.size(), 0.0f);
  topology_values_.assign(topology_values_.size(), 0.0f);
}

void Atem::start() {
  std::cout << "thread start" << std::endl;
  active_ = true;

  udp_.setup(atem_ip_);

  recv_thread_ = std::thread([this]() {
    while (active_) {
      std::vector<unsigned char> packet = udp_.recvPacket();
      if (packet.size() > 0) {
        parsePacket(packet);
      }
    }
  });

  send_thread_ = std::thread([this]() {
    while (active_) {
      while (!sender_que_.empty()) {
        udp_.sendPacket(sender_que_.front());
        sender_que_.pop();
      }
    }
  });

  sendPacketStart();
}

void Atem::stop() {
  std::cout << "thread stop" << std::endl;
  active_ = false;

  if (recv_thread_.joinable()) recv_thread_.join();
  if (send_thread_.joinable()) send_thread_.join();

  init();

  udp_.teardown();
}

void Atem::sendPacket(std::vector<uint8_t> packet) {
  packet[2] = uint8_t(session_id_ >> 0x08);
  packet[3] = uint8_t(session_id_ & 0xff);

  sender_que_.push(packet);

  // for reconnect
  last_send_packet_t_ = time(NULL);
}

void Atem::sendPacketStart() {
  sendPacket(start_packet_);
  conn_state_ = connStateConnecting;
}

void Atem::sendPacketAck() {
  std::vector<uint8_t> packet(ack_packet_);

  packet[4] = uint8_t(remote_packet_id_ >> 0x08);
  packet[5] = uint8_t(remote_packet_id_ & 0xff);

  sendPacket(packet);
}

void Atem::parseSessionID(std::vector<unsigned char> packet) {
  uint16_t sid = word(packet[2], packet[3]);
  if (sid > last_session_id_) {
    session_id_ = sid;
    last_session_id_ = sid;
  }
}

void Atem::parseRemotePacketID(std::vector<unsigned char> packet) {
  uint16_t rid = word(packet[10], packet[11]);
  if (rid > last_remote_packet_id_) {
    remote_packet_id_ = rid;
    last_remote_packet_id_ = rid;
  }
}

void Atem::parsePacket(std::vector<unsigned char> packet) {
  parseSessionID(packet);
  parseRemotePacketID(packet);

  uint8_t flags = packet[0] >> 3;

  if ((flags & flagHelloPacket) && isConnecting()) {
    sendPacketAck();
  }

  if ((flags & flagAck) && isConnecting()) {
    conn_state_ = connStateConnected;
  }

  if ((flags & flagAckRequest) && !isClosed()) {
    if (flags & flagResend) {
      return;
    }

    last_recv_packet_t_ = time(NULL);
    sendPacketAck();

    if (packet.size() > 12) {
      packet.erase(packet.begin(), packet.begin() + 12);
      parseCommand(packet);
    }
  }
}

void Atem::parseCommand(std::vector<uint8_t> packet) {
  uint16_t length = word(packet[0], packet[1]);
  std::string command = std::string(packet.begin() + 4, packet.begin() + 8);

  std::vector<uint8_t> data(packet.begin() + 8, packet.begin() + length);
  readCommand(command, data);

  if (packet.size() > length) {
    packet.erase(packet.begin(), packet.begin() + length);
    parseCommand(packet);
  }
}

void Atem::readCommand(std::string command, std::vector<uint8_t> data) {
  if (command == "_top") readCommandTopology(data);
  if (command == "_pin") readCommandProductId(data);
  if (command == "Warn") readCommandWarning(data);
  if (command == "InPr") readCommandInputProperty(data);
  if (command == "MPrp") readCommandMacroProperty(data);
  if (command == "MRPr") readCommandMacroRunStatus(data);
  if (command == "PrgI") readCommandProgramInput(data);
  if (command == "PrvI") readCommandPreviewInput(data);
  if (command == "AuxS") readCommandAuxSource(data);
  if (command == "DskS") readCommandDownstreamKeyer(data);
}

void Atem::readCommandTopology(std::vector<uint8_t> data) {
  n_of_mes_ = (int)data[0];
  n_of_sources_ = (int)data[1];
  n_of_cgs_ = (int)data[2];
  n_of_auxs_ = (int)data[3];
  n_of_dsks_ = (int)data[4] | 0x02;
  n_of_stingers_ = (int)data[5];
  n_of_dves_ = (int)data[6];
  n_of_ssrcs_ = (int)data[7];

  topology_values_[0] = (float)n_of_mes_;
  topology_values_[1] = (float)n_of_sources_;
  topology_values_[2] = (float)n_of_cgs_;
  topology_values_[3] = (float)n_of_auxs_;
  topology_values_[4] = (float)n_of_dsks_;
  topology_values_[5] = (float)n_of_stingers_;
  topology_values_[6] = (float)n_of_dves_;
  topology_values_[7] = (float)n_of_ssrcs_;

  // for send command
  dcut_.assign(n_of_mes_, false);
  daut_.assign(n_of_mes_, false);
  cpgi_.assign(n_of_mes_, 0);
  cpvi_.assign(n_of_mes_, 0);
  ddsa_.assign(n_of_dsks_, false);

  // for read command
  chan_names_.clear();
  chan_values_.clear();
  for (int i = 0; i < n_of_mes_; i++) {
    chan_names_.push_back("prgi1");
    chan_values_.push_back(0.0f);
  }
  for (int i = 0; i < n_of_mes_; i++) {
    chan_names_.push_back("prvi1");
    chan_values_.push_back(0.0f);
  }
  for (int i = 0; i < n_of_auxs_; i++) {
    chan_names_.push_back("auxs1");
    chan_values_.push_back(0.0f);
  }
  for (int i = 0; i < n_of_dsks_; i++) {
    chan_names_.push_back("dsks1");
    oss_ << "dsks" << (i + 1) << "_intran";
    chan_names_.push_back(oss_.str());
    oss_.str("");
    oss_.clear();
    oss_ << "dsks" << (i + 1) << "_inauto";
    chan_names_.push_back(oss_.str());
    oss_.str("");
    oss_.clear();
    oss_ << "dsks" << (i + 1) << "_remain";
    chan_names_.push_back(oss_.str());
    oss_.str("");
    oss_.clear();

    chan_values_.push_back(0.0f);
    chan_values_.push_back(0.0f);
    chan_values_.push_back(0.0f);
    chan_values_.push_back(0.0f);
  }
}

void Atem::readCommandProductId(std::vector<uint8_t> data) {
  atem_product_id_ = std::string((const char*)data.data());
}

void Atem::readCommandWarning(std::vector<uint8_t> data) {
  atem_warning_ = std::string((const char*)data.data());
}

void Atem::readCommandInputProperty(std::vector<uint8_t> data) {
  uint16_t i = word(data[0], data[1]);

  if (i > 0 && i < 1000) {
    if (i > n_of_inputs_) {
      input_names_.resize(i);
      input_labels_.resize(i);
      n_of_inputs_ = i;
      topology_values_[8] = i;
    }

    std::vector<uint8_t> name_data(data.begin() + 2, data.begin() + 21);
    input_names_[i - 1] = std::string((const char*)name_data.data());

    std::vector<uint8_t> label_data(data.begin() + 22, data.begin() + 25);
    input_labels_[i - 1] = std::string((const char*)label_data.data());
  }
}

void Atem::readCommandMacroRunStatus(std::vector<uint8_t> data) {
  uint16_t i = word(data[2], data[3]);

  if (i == 65535) {
    fill(macro_run_status_.begin(), macro_run_status_.end(), 0);
  } else {
    if (i >= n_of_macros_) {
      n_of_macros_ = int(i + 1);
      topology_values_[9] = (float)n_of_macros_;
      macro_names_.resize(n_of_macros_);
      macro_run_status_.resize(n_of_macros_);
    }

    uint16_t s = (int)data[0];
    macro_run_status_[i] = s;
  }
}

void Atem::readCommandMacroProperty(std::vector<uint8_t> data) {
  int i = (int)data[1];
  int v = (int)data[2];

  if (i >= n_of_macros_) {
    n_of_macros_ = int(i + 1);
    topology_values_[9] = (float)n_of_macros_;
    macro_names_.resize(n_of_macros_);
    macro_run_status_.resize(n_of_macros_);
  }

  if (v > 0) {
    uint16_t name_length = word(data[4], data[5]);
    std::string name =
        std::string(data.begin() + 8, data.begin() + 8 + name_length);
    macro_names_[i] = name;
  } else {
    macro_names_[i] = "";
  }
}

void Atem::readCommandProgramInput(std::vector<uint8_t> data) {
  int m = (int)data[0];
  uint16_t i = word(data[2], data[3]);
  chan_values_[m] = (float)i;
}

void Atem::readCommandPreviewInput(std::vector<uint8_t> data) {
  int m = (int)data[0] + n_of_mes_;
  uint16_t i = word(data[2], data[3]);
  chan_values_[m] = (float)i;
}

void Atem::readCommandAuxSource(std::vector<uint8_t> data) {
  int i = (int)data[0] + n_of_mes_ * 2;
  uint16_t s = word(data[2], data[3]);
  chan_values_[i] = (float)s;
}

void Atem::readCommandDownstreamKeyer(std::vector<uint8_t> data) {
  int i = data[0] * 4 + n_of_mes_ * 2 + n_of_auxs_;
  bool onair = (data[1] & 0x01) > 0;
  bool intran = (data[2] & 0x01) > 0;
  bool inauto = (data[3] & 0x01) > 0;
  int remain = (int)data[5];
  chan_values_[i] = (float)onair;
  chan_values_[static_cast<__int64>(i) + 1] = (float)intran;
  chan_values_[static_cast<__int64>(i) + 2] = (float)inauto;
  chan_values_[static_cast<__int64>(i) + 3] = (float)remain;
}

void Atem::sendCommand(std::string command, std::vector<uint8_t> data) {
  if (!isConnected()) return;

  local_packet_id_++;

  int size = 20 + int(data.size());
  std::vector<uint8_t> packet(size, 0);

  packet[0] = size >> 0x08 | 0x08;
  packet[1] = size & 0xff;
  packet[10] = local_packet_id_ >> 0x08;
  packet[11] = local_packet_id_ & 0xff;
  packet[12] = uint8_t((8 + command.size()) >> 0x08);
  packet[13] = uint8_t((8 + command.size()) & 0xff);

  for (int i = 0; i < command.size(); i++) {
    packet[static_cast<__int64>(i) + 16] = command[i];
  }
  for (int i = 0; i < data.size(); i++) {
    packet[static_cast<__int64>(i) + 20] = data[i];
  }

  sendPacket(packet);
}

void Atem::performCut(uint8_t me) {
  std::vector<uint8_t> data{me, 0, 0, 0};
  sendCommand("DCut", data);
}

void Atem::performAuto(uint8_t me) {
  std::vector<uint8_t> data{me, 0, 0, 0};
  sendCommand("DAut", data);
}

void Atem::changeProgramInput(uint8_t me, uint16_t source) {
  cpgi_[me] = source;

  std::vector<uint8_t> data{me, 0};
  data.push_back(source >> 0x08);
  data.push_back(source & 0xff);

  sendCommand("CPgI", data);
}

void Atem::changePreviewInput(uint8_t me, uint16_t source) {
  cpvi_[me] = source;

  std::vector<uint8_t> data{me, 0};
  data.push_back(source >> 0x08);
  data.push_back(source & 0xff);

  sendCommand("CPvI", data);
}

void Atem::changeAuxSource(uint8_t index, uint16_t source) {
  caus(index, source);

  std::vector<uint8_t> data{1, index};
  data.push_back(source >> 0x08);
  data.push_back(source & 0xff);

  sendCommand("CAuS", data);
}

void Atem::changeDownstreamKeyer(uint8_t keyer, bool onair) {
  cdsl_[keyer] = onair;

  std::vector<uint8_t> data{keyer, 0, 0, 0};
  if (onair) data[1] |= 0x01;

  sendCommand("CDsL", data);
}

void Atem::performDownstreamKeyerAuto(uint8_t keyer) {
  std::vector<uint8_t> data{keyer, 0, 0, 0};
  sendCommand("DDsA", data);
}

uint16_t Atem::word(uint8_t n1, uint8_t n2) {
  BYTE n[] = {n2, n1};
  WORD* w = reinterpret_cast<WORD*>(&n);
  return uint16_t(*w);
}
