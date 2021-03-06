#
#  /Developer.3.26/usr/bin/xcodebuild  -target ext2_kext
#

EXTROOT = /Library/Extensions
#SDK_ROOT = /Developer.3.26
#SYSROOT = $(SDK_ROOT)/SDKs/MacOSX10.4u.sdk
#OSX_VERSION = 10.4
#SDK_ROOT = /Developer.4.6.2/Xcode.app/Contents/Developer
SDK_ROOT = /Developer
SYSROOT = $(SDK_ROOT)/SDKs/MacOSX10.6.sdk
OSX_VERSION = 10.6
XNU_VERSION = 4570.1.46

#CC = $(SDK_ROOT)/usr/bin/gcc-4.0
#LD = $(SDK_ROOT)/usr/bin/gcc-4.0
CC = $(SDK_ROOT)/usr/bin/gcc
LD = $(CC)

# detect current kernel architecture
CPU  := $(shell uname -m)
ifeq ($(CPU),i386)
ARCH = -arch i386
else
ifeq ($(CPU),x86_64)
ARCH = -m64
endif
endif

CFLAGS += -pipe -std=gnu99 -fasm-blocks -fmessage-length=0
# DEBUG
CFLAGS += -gdwarf-2 -O0 
WARNS += -Wno-trigraphs -Wmost -Wfloat-equal -Wno-four-char-constants -Wno-unknown-pragmas
CFLAGS_KERN += -fno-builtin -fno-common \
	-force_cpusubtype_ALL -finline -fno-keep-inline-functions 
CFLAGS_APP += -fpascal-strings -mdynamic-no-pic -fvisibility=hidden
DEFS +=	-DOSX -DDEBUG -DEXT2FS_DEBUG=1 -DDIAGNOSTIC -DDX_DEBUG \
	-DEXT2FS_TRACE -DAPPLE
#DEFS +=	-DBSD -DBYTE_ORDER=LITTLE_ENDIAN

DEFS_KERN += -D_KERNEL -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DNeXT

CFLAGS += -mmacosx-version-min=$(OSX_VERSION) -msoft-float -mkernel

KERN_INCS += -I/System/Library/Frameworks/Kernel.framework/PrivateHeaders \
	-I$(SYSROOT)/System/Library/Frameworks/Kernel.framework/Headers

INCS += -I$(SYSROOT)/usr/include \
	-I.

KEXT_OBJS = \
	exec_shell_imgact.o \
	imgact_linux.o

all: imgact_linux.kmod 

exec_shell_imgact.o: exec_shell_imgact-$(XNU_VERSION).c
	$(CC) $(ARCH) -no-cpp-precomp -nostdinc $(CFLAGS) $(CFLAGS_KERN) $(DEFS) $(DEFS_KERN) $(KERN_INCS) $(INCS) $(WARNS) -c -o exec_shell_imgact.o exec_shell_imgact-$(XNU_VERSION).c

imgact_linux.o: imgact_linux.c
	$(CC) $(ARCH) -isysroot $(SYSROOT) -no-cpp-precomp -nostdinc $(CFLAGS) $(CFLAGS_KERN) $(DEFS) $(DEFS_KERN) $(KERN_INCS) $(INCS) $(WARNS) -c -o imgact_linux.o imgact_linux.c

imgact_linux.kmod: $(KEXT_OBJS)
	$(LD) $(ARCH) \
		-mmacosx-version-min=$(OSX_VERSION) \
		-Xlinker -kext \
		-nostdlib -fno-builtin \
		-lcc_kext -o imgact_linux.kmod $(KEXT_OBJS)

Info.plist:
	./mkinfo.sh Info.plist

execsw_proxy: Proxy.plist
	# See https://github.com/slavaim/dl_kextsymboltool
	# old MacOS X placed the kernel in the root directory
	nm -gj /mach_kernel > allsymbols
	# on the lates macOS the kernel can be found at /System/Library/Kernels
	#nm -gj /System/Library/Kernels/kernel > allsymbols
	echo "_execsw" > execsw_proxy.exports
	# include any more symbols needed

	dl_kextsymboltool -arch i386 -import allsymbols  -export execsw_proxy.exports -output execsw_proxy_32
	dl_kextsymboltool -arch x86_64 -import allsymbols -export execsw_proxy.exports -output execsw_proxy_64

	cp execsw_proxy_64 execsw_proxy
	# or make a universal kext
	#lipo -create execsw_proxy_32 execsw_proxy_64 -output execsw_proxy

proxy-install: execsw_proxy
	sudo rm -rf ${EXTROOT}/execsw_proxy.kext
	sudo mkdir -p ${EXTROOT}/execsw_proxy.kext/Contents/MacOS
	sudo cp execsw_proxy ${EXTROOT}/execsw_proxy.kext/Contents/MacOS/execsw_proxy
	sudo cp Proxy.plist ${EXTROOT}/execsw_proxy.kext/Contents/Info.plist
	sudo chown -R root:wheel ${EXTROOT}/execsw_proxy.kext

install: Info.plist imgact_linux.kmod
	sudo rm -rf $(EXTROOT)/imgact_linux.kext
	sudo mkdir -p $(EXTROOT)/imgact_linux.kext/Contents/MacOS 
	sudo cp imgact_linux.kmod $(EXTROOT)/imgact_linux.kext/Contents/MacOS/imgact_linux
	sudo cp Info.plist $(EXTROOT)/imgact_linux.kext/Contents/Info.plist
	sudo chown -R root:wheel $(EXTROOT)/imgact_linux.kext

codesign:
	sudo /usr/bin/codesign --force --sign 'Self-signed certificate' \
          $(EXTROOT)/imgact_linux.kext

test: install
	sudo kextutil -d ${EXTROOT} -t -v 6 $(EXTROOT)/imgact_linux.kext

clean:
	rm -f $(KEXT_OBJS)

distclean: clean
	rm -f imgact_linux.kmod Info.plist

