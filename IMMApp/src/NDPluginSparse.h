#ifndef NDPluginSparse_H
#define NDPluginSparse_H

#include <epicsTypes.h>

#include "NDPluginDriver.h"

#define NDPluginSparseArraySizeString     "SPARSE_ARRAY_SIZE"
#define NDPluginSparseThresholdString     "THRESHOLD"
#define NDPluginSparseLastSizeString      "LAST_WRITTEN_SIZE"

class epicsShareClass NDPluginSparse : public NDPluginDriver {
	public:
		NDPluginSparse(const char *portName, int queueSize, int blockingCallbacks, 
		               const char *NDArrayPort, int NDArrayAddr,
		               int maxBuffers, size_t maxMemory,
		               int priority, int stackSize, int maxThreads=1);
	
		/* These methods override the virtual methods in the base class */
		void processCallbacks(NDArray *pArray);
		
	protected:
		int NDPluginSparseArraySize;
		int NDPluginSparseThreshold;
		int NDPluginSparseLastSize;
		
	private:
		template <typename epicsType> std::vector< std::pair <size_t, epicsUInt32> > doCompress(NDArray* input);
};

#endif
