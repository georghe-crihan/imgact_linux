#!/bin/sh

EXTROOT=/Library/Extensions
SRC="${1}"

if [ ! -d "${1}" ]; then
    echo "The ${1} binary does not seem to exist!"
    exit 1
fi

case "${2}" in
"proxy-install")
	sudo rm -rf ${EXTROOT}/execsw_proxy.kext
	sudo mkdir -p ${EXTROOT}/execsw_proxy.kext/Contents/MacOS
	sudo cp ../execsw_proxy ${EXTROOT}/execsw_proxy.kext/Contents/MacOS/execsw_proxy
	sudo cp ../Proxy.plist ${EXTROOT}/execsw_proxy.kext/Contents/Info.plist
	sudo chown -R root:wheel ${EXTROOT}/execsw_proxy.kext
        ;;
"install"|*)
	sudo rm -rf ${EXTROOT}/imgact_linux.kext
	sudo mkdir -p ${EXTROOT}/imgact_linux.kext/Contents/MacOS 
	sudo cp ${SRC}/imgact_linux.kmod ${EXTROOT}/imgact_linux.kext/Contents/MacOS/imgact_linux
	sudo cp ${SRC}/Info.plist ${EXTROOT}/imgact_linux.kext/Contents/Info.plist
	sudo chown -R root:wheel ${EXTROOT}/imgact_linux.kext
        ;;
"codesign")
	sudo /usr/bin/codesign --force --sign 'Self-signed certificate' \
          ${EXTROOT}/imgact_linux.kext
        ;;
"test")
	sudo kextutil -d ${EXTROOT} -t -v 6 ${EXTROOT}/imgact_linux.kext
esac

