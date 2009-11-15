#ifndef   __LHID2HCI_H__
#define   __LHID2HCI_H__

#include <windows.h>

LPTSTR IsBTHidDevice( 
	__in USHORT pVendorId,
	__in USHORT pProductId
	);


BOOLEAN SwitchLogitech( __in LPTSTR lpDongleName, __in HANDLE hHidDevice, __in BOOL toHID );

typedef struct _BT_DEVICE_INFO
{
	TCHAR lptstrBluetoothDeviceName[256];
	USHORT usVendorId;
	USHORT pProductId;
} BT_DEVICE_INFO, *PBT_DEVICE_INFO;

BOOLEAN LoadSupportedBluetoothDevices();

#endif //__LHID2HCI_H__