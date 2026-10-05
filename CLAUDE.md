# Glugg

Glugg (Swedish for a small opening in a wall) watches the windows of the user's wood workshop. An ESP32-S3 pairs with IKEA MYGGBETT door/window sensors (Matter over Thread) and lights one LED per open window on an 8-LED WS2812 stick. Sensor n owns LED n, in the order sensors were paired.

- Full plan, wiring diagram, risks and sources: https://claude.ai/artifact/5PzD6ZSrUVS8m9g1BBY7jM (read it with the Artifact tool, `action: "read"`)
  - Local snapshot: `docs/plan.html` (rev 11, 2026-10-05, the version published from it). It doesn't update itself; the artifact is the live version. It includes the publish skeleton; strip everything up to `<body>` and the closing `</body></html>` before republishing from it.
- Parts list: `bom.csv` (Electrokit article numbers, `sku; qty`) has every electronic component.
  - Not in it: pin headers (both boards are the no-header versions), perfboard and a DIP-14 socket, hook-up wire, terminal blocks for the rails, DIN rail or an enclosure, and the mains lead and fused switch.
  - The first 7 lines were ordered on 2026-09-30.
  - Lines 8–10 (H2-Zero 41032818, Adafruit NeoPixel Stick 41012479, Mean Well HDR-15-5 41021466) are parts the user already owned, or an Electrokit substitute for them. Electrokit doesn't sell the RS-15-5, so HDR-15-5 stands in: same wiring, but Class II, so there is no FG/earth terminal.
  - The last line (40811310, 1 kΩ metal film) is the UART series resistor. It was added after the order and is still to buy.
  - The MYGGBETT sensors come from IKEA, not Electrokit.

## Settled decisions (don't reopen without asking the user)

| Area | Decision |
|---|---|
| Controller | Waveshare **ESP32-S3-Zero-N8R8** (8 MB flash, 8 MB PSRAM, probably octal; verify with `esptool.py flash_id` on arrival) |
| Thread radio | Waveshare **ESP32-H2-Zero**, flashed with ESP-IDF `ot_rcp` and connected to the S3 over UART |
| Network | **Fully offline, no Wi-Fi.** The S3 runs the Thread network plus the SRP and DNS-SD servers itself. |
| Pairing | USB serial console on the S3: `pair <11-digit code>` |
| LEDs | open = warm amber at about 25% brightness · closed or empty slot = off · pairing into slot = slow green pulse · sensor not heard from = dim blue blink · starting up = a dim white dot sweeping back and forth across all 8 LEDs until the Thread network and Matter's lookup service are up · paired sensor that hasn't reported yet (since boot or pairing) = steady dim white, until it reports or the blue blink takes over after 2 min · Thread radio (H2) not responding = all LEDs red for a few seconds, then the S3 restarts |
| Slots | 8 max for now (one 8-LED stick). Removing a sensor frees its slot, and the next pairing takes the lowest free slot. |
| Scale-up (decided 2026-10-01) | The workshop has 4 rooms with ~30 windows each. Plan: **one Glugg unit per room** (S3 + H2 + its own LED strip, one LED per window, its own Thread network), not one central controller with Thread routers. Comes after Phase 4 (see Phase 6). |
| Toolchain | **ESP-IDF v5.5.5** + **esp-matter release/v1.6** (component 1.6.0) + `espressif/led_strip` 3.0.3 |
| Emulation | No Wokwi. The user skipped the Matter desktop tools (chip-tool + contact-sensor-app). Test LED code on the real H2-Zero + LED stick. |

Rejected options: the H2 alone (no PSRAM, and Espressif doesn't test the controller on it) and the ESP32-C5-Zero (no PSRAM, plus esp-matter #1851: children can't attach while BLE is initialised).

## Wiring (pins are fixed)

- **Power:** Mean Well RS-15-5 (or the BOM's HDR-15-5: same +V/−V wiring, no FG terminal) set to 5.0 V feeds a +5 V rail and a GND rail. Each board's `5V` pin goes through its own **1N5817** diode, because both boards wire 5V straight to USB VBUS. The ~4.7 V after the diode is fine for both boards, so leave the supply at 5.0 V.
  - The diode protects the rail from USB, not USB from the rail. With the supply on, each board's USB-C VBUS already sits at ~4.7 V, and a USB-C host (the Mac) may refuse to attach to a port that already has VBUS on it. A direct C-to-C cable plugged in after the supply is on may then give no console.
  - Fix: connect the S3 through something that stays plugged into the Mac (any USB hub or dock, or a USB-C→USB-A adapter plus an A-to-C cable), and plug and unplug only at the board end. The Type-C check happens when the hub or adapter goes into the Mac, and the S3 shows up later like any device on a USB-A port. The Mac's 5 V is above the ~4.7 V behind the diode, so nothing back-feeds the Mac.
  - With a direct C-to-C cable, plug USB in before switching the supply on. Many ports are lenient, so the direct cable may work anyway. Check it once on the bench.
- **UART link:** H2 `TX` (GPIO24) → S3 **GPIO5** (UART1 RX), and S3 **GPIO6** (UART1 TX) → **1 kΩ** (at the S3 end) → H2 `RX` (GPIO23). The link runs at **460800** baud on UART port 1, which matches `ot_rcp`'s default. Espressif's example used GPIO17/18. `firmware/controller/main/esp_ot_config.h` now uses 5/6 in `radio_uart_config`.
  - Why the 1 kΩ: when only the S3 is powered (e.g. on USB with the supply off), its TX idles high and would back-power the H2 through the H2's input protection diode. The resistor limits that current and is harmless at 460800 baud. The other direction needs nothing: the H2-Zero board already has 499 Ω in series on its TX.
- **LED data:** S3 **GPIO13** → 74HCT125N pin 2 (1A), with a 10 kΩ pull-down → pin 3 (1Y) → 330 Ω → stick DIN. 74HCT125N pins 1 (1OE) and 7 go to GND, and pin 14 goes to +5 V with 100 nF to GND. The unused gates' inputs and OE pins (4, 5, 9, 10, 12, 13) also go to GND. Leave their outputs (6, 8, 11) unconnected. A 1000 µF capacitor sits across the stick's power.
- **Onboard parts:**
  - S3: RGB LED on GPIO21, BOOT on GPIO0, USB on GPIO19/20, UART0 on GPIO43/44. Strapping pins: 0, 3, 45, 46. GPIO33–37 are used by PSRAM.
  - H2: RGB LED on GPIO8, BOOT on GPIO9.

## MYGGBETT facts

- Matter over Thread, commissioned over BLE. VID `0x117C`, PID `0x8007`, firmware 1.0.9–1.1.6.
- It is a SIT ICD sleepy end device **without** the Check-In protocol, so the controller must keep a subscription alive.
- Endpoint 1 is a Contact Sensor with the **BooleanState** cluster (`0x0045`). Subscribe to attribute `StateValue` (`0x0000`). **true = closed, false = open.** It sends no StateChange event.
- Attestation needs IKEA's PAA certificate bundled in the trust store, because the device is offline. It's bundled as `firmware/controller/paa_cert/ikea_g1.der` (`CN=IKEA of Sweden Matter PAA G1`, vid 0x117C) along with every other production PAA on the DCL. See `paa_cert.md`.

## Risks, in priority order

1. **Offline lookup (Phase 1 go/no-go). Passed in simulation on 2026-09-30.** See `sim/README.md`.
   - The controller example's OTBR config (`sdkconfig.defaults.otbr`) sets `CONFIG_OPENTHREAD_SRP_CLIENT=n` and `DNS_CLIENT=n`, and relies on Wi-Fi mDNS.
   - Offline, we must re-enable both clients and start the SRP server by hand (`otSrpServerSetEnabled`). No manual DNS server config is needed: Matter's `OnSrpClientStateChange` points the DNS client at the SRP server its own SRP client auto-selects, which is the S3 itself.
   - Still unproven, check on the S3 in Phase 2:
     - The DNS query to the S3's own ML-EID goes through lwIP (`PLATFORM_UDP`), so it needs `CONFIG_LWIP_NETIF_LOOPBACK=y`.
     - `CONFIG_OPENTHREAD_BORDER_ROUTER` (needed for the SRP and DNS-SD servers) must work with Wi-Fi compiled out. It **builds** in `firmware/controller`, which needed an mdns link fix (see its README). Runtime is unproven.
   - Fallback: a custom resolver that reads the SRP server's table (`otSrpServerGetNextHost` / `otSrpServerHostGetAddresses`).
2. **IKEA BLE commissioning bug:** `Disabling CHIPoBLE service due to error: ac` (esp-matter #1772, #1532, both still open; IKEA TIMMERFLOTTE, KLIPPBOK, BILRESA and Aqara devices reported).
   - **Likely cause found and patched locally (2026-10-05), unconfirmed until Phase 2:** NimBLE reports the central's connection late, after the peripheral has already exchanged the ATT MTU, so Matter's own `ble_gattc_exchange_mtu()` gets `BLE_HS_EALREADY`, which it turns into `CHIP_ERROR_INTERNAL` (0xac). See `patches/README.md`.
   - If pairing still fails, the log now shows `BLE MTU exchange failed: <code>`. If it works, offer to post the analysis on both issues and a PR upstream (ask the user first).
3. **Resubscription:** esp-matter's `k_max_resubscribe_retries = 2`. **Implemented, still to be proven in the Phase 4 soak:** `firmware/controller/main/sensor_link.cpp` keeps its own subscriptions with auto-resubscribe off, and `firmware/components/app_core/sensor_mgr.c` retries with unlimited backoff (10 s → 5 min) and sets the "not heard from" state.

## Work phases

0. **Toolchain + hardware check.** Install ESP-IDF 5.5.5 and esp-matter v1.6. Run an LED test on real hardware.
   - **Toolchain installed. LED test firmware written (`firmware/led_test`), not yet run on hardware.**
1. **Offline Thread network + lookup service.**
   - **Passed in simulation on 2026-09-30** (`sim/README.md`). The S3-only checks under Risk 1 happen in Phase 2.
   - Before the S3 arrives, prove it in the OpenThread POSIX simulation: `./script/cmake-build simulation`, then run `ot-cli-ftd` nodes.
   - The leader runs `srp server enable`. A second node registers a service over SRP. The leader then resolves that service against **its own** ML-EID with `dns browse` / `dns service`.
2. **Pair one MYGGBETT** (needs the S3).
   - **Not started.**
3. **The app:** slot manager (NVS), subscription manager, LED renderer, console commands (`pair`, `list`, `remove`, `identify`, `factory-reset confirm`).
   - **Written, not yet run on hardware.** Host logic is in `firmware/components/app_core` + `led_pattern` (tests: `make -C firmware/components/app_core/test` and `make -C firmware/components/led_pattern/test`). The Matter glue is in `firmware/controller/main` (see its README). It builds for the S3.
4. **Multiple sensors + 48 h soak test.**
   - **Not started.**
5. **Enclosure + README.**
   - **Not started.**
6. **Scale to one unit per room (~30 sensors each).** Not started; size it from Phase 4's measured memory use per sensor.
   - `SLOT_COUNT` 8 → ~30 (bump the slot blob `VERSION` and migrate), a ~30-LED strip on the same data path.
   - OpenThread child table (`CONFIG_OPENTHREAD_MLE_MAX_CHILDREN`) 10 → ≥32. Matter secure session pool 17 → ~40, exchanges and handshake pools to match.
   - Cap concurrent subscribe attempts (e.g. 8) so 30 sleepy sensors don't all reconnect at once after a power cut.
   - `pair <code> <slot>`, so a window's LED position is chosen, not "lowest free".
   - Probably no Thread router needed inside one room; check link quality with `matter esp ot_cli child table`.

Keep app logic (slots, LED state mapping, backoff, command parsing) free of Matter and ESP dependencies behind small interfaces, so it can be unit-tested on the host.

## Environment

- This VM `baggio-dev` (Debian 13, Python 3.13, 4 cores, 8 GB RAM + 6 GB swap, 40 GB disk) is the build machine. No boards are attached to it.
- Toolchains live in `~/esp/esp-idf` and `~/esp/esp-matter`. Load them with `. ~/esp/esp-idf/export.sh && . ~/esp/esp-matter/export.sh`. esp-matter was installed with `--no-host-tool`.
- **Local patches** to the SDK, both in `~/esp/esp-matter/connectedhomeip/connectedhomeip`. Re-apply them after any re-clone or submodule update:
  - `scripts/setup/constraints.txt`: `typing-extensions` is raised from 4.8.0 to 4.15.0. Without it, esp-matter's `install.sh` fails on Python 3.13.
  - `src/platform/ESP32/nimble/BLEManagerImpl.cpp`: the IKEA BLE pairing fix (Risk 2). Kept as `patches/connectedhomeip-ble-mtu-ealready.patch`; apply with `git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip apply ~/glugg/patches/connectedhomeip-ble-mtu-ealready.patch`.
- The OpenThread simulation (`ot-cli-ftd`, built from ESP-IDF's bundled OpenThread) is at `~/esp/ot-sim`.
- The boards plug into the user's Mac. To flash from the VM, either copy the binaries to the Mac and use `esptool`, or run `esp_rfc2217_server.py` on the Mac and use `idf.py -p rfc2217://<mac-ip>:4000 flash monitor`.
- Limit build parallelism if the build runs out of memory. Each compile job needs about 1–1.5 GB.
