/* Compile-time verification only: no transport or physical-output calls. */
#include "onewili.h"
_Static_assert(_Generic(&ow_io_analog_out_set_v_prog_vout,
    ow_status (*)(ow_device *, int32_t, double): 1, default: 0), "VOut API changed");
_Static_assert(_Generic(&ow_io_analog_out_set_glitch,
    ow_status (*)(ow_device *, int32_t): 1, default: 0), "Glitch API changed");
