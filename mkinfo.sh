#!/bin/sh

KERNEL_VERSION=$(uname -r)
ARCH=$(uname -m)
EXECUTABLE_NAME=imgact_linux
PRODUCT_NAME=${EXECUTABLE_NAME}
echo Producing Info.plist for ${KERNEL_VERSION}...
cat <<EOP > "${1}"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>English</string>
	<key>CFBundleExecutable</key>
	<string>${EXECUTABLE_NAME}</string>
	<key>CFBundleName</key>
	<string>${PRODUCT_NAME}</string>
	<key>CFBundleIconFile</key>
	<string></string>
	<key>CFBundleIdentifier</key>
	<string>com.github.kext.${PRODUCT_NAME}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundlePackageType</key>
	<string>KEXT</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>CFBundleVersion</key>
	<string>1.0.0d1</string>
	<key>OSBundleLibraries</key>
	<dict>
EOP
if [ ${ARCH} == "x86_64" ]; then
cat <<EOP >> "${1}"
                <key>com.github.kext.execsw_proxy</key>
                <string>10.0.0</string>
                <key>com.apple.kpi.libkern</key>
                <string>8.0.0b2</string>
                <key>com.apple.kpi.bsd</key>
                <string>8.0.0b2</string>
                <key>com.apple.kpi.unsupported</key>
                <string>8.0.0b2</string>
                <key>com.apple.kpi.iokit</key>
                <string>8.0.0b2</string>
EOP
else # Alas, this is no longer permitted in 64 bit world...
cat <<EOP >> "${1}"
		<key>com.apple.kernel</key>
		<string>${KERNEL_VERSION}</string>
EOP
fi

cat <<EOP >> "${1}"
	</dict>
</dict>
</plist>
EOP
