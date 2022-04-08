#include <header.h>

AtemCHOP::AtemCHOP(const OP_NodeInfo* info) {
  atem = new Atem();
  atem->init();
}

AtemCHOP::~AtemCHOP() { atem->stop(); }

void AtemCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs,
                              void* reserved1) {
  ginfo->cookEveryFrameIfAsked = true;
  ginfo->timeslice = false;
  ginfo->inputMatchIndex = 0;
}

bool AtemCHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs,
                             void* reserved1) {
  info->numSamples = 1;
  if (atem->isConnected()) {
    info->numChannels = int32_t(atem->chan_names().size());
  } else {
    info->numChannels = 0;
  }
  return true;
}

void AtemCHOP::getChannelName(int32_t index, OP_String* name,
                              const OP_Inputs* inputs, void* reserved1) {
  name->setString(atem->chan_names()[index].c_str());
}

void AtemCHOP::execute(CHOP_Output* output, const OP_Inputs* inputs,
                       void* reserved) {
  executeHandleParameters(inputs);
  executeHandleInputs(inputs);

  time_t now_t = time(NULL);

  // auto close
  if ((int)now_t - checkCloseTerm > (int)atem->last_recv_packet_t() &&
      atem->isConnected()) {
    atem->init();
  }

  // auto reconnect
  if ((int)now_t - checkReconnTerm > (int)atem->last_send_packet_t() &&
      !atem->isConnected()) {
    atem->sendPacketStart();
  }

  // provide data
  if (atem->isConnected()) {
    for (int i = 0; i < atem->chan_values().size(); i++) {
      for (int j = 0; j < output->numSamples; j++) {
        output->channels[i][j] = atem->chan_values()[i];
      }
    }
  }
}

int32_t AtemCHOP::getNumInfoCHOPChans(void* reserved1) {
  int chans = 3 + (int)atem->topology_values().size();
  return chans;
}

void AtemCHOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan,
                               void* reserved1) {
  if (index == 0) {
    chan->name->setString("atemSessionID");
    chan->value = (float)atem->session_id();
  }
  if (index == 1) {
    chan->name->setString("atemRemotePacketID");
    chan->value = (float)atem->remote_packet_id();
  }
  if (index == 2) {
    chan->name->setString("atemLocalPacketID");
    chan->value = (float)atem->local_packet_id();
  }
  if (index >= 3) {
    chan->name->setString(
        atem->topology_names()[int(static_cast<__int64>(index) - 3)].c_str());
    chan->value =
        (float)atem->topology_values()[int(static_cast<__int64>(index) - 3)];
  }
}

bool AtemCHOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1) {
  infoSize->rows = 2 + atem->n_of_inputs() + atem->n_of_macros();
  infoSize->cols = 3;
  infoSize->byColumn = false;
  return true;
}

void AtemCHOP::getInfoDATEntries(int32_t index, int32_t nEntries,
                                 OP_InfoDATEntries* entries, void* reserved1) {
  // Product Id
  if (index == 0) {
    entries->values[0]->setString("Product Id");
    entries->values[1]->setString(atem->atem_product_id().c_str());
    entries->values[2]->setString("");
  }

  // Warning
  else if (index == 1) {
    entries->values[0]->setString("Warning");
    entries->values[1]->setString(atem->atem_warning().c_str());
    entries->values[2]->setString("");
  }

  // Input Property
  else if (index < 2 + atem->n_of_inputs()) {
    int i = index - 2;
    char key[10];
    sprintf_s(key, "Input%d", int(i + 1));

    entries->values[0]->setString(key);
    entries->values[1]->setString(atem->input_names()[i].c_str());
    entries->values[2]->setString(atem->input_labels()[i].c_str());
  }

  // Macro Property
  else if (index < 2 + atem->n_of_inputs() + atem->n_of_macros()) {
    int i = index - 2 - atem->n_of_inputs();
    char key[10];
    sprintf_s(key, "Macro%d", int(i + 1));
    std::string s = atem->macro_run_status()[i] > 0 ? "running" : "";

    entries->values[0]->setString(key);
    entries->values[1]->setString(atem->macro_names()[i].c_str());
    entries->values[2]->setString(s.c_str());
  }
}

void AtemCHOP::setupParameters(OP_ParameterManager* manager, void* reserved1) {
  {
    OP_StringParameter sp;
    sp.name = "Atemip";
    sp.label = "Atem IP";
    OP_ParAppendResult res = manager->appendString(sp);
    assert(res == OP_ParAppendResult::Success);
  }
}

void AtemCHOP::executeHandleParameters(const OP_Inputs* inputs) {
  std::string ip = inputs->getParString("Atemip");
  if (atem->atem_ip() != ip || atem->active() == -1) {
    atem->atem_ip(ip);
    if (atem->active()) atem->stop();
    atem->start();
  }
}

void AtemCHOP::executeHandleInputs(const OP_Inputs* inputs) {
  for (int i = 0; i < inputs->getNumInputs(); i++) {
    const OP_CHOPInput* cinput = inputs->getInputCHOP(i);
    for (int j = 0; j < cinput->numChannels; j++) {
      std::string cname = cinput->getChannelName(j);

      if (!strncmp(cname.c_str(), "dcut", 4)) {
        uint8_t me = stoi(cname.substr(4, 1)) - 1;
        bool flag = cinput->getChannelData(j)[0] >= 1;
        if (atem->n_of_mes() > me && atem->dcut(me) != flag) {
          atem->dcut(me, flag);
          if (flag) atem->performCut(me);
        }
      }

      if (!strncmp(cname.c_str(), "daut", 4)) {
        uint8_t me = stoi(cname.substr(4, 1)) - 1;
        bool flag = cinput->getChannelData(j)[0] >= 1;
        if (atem->n_of_mes() > me && atem->daut(me) != flag) {
          atem->daut(me, flag);
          if (flag) atem->performAuto(me);
        }
      }

      if (!strncmp(cname.c_str(), "cpgi", 4)) {
        uint8_t me = stoi(cname.substr(4, 1)) - 1;
        uint16_t source = uint16_t(cinput->getChannelData(j)[0]);
        if (atem->n_of_mes() > me && atem->cpgi(me) != source) {
          atem->changeProgramInput(me, source);
        }
      }

      if (!strncmp(cname.c_str(), "cpvi", 4)) {
        uint8_t me = stoi(cname.substr(4, 1)) - 1;
        uint16_t source = uint16_t(cinput->getChannelData(j)[0]);
        if (atem->n_of_mes() > me && atem->cpvi(me) != source) {
          atem->changePreviewInput(me, source);
        }
      }

      if (!strncmp(cname.c_str(), "caus", 4)) {
        uint8_t index = stoi(cname.substr(4, 1)) - 1;
        uint16_t source = uint16_t(cinput->getChannelData(j)[0]);
        if (atem->n_of_auxs() > index && atem->caus(index) != source) {
          atem->changeAuxSource(index, source);
        }
      }

      if (!strncmp(cname.c_str(), "cdsl", 4)) {
        uint8_t keyer = stoi(cname.substr(4, 1)) - 1;
        bool onair = cinput->getChannelData(j)[0] >= 1;
        if (atem->n_of_dsks() > keyer && atem->cdsl(keyer) != onair) {
          atem->changeDownstreamKeyer(keyer, onair);
        }
      }

      if (!strncmp(cname.c_str(), "ddsa", 4)) {
        uint8_t keyer = stoi(cname.substr(4, 1)) - 1;
        bool flag = cinput->getChannelData(j)[0] >= 1;
        if (atem->n_of_dsks() > keyer && atem->ddsa(keyer) != flag) {
          atem->ddsa(keyer, flag);
          if (flag) atem->performDownstreamKeyerAuto(keyer);
        }
      }
    }
  }
}

extern "C" {
DLLEXPORT void FillCHOPPluginInfo(CHOP_PluginInfo* info) {
  info->apiVersion = CHOPCPlusPlusAPIVersion;
  info->customOPInfo.opType->setString("Atem");
  info->customOPInfo.opLabel->setString("Atem");
  info->customOPInfo.authorName->setString("Akira Kamikura");
  info->customOPInfo.authorEmail->setString("akira.kamikura@gmail.com");

  info->customOPInfo.opIcon->setString("ATM");

  info->customOPInfo.minInputs = 0;
  info->customOPInfo.maxInputs = 1;
}

DLLEXPORT CHOP_CPlusPlusBase* CreateCHOPInstance(const OP_NodeInfo* info) {
  return new AtemCHOP(info);
}

DLLEXPORT void DestroyCHOPInstance(CHOP_CPlusPlusBase* instance) {
  delete (AtemCHOP*)instance;
}
};
