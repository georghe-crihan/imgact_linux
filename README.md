# imgact_linux

OSX kext to allow running of linux binary executables (through Noah ABI)
transparently.

I.e. 
1. set the executable mode bits for a Linux ELF
2. load the `imgact_linux` kext
3. make sure you have Noah binary installed under _/opt/local/libexec/noah_
4. make sure you have a Linux filesystem hierarchy installed under 
_/compat/linux_ (use _noahstrap(1)_ to install it)
5. make sure /var/log/noah directory is world-writable
6. run your Linux executable just by executing it! I.e. double-clicking
in Finder or launching from the shell.

NB: many of the above settings could be altered by modifying the
`imgact_linux` sysctl: `kern.imgact_linux.interpreter_commandline`.

## A word of caution
This KEXT is extremely XNU version-dependent. It will certainly crash your
system, if the versions of _exec_shell_imgact()_ thereto differ significantly.

That is because it literally hijacks Apple's unexported in-kernel execsw
executable switch requiring a proxy kext in addition.
The default shell activator is hijacked (I didn't want to mess with mach-o,
which would make things even worse).

Sadly, unlike FreeBSD, Apple does  not provide any mechanisms to install custom
image activators, hence the inevitable hack.

On the other hand, it does work, if your kernel matches the version of
_exec_shell_imgact()_ the kext would use. See the Building section for more
details.

You have been warned!

## Inspired by: 
* [xbinary](http://osxbook.com/software/xbinary),
* [SyscallExt](http://osxbook.com/book/bonus/ancient/syscall) both by Amit Singh, 
* [FreeBSD kernel exec_elf_imgact()](http://fxr.watson.org/fxr/source/kern/imgact_elf.c?v=FREEBSD4#L466)
* [FreeBSD Linux ABI exec_linux_imgact()](http://fxr.watson.org/fxr/source/i386/linux/imgact_linux.c?v=FREEBSD4)(aka 'The Linuxolator'),
* [XNU exec_shell_imgact()](http://fxr.watson.org/fxr/source/bsd/kern/kern_exec.c?v=xnu-1228#L416),
* the [Noah Linux ABI project](https://github.com/linux-noah/noah).
* [Proxy kexts for unexported kernel symbols by Slava Imameev](https://github.com/slavaim/dl_kextsymboltool).

Special thanks go to Robert Watson of the FreeBSD project for his marvelous 
[kernel FXR](http://fxr.watson.org/fxr) online and to Slava Imameev for his `dl_kextsymboltool`.

## Building
### Introduction
It should compile without the XCode GUI via _make(1)_ and the command line
tools.

NB: The 32-bit kext calls the kernel directly, not via the KPI ABI but through
com.apple.kernel, so no proxy kext is required.

The activator kext is strongly kernel version dependent and has to be rebuilt
after every version change, see [mkinfo.sh](mkinfo.sh).

Installing and loading under today's OSX requires either SPI switched off.

NB: Signing via the _codesign(1)_ tool, perhaps with a self-signed CA, certificate installed locally just won't work.

### Make
Edit the Makefile to set the appropriate SDK, SYSROOT and CC and most
importantly, the XNU_VERSION.

See _uname -a_ to find it out and look for appropriate 
_exec_shell_imgact-*.c_ file.

#### Make targets:
* `clean` - remove build artifacts, except for products - plists, kexts 
* `distclean` - remove everything, except for the proxy kext
* `all` - build products
* `execsw_proxy` - generate the proxy kext, requires `dl_kextsymboltool`
* `proxy-install` - install the `execsw_proxy` kext to _/Library/Extensions_
* `codesign` - attempt to sign the kext - wouldn't help anyway
* `install` - install the `imgact_linux` kext to _/Library/Extensions_ 
* `test` - implies `install`, loads the kext into the kernel 

#### Adding support for your kernel

NB1: You're on your own, at your own risk, I do not provide any warranties,
express or implied and hereby disclaim any liability whatsoever.

NB2: This is not a primer, you have to have some kernel experience to do this
safely.

Look at the
https://github.com/opensource-apple/xnu/blob/master/bsd/kern/kern_exec.c

Make sure you switch to the closest possbile version tag (see the _Branch:_
selector at the top left-hand side).

Basically:
1. create the _exec_shell_imgact-X.Y.Z.c_ file, where the _X.Y.Z_ is the XNU
version from the above
2. Copy the #include<>s from the original XNU _kern_exec.c_
3. Add the _#include "imgact_linux.h"_ afterwards
4. Copy the _exec_save_path()_, _exec_reset_save_path()_, _exec_shell_imgact()_
if any, i.e. the function designated as _"Interpreter Script"_ image activator
at the _execsw[]_ declarations at the top, as well as all its dependencies and
all the necessary definitions.
5. Rename _exec_shell_imgact()_ to _my_exec_shell_imgact()_.
6. You might need to add some more definitions in order to satisfy all of 
the _my_exec_shell_imgact()_ dependencies.
7. Make sure to include
``` c
	const Elf_Ehdr *hdr = (const Elf_Ehdr *) imgp->ip_vdata;
	char *vdata = interp_bufr;
```
and exclude conflicting definitions to ensure proper parsing of the
'shebang path' and its parameters.

8. Watch out for _vdata[2]_ as well as _\n_ and _#_  as line terminators, as
it's no longer a 'shebang path', but a plain, c-string command line.

9. Replace the
``` c
	/*
	 * Make sure it's a shell script.  If we've already redirected
	 * from an interpreted file once, don't do it again.
	 */
	if (vdata[0] != '#' ||
	    vdata[1] != '!' ||
	    (imgp->ip_flags & IMGPF_INTERPRET) != 0) {
		return (-1);
	}
```
check by
``` c
	/*
	 * Do we have a valid ELF header ?
	 */
	if (elf_check_header(hdr) != 0 || hdr->e_type != ET_EXEC)
		return (orig_shell_imgact(imgp));

        if (kdebug_enable)
	    printf("ELF brand (OS ABI): %x\n", hdr->e_ident[EI_OSABI]);
```
10. You might need to comment out some of the more shell-activator-specific
(e.g. SUID _/dev/fd/X_ tricks) parts of _my_exec_shell_imgact()_
11. Make sure you chain to the _orig_shell_imgact()_ on header mismatch to
allow the system run normal shell scripts
12. _return (-3)_ on successful completion, to launch the interpreter proper.

YMMV, as it's a non-trivial, creative process!

You can always use the provided _exec_shell_imgact-*.c_ files as reference.
