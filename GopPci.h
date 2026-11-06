/*

	UEFI GOP Miniport PCI interface
	Written by Nasir "Nassic" W. (@nasiratif on GitHub)
	(C) 2026

*/

#ifndef _GOPPCI_H_
#define _GOPPCI_H_

#ifdef _GOPPCI_C_
#include <ntddk.h> // GopPci.c needs to use functions that miniport.h doesn't provide
#else
#include <miniport.h>
#endif

// DEFINES:
// -----
// PCIe allows more but I'm not really expecting that much on any system
#define PCI_MAX_BUS 256
// Number of bars that a GPU would typically have
#define PCI_NUM_BARS 3
#define PCI_BAR_MEMTYPE_MEMORY 0
#define PCI_BAR_MEMTYPE_IO 1
#define PCI_BAR_BITS_32 0x0
#define PCI_BAR_BITS_64 0x2
#define PCI_BAR_IS_IO(baseAddr) ((baseAddr) & 0b1)
#define PCI_GET_BAR_TYPE(baseAddr) ((baseAddr >> 1) & 0b11)
// -----

// Writes a device's PCI_COMMON_CONFIG at the specified bus & slot number (which is a PCI_SLOT_NUMBER as a ULONG)
// Returns the number of bytes actually written to Buffer
ULONG
GopPciGetDeviceConfig(
	ULONG BusNumber,
	ULONG SlotNumber,
	PVOID Buffer,
	ULONG Length);
// Writes the full physical address of a BAR into BaseAddress
// Header is obtained from PCI_COMMON_CONFIG
// CurrentBar, on input, is a pointer to a ULONG containing the index of the BAR to retrieve,
// on output it is set to the next BAR (given the function succeeds), allowing this function to be used iteratively
// IsIo is written to determine whether the address is in I/O space or physical space
// Returns TRUE if a BAR was successfully retrieved, otherwise FALSE
BOOLEAN
GopPciGetDeviceBars(
	PPCI_COMMON_HEADER Header,
	PULONG CurrentBar,
	PPHYSICAL_ADDRESS BaseAddress,
	PBOOLEAN IsIo);

#endif