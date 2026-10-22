// Host-side check of the protocol core: cc -Wall -Wextra -Werror protocol/*.c -o /tmp/t && /tmp/t
#include <assert.h>
#include <stdio.h>
#include "x360w.h"

int main(void)
{
    const uint8_t connected[] = {0x08, 0x80};
    const uint8_t disconnected[] = {0x08, 0x00};
    assert(x360w_classify(connected, 2) == X360W_CONNECTED);
    assert(x360w_classify(disconnected, 2) == X360W_DISCONNECTED);

    // A pressed, dpad left, start, guide, LT 0xFF, LX max, LY min, RX 0, RY -1.
    uint8_t in[29] = {0x00, 0x01, 0x00, 0xF0, 0x00, 0x13,
                      0x04 | 0x10, 0x10 | 0x04, 0xFF, 0x00,
                      0xFF, 0x7F, 0x00, 0x80, 0x00, 0x00, 0xFF, 0xFF};
    assert(x360w_classify(in, sizeof in) == X360W_INPUT);
    assert(x360w_classify(in, 17) == X360W_OTHER);

    uint8_t r[X360W_REPORT_SIZE];
    x360w_build_report(in, r);
    assert(r[0] == X360W_REPORT_ID);
    assert(r[1] == 0xFF && r[2] == 0xFF);   // LX  32767 -> 65535
    assert(r[3] == 0xFF && r[4] == 0xFF);   // LY -32768 (full down) -> 65535 (HID down)
    assert(r[5] == 0x00 && r[6] == 0x80);   // RX      0 -> 32768
    assert(r[7] == 0x00 && r[8] == 0x80);   // RY     -1 -> 32768
    assert(r[9] == 0xFF && r[10] == 0x03);  // LT 255 -> 1023
    assert(r[11] == 0x00 && r[12] == 0x00); // RT
    assert(r[13] == 7);                     // hat: left
    assert(r[14] == 0x01);                  // A
    assert(r[15] == (0x08 | 0x10));         // menu, guide
    assert(r[16] == 0x00);

    in[6] = 0x01 | 0x08;                    // up + right
    x360w_build_report(in, r);
    assert(r[13] == 2);
    in[6] = 0x01 | 0x02;                    // up + down cancel
    x360w_build_report(in, r);
    assert(r[13] == 0);

    uint8_t cmd[X360W_COMMAND_SIZE];
    x360w_set_led(cmd, 1);
    assert(cmd[2] == 0x08 && cmd[3] == 0x47);
    x360w_presence_query(cmd);
    assert(cmd[0] == 0x08 && cmd[2] == 0x0F && cmd[3] == 0xC0);

    puts("ok");
    return 0;
}
