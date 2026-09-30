# Phase 1: offline lookup in the OpenThread simulation

`phase1_offline_lookup.py` answers the Phase 1 go/no-go question: can the S3, with no Wi-Fi and no mDNS, find a sleepy sensor's Matter service using only its own SRP server and DNS-SD server?

**Result (2026-09-30): yes.** 17 of 17 checks passed in each of 4 runs.

## Run it

```bash
# once: build the simulation from the OpenThread that ships with ESP-IDF v5.5.5
cd ~/esp/esp-idf/components/openthread/openthread
OT_CMAKE_NINJA_TARGET=ot-cli-ftd OT_CMAKE_BUILD_DIR=$HOME/esp/ot-sim ./script/cmake-build simulation

# the test (about 1 minute; each node's full console goes to sim/logs/)
python3 sim/phase1_offline_lookup.py
```

## What it does

Two simulated nodes. **controller** plays the S3 and **sensor** plays a MYGGBETT.

1. The controller forms a network, becomes leader and runs `srp server enable`.
2. The controller's own SRP client registers its Matter operational record with its own server, the way Matter's SRP client will.
3. The sensor joins as a **sleepy end device** (`mode -`, 2 s poll) and registers `<fabric>-<node>._matter._tcp` over SRP. The record has an `_I<fabric>` subtype and ICD TXT keys.
4. The controller resolves the sensor through its own DNS-SD server with the **default** DNS config, which is what Matter does. It checks `dns service`, `dns resolve`, both kinds of browse, and a not-found control.
5. The controller is killed and restarted from its saved settings, like the S3 losing power. The sensor must re-register, and the lookups are repeated.

## Findings

- **No DNS server config is needed.** OpenThread (`DEFAULT_SERVER_ADDRESS_AUTO_SET`) and Matter both set the DNS client's default server to whatever SRP server the SRP client auto-selects. On the S3, that is its own ML-EID on port 53. See Matter's `GenericThreadStackManagerImpl_OpenThread.hpp`, `OnSrpClientStateChange`.
- **A query to the controller's own address works.** The controller's DNS client reaches its own DNS-SD server, and the controller's SRP client reaches its own SRP server.
- **One round trip is enough.** The DNS-SD server includes the host's AAAA record in the SRV/TXT answer, so Matter's `ResolveService` gets the address without a second `ResolveAddress` query.
- **The sensor re-registers after a controller reboot.** SRP registrations are not persisted. But the SRP server moves to the next port on each start (53536 → 53537), so the sensor's SRP client sees a new server and re-registers within 1–8 s instead of waiting for its 2 h lease.

## What the simulation does not prove (check in Phase 2 on the S3)

1. **The lwIP hop.** On the ESP, the DNS client socket and the DNS-SD server socket are both bound to "any interface". Because of that, `PLATFORM_UDP` sends them through lwIP instead of OpenThread's internal UDP. A query to the S3's own ML-EID therefore relies on lwIP's per-interface loopback (`CONFIG_LWIP_NETIF_LOOPBACK=y`, which the controller build has by default). The simulation has no lwIP.
   - If this fails, the fix is `OPENTHREAD_CONFIG_DNS_CLIENT_BIND_UDP_TO_THREAD_NETIF=1`, or the SRP-table resolver fallback.
2. **Border router mode without Wi-Fi.** ESP-IDF only compiles the SRP server and DNS-SD server into OpenThread under `CONFIG_OPENTHREAD_BORDER_ROUTER`. That option needs `OPENTHREAD_PLATFORM_NETIF` and also enables border routing, which expects a backbone interface.
   - **It builds** (2026-09-30, `firmware/controller`). This needed one link fix; see that README.
   - Still to check on the S3: it boots and runs the SRP server with no backbone set up.
3. **The real MYGGBETT's SRP client.** The simulated sensor is OpenThread's. IKEA's firmware may pick a different lease or behave differently after the controller reboots.

## Config the S3 firmware will need (from this phase)

| Setting | Why |
|---|---|
| `CONFIG_OPENTHREAD_SRP_CLIENT=y` | `sdkconfig.defaults.otbr` sets it to `n`. It also switches on Matter's `CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT`. |
| `CONFIG_OPENTHREAD_DNS_CLIENT=y` | Same, for `..._THREAD_DNS_CLIENT`. Without both, Matter's ESP32 `DnssdImpl.cpp` has no Thread resolve path. |
| `CONFIG_OPENTHREAD_BORDER_ROUTER=y` | Needed for the SRP and DNS-SD servers to be compiled in. |
| `CONFIG_LWIP_NETIF_LOOPBACK=y` | Already the default. Keep it (finding 1 above). |
| Wi-Fi station off | Otherwise Matter also tries mDNS over Wi-Fi. |
| `otSrpServerSetEnabled(true)` at boot | The server is off until enabled. It keeps the default unicast address mode. |
