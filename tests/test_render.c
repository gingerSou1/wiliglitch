#include "ui.h"
#include "hardware.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned clears, buttons;
static bool disarmed;
static bool partial_only;
void st7796_fill_screen(uint16_t c) { (void)c; ++clears; }
void st7796_fill_rect(int x, int y, int w, int h, uint16_t c) {
    if (partial_only) assert(h == 8 && w == 448);
    (void)c; assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= 480 && y + h <= 320); ++buttons;
}
void st7796_draw_text(int x, int y, int scale, uint16_t fg, uint16_t bg, const char *s) {
    (void)fg; (void)bg;
    assert(x >= 0 && y >= 0 && x + (int)strlen(s) * 6 * scale <= 480 && y + 7 * scale <= 320);
    if (strcmp(s, "Output State: DISARMED") == 0) disarmed = true;
}
int main(void) {
    struct wg_model m; wg_init(&m, wg_disabled_adapter());
    for (unsigned p = WG_HOME; p <= WG_RESULTS; ++p) {
        m.page = (enum wg_page)p; disarmed = false;
        wg_render(&m, true); assert(disarmed);
        wg_render(&m, false);
    }
    assert(clears == 14 && buttons >= 14);
    wg_trigger(&m, 0); wg_render(&m, true);
    struct wg_hardware hardware;
    wg_hardware_init(&hardware, (struct wg_link_ops){0}); m.hardware = &hardware;
    for (unsigned state = WG_LINK_NOT_PROBED; state <= WG_LINK_ERROR; ++state) {
        hardware.link = (enum wg_link)state; hardware.command_status = (int)state;
        for (unsigned p = WG_HOME; p <= WG_RESULTS; ++p) {
            m.page = (enum wg_page)p; wg_render(&m, true);
        }
    }
    m.page = WG_VOLTAGE;
    struct wg_ui_state ui = {0};
    unsigned original_clears = clears;
    wg_ui_update(&ui, &m, true, false, 0);
    assert(clears == original_clears + 1 && ui.full_draws == 1);
    partial_only = true;
    /* Response/status storms must never produce a full-screen clear. */
    for (unsigned ms = 10; ms <= 10000; ms += 10) {
        hardware.command_status = ms % 7;
        hardware.link = ms % 4;
        wg_ui_update(&ui, &m, true, false, ms);
    }
    assert(clears == original_clears + 1 && ui.full_draws == 1 && ui.status_draws == 10);
    partial_only = false;
    m.page = WG_HOME; wg_ui_update(&ui, &m, true, true, 10010);
    assert(clears == original_clears + 2 && ui.full_draws == 2);
    ui.full_draws = UINT32_MAX; ui.status_draws = UINT32_MAX;
    hardware.poll_count = UINT32_MAX; hardware.open_attempts = UINT32_MAX;
    wg_ui_update(&ui, &m, true, true, 11010);
    partial_only = true; wg_ui_update(&ui, &m, true, false, 12010);
    assert(ui.full_draws == UINT32_MAX && ui.status_draws == UINT32_MAX);
    partial_only = false;
    m.page = WG_PROFILES; ui.status_draws = 0;
    wg_ui_update(&ui, &m, true, true, 13010);
    unsigned idle_clears = clears, idle_rects = buttons;
    partial_only = true;
    wg_ui_update(&ui, &m, true, false, 14010);
    wg_ui_update(&ui, &m, true, false, 15010);
    assert(clears == idle_clears && buttons == idle_rects && ui.status_draws == 0);
    partial_only = false;
    puts("PASS: all screens, full clears, display bounds, output banner, touch failure, result rendering");
    return 0;
}
