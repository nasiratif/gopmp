/*

	UEFI GOP Miniport Driver v1.0
	Written by Nasir "Nassic" W. (@nasiratif on GitHub)
	(C) 2026

	Primarily intended for Windows 7, although it works on Vista as well.

	Modern computers now make use of a new video interface called GOP (Graphics Output Protocol).
	This was introduced in UEFI 2.0, and is meant to replace the older interface of using VGA BIOS
	interrupts (int 10h) with a more convenient standardized interface to directly access the
	GPU framebuffer.

	The problem is that Windows 7/Vista only makes partial use of it, their standard VGA drivers
	always assume legacy VGA hardware is available, no support is made for GOP, which poses
	a problem for modern systems as VGA BIOS ports/interrupts are no longer being supported on
	these systems.

	CSM mode is meant to rectify this, but even that's not available on most modern systems,
	particularly laptops.

	The bootloaders, however, seem to be using GOP as they set the
	graphics mode to 1024x768 and display the Windows is loading
	files/Starting Windows screen by means of it.

	This driver aims to be a drop-in replacement for vga.sys (Windows's standard VGA
	driver used prior to Windows 8). Rather than utilizing legacy VGA mechanisms,
	it instead searches for a potential GOP framebuffer in the system by means of PCI
	configuration enumeration and, if found, maps it to Windows for video output.

	I hope this works for you!

*/

#ifndef _GOPMP_H_
#define _GOPMP_H_

#include <miniport.h>
#include <dderror.h>
#include <ntddvdeo.h>
#include <video.h>
#include <devioctl.h>

// DEFINES:
// -----
// Windows bootloader will set the resolution to 1024x768 so that's all we can expose
#define GOP_MODE_WIDTH 1024
#define GOP_MODE_HEIGHT 768
#define GOP_MODE_STRIDE (GOP_MODE_WIDTH * 4)
// Minimum size of the framebuffer
#define GOP_MODE_FRAMEBUFFERSIZE (GOP_MODE_WIDTH * GOP_MODE_HEIGHT * 4)
// -----

// Device extension for this miniport
typedef struct
{
	PHYSICAL_ADDRESS FrameBufferBase;
} GOP_DATA, * PGOP_DATA;

#endif