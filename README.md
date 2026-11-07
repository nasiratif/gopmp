# gopmp (UEFI GOP Video Miniport)

This is an experimental miniport driver for Windows 7 (can work on Vista too) x64 that allows it to display video output using framebuffers exposed for the UEFI Graphics Output Protocol (GOP). It's supposed to replace the *vga.sys* file Windows uses for VGA output.

Although Windows 7 has support for UEFI boot, it does not make use of GOP for it's standard video drivers, which means that if you attempt to boot Windows 7 on, say a modern device that is either UEFI Class 3 and/or doesn't have a legacy VBIOS, chances are likely you'd just get stuck on the *Starting Windows* screen, due to it insisting on using VGA/VBE which is increasingly being dropped on modern systems.

Several projects have been made by others to try & workaround this problem; one of them is [UefiSeven](https://github.com/manatails/uefiseven) (which I have never gotten to work on my systems, seems to only play well with Intel hardware from what I've seen), others are [VgaShim](https://github.com/davidcie/VgaShim) (which UefiSeven is based on, personally no success with this one either), as well as [Flashboot Pro](https://www.prime-expert.com/flashboot/) (paid, but probably the best option).

<small>Side note: in the case that you get stuck on the *Starting Windows* screen (and the logo is still animating), Windows has actually still booted; it just still shows the *Starting Windows* screen because the VGA driver fails to initialize (lack of `int 10h` and VGA I/O ports), this can be proven by pressing ESC, Left, Enter in order (opens a confirmation message box asking to quit the installer) and your computer will actually restart, as the OS has responded to it.</small>

## How it works

The driver enumerates through a bunch of PCI buses to locate a suitable display controller and finds a non-IO base address register (BAR) that is mappable within the window of the framebuffer `(1024 * 768 * 4)`. Afterwards this address is assumed to be the on-screen framebuffer and is passed to Windows to direct video output to.

Unlike the standard legacy VGA system, where the framebuffer is located at a fixed address (normally `0xA0000`), GOP framebuffers could be mapped anywhere depending on the machine, hence why this sort of approach is necessary.

## The catch

Probably won't work if you have multiple GPUs/monitors (it might pick the wrong one). The driver is also unsigned, so you are required to disable driver signature enforcement to use it.

I label this driver as 'experimental' for a reason. I'm not aware of any standardized way to find the physical address of the current GOP framebuffer outside of boot services, as no GOP interfaces are available for a Windows driver (drivers have no access to UEFI boot services, of course). What this basically means is that this driver just plays a guessing game on where the framebuffer may possibly be, there's no guarantees this will work for you. It seems to be reliable from my testing across a few configurations, though.

Also, this driver does not handle BSODs (that also insists on `int 10h`, so you won't see it unfortunately).

## Usage

### If you already have Windows 7 installed
Replace your *vga.sys* file with this driver. It is located in *Windows\System32\drivers*. As noted above you will need to have driver signature enforcement disabled as this driver is not signed.

### If you don't yet have Windows 7 installed
Mount your `boot.wim` file using this command:

```bat
dism /Mount-image /imagefile:path\to\boot.wim /Index:<edition_index> /MountDir:path\to\your\already\existing\mountdir
```
`<edition_index>` is normally `2`. You can use a tool like [Dism++](https://github.com/Chuyu-Team/Dism-Multi-language) to find out what to set it to (look for the *Windows Setup* image entry in the WIM).

Then, in the mount directory, replace your *vga.sys* file with this driver. Afterwards commit the changes:

```bat
dism /Commit-image /MountDir:path\to\your\already\existing\mountdir
```

Then unmount the image if you want:

```bat
dism /Unmount-image /MountDir:path\to\your\already\existing\mountdir /discard
```

You should be able to boot into the Windows installer and see video output now. 

Note that once you've installed Windows you will have to replace *vga.sys* again in the drive you installed Windows in.

As I briefly mentioned above this driver is also compatible with Windows Vista. Unfortunately there you won't see the startup screen, though, that presumably is still trying to use legacy VGA, however after the OS loads the driver does kick in.

## License/Contributing
The source code is available; do whatever you want with it! Just don't be naughty and claim you were the original author of the code.

Contributions are appreciated, especially if you figure out a better & more reliable way to locate the framebuffer rather than the brute-force method I decided to use. Just be sure to stick to WDM code conventions.

## Credits

- The [ReactOS](https://github.com/reactos/reactos) project for showing me how a video miniport is made
- [OSDev.org](https://wiki.osdev.org/PCI) for teaching me how PCI works