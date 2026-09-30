// Host unit tests for app_core: `make -C firmware/components/app_core/test`
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backoff.h"
#include "command.h"
#include "pairing_code.h"
#include "sensor_mgr.h"
#include "slot_table.h"
#include "thread_channel.h"

static int s_failures;
static int s_checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        s_checks++;                                                          \
        if (!(cond)) {                                                       \
            s_failures++;                                                    \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

// ---------------------------------------------------------------------------------------------------------------
// pairing_code

// Appends the Verhoeff check digit the way the Matter SDK does (lib/support/verhoeff/Verhoeff10.cpp): only the
// base permutation, applied i times, so it doesn't share pairing_code.c's precomputed table.
static void complete_code(const char *first10, char out[12])
{
    static const uint8_t mul[10][10] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 4, 0, 6, 7, 8, 9, 5}, {2, 3, 4, 0, 1, 7, 8, 9, 5, 6},
        {3, 4, 0, 1, 2, 8, 9, 5, 6, 7}, {4, 0, 1, 2, 3, 9, 5, 6, 7, 8}, {5, 9, 8, 7, 6, 0, 4, 3, 2, 1},
        {6, 5, 9, 8, 7, 1, 0, 4, 3, 2}, {7, 6, 5, 9, 8, 2, 1, 0, 4, 3}, {8, 7, 6, 5, 9, 3, 2, 1, 0, 4},
        {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    };
    static const uint8_t perm[10] = {1, 5, 7, 6, 2, 8, 3, 0, 9, 4};
    int c = 0;
    for (int i = 1; i <= 10; i++) {
        int v = first10[10 - i] - '0';
        for (int k = 0; k < i; k++) {
            v = perm[v];
        }
        c = mul[c][v];
    }
    c = (c > 0 && c < 5) ? 5 - c : c; // inverse in D5
    snprintf(out, 12, "%s%c", first10, '0' + c);
}

// Encodes per Matter Core spec 5.1.4.1 (without the check digit).
static void encode_first10(uint32_t passcode, uint8_t short_disc, char out[11])
{
    unsigned chunk1 = short_disc >> 2;
    unsigned chunk2 = ((short_disc & 0x3u) << 14) | (passcode & 0x3FFFu);
    unsigned chunk3 = passcode >> 14;
    char buf[32];
    snprintf(buf, sizeof(buf), "%01u%05u%04u", chunk1, chunk2, chunk3);
    memcpy(out, buf, 10);
    out[10] = '\0';
}

static void test_pairing_code(void)
{
    pairing_code_t pc;

    // Matter SDK test device: passcode 20202021, discriminator 3840 (short 0xF) -> 34970112332.
    CHECK(pairing_code_parse("34970112332", &pc) == PAIRING_CODE_OK);
    CHECK(pc.passcode == 20202021);
    CHECK(pc.short_discriminator == 0xF);
    CHECK(strcmp(pc.digits, "34970112332") == 0);
    // More codes generated with the SDK's algorithm, including 7th digits of 6 (passcodes >= 98304000).
    CHECK(pairing_code_parse("00787260423", &pc) == PAIRING_CODE_OK && pc.passcode == 99000000);
    CHECK(pairing_code_parse("35759861036", &pc) == PAIRING_CODE_OK && pc.passcode == 99999998);
    CHECK(pc.short_discriminator == 15);
    CHECK(pairing_code_parse("11638460003", &pc) == PAIRING_CODE_OK && pc.passcode == 98304000);
    CHECK(pc.short_discriminator == 5);
    CHECK(pairing_code_parse("00000100007", &pc) == PAIRING_CODE_OK && pc.passcode == 1);
    CHECK(pairing_code_parse("22491107535", &pc) == PAIRING_CODE_OK && pc.passcode == 12345679);
    CHECK(pc.short_discriminator == 9);
    CHECK(pairing_code_parse("3497-011-2332", &pc) == PAIRING_CODE_OK && pc.passcode == 20202021);
    CHECK(pairing_code_parse("3497 011 2332", &pc) == PAIRING_CODE_OK && strcmp(pc.digits, "34970112332") == 0);

    CHECK(pairing_code_parse("", &pc) == PAIRING_CODE_BAD_LENGTH);
    CHECK(pairing_code_parse("3497011233", &pc) == PAIRING_CODE_BAD_LENGTH);
    CHECK(pairing_code_parse("349701123320", &pc) == PAIRING_CODE_BAD_LENGTH);
    CHECK(pairing_code_parse("3497O112332", &pc) == PAIRING_CODE_BAD_CHAR);
    CHECK(pairing_code_parse("MT:Y.K9042C00KA0648G00", &pc) == PAIRING_CODE_BAD_CHAR);

    // Verhoeff catches every single-digit error and every adjacent transposition.
    const char *good = "34970112332";
    for (int pos = 0; pos < 11; pos++) {
        for (char c = '0'; c <= '9'; c++) {
            if (c == good[pos]) {
                continue;
            }
            char typo[12];
            memcpy(typo, good, 12);
            typo[pos] = c;
            CHECK(pairing_code_parse(typo, &pc) == PAIRING_CODE_BAD_CHECK);
        }
        if (pos < 10 && good[pos] != good[pos + 1]) {
            char swapped[12];
            memcpy(swapped, good, 12);
            swapped[pos] = good[pos + 1];
            swapped[pos + 1] = good[pos];
            CHECK(pairing_code_parse(swapped, &pc) == PAIRING_CODE_BAD_CHECK);
        }
    }

    // Round trip over a spread of passcodes and discriminators, with extra weight on the top of the range.
    srand(1);
    for (int i = 0; i < 4000; i++) {
        uint32_t passcode = i % 2 ? 98000000u + (uint32_t)rand() % 1999999u : 1 + (uint32_t)rand() % 99999998u;
        uint8_t disc = (uint8_t)(rand() % 16);
        if (passcode == 11111111 || passcode == 22222222 || passcode == 33333333 || passcode == 44444444 ||
            passcode == 55555555 || passcode == 66666666 || passcode == 77777777 || passcode == 88888888 ||
            passcode == 12345678 || passcode == 87654321) {
            continue;
        }
        char first10[11], code[12];
        encode_first10(passcode, disc, first10);
        complete_code(first10, code);
        CHECK(pairing_code_parse(code, &pc) == PAIRING_CODE_OK);
        CHECK(pc.passcode == passcode && pc.short_discriminator == disc);
    }

    char code[12];
    complete_code("4497011233", code); // first digit 4: VID/PID flag set
    CHECK(pairing_code_parse(code, &pc) == PAIRING_CODE_LONG_FORMAT);
    char first10[11];
    encode_first10(11111111, 3, first10);
    complete_code(first10, code);
    CHECK(pairing_code_parse(code, &pc) == PAIRING_CODE_BAD_PASSCODE);
    complete_code("0999990000", code); // chunk2 > 16 bits
    CHECK(pairing_code_parse(code, &pc) == PAIRING_CODE_BAD_PASSCODE);
    complete_code("8000000001", code); // the SDK rejects a first digit of 8 or 9
    CHECK(pairing_code_parse(code, &pc) == PAIRING_CODE_BAD_FIRST_DIGIT);
    complete_code("9497011233", code);
    CHECK(pairing_code_parse(code, &pc) == PAIRING_CODE_BAD_FIRST_DIGIT);
}

// ---------------------------------------------------------------------------------------------------------------
// slot_table

static void test_slot_table(void)
{
    slot_table_t t;
    slot_table_init(&t);
    CHECK(slot_table_lowest_free(&t) == 0);
    CHECK(slot_table_take_node_id(&t) == SLOT_FIRST_NODE_ID);
    CHECK(slot_table_take_node_id(&t) == SLOT_FIRST_NODE_ID + 1);

    for (int i = 0; i < SLOT_COUNT; i++) {
        slot_table_assign(&t, slot_table_lowest_free(&t), slot_table_take_node_id(&t));
    }
    CHECK(slot_table_lowest_free(&t) == -1);
    slot_table_clear(&t, 5);
    slot_table_clear(&t, 2);
    CHECK(slot_table_lowest_free(&t) == 2);
    CHECK(slot_table_find_node(&t, t.slot[6].node_id) == 6);
    CHECK(slot_table_find_node(&t, 0xDEAD) == -1);

    uint8_t blob[SLOT_TABLE_BLOB_SIZE];
    CHECK(slot_table_encode(&t, blob, sizeof(blob) - 1) == 0);
    CHECK(slot_table_encode(&t, blob, sizeof(blob)) == SLOT_TABLE_BLOB_SIZE);
    slot_table_t u;
    CHECK(slot_table_decode(&u, blob, sizeof(blob)));
    CHECK(memcmp(&t.next_node_id, &u.next_node_id, sizeof(u.next_node_id)) == 0);
    for (int i = 0; i < SLOT_COUNT; i++) {
        CHECK(t.slot[i].used == u.slot[i].used && t.slot[i].node_id == u.slot[i].node_id);
    }

    // Rejections leave the output untouched.
    slot_table_t untouched;
    slot_table_init(&untouched);
    slot_table_t probe = untouched;
    uint8_t bad[SLOT_TABLE_BLOB_SIZE];
    CHECK(!slot_table_decode(&probe, blob, sizeof(blob) - 1));
    memcpy(bad, blob, sizeof(bad));
    bad[0] = 'X';
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    bad[2] = 2; // version
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    bad[12] = 7; // used flag not 0/1
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    memcpy(bad + 12 + 9 + 1, bad + 12 + 1, 8); // slot 1 duplicates slot 0's node ID
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    memcpy(bad, blob, sizeof(bad));
    memset(bad + 4, 0, 8); // next_node_id below every used ID
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    // With no slots used, the counter itself must still be a usable node ID.
    slot_table_t none;
    slot_table_init(&none);
    none.next_node_id = 0;
    slot_table_encode(&none, bad, sizeof(bad));
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    none.next_node_id = UINT64_MAX;
    slot_table_encode(&none, bad, sizeof(bad));
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    none.next_node_id = SLOT_MAX_NODE_ID;
    slot_table_encode(&none, bad, sizeof(bad));
    CHECK(slot_table_decode(&probe, bad, sizeof(bad)) && probe.next_node_id == SLOT_MAX_NODE_ID);
    probe = untouched;
    CHECK(!slot_table_next_id_valid(SLOT_FIRST_NODE_ID - 1) && slot_table_next_id_valid(SLOT_RECOVERY_NODE_ID));
    memcpy(bad, blob, sizeof(bad));
    bad[12 + 2 * 9 + 1] = 1; // slot 2 is free but has a node ID
    CHECK(!slot_table_decode(&probe, bad, sizeof(bad)));
    CHECK(memcmp(&probe, &untouched, sizeof(probe)) == 0);

    // Random corruption never crashes and never yields a table with duplicate IDs.
    srand(2);
    for (int i = 0; i < 20000; i++) {
        memcpy(bad, blob, sizeof(bad));
        bad[rand() % sizeof(bad)] = (uint8_t)rand();
        slot_table_t r;
        if (slot_table_decode(&r, bad, sizeof(bad))) {
            for (int a = 0; a < SLOT_COUNT; a++) {
                if (r.slot[a].used) {
                    CHECK(slot_table_find_node(&r, r.slot[a].node_id) == a);
                    CHECK(r.slot[a].node_id < r.next_node_id);
                }
            }
            CHECK(slot_table_next_id_valid(r.next_node_id));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// backoff

static void test_backoff(void)
{
    backoff_t b;
    backoff_init(&b, 10000, 300000);
    const uint32_t expected[] = {10000, 20000, 40000, 80000, 160000, 300000, 300000, 300000};
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        backoff_t copy = b;
        CHECK(backoff_next(&copy, 0) == expected[i] * 8 / 10);
        copy = b;
        CHECK(backoff_next(&copy, expected[i] * 2 / 5) == expected[i] * 12 / 10);
        uint32_t d = backoff_next(&b, (uint32_t)rand());
        CHECK(d >= expected[i] * 8 / 10 && d <= expected[i] * 12 / 10);
    }
    for (int i = 0; i < 100; i++) {
        CHECK(backoff_next(&b, (uint32_t)rand()) <= 360000);
    }
    backoff_reset(&b);
    uint32_t d = backoff_next(&b, 0);
    CHECK(d >= 8000 && d <= 12000);

    backoff_t zero;
    backoff_init(&zero, 0, 0);
    CHECK(backoff_next(&zero, 99) == 0);
}

// ---------------------------------------------------------------------------------------------------------------
// sensor_mgr

#define MIN (60u * 1000u)

static void paired_table(slot_table_t *t, int count)
{
    slot_table_init(t);
    for (int i = 0; i < count; i++) {
        slot_table_assign(t, i, slot_table_take_node_id(t));
    }
}

static void test_sensor_subscriptions(uint32_t t0)
{
    slot_table_t t;
    paired_table(&t, 2);
    sensor_mgr_t m;
    sensor_mgr_init(&m, &t, t0 - 5 * MIN); // a slow boot: the network comes up 5 min later, at t0

    // Until then nothing is subscribed, nothing counts as unreachable, and the whole stick shows the sweep.
    CHECK(sensor_mgr_next_subscribe(&m, t0 - 1) == -1);
    CHECK(!sensor_mgr_is_unreachable(&m, 0, t0 - 1));
    CHECK(sensor_mgr_led_state(&m, 0, t0 - 1) == LED_SLOT_STARTING);
    CHECK(sensor_mgr_led_state(&m, 5, t0 - 1) == LED_SLOT_STARTING);
    char line[128];
    sensor_mgr_format_slot(&m, 0, t0 - 1, line, sizeof(line));
    CHECK(strstr(line, "1  waiting") == line && strstr(line, "waiting for the Thread network"));

    // Both paired sensors are due once it is up; empty slots never are. The grace period starts now.
    sensor_mgr_set_network_ready(&m, t0);
    CHECK(sensor_mgr_next_subscribe(&m, t0) == 0);
    CHECK(sensor_mgr_next_subscribe(&m, t0) == 1);
    CHECK(sensor_mgr_next_subscribe(&m, t0) == -1);
    CHECK(sensor_mgr_led_state(&m, 0, t0) == LED_SLOT_WAITING); // no report yet
    CHECK(sensor_mgr_led_state(&m, 5, t0) == LED_SLOT_EMPTY);
    sensor_mgr_format_slot(&m, 0, t0 + 3000, line, sizeof(line));
    CHECK(strstr(line, "1  waiting") == line && strstr(line, "subscribing (down 3s)"));
    sensor_mgr_set_network_ready(&m, t0 + 1000); // a repeat changes nothing
    CHECK(sensor_mgr_next_subscribe(&m, t0 + 1000) == -1);

    // Sensor 0 subscribes; MYGGBETT StateValue false = open, true = closed.
    sensor_mgr_on_subscribed(&m, 0, 60, t0 + 1000);
    sensor_mgr_on_state_value(&m, 0, false, t0 + 1100);
    CHECK(sensor_mgr_led_state(&m, 0, t0 + 1200) == LED_SLOT_OPEN);
    sensor_mgr_on_state_value(&m, 0, true, t0 + 1300);
    CHECK(sensor_mgr_led_state(&m, 0, t0 + 1400) == LED_SLOT_CLOSED);

    // Sensor 1 never answers: fine during the grace period, "not heard from" after it.
    sensor_mgr_on_subscription_down(&m, 1, t0 + 20000, 0);
    CHECK(sensor_mgr_led_state(&m, 1, t0 + 2 * MIN - 1) == LED_SLOT_WAITING);
    CHECK(!sensor_mgr_is_unreachable(&m, 1, t0 + 2 * MIN - 1));
    CHECK(sensor_mgr_led_state(&m, 1, t0 + 2 * MIN) == LED_SLOT_UNREACHABLE);
    sensor_mgr_format_slot(&m, 1, t0 + 25000, line, sizeof(line));
    CHECK(strstr(line, "2  waiting") == line && strstr(line, "down 25s, retry in"));
    // A second report of the same failure doesn't push the retry further out.
    uint32_t retry_at = m.sensor[1].next_attempt_ms;
    sensor_mgr_on_subscription_down(&m, 1, t0 + 21000, UINT32_MAX);
    CHECK(m.sensor[1].next_attempt_ms == retry_at && m.sensor[1].backoff.attempt == 1);

    // Retries back off: first ~10 s, then ~20 s, ...
    CHECK(sensor_mgr_next_subscribe(&m, t0 + 20000 + 7999) == -1);
    CHECK(sensor_mgr_next_subscribe(&m, t0 + 20000 + 12000) == 1);
    sensor_mgr_on_subscription_down(&m, 1, t0 + 40000, UINT32_MAX / 2);
    CHECK(sensor_mgr_next_subscribe(&m, t0 + 40000 + 15999) == -1);
    CHECK(sensor_mgr_next_subscribe(&m, t0 + 40000 + 24000) == 1);
    // It finally subscribes (1 h max interval, so it stays quiet below): no longer unreachable, backoff resets.
    sensor_mgr_on_subscribed(&m, 1, 3600, t0 + 3 * MIN);
    CHECK(sensor_mgr_led_state(&m, 1, t0 + 3 * MIN) == LED_SLOT_WAITING); // subscribed, value not in yet
    sensor_mgr_on_state_value(&m, 1, true, t0 + 3 * MIN);                 // the first report carries it
    CHECK(sensor_mgr_led_state(&m, 1, t0 + 3 * MIN) == LED_SLOT_CLOSED);
    CHECK(m.sensor[1].backoff.attempt == 0);

    // Keep-alives keep sensor 0 healthy well past its max interval.
    uint32_t now = t0 + 3 * MIN;
    sensor_mgr_on_heard(&m, 0, now);
    for (int i = 0; i < 10; i++) {
        now += 60000;
        sensor_mgr_on_heard(&m, 0, now);
        CHECK(sensor_mgr_next_subscribe(&m, now) == -1);
        CHECK(!sensor_mgr_is_unreachable(&m, 0, now));
    }
    // Then silence: at 2 x 60 s + 30 s it is stale, gets resubscribed and stays "not heard from".
    CHECK(sensor_mgr_next_subscribe(&m, now + 150000) == -1);
    CHECK(!sensor_mgr_is_unreachable(&m, 0, now + 150000));
    CHECK(sensor_mgr_is_unreachable(&m, 0, now + 150001));
    CHECK(sensor_mgr_next_subscribe(&m, now + 150001) == 0);
    CHECK(sensor_mgr_is_unreachable(&m, 0, now + 150002));
    CHECK(sensor_mgr_led_state(&m, 0, now + 160000) == LED_SLOT_UNREACHABLE);
    CHECK(sensor_mgr_next_subscribe(&m, now + 160000) == -1); // in flight
    sensor_mgr_on_subscribed(&m, 0, 60, now + 170000);
    CHECK(sensor_mgr_led_state(&m, 0, now + 170000) == LED_SLOT_CLOSED); // last known state kept

    // A sleepy sensor with a 1 h max interval that drops after 50 min of normal quiet gets a fresh grace period.
    sensor_mgr_on_subscribed(&m, 1, 3600, t0 + 10 * MIN);
    sensor_mgr_on_subscription_down(&m, 1, t0 + 60 * MIN, 0);
    CHECK(!sensor_mgr_is_unreachable(&m, 1, t0 + 61 * MIN));
    CHECK(sensor_mgr_is_unreachable(&m, 1, t0 + 62 * MIN));

    sensor_mgr_format_slot(&m, 0, now + 175000, line, sizeof(line));
    CHECK(strstr(line, "1  closed") == line && strstr(line, "node 0x100") && strstr(line, "heard 5s ago"));
    sensor_mgr_format_slot(&m, 1, t0 + 62 * MIN, line, sizeof(line));
    CHECK(strstr(line, "2  LOST") == line && strstr(line, "down 2m"));
    sensor_mgr_format_slot(&m, 7, t0, line, sizeof(line));
    CHECK(strcmp(line, "8  empty") == 0);

    // An event stamped just after the caller read its clock is "just now", not 49 days ago.
    sensor_mgr_on_heard(&m, 0, now + 200000);
    CHECK(!sensor_mgr_is_unreachable(&m, 0, now + 199999));
    CHECK(sensor_mgr_led_state(&m, 0, now + 199999) == LED_SLOT_CLOSED);
    sensor_mgr_format_slot(&m, 0, now + 199999, line, sizeof(line));
    CHECK(strstr(line, "heard 0s ago"));
    sensor_mgr_on_subscription_down(&m, 0, now + 210000, 0);
    CHECK(!sensor_mgr_is_unreachable(&m, 0, now + 209999));

    // The radio failing overrides everything.
    sensor_mgr_set_radio_failed(&m);
    for (int i = 0; i < SLOT_COUNT; i++) {
        CHECK(sensor_mgr_led_state(&m, i, now) == LED_SLOT_RADIO_FAILED);
    }
}

static void test_sensor_watchdogs(uint32_t t0)
{
    slot_table_t t;
    paired_table(&t, 1);
    sensor_mgr_t m;
    sensor_mgr_init(&m, &t, t0);
    sensor_mgr_set_network_ready(&m, t0);

    // An attempt Matter never reports back on is handed out again after the timeout.
    CHECK(sensor_mgr_next_subscribe(&m, t0) == 0);
    CHECK(sensor_mgr_next_subscribe(&m, t0 + SENSOR_ATTEMPT_TIMEOUT_MS - 1) == -1);
    CHECK(sensor_mgr_is_unreachable(&m, 0, t0 + SENSOR_ATTEMPT_TIMEOUT_MS - 1));
    CHECK(sensor_mgr_next_subscribe(&m, t0 + SENSOR_ATTEMPT_TIMEOUT_MS) == 0);
    CHECK(sensor_mgr_next_subscribe(&m, t0 + SENSOR_ATTEMPT_TIMEOUT_MS + 1000) == -1);
    sensor_mgr_on_subscribed(&m, 0, 60, t0 + SENSOR_ATTEMPT_TIMEOUT_MS + 2000);
    CHECK(!sensor_mgr_is_unreachable(&m, 0, t0 + SENSOR_ATTEMPT_TIMEOUT_MS + 2000));
    // A subscribed sensor has no attempt in flight, so the timeout never fires on it.
    uint32_t now = t0 + SENSOR_ATTEMPT_TIMEOUT_MS + 2000;
    for (int i = 0; i < 20; i++) {
        now += MIN;
        sensor_mgr_on_heard(&m, 0, now);
        CHECK(sensor_mgr_next_subscribe(&m, now) == -1);
    }

    // A sensor down for 60 days (a dead battery) blinks blue the whole time, across the 49.7-day clock wrap.
    sensor_mgr_on_subscription_down(&m, 0, now, 0);
    uint32_t down = now;
    bool always_lost = true;
    for (uint32_t minute = 1; minute <= 60 * 24 * 60; minute++) {
        now = down + minute * MIN;
        if (sensor_mgr_next_subscribe(&m, now) == 0) {
            sensor_mgr_on_subscription_down(&m, 0, now, 0);
        }
        if (minute >= SENSOR_UNREACHABLE_AFTER_MS / MIN) { // past the grace period after the drop
            always_lost &= sensor_mgr_led_state(&m, 0, now) == LED_SLOT_UNREACHABLE;
        }
    }
    CHECK(always_lost);
    char line[128];
    sensor_mgr_format_slot(&m, 0, now, line, sizeof(line));
    CHECK(strstr(line, "1  LOST") == line && strstr(line, "down 480h"));
}

static void test_sensor_pairing(void)
{
    slot_table_t t;
    slot_table_init(&t);
    sensor_mgr_t m;
    sensor_mgr_init(&m, &t, 0);

    int slot;
    uint64_t node;
    // No pairing before the Thread network is up, and no node ID is used up by trying.
    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_NOT_READY);
    CHECK(m.table.next_node_id == SLOT_FIRST_NODE_ID);
    sensor_mgr_set_network_ready(&m, 0);
    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_OK && slot == 0 && node == SLOT_FIRST_NODE_ID);
    CHECK(sensor_mgr_led_state(&m, 0, 0) == LED_SLOT_PAIRING);
    CHECK(sensor_mgr_led_state(&m, 1, 0) == LED_SLOT_EMPTY);
    CHECK(sensor_mgr_led_state(&m, -1, 0) == LED_SLOT_EMPTY);
    char line[64];
    sensor_mgr_format_slot(&m, 0, 0, line, sizeof(line));
    CHECK(strcmp(line, "1  pairing  node 0x100") == 0);
    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_BUSY);
    CHECK(sensor_mgr_next_subscribe(&m, 0) == -1);

    // Failure frees the slot but the node ID is not reused.
    sensor_mgr_end_pairing(&m, false, 100);
    CHECK(sensor_mgr_led_state(&m, 0, 100) == LED_SLOT_EMPTY);
    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_OK && slot == 0 && node == SLOT_FIRST_NODE_ID + 1);
    sensor_mgr_end_pairing(&m, true, 200);
    CHECK(m.table.slot[0].used && m.table.slot[0].node_id == SLOT_FIRST_NODE_ID + 1);
    CHECK(sensor_mgr_next_subscribe(&m, 200) == 0);

    for (int i = 1; i < SLOT_COUNT; i++) {
        CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_OK && slot == i);
        sensor_mgr_end_pairing(&m, true, 300);
    }
    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_FULL);

    // Removing slot 3 (console "4") frees it for the next pairing.
    CHECK(sensor_mgr_remove(&m, 3, &node) == SENSOR_MGR_OK && node == SLOT_FIRST_NODE_ID + 4);
    CHECK(sensor_mgr_remove(&m, 3, &node) == SENSOR_MGR_NOT_PAIRED);
    CHECK(sensor_mgr_remove(&m, 8, &node) == SENSOR_MGR_BAD_SLOT);
    CHECK(sensor_mgr_remove(&m, -1, &node) == SENSOR_MGR_BAD_SLOT);
    CHECK(sensor_mgr_led_state(&m, 3, 400) == LED_SLOT_EMPTY);

    // Late callbacks for the removed sensor change nothing.
    sensor_mgr_on_state_value(&m, 3, false, 400);
    sensor_mgr_on_subscribed(&m, 3, 60, 400);
    sensor_mgr_on_subscription_down(&m, 3, 400, 0);
    CHECK(sensor_mgr_led_state(&m, 3, 400) == LED_SLOT_EMPTY);

    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_OK && slot == 3);
    sensor_mgr_end_pairing(&m, true, 500);
    CHECK(sensor_mgr_led_state(&m, 3, 500) == LED_SLOT_WAITING); // new sensor, nothing reported yet

    // Clearing everything mid-pairing (factory-reset) also ends the pairing.
    CHECK(sensor_mgr_remove(&m, 5, &node) == SENSOR_MGR_OK);
    CHECK(sensor_mgr_begin_pairing(&m, &slot, &node) == SENSOR_MGR_OK && slot == 5);
    uint64_t next = m.table.next_node_id;
    sensor_mgr_clear_all(&m);
    CHECK(slot_table_lowest_free(&m.table) == 0 && m.table.next_node_id == next);
    CHECK(m.pairing_slot == -1 && sensor_mgr_led_state(&m, 5, 600) == LED_SLOT_EMPTY);
    CHECK(sensor_mgr_next_subscribe(&m, 600) == -1);
}

// ---------------------------------------------------------------------------------------------------------------
// command

static bool parse(const char *line, command_t *cmd, char *err)
{
    char buf[128];
    const char *argv[16];
    int argc = 0;
    snprintf(buf, sizeof(buf), "%s", line);
    for (char *tok = strtok(buf, " "); tok && argc < 16; tok = strtok(NULL, " ")) {
        argv[argc++] = tok;
    }
    return command_parse(argc, argv, cmd, err, 128);
}

static void test_command(void)
{
    command_t c;
    char err[128];

    CHECK(parse("pair 34970112332", &c, err) && c.kind == CMD_PAIR && c.code.passcode == 20202021);
    CHECK(parse("pair 3497 011 2332", &c, err) && c.kind == CMD_PAIR && c.code.passcode == 20202021);
    CHECK(parse("pair 3497-011-2332", &c, err) && c.kind == CMD_PAIR);
    CHECK(!parse("pair 34970112333", &c, err) && strstr(err, "typo"));
    CHECK(!parse("pair", &c, err) && strstr(err, "usage"));
    CHECK(!parse("pair 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5", &c, err) && strstr(err, "11 digits"));

    CHECK(parse("remove 1", &c, err) && c.kind == CMD_REMOVE && c.slot == 0);
    CHECK(parse("identify 8", &c, err) && c.kind == CMD_IDENTIFY && c.slot == 7);
    CHECK(!parse("remove 0", &c, err) && strstr(err, "1-8"));
    CHECK(!parse("remove 9", &c, err));
    CHECK(!parse("remove 10", &c, err));
    CHECK(!parse("remove x", &c, err));
    CHECK(!parse("remove", &c, err));
    CHECK(!parse("remove 1 2", &c, err));

    CHECK(!parse("factory-reset", &c, err) && strstr(err, "factory-reset confirm"));
    CHECK(!parse("factory-reset yes", &c, err));
    CHECK(parse("factory-reset confirm", &c, err) && c.kind == CMD_FACTORY_RESET);

    CHECK(parse("list", &c, err) && c.kind == CMD_LIST);
    CHECK(!parse("help", &c, err));
    CHECK(!parse("list all", &c, err));
    CHECK(!parse("open-sesame", &c, err) && strstr(err, "unknown command"));
    CHECK(!command_parse(0, NULL, &c, err, sizeof(err)));
}

// ---------------------------------------------------------------------------------------------------------------
// thread_channel

static void test_thread_channel(void)
{
    int8_t rssi[THREAD_CHANNEL_COUNT];
    for (int i = 0; i < THREAD_CHANNEL_COUNT; i++) {
        rssi[i] = -95;
    }
    CHECK(thread_channel_pick(rssi, 0) == -1);                        // nothing scanned
    CHECK(thread_channel_pick(rssi, 0xFFFF) == 15);                  // all equal: first preferred channel
    rssi[15 - 11] = -60;                                             // Wi-Fi next to 15
    CHECK(thread_channel_pick(rssi, 0xFFFF) == 20);
    rssi[25 - 11] = -60;
    rssi[20 - 11] = -93;                                             // 2 dB louder than the quietest: still fine
    CHECK(thread_channel_pick(rssi, 0xFFFF) == 20);
    rssi[20 - 11] = -91;                                             // 4 dB louder: the quietest channel wins
    CHECK(thread_channel_pick(rssi, 0xFFFF) == 11);
    rssi[18 - 11] = -99;
    CHECK(thread_channel_pick(rssi, 0xFFFF) == 18);
    CHECK(thread_channel_pick(rssi, 1u << (26 - 11)) == 26);         // only one channel measured
    CHECK(thread_channel_pick(rssi, (uint16_t) ~(1u << (18 - 11))) == 11); // 18 not measured
}

int main(void)
{
    test_pairing_code();
    test_slot_table();
    test_backoff();
    test_sensor_subscriptions(1000);
    test_sensor_subscriptions(UINT32_MAX - 90000); // clock wraps ~90 s in
    test_sensor_watchdogs(1000);
    test_sensor_watchdogs(UINT32_MAX - 90000);
    test_sensor_pairing();
    test_command();
    test_thread_channel();
    printf("app_core: %d checks, %d failed\n", s_checks, s_failures);
    return s_failures ? 1 : 0;
}
