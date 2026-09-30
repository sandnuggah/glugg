#include "slot_table.h"

#include <string.h>

// Blob layout: 'W' 'S' version count | next_node_id (u64) | count x (used u8, node_id u64)
#define MAGIC0  'W'
#define MAGIC1  'S'
#define VERSION 1

void slot_table_init(slot_table_t *t)
{
    memset(t, 0, sizeof(*t));
    t->next_node_id = SLOT_FIRST_NODE_ID;
}

int slot_table_lowest_free(const slot_table_t *t)
{
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (!t->slot[i].used) {
            return i;
        }
    }
    return -1;
}

int slot_table_find_node(const slot_table_t *t, uint64_t node_id)
{
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (t->slot[i].used && t->slot[i].node_id == node_id) {
            return i;
        }
    }
    return -1;
}

uint64_t slot_table_take_node_id(slot_table_t *t)
{
    return t->next_node_id++;
}

void slot_table_assign(slot_table_t *t, int slot, uint64_t node_id)
{
    t->slot[slot].used = true;
    t->slot[slot].node_id = node_id;
}

void slot_table_clear(slot_table_t *t, int slot)
{
    t->slot[slot].used = false;
    t->slot[slot].node_id = 0;
}

static void put_u64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++) {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

static uint64_t get_u64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v |= (uint64_t)p[i] << (8 * i);
    }
    return v;
}

size_t slot_table_encode(const slot_table_t *t, uint8_t *buf, size_t len)
{
    if (len < SLOT_TABLE_BLOB_SIZE) {
        return 0;
    }
    uint8_t *p = buf;
    *p++ = MAGIC0;
    *p++ = MAGIC1;
    *p++ = VERSION;
    *p++ = SLOT_COUNT;
    put_u64(p, t->next_node_id);
    p += 8;
    for (int i = 0; i < SLOT_COUNT; i++) {
        *p++ = t->slot[i].used ? 1 : 0;
        put_u64(p, t->slot[i].used ? t->slot[i].node_id : 0);
        p += 8;
    }
    return (size_t)(p - buf);
}

bool slot_table_next_id_valid(uint64_t id)
{
    return id >= SLOT_FIRST_NODE_ID && id <= SLOT_MAX_NODE_ID;
}

bool slot_table_decode(slot_table_t *t, const uint8_t *buf, size_t len)
{
    if (len != SLOT_TABLE_BLOB_SIZE || buf[0] != MAGIC0 || buf[1] != MAGIC1 || buf[2] != VERSION ||
        buf[3] != SLOT_COUNT) {
        return false;
    }
    slot_table_t tmp;
    memset(&tmp, 0, sizeof(tmp)); // slots after i must read as unused for the duplicate check below
    tmp.next_node_id = get_u64(buf + 4);
    if (!slot_table_next_id_valid(tmp.next_node_id)) {
        return false;
    }
    const uint8_t *p = buf + 12;
    for (int i = 0; i < SLOT_COUNT; i++, p += 9) {
        if (p[0] > 1) {
            return false;
        }
        tmp.slot[i].used = p[0] == 1;
        tmp.slot[i].node_id = get_u64(p + 1);
        // A used slot needs an ID that was handed out before and isn't owned by another slot.
        if (tmp.slot[i].used && (tmp.slot[i].node_id < SLOT_FIRST_NODE_ID || tmp.slot[i].node_id >= tmp.next_node_id ||
                                 slot_table_find_node(&tmp, tmp.slot[i].node_id) != i)) {
            return false;
        }
        if (!tmp.slot[i].used && tmp.slot[i].node_id != 0) {
            return false;
        }
    }
    *t = tmp;
    return true;
}
