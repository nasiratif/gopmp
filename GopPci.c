/*

	UEFI GOP Miniport PCI interface
	Written by Nasir "Nassic" W. (@nasiratif on GitHub)
	(C) 2026

*/

#define _GOPPCI_C_
#include "GopPci.h"

ULONG
GopPciGetDeviceConfig(
	ULONG BusNumber,
	ULONG SlotNumber,
	PVOID Buffer,
	ULONG Length)
{
	// VideoPortGetBusData is a thing, but not that it works.. apparently
	// (it doesn't even let me specify a bus number)
	// So I'm resorting to this instead, it's deprecated in the latest WDK but so is videoprt itself
    return HalGetBusDataByOffset(PCIConfiguration, BusNumber, SlotNumber, Buffer, 0, Length);
}

BOOLEAN
GopPciGetDeviceBars(
	PPCI_COMMON_HEADER Header,
	PULONG CurrentBar,
	PPHYSICAL_ADDRESS BaseAddress,
	PBOOLEAN IsIo)
{
	if ((*CurrentBar) >= PCI_NUM_BARS)
		return FALSE;

	struct _PCI_HEADER_TYPE_0* Type0 = &Header->u.type0;

	*IsIo = PCI_BAR_IS_IO(Type0->BaseAddresses[*CurrentBar]);
	if (*IsIo)
	{
		BaseAddress->QuadPart = Type0->BaseAddresses[(*CurrentBar)++] & 0xFFFFFFFC;
	}
	else
	{
		switch (PCI_GET_BAR_TYPE(Type0->BaseAddresses[*CurrentBar]))
		{
		case PCI_BAR_BITS_32:
		{
			BaseAddress->QuadPart = Type0->BaseAddresses[(*CurrentBar)++] & 0xFFFFFFF0;
			break;
		}
		case PCI_BAR_BITS_64:
		{
			if (*CurrentBar + 1 >= PCI_NUM_BARS)
				return FALSE; // somehow

			BaseAddress->QuadPart = ((ULONGLONG)Type0->BaseAddresses[*CurrentBar] & 0xFFFFFFF0) + (((ULONGLONG)Type0->BaseAddresses[(*CurrentBar) + 1] & 0xFFFFFFFF) << 32);
			(*CurrentBar) += 2;
			break;
		}
		default:
			return FALSE;
		}
	}
	return TRUE;
}
