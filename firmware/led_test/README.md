# LED test (Phase 0)

Checks the LED stick wiring and shows every slot pattern. Runs on the H2-Zero now, and on the S3-Zero later, without code changes.

## Wiring

Use the final wiring from `CLAUDE.md`, but connect the data line to the board you're testing:

| Board | Data GPIO | Onboard LED |
|---|---|---|
| H2-Zero | **GPIO4** → 74HCT125N pin 2 | GPIO8 |
| S3-Zero | **GPIO13** → 74HCT125N pin 2 | GPIO21 |

To change a pin, run `idf.py -B build_<target> -D SDKCONFIG=sdkconfig.<target> menuconfig` and open the **LED test** menu. The board's GND must share the rail's GND.

If the board's `5V` pin is on the rail (through its diode), the Mac may not see the board when a direct C-to-C cable goes in after the supply is on. Plug USB in first, or go through a USB hub or USB-C→USB-A adapter that stays in the Mac (see **Power** in `CLAUDE.md`).

## Build (on the VM)

```bash
. ~/esp/esp-idf/export.sh
cd firmware/led_test
idf.py -B build_esp32h2 -D SDKCONFIG=sdkconfig.esp32h2 set-target esp32h2 build
idf.py -B build_esp32s3 -D SDKCONFIG=sdkconfig.esp32s3 set-target esp32s3 build
```

Run `set-target` only the first time. It wipes that build directory.

## Flash (the boards plug into the Mac)

**Option A: copy the binaries.** Copy `build_esp32h2/` to the Mac. Then, from inside it, run:

```bash
python -m esptool --chip esp32h2 -b 460800 write_flash @flash_args
```

Then open the console with any serial monitor at 115200 (e.g. `python -m serial.tools.miniterm /dev/cu.usbmodem* 115200`).

**Option B: RFC2217.** On the Mac, run `esp_rfc2217_server.py -v -p 4000 /dev/cu.usbmodem*`. Then, on the VM, run:

```bash
idf.py -B build_esp32h2 -D SDKCONFIG=sdkconfig.esp32h2 -p rfc2217://<mac-ip>:4000 flash monitor
```

If the board doesn't show up, hold BOOT while plugging it in.

## What you should see

The test loops forever and logs each step to the console. The onboard LED always mirrors LED 1 (the DIN end).

1. **Colour check.** All LEDs go red, then green, then blue.
   - Red and green swapped on the stick: its colour order isn't GRB, so edit `new_strip(..., false)` in `main.c`, and `LED_STRIP_COLOR_COMPONENT_FMT_GRB` in `../controller/main/led_task.cpp` the same way.
   - Red and green swapped on the onboard LED only: enable *Onboard LED uses RGB byte order* in menuconfig.
2. **Pixel order.** A white dot walks from LED 1, the DIN end, to LED 8. LED n is console slot n.
3. **Startup sweep, 6 s.** A dim white dot goes from LED 1 to LED 8 and back every 1.4 s, fading into its neighbours. The controller shows this until its Thread network is up.
4. **Slot patterns, 15 s:**

   | LEDs | State | Pattern |
   |---|---|---|
   | 1 | window open | steady warm amber |
   | 2, 6 | closed / empty slot | off |
   | 3, 7 | pairing | slow green pulse (3 s period) |
   | 4, 8 | not heard from | dim blue blink (0.3 s every 2 s) |
   | 5 | paired, no report yet | steady dim white |

   The controller's last state, Thread radio not answering, is all LEDs red: the colour check's red, dimmer.

## Failure hints

- **Nothing on the stick, but the onboard LED works:** check the 74HCT125N. Pins 1 and 7 go to GND, pin 14 to +5 V, and pin 3 through 330 Ω to DIN. The unused gates' pins 4, 5, 9, 10, 12 and 13 go to GND too (outputs 6, 8, 11 stay unconnected). Also check the common GND.
- **First LED is wrong or flickers:** the data edges are too slow or noisy. Keep the 330 Ω resistor close to DIN.
- **Random colours:** the stick has no GND reference, or the 1000 µF capacitor is missing.

The pattern colours and timings live in `../components/led_pattern`. Its host tests run with `make -C ../components/led_pattern/test`.
