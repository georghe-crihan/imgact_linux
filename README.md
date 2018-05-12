# imgact_linux

OSX kext to allow running of linux binary executables (through Noah ABI).

Inspired by: 
* [xbinary](http://osxbook.com/software/xbinary),
* [SyscallExt](http://osxbook.com/book/bonus/ancient/syscall) both by Amit Singh, 
* [FreeBSD Linux ABI exec_linux_imgact()](http://fxr.watson.org/fxr/source/i386/linux/imgact_linux.c?v=FREEBSD4)(aka 'The Linuxolator'),
* [XNU exec_shell_imgact()](http://fxr.watson.org/fxr/source/bsd/kern/kern_exec.c?v=xnu-1228#L416),
* the [Noah Linux ABI project](https://github.com/linux-noah/noah).

It should compile without the XCode GUI via _make(1)_.

NB: The kext uses the kernel directly (not via the KPI ABI), so it is strongly kernel version dependent and has to be rebuilt after every version change.

Installing and loading under today's OSX requires either SPI switched off.

NB: Signing via the _codesign(1)_ tool, perhaps with a self-signed CA, certificate installed locally just won't work.

Special thanks to Robert Watson of the FreeBSD project for his marvelous 
[kernel FXR](http://fxr.watson.org/fxr) online.
