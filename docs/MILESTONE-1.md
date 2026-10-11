# Milestone 1 report — 2026-10-11

Current checkpoint status: [v003 validated baseline](STATUS.md). This report preserves the historical investigation/build findings.

**Status: partial delivery; original M1 completion criteria are not met.**

Implemented:

- Official 480×320 DISPLAY SRAM application and UF2 app metadata.
- Six touchscreen sections, Back, CANCEL/HOME menu recovery, official five-second HOME exit and PAGE About lifecycle.
- DISARMED startup; validated configuration edits shared between Voltage Glitch and Timing & Trigger.
- Defaults, session profile save/load, validation before applying profiles, disarm on edits/loading/navigation.
- Separate adapter with mockable request outcomes; actual output state remains UNKNOWN even after a mock acknowledgement.
- Bounded 16-record RAM log, overflow counter, app-uptime timestamp, requested parameters, command result and optional observation; CSV quoting/formatting and truncation detection through snprintf return value.
- Compile-time verification against the pinned official API signatures.

Unavailable:

- Physical output changes, ARM and manual injection. These were deliberately omitted from the safety-limited implementation, not blocked by an API build error. Their buttons explicitly show N/A. Repeated taps can log requests but never send physical commands.
- Automatic campaigns, external trigger modes/delay, live UART/GPIO/analog monitoring, persistent profiles, SD export and wall-clock synchronization.
- Runtime OneWili transport integration: there is no connection handshake or live command flow in this build.

Verification:

- Host Clang 19.1.7 through Zig 0.14.1: 2/2 CTest tests pass. Foundation assertions cover navigation, touch-edge suppression, ranges, unavailable arm/trigger, mock acknowledgements/errors/disconnection, profile loads, HOME model recovery and CSV escaping/overflow.
- Rendering assertions exercise all seven screens with and without touch, complete surface clear, text/button bounds, persistent DISARMED banner and results rendering.
- ARM GNU 15.2.1 / Pico SDK 2.3.0: device build succeeds. Official post-build UF2 checker identifies SRAM target and WiliGlitch v001 metadata. An upstream Pico-PIO-USB attribute warning remains; app-owned files compile with warnings treated as errors.
- No hardware was contacted or deployed. Physical touch calibration, HOME watchdog exit and display behavior remain unverified on hardware. Host checks establish layout bounds, not visual appearance.
- Existing dependency sources were used read-only. WiliPirate and upstream trees were not modified. No commits or pushes.

Changed files: root CMakeLists.txt/.gitignore/README.md; src/model.h, model.c, adapter.c, api_contract.c, main.c, ui.h, ui.c; tests/test_model.c, test_render.c; docs/API.md, MILESTONE-1.md; dist/WiliGlitch-v001.uf2 and SHA256.txt.

Artifact: `dist/WiliGlitch-v001.uf2`, configuration preview ready for review. Build intermediates and test executables are under ignored `build/`.
