#pragma once

// Console command parsing. Takes argv as split by esp_console, so the glue only dispatches.
//   pair <11-digit code>      the code may be split by spaces or dashes
//   list
//   remove <slot 1-8>
//   identify <slot 1-8>
//   factory-reset confirm     removes every sensor; the extra word guards against accidents
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
    CMD_FACTORY_RESET,
} command_kind_t;

typedef struct {
    command_kind_t kind;
    int slot;            // 0-based; CMD_REMOVE and CMD_IDENTIFY
    pairing_code_t code; // CMD_PAIR
} command_t;

// On failure writes a one-line message for the user to err.
bool command_parse(int argc, const char *const *argv, command_t *out, char *err, size_t err_len);

#ifdef __cplusplus
}
#endif
