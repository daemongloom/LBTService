#ifndef   __LHID2HCI_H__
#define   __LHID2HCI_H__

#include <windows.h>

typedef BOOLEAN(*ConversionFunction)(__in LPTSTR lpDongleName, __in HANDLE hHidDevice, __in BOOL toHID);

LPTSTR IsBTHidDevice( 
	__in USHORT pVendorId,
	__in USHORT pProductId,
	__out ConversionFunction *conversionFunction
	);


BOOLEAN SwitchLogitech( __in LPTSTR lpDongleName, __in HANDLE hHidDevice, __in BOOL toHID );
BOOLEAN SwitchCSR(__in LPTSTR lpDongleName, __in HANDLE hHidDevice, __in BOOL toHID);

typedef struct _BT_DEVICE_INFO
{
	TCHAR lptstrBluetoothDeviceName[256];
	USHORT usVendorId;
	USHORT pProductId;
	TCHAR lptstrProtocol[256];
} BT_DEVICE_INFO, *PBT_DEVICE_INFO;

BOOLEAN LoadSupportedBluetoothDevices();

#endif //__LHID2HCI_H__