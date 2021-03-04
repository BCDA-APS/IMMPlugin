#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <ctime>
#include <cstring>

#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsTime.h>
#include <iocsh.h>
#define epicsAssertAuthor "the EPICS areaDetector collaboration (https://github.com/areaDetector/ADCore/issues)"
#include <epicsAssert.h>

#include <asynDriver.h>

#include <epicsExport.h>
#include "NDFileIMM.h"


static const char *driverName = "NDFileIMM";

asynStatus NDFileIMM::openFile(const char *fileName, NDFileOpenMode_t openMode, NDArray *pArray)
{
	static const char *functionName = "openFile";

	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Filename: %s\n", driverName, functionName, fileName);

	// We don't support reading yet
	if (openMode & NDFileModeRead)
	{
		setIntegerParam(NDFileCapture, 0);
		setIntegerParam(NDWriteFile, 0);
		return asynError;
	}

	// We don't support opening an existing file for appending yet
	if (openMode & NDFileModeAppend)
	{
		setIntegerParam(NDFileCapture, 0);
		setIntegerParam(NDWriteFile, 0);
		return asynError;
	}

	// Check if an invalid (<0) number of frames has been configured for capture
	int numCapture;
	getIntegerParam(NDFileNumCapture, &numCapture);
	if (numCapture < 0)
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
				  "%s::%s Invalid number of frames to capture: %d. Please specify a number >= 0\n",
				  driverName, functionName, numCapture);
		return asynError;
	}
	
	int fileNumber;
	this->getIntegerParam(NDFileNumber, &fileNumber);

	// Check to see if a file is already open and close it
	if (this->file.is_open())    { this->closeFile(); }
	
	char altFile[MAX_FILENAME_LEN];
	
	epicsSnprintf(altFile, MAX_FILENAME_LEN, "%s_%05d-%05d.imm", fileName, fileNumber - 1, fileNumber + numCapture - 2);
	
	this->setStringParam(NDFullFileName, altFile);
	
	// Create the new file
	this->file.open(altFile, std::ofstream::binary);

	fileNumber += numCapture - 1;
	setIntegerParam(NDFileNumber, fileNumber);
	
	if (! this->file.is_open())
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
				  "%s::%s ERROR Failed to create a new output file\n",
				  driverName, functionName);
		return asynError;
	}

	this->frameNo = 0;

	return asynSuccess;
}

static void checkForValue(NDAttributeList* list, NDAttrDataType_t dataType, const char* name, void* output)
{
	NDAttribute* attr = list->find(name);
	if (attr)    { attr->getValue(dataType, output); }
}

/** Writes NDArray data to a raw file.
  * \param[in] pArray Pointer to an NDArray to write to the file. This function can be called multiple
  *            times between the call to openFile and closeFile if NDFileModeMultiple was set in
  *            openMode in the call to NDFileRaw::openFile.
  */
asynStatus NDFileIMM::writeFile(NDArray *pArray)
{
	asynStatus status = asynSuccess;
	static const char *functionName = "writeFile";

	if (! this->file.is_open())
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
				  "%s::%s file is not open!\n",
				  driverName, functionName);
		return asynError;
	}
	
	NDArrayInfo info;
	pArray->getInfo(&info);
	
	NDAttributeList* list = pArray->pAttributeList;
	
	epicsInt32 zero = 0;
	epicsInt32 one = 1;
	epicsInt32 two = 2;
	epicsInt32 twelve = 12;

	epicsInt32 compression = 0;
	checkForValue(list, NDAttrInt32, "compressed", &compression);

	char header[1024];
	
	memcpy(&header[0], &two, sizeof(epicsInt32)); // Mode (always 2)
	memcpy(&header[4], &compression, sizeof(epicsInt32)); // Compression (0 for raw, 6 for compressed)
	
	char buffer[32] = {0};
	std::time_t datetime = std::time(NULL);
	strncpy(buffer, std::asctime(std::localtime(&datetime)), 32);
	
	memcpy(&header[8], buffer, 32); // Date
	
	memcpy(&header[56], &this->frameNo, sizeof(epicsInt32)); // Frame Number

	memcpy(&header[76], &zero, sizeof(epicsInt32)); // Monitor (Unused)
	memcpy(&header[80], &zero, sizeof(epicsInt32));  // Shutter (Unused)
	
	epicsInt32 rows = info.ySize;
	epicsInt32 row_beg = 0;
	epicsInt32 row_end = info.ySize;
	epicsInt32 row_bin = 1;
	
	epicsInt32 cols = info.xSize;
	epicsInt32 col_beg = 0;
	epicsInt32 col_end = info.xSize;
	epicsInt32 col_bin = 1;

	checkForValue(list, NDAttrInt32, "rows", &rows);
	checkForValue(list, NDAttrInt32, "cols", &cols);
	checkForValue(list, NDAttrInt32, "row_beg", &row_beg);
	checkForValue(list, NDAttrInt32, "row_end", &row_end);
	checkForValue(list, NDAttrInt32, "row_bin", &row_bin);
	checkForValue(list, NDAttrInt32, "col_beg", &col_beg);
	checkForValue(list, NDAttrInt32, "col_end", &col_end);
	checkForValue(list, NDAttrInt32, "col_bin", &col_bin);

	memcpy(&header[84], &row_beg, sizeof(epicsInt32)); // Index of first row
	memcpy(&header[88], &row_end, sizeof(epicsInt32)); // Number of rows (not actual last row index)
	memcpy(&header[92], &col_beg, sizeof(epicsInt32)); // Index of first column
	memcpy(&header[96], &col_end, sizeof(epicsInt32)); // Number of columnes (not actual last column index)
	memcpy(&header[100], &row_bin, sizeof(epicsInt32)); // Number of row bins
	memcpy(&header[104], &col_bin, sizeof(epicsInt32)); // Number of column bins
	memcpy(&header[108], &rows, sizeof(epicsInt32));    // Number of rows
	memcpy(&header[112], &cols, sizeof(epicsInt32));    // Number of columns
	
	epicsInt32 pixel_size = info.bytesPerElement;
	checkForValue(list, NDAttrInt32, "pixel_size", &pixel_size);

	memcpy(&header[116], &pixel_size, sizeof(epicsInt32)); // Bytes per Pixel
	memcpy(&header[120], &zero, sizeof(epicsInt32)); // Kinetics (Unused)
	memcpy(&header[124], &zero, sizeof(epicsInt32)); // Kinetics Winsize (Unused)

	epicsFloat64 dzero = 0.0;

	memcpy(&header[128], &pArray->timeStamp, sizeof(epicsFloat64)); // Elapsed (Currently just timestamp)
	memcpy(&header[136], &dzero, sizeof(epicsFloat64)); // Preset (Unused)

	memcpy(&header[144], &zero, sizeof(epicsInt32)); // Topup (Unused)
	memcpy(&header[148], &zero, sizeof(epicsInt32)); // Inject (Unused)

	epicsInt32 dlen = info.nElements;
	checkForValue(list, NDAttrInt32, "dlen", &dlen);
	this->setIntegerParam(NDPluginLastLengthRead, dlen);
	this->callParamCallbacks();
	
	memcpy(&header[152],  &dlen, sizeof(epicsInt32)); // Data Length

	memcpy(&header[156], &one, sizeof(epicsInt32)); // ROI Number (Always 1)
	memcpy(&header[160], &this->frameNo, sizeof(epicsInt32)); // Buffer Number (same as Frame Number)

	memcpy(&header[164], &pArray->uniqueId, sizeof(epicsInt32)); // Sys Tick (Currently just unique ID)
	
	memcpy(&header[608], &zero, sizeof(epicsInt32)); // Imageserver (Unused)
	memcpy(&header[612], &zero, sizeof(epicsInt32)); // CPUspeed (Unused)

	memcpy(&header[616], &twelve, sizeof(epicsInt32)); // IMM Version (Always 12)
	memcpy(&header[620], &pArray->uniqueId, sizeof(epicsInt32)); // Timestamp (Actually unique ID, not frame timestamp)

	memcpy(&header[624], &zero, sizeof(epicsInt32)); // Camera Type (Unused)
	memcpy(&header[628], &zero, sizeof(epicsInt32)); // Threshold (Unused)

	this->file.write(header, 1024);
	
	if (compression == 0)
	{
		this->file.write((const char*) pArray->pData, info.bytesPerElement * dlen);
	}
	else
	{
		epicsUInt32* data = (epicsUInt32*) pArray->pData;

		for (int index = 0; index < (dlen * 2); index += 1)
		{
			epicsUInt32 val = data[index];
			
			// First half is 4-byte positions, then pixel_size values
			if (index < dlen)    { this->file.write((const char*) &val, sizeof(epicsUInt32)); }
			else                 { this->file.write((const char*) &val, pixel_size); }
		}
	}

	this->frameNo += 1;

	return asynSuccess;
}

/** Read NDArray data from an IMM file; NOTE: not implemented yet.
  * \param[in] pArray Pointer to the address of an NDArray to read the data into.  */
asynStatus NDFileIMM::readFile(NDArray **pArray)
{
  return asynError;
}

/** Closes the file opened with NDFileIMM::openFile
 */
asynStatus NDFileIMM::closeFile()
{
	static const char *functionName = "closeFile";

	if (!this->file.is_open())
	{
		asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
				  "%s::%s file was not open! Ignoring close command.\n",
				  driverName, functionName);
		return asynSuccess;
	}

	this->file.close();

	asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s file closed!\n", driverName, functionName);


	return asynSuccess;
}


/** Constructor for NDFileIMM; parameters are identical to those for NDPluginFile::NDPluginFile,
    and are passed directly to that base class constructor.
  * After calling the base class constructor this method sets NDPluginFile::supportsMultipleArrays=1.
  */
NDFileIMM::NDFileIMM(const char *portName, int queueSize, int blockingCallbacks,
                     const char *NDArrayPort, int NDArrayAddr,
                     int priority, int stackSize)
  /* Invoke the base class constructor.
   * We allocate 2 NDArrays of unlimited size in the NDArray pool.
   * This driver can block (because writing a file can be slow), and it is not multi-device.
   * Set autoconnect to 1.  priority and stacksize can be 0, which will use defaults. */
  : NDPluginFile(portName, queueSize, blockingCallbacks,
                 NDArrayPort, NDArrayAddr, 1, NUM_NDFILE_IMM_PARAMS,
                 2, 0, asynGenericPointerMask, asynGenericPointerMask,
                 ASYN_CANBLOCK, 1, priority, stackSize)
{
	setStringParam(NDPluginDriverPluginType, "NDFileIMM");
	
	createParam(NDPluginLastLengthReadString, asynParamInt32, &NDPluginLastLengthRead);

	this->supportsMultipleArrays = true;
}



/** Configuration routine. */
extern "C" int NDFileIMMConfigure(const char *portName, int queueSize, int blockingCallbacks,
                                  const char *NDArrayPort, int NDArrayAddr,
                                  int priority, int stackSize)
{
  NDFileIMM* temp = new NDFileIMM(portName, queueSize, blockingCallbacks, NDArrayPort, NDArrayAddr, priority, stackSize);

  return temp->start();
}


/** EPICS iocsh shell commands */
static const iocshArg initArg0 = { "portName",iocshArgString};
static const iocshArg initArg1 = { "frame queue size",iocshArgInt};
static const iocshArg initArg2 = { "blocking callbacks",iocshArgInt};
static const iocshArg initArg3 = { "NDArray Port",iocshArgString};
static const iocshArg initArg4 = { "NDArray Addr",iocshArgInt};
static const iocshArg initArg5 = { "priority",iocshArgInt};
static const iocshArg initArg6 = { "stack size",iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0,
                                            &initArg1,
                                            &initArg2,
                                            &initArg3,
                                            &initArg4,
                                            &initArg5,
                                            &initArg6};
static const iocshFuncDef initFuncDef = {"NDFileIMMConfigure",7,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
  NDFileIMMConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].sval,
                      args[4].ival, args[5].ival, args[6].ival);
}

extern "C" void NDFileIMMRegister(void)
{
  iocshRegister(&initFuncDef,initCallFunc);
}

extern "C" {
epicsExportRegistrar(NDFileIMMRegister);
}

