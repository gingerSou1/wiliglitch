#include "fw2.h"
#include "pico/time.h"
#include "ui.h"
#include "hardware.h"
static struct wg_model model;
static struct wg_hardware hardware;
static bool touch_available;
static struct wg_ui_state ui_state;
static void redraw(void) {
    wg_ui_update(&ui_state, &model, touch_available, true, to_ms_since_boot(get_absolute_time()));
}
int main(void) {
    wg_hardware_init(&hardware, wg_onewili_ops());
    /* Passive isolation build: no open/send/retry can block navigation/HOME. */
    wg_hardware_pause_for_diagnostics(&hardware);
    wg_init(&model, wg_hardware_adapter(&hardware));
    model.hardware = &hardware;
    board_init();
    fw2_app_recovery_init();
    st7796_init();
    touch_available = ft6336_init();
    fw2_app_about_use_lcd_restore(redraw);
    redraw();
    board_backlight_set(1);
    picpwr_release_unused();
    for (;;) {
        fw2_app_recovery_task();
        uint64_t now = to_ms_since_boot(get_absolute_time());
        (void)wg_hardware_poll(&hardware, now);
        wg_ui_update(&ui_state, &model, touch_available, false, now);
        uartkbd_event_t event;
        while (uartkbd_next_event(&event)) {
            if (event.pressed && (event.btn == UARTKBD_BTN_CANCEL || event.btn == UARTKBD_BTN_HOME)) {
                wg_home(&model); redraw();
            }
        }
        uint16_t x = 0, y = 0;
        bool down = touch_available && ft6336_poll(&x, &y);
        if (wg_touch(&model, down, x, y, to_ms_since_boot(get_absolute_time()))) redraw();
        fw2_app_recovery_sleep_ms(10);
    }
}
