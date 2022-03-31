#include "atem.h"

Atem::Atem() {}

Atem::~Atem() {}

bool Atem::isClosed() { return this->conn_state == connStateClosed; }
bool Atem::isConnected() { return this->conn_state == connStateConnected; }
bool Atem::isConnecting() { return this->conn_state == connStateConnecting; }

void Atem::init() {
  session_id = 0;
  last_session_id = 0;

  remote_packet_id = 0;
  last_remote_packet_id = 0;

  local_packet_id = 0;

  conn_state = connStateClosed;

  chan_values.assign(chan_values.size(), 0.0f);
  topology_values.assign(topology_values.size(), 0.0f);
}

void Atem::start() {
  cout << "thread start" << endl;
  active = true;

  udp.setup(atem_ip);

  recv_thread = thread([this]() {
    while (active) {
      vector<unsigned char> packet = udp.recvPacket();
      if (packet.size() > 0) {
        parsePacket(packet);
      }
    }
  });

  send_thread = thread([this]() {
    while (active) {
      while (!que.empty()) {
        // cout << "send packet: " << session_id << endl;
        udp.sendPacket(que.front());
        que.pop();
      }
    }
  });

  sendPacketStart();
}

void Atem::stop() {
  cout << "thread stop" << endl;
  active = false;

  if (recv_thread.joinable()) recv_thread.join();

  if (send_thread.joinable()) send_thread.join();

  init();

  udp.teardown();
}

void Atem::sendPacket(vector<uint8_t> packet) {
  packet[2] = uint8_t(session_id >> 0x08);
  packet[3] = uint8_t(session_id & 0xff);

  que.push(packet);

  // for reconnect
  last_send_packet_t = time(NULL);
}

void Atem::sendPacketStart() {
  // cout << "send packet for start" << endl;
  sendPacket(start_packet);
  conn_state = connStateConnecting;
}

void Atem::sendPacketAck() {
  // cout << "send packet for ack: " << remote_packet_id << endl;
  vector<uint8_t> packet(ack_packet);

  packet[4] = uint8_t(remote_packet_id >> 0x08);
  packet[5] = uint8_t(remote_packet_id & 0xff);

  this->sendPacket(packet);
}

void Atem::parseSessionID(vector<unsigned char> packet) {
  uint16_t sid = word(packet[2], packet[3]);
  if (sid > last_session_id) {
    session_id = sid;
    last_session_id = sid;
  }
}

void Atem::parseRemotePacketID(vector<unsigned char> packet) {
  uint16_t rid = word(packet[10], packet[11]);
  if (rid > last_remote_packet_id) {
    remote_packet_id = rid;
    last_remote_packet_id = rid;
  }
}

void Atem::parsePacket(vector<unsigned char> packet) {
  parseSessionID(packet);
  parseRemotePacketID(packet);

  uint8_t flags = packet[0] >> 3;

  if ((flags & flagHelloPacket) && isConnecting()) {
    // cout << "received hello" << endl;
    sendPacketAck();
  }

  if ((flags & flagAck) && isConnecting()) {
    conn_state = connStateConnected;
    // cout << "received ack: " << session_id << endl;
  }

  if ((flags & flagAckRequest) && !isClosed()) {
    if (flags & flagResend) {
      return;
    }

    last_recv_packet_t = time(NULL);
    // cout << "received ack request: " << session_id << " : " <<
    // remote_packet_id << endl;
    sendPacketAck();

    if (packet.size() > 12) {
      packet.erase(packet.begin(), packet.begin() + 12);
      parseCommand(packet);
    }
  }
}

void Atem::parseCommand(vector<uint8_t> packet) {
  uint16_t length = word(packet[0], packet[1]);
  string command = string(packet.begin() + 4, packet.begin() + 8);

  vector<uint8_t> data(packet.begin() + 8, packet.begin() + length);
  readCommand(command, data);

  if (packet.size() > length) {
    packet.erase(packet.begin(), packet.begin() + length);
    parseCommand(packet);
  }
}

void Atem::readCommand(string command, vector<uint8_t> data) {
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

void Atem::readCommandTopology(vector<uint8_t> data) {
  nofMEs = (int)data[0];
  nofSources = (int)data[1];
  nofCGs = (int)data[2];
  nofAuxs = (int)data[3];
  nofDSKs = (int)data[4] | 0x02;
  nofStingers = (int)data[5];
  nofDVEs = (int)data[6];
  nofSSrcs = (int)data[7];

  topology_values[0] = (float)nofMEs;
  topology_values[1] = (float)nofSources;
  topology_values[2] = (float)nofCGs;
  topology_values[3] = (float)nofAuxs;
  topology_values[4] = (float)nofDSKs;
  topology_values[5] = (float)nofStingers;
  topology_values[6] = (float)nofDVEs;
  topology_values[7] = (float)nofSSrcs;

  // for send command
  dcut.assign(nofMEs, false);
  daut.assign(nofMEs, false);
  cpgi.assign(nofMEs, 0);
  cpvi.assign(nofMEs, 0);
  ddsa.assign(nofDSKs, false);

  // for read command
  chan_names.clear();
  chan_values.clear();
  for (int i = 0; i < nofMEs; i++) {
    chan_names.push_back("prgi1");
    chan_values.push_back(0.0f);
  }
  for (int i = 0; i < nofMEs; i++) {
    chan_names.push_back("prvi1");
    chan_values.push_back(0.0f);
  }
  for (int i = 0; i < nofAuxs; i++) {
    chan_names.push_back("auxs1");
    chan_values.push_back(0.0f);
  }
  for (int i = 0; i < nofDSKs; i++) {
    chan_names.push_back("dsks1");
    oss << "dsks" << (i + 1) << "_intran";
    chan_names.push_back(oss.str());
    oss.str("");
    oss.clear();
    oss << "dsks" << (i + 1) << "_inauto";
    chan_names.push_back(oss.str());
    oss.str("");
    oss.clear();
    oss << "dsks" << (i + 1) << "_remain";
    chan_names.push_back(oss.str());
    oss.str("");
    oss.clear();

    chan_values.push_back(0.0f);
    chan_values.push_back(0.0f);
    chan_values.push_back(0.0f);
    chan_values.push_back(0.0f);
  }
}

void Atem::readCommandProductId(vector<uint8_t> data) {
  atem_product_id = readCommandStringUpToNull(data);
}

void Atem::readCommandWarning(vector<uint8_t> data) {
  atem_warning = readCommandStringUpToNull(data);
}

string Atem::readCommandStringUpToNull(vector<uint8_t> data) {
  // string until null is found
  vector<uint8_t>::iterator itr = find(data.begin(), data.end(), uint8_t(0));
  size_t index = distance(data.begin(), itr);
  return string(data.begin(), data.begin() + index);
}

void Atem::readCommandInputProperty(vector<uint8_t> data) {
  uint16_t i = word(data[0], data[1]);

  if (i > 0 && i < 1000) {
    if (i > nofInputs) {
      nofInputs = i;
      topology_values[8] = i;
      atem_input_names.resize(i);
      atem_input_labels.resize(i);
    }

    vector<uint8_t> name_data(data.begin() + 2, data.begin() + 21);
    atem_input_names[i - 1] = readCommandStringUpToNull(name_data);

    vector<uint8_t> label_data(data.begin() + 22, data.begin() + 25);
    atem_input_labels[i - 1] = readCommandStringUpToNull(label_data);
  }
}

void Atem::readCommandMacroRunStatus(vector<uint8_t> data) {
  uint16_t i = word(data[2], data[3]);

  if (i == 65535) {
    fill(atem_macro_run_status.begin(), atem_macro_run_status.end(), 0);
  } else {
    if (i >= nofMacros) {
      nofMacros = int(i + 1);
      topology_values[9] = (float)nofMacros;
      atem_macro_names.resize(nofMacros);
      atem_macro_run_status.resize(nofMacros);
    }

    uint16_t s = (int)data[0];
    atem_macro_run_status[i] = s;
  }
}

void Atem::readCommandMacroProperty(vector<uint8_t> data) {
  int i = (int)data[1];
  int v = (int)data[2];

  if (i >= nofMacros) {
    nofMacros = int(i + 1);
    topology_values[9] = (float)nofMacros;
    atem_macro_names.resize(nofMacros);
    atem_macro_run_status.resize(nofMacros);
  }

  if (v > 0) {
    uint16_t name_length = word(data[4], data[5]);
    string name = string(data.begin() + 8, data.begin() + 8 + name_length);
    atem_macro_names[i] = name;
  } else {
    atem_macro_names[i] = "";
  }
}

void Atem::readCommandProgramInput(vector<uint8_t> data) {
  int m = (int)data[0];
  uint16_t i = word(data[2], data[3]);
  chan_values[m] = (float)i;
}

void Atem::readCommandPreviewInput(vector<uint8_t> data) {
  int m = (int)data[0] + nofMEs;
  uint16_t i = word(data[2], data[3]);
  chan_values[m] = (float)i;
}

void Atem::readCommandAuxSource(vector<uint8_t> data) {
  int i = (int)data[0] + nofMEs * 2;
  uint16_t s = word(data[2], data[3]);
  chan_values[i] = (float)s;
}

void Atem::readCommandDownstreamKeyer(vector<uint8_t> data) {
  int i = data[0] * 4 + nofMEs * 2 + nofAuxs;
  bool onair = (data[1] & 0x01) > 0;
  bool intran = (data[2] & 0x01) > 0;
  bool inauto = (data[3] & 0x01) > 0;
  int remain = (int)data[5];
  chan_values[i] = (float)onair;
  chan_values[static_cast<__int64>(i) + 1] = (float)intran;
  chan_values[static_cast<__int64>(i) + 2] = (float)inauto;
  chan_values[static_cast<__int64>(i) + 3] = (float)remain;
}

void Atem::sendCommand(string command, vector<uint8_t> data) {
  if (!isConnected()) return;

  local_packet_id++;

  int size = 20 + int(data.size());
  vector<uint8_t> packet(size, 0);

  packet[0] = size >> 0x08 | 0x08;
  packet[1] = size & 0xff;
  packet[10] = local_packet_id >> 0x08;
  packet[11] = local_packet_id & 0xff;
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
  vector<uint8_t> data{me, 0, 0, 0};
  sendCommand("DCut", data);
}

void Atem::performAuto(uint8_t me) {
  vector<uint8_t> data{me, 0, 0, 0};
  sendCommand("DAut", data);
}

void Atem::changeProgramInput(uint8_t me, uint16_t source) {
  cpgi[me] = source;

  vector<uint8_t> data{me, 0};
  data.push_back(source >> 0x08);
  data.push_back(source & 0xff);

  sendCommand("CPgI", data);
}

void Atem::changePreviewInput(uint8_t me, uint16_t source) {
  cpvi[me] = source;

  vector<uint8_t> data{me, 0};
  data.push_back(source >> 0x08);
  data.push_back(source & 0xff);

  sendCommand("CPvI", data);
}

void Atem::changeAuxSource(uint8_t index, uint16_t source) {
  caus[index] = source;

  vector<uint8_t> data{1, index};
  data.push_back(source >> 0x08);
  data.push_back(source & 0xff);

  sendCommand("CAuS", data);
}

void Atem::changeDownstreamKeyer(uint8_t keyer, bool onair) {
  cdsl[keyer] = onair;

  vector<uint8_t> data{keyer, 0, 0, 0};
  if (onair) data[1] |= 0x01;

  sendCommand("CDsL", data);
}

void Atem::performDownstreamKeyerAuto(uint8_t keyer) {
  vector<uint8_t> data{keyer, 0, 0, 0};
  sendCommand("DDsA", data);
}

uint16_t Atem::word(uint8_t n1, uint8_t n2) {
  BYTE n[] = {n2, n1};
  WORD* w = reinterpret_cast<WORD*>(&n);
  return uint16_t(*w);
}
