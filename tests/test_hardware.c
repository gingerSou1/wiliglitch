#include "hardware.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int open_status, probe_status;
static size_t returned_length = 16;
static unsigned opens, probes, pumps;
static int open_link(void *ctx) { (void)ctx; ++opens; return open_status; }
static int probe(void *ctx, uint8_t *data, size_t cap, size_t *length) {
    (void)ctx; assert(cap == 16); ++probes;
    memset(data, 0x5a, cap); *length = returned_length; return probe_status;
}
static void pump(void *ctx) { (void)ctx; ++pumps; }
static int begin(void *ctx) { (void)ctx; return 0; }
int main(void) {
    struct wg_hardware h;
    wg_hardware_init(&h, (struct wg_link_ops){.open=open_link, .probe=probe, .pump=pump});
    struct wg_model m; wg_init(&m, wg_hardware_adapter(&h)); m.hardware = &h;
    assert(h.link == WG_LINK_NOT_PROBED && !h.transport_open && h.command_status == -1);
    assert(opens == 0 && probes == 0 && !m.armed && h.observed_output == WG_OUTPUT_UNKNOWN);
    assert(wg_configure(&m, (struct wg_config){5500, 2000}));
    assert(h.requested.millivolts == 5500 && h.requested.pulse_ns == 2000 && probes == 0);
    assert(!wg_configure(&m, (struct wg_config){5501, 2000}));
    assert(h.requested.millivolts == 5500);
    assert(wg_hardware_poll(&h, 0) && h.link == WG_LINK_CONFIRMED && h.replies == 1);
    assert(h.identity_length == 16 && h.last_reply_ms == 0 && opens == 1 && probes == 1);
    assert(!wg_hardware_poll(&h, 4999) && probes == 1 && pumps == 1);
    assert(!wg_arm(&m)); wg_output_request(&m, true); wg_trigger(&m, 20);
    assert(m.status == WG_UNAVAILABLE && !m.armed && probes == 1);
    assert(wg_hardware_set_output(&h, false, m.config) == WG_UNAVAILABLE);
    assert(wg_hardware_glitch(&h, 10) == WG_UNAVAILABLE);
    assert(wg_hardware_glitch(&h, 2000) == WG_UNAVAILABLE);
    assert(wg_hardware_glitch(&h, 2001) == WG_INVALID);
    assert(wg_hardware_glitch(&h, -1) == WG_INVALID);
    assert(wg_hardware_set_output(&h, true, (struct wg_config){0, 10}) == WG_INVALID);
    assert(opens == 1 && probes == 1);
    probe_status = 3; assert(wg_hardware_poll(&h, 5000));
    assert(h.link == WG_LINK_NO_REPLY && h.command_status == 3 && h.identity_length == 0 && h.identity[0] == 0);
    assert(h.observed_output == WG_OUTPUT_UNKNOWN && h.failures == 1);
    assert(!wg_hardware_poll(&h, 9999));
    probe_status = 4; assert(wg_hardware_poll(&h, 10000) && h.link == WG_LINK_CONFIRMED);
    assert(h.command_status == 4 && h.identity_length == 0 && h.replies == 2);
    probe_status = 0; returned_length = 0;
    assert(wg_hardware_poll(&h, 15000) && h.command_status == 5 && h.link == WG_LINK_ERROR);
    returned_length = 16; assert(wg_hardware_poll(&h, 20000) && h.link == WG_LINK_CONFIRMED);
    assert(h.replies == 3 && h.last_reply_ms == 20000);
    m.profile = (struct wg_config){1000, 10}; m.profile_saved = true;
    unsigned n = probes; assert(wg_load_profile(&m));
    assert(h.requested.millivolts == 1000 && h.requested.pulse_ns == 10 && probes == n);
    wg_home(&m); assert(m.page == WG_HOME && !m.armed);
    open_status = 2; wg_hardware_init(&h, (struct wg_link_ops){.open=open_link, .probe=probe, .pump=pump});
    assert(wg_hardware_poll(&h, 0) && !h.transport_open && h.link == WG_LINK_NO_REPLY && probes == n);
    open_status = 0; assert(wg_hardware_poll(&h, 5000) && h.link == WG_LINK_CONFIRMED);
    wg_hardware_init(&h, (struct wg_link_ops){0});
    assert(wg_hardware_poll(&h, 0) && h.command_status == 2);
    assert(strcmp(wg_command_status_name(3), "OW_ERR_TIMEOUT") == 0);
    wg_hardware_init(&h, (struct wg_link_ops){.open=open_link, .probe=probe, .pump=pump, .begin=begin});
    probe_status = -2;
    assert(wg_hardware_poll(&h, 0) && h.pending && h.command_status == -2);
    for (unsigned ms = 10; ms < 1000; ms += 10) {
        assert(!wg_hardware_poll(&h, ms) && h.pending);
        /* UI processing remains possible while replies are pending. */
        wg_home(&m); assert(!m.armed && m.page == WG_HOME);
    }
    assert(wg_hardware_poll(&h, 1000) && !h.pending && h.command_status == 3);
    assert(!wg_hardware_poll(&h, 5999));
    assert(wg_hardware_poll(&h, 6000) && h.pending);
    probe_status = 0;
    assert(wg_hardware_poll(&h, 6010) && !h.pending && h.link == WG_LINK_CONFIRMED);
    assert(h.last_response[0] && h.last_reply_ms == 6010);
    char confirmed[64]; memcpy(confirmed, h.last_response, sizeof(confirmed));
    assert(wg_hardware_poll(&h, 11010) && h.pending);
    probe_status = 3; assert(wg_hardware_poll(&h, 11020));
    assert(strcmp(confirmed, h.last_response) == 0 && h.last_reply_ms == 6010);
    wg_hardware_init(&h, (struct wg_link_ops){.open=open_link, .probe=probe, .pump=pump, .begin=begin});
    wg_hardware_pause_for_diagnostics(&h);
    unsigned old_opens = opens, old_probes = probes, old_pumps = pumps;
    for (unsigned i = 0; i < 10000; ++i) {
        assert(!wg_hardware_poll(&h, i * 1000ULL));
        wg_home(&m); assert(!m.armed && m.page == WG_HOME);
    }
    assert(opens == old_opens && probes == old_probes && pumps == old_pumps);
    assert(h.poll_count == 10000 && h.open_attempts == 0 && h.link == WG_LINK_PAUSED && !h.pending);
    h.poll_count = UINT32_MAX; wg_hardware_poll(&h, 10000001); assert(h.poll_count == UINT32_MAX);
    puts("PASS: staging, real-probe lifecycle, command errors, reconnect, output locks, profiles, HOME");
    return 0;
}
