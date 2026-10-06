# Controller firmware (S3)

This is the app firmware. It started as a copy of esp-matter `release/v1.6` `examples/controller` and runs fully offline: the S3 is the Thread leader, the SRP server, the DNS-SD server and the Matter commissioner, and has no Wi-Fi.

## How it fits together

| File | Does |
|---|---|
| `main/app_main.cpp` | Boot order: NVS → slots → LED task → console → radio watch → Matter → commissioner → Thread network → subscription tick. Matter's `kDnssdInitialized` event marks the network ready. |
| `main/app_state.*` | The one `sensor_mgr` (from `../components/app_core`), guarded by a mutex. Saves the slot table to NVS (`windows/slots`, plus the next node ID under `windows/next_node`). |
| `main/led_task.*` | 50 Hz loop: `sensor_mgr_led_state()` → `led_pattern_color()` → stick on GPIO13, sent by RMT with DMA. |
| `main/thread_network.*` | On first boot, scans all 16 channels and forms a new Thread network on the quietest (the dataset persists). Starts Thread and enables the SRP server. Turns the LEDs red and logs what to check if the H2 radio doesn't answer. |
| `main/sensor_link.*` | Everything Matter: commissioning, one `ReadClient` subscription per sensor, remove/unpair, Identify and factory reset. Its 1 s tick feeds the task watchdog. |
| `main/console_cmds.*` | Registers `pair`, `list`, `remove`, `identify`, `factory-reset confirm` with esp_console. |
| `paa_cert/`, `paa_cert.md` | Offline attestation trust store: every production PAA from the DCL, IKEA's included. |

### Threads and locking

Matter's shell runs every console command on the Matter thread, and all Matter callbacks and the 1 s subscription tick run there too. So `sensor_link` never needs the Matter stack lock. The LED task is the only other reader of `sensor_mgr`. The mutex in `app_state` is held only briefly, and **never across a Matter call**, because Matter may call back synchronously. Nothing prints while holding it, and the clock is read while holding it, so no event can be newer than a query's `now`.

### Startup

1. The LEDs show a dim white sweep from the moment the LED task starts (about a second after power-on).
2. On first boot only, the S3 listens on every channel for 300 ms (about 5 s in all) and forms the network on the quietest.
3. Thread comes up. In the simulation this took 7 s on the first boot and 15 s on a reboot, because a node with a saved network first looks for it before it takes over as leader.
4. The S3's own SRP client registers with its own SRP server. Matter then posts `kDnssdInitialized`, and `sensor_link_network_ready()` starts subscribing. Pairing is refused until then.
5. Each sensor shows dim white until its first report, then its real state. A sensor still silent 2 min after the network came up blinks blue.

If the network never comes up, the sweep keeps going, and after 60 s the log says which `ot_cli` commands to check. If the H2 doesn't answer, or runs `ot_rcp` from a different ESP-IDF, OpenThread stops the S3. `thread_network` first logs what to check and turns every LED red for 3 s, and then the S3 restarts. Expect a loop of sweep, red, reboot until the radio is fixed.

### Subscriptions

`sensor_link` uses its own `ReadClient::Callback` rather than esp-matter's `subscribe_command`. That class drops failures silently, never reports `OnError` or keep-alives, and gives up after 2 resubscribes. Each subscription:

- asks for BooleanState.StateValue on endpoint 1, with a max interval ceiling of 300 s (a sleepy sensor may choose longer);
- counts every report as a sign of life through `NotifySubscriptionStillActive`, including the empty keep-alives a closed window sends. `OnReportEnd` fires only for reports that carry data, so using it would mark quiet sensors dead every ~10 min;
- uses `KeepSubscriptions = false`, so a new subscription replaces any old one we left on the sensor;
- disables auto-resubscribe. `sensor_mgr` schedules retries with backoff and marks the slot "not heard from".

### Pairing

`pair <code>` reserves the lowest free slot and a new node ID, then calls `PairDevice()` with the manual code and the S3's Thread dataset. BLE discovery is handled by Matter's setup-code pairer. esp-matter's `pairing_code_thread()` isn't used: it swallows `PairDevice()` errors.

Our own `DevicePairingDelegate` ends the pairing on the first final callback:

| Callback | Means |
|---|---|
| `OnStatusUpdate(SecurePairingFailed)` | No sensor found over BLE, or PASE failed with every sensor found. This is the only report of a discovery failure. |
| `OnPairingComplete(error)` | PASE failed. |
| `OnCommissioningSuccess` / `OnCommissioningFailure` | The last callbacks after commissioning. `OnCommissioningComplete` isn't used: the SDK still calls the delegate right after it. |
| 5 min timeout | Anything else; `StopPairing()` is called. |

The delegate is never unregistered from a callback. The SDK calls it again afterwards, and the setup-code pairer swaps itself in and out around PASE. A failure from network setup onwards (for example `kFindOperationalForCommissioningComplete`, Risk 1) leaves the PASE session open for a retry, so it's released. If `CommissioningComplete` may already have reached the sensor, the sensor is also asked to leave our fabric.

### Factory reset

`factory-reset confirm` cancels any pairing, clears the slots and asks every sensor to leave our fabric. Sleepy sensors only answer when they wake, so it waits until every sensor has answered, or 2 min at most, before it erases NVS (Matter fabric, Thread dataset, slots) and reboots.

## Changes from the upstream example

| File | Change | Why |
|---|---|---|
| `sdkconfig.defaults` | Based on upstream `sdkconfig.defaults.otbr`, merged with its `esp32s3` settings. The other defaults files are removed. | One config; `idf.py build` works with no `-D` flags. |
| | `CONFIG_OPENTHREAD_SRP_CLIENT=y`, `CONFIG_OPENTHREAD_DNS_CLIENT=y` | Matter resolves sensors through Thread SRP/DNS-SD (`sim/README.md`). |
| | `CONFIG_ENABLE_WIFI_STATION=n` | Offline. |
| | `CONFIG_SPIRAM_MODE_OCT=y` (was QUAD) | The N8R8 board is expected to have octal PSRAM. **Check this on first boot.** |
| | `CONFIG_LWIP_NETIF_LOOPBACK=y`, set explicitly | The DNS query to the S3's own address goes through lwIP. |
| | `CONFIG_SPIFFS_ATTESTATION_TRUST_STORE=y`, `CONFIG_ESP_MATTER_COMMISSIONER_SUPPORT_TEST_CD=n` | Real attestation offline. Test certificates and test certification declarations are not accepted. |
| | `CONFIG_MAX_EXCHANGE_CONTEXTS=20` (was 8), plus `CHIP_CONFIG_UNAUTHENTICATED_CONNECTION_POOL_SIZE 10` (was 4) in `main/matter_project_config.h` | After a boot, all 8 sleepy sensors resubscribe at once, and each holds a CASE handshake and an exchange open for seconds. The defaults run out. |
| | `CONFIG_ESP_TASK_WDT_PANIC=y`, `CONFIG_ESP_TASK_WDT_TIMEOUT_S=30`, `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` | Unattended device: a hung Matter event loop restarts the S3, and the last crash is kept for `idf.py coredump-info`. |
| `partitions_br.csv` | NVS 64 K, app 3.5 M, `paa_cert` SPIFFS 256 K, `coredump` 128 K. `rcp_fw` removed. | Room to grow. There is no RCP auto-update. |
| `main/esp_ot_config.h` | RCP UART RX/TX on GPIO5/6 (was 17/18). RCP update config removed. | Our wiring. |
| `main/app_main.cpp` | Rewritten (see above). No Wi-Fi, no RCP update, no `esp_openthread_border_router_init()`. | No backbone. |
| `main/idf_component.yml`, `main/CMakeLists.txt` | Added `espressif/led_strip` 3.0.3 and `espressif/mdns` (pinned to ~1.13.1), and linked mdns from the `openthread` library. `esp_rcp_update` is kept only because Matter's `chip` component requires it in BR mode. | See the link-error note below. |

### Link error without Wi-Fi (ESP-IDF 5.5.5)

ESP-IDF's prebuilt `libopenthread_br.a` calls `mdns_service_add` and related functions to advertise the border agent whenever the node gets a role. The `openthread` component only declares mdns as a dependency when TREL is enabled. Matter's `chip` component does depend on mdns, but with Wi-Fi off nothing references it before the openthread archive in the link order. The link then fails with `undefined reference to mdns_service_add`.

The fix links mdns but never calls `mdns_init()`. Every call from the border router library then returns `ESP_ERR_INVALID_STATE` and does nothing (checked in mdns 1.13.1, `mdns_responder.c`). Expect a harmless error log about this on the S3 when it becomes leader.

## Build

```bash
. ~/esp/esp-idf/export.sh && . ~/esp/esp-matter/export.sh
cd firmware/controller
idf.py set-target esp32s3   # first time only
idf.py build
```

Build with 3 or fewer jobs on the VM (`export CMAKE_BUILD_PARALLEL_LEVEL=3`). After editing `sdkconfig.defaults`, delete `sdkconfig` so the new defaults apply.

## Console

```
pair 3497-011-2332       pair a sensor (it must be in pairing mode) into the lowest free slot
list                     all 8 slots: state, node ID, last heard / retry timing
remove 3                 free slot 3 and ask the sensor to leave our fabric
identify 3               make sensor 3 blink (if it supports the Identify cluster)
factory-reset confirm    unpair everything, erase all settings, reboot
help                     every command, including `matter ...` for debugging
```

If `remove` or `factory-reset` can't reach a sensor (asleep, out of range, battery dead), the log says so and that sensor keeps our old fabric. Factory reset it by hand before pairing it again. `identify` only reports whether the command could be sent; a sensor that can't be reached shows up in the log.

Plug the console's USB cable in at the board end, through a hub or adapter that stays in the Mac. Then the supply and the cable can go on and off in any order (see Power in `CLAUDE.md`).

## Phase 2 bring-up on the S3

On first boot the S3 forms its own Thread network. Check it with:

```
matter esp ot_cli state             # -> leader
matter esp ot_cli channel           # the channel the energy scan picked (also in the boot log)
matter esp ot_cli srp server state  # -> running
matter esp ot_cli srp client state  # -> Enabled; `srp client server` should be this node's own address
matter esp ot_cli dns config        # Server should be this node's ML-EID, port 53
```

The log should show `Thread network and lookup service are up`, and the LEDs should stop sweeping. Also check once whether a direct USB-C cable from the Mac attaches while the supply is already on.

Then `pair` one MYGGBETT and watch the log for the attestation and subscription results. A healthy pairing logs `BLE GAP connection established` and `GATT discovery complete` right after `exchange mtu`. The build needs the console patch in `../../patches` applied to the SDK; without it the S3 reboots every ~30 s whenever no computer is on its USB port. Also run the checks that the simulation couldn't; see "What the simulation does not prove" in `sim/README.md`.
