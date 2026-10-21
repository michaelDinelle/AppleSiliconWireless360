#include <os/log.h>
#include <DriverKit/IOLib.h>
#include <USBDriverKit/IOUSBHostDevice.h>
#include "Wireless360Receiver.h"

#define LOG(fmt, ...) os_log(OS_LOG_DEFAULT, "wireless360 receiver: " fmt, ##__VA_ARGS__)

kern_return_t IMPL(Wireless360Receiver, Start)
{
    kern_return_t ret = Start(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    IOUSBHostDevice *device = OSDynamicCast(IOUSBHostDevice, provider);
    if (!device) {
        Stop(provider, SUPERDISPATCH);
        return kIOReturnBadArgument;
    }
    ret = device->Open(this, 0, 0);
    if (ret != kIOReturnSuccess) {
        LOG("Open failed: 0x%x", ret);
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    ret = device->SetConfiguration(1, true);
    if (ret != kIOReturnSuccess) {
        LOG("SetConfiguration(1) failed: 0x%x", ret);
        device->Close(this, 0);
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    LOG("configuration 1 selected");
    RegisterService();
    return kIOReturnSuccess;
}

kern_return_t IMPL(Wireless360Receiver, Stop)
{
    IOUSBHostDevice *device = OSDynamicCast(IOUSBHostDevice, provider);
    if (device) {
        device->Close(this, 0);
    }
    return Stop(provider, SUPERDISPATCH);
}
