#ifndef   __LBTSERVICE_H__
#define   __LBTSERVICE_H__

#include <windows.h>

VOID SvcInstall(void);
VOID SvcUninstall(void);
DWORD WINAPI SvcCtrlHandler( __in DWORD dwControl, __in DWORD dwEventType, __in  LPVOID lpEventData, __in  LPVOID lpContext );
VOID WINAPI SvcMain( DWORD, LPTSTR * ); 

VOID ReportSvcStatus( DWORD, DWORD, DWORD );
VOID SvcInit( DWORD, LPTSTR * ); 
BOOLEAN TryToSwitchLogitech( LPTSTR devPath );
BOOLEAN TryToSwitchAllDevices();
BOOL LBTStopService( SC_HANDLE schSCManager, SC_HANDLE schService );


#endif //__LBTSERVICE_H__