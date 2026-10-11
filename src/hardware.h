#ifndef WG_HARDWARE_H
#define WG_HARDWARE_H
#include "model.h"
/* Numeric values preserve the official ow_status for reporting. */
enum wg_link { WG_LINK_NOT_PROBED, WG_LINK_CONFIRMED, WG_LINK_NO_REPLY, WG_LINK_ERROR, WG_LINK_PAUSED };
struct wg_link_ops {
    void *context;
    int (*open)(void *);
    int (*probe)(void *, uint8_t *, size_t, size_t *);
    void (*pump)(void *);
    int (*begin)(void *);
};
struct wg_hardware {
    struct wg_link_ops ops;
    struct wg_config requested;
    enum wg_link link;
    enum wg_output observed_output;
    bool transport_open, attempted, pending;
    uint64_t last_attempt_ms, last_reply_ms;
    int command_status;
    uint8_t identity[16];
    size_t identity_length;
    unsigned replies, failures;
    char last_response[64];
    bool diagnostic_paused;
    unsigned poll_count, open_attempts;
};
void wg_hardware_init(struct wg_hardware *hw, struct wg_link_ops ops);
void wg_hardware_pause_for_diagnostics(struct wg_hardware *hw);
bool wg_hardware_poll(struct wg_hardware *hw, uint64_t now_ms);
enum wg_result wg_hardware_stage(struct wg_hardware *hw, struct wg_config config);
enum wg_result wg_hardware_set_output(struct wg_hardware *hw, bool enable, struct wg_config config);
enum wg_result wg_hardware_glitch(struct wg_hardware *hw, int pulse_ns);
struct wg_adapter wg_hardware_adapter(struct wg_hardware *hw);
const char *wg_link_name(enum wg_link link);
const char *wg_command_status_name(int status);
struct wg_link_ops wg_onewili_ops(void);
#endif
