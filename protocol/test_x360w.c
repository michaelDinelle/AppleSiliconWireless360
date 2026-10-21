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
    assert(r[3] == 0x00 && r[4] == 0x00);   // LY -32768 -> 0
    assert(r[5] == 0x00 && r[6] == 0x80);   // RX      0 -> 32768
    assert(r[7] == 0xFF && r[8] == 0x7F);   // RY     -1 -> 32767
    assert(r[9] == 0xFF && r[10] == 0x00);  // LT
    assert(r[11] == 0x00 && r[12] == 0x00); // RT
    assert(r[13] == ((1u << 3) | (1u << 4)));   // dpad left, A
    assert(r[14] == ((1u << 4) | (1u << 6)));   // start, guide

    uint8_t cmd[X360W_COMMAND_SIZE];
    x360w_set_led(cmd, 1);
    assert(cmd[2] == 0x08 && cmd[3] == 0x47);
    x360w_presence_query(cmd);
    assert(cmd[0] == 0x08 && cmd[2] == 0x0F && cmd[3] == 0xC0);

    puts("ok");
    return 0;
}
