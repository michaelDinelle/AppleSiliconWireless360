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
    CURRENT_PROJECT_VERSION="$(date +%j).$(date +%H%M).$(date +%S)" build | tail -25   # unique per second, at most 4 digits per part

[ "$1" = "--no-install" ] && exit 0
pkill -x Wireless360 2>/dev/null || true
rm -rf /Applications/Wireless360.app
cp -R build/DerivedData/Build/Products/Debug/Wireless360.app /Applications/
open -W /Applications/Wireless360.app   # activates the driver, then quits

# Hand the receiver to the new driver without unplugging it.
clang -Wno-deprecated-declarations -o build/replug tools/replug.c -framework IOKit -framework CoreFoundation
build/replug
