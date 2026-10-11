#ifndef WG_MODEL_H
#define WG_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum wg_page { WG_HOME, WG_VOLTAGE, WG_TIMING, WG_CAMPAIGNS, WG_MONITOR, WG_PROFILES, WG_RESULTS };
enum wg_result { WG_OK, WG_UNAVAILABLE, WG_DISCONNECTED, WG_FAILED, WG_INVALID };
enum wg_output { WG_OUTPUT_UNKNOWN, WG_OUTPUT_OFF, WG_OUTPUT_ON };
struct wg_config { int millivolts; int pulse_ns; };
struct wg_record { uint64_t uptime_ms; struct wg_config requested; enum wg_result result; char observation[64]; };
struct wg_adapter {
    void *context;
    enum wg_result (*request)(void *, bool, struct wg_config);
    enum wg_result (*stage)(void *, struct wg_config);
};
struct wg_hardware;
struct wg_model {
    enum wg_page page;
    struct wg_config config, profile;
    bool profile_saved, armed, touch_down;
    enum wg_output output;
    enum wg_result status;
    struct wg_adapter adapter;
    const struct wg_hardware *hardware;
    struct wg_record records[16];
    unsigned record_count, dropped_records;
};
const char *wg_page_name(enum wg_page page);
const char *wg_result_name(enum wg_result result);
struct wg_adapter wg_disabled_adapter(void);
void wg_init(struct wg_model *m, struct wg_adapter adapter);
bool wg_valid(struct wg_config c);
bool wg_configure(struct wg_model *m, struct wg_config c);
void wg_home(struct wg_model *m);
bool wg_arm(struct wg_model *m);
void wg_disarm(struct wg_model *m);
void wg_output_request(struct wg_model *m, bool enable);
void wg_trigger(struct wg_model *m, uint64_t uptime_ms);
bool wg_load_profile(struct wg_model *m);
bool wg_touch(struct wg_model *m, bool down, unsigned x, unsigned y, uint64_t uptime_ms);
int wg_record_csv(const struct wg_record *r, char *out, size_t capacity);
#endif
