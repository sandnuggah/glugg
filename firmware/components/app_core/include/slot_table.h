#pragma once

// The persisted part of the app: which Matter node owns which LED slot.
// Slot i (0-based here, 1-based on the console) drives LED i. New pairings take the lowest free slot.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLOT_COUNT 8

// Node IDs are never reused, so a removed sensor's stale sessions can't be mistaken for a new one.
#define SLOT_FIRST_NODE_ID 0x100
// Matter's largest operational node ID (kMaxOperationalNodeId).
#define SLOT_MAX_NODE_ID   0xFFFFFFEFFFFFFFFFull
// Where the counter restarts if the saved table and the saved counter are both lost. Sensors paired before may still
// be on the fabric with low IDs; this is far above any ID 8 slots would reach in practice.
#define SLOT_RECOVERY_NODE_ID 0x10000

#define SLOT_TABLE_BLOB_SIZE (4 + 8 + SLOT_COUNT * 9)

typedef struct {
    bool used;
    uint64_t node_id;
} slot_t;

typedef struct {
    slot_t slot[SLOT_COUNT];
    uint64_t next_node_id;
} slot_table_t;

void slot_table_init(slot_table_t *t);

// Lowest free slot, or -1 if all are used.
int slot_table_lowest_free(const slot_table_t *t);

// Slot owned by node_id, or -1.
int slot_table_find_node(const slot_table_t *t, uint64_t node_id);

// Hands out the next node ID. Persist the table afterwards so the ID is not handed out again after a reboot.
uint64_t slot_table_take_node_id(slot_table_t *t);

void slot_table_assign(slot_table_t *t, int slot, uint64_t node_id);
void slot_table_clear(slot_table_t *t, int slot);

// Fixed-size little-endian encoding for NVS. Returns bytes written (SLOT_TABLE_BLOB_SIZE) or 0 if buf is too small.
size_t slot_table_encode(const slot_table_t *t, uint8_t *buf, size_t len);

// Returns false (and leaves *t untouched) for a blob with the wrong size, magic, version or inconsistent contents,
// including a next node ID outside SLOT_FIRST_NODE_ID..SLOT_MAX_NODE_ID.
bool slot_table_decode(slot_table_t *t, const uint8_t *buf, size_t len);

// True if `id` can be the next node ID handed out.
bool slot_table_next_id_valid(uint64_t id);

#ifdef __cplusplus
}
#endif
