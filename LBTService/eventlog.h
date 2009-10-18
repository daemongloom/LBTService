#ifndef   __EVENTLOG_H__
#define   __EVENTLOG_H__

#include <windows.h>

#define SVCNAME TEXT("LBTService")

VOID LbtReportFunctionError(LPTSTR szFunction);
VOID LbtReportDongleSwitch( LPTSTR szDongleName );


#endif //__EVENTLOG_H__
