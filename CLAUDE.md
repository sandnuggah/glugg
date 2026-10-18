# Glugg

Glugg (Swedish for a small opening in a wall) watches the windows of the user's wood workshop. An ESP32-S3 pairs with IKEA MYGGBETT door/window sensors (Matter over Thread) and lights one LED per open window on an 8×8 WS2812 panel (changed 2026-10-17 from an 8-LED stick whose pads broke off). Up to 64 sensors (Phase 6): sensor n owns LED n along the data chain from the DIN end, either the slot chosen at pairing or the lowest free one. LEDs without a sensor stay off except for the startup ring and the red radio-failure state.

- Full plan, wiring diagram, risks and sources: https://claude.ai/artifact/5PzD6ZSrUVS8m9g1BBY7jM (read it with the Artifact tool, `action: "read"`)
  - Local snapshot: `docs/plan.html` (rev 24, 2026-10-18, the version published from it). It doesn't update itself; the artifact is the live version. It includes the publish skeleton; strip everything up to `<body>` and the closing `</body></html>` before republishing from it.
- Parts list: `bom.csv` (Electrokit article numbers, `sku; qty`) has every Electrokit part of the protoboard build as of 2026-10-17: both Zero boards (header versions 41036436 / 41032832), 74HCT32 40381032, 18-pin DIL socket, 1N5817, 330 Ω / 10 kΩ / 1 kΩ, 1000 µF, 100 nF, 3-pole 2.54 mm screw terminal 41002454, 40×60 mm protoboard 41010656.
  - Not in it: the 8×8 WS2812 panel (bought elsewhere), a USB-C charger and cable (see Power), solid-core hook-up wire, and an enclosure. The MYGGBETT sensors come from IKEA.
  - History: the 2026-09-30 order had the no-header boards and a 74HCT125N; the first S3 failed during the first protoboard attempt; the 2026-10-16 order brought the header boards, the 74HCT32 (the 74HCT125N was out of stock) and the protoboard parts. The 8-LED NeoPixel Stick's pads broke off (2026-10-17), so an 8×8 panel replaced it. The Mean Well supply was dropped on 2026-10-07 in favour of USB-C power.

## Settled decisions (don't reopen without asking the user)

| Area | Decision |
|---|---|
| Controller | Waveshare **ESP32-S3-Zero-N8R8**. Checked 2026-10-06 with `esptool flash-id`: an **ESP32-S3-PICO-1** (LGA56, rev v0.2) system-in-package with 8 MB GigaDevice quad flash and 8 MB embedded AP Memory PSRAM at 3.3 V. Embedded 8 MB on the S3 is octal, matching `CONFIG_SPIRAM_MODE_OCT`; the first boot log confirms it. |
| Thread radio | Waveshare **ESP32-H2-Zero**, flashed with ESP-IDF `ot_rcp` (`firmware/rcp`) and connected to the S3 over UART |
| Network | **Fully offline, no Wi-Fi.** The S3 runs the Thread network plus the SRP and DNS-SD servers itself. |
| Pairing | USB serial console on the S3: `pair <11-digit code> [slot <n>]` |
| LEDs | open = warm amber at about 25% brightness · closed or empty slot = off · pairing into slot = slow green pulse · sensor not heard from = dim blue blink · starting up = a dim white ring breathing out from the panel's centre (every 2.2 s) until the Thread network and Matter's lookup service are up · paired sensor that hasn't reported yet (since boot or pairing) = steady dim white, until it reports or the blue blink takes over after 2 min · Thread radio (H2) not responding = the whole panel red for a few seconds, then the S3 restarts |
| Slots | 64, slot n = LED n across the 8×8 panel (raised from 8 on 2026-10-17). `pair … slot <n>` picks the LED, otherwise the lowest free slot. `move <from> <to>` puts a sensor on another LED and swaps if that one is used. Removing a sensor frees its slot. |
| Scale-up (decided 2026-10-01; target raised to 64 on 2026-10-17) | The workshop has 4 rooms with ~30 windows each. Plan: **one Glugg unit per room** (S3 + H2 + its own 8×8 panel, one LED per window, its own Thread network), not one central controller with Thread routers. **Each unit supports up to 64 sensors**, so a room can grow or one unit can cover two rooms. Comes after Phase 4 (see Phase 6). |
| Toolchain | **ESP-IDF v5.5.5** + **esp-matter release/v1.6** (component 1.6.0) + `espressif/led_strip` 3.0.3 |
| Emulation | No Wokwi. The user skipped the Matter desktop tools (chip-tool + contact-sensor-app). Test LED code on the real S3 + LED panel. |

Rejected options: the H2 alone (no PSRAM, and Espressif doesn't test the controller on it) and the ESP32-C5-Zero (no PSRAM, plus esp-matter #1851: children can't attach while BLE is initialised).

## Wiring (pins are fixed)

- **Power (changed 2026-10-08):** a USB-C charger (any; the user has a GaN brick) plugs straight into the **S3's USB-C port**. The S3-Zero ties its `5V` pin to its USB VBUS, so the **S3's 5V pin is the +5 V rail**: it feeds the 74HCT32, the LED panel and the 1000 µF capacitor directly, and the H2 through the one **1N5817** (**S3 5V → diode → H2 5V**). No breakout, no mains, no earth. The H2 gets ~4.7 V, fine for its 3.3 V LDO.
  - Current budget: everything runs through the S3-Zero's USB-C socket and its 5V trace (undocumented width), so keep the total under ~1 A. Boards ~0.3 A; amber at the 25% cap ~7 mA per LED. For ~30 LEDs, 50% amber (~0.7 A worst case, all windows open) is comfortable. Full brightness would need the LEDs fed straight from the charger (a USB-C breakout).
  - Console: power and console share the S3's one port. To pair or debug, swap the charger for the Mac (the unit restarts, pairings survive) and the Mac powers everything. Or put a USB-C hub with power pass-through between the charger, the S3 and the Mac.
  - If a Mac port shuts off ("USB accessory needs too much power"), the 1000 µF capacitor's inrush is the likely cause; 100–220 µF is plenty at 25%.
  - To flash the H2 over its own USB while wired, unplug the S3's power first. Its diode then keeps the Mac's USB from feeding the S3, and the Mac's port doesn't see 5 V already on the H2's socket.
  - Replaced (2026-10-07 to 10-08): a USB-C breakout feeding a rail, with a second diode in front of the S3. Before that, a Mean Well mains supply.
- **UART link:** H2 `TX` (GPIO24) → S3 **GPIO5** (UART1 RX), and S3 **GPIO6** (UART1 TX) → **1 kΩ** (at the S3 end) → H2 `RX` (GPIO23). The link runs at **460800** baud on UART port 1, which matches `ot_rcp`'s default. Espressif's example used GPIO17/18. `firmware/controller/main/esp_ot_config.h` now uses 5/6 in `radio_uart_config`.
  - Why the 1 kΩ: if the S3 runs while the H2 has no power (now only while wiring or flashing, since the H2 is fed from the S3's 5V pin), the S3's TX idles high and would back-power the H2 through the H2's input protection diode. The resistor limits that current and is harmless at 460800 baud. The other direction needs nothing: the H2-Zero board already has 499 Ω in series on its TX.
- **LED data:** S3 **GPIO13** → 74HCT32 pins 9 and 10 (gate 3's inputs, tied together), with a 10 kΩ pull-down → pin 8 (gate 3's output) → 330 Ω → screw terminal DIN → panel DIN. Pin 14 to +5 V with 100 nF to GND, pin 7 to GND. Unused inputs: pins 1, 2, 12, 13 to +5 V and pins 4, 5 to GND; outputs 3, 6, 11 unconnected. A 1000 µF capacitor sits across the terminal's 5V and GND. On a 74HCT125 pin 10 would be an enable pin, so this wiring is '32-only. Hole-by-hole layout: the build guide artifact https://claude.ai/artifact/TmpncgYGMUV4vgWFSSakSP and `docs/protoboard-small*.svg`.
- **Onboard parts:**
  - S3: RGB LED on GPIO21, BOOT on GPIO0, USB on GPIO19/20, UART0 on GPIO43/44. Strapping pins: 0, 3, 45, 46. GPIO33–37 are used by PSRAM.
  - H2: RGB LED on GPIO8, BOOT on GPIO9.

## MYGGBETT facts

- Matter over Thread, commissioned over BLE. VID `0x117C`, PID `0x8007`, firmware 1.0.9–1.1.6.
- It is a SIT ICD sleepy end device **without** the Check-In protocol, so the controller must keep a subscription alive.
- Measured 2026-10-07: it picks a subscription **max interval of 1800 s** (our 300 s ceiling is only a request) and an MRP idle interval of 17 s. State changes still arrive within a second, but a dead or out-of-range sensor shows "not heard from" only after ~33 min (Matter's 1880 s liveness timeout + our 2-min grace).
- After joining, and after the S3 reboots, it registers with the S3's SRP server a little after Matter's first lookup (within 15 s of joining Thread). Each Matter lookup asks once and then waits its full timeout, `CHIP_CONFIG_ADDRESS_RESOLVE_MAX_LOOKUP_TIME_MS`. That is 15 s in `firmware/controller/main/matter_project_config.h`, down from the 45 s default. Pairing makes 3 lookup attempts, so don't go much lower.
  - Measured 2026-10-07 with 15 s: `pair` → paired 30 s (was 60 s), → subscribed 46 s. Network up → subscribed after a reboot 42 s (was ~59 s).
  - The rest is the sensor's sleep: each first message waits up to ~15 s for its next check-in.
- `identify` (Matter Identify command) is accepted but has no visible effect on MYGGBETT (tested 2026-10-17). The command stays for other sensor models.
- Endpoint 1 is a Contact Sensor with the **BooleanState** cluster (`0x0045`). Subscribe to attribute `StateValue` (`0x0000`). **true = closed, false = open.** It sends no StateChange event.
- Attestation needs IKEA's PAA certificate bundled in the trust store, because the device is offline. It's bundled as `firmware/controller/paa_cert/ikea_g1.der` (`CN=IKEA of Sweden Matter PAA G1`, vid 0x117C) along with every other production PAA on the DCL. See `paa_cert.md`.

## Risks, in priority order

1. **Offline lookup (Phase 1 go/no-go). Passed in simulation on 2026-09-30.** See `sim/README.md`.
   - The controller example's OTBR config (`sdkconfig.defaults.otbr`) sets `CONFIG_OPENTHREAD_SRP_CLIENT=n` and `DNS_CLIENT=n`, and relies on Wi-Fi mDNS.
   - Offline, we must re-enable both clients and start the SRP server by hand (`otSrpServerSetEnabled`). No manual DNS server config is needed: Matter's `OnSrpClientStateChange` points the DNS client at the SRP server its own SRP client auto-selects, which is the S3 itself.
   - **Passed on the real S3 + H2 on 2026-10-06.** `matter esp ot_cli dns browse _matter._tcp.default.service.arpa.` answered with the controller's own SRP-registered service, address included:
     - The DNS query to the S3's own ML-EID goes through lwIP (`PLATFORM_UDP`), and `CONFIG_LWIP_NETIF_LOOPBACK=y` makes it work.
     - `CONFIG_OPENTHREAD_BORDER_ROUTER` runs with Wi-Fi compiled out (with the mdns link fix; the one `Failed to publish meshcop mdns service` error is expected).
     - The S3's SRP client picks its own server (`srp client server` = its ML-EID, port 53536). Network on channel 15. Ready ("Thread network and lookup service are up") 18.6 s after a reboot.
   - Fallback: a custom resolver that reads the SRP server's table (`otSrpServerGetNextHost` / `otSrpServerHostGetAddresses`).
2. **IKEA BLE commissioning bug:** `Disabling CHIPoBLE service due to error: ac` (esp-matter #1772, #1532, both still open; IKEA TIMMERFLOTTE, KLIPPBOK, BILRESA and Aqara devices reported).
   - **Not a problem for MYGGBETT (2026-10-07):** it pairs reliably with stock esp-matter code. A test build that logged the suspect path showed MYGGBETT never takes it.
   - A suspected cause (NimBLE reporting the central's connection only after the peripheral's own ATT MTU exchange, so Matter's `ble_gattc_exchange_mtu()` gets `BLE_HS_EALREADY` and fails with 0xac) and a patch for it were removed at the user's request, because MYGGBETT doesn't need it. Both are in git history (`patches/`, commits c91d4a4 and e30da52) if a future sensor model shows `error: ac`. The diagnosis is unconfirmed; don't present it upstream as a fix.
3. **Resubscription:** esp-matter's `k_max_resubscribe_retries = 2`. **Implemented, still to be proven in the Phase 4 soak:** `firmware/controller/main/sensor_link.cpp` keeps its own subscriptions with auto-resubscribe off, and `firmware/components/app_core/sensor_mgr.c` retries with unlimited backoff (10 s → 5 min) and sets the "not heard from" state.

## Work phases

0. **Toolchain + hardware check.** Install ESP-IDF 5.5.5 and esp-matter v1.6. Run an LED test on real hardware.
   - **Toolchain installed. Parts arrived 2026-10-06.**
   - **LED test passed on the S3 (2026-10-06)**, on the bench: GPIO13 → 74HCT125 → 330 Ω → stick, everything powered from the S3's USB 5V pin, no Mean Well supply.
   - **H2 flashed with `firmware/rcp` (2026-10-06):** ESP-IDF's `ot_rcp` from the same tree, UART0 460800 on GPIO23/24. The chip is an ESP32-H2 rev v1.2.
   - Still open: the checkpoint on supply power with USB attached comes with the final wiring.
1. **Offline Thread network + lookup service.**
   - **Passed in simulation on 2026-09-30** (`sim/README.md`) and **on the hardware on 2026-10-06** (bench: both boards on USB, UART link with GND): leader, SRP server running, own service registered and resolved through DNS-SD, network back after a reboot.
   - Before the S3 arrives, prove it in the OpenThread POSIX simulation: `./script/cmake-build simulation`, then run `ot-cli-ftd` nodes.
   - The leader runs `srp server enable`. A second node registers a service over SRP. The leader then resolves that service against **its own** ML-EID with `dns browse` / `dns service`.
2. **Pair one MYGGBETT** (needs the S3).
   - **Passed 2026-10-06** (bench): one MYGGBETT paired over BLE with the patched SDK, reports open/closed live, and is restored after a reboot without re-pairing. A first attempt found no sensor because its 15-minute pairing window had closed; a factory reset (hold the button ~10 s) reopened it.
3. **The app:** slot manager (NVS), subscription manager, LED renderer, console commands (`pair`, `list`, `remove`, `identify`, `factory-reset confirm`).
   - **Verified on the protoboard (2026-10-17), on the 8×8 panel:** startup ring, then a dark panel; `pair`, with open/closed on LED 1 (amber/off); `list`; recovery after a reboot without re-pairing; `remove`; `factory-reset confirm`. `identify` is accepted, but MYGGBETT shows nothing visible (see MYGGBETT facts). The blue "not heard from" blink was confirmed later the same day: battery out → blue blink, battery back → normal again, with no re-pairing. At the time the 74HCT32 was out of its socket (it had been damaged; debugged in a separate session), so the panel ran on 3.3 V data.
   - Host logic is in `firmware/components/app_core` + `led_pattern` (tests: `make -C firmware/components/app_core/test` and `make -C firmware/components/led_pattern/test`). The Matter glue is in `firmware/controller/main` (see its README). It builds for the S3.
4. **Multiple sensors + 48 h soak test.** **Passed 2026-10-18.**
   - **Memory measured 2026-10-17** (`matter esp diagnostics mem-dump`, after each sensor's LED showed its state). Free internal / PSRAM in bytes: 0 sensors 215 447 / 8 097 256 · 1 sensor 214 839 / 8 096 512 · 2 sensors 214 323 / 8 095 780 · 2 sensors after a reboot 214 895 / 8 096 496. Lowest free internal since boot ~188–189 KB (the pairing/BLE peak), largest free internal block 147 KB.
   - So the heap costs about 0.3–0.6 KB internal + 0.4–0.75 KB PSRAM per sensor: ~40 KB internal + ~50 KB PSRAM for 64. The Phase 6 pool increases are static and land in PSRAM (+123 KB, see Phase 6).
   - **Two sensors verified 2026-10-17** (slots 1 and 2): each window drives only its own LED; `remove 1` + re-pair lands on slot 1 again while slot 2 keeps working. Battery-out blue blink and recovery confirmed 2026-10-17.
   - **48 h soak passed (2026-10-16/18)**, two sensors on the charger, with the 8-slot panel firmware (before Phase 6): both LEDs followed their windows throughout; after a power cut and a battery swap, Glugg and the sensors recovered by themselves in about a minute (~20 s ring + ~40 s, observed, not timed). The coredump partition (0x3e0000, 128 KB) read back blank, so no panic or watchdog reset. Not measured: memory after a long run, because attaching the Mac restarts the unit. Use a USB-C hub with power pass-through and `mem-dump` at start and end next time.
5. **Enclosure + README.**
   - **Not started.**
6. **Scale to 64 sensors per unit (target changed 2026-10-17 from ~30), one unit per room.** **Firmware verified on the protoboard 2026-10-18 with two sensors** (H2 reflashed with the 64-entry pending table, S3 flashed without erasing). Sized from Phase 4's measured memory use per sensor.
   - Hardware results: the old 8-slot table migrated (both sensors kept slots 1 and 2); `list` shows `2 of 64 slots used`; `move` (including a swap) keeps each LED following its window; `pair <code> slot 20` paired onto LED 20, and pairing onto an occupied slot is refused. Power cut, timed: LED 1 back after 28 s, LED 2 after 58 s (~20 s ring + each sensor's next check-in). `mem-dump`: internal free 214.5 KB (unchanged), lowest 187.9 KB; PSRAM free 7,945 KB (−151 KB: the +123 KB static pools plus ~28 KB of 64-entry tables).
   - Still open: more sensors (the 8-at-once reconnect cap only engages above 8), radio range across a full room, and the room units themselves (possibly on the PCB).
   - `SLOT_COUNT` 8 → 64. No blob `VERSION` bump: the blob stores its own slot count, and `slot_table_decode` accepts any count 1–64, so the 8-slot table saved before loads as-is and is re-saved with 64. Slot n = LED n across the whole 8×8 panel; all 64 amber at 25% is about 0.45 A, inside the S3's budget.
   - OpenThread child table (`CONFIG_OPENTHREAD_MLE_MAX_CHILDREN`) 10 → ~70 (ceiling 511). Matter secure session pool (`CHIP_CONFIG_SECURE_SESSION_POOL_SIZE`) 17 → ~72, `CHIP_CONFIG_CONTROLLER_MAX_ACTIVE_DEVICES` 8 → 64, CASE session resumption cache → 64 (all in `matter_project_config.h`; the child table in `sdkconfig.defaults`).
   - The H2's `CONFIG_IEEE802154_PENDING_TABLE_SIZE` (20, max 100) only holds sensors with a message waiting right now; when it overflows OpenThread acks every poll with "pending", which costs sensor battery but doesn't fail. Raised to 64 in `firmware/rcp/sdkconfig.defaults`, so the H2 needs reflashing with this change.
   - Static data already lives in PSRAM (`CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`; internal `.dram0.bss` is only 11.6 KB). Measured 2026-10-17 with a scratch build using the 64-sensor settings above (`SLOT_COUNT` 64, 70 children, 72 sessions, 64 active devices, 64 resumption entries): PSRAM `.ext_ram.bss` 78.8 KB → 201.7 KB (+123 KB), internal unchanged. With the per-sensor heap from Phase 4 (~40 KB internal for 64), memory is not a constraint.
   - `SENSOR_MAX_ATTEMPTS_IN_FLIGHT` 8 in `sensor_mgr`: at most 8 subscribe attempts at once, lowest slot first, so 64 sleepy sensors don't all reconnect at once after a power cut; expect all back within 2–3 min. Time it.
   - Console: `pair <code> [slot <n>]` (the word `slot` marks it, because the code may contain spaces), `move <from> <to>` (swaps), slots 1–64 everywhere, and `list` shows only used slots plus "N of 64 slots used". Subscriptions find their slot by node ID at each callback, so a `move` can't misroute a report.
   - Traffic is negligible (64 polls per ~15 s plus 30-min keep-alives). Range is the open question: every sensor attaches directly to the H2. If some can't reach it, add a Thread router: a spare ESP32-H2 with OpenThread's full-device (FTD) firmware, given the network key; no Matter pairing needed. Check link quality with `matter esp ot_cli child table`.

**Parked: pairing from the user's iPhone (for convenience; nothing decided).** Raise it again once Phase 2 pairs a MYGGBETT, and before Phase 6.
- The idea: a Bluetooth console on the S3 (Nordic UART service layout, added through the ESP32 BLE manager's `ConfigureExtraServices`, with its own advertising and GAP handler) next to the USB console, opened for a few minutes by pressing the S3's BOOT button.
- The phone side, in steps: a generic app (Bluefruit Connect / nRF Toolbox UART) first, then perhaps a SwiftUI app that scans the sensor's `MT:` QR code. Glugg would accept `pair MT:…`.
- Safari has no Web Bluetooth. Apple Home is out (offline). Main risk: the S3 being peripheral to the phone and central to the sensor at the same time.

**To explore: a custom PCB (added 2026-10-17; nothing decided).** After the Phase 4 soak and before building the Phase 6 room units.
- The idea: a carrier PCB for the S3-Zero + H2-Zero with the 74HCT32, diode, resistors, capacitors, an LED panel connector and mounting holes, ordered in a small batch, so every room unit is identical and no protoboard wiring is needed.
- Decided 2026-10-17: design it in **KiCad** (schematic + board as the source, checked with KiCad's ERC/DRC, Gerbers and drill files exported from it), not hand-generated Gerbers. Install KiCad 9 on the VM (`sudo apt install kicad`) when the work starts; Freerouting (Java) if autorouting is needed. The Zero-board footprints are custom (2 × 9, 15.24 mm rows); check them against the real boards with a 1:1 print before ordering.
- Still to settle: through-hole vs factory-assembled SMD, keep the Zero boards vs put the modules on the board, cost and lead time.

Keep app logic (slots, LED state mapping, backoff, command parsing) free of Matter and ESP dependencies behind small interfaces, so it can be unit-tested on the host.

## Environment

- This VM `baggio-dev` (Debian 13, Python 3.13, 4 cores, 8 GB RAM + 6 GB swap, 40 GB disk) is the build machine. No boards are attached to it.
- Toolchains live in `~/esp/esp-idf` and `~/esp/esp-matter`. Load them with `. ~/esp/esp-idf/export.sh && . ~/esp/esp-matter/export.sh`. esp-matter was installed with `--no-host-tool`.
- **Local patches** to the SDK, in `~/esp/esp-matter/connectedhomeip/connectedhomeip`. Re-apply them after any re-clone or submodule update:
  - `scripts/setup/constraints.txt`: `typing-extensions` is raised from 4.8.0 to 4.15.0. Without it, esp-matter's `install.sh` fails on Python 3.13.
  - `src/lib/shell/MainLoopESP32.cpp`: Matter's console task spun (and the task watchdog rebooted the S3 every ~30 s) whenever no USB host is on the S3's port, which is normal use on the 5V pin. `patches/connectedhomeip-shell-no-usb-host.patch`.
  - Apply it with the loop in `patches/README.md`. Only patches the project needs are kept (the user's rule).
- The OpenThread simulation (`ot-cli-ftd`, built from ESP-IDF's bundled OpenThread) is at `~/esp/ot-sim`.
- The boards plug into the user's Mac. To flash from the VM, either copy the binaries to the Mac and use `esptool`, or run `esp_rfc2217_server.py` on the Mac and use `idf.py -p rfc2217://<mac-ip>:4000 flash monitor`.
- Limit build parallelism if the build runs out of memory. Each compile job needs about 1–1.5 GB.
