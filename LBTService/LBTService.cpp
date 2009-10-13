#define WIN32_NO_STATUS
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <dbt.h>
extern "C"
{
#include <ntstatus.h>
#include <hidusage.h>
#include <hidsdi.h>
#include <hidpi.h>
}
#include <SetupAPI.h>
#include "lhid2hci.h"
#include "LBTServiceMsg.h"


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

#define SVCNAME TEXT("LBTService")

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;

VOID SvcInstall(void);
VOID SvcUninstall(void);
DWORD WINAPI SvcCtrlHandler( __in DWORD dwControl, __in DWORD dwEventType, __in  LPVOID lpEventData, __in  LPVOID lpContext );
VOID WINAPI SvcMain( DWORD, LPTSTR * ); 

VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * ); 
VOID SvcReportEvent( LPTSTR );


//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None
//
void __cdecl _tmain(int argc, TCHAR *argv[]) 
{ 
	// If command-line parameter is "install", install the service. 
	// Otherwise, the service is probably being started by the SCM.

	if( lstrcmpi( argv[1], TEXT("install")) == 0 )
	{
		SvcInstall();
		return;
	}

	if( lstrcmpi( argv[1], TEXT("uninstall")) == 0 )
	{
		SvcUninstall();
		return;
	}

	// TO_DO: Add any additional services for the process to this table.
	SERVICE_TABLE_ENTRY DispatchTable[] = 
	{ 
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
		{ NULL, NULL } 
	}; 

	// This call returns when the service has stopped. 
	// The process should simply terminate when the call returns.

	if (!StartServiceCtrlDispatcher( DispatchTable )) 
	{ 
		SvcReportEvent(TEXT("StartServiceCtrlDispatcher")); 
	}

	printf("Use \"install\" to install the service\n");
	printf("Use \"uninstall\" to uninstall the service\n");
} 

VOID SvcUninstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	schSCManager = OpenSCManager( 
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager) 
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	schService = OpenService( 
		schSCManager,       // SCM database 
		SVCNAME,          // name of service 
		DELETE);            // need delete access 

	BOOL bResult = DeleteService( schService );

	if (FALSE == schSCManager) 
	{
		printf("Unable to delete service (%d)\n", GetLastError());
		return;
	}
	else
		printf("Service uninstalled succesfully\n");

	CloseServiceHandle(schService); 
	CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	TCHAR szPath[MAX_PATH];

	if( !GetModuleFileName( NULL, szPath, MAX_PATH ) )
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the SCM database. 

	schSCManager = OpenSCManager( 
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager) 
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Create the service

	schService = CreateService( 
		schSCManager,              // SCM database 
		SVCNAME,                   // name of service 
		SVCNAME,                   // service name to display 
		SERVICE_ALL_ACCESS,        // desired access 
		SERVICE_WIN32_OWN_PROCESS, // service type 
		SERVICE_DEMAND_START,      // start type 
		SERVICE_ERROR_NORMAL,      // error control type 
		szPath,                    // path to service's binary 
		NULL,                      // no load ordering group 
		NULL,                      // no tag identifier 
		NULL,                      // no dependencies 
		NULL,                      // LocalSystem account 
		NULL);                     // no password 

	if (schService == NULL) 
	{
		printf("CreateService failed (%d)\n", GetLastError()); 
		CloseServiceHandle(schSCManager);
		return;
	}
	else printf("Service installed successfully\n"); 

	CloseServiceHandle(schService); 
	CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
	// Register the handler function for the service

	gSvcStatusHandle = RegisterServiceCtrlHandlerEx( 
		SVCNAME, 
		SvcCtrlHandler,
		NULL);

	if( !gSvcStatusHandle )
	{ 
		SvcReportEvent(TEXT("RegisterServiceCtrlHandler")); 
		return; 
	} 

	// These SERVICE_STATUS members remain as set here

	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
	gSvcStatus.dwServiceSpecificExitCode = 0;    

	// Report initial status to the SCM

	ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );

	// Perform service-specific initialization and work.

	SvcInit( dwArgc, lpszArgv );
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit( DWORD dwArgc, LPTSTR *lpszArgv)
{
	// TO_DO: Declare and set any required variables.
	//   Be sure to periodically call ReportSvcStatus() with 
	//   SERVICE_START_PENDING. If initialization fails, call
	//   ReportSvcStatus with SERVICE_STOPPED.

	// Create an event. The control handler function, SvcCtrlHandler,
	// signals this event when it receives the stop control code.

	ghSvcStopEvent = CreateEvent(
		NULL,    // default security attributes
		TRUE,    // manual reset event
		FALSE,   // not signaled
		NULL);   // no name

	if ( ghSvcStopEvent == NULL)
	{
		ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
		return;
	}

	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	DEV_BROADCAST_DEVICEINTERFACE dbvNotificationFilter;

	ZeroMemory( &dbvNotificationFilter, sizeof(dbvNotificationFilter) );
	dbvNotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	dbvNotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	dbvNotificationFilter.dbcc_classguid = hidGuid;

	HDEVNOTIFY hDeviceNotification = RegisterDeviceNotification( 
		gSvcStatusHandle, &dbvNotificationFilter, DEVICE_NOTIFY_SERVICE_HANDLE );

	if ( hDeviceNotification == NULL)
	{
		ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
		return;
	}

	// Report running status when initialization is complete.

	ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );

	// TO_DO: Perform work until service stops.


	while(1)
	{
		// Check whether to stop the service.
		WaitForSingleObject(ghSvcStopEvent, INFINITE);
		break;
	}

	UnregisterDeviceNotification(hDeviceNotification);
	ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus( DWORD dwCurrentState,
					 DWORD dwWin32ExitCode,
					 DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.

	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ( (dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED) )
		gSvcStatus.dwCheckPoint = 0;
	else gSvcStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
DWORD WINAPI SvcCtrlHandler(
							__in  DWORD dwControl,
							__in  DWORD dwEventType,
							__in  LPVOID lpEventData,
							__in  LPVOID lpContext )
{
	// Handle the requested control code. 

	switch(dwControl) 
	{  
	case SERVICE_CONTROL_STOP: 
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

		// Signal the service to stop.

		SetEvent(ghSvcStopEvent);

		return NO_ERROR;

	case SERVICE_CONTROL_INTERROGATE: 
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
		return NO_ERROR;

	case SERVICE_CONTROL_DEVICEEVENT:
		if ( dwEventType == DBT_DEVICEARRIVAL )
		{
			PDEV_BROADCAST_HDR pDevBroadcastHdr = (PDEV_BROADCAST_HDR)lpEventData;
			if ( pDevBroadcastHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
			{
				PDEV_BROADCAST_DEVICEINTERFACE pDevBroadcastDevInterface = (PDEV_BROADCAST_DEVICEINTERFACE) lpEventData;
				
				TCHAR buf[2000];
				
				lstrcpy(buf, TEXT("A fost adaugat un device cu numele: "));
				lstrcat(buf, pDevBroadcastDevInterface->dbcc_name);
				SvcReportEvent(buf);
				
				HANDLE hHidDevice = CreateFile(pDevBroadcastDevInterface->dbcc_name, 0, 
					FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0 );

				if ( hHidDevice == INVALID_HANDLE_VALUE )
					return NO_ERROR;

				HIDD_ATTRIBUTES HidAttributes;
				HidAttributes.Size = sizeof(HIDD_ATTRIBUTES);
				
				if ( !HidD_GetAttributes( hHidDevice, &HidAttributes) )
				{
					CloseHandle( hHidDevice );
					return NO_ERROR;
				}

				if ( IsBTHidDevice(HidAttributes.VendorID, HidAttributes.ProductID) )
					TryToSwitchHid2Hci( hHidDevice );

				CloseHandle( hHidDevice );
			}
		}
		return NO_ERROR;
	} 

	return ERROR_CALL_NOT_IMPLEMENTED;
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction) 
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

		ReportEvent(hEventSource,        // event log handle
			EVENTLOG_ERROR_TYPE, // event type
			0,                   // event category
			SVC_ERROR,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}
