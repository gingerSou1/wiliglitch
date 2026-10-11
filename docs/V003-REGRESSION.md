# v002 runtime regression investigation — diagnostic v003

Current checkpoint status: [v003 validated baseline](STATUS.md). This report preserves the historical investigation/build findings.

2026-10-11. User-reported physical failure on FREE-WILi 2 the test device / MAIN v07: v002 launches with flicker/instability and no observed confirmed connection. v001 launch/navigation/DISARMED/HOME had passed. No device interaction was performed during this investigation.

## Findings and suspected cause

**High-confidence flicker mechanism:** v002's main loop called its complete renderer whenever `wg_hardware_poll` reported a state transition. Starting a probe returned true with WAITING; receiving a reply or reaching the one-second deadline returned true again. A failed query then retried five seconds after completion. That path can cause two full page clears per roughly six-second failure cycle, even when the user does nothing. The renderer begins with `st7796_fill_screen`, and the official driver writes all 480×320 pixels synchronously before text/buttons are redrawn. Those blank/intermediate frames are a plausible explanation for visible flashing. The user's observation is not sufficient to prove this explains every aspect of instability.

**Separate responsiveness hazard:** OneWili FwGUI initialization configures UART0 and sends a navigation reset. It also arms the standard SD client, which attempts cleanup of its two server handle slots. FwGUI command and SDFS transmission use `uart_write_blocking` with hardware CTS/RTS. The recovery-aware wrapper services HOME during startup/read waits; it does not bound these writes. A stalled CTS can therefore stop the main loop before touch or HOME is processed. Zero-timeout response polling does not protect initialization or command transmission. The bounded one-second response deadline cannot run while a send is stalled. This is a supported-library limitation, not a proven diagnosis of what the test device did.

**No peripheral collision found in the audited mapping:** OneWili uses UART0, GPIO0–3. Keyboard/HOME/power status uses UART1 AUX, GPIO38/39 and DREQ_UART1_RX. LCD output uses SPI1; its DMA IRQ handler is shared and channel-guarded, while OneWili installs UART0_IRQ. Touch uses the board's I2C path. These are separate peripheral resources. No upstream peripheral or driver initialization was changed.

The reason MAIN did not confirm a reply is still unknown. Existing successful PC USB OTP replies do not establish the DISPLAY FwGUI path. Its ring/frame/checksum state and physical CTS behavior were not measured on hardware. v002 renders the successful state as REPLY CONFIRMED, rather than the literal word CONNECTED.

## v001 / v002 / diagnostic v003 comparison

| Path | v001 validated baseline | v002 failed candidate | v003 diagnostic |
| --- | --- | --- | --- |
| Board/display startup | Normal BSP board/recovery/LCD/touch; full first frame before backlight | Same lifecycle, then automatic OneWili open in main loop | Same BSP lifecycle; OneWili open/send/pump intentionally paused |
| Idle rendering | No background full redraw | Probe start/completion triggers full clear/redraw | Rate-limited, changed-only status regions; no connection-driven full clear |
| Retry behavior | No communication | Five-second retry after query completion | Automatic initialization and all retries paused |
| Input | Touch edge handling; Back/CANCEL/HOME | Same handlers, but communication executes ahead of touch | Same handlers; no transport call can stall the loop in passive mode |
| Recovery | Standard five-second HOME watchdog | Same recovery, with a potential blocking-write gap | Standard recovery continuously serviced; no OneWili startup timer/write path entered |

## Correction and diagnostics

v003 separates deliberate full-page rendering (startup, navigation/configuration actions, About restore) from communication status refresh. `wg_ui_update` clears only changed eight-pixel status/reply regions. Refresh batches run at most once per second. Stable status fields are not redrawn; unchanged pages without the diagnostic counter row perform no idle LCD writes.

For safe isolation, v003 pauses the transport before any automatic open/send/pump or retry. The UI reports Link:CLOSED, MAIN:PAUSED and DIAGNOSTIC PAUSE. This is a real disabled state, not a simulated connection. No new connection button or experimental hardware feature was added.

On Home and Voltage Glitch, the existing status/footer area shows bounded counters: full-page draws, partial status batches, main-loop polls and open attempts. Counters saturate instead of wrapping. Only this small counter region refreshes once per second. Expected idle observation: full draws remain constant after startup; polls advance; open attempts remain zero. Navigating or editing settings intentionally increments full draws. All output requests, ARM and glitch operations remain locked.

The passive artifact tests whether the app is stable with the newly added transport initialization/transmit/retry path removed. It does not claim to repair or validate MAIN communication. If stable, a later separately reviewed diagnostic can reintroduce one supported communication stage at a time. If flicker persists while full-draw count is constant, the present redraw hypothesis is insufficient and physical power/loader/LCD evidence is needed. No undocumented CTS/power-zone workaround was introduced.

## Regression tests and build

Five host suites pass: foundation, hardware adapter, pinned official API response parsing, official HOME hold-state logic and renderer. New assertions include:

- 1,000 changing-status updates cause zero extra full-screen clears and at most ten status batches in ten seconds.
- Unchanged status on an idle non-counter page causes no drawing.
- 10,000 paused hardware polls call no open, begin/probe or pump callback; navigation remains callable and disarmed.
- Diagnostic counters saturate without wrap; maximum-length counter text stays within the display.
- Existing bounds, profiles, failures, blocked operations and HOME tests remain passing.

The plausible cause and targeted regression tests were established before building v003. Cross-compilation initially caught an ARM-specific unsigned/uint32_t counter type mismatch; counters now use consistently typed unsigned values and compile cleanly in app-owned code.

Official DISPLAY toolchain build succeeds: ARM GNU 15.2.1 / Pico SDK 2.3.0. The official UF2 checker accepts SRAM-only WiliGlitch v003 metadata. File: **`dist/WiliGlitch-v003.uf2`**, **95,232 bytes**. Payload: **47,596 bytes**, `0x20000000..0x2000b9ec` (exclusive end), within the supported 448 KiB app loader window. Main SRAM allocation: 146,884 bytes plus two 4 KiB physical stack reservations. UF2 payload matches its ELF. Live programmable-power/glitch API function symbols are absent.

SHA-256:

```text
4f1662241106068261d253dd3c37a5d2a2a17dd7addb9d6c371e35f8b7f73b9b
```

[Artifact audit](V003-ARTIFACT-AUDIT.json).

Frozen v001 (including rollback) still hashes to `81a4039c1f310f06d180c768483fb7473d7d82c4fbc21daef52d7ab97cee71ad`; frozen deployed v002 remains `3e6640c72056f6b5ceaaba8953fc3c7de6dbce995f8cbf369573edfbd063ebd4`.

Changed files: CMakeLists.txt; src/main.c, hardware.h/hardware.c, ui.h/ui.c; tests/test_hardware.c/test_render.c; new tools/audit_v003.py, v003 artifact/checksum and this report; README updated to point here. No interface redesign, stock firmware, WiliPirate or upstream changes. No deployment, physical output activation, commit or push. The user subsequently reported successful v003 physical UI validation. The passive isolation baseline is validated; live DISPLAY-to-MAIN communication remains unverified.
