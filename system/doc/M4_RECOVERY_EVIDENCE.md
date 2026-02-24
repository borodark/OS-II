# M4 Recovery Evidence

This note tracks hardware evidence for:

- `status=4` retry path
- `status=5` degraded path
- watchdog-fed recovery reboot
- boot counter continuity across reboot

## Automated Capture

From repository root:

```bash
cd erts/example/mini_beam_esp32/zephyr_app
./capture_recovery_evidence.sh logs/recovery_evidence.log
```

The checker enforces:

- at least one `status=4`
- at least one `status=5`
- `withholding task_wdt feed` marker
- at least two boot banners
- at least two `boot counter=` lines
- boot counter increment across reboot (`first < second`)

Boot counter persistence implementation:

- nRF retained registers `GPREGRET`/`GPREGRET2`
- avoids reliance on RAM `__noinit` retention behavior across watchdog resets

## Current Session Status

- Full recovery path has been observed and validated by checker:
  - `status=4` retry
  - `status=5` degraded
  - watchdog hold marker
  - reboot to second boot banner
  - boot counter increment (`1 -> 2`)
- Validation source: `logs/recovery_evidence.log` (local runtime artifact).
