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

// Hat switch value for dpad bits up, down, left, right (xpad order). 0 is centered, 1..8 run
// clockwise from up. Opposite directions pressed together cancel out.
static uint8_t hat(uint8_t lo)
{
    int x = !!(lo & 0x08) - !!(lo & 0x04);   // right - left
    int y = !!(lo & 0x02) - !!(lo & 0x01);   // down - up
    static const uint8_t table[3][3] = {
        {8, 1, 2},   // y = -1 (up): left, none, right
        {7, 0, 3},   // y =  0
        {6, 5, 4},   // y = +1 (down)
    };
    return table[y + 1][x + 1];
}

// Xbox Wireless Controller (045E:0B13) Bluetooth report 1, the layout of kReportDescriptor in
// Wireless360Pad.cpp and of SDL's HIDAPI xboxone Bluetooth parser:
// [0] ID, [1..8] LX LY RX RY unsigned 16-bit with Y down, [9..12] LT RT 10-bit, [13] hat,
// [14] A B - X Y - LB RB, [15] - - view menu guide LS RS -, [16] share (unused).
void x360w_build_report(const uint8_t *p, uint8_t *r)
{
    uint8_t lo = p[IN_BUTTONS_LO], hi = p[IN_BUTTONS_HI];

    memset(r, 0, X360W_REPORT_SIZE);
    r[0] = X360W_REPORT_ID;
    for (int i = 0; i < 4; i++) {
        uint16_t raw = (uint16_t)(p[IN_LX + 2 * i] | (p[IN_LX + 2 * i + 1] << 8));
        uint16_t u = (uint16_t)(raw ^ 0x8000);   // int16 to 0..65535
        put16(r + 1 + 2 * i, (i & 1) ? (uint16_t)(0xFFFF - u) : u);   // 360 Y is up, HID Y is down
    }
    put16(r + 9, (uint16_t)(p[IN_LT] << 2 | p[IN_LT] >> 6));   // 0..255 to 0..1023
    put16(r + 11, (uint16_t)(p[IN_RT] << 2 | p[IN_RT] >> 6));
    r[13] = hat(lo);

    if (hi & 0x10) r[14] |= 0x01;   // A
    if (hi & 0x20) r[14] |= 0x02;   // B
    if (hi & 0x40) r[14] |= 0x08;   // X
    if (hi & 0x80) r[14] |= 0x10;   // Y
    if (hi & 0x01) r[14] |= 0x40;   // LB
    if (hi & 0x02) r[14] |= 0x80;   // RB
    if (lo & 0x20) r[15] |= 0x04;   // back -> view
    if (lo & 0x10) r[15] |= 0x08;   // start -> menu
    if (hi & 0x04) r[15] |= 0x10;   // guide
    if (lo & 0x40) r[15] |= 0x20;   // LS
    if (lo & 0x80) r[15] |= 0x40;   // RS
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
