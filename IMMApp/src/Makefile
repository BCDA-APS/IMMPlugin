TOP=../..

include $(TOP)/configure/CONFIG

-include $(ADCORE)/ADApp/commonDriverMakefile

LIBRARY_IOC += IMMPlugin

IMMPlugin_SRCS += NDFileIMM.cpp
IMMPlugin_SRCS += NDPluginSparse.cpp

DBD += IMMSupport.dbd

IMMSupport_DBD += NDFileIMM.dbd
IMMSupport_DBD += NDPluginSparse.dbd

IMMPlugin_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
