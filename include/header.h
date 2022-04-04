#define WIN32_LEAN_AND_MEAN

#include "CHOP_CPlusPlusBase.h"
#include "CPlusPlus_Common.h"
#include "atem.h"

using namespace std;

class AtemCHOP : public CHOP_CPlusPlusBase {
 private:
  Atem* atem;

  void executeHandleParameters(const OP_Inputs* inputs);
  void executeHandleInputs(const OP_Inputs* inputs);

 public:
  AtemCHOP(const OP_NodeInfo* info);
  virtual ~AtemCHOP();

  void getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs,
                      void* reserved1);
  bool getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs,
                     void* reserved1);
  void getChannelName(int32_t index, OP_String* name, const OP_Inputs* inputs,
                      void* reserved1);
  void execute(CHOP_Output* output, const OP_Inputs* inputs, void* reserved);
  int32_t getNumInfoCHOPChans(void* reserved1);
  void getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1);
  bool getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1);
  void getInfoDATEntries(int32_t index, int32_t nEntries,
                         OP_InfoDATEntries* entries, void* reserved1);
  void setupParameters(OP_ParameterManager* manager, void* reserved1);
};
