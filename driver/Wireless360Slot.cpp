// Flow: Start finds the slot's interrupt endpoints, asks the receiver whether a controller is
// present, and arms one IN read. ReadComplete creates the pad on connect, terminates it on
// disconnect, forwards input reports, and re-arms the read at once.
#include <os/log.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <USBDriverKit/USBDriverKit.h>
#include <USBDriverKit/IOUSBHostInterface.h>
#include <USBDriverKit/IOUSBHostPipe.h>
#include <USBDriverKit/AppleUSBDescriptorParsing.h>
#include "x360w.h"
#include "Wireless360Pad.h"
#include "Wireless360Slot.h"

#define LOG(fmt, ...) os_log(OS_LOG_DEFAULT, "wireless360 slot %u: " fmt, ivars->slot, ##__VA_ARGS__)

static const uint32_t kPacketSize = 32;

struct Wireless360Slot_IVars {
    IOUSBHostInterface *interface;
    IOUSBHostPipe *inPipe;
    IOUSBHostPipe *outPipe;
    IOBufferMemoryDescriptor *inBuffer;
    IOBufferMemoryDescriptor *outBuffer;
    uint8_t *inBytes;
    uint8_t *outBytes;
    OSAction *readAction;
    OSAction *writeAction;
    Wireless360Pad *pad;
    unsigned slot;
    bool interfaceOpen;
    bool stopping;
    bool writeBusy;
};

bool Wireless360Slot::init()
{
    if (!super::init()) {
        return false;
    }
    ivars = IONewZero(Wireless360Slot_IVars, 1);
    return ivars != nullptr;
}

void Wireless360Slot::free()
{
    if (ivars) {
        OSSafeReleaseNULL(ivars->pad);
        OSSafeReleaseNULL(ivars->readAction);
        OSSafeReleaseNULL(ivars->writeAction);
        OSSafeReleaseNULL(ivars->inBuffer);
        OSSafeReleaseNULL(ivars->outBuffer);
        OSSafeReleaseNULL(ivars->inPipe);
        OSSafeReleaseNULL(ivars->outPipe);
        OSSafeReleaseNULL(ivars->interface);
    }
    IOSafeDeleteNULL(ivars, Wireless360Slot_IVars, 1);
    super::free();
}

static uint8_t *bufferBytes(IOBufferMemoryDescriptor *buffer)
{
    IOAddressSegment range = {};
    if (buffer->GetAddressRange(&range) != kIOReturnSuccess) {
        return nullptr;
    }
    return reinterpret_cast<uint8_t *>(range.address);
}

// Finds the interrupt IN and OUT endpoints and the slot number (interface 0, 2, 4, 6 -> 0..3).
static bool findEndpoints(IOUSBHostInterface *interface, uint8_t *in, uint8_t *out, unsigned *slot)
{
    const IOUSBConfigurationDescriptor *config = interface->CopyConfigurationDescriptor();
    if (!config) {
        return false;
    }
    const IOUSBInterfaceDescriptor *desc = interface->GetInterfaceDescriptor(config);
    if (desc) {
        *slot = desc->bInterfaceNumber / 2;
        const IOUSBEndpointDescriptor *ep = nullptr;
        while ((ep = IOUSBGetNextEndpointDescriptor(config, desc, reinterpret_cast<const IOUSBDescriptorHeader *>(ep)))) {
            if (IOUSBGetEndpointType(ep) != kIOUSBEndpointTypeInterrupt) {
                continue;
            }
            if (IOUSBGetEndpointDirection(ep) == kIOUSBEndpointDirectionIn) {
                *in = IOUSBGetEndpointAddress(ep);
            } else {
                *out = IOUSBGetEndpointAddress(ep);
            }
        }
    }
    IOUSBHostFreeDescriptor(config);
    return desc && *in && *out;
}

// Abort transfers asynchronously and close the interface. Completions arrive on this driver's
// queue, so a synchronous abort from Stop would wait on itself.
static void teardown(Wireless360Slot *self, Wireless360Slot_IVars *iv)
{
    iv->stopping = true;
    if (iv->pad) {
        iv->pad->Terminate(0);
        OSSafeReleaseNULL(iv->pad);
    }
    if (iv->inPipe) {
        iv->inPipe->Abort(0, kIOReturnAborted, self);
    }
    if (iv->outPipe) {
        iv->outPipe->Abort(0, kIOReturnAborted, self);
    }
    if (iv->interface && iv->interfaceOpen) {
        iv->interface->Close(self, 0);
        iv->interfaceOpen = false;
    }
}

kern_return_t IMPL(Wireless360Slot, Start)
{
    Wireless360Slot_IVars *iv = ivars;
    kern_return_t ret = Start(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    iv->interface = OSDynamicCast(IOUSBHostInterface, provider);
    if (!iv->interface) {
        Stop(provider, SUPERDISPATCH);
        return kIOReturnBadArgument;
    }
    iv->interface->retain();

    uint8_t in = 0, out = 0;
    if (!findEndpoints(iv->interface, &in, &out, &iv->slot)) {
        LOG("interrupt endpoints not found");
        Stop(provider, SUPERDISPATCH);
        return kIOReturnNotFound;
    }
    ret = iv->interface->Open(this, 0, 0);
    if (ret == kIOReturnSuccess) {
        iv->interfaceOpen = true;
        ret = iv->interface->CopyPipe(in, &iv->inPipe);
    }
    if (ret == kIOReturnSuccess) {
        ret = iv->interface->CopyPipe(out, &iv->outPipe);
    }
    if (ret == kIOReturnSuccess) {
        ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionIn, kPacketSize, 0, &iv->inBuffer);
    }
    if (ret == kIOReturnSuccess) {
        ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut, X360W_COMMAND_SIZE, 0, &iv->outBuffer);
    }
    if (ret == kIOReturnSuccess) {
        ret = CreateActionReadComplete(0, &iv->readAction);
    }
    if (ret == kIOReturnSuccess) {
        ret = CreateActionWriteComplete(0, &iv->writeAction);
    }
    if (ret == kIOReturnSuccess) {
        iv->inBytes = bufferBytes(iv->inBuffer);
        iv->outBytes = bufferBytes(iv->outBuffer);
        if (!iv->inBytes || !iv->outBytes) {
            ret = kIOReturnNoMemory;
        }
    }
    if (ret != kIOReturnSuccess) {
        LOG("setup failed: 0x%x", ret);
        teardown(this, iv);
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    iv->inBuffer->SetLength(kPacketSize);
    iv->outBuffer->SetLength(X360W_COMMAND_SIZE);

    // Ask whether a controller is already paired to this slot. The answer arrives as a presence
    // packet on the IN pipe. Synchronous is safe here because the read loop is not running yet.
    x360w_presence_query(iv->outBytes);
    uint32_t sent = 0;
    ret = iv->outPipe->IO(iv->outBuffer, X360W_COMMAND_SIZE, &sent, 0);
    if (ret != kIOReturnSuccess) {
        LOG("presence query failed: 0x%x", ret);
    }

    ret = iv->inPipe->AsyncIO(iv->inBuffer, kPacketSize, iv->readAction, 0);
    if (ret != kIOReturnSuccess) {
        LOG("first AsyncIO failed: 0x%x", ret);
        teardown(this, iv);
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    RegisterService();
    LOG("started, endpoints in 0x%02x out 0x%02x", in, out);
    return kIOReturnSuccess;
}

kern_return_t IMPL(Wireless360Slot, Stop)
{
    teardown(this, ivars);
    return Stop(provider, SUPERDISPATCH);
}

static void connectPad(Wireless360Slot *self, Wireless360Slot_IVars *iv)
{
    if (iv->pad) {
        return;
    }
    IOService *service = nullptr;
    kern_return_t ret = self->Create(self, "Wireless360PadProperties", &service);
    if (ret != kIOReturnSuccess || !service) {
        os_log(OS_LOG_DEFAULT, "wireless360 slot %u: creating pad failed: 0x%x", iv->slot, ret);
        return;
    }
    iv->pad = OSDynamicCast(Wireless360Pad, service);
    if (!iv->pad) {
        service->Terminate(0);
        service->release();
        return;
    }
    os_log(OS_LOG_DEFAULT, "wireless360 slot %u: controller connected", iv->slot);

    if (!iv->writeBusy) {
        x360w_set_led(iv->outBytes, iv->slot);
        iv->writeBusy = iv->outPipe->AsyncIO(iv->outBuffer, X360W_COMMAND_SIZE, iv->writeAction, 0) == kIOReturnSuccess;
    }
}

void IMPL(Wireless360Slot, ReadComplete)
{
    Wireless360Slot_IVars *iv = ivars;
    if (iv->stopping || status == kIOReturnAborted) {
        return;
    }
    if (status == kIOReturnSuccess) {
        switch (x360w_classify(iv->inBytes, actualByteCount)) {
        case X360W_CONNECTED:
            connectPad(this, iv);
            break;
        case X360W_DISCONNECTED:
            if (iv->pad) {
                LOG("controller disconnected");
                iv->pad->Terminate(0);
                OSSafeReleaseNULL(iv->pad);
            }
            break;
        case X360W_INPUT: {
            connectPad(this, iv);   // input without a presence packet: the pad was already on
            if (iv->pad) {
                uint8_t report[X360W_REPORT_SIZE];
                x360w_build_report(iv->inBytes, report);
                iv->pad->postReport(report, sizeof report, completionTimestamp);
            }
            break;
        }
        case X360W_OTHER:
            break;
        }
    } else {
        LOG("read error 0x%x", status);
    }
    kern_return_t ret = iv->inPipe->AsyncIO(iv->inBuffer, kPacketSize, iv->readAction, 0);
    if (ret != kIOReturnSuccess) {
        LOG("re-arm AsyncIO failed: 0x%x", ret);
    }
}

void IMPL(Wireless360Slot, WriteComplete)
{
    ivars->writeBusy = false;
}
