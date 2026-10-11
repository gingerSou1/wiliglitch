#include "model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;
static enum wg_result reply;
static enum wg_result mock(void *ctx, bool enable, struct wg_config c) {
    (void)ctx; (void)enable; assert(wg_valid(c)); ++calls; return reply;
}
static void tap(struct wg_model *m, unsigned x, unsigned y) {
    assert(wg_touch(m, true, x, y, 123));
    assert(!wg_touch(m, true, x, y, 124));
    assert(!wg_touch(m, false, x, y, 125));
}
int main(void) {
    struct wg_model m;
    wg_init(&m, (struct wg_adapter){.context = NULL, .request = mock});
    assert(!m.armed && m.page == WG_HOME && m.output == WG_OUTPUT_UNKNOWN && calls == 0);
    for (unsigned i = 0; i < 6; ++i) {
        tap(&m, 20 + (i % 2) * 232, 90 + (i / 2) * 54);
        assert(m.page == (enum wg_page)(WG_VOLTAGE + i)); tap(&m, 30, 270);
        assert(m.page == WG_HOME && !m.armed);
    }
    assert(calls == 0);
    assert(wg_valid((struct wg_config){1000, 10}));
    assert(wg_valid((struct wg_config){5500, 2000}));
    assert(!wg_valid((struct wg_config){999, 10}));
    assert(!wg_valid((struct wg_config){5501, 10}));
    assert(!wg_valid((struct wg_config){3300, 9}));
    assert(!wg_valid((struct wg_config){3300, 2001}));
    assert(!wg_configure(&m, (struct wg_config){3300, -1}));
    assert(m.config.pulse_ns == 50 && m.status == WG_INVALID);
    assert(!wg_arm(&m) && !m.armed && calls == 0);
    for (reply = WG_OK; reply <= WG_FAILED; reply++) {
        wg_output_request(&m, true);
        assert(m.status == reply && m.output == WG_OUTPUT_UNKNOWN && !m.armed);
    }
    unsigned before = calls;
    wg_trigger(&m, 42); wg_trigger(&m, 43);
    assert(calls == before && !m.armed && m.record_count == 2);
    m.profile = (struct wg_config){5500, 2000}; m.profile_saved = true;
    assert(wg_load_profile(&m) && m.config.pulse_ns == 2000 && calls == before);
    m.profile.pulse_ns = 2001; assert(!wg_load_profile(&m));
    m.profile_saved = false; assert(wg_load_profile(&m) && m.config.millivolts == 3300);
    tap(&m, 20, 90); tap(&m, 410, 95); assert(m.config.millivolts == 3400);
    tap(&m, 410, 140); assert(m.config.pulse_ns == 60);
    tap(&m, 330, 185); assert(m.record_count == 3 && calls == before);
    wg_home(&m); assert(m.page == WG_HOME && !m.armed);
    assert(!wg_touch(&m, true, 480, 80, 0)); wg_touch(&m, false, 0, 0, 0);
    for (unsigned i = 0; i < 20; ++i) wg_trigger(&m, i);
    assert(m.record_count == 16 && m.dropped_records == 7);
    struct wg_record r = {42, {3300, 50}, WG_FAILED, "reset,\"quoted\""};
    char csv[256]; int n = wg_record_csv(&r, csv, sizeof(csv));
    assert(n == (int)strlen(csv));
    assert(strcmp(csv, "42,3300,50,FAILED,\"reset,\"\"quoted\"\"\"\r\n") == 0);
    char shortbuf[8]; assert(wg_record_csv(&r, shortbuf, sizeof(shortbuf)) >= 8);
    wg_init(&m, wg_disabled_adapter()); wg_output_request(&m, true);
    assert(m.status == WG_UNAVAILABLE);
    m.adapter.request = NULL; wg_output_request(&m, false); assert(m.status == WG_DISCONNECTED);
    puts("PASS: navigation, touch edges, bounds, disarm, unavailable arm/trigger, failures, disconnection, HOME model, profiles, records");
    return 0;
}
