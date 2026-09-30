#pragma once

// App state without Matter: slots, pairing, subscription health and what each LED should show.
// The Matter glue feeds it events and asks it which sensor to (re)subscribe next.
// All times are a free-running uint32 millisecond clock; wraparound is handled, and an event stamped slightly
// after the `now` of a query counts as "just now".

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "backoff.h"
#include "led_pattern.h"
#include "slot_table.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_RESUBSCRIBE_BASE_MS   10000u
#define SENSOR_RESUBSCRIBE_MAX_MS    (5u * 60u * 1000u)
// A sensor without a working subscription for this long shows "not heard from". After boot the clock starts when
// the network comes up.
#define SENSOR_UNREACHABLE_AFTER_MS  (2u * 60u * 1000u)
// A subscription with no report for 2 x max interval + this margin is treated as dead.
#define SENSOR_STALE_MARGIN_MS       30000u
// A subscribe attempt Matter never reported back on (neither established nor down) is abandoned and retried.
#define SENSOR_ATTEMPT_TIMEOUT_MS    (5u * 60u * 1000u)
// "Down for" stops counting here, so a sensor that stays down past the 49.7-day clock wrap keeps blinking blue.
#define SENSOR_DOWN_CLAMP_MS         (20u * 24u * 3600u * 1000u)

typedef enum {
    CONTACT_UNKNOWN,
    CONTACT_OPEN,
    CONTACT_CLOSED,
} contact_t;

typedef struct {
    contact_t contact;
    bool subscribed;
    bool attempt_in_flight;
    bool heard;               // last_heard_ms is valid
    uint32_t last_heard_ms;
    uint32_t down_since_ms;      // valid while !subscribed
    uint32_t next_attempt_ms;    // valid while !subscribed && !attempt_in_flight
    uint32_t attempt_started_ms; // valid while attempt_in_flight
    uint32_t max_interval_ms;    // negotiated subscription max interval
    backoff_t backoff;
} sensor_state_t;

typedef struct {
    slot_table_t table;
    sensor_state_t sensor[SLOT_COUNT];
    int pairing_slot; // -1 when not pairing
    uint64_t pairing_node_id;
    bool network_ready; // Thread is up and Matter can look sensors up; nothing is subscribed before this
    bool radio_failed;  // the Thread radio (H2) stopped answering; the S3 is about to restart
} sensor_mgr_t;

typedef enum {
    SENSOR_MGR_OK,
    SENSOR_MGR_BUSY,       // a pairing is already in progress
    SENSOR_MGR_FULL,       // all slots are used
    SENSOR_MGR_NOT_PAIRED, // the slot is empty
    SENSOR_MGR_BAD_SLOT,
    SENSOR_MGR_NOT_READY,  // the Thread network isn't up yet
} sensor_mgr_err_t;

// Starts from a loaded (or freshly initialised) table. Nothing is subscribed until sensor_mgr_set_network_ready().
void sensor_mgr_init(sensor_mgr_t *m, const slot_table_t *table, uint32_t now_ms);

// Thread is up and Matter's lookup service is ready (DNS-SD initialised). Every paired sensor without a subscription
// becomes due at once and gets a fresh "not heard from" grace period, so a slow boot doesn't count against it.
void sensor_mgr_set_network_ready(sensor_mgr_t *m, uint32_t now_ms);
// The Thread radio stopped answering. Every LED shows it until the S3 restarts.
void sensor_mgr_set_radio_failed(sensor_mgr_t *m);

// Reserves the lowest free slot and a new node ID for commissioning. Persist m->table afterwards.
// Refuses with SENSOR_MGR_NOT_READY before the network is up.
sensor_mgr_err_t sensor_mgr_begin_pairing(sensor_mgr_t *m, int *slot, uint64_t *node_id);
// On success the slot becomes owned by the new node and is due for a subscribe. Persist m->table afterwards.
void sensor_mgr_end_pairing(sensor_mgr_t *m, bool success, uint32_t now_ms);

// Frees a slot. *node_id tells the glue which node to drop subscriptions for and unpair. Persist afterwards.
sensor_mgr_err_t sensor_mgr_remove(sensor_mgr_t *m, int slot, uint64_t *node_id);
// Empties every slot and cancels pairing. Node IDs keep counting up. Persist afterwards.
void sensor_mgr_clear_all(sensor_mgr_t *m);

// Returns a slot the glue should subscribe now (dropping any subscription it still holds for it), or -1.
// Marks the slot as in flight; call repeatedly until -1. Also detects silent subscriptions and attempts that never
// finished, and returns them. Call at least once a day (the glue's 1 s tick does); it also keeps the clock clamps
// fresh. Returns -1 until the network is ready.
int sensor_mgr_next_subscribe(sensor_mgr_t *m, uint32_t now_ms);

// Subscription established (Matter's OnSubscriptionEstablished).
void sensor_mgr_on_subscribed(sensor_mgr_t *m, int slot, uint32_t max_interval_s, uint32_t now_ms);
// The subscription is alive: any report arrived, including empty keep-alives (NotifySubscriptionStillActive).
void sensor_mgr_on_heard(sensor_mgr_t *m, int slot, uint32_t now_ms);
// BooleanState.StateValue report. MYGGBETT: true = closed, false = open.
void sensor_mgr_on_state_value(sensor_mgr_t *m, int slot, bool state_value, uint32_t now_ms);
// Subscription lost, or a subscribe attempt failed. Schedules the next attempt with backoff.
// A second report for the same drop (nothing subscribed or in flight) is ignored.
void sensor_mgr_on_subscription_down(sensor_mgr_t *m, int slot, uint32_t now_ms, uint32_t random);

bool sensor_mgr_is_unreachable(const sensor_mgr_t *m, int slot, uint32_t now_ms);
// Until the network is up every LED shows LED_SLOT_STARTING (except a pairing slot). After that, a paired sensor
// that hasn't reported since boot or pairing shows LED_SLOT_WAITING until it reports or becomes unreachable.
led_slot_state_t sensor_mgr_led_state(const sensor_mgr_t *m, int slot, uint32_t now_ms);

// One line for the `list` command, e.g. "3  open     node 0x102  heard 12s ago". Slot numbers print 1-based.
void sensor_mgr_format_slot(const sensor_mgr_t *m, int slot, uint32_t now_ms, char *buf, size_t len);

#ifdef __cplusplus
}
#endif
