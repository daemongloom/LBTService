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

CHAR Reports[3][6] =
{
	{ 0xFF, 0x80, 0x80, 0x01, 0x00, 0x00 },
	{ 0xFF, 0x80, 0x00, 0x00, 0x30, 0x00 },
	{ 0xFF, 0x81, 0x80, 0x00, 0x00, 0x00 }
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

BOOLEAN TryToSwitchHid2Hci( __in HANDLE hHidDevice )
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

	if ( HidCaps.OutputReportByteLength != 7 )
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

	for (int i = 0; i < 3; i++ )
	{
		HidReport[0] = 0x10;
		RtlCopyMemory( HidReport + 1, Reports[i], 6);

		if (!HidD_SetOutputReport( hHidDevice, HidReport, HidCaps.OutputReportByteLength ))
		{
			HeapFree( GetProcessHeap(), 0, HidReport );
			HidD_FreePreparsedData(PreparsedData);
			return FALSE;
		}
	}

	HeapFree( GetProcessHeap(), 0, HidReport );
	HidD_FreePreparsedData(PreparsedData);
	return TRUE;
}
