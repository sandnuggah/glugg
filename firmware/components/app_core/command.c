#include "command.h"

#include <stdio.h>
#include <string.h>

#include "slot_table.h"

// "1".."SLOT_COUNT", plain decimal: no sign, no leading zero.
static bool parse_slot(const char *text, int *slot)
{
    if (text[0] < '1' || text[0] > '9') {
        return false;
    }
    int n = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9' || n > SLOT_COUNT) {
            return false;
        }
        n = n * 10 + (*p - '0');
    }
    if (n > SLOT_COUNT) {
        return false;
    }
    *slot = n - 1;
    return true;
}

bool command_parse(int argc, const char *const *argv, command_t *out, char *err, size_t err_len)
{
    memset(out, 0, sizeof(*out));
    err[0] = '\0';
    if (argc < 1) {
        snprintf(err, err_len, "empty command");
        return false;
    }
    const char *name = argv[0];

    if (strcmp(name, "pair") == 0) {
        // `pair <code> slot <n>`: the code itself may be split by spaces, so the slot is marked by the word "slot".
        int code_end = argc;
        out->slot = -1;
        if (argc >= 3 && strcmp(argv[argc - 2], "slot") == 0) {
            if (!parse_slot(argv[argc - 1], &out->slot)) {
                snprintf(err, err_len, "pair: the slot must be 1-%d", SLOT_COUNT);
                return false;
            }
            code_end = argc - 2;
        } else if (argc >= 2 && strcmp(argv[argc - 1], "slot") == 0) {
            snprintf(err, err_len, "pair: which slot? usage: pair <11-digit code> [slot <1-%d>]", SLOT_COUNT);
            return false;
        }
        if (code_end < 2) {
            snprintf(err, err_len, "usage: pair <11-digit code> [slot <1-%d>]", SLOT_COUNT);
            return false;
        }
        char joined[64] = "";
        for (int i = 1; i < code_end; i++) {
            if (strlen(joined) + strlen(argv[i]) + 1 >= sizeof(joined)) {
                snprintf(err, err_len, "pair: %s", pairing_code_strerror(PAIRING_CODE_BAD_LENGTH));
                return false;
            }
            strcat(joined, argv[i]);
        }
        pairing_code_err_t e = pairing_code_parse(joined, &out->code);
        if (e != PAIRING_CODE_OK) {
            snprintf(err, err_len, "pair: %s", pairing_code_strerror(e));
            return false;
        }
        out->kind = CMD_PAIR;
        return true;
    }

    if (strcmp(name, "remove") == 0 || strcmp(name, "identify") == 0) {
        if (argc != 2 || !parse_slot(argv[1], &out->slot)) {
            snprintf(err, err_len, "usage: %s <slot 1-%d>", name, SLOT_COUNT);
            return false;
        }
        out->kind = name[0] == 'r' ? CMD_REMOVE : CMD_IDENTIFY;
        return true;
    }

    if (strcmp(name, "move") == 0) {
        if (argc != 3 || !parse_slot(argv[1], &out->slot) || !parse_slot(argv[2], &out->slot2)) {
            snprintf(err, err_len, "usage: move <from 1-%d> <to 1-%d>", SLOT_COUNT, SLOT_COUNT);
            return false;
        }
        if (out->slot == out->slot2) {
            snprintf(err, err_len, "move: the two slots are the same");
            return false;
        }
        out->kind = CMD_MOVE;
        return true;
    }

    if (strcmp(name, "factory-reset") == 0) {
        if (argc != 2 || strcmp(argv[1], "confirm") != 0) {
            snprintf(err, err_len, "this removes every sensor; type: factory-reset confirm");
            return false;
        }
        out->kind = CMD_FACTORY_RESET;
        return true;
    }

    if (strcmp(name, "list") == 0) {
        if (argc != 1) {
            snprintf(err, err_len, "usage: list");
            return false;
        }
        out->kind = CMD_LIST;
        return true;
    }

    snprintf(err, err_len, "unknown command '%s'", name);
    return false;
}
