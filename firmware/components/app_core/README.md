# app_core

The app's logic, free of Matter and ESP-IDF, so it runs and is tested on the host:

```bash
make -C firmware/components/app_core/test   # ASan + UBSan, ~35k checks
```

| Module | Does |
|---|---|
| `pairing_code` | Parses the 11-digit manual code (Matter Core spec 5.1.4): Verhoeff check digit, passcode and short discriminator, and rejection of 21-digit codes, a first digit of 8 or 9, and disallowed passcodes. The tests check it against codes built with the Matter SDK's own Verhoeff algorithm. |
| `slot_table` | 8 slots, lowest free slot first, node IDs never reused (start at `0x100`). Fixed 84-byte versioned encoding for NVS; the decoder rejects corrupt blobs, including an unusable next node ID. |
| `backoff` | Exponential backoff with ±20% jitter and no retry limit. |
| `sensor_mgr` | Network readiness, pairing reservations, per-sensor subscription health, "not heard from", the LED state for each slot, and `list` output lines. |
| `command` | Console argv → command. Slots are **1–8** on the console, which is LED 1 (DIN end) to LED 8. |
| `thread_channel` | Picks the channel for a new Thread network from an energy scan: the quietest, preferring 15, 20 or 25 (between Wi-Fi channels 1, 6 and 11) when they're within 3 dB of it. |

## Timing choices (`sensor_mgr.h`)

| Setting | Value | Meaning |
|---|---|---|
| `SENSOR_RESUBSCRIBE_BASE_MS` / `_MAX_MS` | 10 s → 5 min | Resubscribe backoff, reset on success. |
| `SENSOR_UNREACHABLE_AFTER_MS` | 2 min | How long a sensor can be without a subscription before its LED blinks blue. After boot it counts from when the network came up. |
| `SENSOR_STALE_MARGIN_MS` | 30 s | A subscription silent for 2 × max interval + 30 s is treated as dead and resubscribed. Matter's own liveness timeout normally catches this first. |
| `SENSOR_ATTEMPT_TIMEOUT_MS` | 5 min | A subscribe attempt that Matter never reported back on is abandoned and handed out again. It shouldn't happen, but otherwise the slot would be stuck for good. |
| `SENSOR_DOWN_CLAMP_MS` | 20 days | "Down for" stops counting here, so a sensor that stays down past the 49.7-day clock wrap keeps blinking blue. `list` shows at most `down 480h`. |

## Startup

Until the glue calls `sensor_mgr_set_network_ready()`, nothing is subscribed, pairing is refused, nothing counts as unreachable, and every LED shows `LED_SLOT_STARTING` (the sweep). After that, a paired sensor shows `LED_SLOT_WAITING` (dim white) until its first `StateValue` arrives, since boot or since it was paired, or until it becomes unreachable. `sensor_mgr_set_radio_failed()` turns every LED red for good; the S3 restarts right after.

## Grace periods

When a subscription drops, the sensor gets a new 2-minute grace period, even if its last report was long ago. Otherwise a sleepy sensor with a long max interval would flash blue at every routine resubscribe. A subscription that went silent past the stale limit stays blue until it resubscribes.

## Contract for the Matter glue

- Call `sensor_mgr_set_network_ready()` on Matter's `kDnssdInitialized` event. Matter can't resolve a sensor before it.
- Call `sensor_mgr_next_subscribe()` in a loop until it returns -1, at least once a day (the glue's 1 s tick is plenty). For each slot it returns, drop any subscription or attempt you still hold and start a new one, without Matter's auto-resubscribe.
- Read the clock while holding the lock that guards `sensor_mgr_t`. An event stamped a moment after a query's `now` counts as "just now", but the timing is only exact this way.
- Feed in the Matter callbacks:
  - `on_subscribed` ← `OnSubscriptionEstablished`
  - `on_heard` ← `NotifySubscriptionStillActive`. It fires for every report, including the empty keep-alives a closed window sends each max interval. `OnReportEnd` doesn't: it only fires for reports that carry data.
  - `on_state_value` ← BooleanState `StateValue`
  - `on_subscription_down` ← `OnError` / `OnDone` or a failed attempt
- Map a callback to its slot with `slot_table_find_node()`. Events for a removed node give -1 and are ignored.
- Persist `m->table` after `begin_pairing` (a node ID was used up), `end_pairing`, `remove` and `clear_all`. The glue also keeps `next_node_id` under its own NVS key, so an unreadable table can't restart node IDs at `0x100` while old sensors still use them.
