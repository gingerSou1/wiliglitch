# WiliGlitch

Fault Injection & Resilience Lab — diagnostic version 003 for FREE-WILi 2 DISPLAY.

Development is paused after successful physical UI validation of v003.

- **v001:** Physical UI validation passed.
- **v002:** OneWili integration implemented; physical testing revealed flickering and instability.
- **v003:** Physical UI validation passed; automatic OneWili initialization and retries are paused.
- **Live DISPLAY-to-MAIN communication remains unverified.** Programmable-power, ARM and glitch outputs remain disabled.

See [checkpoint status](docs/STATUS.md) and [regression investigation](docs/V003-REGRESSION.md). Current source is the passive v003 baseline. The v002 transport description below is historical.

This application provides six touchscreen sections, validated configuration staged in a dedicated hardware adapter, session profiles and records of unavailable manual-shot requests. It starts DISARMED. Version 002 integrates the official DISPLAY-to-MAIN FwGUI transport and reports read-only OTP-query connection status. Physical output enable/disable, ARM and injection remain locked. Requested settings are never applied to the programmable rail; observed output state remains UNKNOWN.

Runtime responses use split-phase send/poll and zero-timeout reads, keeping the UI active during reply waits. The upstream FwGUI send still uses blocking UART flow control; stalled-CTS behavior is a concrete unresolved limitation documented in the M2A report. Link initialization alone never reports confirmed MAIN communication. Voltage Glitch shows the last confirmed response separately from current requests/errors.

Touch a section to open it. Use +/− to edit configuration. Back returns to the menu and disarms. CANCEL or HOME returns to the menu; hold physical HOME for five seconds to recover stock DISPLAY firmware. Hold PAGE for five seconds for the official About screen. SAVE RAM stores the current session profile; LOAD / DEFAULT restores it or defaults. Profiles and records do not survive exit. SHOT N/A records an unavailable request without hardware activity.

## Build

Use unmodified official dependencies, supplied through CMake paths:

| Dependency | Verified revision |
| --- | --- |
| WiliBSP | `be4bdd63d31a80f95410e583710cf4e43a7be7fa` |
| OneWili | `b0eeccda21b0594c8062cd17e9755f0c26b0cf6b` |
| Pico SDK | `98a542c1a62fb549ffb5d66a3e5892b06276b670` (2.3.0) |
| ARM compiler used | GNU 15.2.1 |

WiliBSP requires its Pico-PIO-USB sources; Pico SDK requires TinyUSB. Install CMake, Ninja, Python, ARM GCC, and a host C compiler. Supply existing picotool and pioasm packages for offline builds. No dependency tree is modified by this project.

```sh
cmake -S . -B build/host -G Ninja -DWILIBSP_PATH=/path/to/wilibsp
cmake --build build/host
ctest --test-dir build/host --output-on-failure

cmake -S . -B build/device -G Ninja -DWG_DEVICE=ON \
  -DWILIBSP_PATH=/path/to/wilibsp -DONEWILI_PATH=/path/to/onewili \
  -DPICO_SDK_PATH=/path/to/pico-sdk \
  -Dpicotool_DIR=/path/to/picotool/package -Dpioasm_DIR=/path/to/pioasm/package
cmake --build build/device
```

On Windows, use PowerShell line continuations or one command line and specify `CMAKE_MAKE_PROGRAM`, `CMAKE_C_COMPILER`, `CMAKE_AR`, and `CMAKE_RANLIB` when tools are not on PATH. Renderer tests are registered when the BSP header is available; the foundation test has no BSP dependency. Assertions remain enabled in Release tests.

Add `-DONEWILI_PATH=/path/to/onewili` to the host configuration to run the pinned generated API transport tests. With both dependency paths available, all five suites run.

The official `fw2_display_app` wrapper builds a `no_flash` SRAM UF2 and checks app metadata and target addresses. Current source produces v003; packaged review artifact: `dist/WiliGlitch-v003.uf2`. Existing v001/v002 UF2s remain frozen separately. [M2A report](docs/M2A.md) records the historical v002 build and USB MAIN evidence.

## SD application installation

The validated versions were deployed for physical testing. For a separately authorized installation, use the official WiliBSP SD workflow:

```sh
python /path/to/wilibsp/tools/fw.py install-app dist/WiliGlitch-v003.uf2 --device YOUR_DEVICE_SERIAL
```

The official workflow copies the SRAM app into SD `/apps/` and returns SD ownership to MAIN. Select the file through the stock Apps browser. Do not use BOOTSEL flashing to install this application. Holding physical HOME for five seconds returns to stock firmware.

Versioned artifacts and checksum files are in `dist/`; byte-identical rollback copies for all three versions are in `dist/rollback/`. No installation is performed as part of the GitHub checkpoint.

Historical reports: [M1](docs/MILESTONE-1.md), [M2A](docs/M2A.md), [API investigation](docs/API.md). Private device-identifying deployment/probe records remain local and are excluded from Git.
