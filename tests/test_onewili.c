#include "onewili.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ow_device dev;
static const char *response;
static size_t cursor;
static int io_error;
static int write_bytes(void *ctx, const uint8_t *data, size_t len) {
    (void)ctx;
    if (len == 2) assert(memcmp(data, "\x02\n", 2) == 0);
    else {
        const char expected[] = "\x02h\\a\\b 0 16\n";
        assert(len == sizeof(expected) - 1 && memcmp(data, expected, len) == 0);
    }
    return io_error ? -1 : (int)len;
}
static int read_bytes(void *ctx, uint8_t *data, size_t cap, uint32_t timeout) {
    (void)ctx; (void)timeout;
    if (io_error) return -1;
    if (!response || !response[cursor]) return 0;
    /* Fragment every response to exercise the actual official line parser. */
    assert(cap); *data = (uint8_t)response[cursor++]; return 1;
}
static ow_status query(const char *text, uint8_t *out, size_t cap, size_t *length) {
    response = text; cursor = 0;
    return ow_hardware_system_read_otp_info(&dev, 0, 16, out, cap, length);
}
int main(void) {
    ow_transport t = {NULL, write_bytes, read_bytes};
    assert(ow_open(&dev, &t) == OW_OK);
    uint8_t bytes[16]; size_t n = 0;
    assert(query("[h\\a\\b 123 1 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 1]\n", bytes, sizeof(bytes), &n) == OW_OK);
    assert(n == 16 && bytes[15] == 15);
    assert(query("[h\\a\\b 123 2 command_rejected 0]\n", bytes, sizeof(bytes), &n) == OW_ERR_FAILED);
    assert(query(NULL, bytes, sizeof(bytes), &n) == OW_ERR_TIMEOUT);
    io_error = 1;
    assert(query(NULL, bytes, sizeof(bytes), &n) == OW_ERR_IO);
    io_error = 0;
    assert(query("[h\\a\\b 123 3 GG 1]\n", bytes, sizeof(bytes), &n) == OW_ERR_PROTOCOL);
    assert(query("[h\\a\\b 123 4 00 01 1]\n", bytes, 1, &n) == OW_ERR_BUFFER);
    response = NULL; cursor = 0;
    assert(ow_raw_send(&dev, "h\\a\\b 0 16") > 0);
    char raw[128]; int accepted = 0;
    assert(ow_raw_next_response(&dev, raw, sizeof(raw), &accepted, 0) == OW_ERR_TIMEOUT);
    response = "[h\\a\\b 123 5 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 1]\n";
    assert(ow_raw_next_response(&dev, raw, sizeof(raw), &accepted, 0) == OW_OK && accepted);
    assert(strcmp(raw, "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F") == 0);
    puts("PASS: pinned generated C API, read-only command bytes, fragmented responses, errors, timeout, decoding");
    return 0;
}
