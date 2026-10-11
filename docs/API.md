# API investigation

Current checkpoint status: [v003 validated baseline](STATUS.md). This report preserves the historical investigation/build findings.

Checked the pinned local OneWili source and [official analog-output reference](https://github.com/freewili/onewili/blob/b0eeccda21b0594c8062cd17e9755f0c26b0cf6b/docs/analog_out.md).

Both requested C functions return `ow_status`. Their parameter types match the request; `src/api_contract.c` checks the function-pointer types against the actual official header during the device build. Generated implementations propagate command failure and discard the response payload. Neither provides a measured waveform or output-state readback.

Configuration limits from that reference are 1.0–5.5 V and 10–2000 ns. Pulse width is approximate. The analog subsystem requires power zone 11. The documentation does not establish deterministic external-trigger delay for this operation.

The supported DISPLAY-to-MAIN transport is the FwGUI link, with stock MAIN firmware. The BSP requires its HOME-aware `fw2_app_recovery_open_onewili` wrapper after board/recovery initialization; SD users also use `fw2_app_recovery_wrap_sd`. v001 did not initialize that transport. v002 links and initializes it, reads the documented OTP identity chunk and maintains the standard link. No analog zone is requested; physical output operations remain locked. See [M2A](M2A.md) for actual MAIN USB responses and the pending on-device FwGUI validation.

Observation investigation: OneWili exposes GPIO reads, UART menus, analog input menus, and polled text/binary events. Display-link binary support is selective; logic-analyzer binary reports are not mirrored on this pinned transport. No observation mechanism is configured by this application, and no target response is asserted.

Persistent app-owned files would use supported `ow_sd_*` operations beneath `/appdata/WiliGlitch/`, checking close status because writes can fail at close. No persistence/export is implemented here.
