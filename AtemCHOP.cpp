/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <queue>
#include <sstream>
#include <time.h>
#include <algorithm>

#include "Udp.hpp"

#include "CPlusPlus_Common.h"
#include "CHOP_CPlusPlusBase.h"

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

class AtemCHOP : public CHOP_CPlusPlusBase
{
private:
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

	vector<string> topology_names{ "atemNofMEs", "atemNofSoures", "atemNofCGs", "atemNofAuxs", "atemNofDSKs", "atemNofStingers", "atemNofDVEs", "atemNofSSrcs", "atemNofInputs", "atemNofMacros" };
	vector<float> topology_values{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

	vector<string> atem_input_names{ };
	vector<string> atem_input_labels{ };

	vector<string> atem_macro_names{ };
	vector<int> atem_macro_run_status{ };

	uint8_t conn_state;

	vector<uint8_t> start_packet{ 0x10, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	vector<uint8_t> ack_packet{ 0x80, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	vector<bool> dcut{ false };
	vector<bool> daut{ false };
	vector<uint16_t> cpgi{ 0 };
	vector<uint16_t> cpvi{ 0 };
	vector<uint16_t> caus{ 0 };
	vector<bool> cdsl{ false };
	vector<bool> ddsa{ false };

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

	void executeHandleParameters(const OP_Inputs* inputs);
	void executeHandleInputs(const OP_Inputs* inputs);

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

	string readCommandStringUpToNull(vector<uint8_t> data);

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

public:
	AtemCHOP(const OP_NodeInfo* info)
	{
		init();
	}

	virtual ~AtemCHOP()
	{
		stop();
	}

	void getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
	{
		ginfo->cookEveryFrameIfAsked = true;
		ginfo->timeslice = false;
		ginfo->inputMatchIndex = 0;
	}

	bool getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
	{
		info->numSamples = 1;
		if (isConnected()) {
			info->numChannels = int32_t(chan_names.size());
		}
		else {
			info->numChannels = 0;
		}
		return true;
	}

	void getChannelName(int32_t index, OP_String* name, const OP_Inputs* inputs, void* reserved1)
	{
		name->setString(chan_names[index].c_str());
	}

	void execute(CHOP_Output* output, const OP_Inputs* inputs, void* reserved)
	{
		executeHandleParameters(inputs);
		executeHandleInputs(inputs);

		time_t now_t = time(NULL);

		// auto close
		if (int(now_t) - checkCloseTerm > int(last_recv_packet_t) && isConnected()) {
			init();
		}

		// auto reconnect
		if (int(now_t) - checkReconnTerm > int(last_send_packet_t) && !isConnected()) {
			sendPacketStart();
		}

		// provide data
		if (isConnected()) {
			for (int i = 0; i < chan_values.size(); i++) {
				for (int j = 0; j < output->numSamples; j++) {
					output->channels[i][j] = chan_values[i];
				}
			}
		}
	}

	int32_t getNumInfoCHOPChans(void* reserved1)
	{
		int chans = 3 + (int)topology_values.size();
		return chans;
	}

	void getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
	{
		if (index == 0)
		{
			chan->name->setString("atemSessionID");
			chan->value = (float)session_id;
		}
		if (index == 1)
		{
			chan->name->setString("atemRemotePacketID");
			chan->value = (float)remote_packet_id;
		}
		if (index == 2)
		{
			chan->name->setString("atemLocalPacketID");
			chan->value = (float)local_packet_id;
		}
		if (index >= 3)
		{
			chan->name->setString(topology_names[int(static_cast<__int64>(index) - 3)].c_str());
			chan->value = (float)topology_values[int(static_cast<__int64>(index) - 3)];
		}
	}

	bool getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
	{
		infoSize->rows = 2 + nofInputs + nofMacros;
		infoSize->cols = 3;
		infoSize->byColumn = false;
		return true;
	}

	void getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries, void* reserved1)
	{
		// Product Id
		if (index == 0)
		{
			entries->values[0]->setString("Product Id");
			entries->values[1]->setString(atem_product_id.c_str());
			entries->values[2]->setString("");
		}

		// Warning
		else if (index == 1)
		{
			entries->values[0]->setString("Warning");
			entries->values[1]->setString(atem_warning.c_str());
			entries->values[2]->setString("");
		}

		// Input Property
		else if (index < 2 + nofInputs)
		{
			int i = index - 2;
			char key[10];
			sprintf_s(key, "Input%d", int(i + 1));
			entries->values[0]->setString(key);
			entries->values[1]->setString(atem_input_names[i].c_str());
			entries->values[2]->setString(atem_input_labels[i].c_str());
		}

		// Macro Property
		else if (index < 2 + nofInputs + nofMacros)
		{
			int i = index - 2 - nofInputs;
			char key[10];
			sprintf_s(key, "Macro%d", int(i + 1));
			string s = atem_macro_run_status[i] > 0 ? "running" : "";

			entries->values[0]->setString(key);
			entries->values[1]->setString(atem_macro_names[i].c_str());
			entries->values[2]->setString(s.c_str());
		}
	}

	void setupParameters(OP_ParameterManager* manager, void* reserved1)
	{
		{
			OP_StringParameter sp;
			sp.name = "Atemip";
			sp.label = "Atem IP";
			OP_ParAppendResult res = manager->appendString(sp);
			assert(res == OP_ParAppendResult::Success);
		}
	}

	void pulsePressed(const char* name, void* reserved1)
	{
	}
};

/*

*/

bool AtemCHOP::isClosed() { return this->conn_state == connStateClosed; }
bool AtemCHOP::isConnected() { return this->conn_state == connStateConnected; }
bool AtemCHOP::isConnecting() { return this->conn_state == connStateConnecting; }

void AtemCHOP::init()
{
	session_id = 0;
	last_session_id = 0;

	remote_packet_id = 0;
	last_remote_packet_id = 0;

	local_packet_id = 0;

	conn_state = connStateClosed;

	chan_values.assign(chan_values.size(), 0.0f);
	topology_values.assign(topology_values.size(), 0.0f);
}

void AtemCHOP::executeHandleParameters(const OP_Inputs* inputs)
{
	string ip = inputs->getParString("Atemip");
	if (atem_ip != ip || active == -1) {
		atem_ip = ip;
		if (active) stop();
		start();
	}
}

void AtemCHOP::executeHandleInputs(const OP_Inputs* inputs)
{
	for (int i = 0; i < inputs->getNumInputs(); i++) {
		const OP_CHOPInput* cinput = inputs->getInputCHOP(i);
		for (int j = 0; j < cinput->numChannels; j++) {
			string cname = cinput->getChannelName(j);

			if (!strncmp(cname.c_str(), "dcut", 4)) {
				int me = stoi(cname.substr(4, 1)) - 1;
				bool flag = cinput->getChannelData(j)[0] >= 1;
				if (nofMEs > me && dcut[me] != flag) {
					dcut[me] = flag;
					if (flag) performCut(me);
				}
			}
			if (!strncmp(cname.c_str(), "daut", 4)) {
				int me = stoi(cname.substr(4, 1)) - 1;
				bool flag = cinput->getChannelData(j)[0] >= 1;
				if (nofMEs > me && daut[me] != flag) {
					daut[me] = flag;
					if (flag) performAuto(me);
				}
			}
			if (!strncmp(cname.c_str(), "cpgi", 4)) {
				int me = stoi(cname.substr(4, 1)) - 1;
				uint16_t source = uint16_t(cinput->getChannelData(j)[0]);
				if (nofMEs > me && cpgi[me] != source) {
					changeProgramInput(me, source);
				}
			}
			if (!strncmp(cname.c_str(), "cpvi", 4)) {
				int me = stoi(cname.substr(4, 1)) - 1;
				uint16_t source = uint16_t(cinput->getChannelData(j)[0]);
				if (nofMEs > me && cpvi[me] != source) {
					changePreviewInput(me, source);
				}
			}
			if (!strncmp(cname.c_str(), "caus", 4)) {
				int index = stoi(cname.substr(4, 1)) - 1;
				uint16_t source = uint16_t(cinput->getChannelData(j)[0]);
				if (nofAuxs > index && caus[index] != source) {
					changeAuxSource(index, source);
				}
			}
			if (!strncmp(cname.c_str(), "cdsl", 4)) {
				int keyer = stoi(cname.substr(4, 1)) - 1;
				bool onair = cinput->getChannelData(j)[0] >= 1;
				if (nofDSKs > keyer && cdsl[keyer] != onair) {
					changeDownstreamKeyer(keyer, onair);
				}
			}
			if (!strncmp(cname.c_str(), "ddsa", 4)) {
				int keyer = stoi(cname.substr(4, 1)) - 1;
				bool flag = cinput->getChannelData(j)[0] >= 1;
				if (nofDSKs > keyer && ddsa[keyer] != flag) {
					ddsa[keyer] = flag;
					if (flag) performDownstreamKeyerAuto(keyer);
				}
			}
		}
	}
}

void AtemCHOP::start()
{
	cout << "thread start" << endl;
	active = true;

	udp.setup(atem_ip);

	recv_thread = thread([this]() {
		while (active)
		{
			vector<unsigned char> packet = udp.recvPacket();
			if (packet.size() > 0) {
				parsePacket(packet);
			}
		}
	});

	send_thread = thread([this]() {
		while (active)
		{
			while (!que.empty()) {
				//cout << "send packet: " << session_id << endl;
				udp.sendPacket(que.front());
				que.pop();
			}
		}
	});

	sendPacketStart();
}

void AtemCHOP::stop()
{
	cout << "thread stop" << endl;
	active = false;

	if (recv_thread.joinable())
		recv_thread.join();

	if (send_thread.joinable())
		send_thread.join();

	init();

	udp.teardown();
}

void AtemCHOP::sendPacket(vector<uint8_t> packet)
{
	packet[2] = uint8_t(session_id >> 0x08);
	packet[3] = uint8_t(session_id & 0xff);

	que.push(packet);

	// for reconnect
	last_send_packet_t = time(NULL);
}

void AtemCHOP::sendPacketStart()
{
	//cout << "send packet for start" << endl;
	sendPacket(start_packet);
	conn_state = connStateConnecting;
}

void AtemCHOP::sendPacketAck()
{
	//cout << "send packet for ack: " << remote_packet_id << endl;
	vector<uint8_t> packet(ack_packet);

	packet[4] = uint8_t(remote_packet_id >> 0x08);
	packet[5] = uint8_t(remote_packet_id & 0xff);

	this->sendPacket(packet);
}

void AtemCHOP::parseSessionID(vector<unsigned char> packet)
{
	uint16_t sid = word(packet[2], packet[3]);
	if (sid > last_session_id) {
		session_id = sid;
		last_session_id = sid;
	}
}

void AtemCHOP::parseRemotePacketID(vector<unsigned char> packet)
{
	uint16_t rid = word(packet[10], packet[11]);
	if (rid > last_remote_packet_id) {
		remote_packet_id = rid;
		last_remote_packet_id = rid;
	}
}

void AtemCHOP::parsePacket(vector<unsigned char> packet)
{
	parseSessionID(packet);
	parseRemotePacketID(packet);

	uint8_t flags = packet[0] >> 3;

	if ((flags & flagHelloPacket) && isConnecting()) {
		//cout << "received hello" << endl;
		sendPacketAck();
	}

	if ((flags & flagAck) && isConnecting()) {
		conn_state = connStateConnected;
		//cout << "received ack: " << session_id << endl;
	}

	if ((flags & flagAckRequest) && !isClosed()) {
		if (flags & flagResend) {
			return;
		}

		last_recv_packet_t = time(NULL);
		//cout << "received ack request: " << session_id << " : " << remote_packet_id << endl;
		sendPacketAck();

		if (packet.size() > 12) {
			packet.erase(packet.begin(), packet.begin() + 12);
			parseCommand(packet);
		}
	}
}

void AtemCHOP::parseCommand(vector<uint8_t> packet)
{
	uint16_t length = word(packet[0], packet[1]);
	string command = string(packet.begin() + 4, packet.begin() + 8);

	vector<uint8_t> data(packet.begin() + 8, packet.begin() + length);
	readCommand(command, data);

	if (packet.size() > length) {
		packet.erase(packet.begin(), packet.begin() + length);
		parseCommand(packet);
	}
}

void AtemCHOP::readCommand(string command, vector<uint8_t> data)
{
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

void AtemCHOP::readCommandTopology(vector<uint8_t> data)
{
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

void AtemCHOP::readCommandProductId(vector<uint8_t> data)
{
	atem_product_id = readCommandStringUpToNull(data);
}

void AtemCHOP::readCommandWarning(vector<uint8_t> data)
{
	atem_warning = readCommandStringUpToNull(data);
}

string AtemCHOP::readCommandStringUpToNull(vector<uint8_t> data)
{
	// string until null is found
	vector<uint8_t>::iterator itr = find(data.begin(), data.end(), uint8_t(0));
	size_t index = distance(data.begin(), itr);
	return string(data.begin(), data.begin() + index);
}

void AtemCHOP::readCommandInputProperty(vector<uint8_t> data)
{
	uint16_t i = word(data[0], data[1]);

	if (i > 0 && i < 1000)
	{
		if (i > nofInputs)
		{
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

void AtemCHOP::readCommandMacroRunStatus(vector<uint8_t> data)
{
	uint16_t i = word(data[2], data[3]);

	if (i == 65535)
	{
		fill(atem_macro_run_status.begin(), atem_macro_run_status.end(), 0);
	}
	else
	{
		if (i >= nofMacros)
		{
			nofMacros = int(i + 1);
			topology_values[9] = (float)nofMacros;
			atem_macro_names.resize(nofMacros);
			atem_macro_run_status.resize(nofMacros);
		}

		uint16_t s = (int)data[0];
		atem_macro_run_status[i] = s;
	}
}

void AtemCHOP::readCommandMacroProperty(vector<uint8_t> data)
{
	int i = (int)data[1];
	int v = (int)data[2];

	if (i >= nofMacros)
	{
		nofMacros = int(i + 1);
		topology_values[9] = (float)nofMacros;
		atem_macro_names.resize(nofMacros);
		atem_macro_run_status.resize(nofMacros);
	}

	if (v > 0)
	{
		uint16_t name_length = word(data[4], data[5]);
		string name = string(data.begin() + 8, data.begin() + 8 + name_length);
		atem_macro_names[i] = name;
	}
	else
	{
		atem_macro_names[i] = "";
	}
}

void AtemCHOP::readCommandProgramInput(vector<uint8_t> data)
{
	int m = (int)data[0];
	uint16_t i = word(data[2], data[3]);
	chan_values[m] = (float)i;
}

void AtemCHOP::readCommandPreviewInput(vector<uint8_t> data)
{
	int m = (int)data[0] + nofMEs;
	uint16_t i = word(data[2], data[3]);
	chan_values[m] = (float)i;
}

void AtemCHOP::readCommandAuxSource(vector<uint8_t> data)
{
	int i = (int)data[0] + nofMEs * 2;
	uint16_t s = word(data[2], data[3]);
	chan_values[i] = (float)s;
}

void AtemCHOP::readCommandDownstreamKeyer(vector<uint8_t> data)
{
	int i = data[0] * 4 + nofMEs * 2 + nofAuxs;
	bool onair = (data[1] & 0x01) > 0;
	bool intran = (data[2] & 0x01) > 0;
	bool inauto = (data[3] & 0x01) > 0;
	int remain = (int)data[5];
	chan_values[i] = (float)onair;
	chan_values[static_cast<__int64>(i)  +1] = (float)intran;
	chan_values[static_cast<__int64>(i) + 2] = (float)inauto;
	chan_values[static_cast<__int64>(i) + 3] = (float)remain;
}

void AtemCHOP::sendCommand(string command, vector<uint8_t> data)
{
	if (!isConnected())
		return;

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

void AtemCHOP::performCut(uint8_t me)
{
	vector<uint8_t> data{ me, 0, 0, 0 };
	sendCommand("DCut", data);
}

void AtemCHOP::performAuto(uint8_t me)
{
	vector<uint8_t> data{ me, 0, 0, 0 };
	sendCommand("DAut", data);
}

void AtemCHOP::changeProgramInput(uint8_t me, uint16_t source)
{
	cpgi[me] = source;

	vector<uint8_t> data{ me, 0 };
	data.push_back(source >> 0x08);
	data.push_back(source & 0xff);

	sendCommand("CPgI", data);
}

void AtemCHOP::changePreviewInput(uint8_t me, uint16_t source)
{
	cpvi[me] = source;

	vector<uint8_t> data{ me, 0 };
	data.push_back(source >> 0x08);
	data.push_back(source & 0xff);

	sendCommand("CPvI", data);
}

void AtemCHOP::changeAuxSource(uint8_t index, uint16_t source)
{
	caus[index] = source;

	vector<uint8_t> data{ 1, index };
	data.push_back(source >> 0x08);
	data.push_back(source & 0xff);

	sendCommand("CAuS", data);
}

void AtemCHOP::changeDownstreamKeyer(uint8_t keyer, bool onair)
{
	cdsl[keyer] = onair;

	vector<uint8_t> data{ keyer, 0, 0, 0 };
	if (onair) data[1] |= 0x01;

	sendCommand("CDsL", data);
}

void AtemCHOP::performDownstreamKeyerAuto(uint8_t keyer)
{
	vector<uint8_t> data{ keyer, 0, 0, 0 };
	sendCommand("DDsA", data);
}

uint16_t AtemCHOP::word(uint8_t n1, uint8_t n2)
{
	BYTE n[] = { n2, n1 };
	WORD* w = reinterpret_cast<WORD*>(&n);
	return uint16_t(*w);
}

extern "C"
{
	DLLEXPORT void FillCHOPPluginInfo(CHOP_PluginInfo* info)
	{
		info->apiVersion = CHOPCPlusPlusAPIVersion;
		info->customOPInfo.opType->setString("Atem");
		info->customOPInfo.opLabel->setString("Atem");
		info->customOPInfo.authorName->setString("Akira Kamikura");
		info->customOPInfo.authorEmail->setString("akira.kamikura@gmail.com");

		info->customOPInfo.opIcon->setString("ATM");

		info->customOPInfo.minInputs = 0;
		info->customOPInfo.maxInputs = 1;

	}

	DLLEXPORT CHOP_CPlusPlusBase* CreateCHOPInstance(const OP_NodeInfo* info)
	{
		return new AtemCHOP(info);
	}

	DLLEXPORT void DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
	{
		delete (AtemCHOP*)instance;
	}

};
