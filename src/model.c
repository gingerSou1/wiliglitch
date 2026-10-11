#include "model.h"
#include <stdio.h>
#include <string.h>
const char *wg_page_name(enum wg_page p) {
    static const char *names[] = {"WiliGlitch", "Voltage Glitch", "Timing & Trigger", "Campaigns", "Target Monitor", "Profiles", "Results"};
    return (unsigned)p <= WG_RESULTS ? names[p] : "Unknown";
}
const char *wg_result_name(enum wg_result r) {
    static const char *names[] = {"OK", "UNAVAILABLE", "DISCONNECTED", "FAILED", "INVALID"};
    return (unsigned)r <= WG_INVALID ? names[r] : "UNKNOWN";
}
void wg_init(struct wg_model *m, struct wg_adapter adapter) {
    memset(m, 0, sizeof(*m));
    m->config = (struct wg_config){3300, 50};
    m->profile = m->config;
    m->adapter = adapter;
    m->output = WG_OUTPUT_UNKNOWN;
    m->status = WG_UNAVAILABLE;
    if (adapter.stage) m->status = adapter.stage(adapter.context, m->config);
}
bool wg_valid(struct wg_config c) {
    return c.millivolts >= 1000 && c.millivolts <= 5500 && c.pulse_ns >= 10 && c.pulse_ns <= 2000;
}
void wg_disarm(struct wg_model *m) { m->armed = false; }
void wg_home(struct wg_model *m) { wg_disarm(m); m->page = WG_HOME; }
bool wg_configure(struct wg_model *m, struct wg_config c) {
    wg_disarm(m);
    if (!wg_valid(c)) { m->status = WG_INVALID; return false; }
    m->status = m->adapter.stage ? m->adapter.stage(m->adapter.context, c) : WG_OK;
    if (m->status != WG_OK) return false;
    m->config = c; return true;
}
bool wg_arm(struct wg_model *m) {
    /* This distribution has no physical injection capability. */
    wg_disarm(m); m->status = WG_UNAVAILABLE; return false;
}
void wg_output_request(struct wg_model *m, bool enable) {
    wg_disarm(m);
    m->status = m->adapter.request ? m->adapter.request(m->adapter.context, enable, m->config) : WG_DISCONNECTED;
    /* Command acknowledgement cannot establish observed rail state. */
    m->output = WG_OUTPUT_UNKNOWN;
}
void wg_trigger(struct wg_model *m, uint64_t ms) {
    wg_disarm(m); m->status = WG_UNAVAILABLE;
    if (m->record_count == 16) { ++m->dropped_records; return; }
    struct wg_record *r = &m->records[m->record_count++];
    *r = (struct wg_record){ms, m->config, WG_UNAVAILABLE, ""};
}
bool wg_load_profile(struct wg_model *m) {
    wg_disarm(m);
    return wg_configure(m, m->profile_saved ? m->profile : (struct wg_config){3300, 50});
}
bool wg_touch(struct wg_model *m, bool down, unsigned x, unsigned y, uint64_t ms) {
    bool edge = down && !m->touch_down;
    m->touch_down = down;
    if (!edge || x >= 480 || y >= 320) return false;
    if (y >= 260 && y < 294 && x >= 16 && x < 464) { wg_home(m); return true; }
    if (m->page == WG_HOME) {
        for (unsigned i = 0; i < 6; ++i) {
            unsigned tx = 16 + (i % 2) * 232, ty = 86 + (i / 2) * 54;
            if (x >= tx && x < tx + 216 && y >= ty && y < ty + 44) {
                wg_disarm(m); m->page = (enum wg_page)(WG_VOLTAGE + i); return true;
            }
        }
    } else if (m->page == WG_VOLTAGE || m->page == WG_TIMING) {
        struct wg_config c = m->config;
        if (y >= 88 && y < 122 && (x < 80 || x >= 400)) {
            if (m->page == WG_VOLTAGE) c.millivolts += x < 80 ? -100 : 100;
            else c.pulse_ns += x < 80 ? -10 : 10;
            wg_configure(m, c); return true;
        }
        if (m->page == WG_VOLTAGE && y >= 134 && y < 168 && (x < 80 || x >= 400)) {
            c.pulse_ns += x < 80 ? -10 : 10; wg_configure(m, c); return true;
        }
        if (m->page == WG_VOLTAGE && y >= 181 && y < 215) {
            if (x < 160) wg_output_request(m, false);
            else if (x < 320) wg_arm(m);
            else wg_trigger(m, ms);
            return true;
        }
    } else if (m->page == WG_PROFILES && y >= 150 && y < 194) {
        if (x < 240) { m->profile = m->config; m->profile_saved = true; wg_disarm(m); }
        else wg_load_profile(m);
        return true;
    }
    return false;
}
int wg_record_csv(const struct wg_record *r, char *out, size_t cap) {
    /* Quote observations and escape embedded quotes (RFC 4180). */
    char escaped[129]; size_t n = 0;
    for (size_t i = 0; i < sizeof(r->observation) && r->observation[i]; ++i) {
        if (r->observation[i] == '"') escaped[n++] = '"';
        escaped[n++] = r->observation[i];
    }
    escaped[n] = 0;
    return snprintf(out, cap, "%llu,%d,%d,%s,\"%s\"\r\n", (unsigned long long)r->uptime_ms,
        r->requested.millivolts, r->requested.pulse_ns, wg_result_name(r->result), escaped);
}
