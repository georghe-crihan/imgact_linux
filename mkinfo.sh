#!/bin/sh

KERNEL_VERSION=$(uname -r)
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
		<key>com.apple.kpi.libkern</key>
                <string>8.0.0b2</string>
                <key>com.apple.kpi.bsd</key>
                <string>8.0.0b2</string>
                <key>com.apple.kpi.unsupported</key>
                <string>8.0.0b2</string>
		<key>com.apple.kpi.iokit</key>	
                <string>8.0.0b2</string>
<!-- Alas, this is no longer permitted in 64 bit world...
		<key>com.apple.kernel</key>
		<string>${KERNEL_VERSION}</string>
-->
	</dict>
</dict>
</plist>
EOP
