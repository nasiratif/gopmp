/*

	UEFI GOP Miniport Driver implementation
	Written by Nasir "Nassic" W. (@nasiratif on GitHub)
	(C) 2026

*/

#include "GopMp.h"
#include "GopPci.h"

// Due to GOP limitations this is the only mode we can expose; it's first of all
// not accessible after the boot service stage (the framebuffer is all that remains),
// and this is a driver anyways so not that we'd have the pointer to boot services in the first place.
// So we don't have any way of exposing multiple video modes.
VOID
GopGetAdapterMode(
	PVIDEO_MODE_INFORMATION out)
{
	out->Length = sizeof(*out);
	out->ModeIndex = 0;
	out->VisScreenWidth = GOP_MODE_WIDTH;
	out->VisScreenHeight = GOP_MODE_HEIGHT;
	out->ScreenStride = GOP_MODE_STRIDE;
	out->NumberOfPlanes = 1;
	out->BitsPerPlane = 32; // GOP is always 32bpp
	out->Frequency = 60; // no way to actually retrieve this AFAIK, just using 60 as filler
	out->XMillimeter = 512; // dunno if these millimeter fields are correct and dunno if it even matters
	out->YMillimeter = 384;
	out->NumberRedBits =
	out->NumberGreenBits =
	out->NumberBlueBits = 8;
	out->RedMask = 0xFF0000;
	out->GreenMask = 0x00FF00;
	out->BlueMask = 0x0000FF; // I think PixelBlueGreenRedReserved8BitPerColor is always used by Windows
	out->AttributeFlags = VIDEO_MODE_COLOR | VIDEO_MODE_GRAPHICS | VIDEO_MODE_NO_OFF_SCREEN;
	out->VideoMemoryBitmapWidth = out->VisScreenWidth;
	out->VideoMemoryBitmapHeight = out->VisScreenHeight;
	out->DriverSpecificAttributeFlags = 0;
}

VP_STATUS
GopFindAdapter(
	PGOP_DATA HwDeviceExtension,
	PVOID HwContext,
	PWSTR ArgumentString,
	PVIDEO_PORT_CONFIG_INFO ConfigInfo,
	PUCHAR Again)
{
	if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO))
		return ERROR_INSUFFICIENT_BUFFER;

	ULONG Bus, Function, Device, Size;
	PCI_SLOT_NUMBER SlotNumber;
	PCI_COMMON_HEADER PciHeader;

	PHYSICAL_ADDRESS BaseAddress;
	ULONG CurrentBar;
	BOOLEAN IsIo;

	VIDEO_ACCESS_RANGE AccessRange;

	ULONG MapFlags, MapLength;
	PVOID MapAddress;

	// Do a brute-force loop across all PCI buses,
	// if a display controller is found then get it's BARs to guess where the framebuffer *might* be located.
	// AFAIK since we don't have access to GOP interfaces this is the only way we could figure out where
	// the framebuffer might be. This is not guaranteed to be reliable (and what if there are multiple
	// display controllers? which one is really being used?) and I'm just relying on the unproven 
	// assumption that one of the first display controller's BARs corresponds to the active framebuffer (seems to 
	// work on my class 3 UEFI laptop, as well as VMWare & QEMU).
	// Can't just make use of the bus number provided in ConfigInfo because the device we want might not
	// be there:
	for (Bus = 0; Bus < PCI_MAX_BUS; ++Bus)
	{
		for (Device = 0; Device < PCI_MAX_DEVICES; ++Device)
		{
			for (Function = 0; Function < PCI_MAX_FUNCTION; ++Function)
			{
				SlotNumber.u.bits.Reserved = 0;
				SlotNumber.u.bits.DeviceNumber = Device;
				SlotNumber.u.bits.FunctionNumber = Function;

				Size = GopPciGetDeviceConfig(Bus, SlotNumber.u.AsULONG, &PciHeader, sizeof(PciHeader));
				if (Size < sizeof(PciHeader) ||
					PciHeader.BaseClass != PCI_CLASS_DISPLAY_CTLR ||
					PciHeader.SubClass != PCI_SUBCLASS_VID_VGA_CTLR ||
					(PciHeader.HeaderType & ~PCI_MULTIFUNCTION) != 0)
					continue;

				CurrentBar = 0;
				// Enumerate through 3 of the PCI BARs to locate a suitable physical address that might correspond to the GOP framebuffer:
				while (GopPciGetDeviceBars(&PciHeader, &CurrentBar, &BaseAddress, &IsIo))
				{
					if (IsIo) // we want a memory-mapped framebuffer, which I don't think would ever be in I/O space
						continue;

					// Check if this memory (all of it) is actually mappable.
					// Just a further check to ensure this really could be the framebuffer:
					// -----
					MapFlags = VIDEO_MEMORY_SPACE_MEMORY;
					MapLength = GOP_MODE_FRAMEBUFFERSIZE;
					MapAddress = NULL;
					if (VideoPortMapMemory(HwDeviceExtension, BaseAddress, &MapLength, &MapFlags, &MapAddress) != NO_ERROR)
						continue;

					VideoPortUnmapMemory(HwDeviceExtension, MapAddress, NULL);
					if (MapLength < GOP_MODE_FRAMEBUFFERSIZE)
						continue; // it's not big enough
					// -----
					
					// Attempt to claim the access range for the framebuffer.
					// It doesn't necessarily succeed however so I'm just ignoring the return value.
					// Should still be alright if it can't be claimed as the address would be mapped anyways (?)
					// -----
					AccessRange.RangeStart = BaseAddress;
					AccessRange.RangeLength = GOP_MODE_FRAMEBUFFERSIZE;
					AccessRange.RangeInIoSpace = FALSE;
					AccessRange.RangeVisible = FALSE;
					AccessRange.RangeShareable = FALSE;
					AccessRange.RangePassive = FALSE;
					VideoPortVerifyAccessRanges(HwDeviceExtension, 1, &AccessRange);
					// -----

					// Nice, we got something:
					HwDeviceExtension->FrameBufferBase = BaseAddress;
					// Set known information:
					ConfigInfo->AdapterInterfaceType = PCIBus;
					ConfigInfo->SystemIoBusNumber = Bus;
					return NO_ERROR;
				}
			}
		}
	}
	// :(
	return ERROR_DEV_NOT_EXIST;
}

BOOLEAN
GopInitialize(
	PGOP_DATA HwDeviceExtension)
{
	// The initialization work is done in FindAdapter
	return TRUE;
}

BOOLEAN
GopStartIO(
	PGOP_DATA HwDeviceExtension,
	PVIDEO_REQUEST_PACKET RequestPacket)
{
	BOOLEAN Result = TRUE;
	RequestPacket->StatusBlock->Status = NO_ERROR;

	switch (RequestPacket->IoControlCode)
	{
	case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:
	{
		// The only video mode that is exposed is 1024x768@32bpp, because that's what the Windows bootloader will set the GOP framebuffer to, and since we don't have access to boot services there's no way we can change that.

		if (RequestPacket->OutputBufferLength < sizeof(VIDEO_NUM_MODES))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}

		PVIDEO_NUM_MODES Mode = (PVIDEO_NUM_MODES)RequestPacket->OutputBuffer;
		Mode->NumModes = 1;
		Mode->ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);
		RequestPacket->StatusBlock->Information = sizeof(VIDEO_NUM_MODES);
		break;
	}
	case IOCTL_VIDEO_QUERY_AVAIL_MODES:
	case IOCTL_VIDEO_QUERY_CURRENT_MODE:
	{
		if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MODE_INFORMATION))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}

		GopGetAdapterMode((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer);
		RequestPacket->StatusBlock->Information = sizeof(VIDEO_MODE_INFORMATION);
		break;
	}
	case IOCTL_VIDEO_SET_CURRENT_MODE:
	{
		if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}
		PVIDEO_MODE VidMode = (PVIDEO_MODE)RequestPacket->InputBuffer;
		if (VidMode->RequestedMode != 0) // again we only have one mode
		{
			RequestPacket->StatusBlock->Status = ERROR_INVALID_PARAMETER;
			Result = FALSE;
			break;
		}
		break;
	}
	case IOCTL_VIDEO_RESET_DEVICE: // can't do this but we won't return an error
	{
		break;
	}
	case IOCTL_VIDEO_MAP_VIDEO_MEMORY:
	{
		if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY) ||
			RequestPacket->OutputBufferLength < sizeof(VIDEO_MEMORY_INFORMATION))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}

		PVIDEO_MEMORY VidMem = (PVIDEO_MEMORY)RequestPacket->InputBuffer;

		ULONG RequestedLength = GOP_MODE_FRAMEBUFFERSIZE;
		PVOID RequestedAddress = VidMem->RequestedVirtualAddress;
		ULONG IoSpace = VIDEO_MEMORY_SPACE_MEMORY;
		if (VideoPortMapMemory(HwDeviceExtension, HwDeviceExtension->FrameBufferBase, &RequestedLength, &IoSpace, &RequestedAddress) != NO_ERROR)
		{
			RequestPacket->StatusBlock->Status = ERROR_NOT_ENOUGH_MEMORY;
			Result = FALSE;
			break;
		}

		PVIDEO_MEMORY_INFORMATION VidMemInfo = (PVIDEO_MEMORY_INFORMATION)RequestPacket->OutputBuffer;
		VidMemInfo->VideoRamBase = VidMemInfo->FrameBufferBase = RequestedAddress;
		VidMemInfo->VideoRamLength = VidMemInfo->FrameBufferLength = RequestedLength;
		RequestPacket->StatusBlock->Information = sizeof(VIDEO_MEMORY_INFORMATION);
		break;
	}
	case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:
	{
		if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}
		PVIDEO_MEMORY VidMem = (PVIDEO_MEMORY)RequestPacket->InputBuffer;
		VideoPortUnmapMemory(HwDeviceExtension, VidMem->RequestedVirtualAddress, NULL);
		break;
	}
	// NB: haven't tested shared video memory and idk how I would trigger that
	case IOCTL_VIDEO_SHARE_VIDEO_MEMORY:
	{
		if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY) ||
			RequestPacket->OutputBufferLength < sizeof(VIDEO_SHARE_MEMORY_INFORMATION))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}

		PVIDEO_SHARE_MEMORY VidMem = (PVIDEO_SHARE_MEMORY)RequestPacket->InputBuffer;
		if (VidMem->ViewSize == 0 ||
			VidMem->ViewOffset >= GOP_MODE_FRAMEBUFFERSIZE ||
			VidMem->ViewSize > GOP_MODE_FRAMEBUFFERSIZE ||
			VidMem->ViewOffset + VidMem->ViewSize > GOP_MODE_FRAMEBUFFERSIZE)
		{
			RequestPacket->StatusBlock->Status = ERROR_INVALID_PARAMETER;
			Result = FALSE;
			break;
		}

		PHYSICAL_ADDRESS RequestedBase;
		RequestedBase.QuadPart = HwDeviceExtension->FrameBufferBase.QuadPart + VidMem->ViewOffset;
		ULONG RequestedLength = VidMem->ViewSize;
		PVOID RequestedAddress = VidMem->RequestedVirtualAddress;
		ULONG IoSpace = VIDEO_MEMORY_SPACE_MEMORY;
		if (VideoPortMapMemory(HwDeviceExtension, RequestedBase, &RequestedLength, &IoSpace, &RequestedAddress) != NO_ERROR)
		{
			RequestPacket->StatusBlock->Status = ERROR_NOT_ENOUGH_MEMORY;
			Result = FALSE;
			break;
		}

		PVIDEO_SHARE_MEMORY_INFORMATION VidMemInfo = (PVIDEO_SHARE_MEMORY_INFORMATION)RequestPacket->OutputBuffer;
		VidMemInfo->SharedViewOffset = VidMem->ViewOffset;
		VidMemInfo->SharedViewSize = VidMem->ViewSize;
		VidMemInfo->VirtualAddress = RequestedAddress;
		RequestPacket->StatusBlock->Information = sizeof(VIDEO_SHARE_MEMORY_INFORMATION);
		break;
	}
	case IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY:
	{
		if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY))
		{
			RequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
			Result = FALSE;
			break;
		}
		PVIDEO_SHARE_MEMORY VidMem = (PVIDEO_SHARE_MEMORY)RequestPacket->InputBuffer;
		VideoPortUnmapMemory(HwDeviceExtension, VidMem->RequestedVirtualAddress, VidMem->ProcessHandle);
		break;
	}
	default:
		RequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
		Result = FALSE;
	}
	return Result;
}

BOOLEAN
GopResetHw(
	PGOP_DATA HwDeviceExtension,
	ULONG Columns,
	ULONG Rows)
{
	return FALSE; // nothing we can do here
}

/*

---------- ENTRY POINT ----------

*/

ULONG
DriverEntry(
	PVOID Context1,
	PVOID Context2)
{
	VIDEO_HW_INITIALIZATION_DATA HwData;
	VideoPortZeroMemory(&HwData, sizeof(HwData));
	HwData.HwInitDataSize = sizeof(HwData);
	HwData.HwDeviceExtensionSize = sizeof(GOP_DATA);
	HwData.HwFindAdapter = GopFindAdapter;
	HwData.HwInitialize = GopInitialize;
	HwData.HwStartIO = GopStartIO;
	HwData.HwResetHw = GopResetHw;
	return VideoPortInitialize(Context1, Context2, &HwData, NULL);

	// there is also new functions like HwSet/GetPowerState, and HwGetChildVideoDescriptor,
	// which apparently are required according to Microsoft docs but they don't actually
	// seem to be, and not that we can do what they request anyways
}