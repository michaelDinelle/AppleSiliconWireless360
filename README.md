# Wireless360

A macOS driver for the Xbox 360 Wireless Receiver. It runs on Apple Silicon and recent macOS with System Integrity Protection left on.

Each connected wireless controller appears as a standard gamepad. That makes it work in games that use Apple's Game Controller framework, SDL or IOHID, including Steam games. It supports up to four controllers per receiver. A controller only appears while it is switched on, and its ring light shows its player number.

Supported receivers (USB IDs):

- `045E:0719`: Microsoft Xbox 360 Wireless Receiver for Windows
- `045E:0291`: Microsoft, earlier revision
- `045E:02A9`: common third-party clone

You don't need this for wired Xbox 360 controllers. macOS 15 and later supports them natively.

## Status

Early development. Tested with a `045E:0719` receiver on macOS 27.2 (Apple Silicon):

- The driver loads with SIP on, and the controller's ring shows its player number.
- Apple's Game Controller framework sees each pad as an extended gamepad, with correct buttons, analog triggers and sticks (`tools/gc_check.swift`). So do apps that read HID directly.

Each pad presents itself as an **Xbox Wireless Controller** (`045E:0B13`) and sends that controller's Bluetooth report layout. On macOS 27, Game Controller only accepts HID devices from third-party drivers when their ID is on Apple's `GCIOMatchVirtual` list. The `IOGCDynamicDeviceProbe` and `AppleGCHIDUserEventDriver` probes reject anything whose bundle ID doesn't start with `com.apple.`, and they key on the device's real bundle ID, which a driver can't change.

There is no rumble or headset support yet.

## Install

There are no signed releases yet. Distributing a driver requires a DriverKit entitlement from Apple, which hasn't been granted yet.

## Build from source

Requirements:

- A paid Apple Developer account
- Xcode
- [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install xcodegen`)

In `project.yml`, set `DEVELOPMENT_TEAM` and the bundle IDs to your own. Then run:

```sh
./run.sh
```

The script does the following:

1. Runs the protocol test.
2. Builds the app and the driver.
3. Copies the app to `/Applications` and opens it. The app has no window: it activates the driver and quits.
4. Re-enumerates the receiver (`tools/replug.c`), so the new driver takes over without unplugging.

The first time, approve the driver in System Settings → General → Login Items & Extensions → Driver Extensions. The app opens that page for you. After that, plug in the receiver and press the guide button on the controller. To uninstall, move the app to the Trash.

To see the driver's logs:

```sh
log stream --predicate 'eventMessage CONTAINS "wireless360"'
```

## How it works

- `Wireless360Receiver` selects the receiver's USB configuration.
- `Wireless360Slot` runs once per controller slot. It polls the slot's interrupt endpoint and creates or removes a `Wireless360Pad` HID device as controllers connect and disconnect.
- `protocol/x360w.c` parses the packets and builds the HID report. It's plain C with a host-side test.

The packet formats come from the Linux `xpad` driver and SDL. The HID descriptor comes from [OpenJoystickDriver](https://github.com/xsyetopz/OpenJoystickDriver). The driver structure follows [macgip](https://github.com/leetrout/macgip).

Xbox is a trademark of the Microsoft group of companies. This project is not affiliated with Microsoft.
