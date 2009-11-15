#define WIN32_NO_STATUS
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
extern "C"
{
#include <ntstatus.h>
#include <hidusage.h>
#include <hidsdi.h>
#include <hidpi.h>
}
#include "lhid2hci.h"
#include "eventlog.h"

#define CONFIG_KEY TEXT("SOFTWARE\\CSa\\LBTService")
#define BT_DEVICE_INFO_ALLOC_STEP 10

CHAR ToHCIReports[3][7] =
{
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x80, (CHAR)0x80, (CHAR)0x01, (CHAR)0x00, (CHAR)0x00 },
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x80, (CHAR)0x00, (CHAR)0x00, (CHAR)0x30, (CHAR)0x00 },
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x81, (CHAR)0x80, (CHAR)0x00, (CHAR)0x00, (CHAR)0x00 }
};

CHAR ToHIDReports[2][7] =
{
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x81, (CHAR)0x80, (CHAR)0x00, (CHAR)0x00, (CHAR)0x00 },
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x80, (CHAR)0x80, (CHAR)0x03, (CHAR)0x00, (CHAR)0x00 },
};

PBT_DEVICE_INFO btDeviceInfo = NULL;
DWORD cbtDeviceInfoSize = 0;

BOOLEAN LoadSupportedBluetoothDevices()
{
	HKEY hConfigKey;

	if ( btDeviceInfo != NULL )
		return FALSE;

	if ( ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONFIG_KEY, 0, KEY_READ, &hConfigKey ) )
	{
		LbtReportFunctionError( TEXT("Unable to open key \"") CONFIG_KEY TEXT("\"; probably it doesn't exist RegOpenKeyEx") );
		return FALSE;
	}

	DWORD dwValuesCount;
	if ( ERROR_SUCCESS != RegQueryInfoKey(hConfigKey, NULL, NULL, NULL, NULL, NULL, NULL, &dwValuesCount, NULL, NULL, NULL, NULL) )
		return FALSE;
	
	cbtDeviceInfoSize = dwValuesCount;
	btDeviceInfo = (PBT_DEVICE_INFO) HeapAlloc(GetProcessHeap(), 0, dwValuesCount * sizeof(BT_DEVICE_INFO) );
	if ( btDeviceInfo == NULL )
		return FALSE;
	
	for ( DWORD i = 0; i < dwValuesCount; i++ )
	{
		DWORD dwKeyNameSize = sizeof(btDeviceInfo->lptstrBluetoothDeviceName) / sizeof( TCHAR );
		DWORD dwType;
		TCHAR lptstrValue[100];
		DWORD dwValueSize = sizeof(lptstrValue) / sizeof(TCHAR);

		if ( ERROR_SUCCESS != RegEnumValue(hConfigKey, i, btDeviceInfo[i].lptstrBluetoothDeviceName, &dwKeyNameSize, NULL, &dwType, (LPBYTE)lptstrValue, &dwValueSize) &&
			dwValueSize < sizeof(lptstrValue) / sizeof (TCHAR) )
		{

			HeapFree(GetProcessHeap(), 0, btDeviceInfo);
			btDeviceInfo = NULL;
			return FALSE;
		}
		
		TCHAR *next_token = NULL;
		TCHAR *szVendorId = _tcstok_s(lptstrValue, TEXT(" "), &next_token);
		if ( szVendorId == NULL )
		{
			HeapFree(GetProcessHeap(), 0, btDeviceInfo);
			btDeviceInfo = NULL;
			return FALSE;
		}

		btDeviceInfo[i].usVendorId = (USHORT)wcstoul( szVendorId, 0, 16 );

		TCHAR *szProductId = _tcstok_s(NULL, TEXT(" "), &next_token);
		if ( szProductId == NULL )
		{
			HeapFree(GetProcessHeap(), 0, btDeviceInfo);
			btDeviceInfo = NULL;
			return FALSE;
		}

		btDeviceInfo[i].pProductId = (USHORT)wcstoul( szProductId, 0, 16 );
	}

	RegCloseKey( hConfigKey );
	return TRUE;
}

LPTSTR IsBTHidDevice(
					  __in USHORT pVendorId,
					  __in USHORT pProductId
					  )
{
	if ( btDeviceInfo != NULL && cbtDeviceInfoSize != 0 )
		for (DWORD i = 0; i < cbtDeviceInfoSize; i++)
			if ( btDeviceInfo[i].pProductId == pProductId && 
				btDeviceInfo[i].usVendorId == pVendorId )
				return btDeviceInfo[i].lptstrBluetoothDeviceName;
	
	return NULL;
}

BOOLEAN SwitchLogitech( __in LPTSTR lpDongleName, __in HANDLE hHidDevice, __in BOOL toHID )
{
	PHIDP_PREPARSED_DATA PreparsedData;

	if ( !HidD_GetPreparsedData( hHidDevice, &PreparsedData ) )
	{
		LbtReportFunctionError( TEXT("HidD_GetPreparsedData") );
		return FALSE;
	}

	HIDP_CAPS       HidCaps;

	if ( !HidP_GetCaps( PreparsedData, &HidCaps ) )
	{
		LbtReportFunctionError( TEXT("HidP_GetCaps") );
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	if ( HidCaps.UsagePage != 0xFF00 || HidCaps.Usage != 0x0001 )
	{
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	if ( HidCaps.OutputReportByteLength != sizeof(ToHCIReports[0]) )
	{
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	DWORD noReports = toHID ? sizeof(ToHCIReports) / sizeof(ToHCIReports[0]) :
		sizeof(ToHIDReports) / sizeof(ToHIDReports[0]);

	for (DWORD i = 0; i < noReports; i++ )
	{
		CHAR ReportBuffer[sizeof(ToHCIReports[0])];
		RtlCopyMemory( ReportBuffer, toHID ? ToHCIReports[i] : ToHIDReports[i], sizeof( ToHCIReports[0] ) );
		if (!HidD_SetOutputReport( hHidDevice, ReportBuffer, HidCaps.OutputReportByteLength ) )
		{
			LbtReportFunctionError( TEXT("HidD_SetOutputReport") );
			HidD_FreePreparsedData(PreparsedData);
			return FALSE;
		}
	}

	HidD_FreePreparsedData(PreparsedData);
	LbtReportDongleSwitch( lpDongleName, toHID );
	return TRUE;
}
