// replug: ask macOS to re-enumerate every attached Xbox 360 wireless receiver, as if it had been
// unplugged and plugged back in. run.sh uses it so a freshly installed driver takes over at once.
// Build: clang -o build/replug tools/replug.c -framework IOKit -framework CoreFoundation
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <stdio.h>

int main(void)
{
    static const int productIDs[] = {0x0719, 0x0291, 0x02A9};
    int found = 0;
    for (size_t p = 0; p < sizeof productIDs / sizeof *productIDs; p++) {
        CFMutableDictionaryRef match = IOServiceMatching("IOUSBHostDevice");
        int vid = 0x045E, pid = productIDs[p];
        CFDictionarySetValue(match, CFSTR("idVendor"), CFNumberCreate(NULL, kCFNumberIntType, &vid));
        CFDictionarySetValue(match, CFSTR("idProduct"), CFNumberCreate(NULL, kCFNumberIntType, &pid));
        io_iterator_t it;
        if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &it) != KERN_SUCCESS) {
            continue;
        }
        for (io_service_t dev; (dev = IOIteratorNext(it)); IOObjectRelease(dev)) {
            found++;
            IOCFPlugInInterface **plugin = NULL;
            IOUSBDeviceInterface **usb = NULL;
            SInt32 score;
            if (IOCreatePlugInInterfaceForService(dev, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin, &score) != KERN_SUCCESS || !plugin) {
                fprintf(stderr, "replug: no USB interface for %04x\n", pid);
                continue;
            }
            (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&usb);
            (*plugin)->Release(plugin);
            if (!usb) {
                continue;
            }
            // Re-enumeration needs the device open. Our driver holds it, so seize it from the driver.
            IOReturn r = (*usb)->USBDeviceOpenSeize(usb);
            if (r == kIOReturnSuccess) {
                r = (*usb)->USBDeviceReEnumerate(usb, 0);
                (*usb)->USBDeviceClose(usb);
            }
            printf("replug: receiver %04x re-enumerate: 0x%x\n", pid, r);
            (*usb)->Release(usb);
        }
        IOObjectRelease(it);
    }
    if (!found) {
        printf("replug: no receiver attached\n");
    }
    return 0;
}
