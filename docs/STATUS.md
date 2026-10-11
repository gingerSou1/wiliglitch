# Validated v003 checkpoint

Development is paused as of 2026-10-11. Physical validation results below were reported by the user; build/test results are recorded separately.

| Version | Current status |
| --- | --- |
| v001 | Physical UI validation passed: launch, six sections, Back, DISARMED banner and five-second HOME recovery. |
| v002 | Official OneWili integration implemented; deployed physical testing revealed flickering and instability. |
| v003 | Physical UI validation passed. Automatic OneWili initialization and retries are paused. |

Live DISPLAY-to-MAIN communication remains unverified. Historical PC USB read-only replies do not validate the DISPLAY transport. Programmable-power enable/disable, ARM and glitch operations remain disabled. Configured voltage/pulse width are local requests, not hardware-confirmed measurements.

The v003 source uses normal BSP startup, passive transport isolation, bounded diagnostic counters and rate-limited partial status updates. Details and unresolved blocking-write concerns are in [the regression investigation](V003-REGRESSION.md).

Available host suites: foundation, hardware adapter, official C API response parsing, official HOME hold-state logic and rendering. All five pass at this checkpoint. No new firmware build or deployment was performed during checkpoint preparation.

Validated release UF2s are retained under `dist/`; matching rollback copies are under `dist/rollback/v001`, `v002` and `v003`. Their original bytes and SHA-256 values are preserved. Private deployment/probe logs, serial identities, local paths, build directories, toolchains, compiler caches and the superseded undeployed review candidate are excluded from Git.

This checkpoint records project state only. It does not authorize further development, firmware changes, deployments or output activation.
