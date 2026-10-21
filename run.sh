#!/bin/bash
# Test the protocol core, build the app and driver (Debug), copy the app to /Applications, open it.
# macOS only loads a driver from an app in /Applications.
set -eo pipefail   # a failed xcodebuild must stop before the install step
cd "$(dirname "$0")"

mkdir -p build
cc -std=c11 -Wall -Wextra -Werror protocol/x360w.c protocol/test_x360w.c -o build/test_x360w
build/test_x360w

xcodegen generate --quiet
xcodebuild -project Wireless360.xcodeproj -scheme Wireless360 -configuration Debug \
    -derivedDataPath build/DerivedData -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
    CURRENT_PROJECT_VERSION="1.$(date +%j).$(date +%H%M)" build | tail -25

[ "$1" = "--no-install" ] && exit 0
osascript -e 'quit app "Wireless360"' 2>/dev/null || true
rm -rf /Applications/Wireless360.app
cp -R build/DerivedData/Build/Products/Debug/Wireless360.app /Applications/
open /Applications/Wireless360.app
