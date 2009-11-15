#ifndef   __EVENTLOG_H__
#define   __EVENTLOG_H__

#include <windows.h>

#define SVCNAME TEXT("LBTService")

VOID LbtReportFunctionError(LPTSTR szFunction);
VOID LbtReportDongleSwitch( LPTSTR szDongleName, BOOL toHID );


#endif //__EVENTLOG_H__
