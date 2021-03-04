#include <fstream>
#include <asynDriver.h>
#include <NDPluginFile.h>
#include <NDArray.h>

#define NDPluginLastLengthReadString    "NUM_COMP_PIXELS"

class epicsShareClass NDFileIMM : public NDPluginFile
{
  public:
    NDFileIMM(const char *portName, int queueSize, int blockingCallbacks,
               const char *NDArrayPort, int NDArrayAddr,
               int priority, int stackSize);

    /* The methods that this class implements */
    virtual asynStatus openFile(const char *fileName, NDFileOpenMode_t openMode, NDArray *pArray);
    virtual asynStatus readFile(NDArray **pArray);
    virtual asynStatus writeFile(NDArray *pArray);
    virtual asynStatus closeFile();

  protected:
    /* plugin parameters */
	int NDPluginLastLengthRead;

  private:
	std::ofstream file;
	int frameNo;
};
#define NUM_NDFILE_IMM_PARAMS 0
