#include "model.h"
static enum wg_result unavailable(void *context, bool enable, struct wg_config c) {
    (void)context; (void)enable; (void)c;
    return WG_UNAVAILABLE;
}
struct wg_adapter wg_disabled_adapter(void) {
    return (struct wg_adapter){.context = NULL, .request = unavailable};
}
