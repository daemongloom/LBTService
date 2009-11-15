#include <windows.h>
#include <strsafe.h>
#include "resource.h"
#include "eventlog.h"
#include "LBTServiceMsg.h"

VOID LbtReportFunctionError(LPTSTR szFunction) 
{ 
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];

	hEventSource = RegisterEventSource(NULL, SVCNAME);

	if( NULL != hEventSource )
	{
		StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

		lpszStrings[0] = SVCNAME;
		lpszStrings[1] = Buffer;

		ReportEvent(hEventSource,// event log handle
			EVENTLOG_ERROR_TYPE, // event type
			0,                   // event category
			LBT_ERROR,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}

VOID LbtReportDongleSwitch( LPTSTR szDongleName, BOOL toHID )
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[3];

	hEventSource = RegisterEventSource(NULL, SVCNAME);

	if( NULL != hEventSource )
	{
		lpszStrings[0] = SVCNAME;
		lpszStrings[1] = szDongleName;
		lpszStrings[2] = toHID ? TEXT("HCI") : TEXT("HID");

		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, LBT_CONVERTED, NULL, 3, 0, lpszStrings, NULL);

		DeregisterEventSource(hEventSource);
	}
}
