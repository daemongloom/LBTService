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
#include "LBTService.h"
#include "eventlog.h"
#include "resource.h"


#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;

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
		LbtReportFunctionError(TEXT("StartServiceCtrlDispatcher")); 
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
		DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS );            // need delete access 

	BOOL bResult = LBTStopService(schSCManager, schService);
	if ( !bResult ) 
	{
		printf("Unable to delete service (%d)\n", GetLastError());
		return;
	}

	bResult = DeleteService( schService );
	if ( !bResult )  
	{
		printf("Unable to delete service (%d)\n", GetLastError());
		return;
	}

	CloseServiceHandle(schService); 
	CloseServiceHandle(schSCManager);

	HKEY hApplicationEventLogKey;

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\services\\Eventlog\\Application"), 0, KEY_CREATE_SUB_KEY, &hApplicationEventLogKey ) )
		return;

	if ( ERROR_SUCCESS != RegDeleteKey(hApplicationEventLogKey, SVCNAME ) )
		return;

	printf("Service uninstalled succesfully\n");

}

BOOL LBTStopService( SC_HANDLE schSCManager, SC_HANDLE schService )
{
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwStartTime = GetTickCount();
	DWORD dwBytesNeeded;
	DWORD dwTimeout = 30000; // 30-second time-out
	DWORD dwWaitTime;

	if ( !QueryServiceStatusEx( 
		schService, 
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ssp, 
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded ) )
	{
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError()); 
		return FALSE;
	}

	if ( ssp.dwCurrentState == SERVICE_STOPPED )
		return TRUE;

	while ( ssp.dwCurrentState == SERVICE_STOP_PENDING ) 
	{
		// Do not wait longer than the wait hint. A good interval is 
		// one-tenth of the wait hint but not less than 1 second  
		// and not more than 10 seconds. 

		dwWaitTime = ssp.dwWaitHint / 10;

		if( dwWaitTime < 1000 )
			dwWaitTime = 1000;
		else if ( dwWaitTime > 10000 )
			dwWaitTime = 10000;

		Sleep( dwWaitTime );

		if ( !QueryServiceStatusEx( 
			schService, 
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp, 
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded ) )
		{
			printf("QueryServiceStatusEx failed (%d)\n", GetLastError()); 
			return FALSE;
		}

		if ( ssp.dwCurrentState == SERVICE_STOPPED )
		{
			return TRUE;
		}

		if ( GetTickCount() - dwStartTime > dwTimeout )
		{
			printf("Service stop timed out.\n");
			return FALSE;
		}
	}

    if ( !ControlService( 
            schService, 
            SERVICE_CONTROL_STOP, 
            (LPSERVICE_STATUS) &ssp ) )
    {
        printf( "ControlService failed (%d)\n", GetLastError() );
		return FALSE;
    }

	while ( ssp.dwCurrentState != SERVICE_STOPPED ) 
	{
		Sleep( ssp.dwWaitHint );
		if ( !QueryServiceStatusEx( 
			schService, 
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp, 
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded ) )
		{
			printf( "QueryServiceStatusEx failed (%d)\n", GetLastError() );
			return FALSE;
		}

		if ( ssp.dwCurrentState == SERVICE_STOPPED )
			break;

		if ( GetTickCount() - dwStartTime > dwTimeout )
		{
			printf( "Wait timed out\n" );
			return FALSE;
		}
	}

	return TRUE;
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
		SERVICE_AUTO_START,        // start type 
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

	if ( !StartService(schService, 0, NULL) )
	{
		printf("Unable to start service (%d)\n", GetLastError()); 
		CloseServiceHandle(schService); 
		CloseServiceHandle(schSCManager);
	}

	CloseServiceHandle(schService); 
	CloseServiceHandle(schSCManager);

	HKEY hServiceKey;

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\services\\")SVCNAME, 0, KEY_SET_VALUE, &hServiceKey ) )
		return;

	TCHAR lpDescription[1000];
	if ( !LoadString( GetModuleHandle(NULL), SVC_DESCRIPTION, lpDescription, sizeof(lpDescription) / sizeof(TCHAR) ) )
	{
		RegCloseKey( hServiceKey );
		return;
	}

	size_t dwDescriptionSize;
	if ( FAILED( StringCbLength(lpDescription, sizeof(lpDescription) / sizeof(TCHAR), &dwDescriptionSize) ) )
	{
		RegCloseKey( hServiceKey );
		return;
	}

	if ( ERROR_SUCCESS != RegSetValueEx(hServiceKey, TEXT("Description"), NULL, REG_SZ, (BYTE *)lpDescription, dwDescriptionSize + sizeof(TCHAR) ) )
	{
		RegCloseKey( hServiceKey );
		printf("Unable to set the service description"); 
		return;
	}
	
	RegCloseKey( hServiceKey );

	HKEY hApplicationEventLogKey, hServiceEventLogKey;

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\services\\Eventlog\\Application"), 0, KEY_CREATE_SUB_KEY, &hApplicationEventLogKey ) )
		return;

	if ( ERROR_SUCCESS != RegCreateKeyEx( hApplicationEventLogKey, SVCNAME, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hServiceEventLogKey, NULL) )
	{
		RegCloseKey( hApplicationEventLogKey );
		printf("Unable to create the service event log key"); 
		return;
	}

	size_t dwPathSize;
	if ( FAILED( StringCbLength(szPath, sizeof(szPath) / sizeof(TCHAR), &dwPathSize) ) )
	{
		RegCloseKey( hServiceKey );
		return;
	}

	if ( ERROR_SUCCESS != RegSetValueEx(hServiceEventLogKey, TEXT("EventMessageFile"), NULL, REG_SZ, (BYTE *)szPath, dwPathSize + sizeof(TCHAR) ) )
	{
		RegCloseKey( hServiceEventLogKey );
		RegCloseKey( hApplicationEventLogKey );
		printf("Unable to write EventMessageFile registry value"); 
		return;
	}

	DWORD dwTypesSupported = EVENTLOG_ERROR_TYPE | EVENTLOG_INFORMATION_TYPE | EVENTLOG_WARNING_TYPE ;
	if ( ERROR_SUCCESS != RegSetValueEx(hServiceEventLogKey, TEXT("TypesSupported"), NULL, REG_DWORD, (BYTE *)&dwTypesSupported, sizeof( DWORD ) ) )
	{
		RegCloseKey( hServiceEventLogKey );
		RegCloseKey( hApplicationEventLogKey );
		printf("Unable to write TypeSupported registry value"); 
		return;
	}

	RegCloseKey( hServiceEventLogKey );
	RegCloseKey( hApplicationEventLogKey );



	printf("Service installed successfully\n"); 

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
		LbtReportFunctionError(TEXT("RegisterServiceCtrlHandler")); 
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

	if ( !LoadSupportedBluetoothDevices() )
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

	TryToSwitchAllDevices( TRUE );

	// Report running status when initialization is complete.
	ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );

	while(1)
	{
		// Check whether to stop the service.
		WaitForSingleObject(ghSvcStopEvent, INFINITE);
		break;
	}

	UnregisterDeviceNotification(hDeviceNotification);
	ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
}

BOOLEAN TryToSwitchAllDevices( BOOL toHID )
{
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);

	HDEVINFO hDevInfo = SetupDiGetClassDevs( &hidGuid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE );

	if ( hDevInfo == INVALID_HANDLE_VALUE )
	{
		LbtReportFunctionError(TEXT("SetupDiGetClassDevs"));
		ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
		return FALSE;
	}

	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (DWORD dMemberIndex = 0; SetupDiEnumDeviceInterfaces( hDevInfo, NULL, &hidGuid, dMemberIndex, &DeviceInterfaceData); dMemberIndex++ )
	{
		DWORD RequiredSize;

		if ( SetupDiGetDeviceInterfaceDetail( hDevInfo, &DeviceInterfaceData, NULL, 0, &RequiredSize, 0) || GetLastError() != ERROR_INSUFFICIENT_BUFFER )
		{
			LbtReportFunctionError(TEXT("SetupDiGetDeviceInterfaceDetail"));
			ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
			return FALSE;
		}

		PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc( GetProcessHeap(), 0, RequiredSize);

		if ( pDeviceInterfaceDetailData == NULL )
		{
			LbtReportFunctionError(TEXT("HeapAlloc"));
			ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
			return FALSE;
		}

		pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if ( !SetupDiGetDeviceInterfaceDetail( hDevInfo, &DeviceInterfaceData, pDeviceInterfaceDetailData, RequiredSize, NULL, 0) )
		{
			LbtReportFunctionError(TEXT("SetupDiGetDeviceInterfaceDetail"));
			HeapFree(GetProcessHeap(), 0, pDeviceInterfaceDetailData);
			ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
			return FALSE;
		}

		TryToSwitchLogitech( pDeviceInterfaceDetailData->DevicePath, toHID );

		HeapFree(GetProcessHeap(), 0, pDeviceInterfaceDetailData);
	}

	return TRUE;
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

	gSvcStatus.dwControlsAccepted |= SERVICE_ACCEPT_POWEREVENT;

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
	switch(dwControl) 
	{
	case SERVICE_CONTROL_STOP:
		TryToSwitchAllDevices( FALSE );
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
		SetEvent(ghSvcStopEvent);
		return NO_ERROR;

	case SERVICE_CONTROL_INTERROGATE: 
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
		return NO_ERROR;

	case SERVICE_CONTROL_POWEREVENT:
		if ( dwEventType == PBT_APMRESUMEAUTOMATIC )
			TryToSwitchAllDevices( TRUE );
		return NO_ERROR;

	case SERVICE_CONTROL_DEVICEEVENT:
		if ( dwEventType == DBT_DEVICEARRIVAL )
		{
			PDEV_BROADCAST_HDR pDevBroadcastHdr = (PDEV_BROADCAST_HDR)lpEventData;
			if ( pDevBroadcastHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
			{
				PDEV_BROADCAST_DEVICEINTERFACE pDevBroadcastDevInterface = (PDEV_BROADCAST_DEVICEINTERFACE) lpEventData;
				TryToSwitchLogitech(pDevBroadcastDevInterface->dbcc_name, TRUE);
			}
		}
		return NO_ERROR;
	} 

	return ERROR_CALL_NOT_IMPLEMENTED;
}

BOOLEAN TryToSwitchLogitech( LPTSTR devPath, BOOL toHID )
{
	HANDLE hHidDevice = CreateFile(devPath, 0, 
		FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0 );

	if ( hHidDevice == INVALID_HANDLE_VALUE )
		return FALSE;

	HIDD_ATTRIBUTES HidAttributes;
	HidAttributes.Size = sizeof(HIDD_ATTRIBUTES);

	if ( !HidD_GetAttributes( hHidDevice, &HidAttributes) )
	{
		CloseHandle( hHidDevice );
		return FALSE;
	}

	if ( LPTSTR lptstrDongleName = IsBTHidDevice(HidAttributes.VendorID, HidAttributes.ProductID) )
		SwitchLogitech( lptstrDongleName, hHidDevice, toHID );

	CloseHandle( hHidDevice );
	return TRUE;
}
