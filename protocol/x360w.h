// Xbox 360 wireless receiver protocol: parse IN packets, build OUT commands, build the HID report.
// Packet formats follow the Linux xpad driver and SDL's SDL_hidapi_xbox360w.c.
// Plain C with no DriverKit dependencies so it can be tested on the host.
#ifndef X360W_H
#define X360W_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum x360w_kind {
    X360W_OTHER = 0,     // battery, serial, keepalive and anything else we ignore
    X360W_CONNECTED,
    X360W_DISCONNECTED,
    X360W_INPUT,
};

#define X360W_COMMAND_SIZE 12
#define X360W_REPORT_SIZE 15
#define X360W_REPORT_ID 1

enum x360w_kind x360w_classify(const uint8_t *packet, size_t length);

// Writes the HID input report for an X360W_INPUT packet. `report` must hold X360W_REPORT_SIZE bytes.
void x360w_build_report(const uint8_t *packet, uint8_t *report);

// Each writes X360W_COMMAND_SIZE bytes.
void x360w_presence_query(uint8_t *out);
void x360w_set_led(uint8_t *out, unsigned slot);   // slot 0..3 lights quadrant 1..4

#ifdef __cplusplus
}
#endif

#endif
