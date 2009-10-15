#ifndef   __LHID2HCI_H__
#define   __LHID2HCI_H__

#include <windows.h>

BOOLEAN IsBTHidDevice( 
	__in USHORT pVendorId,
	__in USHORT pProductId
	);


BOOLEAN SwitchLogitech( __in HANDLE hHidDevice );

#endif //__LHID2HCI_H__