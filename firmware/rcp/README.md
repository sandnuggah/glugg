# Thread radio firmware (H2)

Turns the ESP32-H2-Zero into the S3's Thread radio (an OpenThread RCP). It's ESP-IDF's `examples/openthread/ot_rcp`, copied from the same v5.5.5 tree that builds `../controller`, with our pins written into `sdkconfig.defaults`:

| | |
|---|---|
| Link | UART0 at 460 800 baud, no flow control (`main/esp_ot_config.h`) |
| H2 TX | GPIO24 → S3 GPIO5 |
| H2 RX | GPIO23 ← 1 kΩ ← S3 GPIO6 |

**Build it from the same ESP-IDF as the controller.** The S3 checks the radio's firmware version at boot. With a mismatch it logs that the radio "runs an incompatible RCP firmware version", turns every LED red and restarts. Rebuild and reflash the H2 whenever ESP-IDF is updated.

The firmware has no console and no logging: it only speaks Spinel to the S3 over its TX/RX pins. Flashing it replaces the LED test, so run `../led_test` on the H2 first if you still need it.

## Build (on the VM)

```bash
. ~/esp/esp-idf/export.sh
cd firmware/rcp
idf.py build
```

## Flash (the H2 plugs into the Mac)

Use the H2's own USB-C port. Flash it before wiring its 5V pin to the rail, or plug USB in before switching the supply on (see **Power** in `CLAUDE.md`).

**Option A: copy the binaries.** Copy `build/` to the Mac. Then, from inside it, run:

```bash
python -m esptool --chip esp32h2 -b 460800 write_flash @flash_args
```

**Option B: RFC2217.** On the Mac, run `esp_rfc2217_server.py -v -p 4000 /dev/cu.usbmodem*`. Then, on the VM, run:

```bash
idf.py -p rfc2217://<mac-ip>:4000 flash
```

If the board doesn't show up, hold BOOT (GPIO9) while plugging it in.

## Check it

The H2 shows nothing by itself. Wire it to the S3, flash `../controller` to the S3, and watch the S3's boot log:

- **Working:** OpenThread logs the RCP version, the network comes up, and the LEDs stop sweeping.
- **Radio not answering** (red LEDs, then a restart): swapped TX/RX, a missing common GND, the H2 not flashed, or a baud mismatch.
- **Incompatible RCP firmware:** the H2 was built from a different ESP-IDF than the S3. Rebuild both from the same tree.
