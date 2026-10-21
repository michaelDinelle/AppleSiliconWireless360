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

- Works: the driver loads with SIP on, the controller's ring shows its player number, and every button, trigger and stick reaches apps that read HID directly through IOKit or SDL (`tools/hid_watch.c`).
- Not working yet: Apple's Game Controller framework doesn't adopt the pad, so games that only use it (for example World of Warcraft) don't see the controller. On macOS 27.2 none of `AppleGameControllerPersonality`'s drivers attach to it, even when it reports the wired Xbox 360 ID `028E`.

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

The script runs the protocol test, builds the app and driver, copies the app to `/Applications` and opens it.

1. Click **Install driver**.
2. Approve the driver in System Settings → General → Login Items & Extensions → Driver Extensions.
3. Plug in the receiver and press the guide button on the controller.

To see the driver's logs:

```sh
log stream --predicate 'eventMessage CONTAINS "wireless360"'
```

## How it works

- `Wireless360Receiver` selects the receiver's USB configuration.
- `Wireless360Slot` runs once per controller slot. It polls the slot's interrupt endpoint and creates or removes a `Wireless360Pad` HID device as controllers connect and disconnect.
- `protocol/x360w.c` parses the packets and builds the HID report. It's plain C with a host-side test.

The packet formats come from the Linux `xpad` driver and SDL. The HID layout is adapted from [macgip](https://github.com/leetrout/macgip).

Xbox is a trademark of the Microsoft group of companies. This project is not affiliated with Microsoft.
