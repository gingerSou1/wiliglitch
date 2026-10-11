#include "hardware.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
enum { API_OK = 0, API_ARG = 1, API_IO = 2, API_TIMEOUT = 3, API_FAILED = 4, API_PROTOCOL = 5 };
static void count(unsigned *value) { if (*value != UINT_MAX) ++*value; }
void wg_hardware_pause_for_diagnostics(struct wg_hardware *h) {
    h->diagnostic_paused = true; h->pending = false;
    h->link = WG_LINK_PAUSED; h->command_status = -3;
}
void wg_hardware_init(struct wg_hardware *h, struct wg_link_ops ops) {
    memset(h, 0, sizeof(*h)); h->ops = ops;
    h->requested = (struct wg_config){3300, 50};
    h->observed_output = WG_OUTPUT_UNKNOWN; h->command_status = -1;
}
const char *wg_link_name(enum wg_link s) {
    static const char *names[] = {"NOT PROBED", "REPLY CONFIRMED", "NO REPLY", "ERROR", "PAUSED"};
    return (unsigned)s < 5 ? names[s] : "UNKNOWN";
}
const char *wg_command_status_name(int s) {
    if (s == -2) return "WAITING";
    if (s == -3) return "DIAGNOSTIC PAUSE";
    static const char *names[] = {"OW_OK", "OW_ERR_ARG", "OW_ERR_IO", "OW_ERR_TIMEOUT", "OW_ERR_FAILED", "OW_ERR_PROTOCOL", "OW_ERR_BUFFER"};
    return (unsigned)s < 7 ? names[s] : "NOT SENT";
}
bool wg_hardware_poll(struct wg_hardware *h, uint64_t now) {
    count(&h->poll_count);
    if (h->diagnostic_paused) return false;
    if (h->transport_open && h->ops.pump) h->ops.pump(h->ops.context);
    if (!h->pending) {
      if (h->attempted && now - h->last_attempt_ms < 5000) return false;
      h->attempted = true; h->last_attempt_ms = now;
    if (!h->transport_open) {
        count(&h->open_attempts);
        h->command_status = h->ops.open ? h->ops.open(h->ops.context) : API_IO;
        h->transport_open = h->command_status == API_OK;
    }
    h->identity_length = 0;
      if (h->transport_open && h->ops.begin) {
        h->command_status = h->ops.begin(h->ops.context);
        if (h->command_status == API_OK) {
            h->pending = true; h->command_status = -2; return true;
        }
      } else if (h->transport_open) h->pending = true;
    }
    if (h->pending) {
        h->command_status = h->ops.probe ? h->ops.probe(h->ops.context, h->identity,
            sizeof(h->identity), &h->identity_length) : API_IO;
        if (h->command_status == -2) {
            if (now - h->last_attempt_ms < 1000) return false;
            h->command_status = API_TIMEOUT;
        }
        if (h->command_status == API_OK && h->identity_length != sizeof(h->identity))
            h->command_status = API_PROTOCOL;
    }
    h->pending = false; h->last_attempt_ms = now;
    if (h->command_status == API_OK) {
        h->link = WG_LINK_CONFIRMED; h->last_reply_ms = now; count(&h->replies);
        size_t pos = 0;
        for (size_t i = 0; i < h->identity_length; ++i)
            pos += (size_t)snprintf(h->last_response + pos, sizeof(h->last_response) - pos,
                i ? " %02X" : "%02X", h->identity[i]);
    } else {
        /* A framed command rejection is itself a confirmed MAIN reply. */
        if (h->command_status == API_FAILED) {
            h->link = WG_LINK_CONFIRMED; h->last_reply_ms = now; count(&h->replies);
            snprintf(h->last_response, sizeof(h->last_response), "MAIN rejected OTP query");
        } else {
            h->link = h->command_status == API_IO || h->command_status == API_TIMEOUT
                ? WG_LINK_NO_REPLY : WG_LINK_ERROR;
        }
        h->identity_length = 0; memset(h->identity, 0, sizeof(h->identity)); count(&h->failures);
    }
    /* A reply confirms MAIN communication only, never programmable rail state. */
    h->observed_output = WG_OUTPUT_UNKNOWN;
    return true;
}
enum wg_result wg_hardware_stage(struct wg_hardware *h, struct wg_config c) {
    if (!wg_valid(c)) return WG_INVALID;
    h->requested = c; return WG_OK; /* no voltage/pulse-setting command is sent */
}
enum wg_result wg_hardware_set_output(struct wg_hardware *h, bool enable, struct wg_config c) {
    (void)h; (void)enable;
    return wg_valid(c) ? WG_UNAVAILABLE : WG_INVALID;
}
enum wg_result wg_hardware_glitch(struct wg_hardware *h, int ns) {
    (void)h;
    return ns >= 10 && ns <= 2000 ? WG_UNAVAILABLE : WG_INVALID;
}
static enum wg_result request(void *h, bool enable, struct wg_config c) {
    return wg_hardware_set_output(h, enable, c);
}
static enum wg_result stage(void *h, struct wg_config c) { return wg_hardware_stage(h, c); }
struct wg_adapter wg_hardware_adapter(struct wg_hardware *h) {
    return (struct wg_adapter){.context = h, .request = request, .stage = stage};
}
