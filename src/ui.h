#ifndef WG_UI_H
#define WG_UI_H
#include "model.h"
void wg_render(const struct wg_model *m, bool touch_available);
struct wg_ui_state {
    bool initialized, transport_open;
    uint64_t last_status_ms;
    unsigned full_draws, status_draws;
    int link, command_status;
    char last_response[64];
};
void wg_ui_update(struct wg_ui_state *state, const struct wg_model *m,
    bool touch_available, bool full, uint64_t now_ms);
#endif
