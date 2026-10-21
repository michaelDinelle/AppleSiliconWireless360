#include <os/log.h>
#include <string.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/IOHIDDeviceKeys.h>
#include <HIDDriverKit/IOHIDUsageTables.h>
#include "x360w.h"
#include "Wireless360Pad.h"

#define LOG(fmt, ...) os_log(OS_LOG_DEFAULT, "wireless360 pad: " fmt, ##__VA_ARGS__)

// Layout adapted from macgip (MIT, github.com/leetrout/macgip), which verified on hardware that it
// makes IOHIDEventDriver classify the device as an extended game controller: sticks on X/Y and
// Z/Rz, triggers on Button 7/8, d-pad on Generic Desktop DPad usages, face buttons on Button 1..4,
// shoulders on 5/6, stick clicks on 9/10, and start/back/guide as Consumer AC Exit/Properties/Home.
// Changes: triggers are 0..255 (the 360 range) and there is no rumble output report.
static const uint8_t kReportDescriptor[] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop)
    0x09, 0x05,                   // Usage (Game Pad)
    0xA1, 0x01,                   // Collection (Application)
    0x85, X360W_REPORT_ID,        //   Report ID (1)
    0x09, 0x01,                   //   Usage (Pointer)
    0xA1, 0x00,                   //   Collection (Physical)
    0x09, 0x30,                   //     Usage (X)
    0x09, 0x31,                   //     Usage (Y)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65535)
    0x95, 0x02,                   //     Report Count (2)
    0x75, 0x10,                   //     Report Size (16)
    0x81, 0x02,                   //     Input (Data, Var, Abs)
    0xC0,                         //   End Collection
    0x09, 0x01,                   //   Usage (Pointer)
    0xA1, 0x00,                   //   Collection (Physical)
    0x09, 0x32,                   //     Usage (Z)
    0x09, 0x35,                   //     Usage (Rz)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65535)
    0x95, 0x02,                   //     Report Count (2)
    0x75, 0x10,                   //     Report Size (16)
    0x81, 0x02,                   //     Input (Data, Var, Abs)
    0xC0,                         //   End Collection
    0x05, 0x09,                   //   Usage Page (Button)
    0x09, 0x07,                   //   Usage (Button 7) left trigger
    0x09, 0x08,                   //   Usage (Button 8) right trigger
    0x15, 0x00,                   //   Logical Minimum (0)
    0x26, 0xFF, 0x00,             //   Logical Maximum (255)
    0x95, 0x02,                   //   Report Count (2)
    0x75, 0x10,                   //   Report Size (16)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x05, 0x01,                   //   Usage Page (Generic Desktop)
    0x09, 0x90,                   //   Usage (DPad Up)
    0x09, 0x91,                   //   Usage (DPad Down)
    0x09, 0x92,                   //   Usage (DPad Right)
    0x09, 0x93,                   //   Usage (DPad Left)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x04,                   //   Report Count (4)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (1)
    0x29, 0x06,                   //   Usage Maximum (6)  A B X Y LB RB
    0x95, 0x06,                   //   Report Count (6)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x19, 0x09,                   //   Usage Minimum (9)
    0x29, 0x0A,                   //   Usage Maximum (10)  LS RS
    0x95, 0x02,                   //   Report Count (2)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x05, 0x0C,                   //   Usage Page (Consumer)
    0x0A, 0x04, 0x02,             //   Usage (AC Exit) start
    0x0A, 0x09, 0x02,             //   Usage (AC Properties) back
    0x0A, 0x23, 0x02,             //   Usage (AC Home) guide
    0x95, 0x03,                   //   Report Count (3)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x03,                   //   Input (Const) padding to 14 data bytes
    0xC0,                         // End Collection
};

struct Wireless360Pad_IVars {
    IOBufferMemoryDescriptor *report;
    uint8_t *reportBytes;
};

bool Wireless360Pad::init()
{
    if (!super::init()) {
        return false;
    }
    ivars = IONewZero(Wireless360Pad_IVars, 1);
    return ivars != nullptr;
}

void Wireless360Pad::free()
{
    if (ivars) {
        OSSafeReleaseNULL(ivars->report);
    }
    IOSafeDeleteNULL(ivars, Wireless360Pad_IVars, 1);
    super::free();
}

bool Wireless360Pad::handleStart(IOService *provider)
{
    if (!super::handleStart(provider)) {
        return false;
    }
    IOAddressSegment range = {};
    if (IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut, X360W_REPORT_SIZE, 0, &ivars->report) != kIOReturnSuccess ||
        ivars->report->GetAddressRange(&range) != kIOReturnSuccess) {
        LOG("report buffer allocation failed");
        return false;
    }
    ivars->report->SetLength(X360W_REPORT_SIZE);
    ivars->reportBytes = reinterpret_cast<uint8_t *>(range.address);
    RegisterService();
    LOG("started");
    return true;
}

OSDictionary *Wireless360Pad::newDeviceDescription(void)
{
    OSDictionary *d = OSDictionary::withCapacity(8);
    if (!d) {
        return nullptr;
    }
    OSDictionarySetUInt64Value(d, kIOHIDVendorIDKey, 0x045E);
    // 0x028E, the wired Xbox 360 controller: GameController.framework adopts it by this ID
    // ("Xbox 360 Controller IOService" in AppleGameControllerPersonality). The receiver's own
    // 0x0719 is not listed, and its generic "dynamic" route did not match on macOS 27.2.
    OSDictionarySetUInt64Value(d, kIOHIDProductIDKey, 0x028E);
    OSDictionarySetStringValue(d, kIOHIDTransportKey, "USB");
    OSDictionarySetStringValue(d, kIOHIDManufacturerKey, "Microsoft");
    OSDictionarySetStringValue(d, kIOHIDProductKey, "Xbox 360 Wireless Controller");
    OSDictionarySetUInt64Value(d, kIOHIDPrimaryUsagePageKey, kHIDPage_GenericDesktop);
    OSDictionarySetUInt64Value(d, kIOHIDPrimaryUsageKey, kHIDUsage_GD_GamePad);
    OSDictionarySetUInt64Value(d, kIOHIDReportIntervalKey, 8000);
    return d;
}

OSData *Wireless360Pad::newReportDescriptor(void)
{
    return OSData::withBytes(kReportDescriptor, sizeof kReportDescriptor);
}

void Wireless360Pad::postReport(const uint8_t *report, uint32_t length, uint64_t timestamp)
{
    if (!ivars->reportBytes || length != X360W_REPORT_SIZE) {
        return;
    }
    memcpy(ivars->reportBytes, report, length);
    handleReport(timestamp, ivars->report, length, kIOHIDReportTypeInput, 0);
}
