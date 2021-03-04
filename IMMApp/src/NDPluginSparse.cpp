#include <vector>
#include <utility>

#include "NDPluginSparse.h"
#include "epicsExport.h"
#include "iocsh.h"

#include <stdio.h>

static const char *driverName="NDPluginSparse";

void NDPluginSparse::processCallbacks(NDArray *pArray)
{
	static const char* functionName = "processCallbacks";

	NDPluginDriver::beginProcessCallbacks(pArray);

	std::vector< std::pair <size_t, epicsUInt32> > values;
	
	switch(pArray->dataType)
	{
		case NDInt8:
			values = doCompress<epicsInt8>(pArray);
			break;
		case NDUInt8:
			values = doCompress<epicsUInt8>(pArray);
			break;
		case NDInt16:
			values = doCompress<epicsInt16>(pArray);
			break;
		case NDUInt16:
			values = doCompress<epicsUInt16>(pArray);
			break;
		case NDInt32:
			values = doCompress<epicsInt32>(pArray);
			break;
		case NDUInt32:
			values = doCompress<epicsUInt32>(pArray);
			break;
		case NDFloat32:
			values = doCompress<epicsFloat32>(pArray);
			break;
		case NDFloat64:
			values = doCompress<epicsFloat64>(pArray);
			break;
		default:
			break;
	}
	
	int max_length;
	this->getIntegerParam(NDPluginSparseArraySize, &max_length);
	
	size_t dlen = values.size();
	size_t dims[1] = { max_length };
	
	if (dlen * 2 > max_length)
	{
		printf("Pixel hits require %d elements, limiting to %d\n", dlen * 2, max_length);
		dlen = (int) max_length / 2;
	}
	
	NDArray* output = this->pNDArrayPool->alloc(1, dims, NDUInt32, 0, NULL);
	output->uniqueId = pArray->uniqueId;
	output->timeStamp = pArray->timeStamp;
	
	epicsUInt32* out_data = (epicsUInt32 *) output->pData;
	
	for (int index = 0; index < dlen; index += 1)
	{
		out_data[index] = values[index].first;
		out_data[index + dlen] = (epicsUInt32) values[index].second;
	}
	
	NDArrayInfo_t info;
	pArray->getInfo(&info);

	int zero = 0;
	int six = 6;

	output->pAttributeList->add("dlen", "Data Length", NDAttrInt32, &dlen);
	output->pAttributeList->add("rows", "Height", NDAttrInt32, &pArray->dims[1].size);
	output->pAttributeList->add("cols", "Width", NDAttrInt32, &pArray->dims[0].size);
	output->pAttributeList->add("row_beg", "Row Begin", NDAttrInt32, &zero);
	output->pAttributeList->add("col_beg", "Col Begin", NDAttrInt32, &zero);
	output->pAttributeList->add("row_end", "Row End", NDAttrInt32, &pArray->dims[1].size);
	output->pAttributeList->add("col_end", "Col End", NDAttrInt32, &pArray->dims[0].size);
	output->pAttributeList->add("pixel_size", "Pixel Bytes", NDAttrInt32, &info.bytesPerElement);
	output->pAttributeList->add("compressed", "Compression Used", NDAttrInt32, &six);

	NDPluginDriver::endProcessCallbacks(output, false, true);
	callParamCallbacks();
}

template <typename epicsType>
std::vector< std::pair <size_t, epicsUInt32> > NDPluginSparse::doCompress(NDArray* input)
{
	epicsType* in_data = (epicsType *) input->pData;

	int threshold;
	this->getIntegerParam(NDPluginSparseThreshold, &threshold);

	std::vector< std::pair <size_t, epicsUInt32> > values;

	size_t width = input->dims[0].size;
	size_t height = input->dims[1].size;

	for (size_t index = 0; index < width * height; index += 1)
	{
		epicsType val = in_data[index];

		if (val > threshold)
		{
			values.push_back(std::pair<size_t, epicsUInt32>(index, (epicsUInt32) val));
		}
	}

	return values;
}

NDPluginSparse::NDPluginSparse(const char *portName, int queueSize, int blockingCallbacks,
		               const char *NDArrayPort, int NDArrayAddr,
		               int maxBuffers, size_t maxMemory,
		               int priority, int stackSize, int maxThreads)
  /* Invoke the base class constructor */
  : NDPluginDriver(portName, queueSize, blockingCallbacks,
           NDArrayPort, NDArrayAddr, 1, maxBuffers, maxMemory,
           asynGenericPointerMask,
           asynGenericPointerMask,
           ASYN_MULTIDEVICE, 1, priority, stackSize, maxThreads)
{
	createParam(NDPluginSparseThresholdString, asynParamInt32, &NDPluginSparseThreshold);
	createParam(NDPluginSparseArraySizeString, asynParamInt32, &NDPluginSparseArraySize);
}

/** Configuration command */
extern "C" int NDSparseConfigure(const char *portName, int queueSize, int blockingCallbacks,
                                 const char *NDArrayPort, int NDArrayAddr,
                                 int maxBuffers, size_t maxMemory,
                                 int priority, int stackSize, int maxThreads)
{
    NDPluginSparse *pPlugin = new NDPluginSparse(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr,
                                              maxBuffers, maxMemory, priority, stackSize, maxThreads);
    return pPlugin->start();
}


/* EPICS iocsh shell commands */
static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "frame queue size",iocshArgInt};
static const iocshArg initArg2 = { "blocking callbacks",iocshArgInt};
static const iocshArg initArg3 = { "NDArrayPort",iocshArgString};
static const iocshArg initArg4 = { "NDArrayAddr",iocshArgInt};
static const iocshArg initArg5 = { "maxBuffers",iocshArgInt};
static const iocshArg initArg6 = { "maxMemory",iocshArgInt};
static const iocshArg initArg7 = { "priority",iocshArgInt};
static const iocshArg initArg8 = { "stackSize",iocshArgInt};
static const iocshArg initArg9 = { "maxThreads",iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2,
                                            &initArg3,
                                            &initArg4,
                                            &initArg5,
                                            &initArg6,
                                            &initArg7,
                                            &initArg8,
                                            &initArg9};
static const iocshFuncDef initFuncDef = {"NDSparseConfigure",10,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    NDSparseConfigure(args[0].sval, args[1].ival, args[2].ival,
                     args[3].sval, args[4].ival, args[5].ival,
                     args[6].ival, args[7].ival, args[8].ival,
                     args[9].ival);
}

extern "C" void NDSparseRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

extern "C"
{
	epicsExportRegistrar(NDSparseRegister);
}
