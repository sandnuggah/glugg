#include "sensor_mgr.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

// Time since `then`. An event stamped just after `now` (the glue read its clock first) counts as 0, not ~49 days.
static uint32_t elapsed(uint32_t now_ms, uint32_t then_ms)
{
    int32_t d = (int32_t)(now_ms - then_ms);
    return d < 0 ? 0 : (uint32_t)d;
}

static bool valid_slot(int slot)
{
    return slot >= 0 && slot < SLOT_COUNT;
}

// Events can still arrive for a sensor that was just removed; they are ignored.
static sensor_state_t *paired(sensor_mgr_t *m, int slot)
{
    return valid_slot(slot) && m->table.slot[slot].used ? &m->sensor[slot] : NULL;
}

static void reset_sensor(sensor_state_t *s, uint32_t now_ms)
{
    memset(s, 0, sizeof(*s));
    s->contact = CONTACT_UNKNOWN;
    s->down_since_ms = now_ms;
    s->next_attempt_ms = now_ms;
    backoff_init(&s->backoff, SENSOR_RESUBSCRIBE_BASE_MS, SENSOR_RESUBSCRIBE_MAX_MS);
}

void sensor_mgr_init(sensor_mgr_t *m, const slot_table_t *table, uint32_t now_ms)
{
    m->table = *table;
    m->pairing_slot = -1;
    m->pairing_node_id = 0;
    m->network_ready = false;
    m->radio_failed = false;
    for (int i = 0; i < SLOT_COUNT; i++) {
        reset_sensor(&m->sensor[i], now_ms);
    }
}

void sensor_mgr_set_network_ready(sensor_mgr_t *m, uint32_t now_ms)
{
    if (m->network_ready) {
        return;
    }
    m->network_ready = true;
    for (int i = 0; i < SLOT_COUNT; i++) {
        sensor_state_t *s = &m->sensor[i];
        if (!s->subscribed && !s->attempt_in_flight) {
            s->down_since_ms = now_ms;
            s->next_attempt_ms = now_ms;
            backoff_reset(&s->backoff);
        }
    }
}

void sensor_mgr_set_radio_failed(sensor_mgr_t *m)
{
    m->radio_failed = true;
}

sensor_mgr_err_t sensor_mgr_begin_pairing(sensor_mgr_t *m, int *slot, uint64_t *node_id)
{
    if (!m->network_ready) {
        return SENSOR_MGR_NOT_READY;
    }
    if (m->pairing_slot >= 0) {
        return SENSOR_MGR_BUSY;
    }
    int free_slot = slot_table_lowest_free(&m->table);
    if (free_slot < 0) {
        return SENSOR_MGR_FULL;
    }
    m->pairing_slot = free_slot;
    m->pairing_node_id = slot_table_take_node_id(&m->table);
    *slot = free_slot;
    *node_id = m->pairing_node_id;
    return SENSOR_MGR_OK;
}

void sensor_mgr_end_pairing(sensor_mgr_t *m, bool success, uint32_t now_ms)
{
    if (m->pairing_slot < 0) {
        return;
    }
    if (success) {
        slot_table_assign(&m->table, m->pairing_slot, m->pairing_node_id);
        reset_sensor(&m->sensor[m->pairing_slot], now_ms);
    }
    m->pairing_slot = -1;
    m->pairing_node_id = 0;
}

sensor_mgr_err_t sensor_mgr_remove(sensor_mgr_t *m, int slot, uint64_t *node_id)
{
    if (!valid_slot(slot)) {
        return SENSOR_MGR_BAD_SLOT;
    }
    if (!m->table.slot[slot].used) {
        return SENSOR_MGR_NOT_PAIRED;
    }
    *node_id = m->table.slot[slot].node_id;
    slot_table_clear(&m->table, slot);
    return SENSOR_MGR_OK;
}

void sensor_mgr_clear_all(sensor_mgr_t *m)
{
    for (int i = 0; i < SLOT_COUNT; i++) {
        slot_table_clear(&m->table, i);
    }
    m->pairing_slot = -1;
    m->pairing_node_id = 0;
}

static bool is_stale(const sensor_state_t *s, uint32_t now_ms)
{
    uint32_t limit = 2 * s->max_interval_ms + SENSOR_STALE_MARGIN_MS;
    return s->subscribed && s->heard && elapsed(now_ms, s->last_heard_ms) > limit;
}

static int start_attempt(sensor_state_t *s, int slot, uint32_t now_ms)
{
    s->attempt_in_flight = true;
    s->attempt_started_ms = now_ms;
    return slot;
}

int sensor_mgr_next_subscribe(sensor_mgr_t *m, uint32_t now_ms)
{
    if (!m->network_ready) {
        return -1; // Matter can't look sensors up yet, so every attempt would fail and back off
    }
    for (int i = 0; i < SLOT_COUNT; i++) {
        sensor_state_t *s = paired(m, i);
        if (!s) {
            continue;
        }
        if (!s->subscribed && elapsed(now_ms, s->down_since_ms) > SENSOR_DOWN_CLAMP_MS) {
            s->down_since_ms = now_ms - SENSOR_DOWN_CLAMP_MS;
        }
        if (s->attempt_in_flight) {
            if (elapsed(now_ms, s->attempt_started_ms) >= SENSOR_ATTEMPT_TIMEOUT_MS) {
                // Matter lost track of it; the glue replaces the stuck attempt with a new one.
                return start_attempt(s, i, now_ms);
            }
            continue;
        }
        if (is_stale(s, now_ms)) {
            // Already silent for 2 x max interval, so it stays "not heard from" without a new grace period.
            s->subscribed = false;
            s->down_since_ms = now_ms - SENSOR_UNREACHABLE_AFTER_MS;
            return start_attempt(s, i, now_ms);
        }
        if (!s->subscribed && reached(now_ms, s->next_attempt_ms)) {
            return start_attempt(s, i, now_ms);
        }
    }
    return -1;
}

void sensor_mgr_on_subscribed(sensor_mgr_t *m, int slot, uint32_t max_interval_s, uint32_t now_ms)
{
    sensor_state_t *s = paired(m, slot);
    if (!s) {
        return;
    }
    s->subscribed = true;
    s->attempt_in_flight = false;
    s->max_interval_ms = max_interval_s * 1000u;
    s->heard = true;
    s->last_heard_ms = now_ms;
    backoff_reset(&s->backoff);
}

void sensor_mgr_on_heard(sensor_mgr_t *m, int slot, uint32_t now_ms)
{
    sensor_state_t *s = paired(m, slot);
    if (!s) {
        return;
    }
    s->heard = true;
    s->last_heard_ms = now_ms;
}

void sensor_mgr_on_state_value(sensor_mgr_t *m, int slot, bool state_value, uint32_t now_ms)
{
    sensor_state_t *s = paired(m, slot);
    if (!s) {
        return;
    }
    s->contact = state_value ? CONTACT_CLOSED : CONTACT_OPEN;
    s->heard = true;
    s->last_heard_ms = now_ms;
}

void sensor_mgr_on_subscription_down(sensor_mgr_t *m, int slot, uint32_t now_ms, uint32_t random)
{
    sensor_state_t *s = paired(m, slot);
    if (!s || (!s->subscribed && !s->attempt_in_flight)) {
        return;
    }
    if (s->subscribed) {
        s->down_since_ms = now_ms;
    }
    s->subscribed = false;
    s->attempt_in_flight = false;
    s->next_attempt_ms = now_ms + backoff_next(&s->backoff, random);
}

bool sensor_mgr_is_unreachable(const sensor_mgr_t *m, int slot, uint32_t now_ms)
{
    if (!m->network_ready || !valid_slot(slot) || !m->table.slot[slot].used) {
        return false;
    }
    const sensor_state_t *s = &m->sensor[slot];
    if (s->subscribed) {
        return is_stale(s, now_ms);
    }
    return elapsed(now_ms, s->down_since_ms) >= SENSOR_UNREACHABLE_AFTER_MS;
}

led_slot_state_t sensor_mgr_led_state(const sensor_mgr_t *m, int slot, uint32_t now_ms)
{
    if (m->radio_failed) {
        return LED_SLOT_RADIO_FAILED;
    }
    if (valid_slot(slot) && slot == m->pairing_slot) {
        return LED_SLOT_PAIRING;
    }
    if (!m->network_ready) {
        return LED_SLOT_STARTING;
    }
    if (!valid_slot(slot) || !m->table.slot[slot].used) {
        return LED_SLOT_EMPTY;
    }
    if (sensor_mgr_is_unreachable(m, slot, now_ms)) {
        return LED_SLOT_UNREACHABLE;
    }
    switch (m->sensor[slot].contact) {
    case CONTACT_OPEN:
        return LED_SLOT_OPEN;
    case CONTACT_CLOSED:
        return LED_SLOT_CLOSED;
    case CONTACT_UNKNOWN:
    default:
        return LED_SLOT_WAITING;
    }
}

static void fmt_duration(char *buf, size_t len, uint32_t ms)
{
    uint32_t s = ms / 1000;
    if (s < 120) {
        snprintf(buf, len, "%" PRIu32 "s", s);
    } else if (s < 2 * 3600) {
        snprintf(buf, len, "%" PRIu32 "m", s / 60);
    } else {
        snprintf(buf, len, "%" PRIu32 "h", s / 3600);
    }
}

void sensor_mgr_format_slot(const sensor_mgr_t *m, int slot, uint32_t now_ms, char *buf, size_t len)
{
    if (valid_slot(slot) && slot == m->pairing_slot) {
        snprintf(buf, len, "%d  pairing  node 0x%" PRIx64, slot + 1, m->pairing_node_id);
        return;
    }
    if (!valid_slot(slot) || !m->table.slot[slot].used) {
        snprintf(buf, len, "%d  empty", slot + 1);
        return;
    }
    const sensor_state_t *s = &m->sensor[slot];
    const char *state = sensor_mgr_is_unreachable(m, slot, now_ms) ? "LOST"
                        : s->contact == CONTACT_OPEN                ? "open"
                        : s->contact == CONTACT_CLOSED              ? "closed"
                                                                    : "waiting";
    char d1[16], d2[16];
    char detail[64];
    if (!m->network_ready) {
        snprintf(detail, sizeof(detail), "waiting for the Thread network");
    } else if (s->subscribed) {
        fmt_duration(d1, sizeof(d1), elapsed(now_ms, s->last_heard_ms));
        snprintf(detail, sizeof(detail), "heard %s ago", d1);
    } else if (s->attempt_in_flight) {
        fmt_duration(d1, sizeof(d1), elapsed(now_ms, s->down_since_ms));
        snprintf(detail, sizeof(detail), "subscribing (down %s)", d1);
    } else {
        fmt_duration(d1, sizeof(d1), elapsed(now_ms, s->down_since_ms));
        fmt_duration(d2, sizeof(d2), reached(now_ms, s->next_attempt_ms) ? 0 : s->next_attempt_ms - now_ms);
        snprintf(detail, sizeof(detail), "down %s, retry in %s", d1, d2);
    }
    snprintf(buf, len, "%d  %-7s  node 0x%" PRIx64 "  %s", slot + 1, state, m->table.slot[slot].node_id, detail);
}
