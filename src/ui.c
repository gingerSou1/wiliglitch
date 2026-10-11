#include "ui.h"
#include "hardware.h"
#include "display/st7796.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>
static void bounded_count(unsigned *value) { if (*value != UINT_MAX) ++*value; }
#define BE(c) ((uint16_t)(((c) >> 8) | ((c) << 8)))
#define BG BE(0x0841)
#define FG BE(0xffff)
#define TILE BE(0x1949)
#define MUTED BE(0xbdf7)
_Static_assert(ST7796_W == 480 && ST7796_H == 320, "Display dimensions changed");
static void text(int x, int y, const char *s) { st7796_draw_text(x, y, 2, FG, BG, s); }
static void small(int y, const char *s) { st7796_draw_text(16, y, 1, MUTED, BG, s); }
static void button(int x, int y, int w, int h, const char *s) {
    st7796_fill_rect(x, y, w, h, TILE);
    st7796_draw_text(x + 8, y + (h - 16) / 2, 2, FG, TILE, s);
}
void wg_render(const struct wg_model *m, bool touch_available) {
    char line[96];
    st7796_fill_screen(BG);
    text(16, 12, wg_page_name(m->page));
    small(39, "Fault Injection & Resilience Lab");
    text(16, 56, "Output State: DISARMED");
    if (m->hardware) {
        snprintf(line, sizeof(line), "Link:%s | MAIN:%s | %s", m->hardware->transport_open ? "INIT" : "CLOSED",
            wg_link_name(m->hardware->link), wg_command_status_name(m->hardware->command_status));
        small(76, line);
    }
    if (m->page == WG_HOME) {
        for (unsigned i = 0; i < 6; ++i)
            button(16 + (i % 2) * 232, 86 + (i / 2) * 54, 216, 44,
                wg_page_name((enum wg_page)(WG_VOLTAGE + i)));
        small(249, "M2A: requests staged locally | physical outputs locked");
    } else if (m->page == WG_VOLTAGE || m->page == WG_TIMING) {
        button(16, 88, 64, 34, "-"); button(400, 88, 64, 34, "+");
        if (m->page == WG_VOLTAGE) {
            snprintf(line, sizeof(line), "Voltage: %d.%03d V", m->config.millivolts / 1000, m->config.millivolts % 1000);
            text(92, 97, line);
            button(16, 134, 64, 34, "-"); button(400, 134, 64, 34, "+");
            snprintf(line, sizeof(line), "Width: %d ns", m->config.pulse_ns); text(92, 143, line);
            button(16, 181, 138, 34, "OUT N/A"); button(168, 181, 138, 34, "ARM N/A");
            button(320, 181, 144, 34, "SHOT N/A");
            snprintf(line, sizeof(line), "Last reply: %s", m->hardware && m->hardware->last_response[0] ? m->hardware->last_response : "none confirmed");
            small(216, line);
            small(226, "Observed power: UNKNOWN | physical operations LOCKED");
            small(236, "Voltage/width: requested only; nothing applied to hardware");
        } else {
            snprintf(line, sizeof(line), "Width: %d ns", m->config.pulse_ns); text(92, 97, line);
            text(16, 142, "Mode: manual single-shot");
            small(174, "External trigger: unavailable | delay: unavailable");
            small(194, "Requested width is configuration, not measured duration.");
        }
        snprintf(line, sizeof(line), "Status: %s", wg_result_name(m->status)); small(246, line);
    } else if (m->page == WG_CAMPAIGNS) {
        text(16, 110, "Bounded sequences: future"); small(154, "Automatic campaigns unavailable in M1.");
    } else if (m->page == WG_MONITOR) {
        text(16, 110, "Monitoring: future"); small(154, "UART / GPIO / analog acquisition not connected.");
        small(177, "No observations or waveform measurements fabricated.");
    } else if (m->page == WG_PROFILES) {
        text(16, 105, "Session profile");
        button(16, 150, 216, 44, "SAVE RAM"); button(248, 150, 216, 44, "LOAD / DEFAULT");
        small(215, "Defaults: 3.300 V, 50 ns | loading always disarms");
        small(236, "RAM only: profile is lost when the application exits.");
    } else if (m->page == WG_RESULTS) {
        snprintf(line, sizeof(line), "Records: %u (RAM)", m->record_count); text(16, 105, line);
        small(139, "Timestamp: app uptime ms; requests are not measurements.");
        if (m->record_count) {
            const struct wg_record *r = &m->records[m->record_count - 1];
            snprintf(line, sizeof(line), "Last request: %d mV / %d ns / %s", r->requested.millivolts, r->requested.pulse_ns, wg_result_name(r->result));
            small(167, line);
        }
        snprintf(line, sizeof(line), "Capacity: 16 | dropped records: %u", m->dropped_records); small(195, line);
        small(226, "CSV formatter implemented; SD export unavailable.");
    }
    button(16, 260, 448, 34, m->page == WG_HOME ? "HOME / RECOVERY HELP" : "BACK / HOME");
    small(305, touch_available ? "Hold HOME 5s: stock app | Hold PAGE 5s: About" : "Touch unavailable | Hold HOME 5s: stock app");
}

void wg_ui_update(struct wg_ui_state *s, const struct wg_model *m,
    bool touch_available, bool full, uint64_t now) {
    if (full || !s->initialized) {
        bounded_count(&s->full_draws);
        wg_render(m, touch_available);
        if (m->hardware) {
            s->link = m->hardware->link; s->command_status = m->hardware->command_status;
            s->transport_open = m->hardware->transport_open;
            memcpy(s->last_response, m->hardware->last_response, sizeof(s->last_response));
        }
        s->initialized = true; s->last_status_ms = now;
        return;
    }
    if (!m->hardware || now - s->last_status_ms < 1000) return;
    s->last_status_ms = now;
    bool changed = s->link != (int)m->hardware->link || s->command_status != m->hardware->command_status ||
        s->transport_open != m->hardware->transport_open;
    bool response_changed = strcmp(s->last_response, m->hardware->last_response) != 0;
    bool counters_visible = m->page == WG_HOME || m->page == WG_VOLTAGE;
    if (!changed && !response_changed && !counters_visible) return;
    bounded_count(&s->status_draws);
    char line[96];
    if (changed) {
      st7796_fill_rect(16, 76, 448, 8, BG);
    snprintf(line, sizeof(line), "Link:%s | MAIN:%s | %s", m->hardware->transport_open ? "INIT" : "CLOSED",
        wg_link_name(m->hardware->link), wg_command_status_name(m->hardware->command_status));
    small(76, line);
      s->link = m->hardware->link; s->command_status = m->hardware->command_status;
      s->transport_open = m->hardware->transport_open;
    }
    if (m->page == WG_HOME || m->page == WG_VOLTAGE) {
        int y = m->page == WG_HOME ? 249 : 246;
        st7796_fill_rect(16, y, 448, 8, BG);
        snprintf(line, sizeof(line), "Diag full:%u status:%u polls:%u opens:%u", s->full_draws,
            s->status_draws, m->hardware->poll_count, m->hardware->open_attempts);
        small(y, line);
    }
    if (m->page == WG_VOLTAGE && response_changed) {
        st7796_fill_rect(16, 216, 448, 8, BG);
        snprintf(line, sizeof(line), "Last reply: %s", m->hardware->last_response[0] ? m->hardware->last_response : "none confirmed");
        small(216, line);
    }
    memcpy(s->last_response, m->hardware->last_response, sizeof(s->last_response));
}
