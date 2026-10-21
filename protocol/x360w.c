#include <string.h>
#include "x360w.h"

// Offsets into an input packet. The 360 input report starts at byte 4.
enum {
    IN_BUTTONS_LO = 6,   // dpad up down left right, start, back, LS, RS
    IN_BUTTONS_HI = 7,   // LB, RB, guide, (unused), A, B, X, Y
    IN_LT = 8,
    IN_RT = 9,
    IN_LX = 10,          // four little-endian int16: LX LY RX RY, up is positive
    IN_MIN_LENGTH = 18,
};

enum x360w_kind x360w_classify(const uint8_t *p, size_t n)
{
    if (n >= 2 && p[0] == 0x08) {
        return (p[1] & 0x80) ? X360W_CONNECTED : X360W_DISCONNECTED;
    }
    if (n >= IN_MIN_LENGTH && p[0] == 0x00 && (p[1] & 0x01)) {   // same test as SDL
        return X360W_INPUT;
    }
    return X360W_OTHER;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

// Report layout matches kReportDescriptor in Wireless360Pad.cpp:
// [0] ID, [1..8] X Y Z Rz as unsigned 16-bit, [9..12] LT RT, [13] dpad up down right left + A B X Y,
// [14] LB RB LS RS, start (AC Exit), back (AC Properties), guide (AC Home), pad.
void x360w_build_report(const uint8_t *p, uint8_t *r)
{
    uint8_t lo = p[IN_BUTTONS_LO], hi = p[IN_BUTTONS_HI];

    r[0] = X360W_REPORT_ID;
    // int16 to unsigned 0..65535. Y passes through unflipped: GameController reads up as positive.
    for (int i = 0; i < 4; i++) {
        uint16_t raw = (uint16_t)(p[IN_LX + 2 * i] | (p[IN_LX + 2 * i + 1] << 8));
        put16(r + 1 + 2 * i, (uint16_t)(raw ^ 0x8000));
    }
    put16(r + 9, p[IN_LT]);
    put16(r + 11, p[IN_RT]);

    uint8_t b13 = 0, b14 = 0;
    if (lo & 0x01) b13 |= 1u << 0;   // dpad up
    if (lo & 0x02) b13 |= 1u << 1;   // dpad down
    if (lo & 0x08) b13 |= 1u << 2;   // dpad right (HID 0x92)
    if (lo & 0x04) b13 |= 1u << 3;   // dpad left (HID 0x93)
    if (hi & 0x10) b13 |= 1u << 4;   // A
    if (hi & 0x20) b13 |= 1u << 5;   // B
    if (hi & 0x40) b13 |= 1u << 6;   // X
    if (hi & 0x80) b13 |= 1u << 7;   // Y
    if (hi & 0x01) b14 |= 1u << 0;   // LB
    if (hi & 0x02) b14 |= 1u << 1;   // RB
    if (lo & 0x40) b14 |= 1u << 2;   // LS
    if (lo & 0x80) b14 |= 1u << 3;   // RS
    if (lo & 0x10) b14 |= 1u << 4;   // start
    if (lo & 0x20) b14 |= 1u << 5;   // back
    if (hi & 0x04) b14 |= 1u << 6;   // guide
    r[13] = b13;
    r[14] = b14;
}

void x360w_presence_query(uint8_t *out)
{
    static const uint8_t cmd[X360W_COMMAND_SIZE] = {0x08, 0x00, 0x0F, 0xC0};
    memcpy(out, cmd, sizeof cmd);
}

void x360w_set_led(uint8_t *out, unsigned slot)
{
    memset(out, 0, X360W_COMMAND_SIZE);
    out[2] = 0x08;
    out[3] = (uint8_t)(0x40 + 6 + (slot % 4));   // LED patterns 6..9: quadrant 1..4 solid
}
