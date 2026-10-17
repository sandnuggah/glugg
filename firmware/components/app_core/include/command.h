#pragma once

// Console command parsing. Takes argv as split by esp_console, so the glue only dispatches.
//   pair <11-digit code> [slot <n>]   the code may be split by spaces or dashes; without a slot, the lowest free one
//   list
//   remove <slot 1-64>
//   identify <slot 1-64>
//   move <from> <to>                  puts a sensor on another LED; if `to` is used, the two sensors swap
//   factory-reset confirm             removes every sensor; the extra word guards against accidents
// `help` is esp_console's own; each command's help string lives in the glue.

#include <stdbool.h>
#include <stddef.h>

#include "pairing_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CMD_PAIR,
    CMD_LIST,
    CMD_REMOVE,
    CMD_IDENTIFY,
    CMD_MOVE,
    CMD_FACTORY_RESET,
} command_kind_t;

typedef struct {
    command_kind_t kind;
    int slot;            // 0-based; CMD_REMOVE, CMD_IDENTIFY, CMD_MOVE (from), CMD_PAIR (-1 = lowest free)
    int slot2;           // 0-based; CMD_MOVE (to)
    pairing_code_t code; // CMD_PAIR
} command_t;

// On failure writes a one-line message for the user to err.
bool command_parse(int argc, const char *const *argv, command_t *out, char *err, size_t err_len);

#ifdef __cplusplus
}
#endif
