# SDK patches

Local fixes to the toolchain in `~/esp`. They live outside this repo, so re-apply them after any re-clone or submodule update of esp-matter.

| Patch | Applies to | Fixes |
|---|---|---|
| `connectedhomeip-ble-mtu-ealready.patch` | `~/esp/esp-matter/connectedhomeip/connectedhomeip` | IKEA (and Aqara) Thread devices failing to pair over BLE with `Disabling CHIPoBLE service due to error: ac` (esp-matter #1772, #1532). |

```bash
git -C ~/esp/esp-matter/connectedhomeip/connectedhomeip apply ~/glugg/patches/connectedhomeip-ble-mtu-ealready.patch
```

Check whether it's applied with `git apply --check -R` and the same paths: no output means it is.

## connectedhomeip-ble-mtu-ealready.patch

As the BLE central, ESP-IDF's NimBLE reports a new connection only after it has read the peer's supported features and version, two round trips over the air (`ble_gap.c`, `ble_gap_rx_rd_rem_ver_info_complete`). A peripheral that starts its own ATT MTU exchange as soon as the link is up has finished it by then, so NimBLE marks the MTU as exchanged (`ble_att_svr.c`, `BLE_L2CAP_CHAN_F_TXED_MTU`).

Matter's `BLEManagerImpl::HandleGAPConnect` then starts its own exchange, gets `BLE_HS_EALREADY`, and turns it into `CHIP_ERROR_INTERNAL` (`0xac`) before GATT discovery starts. CHIPoBLE is disabled and the link dropped. chip-tool on Linux pairs the same devices because BlueZ accepts the MTU the peer already negotiated.

The patch treats `BLE_HS_EALREADY` as success, only exchanges the MTU on a successful connection, and logs any other error code.

**Status:** the diagnosis is from the source and the logs in the two issues. It is unconfirmed until a MYGGBETT pairs in Phase 2. If pairing still fails, the log now shows `BLE MTU exchange failed: <code>` with the real NimBLE error. Upstream `master` still has the original code (checked 2026-10-05).
