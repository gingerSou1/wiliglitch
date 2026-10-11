#include "hardware.h"
#include "input/app_recovery_onewili.h"
#include "platform/diag.h"
#include <stdlib.h>
#include <string.h>
static ow_device device;
_Static_assert(OW_OK == 0 && OW_ERR_ARG == 1 && OW_ERR_IO == 2 && OW_ERR_TIMEOUT == 3 &&
    OW_ERR_FAILED == 4 && OW_ERR_PROTOCOL == 5 && OW_ERR_BUFFER == 6, "Status mapping changed");
static int open_link(void *context) {
    return fw2_app_recovery_open_onewili(context);
}
static int begin(void *context) {
    ow_raw_stash_clear();
    /* Documented read_otp_info command, through official split-phase hooks. */
    int n = ow_raw_send(context, "h\\a\\b 0 16");
    return n < 0 ? -n : OW_OK;
}
static int probe(void *context, uint8_t *bytes, size_t capacity, size_t *length) {
    char response[128]; int ok = 0;
    ow_status status = ow_raw_next_response(context, response, sizeof(response), &ok, 0);
    if (status == OW_ERR_TIMEOUT) return -2;
    if (status != OW_OK) return status;
    if (!ok) return OW_ERR_FAILED;
    *length = 0;
    char *cursor = response;
    while (*cursor) {
        while (*cursor == ' ') ++cursor;
        if (!*cursor) break;
        char *end; unsigned long value = strtoul(cursor, &end, 16);
        if (end == cursor || (*end && *end != ' ') || value > 255) return OW_ERR_PROTOCOL;
        if (*length == capacity) return OW_ERR_BUFFER;
        bytes[(*length)++] = (uint8_t)value; cursor = end;
    }
    DIAG("WiliGlitch M2A MAIN OTP reply: bytes=%u\n", (unsigned)*length);
    return OW_OK;
}
static void pump(void *context) {
    (void)context;
    /* No peer streams are used; avoid their unrelated keepalive writes. */
    ow_fwgui_pump();
}
struct wg_link_ops wg_onewili_ops(void) {
    return (struct wg_link_ops){.context=&device, .open=open_link, .probe=probe, .pump=pump, .begin=begin};
}
