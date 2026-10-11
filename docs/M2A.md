# M2A — OneWili hardware integration, v002

Current checkpoint status: [v003 validated baseline](STATUS.md). This report preserves the historical investigation/build findings.

Built for review on 2026-10-11. **Not deployed.** v001 physical UI validation passed on the test device according to the user's report: launch, all six sections/Back, DISARMED banner and five-second HOME recovery. Both frozen v001 UF2 copies retain their original checksum.

## Implemented

The DISPLAY app links the pinned official `onewili_fwgui` library. Its dedicated device adapter opens MAIN communication with the BSP's `fw2_app_recovery_open_onewili` wrapper after normal board/recovery/display initialization. It pumps the FwGUI receive link from the main loop and uses the official split-phase `ow_raw_send` / `ow_raw_next_response` APIs for the documented read-only OTP identity query. Responses are polled with timeout zero while touch/navigation continue; a one-second application deadline ends an unanswered query. Opening a UART does not count as confirmed communication. No peer-stream keepalive or peer datagram is used.

`src/hardware.c` owns link state, pending-query state, command status, reply/failure counts, confirmed-reply time, the last confirmed response, requested configuration and UNKNOWN observed output state. A valid successful query confirms communication; a framed command rejection also confirms a reply while preserving the command error. Timeout/I/O errors report NO REPLY. Malformed/short identity responses report ERROR and clear stale decoded identity data while retaining the last confirmed response for inspection. Retry spacing is five seconds from completion. Voltage Glitch shows initialized/closed link state separately from MAIN confirmation, WAITING or API error, and the last confirmed reply payload.

The existing configuration controls stage validated voltage and width in the hardware adapter. Defaults and profile loading use the same adapter path. **Nothing is applied to programmable power when staging.** Connection/command indicators appear above the existing section contents. Voltage Glitch explicitly distinguishes requested settings from observed power state. MAIN acknowledgement never implies output enabled or voltage measured.

Dedicated programmable-power and pulse operation entry points validate inputs and return UNAVAILABLE for all valid requests. Both enable and disable requests are locked; ARM and injection remain unavailable. There is no runtime switch or build option to unlock physical output operations in v002. Their documented official API signatures remain compile-checked, but actual output-command invocation is intentionally absent from this M2A build. The final ELF has neither live programmable-power nor glitch API function symbols.

The official generated library is used unchanged. Supported buffer tunables are applied consistently to library/consumers: command 128 bytes, response 512 bytes, text-event queue four entries and read timeout 1000 ms. The app retains its 4 KiB stack reservation; runtime probing no longer uses nested synchronous generated OTP calls. No analog power zone is declared; app metadata still requests DISPLAY only.

Verified pinned signatures (also compile-checked in src/api_contract.c):

```c
ow_status ow_io_analog_out_set_v_prog_vout(ow_device *dev, int32_t enable, double set_voltage);
ow_status ow_io_analog_out_set_glitch(ow_device *dev, int32_t nano_seconds);
```

The documented voltage range is 1.0–5.5 V and requested width range is 10–2000 ns; both operations require analog power zone 11. Acceptance status is not a measured waveform or output-state readback. Neither operation is called in M2A.

## Actual MAIN responses

Historical read-only OTP queries returned three successful matching-path replies through the PC USB serial interface. Device identity payloads, ports and raw traces are intentionally retained only in private local logs.

**This proves MAIN's read-only command/API path over USB, not v002's DISPLAY-to-MAIN FwGUI path.** Physical FwGUI validation requires installing and launching v002, which is explicitly deferred until deployment approval. No programmable-power, ARM, injection, SD handoff or application-launch command was sent during M2A development.

## Verification and artifact

- Host tests: **5/5 passed** — foundation, hardware adapter, pinned generated C API transport/parsing, official HOME hold-state logic and renderer bounds/status variants.
- Tests cover staging without calls, validation, connection opening versus reply confirmation, timeouts, command rejections, malformed replies, recovery after failures, blocked operations, profile loading and navigation.
- Device build: ARM GNU 15.2.1 / Pico SDK 2.3.0 succeeds. Official UF2 validator accepts WiliGlitch v002 SRAM metadata. The existing upstream Pico-PIO-USB attribute warning is unchanged.
- Artifact: **`dist/WiliGlitch-v002.uf2`**, 92,672 bytes; payload 46,204 bytes at `0x20000000..0x2000b47c` (exclusive end), within the supported 448 KiB loader window. ELF main RAM reservation: 145,748 bytes; two 4 KiB physical SRAM stack reservations. Payload bytes match the ELF exactly. The earlier undeployed v002 review candidate is archived under dist/review-history; it is superseded by this artifact.
- SHA-256: **`3e6640c72056f6b5ceaaba8953fc3c7de6dbce995f8cbf369573edfbd063ebd4`**. See [artifact audit](M2A-ARTIFACT-AUDIT.json).
- v001 and its rollback copy remain unchanged: `81a4039c1f310f06d180c768483fb7473d7d82c4fbc21daef52d7ab97cee71ad`.

## Remaining validation

The candidate is ready for build/API review and a separately approved normal-link deployment test, but not full M2A acceptance. Actual FwGUI replies require deploying/launching v002, which has not been authorized.

**Concrete upstream limitation:** FwGUI command transmit uses `uart_write_blocking` with hardware flow control, including initialization. If MAIN stalls CTS, a send can block touchscreen processing and delay HOME servicing. The official recovery wrapper slices reads and services startup recovery but does not bound transport writes. Split-phase response polling fixes the ordinary response-wait pause; it cannot guarantee nonblocking transmit or recovery during a stalled write. No library patch, undocumented flow-control change or driver workaround was introduced. Strict nonblocking disconnect behavior needs a supported upstream transmit/recovery mechanism or further documented evidence before it can be claimed.

The official raw response hook returns the parsed response body/success flag rather than the echoed path. v002 uses one outstanding read-only query and does not interleave other commands; the host probe separately verifies actual echoed paths using the official Python API. Physical reconnect, navigation under communication failures and HOME recovery still require device validation.

No stock firmware, WiliPirate or upstream source was modified. The Python probe's missing `result` package was installed only under this workspace's ignored `build/python-deps`, leaving the shared environment unchanged. No commits or pushes.

Changed code: CMakeLists.txt; src/model.h/model.c/adapter.c/main.c/ui.c; new src/hardware.h/hardware.c/onewili_adapter.c; tests/test_model.c/test_render.c; new tests/test_hardware.c/test_onewili.c. Added offline audit/read-only probe tools, v002 artifact/checksum and this report. No new simulated features or sections were added.
