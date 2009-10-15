#define WIN32_NO_STATUS
#include <windows.h>
extern "C"
{
#include <ntstatus.h>
#include <hidusage.h>
#include <hidsdi.h>
#include <hidpi.h>
}
#include "lhid2hci.h"

CHAR Reports[3][7] =
{
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x80, (CHAR)0x80, (CHAR)0x01, (CHAR)0x00, (CHAR)0x00 },
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x80, (CHAR)0x00, (CHAR)0x00, (CHAR)0x30, (CHAR)0x00 },
	{ (CHAR)0x10, (CHAR)0xFF, (CHAR)0x81, (CHAR)0x80, (CHAR)0x00, (CHAR)0x00, (CHAR)0x00 }
};

BOOLEAN IsBTHidDevice(
					  __in USHORT pVendorId,
					  __in USHORT pProductId
					  )
{
	if ( pVendorId == 0x046d && pProductId == 0xc71f )
		return TRUE;
	else
		return FALSE;
}

BOOLEAN SwitchLogitech( __in HANDLE hHidDevice )
{
	PHIDP_PREPARSED_DATA PreparsedData;

	if ( !HidD_GetPreparsedData( hHidDevice, &PreparsedData ) )
		return FALSE;

	HIDP_CAPS       HidCaps;

	if ( !HidP_GetCaps( PreparsedData, &HidCaps ) )
	{
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	if ( HidCaps.UsagePage != 0xFF00 || HidCaps.Usage != 0x0001 )
	{
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	if ( HidCaps.OutputReportByteLength != sizeof(Reports[0]) )
	{
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	PCHAR HidReport = (PCHAR)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, HidCaps.OutputReportByteLength );

	if ( HidReport == NULL )
	{
		HidD_FreePreparsedData(PreparsedData);
		return FALSE;
	}

	for (int i = 0; i < sizeof(Reports) / sizeof(Reports[0]); i++ )
	{
		CHAR ReportBuffer[sizeof(Reports[0])];
		RtlCopyMemory( ReportBuffer, Reports[i], sizeof( Reports[0] ) );
		if (!HidD_SetOutputReport( hHidDevice, ReportBuffer, HidCaps.OutputReportByteLength ) )
		{
			HidD_FreePreparsedData(PreparsedData);
			return FALSE;
		}
	}

	HidD_FreePreparsedData(PreparsedData);
	return TRUE;
}
