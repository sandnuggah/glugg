# SDK patches

Local fixes to the toolchain in `~/esp`. They live outside this repo, so re-apply them after any re-clone or submodule update of esp-matter.

| Patch | Applies to | Fixes |
|---|---|---|
| `connectedhomeip-shell-no-usb-host.patch` | `~/esp/esp-matter/connectedhomeip/connectedhomeip` | The S3 rebooting every ~30 s (endless white sweep) whenever no computer is on its USB port. |

```bash
for p in ~/glugg/patches/*.patch; do git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip apply "$p"; done
```

Only patches the firmware actually needs live here. Check whether one is applied with `git apply --check -R` and the same paths: no output means it is.

## connectedhomeip-shell-no-usb-host.patch

The S3's console is on its USB-Serial-JTAG port. ESP-IDF treats that port as connected only while a USB host sends start-of-frame packets (`usb_serial_jtag_connection_monitor.c`). A charger in the USB-C port, or power on the 5V pin with the port empty, sends none. While "disconnected", reading the console fails at once instead of waiting (`usb_serial_jtag_vfs.c`, `usb_serial_jtag_read`, Espressif's `TODO: IDF-14303`), and `linenoise()` returns an empty line.

Matter's console task (`src/lib/shell/MainLoopESP32.cpp`, `Engine::RunMainLoop`, priority 5, created by esp-matter's console) loops on `linenoise()` and treats an empty line as "try again" with no pause. So it spins, starves the idle task on its core, and with `CONFIG_ESP_TASK_WDT_PANIC=y` the task watchdog restarts the S3 every ~30 s. Without the panic setting it would silently burn a CPU core instead.

The patch sleeps 50 ms after an empty line. With a computer attached the read blocks as usual, so typing is unaffected.

**Status:** seen 2026-10-07 with both boards on an IKEA USB-C charger (endless white sweep). Fixed the same day: on the same charger with the patched firmware, the sweep stops after ~20 s and the sensor works with no computer attached.
